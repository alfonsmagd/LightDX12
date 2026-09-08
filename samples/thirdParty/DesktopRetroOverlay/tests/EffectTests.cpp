#include "RetroRenderer.hpp"
#include <Ldx12/HLSLLoader.hpp>
#include <Ldx12/Ldx12Native.hpp>
#include <d3d12sdklayers.h>
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace ldx12;
using namespace desktop_retro;

void Require( bool value, const char* message )
{
	if( !value )
		throw std::runtime_error( message );
}
void CheckErrors( RenderDevice& device )
{
	ComPtr<ID3D12InfoQueue> queue;
	if( FAILED( device.GetNative().GetDevice()->QueryInterface( IID_PPV_ARGS( queue.GetAddressOf() ) ) ) )
		return;
	for( UINT64 i = 0; i < queue->GetNumStoredMessages(); ++i )
	{
		SIZE_T size = 0;
		queue->GetMessage( i, nullptr, &size );
		std::vector<unsigned char> bytes( size );
		auto* message = reinterpret_cast<D3D12_MESSAGE*>( bytes.data() );
		if( SUCCEEDED( queue->GetMessage( i, message, &size ) ) && message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR )
			throw std::runtime_error( message->pDescription );
	}
}
int main()
{
	try
	{
		ContextDesc context{};
		auto& device = *DeviceManager::Initialize( context ).GetRenderDevice();
		HLSLLoader::SetRootDirectory( RETRO_TEST_SHADER_ROOT );
		{
			RetroRenderer renderer( device, DXGI_FORMAT_R8G8B8A8_UNORM );
			// No window, swapchain, WGC or D3D11: effects consume ordinary textures.
			// Resize between runs to exercise lazy NewPixie target replacement.
			for( uint32_t width : { 32u, 48u } )
			{
				std::vector<uint32_t> pixels( width * width, 0xff807060u );
				TextureDesc sourceDesc{};
				sourceDesc.width = sourceDesc.height = width;
				sourceDesc.data = pixels.data();
				sourceDesc.rowPitch = width * 4;
				sourceDesc.slicePitch = width * width * 4;
				const auto source = device.CreateTexture( sourceDesc );
				TextureDesc outputDesc{};
				outputDesc.width = outputDesc.height = width;
				outputDesc.usage = TextureUsage::Sampled | TextureUsage::RenderTarget;
				const auto output = device.CreateTexture( outputDesc );
				for( const auto& preset : kPresets )
				{
					auto settings = preset.settings;
					std::vector<uint32_t> first;
					// Resetting history must reproduce the first frame at a fixed time.
					for( int pass = 0; pass < 3; ++pass )
					{
						if( pass == 0 || pass == 2 )
							renderer.ResetHistory();
						auto& commands = device.AcquireCommandBuffer();
						renderer.Record( commands, output, { source, width, width }, settings, 0.f );
						device.Submit( commands );
						device.WaitIdle();
						std::vector<uint32_t> actual( pixels.size() );
						device.DownloadTexture2D( output, actual.data(), width * 4, width * width * 4 );
						const auto center = actual[ ( width / 2 ) * width + width / 2 ];
						Require( ( center & 0x00ffffffu ) != 0, "Filter produced a black center." );
						Require( ( center >> 24 ) == 255, "Filter must remain opaque." );
						if( pass == 0 )
							first = actual;
						if( pass == 2 )
							Require( actual == first, "History reset did not reproduce the initial frame." );
					}
				}
				device.Destroy( source );
				device.Destroy( output );
			}
		}
		CheckErrors( device );
		DeviceManager::ShutdownSingleton();
		std::cout << "Five presets, two sizes, temporal history reset and headless rendering passed.\n";
		return 0;
	}
	catch( const std::exception& error )
	{
		std::cerr << error.what() << '\n';
		DeviceManager::ShutdownSingleton();
		return 1;
	}
}
