#include "Ldx12Utils/DepthTarget.hpp"

namespace ldx12::utils
{
	DepthTarget::DepthTarget( RenderDevice& device, DXGI_FORMAT format ):
		device_( &device ),
		format_( format )
	{
	}

	DepthTarget::~DepthTarget()
	{
		Reset();
	}

	void DepthTarget::Resize( uint32_t width, uint32_t height )
	{
		if( texture_.Valid() && width_ == width && height_ == height )
		{
			return;
		}

		Reset();
		if( width == 0 || height == 0 )
		{
			return;
		}

		TextureDesc desc{};
		desc.debugName = "Ldx12 Utils depth target";
		desc.width = width;
		desc.height = height;
		desc.format = format_;
		desc.usage = TextureUsage::DepthStencil;
		desc.useClearValue = true;
		desc.clearValue.Format = format_;
		desc.clearValue.DepthStencil.Depth = 1.0f;
		texture_ = device_->CreateTexture( desc );
		width_ = width;
		height_ = height;
	}

	void DepthTarget::Reset()
	{
		if( texture_.Valid() )
		{
			device_->Destroy( texture_ );
			texture_ = {};
		}
		width_ = 0;
		height_ = 0;
	}

	TextureHandle DepthTarget::GetTexture() const noexcept
	{
		return texture_;
	}

	uint32_t DepthTarget::GetWidth() const noexcept
	{
		return width_;
	}

	uint32_t DepthTarget::GetHeight() const noexcept
	{
		return height_;
	}
}
