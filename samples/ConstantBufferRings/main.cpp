#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"
#include "Ldx12Utils/Geometry.hpp"

#include <DirectXMath.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>

using namespace DirectX;
using namespace ldx12;

namespace
{
	struct CubeData
	{
		XMFLOAT4X4 modelViewProjection = {};
		XMFLOAT4X4 model = {};
	};

	struct SphereData
	{
		XMFLOAT4X4 modelViewProjection = {};
		XMFLOAT4 position = { 0.0f, 0.0f, 0.0f, 0.7f };
	};

	struct LightData
	{
		XMFLOAT4 position = {};
		XMFLOAT4 color = { 1.0f, 0.45f, 0.12f, 1.0f };
	};

	static_assert( sizeof( CubeData ) == 128 );
	static_assert( sizeof( SphereData ) == 80 );
	static_assert( sizeof( LightData ) == 32 );

	RenderPipelineState CreatePipeline( RenderDevice& device, const char* vertexEntry, const char* pixelEntry )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/ConstantBufferRings.hlsl", "vs_6_6", vertexEntry );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/ConstantBufferRings.hlsl", "ps_6_6", pixelEntry );
		desc.color[ 0 ].format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 1 ].semanticName = "NORMAL";
		desc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 1 ].alignedByteOffset = offsetof( utils::GeometryVertex, normal );
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.rasterizerState.FrontCounterClockwise = TRUE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;

		return device.CreateRenderPipeline( desc );
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12ConstantBufferRingsWindow";
		appDesc.title = L"Ldx12 - Constant Buffer Rings";
		appDesc.width = 1280;
		appDesc.height = 720;

		utils::AppLdx app( appDesc );

		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = appDesc.width;
		swapchainDesc.height = appDesc.height;
		swapchainDesc.vsync = true;

		DeviceManager& manager = DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.SetDeviceManager( manager );
		RenderDevice& device = *manager.GetRenderDevice();

		RenderPipelineState cubePipeline = CreatePipeline( device, "VSCube", "PSCube" );
		RenderPipelineState spherePipeline = CreatePipeline( device, "VSSphere", "PSSphere" );
		utils::GeometryBuffers cubeGeometry = utils::CreateCube( device );
		utils::GeometryBuffers sphereGeometry = utils::CreateSphere( device, 32, 64 );
		utils::DepthTarget depthTarget( device );

		const XMMATRIX view = XMMatrixLookAtRH( XMVectorSet( 0.0f, 5.0f, 10.0f, 1.0f ), XMVectorZero(), XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ) );
		const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();

		while( app.PumpMessages() )
		{
			if( app.WasKeyPressed( VK_ESCAPE ) )
			{
				break;
			}
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			const uint32_t width = manager.GetWidth();
			const uint32_t height = manager.GetHeight();

			depthTarget.Resize( width, height );

			const float time = std::chrono::duration<float>( std::chrono::steady_clock::now() - startTime ).count();
			const float orbitAngle = time * 0.65f + 0.65f;
			const float aspectRatio = static_cast<float>( width ) / static_cast<float>( height );
			const XMMATRIX projection = XMMatrixPerspectiveFovRH( XM_PIDIV4, aspectRatio, 0.1f, 100.0f );

			CubeData cubeData{};
			const float rotationSpeed = 1.0f;
			const float rotationAngle = time * rotationSpeed;
			const XMMATRIX cubeModel =
				XMMatrixScaling( 0.55f, 0.55f, 0.55f ) * XMMatrixRotationX( rotationAngle ) * XMMatrixRotationY( rotationAngle ) * XMMatrixRotationZ( rotationAngle );

			XMStoreFloat4x4( &cubeData.model, cubeModel );
			XMStoreFloat4x4( &cubeData.modelViewProjection, cubeModel * view * projection );

			SphereData sphereData{};
			sphereData.position = { 3.2f * std::cos( orbitAngle ), 0.0f, 3.2f * std::sin( orbitAngle ), 0.7f };

			// Position.w stores the sphere radius.
			const float radius = sphereData.position.w;
			const XMMATRIX sphereModel =
				XMMatrixScaling( radius, radius, radius ) * XMMatrixTranslation( sphereData.position.x, sphereData.position.y, sphereData.position.z );

			XMStoreFloat4x4( &sphereData.modelViewProjection, sphereModel * view * projection );

			LightData lightData{};
			lightData.position = { sphereData.position.x, sphereData.position.y, sphereData.position.z, 1.0f };

			CommandBuffer& commands = device.AcquireCommandBuffer();
			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.015f, 0.02f, 0.035f, 1.0f };
			renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
			renderPass.depthStencil.clearDepth = 1.0f;

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;
			framebuffer.depthStencil.texture = depthTarget.GetTexture();

			commands.CmdBeginRendering( renderPass, framebuffer );

			// Both draws share the same point light through CB1.
			commands.SetConstantBuffer( lightData, 1 );

			// This draw reads CubeData from the current CB0 offset.
			commands.CmdBindRenderPipeline( cubePipeline );
			commands.SetConstantBuffer( cubeData, 0 );
			commands.CmdBindVertexBuffer( cubeGeometry.vertexBuffer );
			commands.CmdBindIndexBuffer( cubeGeometry.indexBuffer );
			commands.CmdDrawIndexed( cubeGeometry.indexCount );

			// A new CB0 offset selects SphereData; the previous draw keeps its own offset.
			commands.CmdBindRenderPipeline( spherePipeline );
			commands.SetConstantBuffer( sphereData, 0 );
			commands.CmdBindVertexBuffer( sphereGeometry.vertexBuffer );
			commands.CmdBindIndexBuffer( sphereGeometry.indexBuffer );
			commands.CmdDrawIndexed( sphereGeometry.indexCount );

			commands.CmdEndRendering();
			device.Submit( commands, backbuffer );
		}

		manager.WaitIdle();
		depthTarget.Reset();
		utils::DestroyGeometry( device, sphereGeometry );
		utils::DestroyGeometry( device, cubeGeometry );
		spherePipeline = {};
		cubePipeline = {};
		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Constant Buffer Rings failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
