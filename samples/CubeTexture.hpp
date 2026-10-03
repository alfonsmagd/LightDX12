#pragma once

#include "Ldx12Utils/TextureLoader.hpp"

namespace ldx12::samples
{
	inline TextureHandle LoadCubeTexture( RenderDevice& device )
	{
		const utils::ImageRgba8 image = utils::LoadImageRgba8( std::filesystem::path( LDX12_MEDIA_DIRECTORY ) / "ldx12-cube.png" );

		TextureDesc desc{};
		desc.debugName = "Ldx12 cube logo";
		desc.width = image.width;
		desc.height = image.height;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.usage = TextureUsage::Sampled;
		desc.data = image.pixels.data();
		desc.rowPitch = image.width * 4;
		desc.slicePitch = desc.rowPitch * image.height;

		return device.CreateTexture( desc );
	}
}
