#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"

#include <DirectXMath.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>

using namespace DirectX;
using namespace ldx12;

namespace
{
	struct alignas( 16 ) PushConstants
	{
		XMFLOAT4X4 mvp;
	};

	static_assert( sizeof( PushConstants ) == 64 );

	struct GraphicsState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState solidPipeline;
		RenderPipelineState wireframePipeline;
	};

	RenderPipelineState CreatePipeline( RenderDevice& device, DXGI_FORMAT colorFormat, const char* vertexEntryPoint, D3D12_FILL_MODE fillMode )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/CookbookCube.hlsl", "vs_6_6", vertexEntryPoint );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/CookbookCube.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_UNKNOWN;
		desc.rasterizerState.FillMode = fillMode;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	PushConstants BuildPushConstants( float animationTime, float aspectRatio )
	{
		const XMVECTOR rotationAxis = XMVector3Normalize( XMVectorSet( 1.0f, 1.0f, 1.0f, 0.0f ) );
		const XMMATRIX model = XMMatrixRotationAxis( rotationAxis, animationTime ) * XMMatrixTranslation( 0.0f, 0.0f, 3.5f );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 45.0f ), aspectRatio, 0.1f, 1000.0f );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.mvp, XMMatrixTranspose( model * projection ) );
		return constants;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t kInitialWidth = 1280;
		constexpr uint32_t kInitialHeight = 720;
		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12CookbookChapter02Window";
		appDesc.title = L"Ldx12 - 3D Graphics Rendering Cookbook Chapter 02";
		appDesc.width = kInitialWidth;
		appDesc.height = kInitialHeight;
		utils::AppLdx app( appDesc );

		GraphicsState gfx{};
		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;
		contextDesc.pixSettings.enableGpuCapture = true;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = kInitialWidth;
		swapchainDesc.height = kInitialHeight;
		swapchainDesc.vsync = true;

		gfx.deviceManager = &DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.SetDeviceManager( *gfx.deviceManager );
		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		gfx.solidPipeline = CreatePipeline( device, contextDesc.swapchainFormat, "VSMainSolid", D3D12_FILL_MODE_SOLID );
		gfx.wireframePipeline = CreatePipeline( device, contextDesc.swapchainFormat, "VSMainWireframe", D3D12_FILL_MODE_WIREFRAME );

		const std::chrono::steady_clock::time_point animationStart = std::chrono::steady_clock::now();
		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			const float animationTime = std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();
			const float aspectRatio = static_cast<float>( gfx.deviceManager->GetWidth() ) / static_cast<float>( gfx.deviceManager->GetHeight() );
			const PushConstants constants = BuildPushConstants( animationTime, aspectRatio );

			CommandBuffer& commands = device.AcquireCommandBuffer();
			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;

			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdPushDebugGroupLabel( "Solid cube", 0xff0000ff );
			commands.CmdBindRenderPipeline( gfx.solidPipeline );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDraw( 36 );
			commands.CmdPopDebugGroupLabel();

			commands.CmdPushDebugGroupLabel( "Wireframe cube", 0xff0000ff );
			commands.CmdBindRenderPipeline( gfx.wireframePipeline );
			commands.CmdDraw( 36 );
			commands.CmdPopDebugGroupLabel();
			commands.CmdEndRendering();
			device.Submit( commands, backbuffer );
		}

		gfx.deviceManager->WaitIdle();
		gfx.solidPipeline = {};
		gfx.wireframePipeline = {};
		DeviceManager::ShutdownSingleton();
		gfx.deviceManager = nullptr;
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Cookbook Chapter 02 failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
