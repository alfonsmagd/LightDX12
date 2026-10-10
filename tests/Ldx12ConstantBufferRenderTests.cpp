// Reuse the sample pipelines and data layouts without opening its window.
#include "../samples/ConstantBufferRings/main.cpp"
#include "Ldx12/Ldx12Native.hpp"
#include "Ldx12Internal.hpp"
#include "Ldx12/TestTemplate.hpp"
#include <iostream>

int main()
{
	try
	{
		tests::DeviceManagerGuard guard;
		ContextDesc context{};
		context.enableDebugLayer = true;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();
		D3D12Native native = device.GetNative();
		ComPtr<ID3D12InfoQueue> infoQueue;
		C_RESULT( native.GetDevice()->QueryInterface( IID_PPV_ARGS( infoQueue.GetAddressOf() ) ), "Info queue unavailable." );
		HLSLLoader::SetRootDirectory( LDX12_RING_SAMPLE_DIRECTORY );
		RenderPipelineState cubePipeline = CreatePipeline( device, "VSCube", "PSCube" );
		RenderPipelineState spherePipeline = CreatePipeline( device, "VSSphere", "PSSphere" );
		utils::GeometryBuffers cubeGeometry = utils::CreateCube( device );
		utils::GeometryBuffers sphereGeometry = utils::CreateSphere( device, 32, 64 );
		utils::DepthTarget depthTarget( device );
		depthTarget.Resize( 320, 180 );

		TextureDesc targetDesc{};
		targetDesc.width = 320;
		targetDesc.height = 180;
		targetDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		targetDesc.usage = TextureUsage::RenderTarget;
		const TextureHandle target = device.CreateTexture( targetDesc );
		const D3D12_RESOURCE_DESC nativeTargetDesc = native.GetResource( target )->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
		UINT64 readbackSize = 0;
		native.GetDevice()->GetCopyableFootprints( &nativeTargetDesc, 0, 1, 0, &footprint, nullptr, nullptr, &readbackSize );
		const CD3DX12_HEAP_PROPERTIES readbackHeap( D3D12_HEAP_TYPE_READBACK );
		const D3D12_RESOURCE_DESC readbackDesc = BufferResource::CreateNativeDesc( readbackSize );
		ComPtr<ID3D12Resource> readback;
		C_RESULT( native.GetDevice()->CreateCommittedResource( &readbackHeap,
					  D3D12_HEAP_FLAG_NONE,
					  &readbackDesc,
					  D3D12_RESOURCE_STATE_COPY_DEST,
					  nullptr,
					  IID_PPV_ARGS( readback.GetAddressOf() ) ),
			"Readback creation failed." );

		const XMMATRIX view = XMMatrixLookAtRH( XMVectorSet( 0, 5, 10, 1 ), XMVectorZero(), XMVectorSet( 0, 1, 0, 0 ) );
		const XMMATRIX projection = XMMatrixPerspectiveFovRH( XM_PIDIV4, 320.0f / 180.0f, 0.1f, 100.0f );
		uint64_t previousImageHash = 0;
		for( uint32_t frame = 0; frame < 900; ++frame )
		{
			CubeData cubeData{};
			const float time = static_cast<float>( frame ) / 60.0f;
			const XMMATRIX cubeModel =
				XMMatrixScaling( 0.55f, 0.55f, 0.55f ) * XMMatrixRotationX( time ) * XMMatrixRotationY( time ) * XMMatrixRotationZ( time );
			XMStoreFloat4x4( &cubeData.model, cubeModel );
			XMStoreFloat4x4( &cubeData.modelViewProjection, cubeModel * view * projection );
			SphereData sphereData{};
			const float angle = static_cast<float>( frame ) / 60.0f * 0.65f + 0.65f;
			sphereData.position = { 3.2f * std::cos( angle ), 0.0f, 3.2f * std::sin( angle ), 0.7f };
			LightData lightData{};
			lightData.position = { sphereData.position.x, sphereData.position.y, sphereData.position.z, 1.0f };
			const XMMATRIX model =
				XMMatrixScaling( 0.7f, 0.7f, 0.7f ) * XMMatrixTranslation( sphereData.position.x, sphereData.position.y, sphereData.position.z );
			XMStoreFloat4x4( &sphereData.modelViewProjection, model * view * projection );

			CommandBuffer& commands = device.AcquireCommandBuffer();
			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.015f, 0.02f, 0.035f, 1.0f };
			renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = target;
			framebuffer.depthStencil.texture = depthTarget.GetTexture();
			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.SetConstantBuffer( lightData, 1 );
			commands.CmdBindRenderPipeline( cubePipeline );
			commands.SetConstantBuffer( cubeData, 0 );
			commands.CmdBindVertexBuffer( cubeGeometry.vertexBuffer );
			commands.CmdBindIndexBuffer( cubeGeometry.indexBuffer );
			commands.CmdDrawIndexed( cubeGeometry.indexCount );
			commands.CmdBindRenderPipeline( spherePipeline );
			commands.SetConstantBuffer( sphereData, 0 );
			commands.CmdBindVertexBuffer( sphereGeometry.vertexBuffer );
			commands.CmdBindIndexBuffer( sphereGeometry.indexBuffer );
			commands.CmdDrawIndexed( sphereGeometry.indexCount );
			commands.CmdEndRendering();
			const bool capture = frame == 0 || frame == 150 || frame == 899;
			if( capture )
			{
				commands.CmdTransitionTexture( target, D3D12_RESOURCE_STATE_COPY_SOURCE );
				D3D12_TEXTURE_COPY_LOCATION source{};
				source.pResource = native.GetResource( target );
				source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				D3D12_TEXTURE_COPY_LOCATION destination{};
				destination.pResource = readback.Get();
				destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				destination.PlacedFootprint = footprint;
				native.GetCommandList( commands )->CopyTextureRegion( &destination, 0, 0, 0, &source, nullptr );
			}
			device.Wait( device.Submit( commands ) );
			if( capture )
			{
				void* mapped = nullptr;
				const D3D12_RANGE readRange{ 0, static_cast<SIZE_T>( readbackSize ) };
				C_RESULT( readback->Map( 0, &readRange, &mapped ), "Readback mapping failed." );
				uint32_t spherePixels = 0;
				uint32_t cubePixels = 0;
				uint64_t imageHash = 14695981039346656037ull;

				for( uint32_t row = 0; row < targetDesc.height; ++row )
				{
					const uint8_t* pixels = static_cast<const uint8_t*>( mapped ) + footprint.Offset + row * footprint.Footprint.RowPitch;

					for( uint32_t column = 0; column < targetDesc.width; ++column )
					{
						const uint8_t* pixel = pixels + column * 4;
						if( pixel[ 0 ] >= 254 && pixel[ 1 ] >= 176 && pixel[ 1 ] <= 178 && pixel[ 2 ] >= 96 && pixel[ 2 ] <= 98 )
						{
							++spherePixels;
						}
						if( pixel[ 0 ] > 30 && pixel[ 0 ] > pixel[ 1 ] * 3 && pixel[ 0 ] > pixel[ 2 ] * 3 )
						{
							++cubePixels;
						}
						for( uint32_t channel = 0; channel < 4; ++channel )
						{
							imageHash = ( imageHash ^ pixel[ channel ] ) * 1099511628211ull;
						}
					}
				}
				const D3D12_RANGE noWrites{ 0, 0 };
				readback->Unmap( 0, &noWrites );
				tests::Require( spherePixels > 50, "The emissive sphere lost its shared CB1 color." );
				tests::Require( cubePixels > 50, "The red cube was not rendered with its own CB0 data." );
				tests::Require( imageHash != previousImageHash, "Animation did not change the rendered image." );
				previousImageHash = imageHash;
			}
		}

		for( UINT64 index = 0; index < infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter(); ++index )
		{
			SIZE_T size = 0;
			infoQueue->GetMessage( index, nullptr, &size );
			std::unique_ptr<uint8_t[]> storage = std::make_unique<uint8_t[]>( size );
			D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>( storage.get() );
			C_RESULT( infoQueue->GetMessage( index, message, &size ), "Could not retrieve debug message." );
			if( message->Severity == D3D12_MESSAGE_SEVERITY_ERROR || message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION )
			{
				throw std::runtime_error( message->pDescription );
			}
		}
		depthTarget.Reset();
		utils::DestroyGeometry( device, sphereGeometry );
		utils::DestroyGeometry( device, cubeGeometry );
		device.Destroy( target );
		device.WaitIdle();
		std::cout << "[PASS] 900 frames rendered, two different structs per frame through CB0, ring 0 wrapped, no D3D12 errors.\n";
		return 0;
	}
	catch( const std::exception& error )
	{
		std::cerr << "[FAIL] " << error.what() << '\n';
		return 1;
	}
}
