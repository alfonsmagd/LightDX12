#include <Ldx12/Ldx12Native.hpp>
#include <array>
#include <iostream>
#include <stdexcept>

using namespace ldx12;

static void Require( bool condition, const char* message )
{
	if( !condition )
		throw std::runtime_error( message );
}

template <class F> static void Reject( F action )
{
	try
	{
		action();
	}
	catch( const std::invalid_argument& )
	{
		return;
	}
	throw std::runtime_error( "Invalid native resource was accepted." );
}

int main()
{
	try
	{
		ContextDesc context{};
		auto& device = *DeviceManager::Initialize( context ).GetRenderDevice();
		auto native = device.GetNative();
		Reject( [ & ] { (void)native.ImportSampledTexture2D( nullptr ); } );

		const std::array<uint32_t, 4> pixels{ 0xff123456u, 0xffabcdefu, 0xff112233u, 0xff998877u };
		TextureDesc desc{};
		desc.width = desc.height = 2;
		desc.data = pixels.data();
		desc.rowPitch = 8;
		desc.slicePitch = 16;
		// Repeated import/destroy checks descriptor recycling and COM ownership.
		for( int i = 0; i < 64; ++i )
		{
			auto original = device.CreateTexture( desc );
			auto imported = native.ImportSampledTexture2D( native.GetResource( original ) );
			device.WaitIdle();
			device.Destroy( original );
			Require( device.IsAlive( imported ), "Import did not retain its own handle." );
			Require( device.GetBindlessIndex( imported ) != UINT32_MAX, "Missing imported SRV." );
			std::array<uint32_t, 4> actual{};
			device.DownloadTexture2D( imported, actual.data(), 8, 16 );
			Require( actual == pixels, "Imported pixels differ after original handle release." );
			device.Destroy( imported );
			Require( !device.IsAlive( imported ), "Imported handle survived destruction." );
		}
		desc.data = nullptr;
		desc.countMipMap = 2;
		auto mipmapped = device.CreateTexture( desc );
		Reject( [ & ] { (void)native.ImportSampledTexture2D( native.GetResource( mipmapped ) ); } );
		device.Destroy( mipmapped );
		device.WaitIdle();
		DeviceManager::ShutdownSingleton();
		std::cout << "Native import: ownership, pixels, descriptor reuse and invalid inputs passed.\n";
		return 0;
	}
	catch( const std::exception& error )
	{
		std::cerr << error.what() << '\n';
		DeviceManager::ShutdownSingleton();
		return 1;
	}
}
