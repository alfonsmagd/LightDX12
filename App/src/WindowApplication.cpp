#include "App/WindowApplication.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

using namespace lightd3d12;

namespace App
{
	namespace
	{
		struct WindowState
		{
			DeviceManager* deviceManager = nullptr;
			bool running = true;
			bool minimized = false;
		};

		LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
		{
			auto* state = reinterpret_cast<WindowState*>( GetWindowLongPtrW( window, GWLP_USERDATA ) );
			switch( message )
			{
				case WM_SIZE:
					if( state && state->deviceManager )
					{
						const uint32_t width = LOWORD( lParam );
						const uint32_t height = HIWORD( lParam );
						state->minimized = wParam == SIZE_MINIMIZED || width == 0 || height == 0;
						if( !state->minimized ) state->deviceManager->Resize( width, height );
					}
					return 0;
				case WM_KEYDOWN:
					if( state && wParam == VK_ESCAPE ) state->running = false;
					return 0;
				case WM_CLOSE:
					if( state ) state->running = false;
					return 0;
				case WM_ERASEBKGND:
					return 1;
				case WM_DESTROY:
					PostQuitMessage( 0 );
					return 0;
				default:
					return DefWindowProcW( window, message, wParam, lParam );
			}
		}

		void RenderFrame( IWindowApplication& application, RenderDevice& device, const WindowApplicationDesc& desc )
		{
			ICommandBuffer& commands = device.AcquireCommandBuffer();
			const TextureHandle backBuffer = device.GetCurrentSwapchainTexture();
			RenderPass renderPass{};
			renderPass.color[0].loadOp = LoadOp::Clear;
			renderPass.color[0].clearColor = desc.clearColor;
			Framebuffer framebuffer{};
			framebuffer.color[0].texture = backBuffer;
			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdPushDebugGroupLabel( desc.debugLabel, 0xff2b75d6u );
			application.Render( commands );
			commands.CmdPopDebugGroupLabel();
			commands.CmdEndRendering();
			device.Submit( commands, backBuffer );
		}
	}

	bool IsKeyDown( int key ) noexcept
	{
		return ( GetAsyncKeyState( key ) & 0x8000 ) != 0;
	}

	int RunWindowApplication( HINSTANCE instance, int showCommand, const WindowApplicationDesc& desc,
		IWindowApplication& application )
	{
		SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
		const HRESULT comResult = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
		const bool uninitializeCom = SUCCEEDED( comResult );
		if( FAILED( comResult ) && comResult != RPC_E_CHANGED_MODE ) return 1;

		HWND window = nullptr;
		bool applicationInitialized = false;
		try
		{
			WNDCLASSEXW windowClass{};
			windowClass.cbSize = sizeof( WNDCLASSEXW );
			windowClass.lpfnWndProc = WindowProc;
			windowClass.hInstance = instance;
			windowClass.lpszClassName = desc.className;
			windowClass.hCursor = LoadCursorW( nullptr, IDC_CROSS );
			if( RegisterClassExW( &windowClass ) == 0 ) throw std::runtime_error( "No se pudo registrar la ventana." );

			RECT windowRect{ 0, 0, static_cast<LONG>( desc.width ), static_cast<LONG>( desc.height ) };
			AdjustWindowRectExForDpi( &windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForSystem() );
			window = CreateWindowExW( 0, desc.className, desc.title, WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
				nullptr, nullptr, instance, nullptr );
			if( !window ) throw std::runtime_error( "No se pudo crear la ventana." );

			WindowState state;
			SetWindowLongPtrW( window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &state ) );
			ShowWindow( window, showCommand );
			UpdateWindow( window );
			RECT clientRect{};
			GetClientRect( window, &clientRect );

			ContextDesc contextDesc{};
			contextDesc.enableDebugLayer = desc.enableDebugLayer;
			contextDesc.framesInFlight = 3;
			contextDesc.swapchainBufferCount = 3;
			SwapchainDesc swapchainDesc{};
			swapchainDesc.window = MakeWin32WindowHandle( window );
			swapchainDesc.width = static_cast<uint32_t>( clientRect.right - clientRect.left );
			swapchainDesc.height = static_cast<uint32_t>( clientRect.bottom - clientRect.top );
			swapchainDesc.vsync = desc.vsync;
			state.deviceManager = &DeviceManager::Initialize( contextDesc, swapchainDesc );
			RenderDevice& device = *state.deviceManager->GetRenderDevice();
			application.Initialize( device, contextDesc.swapchainFormat );
			applicationInitialized = true;

			auto previousTime = std::chrono::steady_clock::now();
			MSG message{};
			while( state.running && !application.ShouldClose() )
			{
				while( PeekMessageW( &message, nullptr, 0, 0, PM_REMOVE ) )
				{
					if( message.message == WM_QUIT ) { state.running = false; break; }
					TranslateMessage( &message );
					DispatchMessageW( &message );
				}
				if( !state.running ) break;
				if( state.minimized ) { WaitMessage(); previousTime = std::chrono::steady_clock::now(); continue; }

				const auto currentTime = std::chrono::steady_clock::now();
				const float deltaSeconds = std::clamp(
					std::chrono::duration<float>( currentTime - previousTime ).count(), 0.0f, 0.05f );
				previousTime = currentTime;
				application.Physics( deltaSeconds );
				RenderFrame( application, device, desc );
			}

			SetWindowLongPtrW( window, GWLP_USERDATA, 0 );
			state.deviceManager->WaitIdle();
			application.Shutdown();
			applicationInitialized = false;
			DeviceManager::ShutdownSingleton();
			state.deviceManager = nullptr;
			DestroyWindow( window );
			window = nullptr;
			UnregisterClassW( desc.className, instance );
			if( uninitializeCom ) CoUninitialize();
			return 0;
		}
		catch( ... )
		{
			if( applicationInitialized ) application.Shutdown();
			DeviceManager::ShutdownSingleton();
			if( window && IsWindow( window ) ) DestroyWindow( window );
			UnregisterClassW( desc.className, instance );
			if( uninitializeCom ) CoUninitialize();
			throw;
		}
	}
}
