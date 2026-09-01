#include <algorithm>
#include <cstdio>

#include "Ldx12BaseMips.hpp"
#include "Ldx12CommandBatch.hpp"
#include "Ldx12CommandBuffer.hpp"
#include "Ldx12ImmediateCommands.hpp"
#include "Ldx12ShaderCompiler.hpp"
#include "Ldx12StagingDevice.hpp"
#include "Ldx12Swapchain.hpp"

namespace ldx12
{
	namespace
	{
		void ReportInvalidDestroy( const char* resourceType, uint32_t index, uint32_t generation ) noexcept
		{
#if defined( _DEBUG )
			char message[ 192 ]{};
			std::snprintf( message,
				sizeof( message ),
				"Ldx12 warning: ignored Destroy(%s) for an invalid or stale handle [index=%u, generation=%u].\n",
				resourceType,
				index,
				generation );
			OutputDebugStringA( message );
#else
			static_cast<void>( resourceType );
			static_cast<void>( index );
			static_cast<void>( generation );
#endif
		}

		struct TextureCreationPlan final
		{
			uint16_t mipLevels_ = 1;
			bool generateInitialMipChain_ = false;
			bool requiresTypedUavViews_ = false;
		};

		template <typename SlotType>
		[[nodiscard]] SlotType AllocateFreeBindingSlot( uint32_t& allocatedMask, uint32_t firstSlot, uint32_t slotCount, const char* exhaustedMessage )
		{
			for( uint32_t slotOffset = 0; slotOffset < slotCount; ++slotOffset )
			{
				const uint32_t bit = 1u << slotOffset;
				if( ( allocatedMask & bit ) == 0 )
				{
					allocatedMask |= bit;
					return static_cast<SlotType>( firstSlot + slotOffset );
				}
			}

			throw std::runtime_error( exhaustedMessage );
		}

		[[nodiscard]] uint64_t AlignUp( uint64_t value, uint64_t alignment ) noexcept
		{
			return ( value + alignment - 1u ) & ~( alignment - 1u );
		}

		[[nodiscard]] uint32_t ToPublicDescriptorIndex( uint32_t index ) noexcept
		{
			return index == UINT32_MAX ? LDX12_DESCRIPTOR_SLOT_INVALID : index;
		}

		[[nodiscard]] uint16_t ClampTextureMipCount( uint32_t width, uint32_t height, uint16_t requestedMipCount ) noexcept
		{
			uint16_t maxMipCount = 1;
			while( width > 1 || height > 1 )
			{
				width = std::max( 1u, width >> 1u );
				height = std::max( 1u, height >> 1u );
				++maxMipCount;
			}

			return std::clamp<uint16_t>( requestedMipCount, 1u, maxMipCount );
		}

		[[nodiscard]] bool IsSrgbTextureFormat( DXGI_FORMAT format ) noexcept
		{
			return format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		}

		[[nodiscard]] bool IsValidSampleCount( uint32_t sampleCount ) noexcept
		{
			return sampleCount != 0 && ( sampleCount & ( sampleCount - 1u ) ) == 0;
		}

		[[nodiscard]] bool SupportsTextureSampleCount( ID3D12Device* device, DXGI_FORMAT format, uint32_t sampleCount ) noexcept
		{
			if( device == nullptr || format == DXGI_FORMAT_UNKNOWN || !IsValidSampleCount( sampleCount ) )
			{
				return false;
			}

			if( sampleCount == 1 )
			{
				return true;
			}

			D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels{};
			qualityLevels.Format = format;
			qualityLevels.SampleCount = sampleCount;
			qualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
			return SUCCEEDED( device->CheckFeatureSupport( D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualityLevels, sizeof( qualityLevels ) ) ) &&
				   qualityLevels.NumQualityLevels > 0;
		}

		[[nodiscard]] bool SupportsComputeMipGeneration( const TextureDesc& desc, uint16_t mipCount ) noexcept
		{
			return mipCount > 1 && desc.data != nullptr && desc.dimension == TextureDimension::Texture2D && desc.depthOrArraySize == 1 &&
				   ( desc.format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB );
		}

		TextureCreationPlan BuildTextureCreationPlan( const TextureDesc& desc )
		{
			TextureCreationPlan plan{};
			const uint16_t requestedMipLevels = std::max<uint16_t>( 1u, desc.countMipMap );
			plan.mipLevels_ = ClampTextureMipCount( desc.width, desc.height, requestedMipLevels );
			plan.generateInitialMipChain_ = SupportsComputeMipGeneration( desc, plan.mipLevels_ );
			plan.requiresTypedUavViews_ = HasTextureUsage( desc.usage, TextureUsage::UnorderedAccess ) || plan.generateInitialMipChain_;

			if( requestedMipLevels > 1u && desc.data != nullptr && !plan.generateInitialMipChain_ )
			{
				throw std::runtime_error( "Compute mip generation currently supports only single Texture2D RGBA8 "
										  "textures." );
			}

			return plan;
		}

		[[nodiscard]] DXGI_FORMAT ResolveTypedUavCompatibleResourceFormat( DXGI_FORMAT format, bool requiresTypedUavViews ) noexcept
		{
			if( requiresTypedUavViews && IsSrgbTextureFormat( format ) )
			{
				return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			}

			return format;
		}

		[[nodiscard]] DXGI_FORMAT ResolveTextureUavFormat( DXGI_FORMAT format ) noexcept
		{
			if( IsSrgbTextureFormat( format ) )
			{
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			}

			return format;
		}

		TextureResource::ResolvedFormats ResolveTextureFormats( const TextureDesc& desc, bool requiresTypedUavViews )
		{
			TextureResource::ResolvedFormats formats{};
			formats.resource_ = desc.format;

			const bool sampled = HasTextureUsage( desc.usage, TextureUsage::Sampled );
			const bool renderTarget = HasTextureUsage( desc.usage, TextureUsage::RenderTarget );
			const bool depthStencil = HasTextureUsage( desc.usage, TextureUsage::DepthStencil );
			const bool unorderedAccess = HasTextureUsage( desc.usage, TextureUsage::UnorderedAccess ) || requiresTypedUavViews;

			if( depthStencil )
			{
				switch( desc.format )
				{
				case DXGI_FORMAT_D16_UNORM:
					formats.resource_ = DXGI_FORMAT_R16_TYPELESS;
					formats.dsv_ = DXGI_FORMAT_D16_UNORM;
					formats.srv_ = sampled ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_UNKNOWN;
					break;

				case DXGI_FORMAT_D24_UNORM_S8_UINT:
					formats.resource_ = DXGI_FORMAT_R24G8_TYPELESS;
					formats.dsv_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
					formats.srv_ = sampled ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_UNKNOWN;
					break;

				case DXGI_FORMAT_D32_FLOAT:
					formats.resource_ = DXGI_FORMAT_R32_TYPELESS;
					formats.dsv_ = DXGI_FORMAT_D32_FLOAT;
					formats.srv_ = sampled ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_UNKNOWN;
					break;

				case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
					formats.resource_ = DXGI_FORMAT_R32G8X24_TYPELESS;
					formats.dsv_ = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
					formats.srv_ = sampled ? DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS : DXGI_FORMAT_UNKNOWN;
					break;

				default:
					throw std::runtime_error( "Unsupported depth texture format." );
				}

				return formats;
			}

			if( unorderedAccess )
			{
				formats.resource_ = ResolveTypedUavCompatibleResourceFormat( desc.format, requiresTypedUavViews );
			}

			formats.srv_ = sampled ? desc.format : DXGI_FORMAT_UNKNOWN;
			formats.rtv_ = renderTarget ? desc.format : DXGI_FORMAT_UNKNOWN;
			formats.uav_ = unorderedAccess ? ResolveTextureUavFormat( desc.format ) : DXGI_FORMAT_UNKNOWN;
			return formats;
		}

		D3D12_RESOURCE_FLAGS
		ResolveTextureResourceFlags( TextureUsage usage, D3D12_RESOURCE_FLAGS additionalFlags, bool requiresTypedUavViews ) noexcept
		{
			D3D12_RESOURCE_FLAGS flags = additionalFlags;
			if( HasTextureUsage( usage, TextureUsage::RenderTarget ) )
			{
				flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			}

			if( HasTextureUsage( usage, TextureUsage::DepthStencil ) )
			{
				flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			}

			if( HasTextureUsage( usage, TextureUsage::UnorderedAccess ) || requiresTypedUavViews )
			{
				flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}

			return flags;
		}

		void ValidateTextureDesc( const TextureDesc& desc )
		{
			constexpr D3D12_RESOURCE_FLAGS allowedAdditionalFlags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
			if( ( desc.additionalResourceFlags & ~allowedAdditionalFlags ) != 0 )
			{
				throw std::runtime_error( "TextureDesc::additionalResourceFlags supports only "
										  "D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS." );
			}

			if( HasTextureUsage( desc.usage, TextureUsage::RenderTarget ) && HasTextureUsage( desc.usage, TextureUsage::DepthStencil ) )
			{
				throw std::runtime_error( "A texture cannot be both RenderTarget and DepthStencil." );
			}

			if( HasTextureUsage( desc.usage, TextureUsage::DepthStencil ) && HasTextureUsage( desc.usage, TextureUsage::UnorderedAccess ) )
			{
				throw std::runtime_error( "DepthStencil textures cannot expose UAVs." );
			}

			if( !IsValidSampleCount( desc.sampleCount ) )
			{
				throw std::invalid_argument( "TextureDesc::sampleCount must be a non-zero power of two." );
			}

			if( desc.sampleCount > 1 )
			{
				if( desc.dimension != TextureDimension::Texture2D || desc.depthOrArraySize != 1 )
				{
					throw std::runtime_error( "Multisampled textures currently support only single-slice Texture2D resources." );
				}
				if( desc.countMipMap != 1 )
				{
					throw std::runtime_error( "Multisampled textures cannot contain mipmaps." );
				}
				if( desc.data != nullptr )
				{
					throw std::runtime_error( "Multisampled textures cannot be initialized from CPU data." );
				}
				if( HasTextureUsage( desc.usage, TextureUsage::UnorderedAccess ) )
				{
					throw std::runtime_error( "Multisampled textures cannot expose UAVs." );
				}
				if( !HasTextureUsage( desc.usage, TextureUsage::RenderTarget ) && !HasTextureUsage( desc.usage, TextureUsage::DepthStencil ) )
				{
					throw std::runtime_error( "A multisampled texture must be a render target or depth-stencil target." );
				}
			}

			if( desc.dimension == TextureDimension::Texture2D )
			{
				if( desc.depthOrArraySize != 1 )
				{
					throw std::runtime_error( "Texture2D resources require depthOrArraySize == 1. Use Texture2DArray for multiple slices." );
				}
			}
			else if( desc.dimension == TextureDimension::TextureCube )
			{
				if( desc.width != desc.height )
				{
					throw std::runtime_error( "TextureCube faces must be square." );
				}
				if( desc.depthOrArraySize != ourCubeMapFaceCount )
				{
					throw std::runtime_error( "TextureCube resources require exactly six faces." );
				}
				if( desc.usage != TextureUsage::Sampled )
				{
					throw std::runtime_error( "TextureCube resources currently support sampled usage only." );
				}
			}
			else if( desc.dimension == TextureDimension::Texture2DArray )
			{
				if( desc.depthOrArraySize == 0 )
				{
					throw std::runtime_error( "Texture2DArray resources require at least one array slice." );
				}
				if( desc.usage != TextureUsage::Sampled )
				{
					throw std::runtime_error( "Texture2DArray resources currently support sampled usage only." );
				}
			}

			if( desc.dimension != TextureDimension::Texture3D )
			{
				return;
			}

			if( desc.depthOrArraySize <= 1 )
			{
				throw std::runtime_error( "Texture3D resources require depthOrArraySize > 1." );
			}

			if( HasTextureUsage( desc.usage, TextureUsage::RenderTarget ) || HasTextureUsage( desc.usage, TextureUsage::DepthStencil ) )
			{
				throw std::runtime_error( "This lightweight API currently supports Texture3D only for SRV/UAV "
										  "usage." );
			}

			if( desc.data != nullptr )
			{
				throw std::runtime_error( "Texture3D CPU uploads are not implemented yet. Populate them with "
										  "compute or add a staging path." );
			}
		}

		[[nodiscard]] D3D12_RESOURCE_DESC BuildTextureResourceDesc( const TextureDesc& desc, const TextureResource& resource ) noexcept
		{
			if( desc.dimension == TextureDimension::Texture3D )
			{
				return CD3DX12_RESOURCE_DESC::Tex3D( resource.formats_.resource_,
					desc.width,
					desc.height,
					desc.depthOrArraySize,
					resource.mipLevels_,
					resource.usageFlags_ );
			}

			return CD3DX12_RESOURCE_DESC::Tex2D( resource.formats_.resource_,
				desc.width,
				desc.height,
				desc.depthOrArraySize,
				resource.mipLevels_,
				desc.sampleCount,
				0,
				resource.usageFlags_ );
		}

		[[nodiscard]] TextureResource PrepareTextureResource( const TextureDesc& desc, const TextureCreationPlan& creationPlan )
		{
			TextureResource resource;
			resource.width_ = desc.width;
			resource.height_ = desc.height;
			resource.mipLevels_ = creationPlan.mipLevels_;
			resource.depthOrArraySize_ = desc.depthOrArraySize;
			resource.dimension_ = desc.dimension;
			resource.format_ = desc.format;
			resource.formats_ = ResolveTextureFormats( desc, creationPlan.requiresTypedUavViews_ );
			resource.usageFlags_ = ResolveTextureResourceFlags( desc.usage, desc.additionalResourceFlags, creationPlan.requiresTypedUavViews_ );
			resource.currentState_ = desc.initialState;
			resource.isDepthFormat_ = TextureResource::IsDepthFormat( resource.formats_.dsv_ );
			resource.isStencilFormat_ = TextureResource::IsDepthStencilFormat( resource.formats_.dsv_ );

			if( creationPlan.requiresTypedUavViews_ && resource.formats_.uav_ == DXGI_FORMAT_UNKNOWN )
			{
				throw std::runtime_error( "This texture format cannot expose the typed UAVs required by the "
										  "requested usage." );
			}

			resource.desc_ = BuildTextureResourceDesc( desc, resource );
			return resource;
		}
	} // namespace

	void DeviceManager::CreateCommittedTextureResource( const TextureDesc& desc, TextureResource& resource )
	{
		const CD3DX12_HEAP_PROPERTIES heapProps( D3D12_HEAP_TYPE_DEFAULT );
		const D3D12_CLEAR_VALUE* clearValue = desc.useClearValue ? &desc.clearValue : nullptr;
		C_RESULT( device_->CreateCommittedResource( &heapProps,
					  D3D12_HEAP_FLAG_NONE,
					  &resource.desc_,
					  desc.initialState,
					  clearValue,
					  IID_PPV_ARGS( resource.resource_.GetAddressOf() ) ),
			"Failed to create texture resource." );
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DeviceManager::MakeBindlessCpuHandle( uint32_t index ) noexcept
	{
		D3D12_CPU_DESCRIPTOR_HANDLE handle = bindlessHeap_->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>( index ) * bindlessDescriptorSize_;
		return handle;
	}

	void DeviceManager::CreateTextureShaderResourceView( TextureResource& resource )
	{
		resource.srvIndex_ = AllocateBindlessDescriptor();
		resource.srvHandle_ = MakeBindlessCpuHandle( resource.srvIndex_ );

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		if( resource.isDepthFormat_ )
		{
			srvDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING( D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0,
				D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1 );
		}
		srvDesc.Format = resource.formats_.srv_;
		if( resource.dimension_ == TextureDimension::Texture3D )
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
			srvDesc.Texture3D.MipLevels = resource.mipLevels_;
		}
		else if( resource.dimension_ == TextureDimension::TextureCube )
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube.MipLevels = resource.mipLevels_;
		}
		else if( resource.dimension_ == TextureDimension::Texture2DArray )
		{
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			srvDesc.Texture2DArray.MipLevels = resource.mipLevels_;
			srvDesc.Texture2DArray.ArraySize = resource.depthOrArraySize_;
		}
		else
		{
			if( resource.desc_.SampleDesc.Count > 1 )
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
			}
			else
			{
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = resource.mipLevels_;
			}
		}
		device_->CreateShaderResourceView( resource.resource_.Get(), &srvDesc, resource.srvHandle_ );
	}

	void DeviceManager::CreateTextureBaseMipViews( TextureResource& resource )
	{
		resource.baseMipsUavCount_ = static_cast<uint16_t>( resource.mipLevels_ - 1u );
		resource.baseMipsUavBaseIndex_ = AllocateBindlessDescriptorRange( resource.baseMipsUavCount_ );
		for( uint16_t mipLevel = 1; mipLevel < resource.mipLevels_; ++mipLevel )
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.Format = resource.formats_.uav_;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			uavDesc.Texture2D.MipSlice = mipLevel;

			device_->CreateUnorderedAccessView( resource.resource_.Get(),
				nullptr,
				&uavDesc,
				MakeBindlessCpuHandle( resource.baseMipsUavBaseIndex_ + static_cast<uint32_t>( mipLevel - 1u ) ) );
		}
	}

	void DeviceManager::CreateTextureUnorderedAccessView( TextureResource& resource )
	{
		resource.uavIndex_ = AllocateBindlessDescriptor();
		resource.uavHandle_ = MakeBindlessCpuHandle( resource.uavIndex_ );

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = resource.formats_.uav_;
		uavDesc.ViewDimension = resource.dimension_ == TextureDimension::Texture3D ? D3D12_UAV_DIMENSION_TEXTURE3D : D3D12_UAV_DIMENSION_TEXTURE2D;
		if( resource.dimension_ == TextureDimension::Texture3D )
		{
			uavDesc.Texture3D.WSize = resource.depthOrArraySize_;
		}
		device_->CreateUnorderedAccessView( resource.resource_.Get(), nullptr, &uavDesc, resource.uavHandle_ );
	}

	void DeviceManager::CreateTextureRenderTargetView( TextureResource& resource )
	{
		resource.rtvIndex_ = AllocateRtvDescriptor();
		resource.rtvHandle_ = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
		resource.rtvHandle_.ptr += static_cast<SIZE_T>( resource.rtvIndex_ ) * rtvDescriptorSize_;

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = resource.formats_.rtv_;
		rtvDesc.ViewDimension = resource.desc_.SampleDesc.Count > 1 ? D3D12_RTV_DIMENSION_TEXTURE2DMS : D3D12_RTV_DIMENSION_TEXTURE2D;
		device_->CreateRenderTargetView( resource.resource_.Get(), &rtvDesc, resource.rtvHandle_ );
	}

	void DeviceManager::CreateTextureDepthStencilView( TextureResource& resource )
	{
		resource.dsvIndex_ = AllocateDsvDescriptor();
		resource.dsvHandle_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
		resource.dsvHandle_.ptr += static_cast<SIZE_T>( resource.dsvIndex_ ) * dsvDescriptorSize_;

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
		dsvDesc.Format = resource.formats_.dsv_;
		dsvDesc.ViewDimension = resource.desc_.SampleDesc.Count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
		device_->CreateDepthStencilView( resource.resource_.Get(), &dsvDesc, resource.dsvHandle_ );
	}

	RenderPipelineDesc::RenderPipelineDesc() noexcept
	{
		blendState = CD3DX12_BLEND_DESC( D3D12_DEFAULT );
		rasterizerState = CD3DX12_RASTERIZER_DESC( D3D12_DEFAULT );
		depthStencilState = CD3DX12_DEPTH_STENCIL_DESC( D3D12_DEFAULT );
	}

	RenderPipelineState::RenderPipelineState( RenderPipelineState&& other ) noexcept
	{
		*this = std::move( other );
	}

	RenderPipelineState& RenderPipelineState::operator=( RenderPipelineState&& other ) noexcept
	{
		if( this != &other )
		{
			pipelineState_ = std::move( other.pipelineState_ );
			topology_ = other.topology_;
			other.topology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
		return *this;
	}

	bool RenderPipelineState::Valid() const noexcept
	{
		return pipelineState_ != nullptr;
	}

	ComputePipelineState::ComputePipelineState( ComputePipelineState&& other ) noexcept
	{
		*this = std::move( other );
	}

	ComputePipelineState& ComputePipelineState::operator=( ComputePipelineState&& other ) noexcept
	{
		if( this != &other )
		{
			pipelineState_ = std::move( other.pipelineState_ );
		}
		return *this;
	}

	bool ComputePipelineState::Valid() const noexcept
	{
		return pipelineState_ != nullptr;
	}

	RenderDevice::RenderDevice( DeviceManager& manager ) noexcept : manager_( &manager )
	{
	}

	CommandBuffer& RenderDevice::AcquireCommandBuffer()
	{
		DeviceManager& manager = *manager_;
		DeviceManager::QueueContext& graphicsQueue = manager.GetGraphicsQueueContext();
		manager.ProcessDeferredReleases();
		return graphicsQueue.immediateCommands_->AcquireCommandBuffer( manager );
	}

	TextureHandle RenderDevice::GetCurrentSwapchainTexture( SwapchainHandle swapchain ) const
	{
		if( !swapchain.Valid() )
		{
			swapchain = manager_->primarySwapchain_;
		}

		Swapchain* nativeSwapchain = manager_->GetSwapchain( swapchain );
		if( nativeSwapchain == nullptr )
		{
			return {};
		}

		return nativeSwapchain->GetCurrentTexture();
	}

	SubmitHandle RenderDevice::SubmitBatch( CommandBuffer* const* commandBuffers, uint32_t commandBufferCount, TextureHandle presentTexture ) const
	{
		return SubmitCommandBufferBatch( *manager_, commandBuffers, commandBufferCount, presentTexture );
	}

	SubmitHandle RenderDevice::Submit( CommandBuffer& buffer, TextureHandle presentTexture )
	{
		CommandBuffer* commandBuffers[] = { &buffer };
		return SubmitBatch( commandBuffers, 1, presentTexture );
	}

	SubmitHandle RenderDevice::Submit( CommandBuffer& buffer ) const
	{
		CommandBuffer* commandBuffers[] = { &buffer };
		return SubmitBatch( commandBuffers, 1 );
	}
	SubmitHandle RenderDevice::SubmitAndPresent( CommandBuffer& buffer, SwapchainHandle swapchain )
	{
		const TextureHandle presentTexture = GetCurrentSwapchainTexture( swapchain );
		return Submit( buffer, presentTexture );
	}
	void RenderDevice::Present( SwapchainHandle swapchain ) const
	{
		if( swapchain.Valid() == false )
		{
			swapchain = manager_->primarySwapchain_;
		}
		if( Swapchain* nativeSwapchain = manager_->GetSwapchain( swapchain ) )
		{
			const TextureHandle textureBackBuffer = nativeSwapchain->GetCurrentTexture();

			TextureResource& presentBackBufferResource = manager_->GetTextureResource( textureBackBuffer );

			if( presentBackBufferResource.currentState_ != D3D12_RESOURCE_STATE_PRESENT )
			{
				throw std::runtime_error( "Present requires the current swapchain texture to already be in D3D12_RESOURCE_STATE_PRESENT. "
										  "Transition it before calling Present, or use SubmitAndPresent()." );
			}

			nativeSwapchain->Present();
			manager_->ProcessDeferredReleases();
		}
	}

	bool RenderDevice::IsReady( SubmitHandle submission ) const
	{
		DeviceManager::QueueContext& graphicsQueue = manager_->GetGraphicsQueueContext();
		return graphicsQueue.immediateCommands_->IsReady( submission );
	}

	void RenderDevice::Wait( SubmitHandle submission ) const
	{
		DeviceManager::QueueContext& graphicsQueue = manager_->GetGraphicsQueueContext();
		graphicsQueue.immediateCommands_->Wait( submission );
		manager_->ProcessDeferredReleases( graphicsQueue );
	}

	RenderPipelineState RenderDevice::CreateRenderPipeline( const RenderPipelineDesc& desc )
	{
		if( desc.vertexShader.source == nullptr || desc.fragmentShader.source == nullptr )
		{
			throw std::runtime_error( "RenderPipelineDesc requires valid vertex and fragment shader source." );
		}
		if( !IsValidSampleCount( desc.sampleCount ) )
		{
			throw std::invalid_argument( "RenderPipelineDesc::sampleCount must be a non-zero power of two." );
		}

		const CompiledShader vertexShader = CompileShader( desc.vertexShader, "vs_6_6" );
		const CompiledShader fragmentShader = CompileShader( desc.fragmentShader, "ps_6_6" );

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = manager_->rootSignature_.Get();
		psoDesc.VS = vertexShader.Bytecode();
		psoDesc.PS = fragmentShader.Bytecode();
		psoDesc.BlendState = desc.blendState;
		psoDesc.RasterizerState = desc.rasterizerState;
		psoDesc.DepthStencilState = desc.depthStencilState;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = desc.primitiveType;

		std::array<D3D12_INPUT_ELEMENT_DESC, ourMaxVertexInputElements> nativeInputElements = {};
		uint32_t nativeInputElementCount = 0;
		for( const VertexInputElementDesc& inputElement : desc.inputElements )
		{
			if( inputElement.semanticName.empty() )
			{
				continue;
			}

			D3D12_INPUT_ELEMENT_DESC nativeInputElement{};
			nativeInputElement.SemanticName = inputElement.semanticName.c_str();
			nativeInputElement.SemanticIndex = inputElement.semanticIndex;
			nativeInputElement.Format = inputElement.format;
			nativeInputElement.InputSlot = inputElement.inputSlot;
			nativeInputElement.AlignedByteOffset = inputElement.alignedByteOffset;
			nativeInputElement.InputSlotClass = inputElement.inputClassification;
			nativeInputElement.InstanceDataStepRate = inputElement.instanceDataStepRate;
			nativeInputElements[ nativeInputElementCount++ ] = nativeInputElement;
		}
		psoDesc.InputLayout.pInputElementDescs = nativeInputElements.data();
		psoDesc.InputLayout.NumElements = nativeInputElementCount;

		uint32_t numRenderTargets = 0;
		for( uint32_t index = 0; index < desc.color.size(); ++index )
		{
			if( desc.color[ index ].format == DXGI_FORMAT_UNKNOWN )
			{
				continue;
			}

			psoDesc.RTVFormats[ numRenderTargets ] = desc.color[ index ].format;
			numRenderTargets++;
		}

		if( numRenderTargets == 0 && desc.colorFormat != DXGI_FORMAT_UNKNOWN )
		{
			psoDesc.RTVFormats[ 0 ] = desc.colorFormat;
			numRenderTargets = 1;
		}

		for( uint32_t index = 0; index < numRenderTargets; ++index )
		{
			if( !SupportsSampleCount( psoDesc.RTVFormats[ index ], desc.sampleCount ) )
			{
				throw std::runtime_error( "The selected color format does not support the requested pipeline sample count." );
			}
		}
		if( desc.depthFormat != DXGI_FORMAT_UNKNOWN && !SupportsSampleCount( desc.depthFormat, desc.sampleCount ) )
		{
			throw std::runtime_error( "The selected depth format does not support the requested pipeline sample count." );
		}

		psoDesc.NumRenderTargets = numRenderTargets;
		psoDesc.DSVFormat = desc.depthFormat;
		psoDesc.SampleDesc = { desc.sampleCount, 0 };
		if( desc.sampleCount > 1 )
		{
			psoDesc.RasterizerState.MultisampleEnable = TRUE;
		}

		RenderPipelineState pipeline;
		C_RESULT( manager_->device_->CreateGraphicsPipelineState( &psoDesc, IID_PPV_ARGS( pipeline.pipelineState_.GetAddressOf() ) ),
			"Failed to create graphics pipeline state." );
		pipeline.topology_ = desc.topology;
		return pipeline;
	}

	ComputePipelineState RenderDevice::CreateComputePipeline( const ComputePipelineDesc& desc )
	{
		if( desc.computeShader.source == nullptr )
		{
			throw std::runtime_error( "ComputePipelineDesc requires a valid compute shader source." );
		}

		const CompiledShader computeShader = CompileShader( desc.computeShader, "cs_6_6" );

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = manager_->rootSignature_.Get();
		psoDesc.CS = computeShader.Bytecode();

		ComputePipelineState pipeline;
		C_RESULT( manager_->device_->CreateComputePipelineState( &psoDesc, IID_PPV_ARGS( pipeline.pipelineState_.GetAddressOf() ) ),
			"Failed to create compute pipeline state." );
		return pipeline;
	}

	BufferHandle RenderDevice::CreateBuffer( const BufferDesc& desc )
	{
		return CreateBufferInternal( desc, UINT32_MAX, UINT32_MAX );
	}

	BufferHandle RenderDevice::CreateBuffer( const BufferDesc& desc, ConstantBufferSlot slot )
	{
		if( !IsValidConstantBufferSlot( slot ) )
		{
			throw std::invalid_argument( "Invalid constant buffer slot." );
		}
		if( desc.type != BufferType::Constant )
		{
			throw std::invalid_argument( "A fixed constant-buffer slot requires Constant buffer usage." );
		}
		return CreateBufferInternal( desc, ToSlotIndex( slot ), UINT32_MAX );
	}

	BufferHandle RenderDevice::CreateBuffer( const BufferDesc& desc, ShaderResourceSlot slot )
	{
		if( !IsValidShaderResourceSlot( slot ) )
		{
			throw std::invalid_argument( "Invalid shader resource slot." );
		}
		if( desc.type != BufferType::Structured && desc.type != BufferType::Raw )
		{
			throw std::invalid_argument( "A fixed shader-resource slot requires Structured or Raw buffer usage." );
		}
		return CreateBufferInternal( desc, UINT32_MAX, ToSlotIndex( slot ) );
	}

	void RenderDevice::ValidateBufferDesc( const BufferDesc& desc ) const
	{
		constexpr uint64_t maximumConstantBufferSize = 64ull * 1024ull;
		if( desc.size == 0 )
		{
			throw std::invalid_argument( "BufferDesc.size must be greater than zero." );
		}

		if( desc.type == BufferType::Constant && desc.size > maximumConstantBufferSize )
		{
			throw std::length_error( "Constant buffer views are limited to 64 KiB. Use an SRV/StructuredBuffer for larger data." );
		}
		if( ( desc.type == BufferType::Vertex || desc.type == BufferType::Index ) && desc.size > std::numeric_limits<uint32_t>::max() )
		{
			throw std::length_error( "Vertex and index buffers are limited to 4 GiB by their D3D12 views." );
		}
		if( desc.type == BufferType::Structured )
		{
			if( desc.stride == 0 )
			{
				throw std::invalid_argument( "Structured buffers require a non-zero stride." );
			}
			if( desc.stride % sizeof( uint32_t ) != 0 || desc.stride > D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES )
			{
				throw std::invalid_argument( "Structured buffer stride must be a multiple of 4 bytes and no greater than 2,048 bytes." );
			}
			if( desc.size % desc.stride != 0 )
			{
				throw std::invalid_argument( "Structured buffer size must be a multiple of its stride." );
			}
			if( desc.size / desc.stride > std::numeric_limits<uint32_t>::max() )
			{
				throw std::length_error( "Structured buffer element count exceeds the D3D12 SRV limit." );
			}
		}
		if( desc.type == BufferType::Raw )
		{
			if( desc.stride != 0 )
			{
				throw std::invalid_argument( "Raw buffers do not use a structured stride." );
			}
			if( desc.size % sizeof( uint32_t ) != 0 )
			{
				throw std::invalid_argument( "Raw buffer size must be aligned to 32-bit values." );
			}
			if( desc.size / sizeof( uint32_t ) > std::numeric_limits<uint32_t>::max() )
			{
				throw std::length_error( "Raw buffer element count exceeds the D3D12 SRV limit." );
			}
		}
	}

	BufferResource RenderDevice::CreateBufferResource( const BufferDesc& desc )
	{
		const uint64_t resourceSize = desc.type == BufferType::Constant ? AlignUp( desc.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT ) : desc.size;

		BufferResource resource;
		resource.bufferSize_ = desc.size;
		resource.bufferStride_ = desc.stride;
		resource.type_ = desc.type;
		resource.memory_ = desc.memory;
		resource.desc_ = BufferResource::CreateNativeDesc( resourceSize );

		if( desc.memory == BufferMemory::CpuToGpu )
		{
			resource.currentState_ = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		else
		{
			switch( desc.type )
			{
			case BufferType::Vertex:
			case BufferType::Constant:
				resource.currentState_ = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
				break;
			case BufferType::Index:
				resource.currentState_ = D3D12_RESOURCE_STATE_INDEX_BUFFER;
				break;
			case BufferType::Structured:
			case BufferType::Raw:
				resource.currentState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
				break;
			case BufferType::Indirect:
				resource.currentState_ = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
				break;
			case BufferType::Generic:
				resource.currentState_ = D3D12_RESOURCE_STATE_COMMON;
				break;
			}
		}

		const D3D12_HEAP_TYPE heapType = desc.memory == BufferMemory::CpuToGpu ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
		const CD3DX12_HEAP_PROPERTIES heapProps( heapType );
		C_RESULT( manager_->device_->CreateCommittedResource( &heapProps,
					  D3D12_HEAP_FLAG_NONE,
					  &resource.desc_,
					  resource.currentState_,
					  nullptr,
					  IID_PPV_ARGS( resource.resource_.GetAddressOf() ) ),
			"Failed to create buffer resource." );

		resource.gpuAddress_ = resource.resource_->GetGPUVirtualAddress();
		if( desc.memory == BufferMemory::CpuToGpu )
		{
			resource.resource_->Map( 0, nullptr, &resource.mappedPtr_ );
		}
		return resource;
	}

	void RenderDevice::CreateBufferDescriptors( BufferResource& resource, const BufferDesc& desc, uint32_t constantBufferSlot, uint32_t shaderResourceSlot )
	{
		if( desc.type == BufferType::Structured || desc.type == BufferType::Raw )
		{
			resource.srvIndex_ =
				shaderResourceSlot != UINT32_MAX ? manager_->AllocateFixedBindlessDescriptor( shaderResourceSlot ) : manager_->AllocateBindlessDescriptor();
			resource.srvHandle_ = manager_->MakeBindlessCpuHandle( resource.srvIndex_ );

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			if( desc.type == BufferType::Raw )
			{
				srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				srvDesc.Buffer.NumElements = static_cast<UINT>( desc.size / sizeof( uint32_t ) );
				srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
			}
			else
			{
				srvDesc.Format = DXGI_FORMAT_UNKNOWN;
				srvDesc.Buffer.StructureByteStride = desc.stride;
				srvDesc.Buffer.NumElements = static_cast<UINT>( desc.size / desc.stride );
			}

			manager_->device_->CreateShaderResourceView( resource.resource_.Get(), &srvDesc, resource.srvHandle_ );
		}

		if( desc.type == BufferType::Constant )
		{
			resource.cbvIndex_ =
				constantBufferSlot != UINT32_MAX ? manager_->AllocateFixedBindlessDescriptor( constantBufferSlot ) : manager_->AllocateBindlessDescriptor();
			resource.cbvHandle_ = manager_->MakeBindlessCpuHandle( resource.cbvIndex_ );

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
			cbvDesc.BufferLocation = resource.gpuAddress_;
			cbvDesc.SizeInBytes = static_cast<UINT>( resource.desc_.Width );
			manager_->device_->CreateConstantBufferView( &cbvDesc, resource.cbvHandle_ );
		}
	}

	BufferHandle RenderDevice::CreateBufferInternal( const BufferDesc& desc, uint32_t constantBufferSlot, uint32_t shaderResourceSlot )
	{
		ValidateBufferDesc( desc );
		BufferResource resource = CreateBufferResource( desc );
		CreateBufferDescriptors( resource, desc, constantBufferSlot, shaderResourceSlot );

		if( desc.initialData != nullptr )
		{
			manager_->stagingDevice_->BufferSubData( resource, 0, static_cast<size_t>( desc.size ), desc.initialData );
		}

		return manager_->slotMapBuffers_.Create( std::move( resource ) );
	}

	void RenderDevice::WriteBuffer( BufferHandle buffer, uint64_t offset, const void* data, uint64_t size )
	{
		if( size == 0 )
		{
			return;
		}
		if( data == nullptr )
		{
			throw std::runtime_error( "WriteBuffer requires a valid data pointer." );
		}

		BufferResource& resource = manager_->GetBufferResource( buffer );
		if( offset > resource.bufferSize_ || size > resource.bufferSize_ - offset )
		{
			throw std::runtime_error( "WriteBuffer range exceeds buffer size." );
		}

		manager_->stagingDevice_->BufferSubData( resource, static_cast<size_t>( offset ), static_cast<size_t>( size ), data );
	}

	TextureHandle RenderDevice::CreateTexture( const TextureDesc& desc )
	{
		ValidateTextureDesc( desc );
		if( !SupportsSampleCount( desc.format, desc.sampleCount ) )
		{
			throw std::runtime_error( "The selected texture format does not support the requested sample count." );
		}
		const TextureCreationPlan creationPlan = BuildTextureCreationPlan( desc );
		TextureResource resource = PrepareTextureResource( desc, creationPlan );
		manager_->CreateCommittedTextureResource( desc, resource );

		if( HasTextureUsage( desc.usage, TextureUsage::Sampled ) )
		{
			manager_->CreateTextureShaderResourceView( resource );
		}

		if( creationPlan.generateInitialMipChain_ )
		{
			manager_->CreateTextureBaseMipViews( resource );
		}

		if( HasTextureUsage( desc.usage, TextureUsage::UnorderedAccess ) )
		{
			manager_->CreateTextureUnorderedAccessView( resource );
		}

		if( HasTextureUsage( desc.usage, TextureUsage::RenderTarget ) )
		{
			manager_->CreateTextureRenderTargetView( resource );
		}

		if( HasTextureUsage( desc.usage, TextureUsage::DepthStencil ) )
		{
			manager_->CreateTextureDepthStencilView( resource );
		}

		const TextureHandle handle = manager_->slotMapTextures_.Create( std::move( resource ) );
		if( desc.data != nullptr && desc.rowPitch > 0 && desc.slicePitch > 0 )
		{
			TextureResource& textureResource = manager_->GetTextureResource( handle );
			manager_->stagingDevice_->TextureSubData( textureResource, desc.data, desc.rowPitch, desc.slicePitch );
			if( creationPlan.generateInitialMipChain_ )
			{
				manager_->baseMips_->Generate( textureResource, desc.initialState );
			}
		}

		return handle;
	}

	SamplerHandle RenderDevice::CreateSampler( const SamplerDesc& desc )
	{
		if( desc.maxAnisotropy == 0 || desc.maxAnisotropy > 16 )
		{
			throw std::invalid_argument( "Sampler maxAnisotropy must be between 1 and 16." );
		}
		if( desc.minLod > desc.maxLod )
		{
			throw std::invalid_argument( "Sampler minLod cannot be greater than maxLod." );
		}
		if( manager_->slotMapSamplers_.NumObjects() == ourCustomSamplerCount )
		{
			throw std::length_error( "All four custom sampler slots are already in use." );
		}

		const SamplerHandle handle = manager_->slotMapSamplers_.Create( SamplerResource{} );
		SamplerResource* resource = manager_->slotMapSamplers_.Get( handle );
		assert( resource != nullptr );
		resource->descriptorIndex_ = LDX12_CUSTOM_SAMPLER_SLOT_FIRST + handle.Index();
		manager_->WriteSamplerDescriptor( resource->descriptorIndex_, desc );
		return handle;
	}

	void RenderDevice::DownloadTexture2D( TextureHandle texture, void* outData, uint32_t rowPitch, uint32_t slicePitch )
	{
		TextureResource& textureResource = manager_->GetTextureResource( texture );
		manager_->stagingDevice_->TextureData2D( textureResource, outData, rowPitch, slicePitch );
	}

	ConstantBufferSlot RenderDevice::GetAvailableConstantBuffer()
	{
		return AllocateFreeBindingSlot<ConstantBufferSlot>( manager_->allocatedFreeBindingSlots_.constantBuffer_,
			LDX12_FREE_CBV_SLOT_FIRST,
			LDX12_FREE_CBV_SLOT_COUNT,
			"No free constant buffer slots are available." );
	}

	ShaderResourceSlot RenderDevice::GetAvailableShaderResource()
	{
		return AllocateFreeBindingSlot<ShaderResourceSlot>( manager_->allocatedFreeBindingSlots_.shaderResource_,
			LDX12_FREE_SRV_SLOT_FIRST,
			LDX12_FREE_SRV_SLOT_COUNT,
			"No free shader resource slots are available." );
	}

	ReadWriteResourceSlot RenderDevice::GetAvailableReadWriteResource()
	{
		return AllocateFreeBindingSlot<ReadWriteResourceSlot>( manager_->allocatedFreeBindingSlots_.readWriteResource_,
			LDX12_FREE_RW_SLOT_FIRST,
			LDX12_FREE_RW_SLOT_COUNT,
			"No free read/write resource slots are available." );
	}

	uint32_t RenderDevice::GetConstantBufferIndex( BufferHandle buffer ) const
	{
		return ToPublicDescriptorIndex( manager_->GetBufferResource( buffer ).cbvIndex_ );
	}

	uint32_t RenderDevice::GetBindlessIndex( BufferHandle buffer ) const
	{
		return ToPublicDescriptorIndex( manager_->GetBufferResource( buffer ).srvIndex_ );
	}

	uint32_t RenderDevice::GetBindlessIndex( TextureHandle texture ) const
	{
		return ToPublicDescriptorIndex( manager_->GetTextureResource( texture ).srvIndex_ );
	}

	uint32_t RenderDevice::GetUnorderedAccessIndex( TextureHandle texture ) const
	{
		return ToPublicDescriptorIndex( manager_->GetTextureResource( texture ).uavIndex_ );
	}

	uint32_t RenderDevice::GetSamplerIndex( SamplerHandle sampler ) const
	{
		const SamplerResource* resource = manager_->slotMapSamplers_.Get( sampler );
		if( resource == nullptr )
		{
			throw std::runtime_error( "Invalid sampler handle." );
		}
		return resource->descriptorIndex_;
	}

	bool RenderDevice::SupportsSampleCount( DXGI_FORMAT format, uint32_t sampleCount ) const noexcept
	{
		return SupportsTextureSampleCount( manager_->device_.Get(), format, sampleCount );
	}

	bool RenderDevice::BindlessSupported() const noexcept
	{
		return manager_->deviceProperties_.resourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3;
	}

	bool RenderDevice::IsAlive( BufferHandle buffer ) const noexcept
	{
		return manager_ != nullptr && manager_->slotMapBuffers_.Contains( buffer );
	}

	bool RenderDevice::IsAlive( TextureHandle texture ) const noexcept
	{
		return manager_ != nullptr && manager_->slotMapTextures_.Contains( texture );
	}

	bool RenderDevice::IsAlive( SamplerHandle sampler ) const noexcept
	{
		return manager_ != nullptr && manager_->slotMapSamplers_.Contains( sampler );
	}

	void RenderDevice::WaitIdle()
	{
		manager_->WaitIdle();
	}

	bool RenderDevice::Destroy( BufferHandle buffer )
	{
		DeviceManager& manager = *manager_;
		BufferResource* resource = manager.slotMapBuffers_.Get( buffer );
		if( resource == nullptr )
		{
			ReportInvalidDestroy( "BufferHandle", buffer.Index(), buffer.Gen() );
			return false;
		}

		DeviceManager::QueueContext& graphicsQueue = manager.GetGraphicsQueueContext();
		const SubmitHandle releaseHandle =
			graphicsQueue.immediateCommands_ != nullptr ? graphicsQueue.immediateCommands_->GetLastSubmitHandle() : SubmitHandle{};
		ComPtr<ID3D12Resource> nativeResource = std::move( resource->resource_ );
		const bool wasMapped = resource->mappedPtr_ != nullptr;
		const uint32_t srvIndex = resource->srvIndex_;
		const uint32_t cbvIndex = resource->cbvIndex_;

		resource->mappedPtr_ = nullptr;
		resource->srvIndex_ = UINT32_MAX;
		resource->cbvIndex_ = UINT32_MAX;
		manager.slotMapBuffers_.Destroy( buffer );

		std::function<void()> release = [ &manager, nativeResource = std::move( nativeResource ), wasMapped, srvIndex, cbvIndex ]() mutable
		{
			if( nativeResource != nullptr && wasMapped )
			{
				nativeResource->Unmap( 0, nullptr );
			}

			manager.FreeBindlessDescriptor( srvIndex );
			manager.FreeBindlessDescriptor( cbvIndex );
			nativeResource.Reset();
		};

		if( graphicsQueue.immediateCommands_ == nullptr || releaseHandle.Empty() || graphicsQueue.immediateCommands_->IsReady( releaseHandle ) )
		{
			release();
		}
		else
		{
			manager.AddDeferredRelease( releaseHandle, std::move( release ) );
		}

		return true;
	}

	bool RenderDevice::Destroy( TextureHandle texture )
	{
		DeviceManager& manager = *manager_;
		TextureResource* resource = manager.slotMapTextures_.Get( texture );
		if( resource == nullptr )
		{
			ReportInvalidDestroy( "TextureHandle", texture.Index(), texture.Gen() );
			return false;
		}
		if( resource->isSwapchainImage_ )
			throw std::runtime_error( "Cannot destroy a swapchain texture directly. Destroy "
									  "the owning swapchain instead." );

		DeviceManager::QueueContext& graphicsQueue = manager.GetGraphicsQueueContext();
		const SubmitHandle releaseHandle =
			graphicsQueue.immediateCommands_ != nullptr ? graphicsQueue.immediateCommands_->GetLastSubmitHandle() : SubmitHandle{};

		ComPtr<ID3D12Resource> nativeResource = std::move( resource->resource_ );
		const uint32_t srvIndex = resource->srvIndex_;
		const uint32_t uavIndex = resource->uavIndex_;
		const uint32_t rtvIndex = resource->rtvIndex_;
		const uint32_t dsvIndex = resource->dsvIndex_;
		const uint32_t baseMipsUavBaseIndex = resource->baseMipsUavBaseIndex_;
		const uint32_t baseMipsUavCount = resource->baseMipsUavCount_;

		manager.slotMapTextures_.Destroy( texture );

		std::function<void()> release =
			[ &manager, nativeResource = std::move( nativeResource ), srvIndex, uavIndex, rtvIndex, dsvIndex, baseMipsUavBaseIndex, baseMipsUavCount ]() mutable
		{
			manager.FreeBindlessDescriptor( srvIndex );
			manager.FreeBindlessDescriptor( uavIndex );
			manager.FreeBindlessDescriptorRange( baseMipsUavBaseIndex, baseMipsUavCount );
			manager.FreeRtvDescriptor( rtvIndex );
			manager.FreeDsvDescriptor( dsvIndex );
			nativeResource.Reset();
		};

		if( graphicsQueue.immediateCommands_ == nullptr || releaseHandle.Empty() || graphicsQueue.immediateCommands_->IsReady( releaseHandle ) )
		{
			release();
		}
		else
		{
			manager.AddDeferredRelease( releaseHandle, std::move( release ) );
		}

		return true;
	}

	bool RenderDevice::Destroy( SamplerHandle sampler )
	{
		if( !manager_->slotMapSamplers_.Contains( sampler ) )
		{
			ReportInvalidDestroy( "SamplerHandle", sampler.Index(), sampler.Gen() );
			return false;
		}

		// The next CreateSampler() may overwrite this descriptor slot.
		manager_->WaitIdle();
		return manager_->slotMapSamplers_.Destroy( sampler );
	}
} // namespace ldx12
