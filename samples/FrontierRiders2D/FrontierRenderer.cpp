#include "FrontierRenderer.hpp"

#include "LightD3D12/LightHLSLLoader.hpp"

#include <wincodec.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace lightd3d12;
using Microsoft::WRL::ComPtr;

namespace frontier
{
	namespace
	{
		struct ImageData
		{
			uint32_t width = 0;
			uint32_t height = 0;
			std::vector<uint32_t> pixels;
		};

		enum class MatteMode
		{
			None,
			Light,
			ChromaGreen,
		};

		struct SpritePushConstants
		{
			float x = 0.0f;
			float y = 0.0f;
			float width = 0.0f;
			float height = 0.0f;
			float u0 = 0.0f;
			float v0 = 0.0f;
			float u1 = 1.0f;
			float v1 = 1.0f;
			float red = 1.0f;
			float green = 1.0f;
			float blue = 1.0f;
			float alpha = 1.0f;
			float viewportWidth = kLogicalWidth;
			float viewportHeight = kLogicalHeight;
			uint32_t textureIndex = 0;
			float rotation = 0.0f;
		};

		static_assert( sizeof( SpritePushConstants ) == 64 );
		static_assert( sizeof( SpritePushConstants ) / sizeof( uint32_t ) <= 63 );

		struct Glyph
		{
			std::array<uint8_t, 7> rows{};
		};

		struct AssetIndices
		{
			uint32_t white = 0;
			uint32_t font = 0;
			uint32_t background = 0;
			uint32_t midground = 0;
			uint32_t foreground = 0;
			uint32_t cowboy = 0;
		};

		constexpr std::array<Glyph, 40> kGlyphs = {
			Glyph{ { 14, 17, 19, 21, 25, 17, 14 } }, Glyph{ { 4, 12, 4, 4, 4, 4, 14 } },
			Glyph{ { 14, 17, 1, 2, 4, 8, 31 } }, Glyph{ { 30, 1, 1, 14, 1, 1, 30 } },
			Glyph{ { 2, 6, 10, 18, 31, 2, 2 } }, Glyph{ { 31, 16, 30, 1, 1, 17, 14 } },
			Glyph{ { 6, 8, 16, 30, 17, 17, 14 } }, Glyph{ { 31, 1, 2, 4, 8, 8, 8 } },
			Glyph{ { 14, 17, 17, 14, 17, 17, 14 } }, Glyph{ { 14, 17, 17, 15, 1, 2, 12 } },
			Glyph{ { 14, 17, 17, 31, 17, 17, 17 } }, Glyph{ { 30, 17, 17, 30, 17, 17, 30 } },
			Glyph{ { 14, 17, 16, 16, 16, 17, 14 } }, Glyph{ { 30, 17, 17, 17, 17, 17, 30 } },
			Glyph{ { 31, 16, 16, 30, 16, 16, 31 } }, Glyph{ { 31, 16, 16, 30, 16, 16, 16 } },
			Glyph{ { 14, 17, 16, 23, 17, 17, 15 } }, Glyph{ { 17, 17, 17, 31, 17, 17, 17 } },
			Glyph{ { 14, 4, 4, 4, 4, 4, 14 } }, Glyph{ { 7, 2, 2, 2, 18, 18, 12 } },
			Glyph{ { 17, 18, 20, 24, 20, 18, 17 } }, Glyph{ { 16, 16, 16, 16, 16, 16, 31 } },
			Glyph{ { 17, 27, 21, 21, 17, 17, 17 } }, Glyph{ { 17, 25, 21, 19, 17, 17, 17 } },
			Glyph{ { 14, 17, 17, 17, 17, 17, 14 } }, Glyph{ { 30, 17, 17, 30, 16, 16, 16 } },
			Glyph{ { 14, 17, 17, 17, 21, 18, 13 } }, Glyph{ { 30, 17, 17, 30, 20, 18, 17 } },
			Glyph{ { 15, 16, 16, 14, 1, 1, 30 } }, Glyph{ { 31, 4, 4, 4, 4, 4, 4 } },
			Glyph{ { 17, 17, 17, 17, 17, 17, 14 } }, Glyph{ { 17, 17, 17, 17, 17, 10, 4 } },
			Glyph{ { 17, 17, 17, 21, 21, 21, 10 } }, Glyph{ { 17, 17, 10, 4, 10, 17, 17 } },
			Glyph{ { 17, 17, 10, 4, 4, 4, 4 } }, Glyph{ { 31, 1, 2, 4, 8, 16, 31 } },
			Glyph{ { 0, 0, 0, 31, 0, 0, 0 } }, Glyph{ { 0, 4, 4, 0, 4, 4, 0 } },
			Glyph{ { 1, 2, 2, 4, 8, 8, 16 } }, Glyph{ { 4, 4, 4, 4, 4, 0, 4 } }
		};

		void ThrowIfFailed( HRESULT result, const char* message )
		{
			if( FAILED( result ) ) throw std::runtime_error( message );
		}

		uint32_t PackRgba( uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255 )
		{
			return static_cast<uint32_t>( red ) | ( static_cast<uint32_t>( green ) << 8u ) |
				( static_cast<uint32_t>( blue ) << 16u ) | ( static_cast<uint32_t>( alpha ) << 24u );
		}

		ImageData CreateFontImage()
		{
			constexpr uint32_t columns = 8;
			constexpr uint32_t rows = 5;
			constexpr uint32_t cellWidth = 6;
			constexpr uint32_t cellHeight = 8;
			ImageData image{ columns * cellWidth, rows * cellHeight, {} };
			image.pixels.assign( static_cast<size_t>( image.width ) * image.height, 0u );
			for( uint32_t glyphIndex = 0; glyphIndex < kGlyphs.size(); ++glyphIndex )
			{
				const uint32_t baseX = glyphIndex % columns * cellWidth;
				const uint32_t baseY = glyphIndex / columns * cellHeight;
				for( uint32_t y = 0; y < 7; ++y )
					for( uint32_t x = 0; x < 5; ++x )
						if( ( kGlyphs[glyphIndex].rows[y] & ( 1u << ( 4u - x ) ) ) != 0 )
							image.pixels[static_cast<size_t>( baseY + y ) * image.width + baseX + x] = PackRgba( 255, 255, 255 );
			}
			return image;
		}

		ImageData LoadPngRgba( const std::filesystem::path& path, MatteMode matteMode = MatteMode::None )
		{
			ComPtr<IWICImagingFactory> factory;
			ThrowIfFailed( CoCreateInstance( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS( factory.GetAddressOf() ) ), "No se pudo crear WICImagingFactory." );
			ComPtr<IWICBitmapDecoder> decoder;
			ThrowIfFailed( factory->CreateDecoderFromFilename( path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.GetAddressOf() ), "No se pudo abrir un PNG de Frontier Riders 2D." );
			ComPtr<IWICBitmapFrameDecode> frame;
			ThrowIfFailed( decoder->GetFrame( 0, frame.GetAddressOf() ), "No se pudo leer el frame PNG." );
			ComPtr<IWICFormatConverter> converter;
			ThrowIfFailed( factory->CreateFormatConverter( converter.GetAddressOf() ), "No se pudo crear el conversor WIC." );
			ThrowIfFailed( converter->Initialize( frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom ), "No se pudo convertir el PNG a RGBA8." );
			ImageData image;
			ThrowIfFailed( converter->GetSize( &image.width, &image.height ), "No se pudo leer el tamano del PNG." );
			image.pixels.resize( static_cast<size_t>( image.width ) * image.height );
			const uint32_t rowPitch = image.width * sizeof( uint32_t );
			ThrowIfFailed( converter->CopyPixels( nullptr, rowPitch, rowPitch * image.height, reinterpret_cast<BYTE*>( image.pixels.data() ) ), "No se pudieron copiar los pixels del PNG." );
			const bool hasRealAlpha = std::any_of( image.pixels.begin(), image.pixels.end(), []( uint32_t pixel ) { return ( pixel >> 24u ) < 250u; } );
			if( matteMode == MatteMode::Light && !hasRealAlpha )
			{
				for( uint32_t& pixel : image.pixels )
				{
					const int red = static_cast<int>( pixel & 0xffu );
					const int green = static_cast<int>( ( pixel >> 8u ) & 0xffu );
					const int blue = static_cast<int>( ( pixel >> 16u ) & 0xffu );
					const int minimum = std::min( { red, green, blue } );
					const int maximum = std::max( { red, green, blue } );
					if( minimum >= 215 && maximum - minimum < 30 )
					{
						pixel = 0u;
					}
					else if( minimum > 165 && maximum > 205 )
					{
						const float alpha = std::clamp( static_cast<float>( 215 - minimum ) / 50.0f, 0.04f, 1.0f );
						auto removeMatte = [alpha]( int channel )
						{
							const float recovered = ( static_cast<float>( channel ) - 248.0f * ( 1.0f - alpha ) ) / alpha;
							return static_cast<uint8_t>( std::clamp( static_cast<int>( std::lround( recovered ) ), 0, 255 ) );
						};
						pixel = PackRgba( removeMatte( red ), removeMatte( green ), removeMatte( blue ),
							static_cast<uint8_t>( std::lround( alpha * 255.0f ) ) );
					}
				}
			}
			else if( matteMode == MatteMode::ChromaGreen && !hasRealAlpha )
			{
				for( uint32_t& pixel : image.pixels )
				{
					const int red = static_cast<int>( pixel & 0xffu );
					const int green = static_cast<int>( ( pixel >> 8u ) & 0xffu );
					const int blue = static_cast<int>( ( pixel >> 16u ) & 0xffu );
					const int dominance = green - std::max( red, blue );
					if( green > 175 && dominance >= 82 )
					{
						pixel = 0u;
					}
					else if( green > 95 && dominance > 18 )
					{
						const float alpha = std::clamp( static_cast<float>( 82 - dominance ) / 64.0f, 0.05f, 1.0f );
						const auto recoverPlain = [alpha]( int channel )
						{
							return static_cast<uint8_t>( std::clamp( static_cast<int>( std::lround( static_cast<float>( channel ) / alpha ) ), 0, 255 ) );
						};
						const float recoveredGreen = ( static_cast<float>( green ) - 255.0f * ( 1.0f - alpha ) ) / alpha;
						pixel = PackRgba( recoverPlain( red ),
							static_cast<uint8_t>( std::clamp( static_cast<int>( std::lround( recoveredGreen ) ), 0, 255 ) ),
							recoverPlain( blue ), static_cast<uint8_t>( std::lround( alpha * 255.0f ) ) );
					}
				}
			}
			return image;
		}

		struct PixelBand
		{
			uint32_t begin = 0;
			uint32_t end = 0;
		};

		std::vector<PixelBand> BuildBands( const std::vector<uint32_t>& occupancy, uint32_t minimumPixels )
		{
			std::vector<PixelBand> bands;
			bool insideBand = false;
			for( uint32_t index = 0; index < occupancy.size(); ++index )
			{
				const bool occupied = occupancy[index] >= minimumPixels;
				if( occupied && !insideBand )
				{
					bands.push_back( { index, index } );
					insideBand = true;
				}
				else if( occupied ) bands.back().end = index;
				else insideBand = false;
			}
			return bands;
		}

		void MergeClosestBands( std::vector<PixelBand>& bands, size_t targetCount )
		{
			while( bands.size() > targetCount )
			{
				size_t bestIndex = 0;
				uint32_t smallestGap = UINT32_MAX;
				for( size_t index = 0; index + 1 < bands.size(); ++index )
				{
					const uint32_t gap = bands[index + 1].begin - bands[index].end - 1u;
					if( gap < smallestGap ) { smallestGap = gap; bestIndex = index; }
				}
				bands[bestIndex].end = bands[bestIndex + 1].end;
				bands.erase( bands.begin() + static_cast<std::ptrdiff_t>( bestIndex + 1 ) );
			}
		}

		ImageData NormalizeCowboySheet( const ImageData& source )
		{
			std::vector<uint32_t> rowOccupancy( source.height, 0u );
			for( uint32_t y = 0; y < source.height; ++y )
				for( uint32_t x = 0; x < source.width; ++x )
					if( ( source.pixels[static_cast<size_t>( y ) * source.width + x] >> 24u ) > 16u ) ++rowOccupancy[y];
			std::vector<PixelBand> rowBands = BuildBands( rowOccupancy, 16u );
			MergeClosestBands( rowBands, 8u );
			if( rowBands.size() != 8u ) throw std::runtime_error( "La hoja del cowboy no contiene ocho bandas de animacion detectables." );

			constexpr uint32_t destinationCellWidth = 192;
			constexpr uint32_t destinationCellHeight = 192;
			constexpr uint32_t destinationPadding = 4;
			ImageData result{ destinationCellWidth * 4u, destinationCellHeight * 8u, {} };
			result.pixels.assign( static_cast<size_t>( result.width ) * result.height, 0u );

			for( uint32_t row = 0; row < 8u; ++row )
			{
				const PixelBand rowBand = rowBands[row];
				std::vector<uint32_t> columnOccupancy( source.width, 0u );
				for( uint32_t y = rowBand.begin; y <= rowBand.end; ++y )
					for( uint32_t x = 0; x < source.width; ++x )
						if( ( source.pixels[static_cast<size_t>( y ) * source.width + x] >> 24u ) > 16u ) ++columnOccupancy[x];
				std::vector<PixelBand> columnBands = BuildBands( columnOccupancy, 2u );
				MergeClosestBands( columnBands, 4u );
				if( columnBands.size() != 4u ) throw std::runtime_error( "Una direccion del cowboy no contiene cuatro frames detectables." );

				for( uint32_t column = 0; column < 4u; ++column )
				{
					const PixelBand columnBand = columnBands[column];
					uint32_t minimumX = columnBand.end;
					uint32_t minimumY = rowBand.end;
					uint32_t maximumX = columnBand.begin;
					uint32_t maximumY = rowBand.begin;
					for( uint32_t y = rowBand.begin; y <= rowBand.end; ++y )
					{
						for( uint32_t x = columnBand.begin; x <= columnBand.end; ++x )
						{
							if( ( source.pixels[static_cast<size_t>( y ) * source.width + x] >> 24u ) <= 16u ) continue;
							minimumX = std::min( minimumX, x );
							minimumY = std::min( minimumY, y );
							maximumX = std::max( maximumX, x );
							maximumY = std::max( maximumY, y );
						}
					}
					const uint32_t sourceWidth = maximumX - minimumX + 1u;
					const uint32_t sourceHeight = maximumY - minimumY + 1u;
					const float scale = std::min( 1.0f, std::min(
						static_cast<float>( destinationCellWidth - destinationPadding * 2u ) / static_cast<float>( sourceWidth ),
						static_cast<float>( destinationCellHeight - destinationPadding * 2u ) / static_cast<float>( sourceHeight ) ) );
					const uint32_t copyWidth = std::max( 1u, static_cast<uint32_t>( std::lround( static_cast<float>( sourceWidth ) * scale ) ) );
					const uint32_t copyHeight = std::max( 1u, static_cast<uint32_t>( std::lround( static_cast<float>( sourceHeight ) * scale ) ) );
					const uint32_t destinationX = column * destinationCellWidth + ( destinationCellWidth - copyWidth ) / 2u;
					const uint32_t destinationY = row * destinationCellHeight + destinationCellHeight - destinationPadding - copyHeight;
					for( uint32_t y = 0; y < copyHeight; ++y )
					{
						const uint32_t sourceY = minimumY + std::min( sourceHeight - 1u, static_cast<uint32_t>( static_cast<float>( y ) / scale ) );
						for( uint32_t x = 0; x < copyWidth; ++x )
						{
							const uint32_t sourceX = minimumX + std::min( sourceWidth - 1u, static_cast<uint32_t>( static_cast<float>( x ) / scale ) );
							result.pixels[static_cast<size_t>( destinationY + y ) * result.width + destinationX + x] =
								source.pixels[static_cast<size_t>( sourceY ) * source.width + sourceX];
						}
					}
				}
			}
			return result;
		}

		TextureHandle UploadTexture( RenderDevice& device, const ImageData& image, const char* debugName )
		{
			TextureDesc desc{};
			desc.debugName = debugName;
			desc.width = image.width;
			desc.height = image.height;
			desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.usage = TextureUsage::Sampled;
			desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			desc.data = image.pixels.data();
			desc.rowPitch = image.width * sizeof( uint32_t );
			desc.slicePitch = desc.rowPitch * image.height;
			return device.CreateTexture( desc );
		}

		std::filesystem::path ExecutableDirectory()
		{
			std::array<wchar_t, 32768> path{};
			const DWORD length = GetModuleFileNameW( nullptr, path.data(), static_cast<DWORD>( path.size() ) );
			if( length == 0 || length >= path.size() ) throw std::runtime_error( "No se pudo localizar el ejecutable." );
			return std::filesystem::path( std::wstring_view( path.data(), length ) ).parent_path();
		}

		std::filesystem::path ResolveRuntimeFile( const std::filesystem::path& relativePath )
		{
			const std::filesystem::path runtimePath = ExecutableDirectory() / relativePath;
			if( std::filesystem::exists( runtimePath ) ) return runtimePath;
			const std::filesystem::path sourcePath = std::filesystem::path( __FILE__ ).parent_path() / relativePath;
			if( std::filesystem::exists( sourcePath ) ) return sourcePath;
			throw std::runtime_error( "Falta un asset requerido por FrontierRiders2D." );
		}

		RenderPipelineState CreateSpritePipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
		{
			RenderPipelineDesc desc{};
			desc.vertexShader = LightHLSLLoader::LoadStage( "shaders/Sprite2D.hlsl", "vs_6_6", "VSMain" );
			desc.fragmentShader = LightHLSLLoader::LoadStage( "shaders/Sprite2D.hlsl", "ps_6_6", "PSMain" );
			desc.color[0].format = colorFormat;
			desc.colorFormat = colorFormat;
			desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			desc.depthStencilState.DepthEnable = FALSE;
			desc.depthStencilState.StencilEnable = FALSE;
			D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.blendState.RenderTarget[0];
			blend.BlendEnable = TRUE;
			blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blend.BlendOp = D3D12_BLEND_OP_ADD;
			blend.SrcBlendAlpha = D3D12_BLEND_ONE;
			blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
			return device.CreateRenderPipeline( desc );
		}

		int GlyphIndex( char character )
		{
			if( character >= '0' && character <= '9' ) return character - '0';
			if( character >= 'A' && character <= 'Z' ) return 10 + character - 'A';
			if( character == '-' ) return 36;
			if( character == ':' ) return 37;
			if( character == '/' ) return 38;
			if( character == '!' ) return 39;
			return -1;
		}

		class FrameRenderCollector final
		{
		public:
			FrameRenderCollector( RenderQueue& queue, const AssetIndices& assets ) noexcept
				: queue_( queue ), assets_( assets )
			{
			}

			void Collect( const GameState& game )
			{
				DrawScene( game );
				DrawEntities( game );
				DrawHud( game );
			}

		private:
			void DrawSprite( uint32_t textureIndex, const RectF& rect, const UvRect& uv = {},
				const Color& tint = {}, float rotation = 0.0f )
			{
				queue_.RecordSprite( textureIndex, rect, uv, tint, rotation );
			}

			void DrawMirroredParallax( uint32_t textureIndex, float cameraX, float movementFactor )
			{
				const float layerPosition = std::max( 0.0f, cameraX * movementFactor );
				const int firstTile = static_cast<int>( std::floor( layerPosition / kLogicalWidth ) );
				const float offset = layerPosition - static_cast<float>( firstTile ) * kLogicalWidth;
				const auto tileUv = []( int tile )
				{
					return ( tile & 1 ) == 0 ? UvRect{} : UvRect{ 1.0f, 0.0f, 0.0f, 1.0f };
				};
				DrawSprite( textureIndex, { -offset, 0.0f, kLogicalWidth, kLogicalHeight }, tileUv( firstTile ) );
				DrawSprite( textureIndex, { kLogicalWidth - offset, 0.0f, kLogicalWidth, kLogicalHeight }, tileUv( firstTile + 1 ) );
			}

			void DrawText( float x, float y, float scale, std::string text, const Color& color )
			{
				queue_.RecordText( assets_.font, x, y, scale, std::move( text ), color );
			}

			static UvRect CowboyFrame( uint32_t row, uint32_t column )
			{
				return { static_cast<float>( column ) / 4.0f, static_cast<float>( row ) / 8.0f,
					static_cast<float>( column + 1u ) / 4.0f, static_cast<float>( row + 1u ) / 8.0f };
			}

			static uint32_t DirectionRow( Vec2 direction )
			{
				constexpr std::array<uint32_t, 8> rowForOctant = { 2u, 3u, 4u, 5u, 6u, 7u, 0u, 1u };
				constexpr float radiansPerOctant = 0.78539816339f;
				int octant = static_cast<int>( std::lround( std::atan2( direction.y, direction.x ) / radiansPerOctant ) );
				if( octant < 0 ) octant += 8;
				return rowForOctant[static_cast<size_t>( octant ) % rowForOctant.size()];
			}

			static std::string PaddedNumber( int value, size_t width )
			{
				std::string text = std::to_string( std::max( 0, value ) );
				if( text.size() < width ) text.insert( text.begin(), width - text.size(), '0' );
				return text;
			}

			static float ActorDepthScale( float feetY )
			{
				const float depth = std::clamp( ( feetY - 535.0f ) / 130.0f, 0.0f, 1.0f );
				return 0.80f + depth * 0.25f;
			}

			void DrawActorShadow( Vec2 position, float scale )
			{
				DrawSprite( assets_.white,
					{ position.x - 39.0f * scale, position.y - 7.0f * scale, 78.0f * scale, 11.0f * scale }, {},
					{ 0.02f, 0.015f, 0.01f, 0.38f } );
			}

			void DrawCowboyActor( Vec2 feetPosition, Vec2 facing, uint32_t frame, const Color& tint, int health = -1 )
			{
				const float scale = ActorDepthScale( feetPosition.y );
				const float size = 176.0f * scale;
				DrawActorShadow( feetPosition, scale );
				DrawSprite( assets_.cowboy,
					{ feetPosition.x - size * 0.5f, feetPosition.y - size * ( 188.0f / 192.0f ), size, size },
					CowboyFrame( DirectionRow( facing ), frame ), tint );
				if( health < 0 ) return;
				const float barY = feetPosition.y - size * 0.88f;
				DrawSprite( assets_.white, { feetPosition.x - 30.0f * scale, barY, 60.0f * scale, 5.0f * scale }, {},
					{ 0.08f, 0.04f, 0.025f, 0.85f } );
				DrawSprite( assets_.white,
					{ feetPosition.x - 29.0f * scale, barY + scale, 29.0f * scale * static_cast<float>( health ), 3.0f * scale }, {},
					{ 0.85f, 0.16f, 0.08f, 1.0f } );
			}

			void DrawEntity( const DeadBody& body, float cameraX )
			{
				const Vec2 feetPosition{ body.position.x - cameraX, body.position.y };
				if( feetPosition.x < -160.0f || feetPosition.x > kLogicalWidth + 160.0f ) return;
				const float scale = ActorDepthScale( feetPosition.y );
				const float size = 176.0f * scale;
				const float progress = std::clamp( 1.0f - body.life / body.maximumLife, 0.0f, 1.0f );
				float fall = std::clamp( progress / 0.16f, 0.0f, 1.0f );
				fall = fall * fall * ( 3.0f - 2.0f * fall );
				const float centerY = feetPosition.y - ( 86.0f - fall * 42.0f ) * scale;
				const float fade = std::clamp( body.life / 0.8f, 0.0f, 1.0f );
				const Color tint{ body.tint.r * 0.68f, body.tint.g * 0.52f, body.tint.b * 0.52f, body.tint.a * fade };
				DrawActorShadow( feetPosition, scale );
				DrawSprite( assets_.cowboy,
					{ feetPosition.x - size * 0.5f, centerY - size * 0.5f, size, size },
					CowboyFrame( DirectionRow( body.facingDirection ), 0u ), tint, body.finalRotation * fall );
			}

			void DrawEntity( const BloodStain& stain, float cameraX )
			{
				const float screenX = stain.position.x - cameraX;
				if( screenX < -50.0f || screenX > kLogicalWidth + 50.0f ) return;
				const float scale = ActorDepthScale( stain.position.y );
				DrawSprite( assets_.white,
					{ screenX - stain.size.x * scale * 0.5f, stain.position.y - stain.size.y * scale * 0.5f,
						stain.size.x * scale, stain.size.y * scale }, {}, stain.color, stain.rotation );
			}

			void DrawEntity( const Enemy& enemy, float cameraX )
			{
				const Vec2 position{ enemy.position.x - cameraX, enemy.position.y };
				if( position.x < -150.0f || position.x > kLogicalWidth + 150.0f ) return;
				const uint32_t frame = static_cast<uint32_t>( enemy.walkCycle ) % 4u;
				const Color tint = enemy.hitFlash > 0.0f ? Color{ 1.0f, 0.35f, 0.20f, 1.0f } : Color{ 0.72f, 0.82f, 0.92f, 1.0f };
				DrawCowboyActor( position, enemy.facingDirection, frame, tint, enemy.health );
			}

			void DrawEntity( const Bullet& bullet, float cameraX )
			{
				const Color color = bullet.fromPlayer ? Color{ 1.0f, 0.82f, 0.22f, 1.0f } : Color{ 1.0f, 0.24f, 0.08f, 1.0f };
				DrawSprite( assets_.white, { bullet.position.x - cameraX - 8.0f, bullet.position.y - 3.0f, 16.0f, 6.0f }, {}, color );
			}

			void DrawEntity( const Particle& particle, float cameraX )
			{
				Color color = particle.color;
				const float fade = std::clamp( particle.life / particle.maximumLife * 2.6f, 0.0f, 1.0f );
				color.a *= fade;
				float width = particle.size.x * ( 0.55f + fade * 0.45f );
				const float height = particle.size.y * ( 0.55f + fade * 0.45f );
				const float speedStretch = std::clamp(
					( std::abs( particle.velocity.x ) + std::abs( particle.velocity.y ) ) / 430.0f, 0.0f, 1.0f );
				width *= 1.0f + speedStretch * 1.35f;
				queue_.RecordParticle(
					{ particle.position.x - cameraX - width * 0.5f, particle.position.y - height * 0.5f, width, height },
					color, std::atan2( particle.velocity.y, particle.velocity.x ) );
			}

			void DrawPlayer( const GameState& world )
			{
				const Vec2 position{ world.playerPosition.x - world.cameraX, world.playerPosition.y };
				const uint32_t frame = world.moving ? static_cast<uint32_t>( world.walkCycle ) % 4u : 0u;
				const Vec2 facing = world.playerFireCooldown > 0.0f ? world.aimDirection : world.movementDirection;
				DrawCowboyActor( position, facing, frame, {} );
			}

			void DrawScene( const GameState& world )
			{
				const float levelTravel = std::max( 1.0f, kLevelWidth - kLogicalWidth );
				const float backgroundPan = std::clamp( world.cameraX / levelTravel, 0.0f, 1.0f ) * 0.16f;
				DrawSprite( assets_.background, { 0.0f, 0.0f, kLogicalWidth, kLogicalHeight },
					{ backgroundPan, 0.0f, backgroundPan + 0.84f, 1.0f } );
				DrawMirroredParallax( assets_.midground, world.cameraX, 0.42f );
				DrawMirroredParallax( assets_.foreground, world.cameraX, 0.86f );
			}

			void DrawActors( const GameState& world )
			{
				struct ActorDraw
				{
					float feetY = 0.0f;
					const Enemy* enemy = nullptr;
					bool isPlayer = false;
				};
				std::vector<ActorDraw> actors;
				actors.reserve( world.enemies.size() + 1u );
				for( const Enemy& enemy : world.enemies )
				{
					const float screenX = enemy.position.x - world.cameraX;
					if( screenX >= -150.0f && screenX <= kLogicalWidth + 150.0f )
						actors.push_back( { enemy.position.y, &enemy, false } );
				}
				if( world.invulnerability <= 0.0f || static_cast<int>( world.animationTime * 12.0f ) % 2 == 0 )
					actors.push_back( { world.playerPosition.y, nullptr, true } );
				std::stable_sort( actors.begin(), actors.end(), []( const ActorDraw& left, const ActorDraw& right )
				{
					return left.feetY < right.feetY;
				} );
				for( const ActorDraw& actor : actors )
				{
					if( actor.isPlayer ) DrawPlayer( world );
					else DrawEntity( *actor.enemy, world.cameraX );
				}
			}

			void DrawEntities( const GameState& world )
			{
				for( const BloodStain& stain : world.bloodStains ) DrawEntity( stain, world.cameraX );
				for( const DeadBody& body : world.deadBodies ) DrawEntity( body, world.cameraX );
				DrawActors( world );
				for( const Bullet& bullet : world.bullets ) DrawEntity( bullet, world.cameraX );
				for( const Particle& particle : world.particles ) DrawEntity( particle, world.cameraX );
			}

			void DrawHud( const GameState& game )
			{
				const Color gold{ 0.96f, 0.68f, 0.27f, 1.0f };
				const Color paper{ 0.94f, 0.88f, 0.72f, 1.0f };
				DrawSprite( assets_.white, { 0.0f, 0.0f, kLogicalWidth, 92.0f }, {}, { 0.018f, 0.025f, 0.027f, 0.94f } );
				DrawSprite( assets_.white, { 0.0f, 88.0f, kLogicalWidth, 4.0f }, {}, { 0.77f, 0.42f, 0.12f, 1.0f } );
				DrawText( 22.0f, 18.0f, 3.0f, "FRONTIER RIDERS", gold );
				DrawText( 430.0f, 19.0f, 2.5f, "SCORE " + PaddedNumber( game.score, 6 ), paper );
				DrawText( 680.0f, 19.0f, 2.5f, "LIVES " + std::to_string( game.lives ), paper );
				DrawText( 885.0f, 19.0f, 2.5f, "TIME " + PaddedNumber( static_cast<int>( game.elapsed ), 3 ), gold );
				DrawText( 1080.0f, 19.0f, 2.5f, "HP", paper );
				DrawSprite( assets_.white, { 1125.0f, 25.0f, 130.0f, 18.0f }, {}, { 0.13f, 0.07f, 0.045f, 1.0f } );
				DrawSprite( assets_.white, { 1129.0f, 29.0f, 1.22f * static_cast<float>( game.health ), 10.0f }, {},
					game.health > 30 ? Color{ 0.2f, 0.78f, 0.25f, 1.0f } : Color{ 0.95f, 0.16f, 0.08f, 1.0f } );
				DrawText( 430.0f, 59.0f, 1.5f, "LEVEL 1", paper );
				DrawSprite( assets_.white, { 510.0f, 64.0f, 490.0f, 9.0f }, {}, { 0.13f, 0.07f, 0.045f, 1.0f } );
				DrawSprite( assets_.white,
					{ 513.0f, 67.0f, 484.0f * std::clamp( game.playerPosition.x / kLevelGoalX, 0.0f, 1.0f ), 3.0f }, {}, gold );
				DrawText( 1015.0f, 58.0f, 1.5f, "GOAL", gold );
				DrawText( 20.0f, 686.0f, 1.5f, "WASD MOVE  ARROWS AIM AND FIRE  SPACE FIRE  R RESTART",
					{ 0.91f, 0.78f, 0.55f, 0.92f } );

				if( !game.gameOver ) return;
				DrawSprite( assets_.white, { 270.0f, 230.0f, 740.0f, 245.0f }, {}, { 0.015f, 0.018f, 0.018f, 0.91f } );
				DrawSprite( assets_.white, { 274.0f, 234.0f, 732.0f, 5.0f }, {}, gold );
				DrawText( game.victory ? 425.0f : 490.0f, 285.0f, 4.0f,
					game.victory ? "LEVEL CLEARED" : "GAME OVER",
					game.victory ? gold : Color{ 0.95f, 0.25f, 0.12f, 1.0f } );
				DrawText( 455.0f, 365.0f, 2.5f, "PRESS R TO RESTART", paper );
			}

			RenderQueue& queue_;
			const AssetIndices& assets_;
		};

		void EmitSprite( ICommandBuffer& commands, const SpriteDrawCommand& draw )
		{
			SpritePushConstants constants;
			constants.x = draw.rect.x;
			constants.y = draw.rect.y;
			constants.width = draw.rect.width;
			constants.height = draw.rect.height;
			constants.u0 = draw.uv.u0;
			constants.v0 = draw.uv.v0;
			constants.u1 = draw.uv.u1;
			constants.v1 = draw.uv.v1;
			constants.red = draw.tint.r;
			constants.green = draw.tint.g;
			constants.blue = draw.tint.b;
			constants.alpha = draw.tint.a;
			constants.textureIndex = draw.textureIndex;
			constants.rotation = draw.rotation;
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDraw( 6 );
		}

		void EmitParticle( ICommandBuffer& commands, uint32_t whiteTextureIndex, const ParticleDrawCommand& draw )
		{
			EmitSprite( commands, { whiteTextureIndex, draw.rect, {}, draw.tint, draw.rotation } );
		}

		void EmitText( ICommandBuffer& commands, const TextDrawCommand& draw )
		{
			float x = draw.x;
			for( char character : draw.text )
			{
				if( character != ' ' )
				{
					const int glyphIndex = GlyphIndex( character );
					if( glyphIndex >= 0 )
					{
						const float column = static_cast<float>( glyphIndex % 8 );
						const float row = static_cast<float>( glyphIndex / 8 );
						EmitSprite( commands,
							{ draw.fontTextureIndex,
								{ x, draw.y, 6.0f * draw.scale, 8.0f * draw.scale },
								{ column / 8.0f, row / 5.0f, ( column + 1.0f ) / 8.0f, ( row + 1.0f ) / 5.0f },
								draw.color,
								0.0f } );
					}
				}
				x += 6.0f * draw.scale;
			}
		}

		void DrawRenderQueue( ICommandBuffer& commands, const RenderPipelineState& pipeline,
			uint32_t whiteTextureIndex, const RenderQueue& queue )
		{
			commands.CmdBindRenderPipeline( pipeline );
			for( const RenderCommandReference& reference : queue.DrawOrder() )
			{
				switch( reference.type )
				{
					case RenderCommandType::Sprite:
						EmitSprite( commands, queue.Sprites()[reference.index] );
						break;
					case RenderCommandType::Particle:
						EmitParticle( commands, whiteTextureIndex, queue.Particles()[reference.index] );
						break;
					case RenderCommandType::Text:
						EmitText( commands, queue.Texts()[reference.index] );
						break;
				}
			}
		}

	}

	FrontierRenderer& FrontierRenderer::Get() noexcept
	{
		static FrontierRenderer renderer;
		return renderer;
	}

	void FrontierRenderer::Initialize( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		if( device_ != nullptr ) throw std::runtime_error( "FrontierRenderer ya esta inicializado." );
		device_ = &device;

		const std::filesystem::path shaderFile = ResolveRuntimeFile( "shaders/Sprite2D.hlsl" );

		LightHLSLLoader::SetRootDirectory( shaderFile.parent_path().parent_path() );
		spritePipeline_ = CreateSpritePipeline( device, colorFormat );
		whiteTexture_ = UploadTexture( device, ImageData{ 1, 1, { PackRgba( 255, 255, 255 ) } }, "Frontier White" );
		fontTexture_ = UploadTexture( device, CreateFontImage(), "Frontier Font" );
		backgroundTexture_ = UploadTexture( device, LoadPngRgba( ResolveRuntimeFile( "assets/western_level_background_v2.png" ) ), "Frontier Far Background" );
		midgroundTexture_ = UploadTexture( device, LoadPngRgba( ResolveRuntimeFile( "assets/western_midground_v3.png" ), MatteMode::ChromaGreen ), "Frontier Midground" );
		foregroundTexture_ = UploadTexture( device, LoadPngRgba( ResolveRuntimeFile( "assets/western_ground_foreground_v2.png" ), MatteMode::Light ), "Frontier Foreground Ground" );
		cowboyTexture_ = UploadTexture( device,
			NormalizeCowboySheet( LoadPngRgba( ResolveRuntimeFile( "assets/cowboy_sheet.png" ), MatteMode::Light ) ), "Frontier Cowboy Atlas" );
		whiteIndex_ = device.GetBindlessIndex( whiteTexture_ );
		fontIndex_ = device.GetBindlessIndex( fontTexture_ );
		backgroundIndex_ = device.GetBindlessIndex( backgroundTexture_ );
		midgroundIndex_ = device.GetBindlessIndex( midgroundTexture_ );
		foregroundIndex_ = device.GetBindlessIndex( foregroundTexture_ );
		cowboyIndex_ = device.GetBindlessIndex( cowboyTexture_ );
		renderQueue_.Reserve( 512, 512, 32 );
	}

	void FrontierRenderer::Shutdown()
	{
		if( device_ == nullptr ) return;
		if( cowboyTexture_.Valid() ) device_->Destroy( cowboyTexture_ );
		if( foregroundTexture_.Valid() ) device_->Destroy( foregroundTexture_ );
		if( midgroundTexture_.Valid() ) device_->Destroy( midgroundTexture_ );
		if( backgroundTexture_.Valid() ) device_->Destroy( backgroundTexture_ );
		if( fontTexture_.Valid() ) device_->Destroy( fontTexture_ );
		if( whiteTexture_.Valid() ) device_->Destroy( whiteTexture_ );

		cowboyTexture_ = {};
		foregroundTexture_ = {};
		midgroundTexture_ = {};
		backgroundTexture_ = {};
		fontTexture_ = {};
		whiteTexture_ = {};
		spritePipeline_ = {};
		device_ = nullptr;
	}

	void FrontierRenderer::Draw( ICommandBuffer& commands, const GameState& game )
	{
		const AssetIndices assets{ whiteIndex_, fontIndex_, backgroundIndex_, midgroundIndex_, foregroundIndex_, cowboyIndex_ };
		renderQueue_.Clear();

		FrameRenderCollector( renderQueue_, assets ).Collect( game );

		DrawRenderQueue( commands, spritePipeline_, assets.white, renderQueue_ );
	}
}
