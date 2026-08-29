#include "Ldx12Utils/TextureLoader.hpp"

#include <wincodec.h>
#include <wrl/client.h>

#include <array>
#include <cstring>
#include <stdexcept>

namespace ldx12::utils
{
	namespace
	{
		class ComInitialization final
		{
		public:
			ComInitialization()
			{
				const HRESULT result = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
				if( result == RPC_E_CHANGED_MODE )
				{
					return;
				}
				if( FAILED( result ) )
				{
					throw std::runtime_error( "Failed to initialize COM for image loading." );
				}
				uninitialize_ = true;
			}

			~ComInitialization()
			{
				if( uninitialize_ )
				{
					CoUninitialize();
				}
			}

		private:
			bool uninitialize_ = false;
		};

		void ThrowIfFailed( HRESULT result, const char* message )
		{
			if( FAILED( result ) )
			{
				throw std::runtime_error( message );
			}
		}

		Microsoft::WRL::ComPtr<IWICImagingFactory> CreateWicFactory()
		{
			Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
			ThrowIfFailed(
				CoCreateInstance(
					CLSID_WICImagingFactory,
					nullptr,
					CLSCTX_INPROC_SERVER,
					IID_PPV_ARGS( factory.GetAddressOf() ) ),
				"Failed to create the WIC imaging factory." );
			return factory;
		}

		ImageRgba8 LoadImageRgba8( IWICImagingFactory& factory, const std::filesystem::path& path )
		{
			Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
			ThrowIfFailed(
				factory.CreateDecoderFromFilename(
					path.c_str(),
					nullptr,
					GENERIC_READ,
					WICDecodeMetadataCacheOnLoad,
					decoder.GetAddressOf() ),
				"Failed to open an image." );

			Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
			ThrowIfFailed( decoder->GetFrame( 0, frame.GetAddressOf() ), "Failed to decode an image." );

			ImageRgba8 image{};
			ThrowIfFailed( frame->GetSize( &image.width, &image.height ), "Failed to read image dimensions." );

			Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
			ThrowIfFailed(
				factory.CreateFormatConverter( converter.GetAddressOf() ),
				"Failed to create a WIC format converter." );
			ThrowIfFailed(
				converter->Initialize(
					frame.Get(),
					GUID_WICPixelFormat32bppRGBA,
					WICBitmapDitherTypeNone,
					nullptr,
					0.0,
					WICBitmapPaletteTypeCustom ),
				"Failed to convert an image to RGBA8." );

			const uint32_t rowPitch = image.width * 4u;
			const uint32_t imageSize = rowPitch * image.height;
			image.pixels.resize( imageSize );
			ThrowIfFailed(
				converter->CopyPixels( nullptr, rowPitch, imageSize, image.pixels.data() ),
				"Failed to copy image pixels." );
			return image;
		}
	}

	ImageRgba8 LoadImageRgba8( const std::filesystem::path& path )
	{
		ComInitialization com;
		Microsoft::WRL::ComPtr<IWICImagingFactory> factory = CreateWicFactory();
		return LoadImageRgba8( *factory.Get(), path );
	}

	TextureHandle CreateCheckerTexture(
		RenderDevice& device,
		uint32_t firstColor,
		uint32_t secondColor,
		uint32_t textureSize,
		uint32_t checkerSize )
	{
		std::vector<uint32_t> pixels( static_cast<size_t>( textureSize ) * textureSize );
		for( uint32_t y = 0; y < textureSize; ++y )
		{
			for( uint32_t x = 0; x < textureSize; ++x )
			{
				const bool first = ( (x / checkerSize) + (y / checkerSize) ) % 2u == 0u;
				pixels[ static_cast<size_t>( y ) * textureSize + x ] = first ? firstColor : secondColor;
			}
		}

		TextureDesc desc{};
		desc.debugName = "Ldx12 Utils checker texture";
		desc.width = textureSize;
		desc.height = textureSize;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.usage = TextureUsage::Sampled;
		desc.data = pixels.data();
		desc.rowPitch = textureSize * sizeof( uint32_t );
		desc.slicePitch = desc.rowPitch * textureSize;
		return device.CreateTexture( desc );
	}

	TextureHandle LoadCubeMap( RenderDevice& device, const std::filesystem::path& directory )
	{
		ComInitialization com;
		Microsoft::WRL::ComPtr<IWICImagingFactory> factory = CreateWicFactory();
		const std::array<std::filesystem::path, ourCubeMapFaceCount> faceNames = {
			"px.png", "nx.png", "py.png", "ny.png", "pz.png", "nz.png"
		};

		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t rowPitch = 0;
		uint32_t slicePitch = 0;
		std::vector<uint8_t> pixels;
		for( uint32_t face = 0; face < ourCubeMapFaceCount; ++face )
		{
			const ImageRgba8 image = LoadImageRgba8( *factory.Get(), directory / faceNames[ face ] );
			if( face == 0 )
			{
				width = image.width;
				height = image.height;
				rowPitch = width * 4u;
				slicePitch = rowPitch * height;
				pixels.resize( static_cast<size_t>( slicePitch ) * ourCubeMapFaceCount );
			}
			else if( image.width != width || image.height != height )
			{
				throw std::runtime_error( "All cubemap faces must have identical dimensions." );
			}

			std::memcpy(
				pixels.data() + static_cast<size_t>( face ) * slicePitch,
				image.pixels.data(),
				slicePitch );
		}

		TextureDesc desc{};
		desc.debugName = "Ldx12 Utils cubemap";
		desc.width = width;
		desc.height = height;
		desc.depthOrArraySize = ourCubeMapFaceCount;
		desc.dimension = TextureDimension::TextureCube;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.usage = TextureUsage::Sampled;
		desc.data = pixels.data();
		desc.rowPitch = rowPitch;
		desc.slicePitch = slicePitch;
		return device.CreateTexture( desc );
	}
}
