#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/Ldx12Utils.hpp"

#include <cstdint>
#include <stdexcept>

using namespace ldx12;
using namespace ldx12::utils;

namespace
{
	struct DepthTarget
	{
		TextureHandle texture = {};
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct AppState
	{
		DeviceManager* deviceManager = nullptr;
		DepthTarget depthTarget = {};
		bool running = true;
		bool minimized = false;
		bool toggleSphere = false;
	};

	LRESULT CALLBACK WindowProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		AppState* app = reinterpret_cast<AppState*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );
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

			case WM_KEYDOWN:
				if( app != nullptr && wParam == VK_SPACE )
				{
					app->toggleSphere = true;
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

	void DestroyDepthTarget( RenderDevice& device, DepthTarget& depthTarget )
	{
		if( depthTarget.texture.Valid() )
		{
			device.Destroy( depthTarget.texture );
			depthTarget.texture = {};
		}
		depthTarget.width = 0;
		depthTarget.height = 0;
	}

	void RecreateDepthTarget( AppState& app )
	{
		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		const uint32_t width = app.deviceManager->GetWidth();
		const uint32_t height = app.deviceManager->GetHeight();
		if( app.depthTarget.texture.Valid() && app.depthTarget.width == width && app.depthTarget.height == height )
		{
			return;
		}

		DestroyDepthTarget( device, app.depthTarget );
		TextureDesc desc{};
		desc.debugName = "World Geometry Depth";
		desc.width = width;
		desc.height = height;
		desc.format = DXGI_FORMAT_D32_FLOAT;
		desc.usage = TextureUsage::DepthStencil;
		desc.useClearValue = true;
		desc.clearValue.Format = desc.format;
		desc.clearValue.DepthStencil.Depth = 1.0f;
		app.depthTarget.texture = device.CreateTexture( desc );
		app.depthTarget.width = width;
		app.depthTarget.height = height;
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
		windowClass.lpszClassName = L"Ldx12WorldGeometryWindow";
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		RegisterClassExW( &windowClass );

		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		HWND hwnd = CreateWindowExW(
			0,
			windowClass.lpszClassName,
			L"Ldx12 World - Instanced geometry and debug wireframes",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			static_cast<int>( initialWidth ),
			static_cast<int>( initialHeight ),
			nullptr,
			nullptr,
			instance,
			nullptr );

		if( hwnd == nullptr )
		{
			throw std::runtime_error( "Failed to create the World Geometry window." );
		}

		ShowWindow( hwnd, showCommand );
		UpdateWindow( hwnd );

		AppState app{};
		SetWindowLongPtr( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &app ) );

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;
		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( hwnd );
		swapchainDesc.width = initialWidth;
		swapchainDesc.height = initialHeight;
		swapchainDesc.vsync = true;

		app.deviceManager = &DeviceManager::Initialize( contextDesc, swapchainDesc );
		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		RecreateDepthTarget( app );

		World world;
		CubeDesc cubeDesc{};
		cubeDesc.transform.position = { -3.0f, -0.6f, 0.0f };
		cubeDesc.transform.rotation = { 0.15f, 0.25f, 0.0f };
		cubeDesc.color = { 1.0f, 0.20f, 0.15f, 1.0f };
		world.AddCube( cubeDesc );

		cubeDesc.transform.position = { -1.5f, -0.2f, 0.4f };
		cubeDesc.transform.rotation = { 0.25f, 0.55f, 0.0f };
		cubeDesc.size = { 0.8f, 1.6f, 0.8f };
		cubeDesc.color = { 1.0f, 0.60f, 0.10f, 1.0f };
		world.AddCube( cubeDesc );

		cubeDesc.transform.position = { 0.0f, 0.1f, 0.0f };
		cubeDesc.transform.rotation = { 0.35f, 0.80f, 0.1f };
		cubeDesc.size = { 1.2f, 1.2f, 1.2f };
		cubeDesc.color = { 0.20f, 0.90f, 0.35f, 1.0f };
		world.AddCube( cubeDesc );

		cubeDesc.transform.position = { 1.5f, -0.2f, 0.4f };
		cubeDesc.transform.rotation = { 0.20f, 1.10f, 0.0f };
		cubeDesc.size = { 0.8f, 1.6f, 0.8f };
		cubeDesc.color = { 0.15f, 0.55f, 1.0f, 1.0f };
		world.AddCube( cubeDesc );

		cubeDesc.transform.position = { 3.0f, -0.6f, 0.0f };
		cubeDesc.transform.rotation = { 0.15f, 1.35f, 0.0f };
		cubeDesc.size = { 1.0f, 1.0f, 1.0f };
		cubeDesc.color = { 0.75f, 0.25f, 1.0f, 1.0f };
		world.AddCube( cubeDesc );

		SphereDesc sphereDesc{};
		sphereDesc.transform.position = { -2.25f, 1.15f, 0.2f };
		sphereDesc.radius = 0.8f;
		sphereDesc.color = { 0.20f, 0.95f, 1.0f, 1.0f };
		world.AddSphere( sphereDesc );

		sphereDesc.transform.position = { 2.25f, 1.15f, 0.2f };
		sphereDesc.color = { 1.0f, 0.25f, 0.75f, 1.0f };
		world.AddSphere( sphereDesc );

		SphereDesc debugSphereDesc{};
		debugSphereDesc.transform.position = { 0.0f, 1.75f, 0.3f };
		debugSphereDesc.radius = 1.0f;
		debugSphereDesc.color = { 1.0f, 0.95f, 0.20f, 1.0f };
		ObjectHandle debugSphere = world.AddSphere( debugSphereDesc );

		ArrowDesc arrowDesc{};
		arrowDesc.start = { -3.5f, -1.8f, 0.0f };
		arrowDesc.end = { -3.5f, 1.8f, 0.0f };
		arrowDesc.color = { 1.0f, 0.85f, 0.10f, 1.0f };
		world.AddArrow( arrowDesc );

		RenderWorldDesc renderWorldDesc{};
		renderWorldDesc.colorFormat = contextDesc.swapchainFormat;
		renderWorldDesc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		{
			RenderWorld renderWorld( device, renderWorldDesc );
			Camera camera{};
			camera.position = { 0.0f, 3.0f, -10.0f };
			camera.target = { 0.0f, 0.4f, 0.0f };

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

				if( app.toggleSphere )
				{
					if( world.Contains( debugSphere ) )
					{
						world.Destroy( debugSphere );
					}
					else
					{
						debugSphere = world.AddSphere( debugSphereDesc );
					}
					app.toggleSphere = false;
				}

				RecreateDepthTarget( app );
				camera.aspectRatio = static_cast<float>( app.deviceManager->GetWidth() ) /
					static_cast<float>( app.deviceManager->GetHeight() );

				ICommandBuffer& commands = device.AcquireCommandBuffer();
				const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
				RenderPass renderPass{};
				renderPass.color[ 0 ].loadOp = LoadOp::Clear;
				renderPass.color[ 0 ].clearColor = { 0.025f, 0.035f, 0.055f, 1.0f };
				renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
				renderPass.depthStencil.clearDepth = 1.0f;

				Framebuffer framebuffer{};
				framebuffer.color[ 0 ].texture = backbuffer;
				framebuffer.depthStencil.texture = app.depthTarget.texture;

				commands.CmdBeginRendering( renderPass, framebuffer );
				renderWorld.Render( commands, world, camera );
				commands.CmdEndRendering();
				device.Submit( commands, backbuffer );
			}
		}

		SetWindowLongPtr( hwnd, GWLP_USERDATA, 0 );
		device.WaitIdle();
		DestroyDepthTarget( device, app.depthTarget );
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
		MessageBoxA( nullptr, error.what(), "Ldx12 World Geometry failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
