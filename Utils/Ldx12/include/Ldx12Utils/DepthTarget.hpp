#pragma once

#include "Ldx12/Ldx12.hpp"

#include <cstdint>

namespace ldx12::utils
{
	class DepthTarget final
	{
	public:
		explicit DepthTarget( RenderDevice& device, DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT );
		~DepthTarget();

		DepthTarget( const DepthTarget& ) = delete;
		DepthTarget& operator=( const DepthTarget& ) = delete;
		DepthTarget( DepthTarget&& ) = delete;
		DepthTarget& operator=( DepthTarget&& ) = delete;

		void Resize( uint32_t width, uint32_t height );
		void Reset();

		TextureHandle GetTexture() const noexcept;
		uint32_t GetWidth() const noexcept;
		uint32_t GetHeight() const noexcept;

	private:
		RenderDevice* device_ = nullptr;
		TextureHandle texture_ = {};
		DXGI_FORMAT format_ = DXGI_FORMAT_D32_FLOAT;
		uint32_t width_ = 0;
		uint32_t height_ = 0;
	};
}
