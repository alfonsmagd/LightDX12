#pragma once

#include "Ldx12/Ldx12.hpp"

namespace ldx12
{
	// Optional Direct3D 12 escape hatch. Returned pointers are borrowed and must
	// not be released by the caller.
	class D3D12Native final
	{
	public:
		[[nodiscard]] ID3D12Device* GetDevice() const noexcept;
		[[nodiscard]] ID3D12CommandQueue* GetCommandQueue() const noexcept;
		[[nodiscard]] ID3D12GraphicsCommandList* GetCommandList( CommandBuffer& commandBuffer ) const noexcept;
		[[nodiscard]] ID3D12Resource* GetResource( BufferHandle buffer ) const;
		[[nodiscard]] ID3D12Resource* GetResource( TextureHandle texture ) const;

		// Retains a reference to a single-mip, non-MSAA RGBA8/BGRA8 texture on
		// this device, initially in COMMON. Exposes an SRV only. The caller must
		// synchronize external writes and return the texture to COMMON before
		// handing it back. Release the returned handle with RenderDevice::Destroy.
		[[nodiscard]] TextureHandle ImportSampledTexture2D( ID3D12Resource* texture ) const;

	private:
		friend class RenderDevice;

		explicit D3D12Native( RenderDevice& device ) noexcept;

		RenderDevice* device_ = nullptr;
	};
}
