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

		// Upload padded rows and slices, then sample every voxel into a 2D target.
		constexpr uint32_t volumeWidth = 3;
		constexpr uint32_t volumeHeight = 2;
		constexpr uint32_t volumeDepth = 3;
		constexpr uint32_t sourceRowElements = 5;
		constexpr uint32_t sourceSliceElements = 12;
		std::array<uint32_t, sourceSliceElements * volumeDepth> voxels{};
		std::array<uint32_t, volumeWidth * volumeHeight * volumeDepth> expectedVoxels{};

		voxels.fill( 0xffff00ffu );

		for( uint32_t z = 0; z < volumeDepth; ++z )
		{
			for( uint32_t y = 0; y < volumeHeight; ++y )
			{
				for( uint32_t x = 0; x < volumeWidth; ++x )
				{
					const uint32_t red = ( x + 1u ) * 60u;
					const uint32_t green = ( y + 1u ) * 70u;
					const uint32_t blue = ( z + 1u ) * 50u;
					const uint32_t color = 0xff000000u | ( blue << 16u ) | ( green << 8u ) | red;

					voxels[ z * sourceSliceElements + y * sourceRowElements + x ] = color;
					expectedVoxels[ ( z * volumeHeight + y ) * volumeWidth + x ] = color;
				}
			}
		}

		TextureDesc volumeDesc{};
		volumeDesc.dimension = TextureDimension::Texture3D;
		volumeDesc.width = volumeWidth;
		volumeDesc.height = volumeHeight;
		volumeDesc.depthOrArraySize = volumeDepth;
		volumeDesc.data = voxels.data();
		volumeDesc.rowPitch = sourceRowElements * sizeof( uint32_t );
		volumeDesc.slicePitch = sourceSliceElements * sizeof( uint32_t );

		const TextureHandle volume = device.CreateTexture( volumeDesc );

		TextureDesc targetDesc{};
		targetDesc.width = volumeWidth;
		targetDesc.height = volumeHeight * volumeDepth;
		targetDesc.usage = TextureUsage::RenderTarget;

		const TextureHandle target = device.CreateTexture( targetDesc );

		static constexpr char volumeShader[] = R"(
cbuffer Bindings : register(b0) { uint volumeIndex; };

float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    const float2 positions[3] = { float2(-1.0, -1.0), float2(-1.0, 3.0), float2(3.0, -1.0) };
    return float4(positions[vertexId], 0.0, 1.0);
}

float4 PSMain(float4 position : SV_Position) : SV_Target0
{
    Texture3D<float4> volume = ResourceDescriptorHeap[volumeIndex];
    const uint2 pixel = uint2(position.xy);
    return volume.Load(int4(pixel.x, pixel.y % 2, pixel.y / 2, 0));
}
)";

		RenderPipelineDesc pipelineDesc{};
		pipelineDesc.vertexShader = { .source = volumeShader, .entryPoint = "VSMain" };
		pipelineDesc.fragmentShader = { .source = volumeShader, .entryPoint = "PSMain" };
		pipelineDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		RenderPipelineState pipeline = device.CreateRenderPipeline( pipelineDesc );
		const uint32_t volumeIndex = device.GetBindlessIndex( volume );

		RenderPass pass{};
		pass.color[ 0 ].loadOp = LoadOp::Clear;

		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = target;

		CommandBuffer& commands = device.AcquireCommandBuffer();
		commands.CmdBeginRendering( pass, framebuffer );
		commands.CmdBindRenderPipeline( pipeline );
		commands.CmdPushConstants( volumeIndex );
		commands.CmdDraw( 3 );
		commands.CmdEndRendering();
		device.Wait( device.Submit( commands ) );

		std::array<uint32_t, expectedVoxels.size()> actualVoxels{};

		device.DownloadTexture2D( target, actualVoxels.data(), volumeWidth * sizeof( uint32_t ), sizeof( actualVoxels ) );
		Require( actualVoxels == expectedVoxels, "Texture3D initial upload changed voxel data or ignored source row/slice padding." );

		device.Destroy( target );
		device.Destroy( volume );
		device.WaitIdle();
	}
}
