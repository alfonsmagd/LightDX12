#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"

#include <DirectXMath.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>

using namespace DirectX;
using namespace ldx12;

namespace
{
	struct alignas( 16 ) PushConstants
	{
		XMFLOAT4X4 mvp;
	};

	static_assert( sizeof( PushConstants ) == 64 );

	struct AppState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState solidPipeline;
		RenderPipelineState wireframePipeline;
		bool running = true;
		bool minimized = false;
	};

	LRESULT CALLBACK WindowProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		auto* app = reinterpret_cast<AppState*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );

		switch( message )
		{
			case WM_SIZE:
				if( app != nullptr && app->deviceManager != nullptr )
				{
					const uint32_t width = LOWORD( lParam );
					const uint32_t height = HIWORD( lParam );
					app->minimized = width == 0 || height == 0;
					if( !app->minimized )
					{
						app->deviceManager->Resize( width, height );
					}
				}
				return 0;

			case WM_CLOSE:
				if( app != nullptr )
				{
					app->running = false;
				}
				return 0;

			case WM_DESTROY:
				PostQuitMessage( 0 );
				return 0;

			default:
				return DefWindowProc( hwnd, message, wParam, lParam );
		}
	}

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
		const XMMATRIX projection = XMMatrixPerspectiveFovLH(
			XMConvertToRadians( 45.0f ),
			aspectRatio,
			0.1f,
			1000.0f );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.mvp, XMMatrixTranspose( model * projection ) );
		return constants;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof( WNDCLASSEX );
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = L"Ldx12CookbookChapter02Window";
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		RegisterClassExW( &windowClass );

		constexpr uint32_t kInitialWidth = 1280;
		constexpr uint32_t kInitialHeight = 720;
		HWND hwnd = CreateWindowExW(
			0,
			windowClass.lpszClassName,
			L"Ldx12 - 3D Graphics Rendering Cookbook Chapter 02",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			static_cast<int>( kInitialWidth ),
			static_cast<int>( kInitialHeight ),
			nullptr,
			nullptr,
			instance,
			nullptr );

		if( hwnd == nullptr )
		{
			throw std::runtime_error( "Failed to create Win32 window." );
		}

		ShowWindow( hwnd, showCommand );
		UpdateWindow( hwnd );

		AppState app{};
		SetWindowLongPtr( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &app ) );
		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;
		contextDesc.pixSettings.enableGpuCapture = true;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( hwnd );
		swapchainDesc.width = kInitialWidth;
		swapchainDesc.height = kInitialHeight;
		swapchainDesc.vsync = true;

		app.deviceManager = &DeviceManager::Initialize( contextDesc, swapchainDesc );
		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		app.solidPipeline = CreatePipeline( device, contextDesc.swapchainFormat, "VSMainSolid", D3D12_FILL_MODE_SOLID );
		app.wireframePipeline = CreatePipeline( device, contextDesc.swapchainFormat, "VSMainWireframe", D3D12_FILL_MODE_WIREFRAME );

		const auto animationStart = std::chrono::steady_clock::now();
		MSG message{};
		while( app.running )
		{
			while( PeekMessage( &message, nullptr, 0, 0, PM_REMOVE ) )
			{
				if( message.message == WM_QUIT )
				{
					app.running = false;
					break;
				}
				TranslateMessage( &message );
				DispatchMessage( &message );
			}

			if( !app.running || app.minimized )
			{
				continue;
			}

			const float animationTime =
				std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();
			const float aspectRatio =
				static_cast<float>( app.deviceManager->GetWidth() ) /
				static_cast<float>( app.deviceManager->GetHeight() );
			const PushConstants constants = BuildPushConstants( animationTime, aspectRatio );

			ICommandBuffer& commands = device.AcquireCommandBuffer();
			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;

			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdPushDebugGroupLabel( "Solid cube", 0xff0000ff );
			commands.CmdBindRenderPipeline( app.solidPipeline );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDraw( 36 );
			commands.CmdPopDebugGroupLabel();

			commands.CmdPushDebugGroupLabel( "Wireframe cube", 0xff0000ff );
			commands.CmdBindRenderPipeline( app.wireframePipeline );
			commands.CmdDraw( 36 );
			commands.CmdPopDebugGroupLabel();
			commands.CmdEndRendering();
			device.Submit( commands, backbuffer );
		}

		SetWindowLongPtr( hwnd, GWLP_USERDATA, 0 );
		app.deviceManager->WaitIdle();
		app.solidPipeline = {};
		app.wireframePipeline = {};
		DeviceManager::ShutdownSingleton();
		app.deviceManager = nullptr;

		if( IsWindow( hwnd ) != FALSE )
		{
			DestroyWindow( hwnd );
		}
		UnregisterClassW( windowClass.lpszClassName, instance );
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Cookbook Chapter 02 failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
