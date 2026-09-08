#include "Ldx12/Ldx12Native.hpp"
#include <stdexcept>

namespace ldx12
{
	D3D12Native::D3D12Native( RenderDevice& device ) noexcept : device_( &device )
	{
	}

	D3D12Native RenderDevice::GetNative() noexcept
	{
		return D3D12Native( *this );
	}

	ID3D12Device* D3D12Native::GetDevice() const noexcept
	{
		return device_->manager_->device_.Get();
	}

	ID3D12CommandQueue* D3D12Native::GetCommandQueue() const noexcept
	{
		return device_->manager_->GetGraphicsQueueContext().commandQueue_.Get();
	}

	ID3D12GraphicsCommandList* D3D12Native::GetCommandList( CommandBuffer& commandBuffer ) const noexcept
	{
		return commandBuffer.GetNativeGraphicsCommandList();
	}

	ID3D12Resource* D3D12Native::GetResource( BufferHandle buffer ) const
	{
		return device_->manager_->GetBufferResource( buffer ).resource_.Get();
	}

	ID3D12Resource* D3D12Native::GetResource( TextureHandle texture ) const
	{
		return device_->manager_->GetTextureResource( texture ).resource_.Get();
	}

	TextureHandle D3D12Native::ImportSampledTexture2D( ID3D12Resource* texture ) const
	{
		if( texture == nullptr )
			throw std::invalid_argument( "ImportSampledTexture2D requires a texture." );
		const auto desc = texture->GetDesc();
		if( desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.DepthOrArraySize != 1 || desc.MipLevels != 1 || desc.SampleDesc.Count != 1 ||
			( desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE ) != 0 ||
			( desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM ) )
			throw std::invalid_argument( "ImportSampledTexture2D requires a single-mip RGBA8/BGRA8 sampled 2D texture." );
		ComPtr<ID3D12Device> owner;
		if( FAILED( texture->GetDevice( IID_PPV_ARGS( owner.GetAddressOf() ) ) ) || owner.Get() != GetDevice() )
			throw std::invalid_argument( "Imported texture must belong to this D3D12 device." );

		TextureResource resource;
		resource.resource_ = texture;
		resource.desc_ = desc;
		resource.width_ = static_cast<uint32_t>( desc.Width );
		resource.height_ = desc.Height;
		resource.format_ = desc.Format;
		resource.formats_.resource_ = desc.Format;
		resource.formats_.srv_ = desc.Format;
		resource.usageFlags_ = desc.Flags;
		auto& manager = *device_->manager_;
		uint32_t descriptor = UINT32_MAX;
		try
		{
			manager.CreateTextureShaderResourceView( resource );
			descriptor = resource.srvIndex_;
			return manager.slotMapTextures_.Create( std::move( resource ) );
		}
		catch( ... )
		{
			manager.FreeBindlessDescriptor( descriptor );
			throw;
		}
	}
}
