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

	private:
		friend class RenderDevice;

		explicit D3D12Native( RenderDevice& device ) noexcept;

		RenderDevice* device_ = nullptr;
	};
}
