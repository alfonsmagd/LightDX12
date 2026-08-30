#include "Ldx12CommandBuffer.hpp"
#include "Ldx12ImmediateCommands.hpp"

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <mutex>

#if defined( LDX12_ENABLE_PIX )
	#define LDX12_INTERNAL_PIX_ENABLED 1
#else
	#define LDX12_INTERNAL_PIX_ENABLED 1
#endif

namespace ldx12
{
	namespace
	{
#if LDX12_INTERNAL_PIX_ENABLED
		std::array<uint32_t, 4> ParseVersionComponents( const std::wstring& versionText )
		{
			std::array<uint32_t, 4> components = {};
			uint32_t componentIndex = 0;
			size_t start = 0;
			while( start < versionText.size() && componentIndex < components.size() )
			{
				const size_t end = versionText.find( L'.', start );
				const std::wstring token = versionText.substr( start, end == std::wstring::npos ? std::wstring::npos : end - start );
				components[ componentIndex++ ] = token.empty() ? 0u : static_cast<uint32_t>( std::wcstoul( token.c_str(), nullptr, 10 ) );

				if( end == std::wstring::npos )
				{
					break;
				}

				start = end + 1;
			}

			return components;
		}

		bool IsVersionGreater( const std::array<uint32_t, 4>& left, const std::array<uint32_t, 4>& right )
		{
			for( size_t index = 0; index < left.size(); ++index )
			{
				const uint32_t leftValue = left[ index ];
				const uint32_t rightValue = right[ index ];
				if( leftValue != rightValue )
				{
					return leftValue > rightValue;
				}
			}

			return false;
		}

		std::filesystem::path FindPixEventRuntimePath()
		{
			// First allow a local copy next to the executable or on PATH.
			if( ::GetModuleHandleW( L"WinPixEventRuntime.dll" ) != nullptr )
			{
				return L"WinPixEventRuntime.dll";
			}

			// Then try the standard NuGet cache location, which is where the PIX event runtime is commonly installed.
			wchar_t* userProfileValue = nullptr;
			size_t userProfileLength = 0;
			if( _wdupenv_s( &userProfileValue, &userProfileLength, L"USERPROFILE" ) == 0 && userProfileValue != nullptr && userProfileValue[ 0 ] != L'\0' )
			{
				const std::filesystem::path runtimeRoot = std::filesystem::path( userProfileValue ) / ".nuget" / "packages" / "winpixeventruntime";
				free( userProfileValue );
				if( std::filesystem::exists( runtimeRoot ) )
				{
					std::filesystem::path latestRuntimePath;
					std::array<uint32_t, 4> latestVersion = {};
					for( const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator( runtimeRoot ) )
					{
						if( !entry.is_directory() )
						{
							continue;
						}

						const std::filesystem::path candidateDll = entry.path() / "bin" / "x64" / "WinPixEventRuntime.dll";
						if( !std::filesystem::exists( candidateDll ) )
						{
							continue;
						}

						const std::array<uint32_t, 4> candidateVersion = ParseVersionComponents( entry.path().filename().wstring() );
						if( latestRuntimePath.empty() || IsVersionGreater( candidateVersion, latestVersion ) )
						{
							latestRuntimePath = candidateDll;
							latestVersion = candidateVersion;
						}
					}

					if( !latestRuntimePath.empty() )
					{
						return latestRuntimePath;
					}
				}
			}
			else
			{
				free( userProfileValue );
			}

			return {};
		}

		struct PixRuntime final
		{
			using BeginEventOnCommandListFn = void( WINAPI* )( ID3D12GraphicsCommandList*, UINT64, PCSTR );
			using EndEventOnCommandListFn = void( WINAPI* )( ID3D12GraphicsCommandList* );

			~PixRuntime()
			{
				if( module_ != nullptr )
				{
					::FreeLibrary( module_ );
				}
			}

			bool Available()
			{
				std::call_once( initFlag_,
					[ this ]()
					{
						const std::filesystem::path runtimePath = FindPixEventRuntimePath();
						if( runtimePath.empty() )
						{
							return;
						}

						module_ = ::LoadLibraryW( runtimePath.c_str() );
						if( module_ == nullptr )
						{
							return;
						}

						beginEventOnCommandList_ = reinterpret_cast<BeginEventOnCommandListFn>( ::GetProcAddress( module_, "PIXBeginEventOnCommandList" ) );
						endEventOnCommandList_ = reinterpret_cast<EndEventOnCommandListFn>( ::GetProcAddress( module_, "PIXEndEventOnCommandList" ) );
						if( beginEventOnCommandList_ == nullptr || endEventOnCommandList_ == nullptr )
						{
							::FreeLibrary( module_ );
							module_ = nullptr;
							beginEventOnCommandList_ = nullptr;
							endEventOnCommandList_ = nullptr;
							return;
						}

						available_ = true;
					} );

				return available_;
			}

			void BeginEvent( ID3D12GraphicsCommandList* commandList, uint64_t color, const char* label )
			{
				if( Available() )
				{
					beginEventOnCommandList_( commandList, color, label );
				}
			}

			void EndEvent( ID3D12GraphicsCommandList* commandList )
			{
				if( Available() )
				{
					endEventOnCommandList_( commandList );
				}
			}

		private:
			std::once_flag initFlag_;
			HMODULE module_ = nullptr;
			BeginEventOnCommandListFn beginEventOnCommandList_ = nullptr;
			EndEventOnCommandListFn endEventOnCommandList_ = nullptr;
			bool available_ = false;
		};

		PixRuntime& GetPixRuntime()
		{
			static PixRuntime runtime;
			return runtime;
		}
#endif

		D3D12_RENDER_PASS_BEGINNING_ACCESS CreateBeginningAccess( LoadOp loadOp, DXGI_FORMAT format, const std::array<float, 4>& clearColor )
		{
			D3D12_RENDER_PASS_BEGINNING_ACCESS beginningAccess{};

			switch( loadOp )
			{
			case LoadOp::Load:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
				break;

			case LoadOp::Clear:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
				beginningAccess.Clear.ClearValue.Format = format;
				std::memcpy( beginningAccess.Clear.ClearValue.Color, clearColor.data(), sizeof( float ) * clearColor.size() );
				break;

			case LoadOp::DontCare:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
				break;
			}

			return beginningAccess;
		}

		D3D12_RENDER_PASS_BEGINNING_ACCESS CreateDepthBeginningAccess( LoadOp loadOp, DXGI_FORMAT format, float clearDepth )
		{
			D3D12_RENDER_PASS_BEGINNING_ACCESS beginningAccess{};

			switch( loadOp )
			{
			case LoadOp::Load:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
				break;

			case LoadOp::Clear:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
				beginningAccess.Clear.ClearValue.DepthStencil.Depth = clearDepth;
				beginningAccess.Clear.ClearValue.Format = format;
				break;

			case LoadOp::DontCare:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
				break;
			}

			return beginningAccess;
		}

		D3D12_RENDER_PASS_BEGINNING_ACCESS CreateStencilBeginningAccess( LoadOp loadOp, DXGI_FORMAT format, uint8_t clearStencil )
		{
			D3D12_RENDER_PASS_BEGINNING_ACCESS beginningAccess{};

			switch( loadOp )
			{
			case LoadOp::Load:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
				break;

			case LoadOp::Clear:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
				beginningAccess.Clear.ClearValue.DepthStencil.Stencil = clearStencil;
				beginningAccess.Clear.ClearValue.Format = format;
				break;

			case LoadOp::DontCare:
				beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
				break;
			}

			return beginningAccess;
		}

		D3D12_RENDER_PASS_ENDING_ACCESS CreateEndingAccess( StoreOp storeOp )
		{
			D3D12_RENDER_PASS_ENDING_ACCESS endingAccess{};
			endingAccess.Type = storeOp == StoreOp::Store ? D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE : D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
			return endingAccess;
		}

		D3D12_RENDER_PASS_BEGINNING_ACCESS CreateNoAccessBeginningAccess()
		{
			D3D12_RENDER_PASS_BEGINNING_ACCESS beginningAccess{};
			beginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
			return beginningAccess;
		}

		D3D12_RENDER_PASS_ENDING_ACCESS CreateNoAccessEndingAccess()
		{
			D3D12_RENDER_PASS_ENDING_ACCESS endingAccess{};
			endingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
			return endingAccess;
		}
	}

	void CommandBufferImpl::Begin( DeviceManager& manager, CommandListWrapper& wrapper ) noexcept
	{
		assert( !active_ );
		manager_ = &manager;
		wrapper_ = &wrapper;
		isRendering_ = false;
		active_ = true;
		debugGroupDepth_ = 0;
		trackedTextureCount_ = 0;

		ID3D12DescriptorHeap* heaps[] = { manager_->bindlessHeap_.Get(), manager_->samplerHeap_.Get() };
		wrapper_->commandList_->SetDescriptorHeaps( static_cast<UINT>( std::size( heaps ) ), heaps );

		ID3D12RootSignature* rootSignature = manager_->rootSignature_.Get();
		wrapper_->commandList_->SetGraphicsRootSignature( rootSignature );
		wrapper_->commandList_->SetComputeRootSignature( rootSignature );
	}

	void CommandBufferImpl::Release() noexcept
	{
		assert( active_ );
		assert( !isRendering_ );
		manager_ = nullptr;
		wrapper_ = nullptr;
		active_ = false;
		debugGroupDepth_ = 0;
		trackedTextureCount_ = 0;
	}

	CommandBufferImpl::TrackedTextureState& CommandBufferImpl::GetTrackedTextureState( TextureHandle texture )
	{
		for( uint32_t index = 0; index < trackedTextureCount_; ++index )
		{
			TrackedTextureState& trackedTexture = trackedTextures_[ index ];
			if( trackedTexture.handle_ == texture )
			{
				return trackedTexture;
			}
		}

		const TextureResource& resource = manager_->GetTextureResource( texture );
		TrackedTextureState trackedTexture;
		trackedTexture.handle_ = texture;
		trackedTexture.initialState_ = resource.currentState_;
		trackedTexture.currentState_ = resource.currentState_;
		if( trackedTextureCount_ == trackedTextures_.size() )
		{
			throw std::length_error( "A command buffer cannot track more than 256 textures." );
		}

		trackedTextures_[ trackedTextureCount_ ] = trackedTexture;
		return trackedTextures_[ trackedTextureCount_++ ];
	}

	void CommandBufferImpl::TransitionTexture( TextureHandle texture, TextureResource& resource, D3D12_RESOURCE_STATES newState )
	{
		TrackedTextureState& trackedTexture = GetTrackedTextureState( texture );
		if( trackedTexture.currentState_ == newState )
		{
			return;
		}

		const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( resource.resource_.Get(), trackedTexture.currentState_, newState );
		wrapper_->commandList_->ResourceBarrier( 1, &barrier );
		trackedTexture.currentState_ = newState;
	}

	CommandListWrapper* CommandBufferImpl::BuildSubmitFixup( CommandBufferImpl* const* previousCommandBuffers, uint32_t previousCommandBufferCount )
	{
		const auto getCurrentState = [ this, previousCommandBuffers, previousCommandBufferCount ]( TextureHandle texture )
		{
			for( uint32_t commandBufferIndex = previousCommandBufferCount; commandBufferIndex > 0; --commandBufferIndex )
			{
				assert( previousCommandBuffers != nullptr );
				const CommandBufferImpl* previousCommandBuffer = previousCommandBuffers[ commandBufferIndex - 1 ];
				assert( previousCommandBuffer != nullptr );
				const TrackedTextureState* trackedTextures = previousCommandBuffer->GetTrackedTextures();
				for( uint32_t textureIndex = 0; textureIndex < previousCommandBuffer->GetTrackedTextureCount(); ++textureIndex )
				{
					if( trackedTextures[ textureIndex ].handle_ == texture )
					{
						return trackedTextures[ textureIndex ].currentState_;
					}
				}
			}

			return manager_->GetTextureResource( texture ).currentState_;
		};

		bool requiresFixup = false;
		for( uint32_t index = 0; index < trackedTextureCount_; ++index )
		{
			const TrackedTextureState& trackedTexture = trackedTextures_[ index ];
			if( getCurrentState( trackedTexture.handle_ ) != trackedTexture.initialState_ )
			{
				requiresFixup = true;
				break;
			}
		}

		if( !requiresFixup )
		{
			return nullptr;
		}

		CommandListWrapper& fixup = manager_->GetGraphicsQueueContext().immediateCommands_->Acquire();

		for( uint32_t index = 0; index < trackedTextureCount_; ++index )
		{
			const TrackedTextureState& trackedTexture = trackedTextures_[ index ];
			const TextureResource& resource = manager_->GetTextureResource( trackedTexture.handle_ );
			const D3D12_RESOURCE_STATES currentState = getCurrentState( trackedTexture.handle_ );
			if( currentState == trackedTexture.initialState_ )
			{
				continue;
			}

			const D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition( resource.resource_.Get(), currentState, trackedTexture.initialState_ );
			fixup.commandList_->ResourceBarrier( 1, &barrier );
		}

		return &fixup;
	}

	void CommandBufferImpl::CommitSubmittedTextureStates()
	{
		for( uint32_t index = 0; index < trackedTextureCount_; ++index )
		{
			const TrackedTextureState& trackedTexture = trackedTextures_[ index ];
			TextureResource& resource = manager_->GetTextureResource( trackedTexture.handle_ );
			resource.currentState_ = trackedTexture.currentState_;
		}
	}

	void CommandBufferImpl::CmdBeginRendering( const RenderPass& renderPass, const Framebuffer& framebuffer )
	{
		if( isRendering_ )
		{
			throw std::runtime_error( "Nested render passes are not supported." );
		}

		std::array<D3D12_RENDER_PASS_RENDER_TARGET_DESC, ourMaxColorAttachments> renderTargetDescs{};
		uint32_t numRenderTargets = 0;
		uint32_t framebufferSampleCount = 0;

		TextureResource* viewportTexture = nullptr;

		for( uint32_t index = 0; index < framebuffer.color.size(); ++index )
		{
			if( !framebuffer.color[ index ].texture.Valid() )
			{
				continue;
			}

			TextureResource& colorTexture = manager_->GetTextureResource( framebuffer.color[ index ].texture );
			if( colorTexture.rtvHandle_.ptr == 0 )
			{
				throw std::runtime_error( "Color attachment does not have an RTV." );
			}
			if( framebufferSampleCount == 0 )
			{
				framebufferSampleCount = colorTexture.desc_.SampleDesc.Count;
			}
			else if( framebufferSampleCount != colorTexture.desc_.SampleDesc.Count )
			{
				throw std::runtime_error( "All framebuffer attachments must use the same sample count." );
			}

			TransitionTexture( framebuffer.color[ index ].texture, colorTexture, D3D12_RESOURCE_STATE_RENDER_TARGET );

			renderTargetDescs[ numRenderTargets ].cpuDescriptor = colorTexture.rtvHandle_;
			renderTargetDescs[ numRenderTargets ].BeginningAccess = CreateBeginningAccess( renderPass.color[ index ].loadOp,
				colorTexture.formats_.rtv_ != DXGI_FORMAT_UNKNOWN ? colorTexture.formats_.rtv_ : colorTexture.format_,
				renderPass.color[ index ].clearColor );
			renderTargetDescs[ numRenderTargets ].EndingAccess = CreateEndingAccess( renderPass.color[ index ].storeOp );
			numRenderTargets++;

			if( viewportTexture == nullptr )
			{
				viewportTexture = &colorTexture;
			}
		}

		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depthStencilDesc{};
		D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* depthStencilDescPtr = nullptr;

		if( framebuffer.depthStencil.texture.Valid() )
		{
			TextureResource& depthTexture = manager_->GetTextureResource( framebuffer.depthStencil.texture );
			if( depthTexture.dsvHandle_.ptr == 0 )
			{
				throw std::runtime_error( "Depth attachment does not have a DSV." );
			}
			if( framebufferSampleCount == 0 )
			{
				framebufferSampleCount = depthTexture.desc_.SampleDesc.Count;
			}
			else if( framebufferSampleCount != depthTexture.desc_.SampleDesc.Count )
			{
				throw std::runtime_error( "All framebuffer attachments must use the same sample count." );
			}

			TransitionTexture( framebuffer.depthStencil.texture, depthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE );

			depthStencilDesc.cpuDescriptor = depthTexture.dsvHandle_;
			depthStencilDesc.DepthBeginningAccess =
				depthTexture.isDepthFormat_ ? CreateDepthBeginningAccess( renderPass.depthStencil.depthLoadOp,
												  depthTexture.formats_.dsv_ != DXGI_FORMAT_UNKNOWN ? depthTexture.formats_.dsv_ : depthTexture.format_,
												  renderPass.depthStencil.clearDepth )
											: CreateNoAccessBeginningAccess();
			depthStencilDesc.DepthEndingAccess =
				depthTexture.isDepthFormat_ ? CreateEndingAccess( renderPass.depthStencil.depthStoreOp ) : CreateNoAccessEndingAccess();
			depthStencilDesc.StencilBeginningAccess =
				depthTexture.isStencilFormat_ ? CreateStencilBeginningAccess( renderPass.depthStencil.stencilLoadOp,
													depthTexture.formats_.dsv_ != DXGI_FORMAT_UNKNOWN ? depthTexture.formats_.dsv_ : depthTexture.format_,
													renderPass.depthStencil.clearStencil )
											  : CreateNoAccessBeginningAccess();
			depthStencilDesc.StencilEndingAccess =
				depthTexture.isStencilFormat_ ? CreateEndingAccess( renderPass.depthStencil.stencilStoreOp ) : CreateNoAccessEndingAccess();
			depthStencilDescPtr = &depthStencilDesc;

			if( viewportTexture == nullptr )
			{
				viewportTexture = &depthTexture;
			}
		}

		if( viewportTexture == nullptr )
		{
			throw std::runtime_error( "Framebuffer does not contain any attachments." );
		}

		wrapper_->commandList_->BeginRenderPass( numRenderTargets,
			numRenderTargets > 0 ? renderTargetDescs.data() : nullptr,
			depthStencilDescPtr,
			D3D12_RENDER_PASS_FLAG_NONE );

		CmdSetViewport( 0.0f, 0.0f, static_cast<float>( viewportTexture->width_ ), static_cast<float>( viewportTexture->height_ ) );
		CmdSetScissor( 0, 0, static_cast<int32_t>( viewportTexture->width_ ), static_cast<int32_t>( viewportTexture->height_ ) );
		isRendering_ = true;
	}

	void CommandBufferImpl::CmdEndRendering()
	{
		if( !isRendering_ )
		{
			return;
		}

		wrapper_->commandList_->EndRenderPass();
		isRendering_ = false;
	}

	void CommandBufferImpl::CmdSetViewport( float x, float y, float width, float height, float minDepth, float maxDepth )
	{
		assert( width >= 0.0f && height >= 0.0f );
		assert( minDepth >= 0.0f && minDepth <= maxDepth && maxDepth <= 1.0f );

		const D3D12_VIEWPORT viewport{ x, y, width, height, minDepth, maxDepth };
		wrapper_->commandList_->RSSetViewports( 1, &viewport );
	}

	void CommandBufferImpl::CmdSetScissor( int32_t left, int32_t top, int32_t right, int32_t bottom )
	{
		assert( right >= left && bottom >= top );

		const D3D12_RECT scissor{ left, top, right, bottom };
		wrapper_->commandList_->RSSetScissorRects( 1, &scissor );
	}

	void CommandBufferImpl::CmdTransitionTexture( TextureHandle texture, D3D12_RESOURCE_STATES newState )
	{
		TextureResource& resource = manager_->GetTextureResource( texture );
		TransitionTexture( texture, resource, newState );
	}

	void CommandBufferImpl::CmdResolveTexture( TextureHandle source, TextureHandle destination )
	{
		if( isRendering_ )
		{
			throw std::runtime_error( "CmdResolveTexture must be called outside a render pass." );
		}
		if( source == destination )
		{
			throw std::invalid_argument( "CmdResolveTexture requires different source and destination textures." );
		}

		TextureResource& sourceResource = manager_->GetTextureResource( source );
		TextureResource& destinationResource = manager_->GetTextureResource( destination );
		if( sourceResource.desc_.SampleDesc.Count <= 1 || destinationResource.desc_.SampleDesc.Count != 1 )
		{
			throw std::runtime_error( "CmdResolveTexture requires a multisampled source and a single-sampled destination." );
		}
		if( sourceResource.width_ != destinationResource.width_ || sourceResource.height_ != destinationResource.height_ )
		{
			throw std::runtime_error( "CmdResolveTexture requires matching source and destination dimensions." );
		}
		if( sourceResource.format_ != destinationResource.format_ )
		{
			throw std::runtime_error( "CmdResolveTexture requires matching source and destination formats." );
		}
		if( sourceResource.isDepthFormat_ || destinationResource.isDepthFormat_ )
		{
			throw std::runtime_error( "CmdResolveTexture supports color textures only." );
		}

		TransitionTexture( source, sourceResource, D3D12_RESOURCE_STATE_RESOLVE_SOURCE );
		TransitionTexture( destination, destinationResource, D3D12_RESOURCE_STATE_RESOLVE_DEST );
		wrapper_->commandList_->ResolveSubresource( destinationResource.resource_.Get(), 0, sourceResource.resource_.Get(), 0, sourceResource.format_ );
	}

	void CommandBufferImpl::CmdBindRenderPipeline( const RenderPipelineState& pipeline )
	{
		wrapper_->commandList_->SetPipelineState( pipeline.pipelineState_.Get() );
		wrapper_->commandList_->IASetPrimitiveTopology( pipeline.topology_ );
	}

	void CommandBufferImpl::CmdBindComputePipeline( const ComputePipelineState& pipeline )
	{
		wrapper_->commandList_->SetPipelineState( pipeline.pipelineState_.Get() );
	}

	void CommandBufferImpl::CmdBindVertexBuffer( BufferHandle buffer, uint32_t stride, uint32_t offset, uint32_t slot )
	{
		const BufferResource& resource = manager_->GetBufferResource( buffer );
		if( resource.type_ != BufferType::Vertex )
		{
			throw std::runtime_error( "This buffer was not created as a vertex buffer." );
		}
		D3D12_VERTEX_BUFFER_VIEW view = resource.GetVertexBufferView( stride );
		if( offset > view.SizeInBytes )
		{
			throw std::runtime_error( "Vertex buffer offset exceeds the buffer size." );
		}
		view.BufferLocation += offset;
		view.SizeInBytes -= offset;
		wrapper_->commandList_->IASetVertexBuffers( slot, 1, &view );
	}

	void CommandBufferImpl::CmdBindIndexBuffer( BufferHandle buffer, DXGI_FORMAT format, uint32_t offset )
	{
		const BufferResource& resource = manager_->GetBufferResource( buffer );
		if( resource.type_ != BufferType::Index )
		{
			throw std::runtime_error( "This buffer was not created as an index buffer." );
		}
		D3D12_INDEX_BUFFER_VIEW view = resource.GetIndexBufferView( format );
		if( offset > view.SizeInBytes )
		{
			throw std::runtime_error( "Index buffer offset exceeds the buffer size." );
		}
		view.BufferLocation += offset;
		view.SizeInBytes -= offset;
		wrapper_->commandList_->IASetIndexBuffer( &view );
	}

	void CommandBufferImpl::CmdPushConstants( const void* data, uint32_t sizeBytes, uint32_t offset32BitValues )
	{
		if( sizeBytes == 0 )
		{
			return;
		}
		if( data == nullptr )
		{
			throw std::invalid_argument( "CmdPushConstants requires valid data." );
		}
		if( sizeBytes % sizeof( uint32_t ) != 0 )
		{
			throw std::invalid_argument( "CmdPushConstants size must be aligned to 32-bit values." );
		}

		const uint32_t valueCount = sizeBytes / sizeof( uint32_t );
		if( offset32BitValues > ourMaxPushConstant32BitValues || valueCount > ourMaxPushConstant32BitValues - offset32BitValues )
		{
			throw std::length_error( "CmdPushConstants cannot exceed 63 32-bit values." );
		}

		wrapper_->commandList_->SetGraphicsRoot32BitConstants( 0, valueCount, data, offset32BitValues );
		wrapper_->commandList_->SetComputeRoot32BitConstants( 0, valueCount, data, offset32BitValues );
	}

	void CommandBufferImpl::CmdPushDebugGroupLabel( const char* label, uint32_t color )
	{
		if( label != nullptr && label[ 0 ] != '\0' )
		{
#if LDX12_INTERNAL_PIX_ENABLED
			GetPixRuntime().BeginEvent( wrapper_->commandList_.Get(), static_cast<uint64_t>( color ), label );
#else
			(void)color;
#endif
			debugGroupDepth_++;
		}
	}

	void CommandBufferImpl::CmdPopDebugGroupLabel()
	{
		if( debugGroupDepth_ > 0 )
		{
#if LDX12_INTERNAL_PIX_ENABLED
			GetPixRuntime().EndEvent( wrapper_->commandList_.Get() );
#endif
			debugGroupDepth_--;
		}
	}

	void CommandBufferImpl::CmdDraw( uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance )
	{
		wrapper_->commandList_->DrawInstanced( vertexCount, instanceCount, firstVertex, firstInstance );
	}

	void CommandBufferImpl::CmdDrawIndexed( uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance )
	{
		wrapper_->commandList_->DrawIndexedInstanced( indexCount, instanceCount, firstIndex, vertexOffset, firstInstance );
	}

	void CommandBufferImpl::CmdDrawIndexedIndirect( BufferHandle indirectBuffer, uint32_t drawCount, uint64_t byteOffset )
	{
		const BufferResource& resource = manager_->GetBufferResource( indirectBuffer );
		if( resource.type_ != BufferType::Indirect )
		{
			throw std::runtime_error( "This buffer was not created as an indirect buffer." );
		}
		constexpr uint64_t indirectCommandSize = sizeof( uint32_t ) + sizeof( D3D12_DRAW_INDEXED_ARGUMENTS );
		if( byteOffset % sizeof( uint32_t ) != 0 || byteOffset > resource.bufferSize_ ||
			drawCount > ( resource.bufferSize_ - byteOffset ) / indirectCommandSize )
		{
			throw std::runtime_error( "Indirect draw range exceeds the buffer." );
		}
		wrapper_->commandList_->ExecuteIndirect( manager_->commandSignature_.Get(), drawCount, resource.resource_.Get(), byteOffset, nullptr, 0 );
	}

	void CommandBufferImpl::CmdDispatch( uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ )
	{
		wrapper_->commandList_->Dispatch( groupCountX, groupCountY, groupCountZ );
	}

	ID3D12GraphicsCommandList* CommandBufferImpl::GetNativeGraphicsCommandList()
	{
		return wrapper_->commandList_.Get();
	}
}
