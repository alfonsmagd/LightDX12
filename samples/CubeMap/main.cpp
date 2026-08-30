#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"
#include "Ldx12Utils/Geometry.hpp"
#include "Ldx12Utils/OrbitCamera.hpp"
#include "Ldx12Utils/TextureLoader.hpp"

#include <DirectXMath.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>

using namespace DirectX;
using namespace ldx12;

namespace
{
	struct PushConstants
	{
		XMFLOAT4X4 viewProjection = {};
		XMFLOAT4X4 skyViewProjection = {};
		XMFLOAT4X4 model = {};
		XMFLOAT4 cameraPosition = {};
		uint32_t cubeMapIndex = 0;
		uint32_t samplerIndex = 0;
		std::array<uint32_t, 2> padding = {};
	};

	static_assert( sizeof( PushConstants ) == 224 );

	RenderPipelineState CreateObjectPipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/CubeMapObject.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/CubeMapObject.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 0 ].alignedByteOffset = 0;
		desc.inputElements[ 1 ].semanticName = "NORMAL";
		desc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 1 ].alignedByteOffset = sizeof( XMFLOAT3 );
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateSkyboxPipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/CubeMapSkybox.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/CubeMapSkybox.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	PushConstants BuildPushConstants( RenderDevice& device,
		const utils::OrbitCamera& camera,
		TextureHandle cubeMap,
		uint32_t width,
		uint32_t height,
		float time )
	{
		const float aspect = static_cast<float>( width ) / static_cast<float>( height );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 60.0f ), aspect, 0.1f, 100.0f );
		const XMVECTOR rotationAxis = XMVector3Normalize( XMVectorSet( 1.0f, 1.0f, 1.0f, 0.0f ) );
		const XMMATRIX model = XMMatrixRotationAxis( rotationAxis, time * 0.12f );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.viewProjection, XMMatrixTranspose( camera.GetViewMatrix() * projection ) );
		XMStoreFloat4x4( &constants.skyViewProjection, XMMatrixTranspose( camera.GetSkyViewMatrix() * projection ) );
		XMStoreFloat4x4( &constants.model, XMMatrixTranspose( model ) );
		XMStoreFloat4( &constants.cameraPosition, camera.GetPosition() );
		constants.cubeMapIndex = device.GetBindlessIndex( cubeMap );
		constants.samplerIndex = ToSamplerIndex( SamplerSlot::LinearClamp );
		return constants;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;

		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12CubeMapWindow";
		appDesc.title = L"Ldx12 - Cube Map";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;

		utils::AppLdx app( appDesc );

		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc context{};
		context.enableDebugLayer = true;

		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;

		DeviceManager& manager = DeviceManager::Initialize( context, swapchain );
		app.SetDeviceManager( manager );
		RenderDevice& device = *manager.GetRenderDevice();

		RenderPipelineState objectPipeline = CreateObjectPipeline( device, context.swapchainFormat );
		RenderPipelineState skyboxPipeline = CreateSkyboxPipeline( device, context.swapchainFormat );
		const std::filesystem::path cubeMapDirectory = std::filesystem::path( LDX12_MEDIA_DIRECTORY ) / "sky_129_cubemap_2k";
		const TextureHandle cubeMap = utils::LoadCubeMap( device, cubeMapDirectory );
		utils::GeometryBuffers cube = utils::CreateCube( device );
		utils::GeometryBuffers sphere = utils::CreateSphere( device );

		{
			utils::OrbitCamera camera;
			utils::DepthTarget depthTarget( device );
			bool showSphere = false;
			const std::chrono::steady_clock::time_point animationStart = std::chrono::steady_clock::now();

			while( app.PumpMessages() )
			{
				if( app.IsWindowMinimized() )
				{
					WaitMessage();
					continue;
				}

				camera.Update( app );
				if( app.WasKeyPressed( VK_SPACE ) )
				{
					showSphere = !showSphere;
					SetWindowTextW( app.GetWindow(), showSphere ? L"Ldx12 - Cube Map - Sphere" : L"Ldx12 - Cube Map - Cube" );
				}

				const uint32_t width = manager.GetWidth();
				const uint32_t height = manager.GetHeight();
				depthTarget.Resize( width, height );

				const float time = std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();
				const PushConstants constants = BuildPushConstants( device, camera, cubeMap, width, height, time );

				const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

				RenderPass renderPass{};
				renderPass.color[ 0 ].loadOp = LoadOp::Clear;
				renderPass.color[ 0 ].clearColor = { 0.02f, 0.025f, 0.04f, 1.0f };
				renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
				renderPass.depthStencil.clearDepth = 1.0f;

				Framebuffer framebuffer{};
				framebuffer.color[ 0 ].texture = backbuffer;
				framebuffer.depthStencil.texture = depthTarget.GetTexture();

				ICommandBuffer& commands = device.AcquireCommandBuffer();
				commands.CmdBeginRendering( renderPass, framebuffer );
				commands.CmdPushConstants( &constants, sizeof( constants ) );

				const utils::GeometryBuffers& geometry = showSphere ? sphere : cube;
				commands.CmdBindRenderPipeline( objectPipeline );
				commands.CmdBindVertexBuffer( geometry.vertexBuffer );
				commands.CmdBindIndexBuffer( geometry.indexBuffer );
				commands.CmdDrawIndexed( geometry.indexCount );

				commands.CmdBindRenderPipeline( skyboxPipeline );
				commands.CmdDraw( 36 );
				commands.CmdEndRendering();
				device.Submit( commands, backbuffer );
			}

			device.WaitIdle();
		}

		utils::DestroyGeometry( device, sphere );
		utils::DestroyGeometry( device, cube );

		device.Destroy( cubeMap );
		skyboxPipeline = {};
		objectPipeline = {};

		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Cube Map failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
