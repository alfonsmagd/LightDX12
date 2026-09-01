#include "Ldx12/Ldx12Native.hpp"

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
}
