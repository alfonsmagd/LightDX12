#include "TestTemplate.hpp"
#include "Ldx12/Ldx12Native.hpp"

#include <array>
#include <cstring>

namespace ldx12::tests
{
	void TestGpuResourceLifecycle()
	{
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();
		D3D12Native native = device.GetNative();

		// Upload a buffer, update its middle, and inspect the complete contents.
		const std::array<uint32_t, 8> initialData = { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u };
		BufferDesc bufferDesc{};
		bufferDesc.debugName = "Resource lifecycle upload buffer";
		bufferDesc.size = sizeof( initialData );
		bufferDesc.memory = BufferMemory::CpuToGpu;
		bufferDesc.initialData = initialData.data();

		const BufferHandle buffer = device.CreateBuffer( bufferDesc );
		const std::array<uint32_t, 2> update = { 30u, 40u };

		device.WriteBuffer( buffer, 2u * sizeof( uint32_t ), update.data(), sizeof( update ) );
		RequireThrows<std::runtime_error>( [ & ] { device.WriteBuffer( buffer, bufferDesc.size, update.data(), sizeof( update ) ); },
			"An out-of-range buffer write was accepted." );

		void* mapped = nullptr;
		const D3D12_RANGE readRange = { 0, sizeof( initialData ) };
		ID3D12Resource* nativeBuffer = native.GetResource( buffer );

		Require( SUCCEEDED( nativeBuffer->Map( 0, &readRange, &mapped ) ), "Could not inspect the upload buffer." );
		std::array<uint32_t, 8> actual{};

		std::memcpy( actual.data(), mapped, sizeof( actual ) );
		const D3D12_RANGE noWrites = { 0, 0 };

		nativeBuffer->Unmap( 0, &noWrites );
		const std::array<uint32_t, 8> expected = { 1u, 2u, 30u, 40u, 5u, 6u, 7u, 8u };

		Require( actual == expected, "Buffer upload/update changed the wrong bytes." );
		Require( device.Destroy( buffer ), "Could not destroy the uploaded buffer." );

		const BufferHandle replacementBuffer = device.CreateBuffer( bufferDesc );

		Require( !device.IsAlive( buffer ) && !device.Destroy( buffer ), "The retired buffer handle affects its replacement." );
		Require( device.IsAlive( replacementBuffer ), "Stale destruction removed the replacement buffer." );
		RequireThrows<std::runtime_error>( [ & ] { device.GetBindlessIndex( buffer ); }, "A query accepted the retired buffer handle." );

		// Upload a sampled texture, read its pixels back, then reuse its released slot.
		const std::array<uint32_t, 4> pixels = { 0xff0000ffu, 0xff00ff00u, 0xffff0000u, 0xffffffffu };
		TextureDesc textureDesc{};
		textureDesc.debugName = "Resource lifecycle sampled texture";
		textureDesc.width = 2;
		textureDesc.height = 2;
		textureDesc.data = pixels.data();
		textureDesc.rowPitch = 2u * sizeof( uint32_t );
		textureDesc.slicePitch = sizeof( pixels );

		const TextureHandle texture = device.CreateTexture( textureDesc );
		std::array<uint32_t, 4> downloadedPixels{};

		device.DownloadTexture2D( texture, downloadedPixels.data(), textureDesc.rowPitch, sizeof( downloadedPixels ) );
		Require( downloadedPixels == pixels, "Texture upload/readback changed the pixels." );
		Require( device.Destroy( texture ), "Could not destroy the uploaded texture." );

		const TextureHandle replacementTexture = device.CreateTexture( textureDesc );

		Require( !device.IsAlive( texture ) && !device.Destroy( texture ), "The retired texture handle affects its replacement." );
		Require( device.IsAlive( replacementTexture ), "Stale destruction removed the replacement texture." );
		RequireThrows<std::runtime_error>( [ & ] { static_cast<void>( native.GetResource( texture ) ); },
			"Native access accepted the retired texture handle." );

		device.Destroy( replacementTexture );
		device.Destroy( replacementBuffer );
		device.WaitIdle();
	}
}
