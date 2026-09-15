#pragma once

#include "Ldx12/Ldx12.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace gltf_sample
{
	struct SceneEnvironment
	{
		std::vector<ldx12::TextureHandle> specular;
		ldx12::TextureHandle irradiance;
		ldx12::TextureHandle brdf;
	};


	// The sample ships uncompressed KTX1 RGBA16F/RGBA32F files.
	// Each prefiltered level is a separate cube, so normal Ldx12 uploads suffice.
	inline std::vector<ldx12::TextureHandle> LoadEnvironmentLevels( ldx12::RenderDevice& device, const std::filesystem::path& path )
	{
		std::ifstream file( path, std::ios::binary );
		std::array<uint8_t, 12> identifier{};
		std::array<uint32_t, 13> header{};

		file.read( reinterpret_cast<char*>( identifier.data() ), identifier.size() );
		file.read( reinterpret_cast<char*>( header.data() ), sizeof( header ) );

		const std::array<uint8_t, 12> ktxIdentifier = { 0xab, 0x4b, 0x54, 0x58, 0x20, 0x31, 0x31, 0xbb, 0x0d, 0x0a, 0x1a, 0x0a };
		if( !file || identifier != ktxIdentifier || header[ 0 ] != 0x04030201 ||
			header[ 3 ] != 0x1908 || ( header[ 4 ] != 0x881a && header[ 4 ] != 0x8814 ) )
		{
			throw std::runtime_error( "Cannot read the sample's HDR KTX environment: " + path.string() );
		}

		const uint32_t width = header[ 6 ];
		const uint32_t height = header[ 7 ];
		const uint32_t faces = header[ 10 ];
		const uint32_t levels = header[ 11 ];
		const uint32_t bytesPerPixel = header[ 2 ] * 4;
		file.seekg( header[ 12 ], std::ios::cur );

		std::vector<ldx12::TextureHandle> textures;

		for( uint32_t level = 0; level < levels; ++level )
		{
			uint32_t faceSize = 0;
			file.read( reinterpret_cast<char*>( &faceSize ), sizeof( faceSize ) );
			std::vector<uint8_t> pixels( static_cast<size_t>( faceSize ) * faces );

			for( uint32_t face = 0; face < faces; ++face )
			{
				file.read( reinterpret_cast<char*>( pixels.data() + static_cast<size_t>( face ) * faceSize ), faceSize );
				file.seekg( ( 4 - faceSize % 4 ) % 4, std::ios::cur );
			}

			ldx12::TextureDesc desc;
			desc.width = std::max( 1u, width >> level );
			desc.height = std::max( 1u, height >> level );
			desc.dimension = faces == 6 ? ldx12::TextureDimension::TextureCube : ldx12::TextureDimension::Texture2D;
			desc.depthOrArraySize = static_cast<uint16_t>( faces );
			desc.format = header[ 4 ] == 0x881a ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32G32B32A32_FLOAT;
			desc.data = pixels.data();
			desc.rowPitch = desc.width * bytesPerPixel;
			desc.slicePitch = faceSize;

			if( !file || faceSize != desc.rowPitch * desc.height )
			{
				throw std::runtime_error( "Incomplete HDR environment level." );
			}

			textures.push_back( device.CreateTexture( desc ) );
		}

		return textures;
	}


	inline SceneEnvironment LoadEnvironment( ldx12::RenderDevice& device, const std::filesystem::path& directory )
	{
		SceneEnvironment environment;

		environment.specular = LoadEnvironmentLevels( device, directory / "piazza_bologni_1k_prefilter.ktx" );
		environment.irradiance = LoadEnvironmentLevels( device, directory / "piazza_bologni_1k_irradiance.ktx" ).at( 0 );
		environment.brdf = LoadEnvironmentLevels( device, directory / "brdfLUT.ktx" ).at( 0 );

		return environment;
	}


	inline void DestroyEnvironment( ldx12::RenderDevice& device, const SceneEnvironment& environment )
	{
		for( ldx12::TextureHandle texture : environment.specular )
		{
			device.Destroy( texture );
		}

		device.Destroy( environment.irradiance );
		device.Destroy( environment.brdf );
	}
}
