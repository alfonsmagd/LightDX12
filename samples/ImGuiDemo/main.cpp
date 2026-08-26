#include "App/imgui_impl_ldx12.h"
#include "Ldx12/Ldx12.hpp"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"

#include <array>
#include <cstdint>
#include <exception>
#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

using namespace ldx12;

namespace
{
	constexpr uint32_t ourFramesInFlight = 3;

	struct WindowState final
	{
		DeviceManager* manager = nullptr;
		bool running = true;
		bool minimized = false;
	};

	LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if( ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) )
		{
			return 1;
		}

		WindowState* state = reinterpret_cast<WindowState*>( GetWindowLongPtr( window, GWLP_USERDATA ) );
		switch( message )
		{
			case WM_SIZE:
			{
				if( state != nullptr && state->manager != nullptr )
				{
					const uint32_t width = LOWORD( lParam );
					const uint32_t height = HIWORD( lParam );
					state->minimized = width == 0 || height == 0;
					if( !state->minimized )
					{
						state->manager->Resize( width, height );
					}
				}
				return 0;
			}

			case WM_CLOSE:
				if( state != nullptr )
				{
					state->running = false;
				}
				return 0;

			case WM_DESTROY:
				PostQuitMessage( 0 );
				return 0;

			default:
				return DefWindowProc( window, message, wParam, lParam );
		}
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	static constexpr wchar_t ourWindowClassName[] = L"Ldx12ImGuiDemoWindow";
	HWND window = nullptr;
	bool imguiWin32Initialized = false;
	bool imguiLdx12Initialized = false;

	try
	{
		// 1. Create a Win32 window.
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof( windowClass );
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = ourWindowClassName;
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		if( RegisterClassExW( &windowClass ) == 0 )
		{
			throw std::runtime_error( "Failed to register the ImGui demo window class." );
		}

		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		window = CreateWindowExW(
			0,
			ourWindowClassName,
			L"Ldx12 - Dear ImGui bindless integration",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			static_cast<int>( initialWidth ),
			static_cast<int>( initialHeight ),
			nullptr,
			nullptr,
			instance,
			nullptr );
		if( window == nullptr )
		{
			throw std::runtime_error( "Failed to create the ImGui demo window." );
		}

		WindowState windowState{};
		SetWindowLongPtr( window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &windowState ) );
		ShowWindow( window, showCommand );
		UpdateWindow( window );

		// 2. Initialize Ldx12.
		ContextDesc contextDesc{};
		contextDesc.swapchainBufferCount = ourFramesInFlight;
		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( window );
		swapchainDesc.width = initialWidth;
		swapchainDesc.height = initialHeight;
		swapchainDesc.vsync = true;

		DeviceManager& manager = DeviceManager::Initialize( contextDesc, swapchainDesc );
		windowState.manager = &manager;
		RenderDevice& device = *manager.GetRenderDevice();

		// 3. Create a sampled Ldx12 texture. Ldx12 creates its bindless SRV.
		std::array<uint32_t, 64u * 64u> checkerPixels{};
		for( uint32_t y = 0; y < 64u; ++y )
		{
			for( uint32_t x = 0; x < 64u; ++x )
			{
				const bool white = ( ( x / 8u ) + ( y / 8u ) ) % 2u == 0u;
				checkerPixels[ y * 64u + x ] = white ? 0xffffffffu : 0xff000000u;
			}
		}

		TextureDesc checkerDesc{};
		checkerDesc.debugName = "ImGui checker texture";
		checkerDesc.width = 64u;
		checkerDesc.height = 64u;
		checkerDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		checkerDesc.usage = TextureUsage::Sampled;
		checkerDesc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		checkerDesc.data = checkerPixels.data();
		checkerDesc.rowPitch = 64u * sizeof( uint32_t );
		checkerDesc.slicePitch = checkerDesc.rowPitch * 64u;
		const TextureHandle checkerTexture = device.CreateTexture( checkerDesc );

		// 4. Keep ImGui's Win32 backend and render with imgui_impl_ldx12.
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		if( !ImGui_ImplWin32_Init( window ) )
		{
			throw std::runtime_error( "ImGui Win32 initialization failed." );
		}
		imguiWin32Initialized = true;

		ImGui_ImplLdx12_InitInfo imguiInfo{};
		imguiInfo.device = &device;
		imguiInfo.framesInFlight = ourFramesInFlight;
		imguiInfo.renderTargetFormat = contextDesc.swapchainFormat;
		if( !ImGui_ImplLdx12_Init( imguiInfo ) )
		{
			throw std::runtime_error( "ImGui Ldx12 initialization failed." );
		}
		imguiLdx12Initialized = true;

		std::array<SubmitHandle, ourFramesInFlight> frameSubmissions{};
		uint32_t frameIndex = 0;
		int buttonPressCount = 0;
		float demoValue = 0.5f;
		MSG message{};
		while( windowState.running )
		{
			while( PeekMessage( &message, nullptr, 0, 0, PM_REMOVE ) )
			{
				if( message.message == WM_QUIT )
				{
					windowState.running = false;
					break;
				}
				TranslateMessage( &message );
				DispatchMessage( &message );
			}

			if( !windowState.running )
			{
				break;
			}
			if( windowState.minimized )
			{
				WaitMessage();
				continue;
			}

			device.Wait( frameSubmissions[ frameIndex ] );

			// 5. TextureHandle can be passed directly to ImGui::Image().
			ImGui_ImplLdx12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			ImGui::Begin( "Ldx12 + Dear ImGui" );
			ImGui::TextUnformatted( "Dear ImGui is rendering directly through Ldx12." );
			ImGui::TextUnformatted( "64x64 Ldx12 texture:" );
			ImGui::Image( checkerTexture, ImVec2( 128.0f, 128.0f ) );
			ImGui::SliderFloat( "Value", &demoValue, 0.0f, 1.0f );
			if( ImGui::Button( "Press me" ) )
			{
				buttonPressCount++;
			}
			ImGui::SameLine();
			ImGui::Text( "Pressed %d times", buttonPressCount );
			ImGui::End();

			// 6. Render ImGui through the same Ldx12 command buffer and bindless heap.
			ICommandBuffer& commands = device.AcquireCommandBuffer();
			const TextureHandle backBuffer = device.GetCurrentSwapchainTexture();
			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.04f, 0.05f, 0.08f, 1.0f };
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backBuffer;

			commands.CmdBeginRendering( renderPass, framebuffer );
			ImGui::Render();
			ImGui_ImplLdx12_RenderDrawData( ImGui::GetDrawData(), commands );
			commands.CmdEndRendering();

			frameSubmissions[ frameIndex ] = device.Submit( commands, backBuffer );
			frameIndex = ( frameIndex + 1u ) % ourFramesInFlight;
		}

		// 7. Shut down while Ldx12 is still alive.
		manager.WaitIdle();
		ImGui_ImplLdx12_Shutdown();
		imguiLdx12Initialized = false;
		ImGui_ImplWin32_Shutdown();
		imguiWin32Initialized = false;
		ImGui::DestroyContext();
		device.Destroy( checkerTexture );

		windowState.manager = nullptr;
		SetWindowLongPtr( window, GWLP_USERDATA, 0 );
		DeviceManager::ShutdownSingleton();
		DestroyWindow( window );
		window = nullptr;
		UnregisterClassW( ourWindowClassName, instance );
		return 0;
	}
	catch( const std::exception& error )
	{
		if( imguiLdx12Initialized )
		{
			ImGui_ImplLdx12_Shutdown();
		}
		if( imguiWin32Initialized )
		{
			ImGui_ImplWin32_Shutdown();
		}
		if( ImGui::GetCurrentContext() != nullptr )
		{
			ImGui::DestroyContext();
		}
		DeviceManager::ShutdownSingleton();
		if( window != nullptr && IsWindow( window ) != FALSE )
		{
			DestroyWindow( window );
		}
		UnregisterClassW( ourWindowClassName, instance );
		MessageBoxA( nullptr, error.what(), "Ldx12 ImGuiDemo", MB_ICONERROR | MB_OK );
		return 1;
	}
}
