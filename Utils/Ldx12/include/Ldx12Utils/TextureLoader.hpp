#pragma once

#include "Ldx12/Ldx12.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace ldx12::utils
{
	struct ImageRgba8
	{
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<uint8_t> pixels;
	};

	ImageRgba8 LoadImageRgba8( const std::filesystem::path& path );
	TextureHandle CreateCheckerTexture(
		RenderDevice& device,
		uint32_t firstColor = 0xffffffffu,
		uint32_t secondColor = 0xff000000u,
		uint32_t textureSize = 64,
		uint32_t checkerSize = 8 );

	// Expected face names: px.png, nx.png, py.png, ny.png, pz.png and nz.png.
	TextureHandle LoadCubeMap( RenderDevice& device, const std::filesystem::path& directory );
}
