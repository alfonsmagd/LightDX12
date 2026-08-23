#include "Ldx12/Ldx12.hpp"

#include <cstdint>
#include <stdexcept>

using namespace ldx12;

namespace
{
	constexpr uint32_t ourRegionCount = 3;

	struct AppState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState pipeline;
		bool running = true;
		bool minimized = false;
	};

	struct DrawConstants
	{
		uint32_t colorIndex = 0;
		uint32_t fullscreen = 0;
	};

	LRESULT CALLBACK WindowProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		auto* app = reinterpret_cast<AppState*>( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );

		switch( message )
		{
			case WM_SIZE:
			{
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
			}

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

	RenderPipelineState CreateTrianglePipeline( RenderDevice& device )
	{
		static constexpr char ourVertexShader[] = R"(
cbuffer PushConstants : register(b0)
{
    uint gColorIndex;
    uint gFullscreen;
};

float4 main(uint vertexID : SV_VertexID) : SV_Position
{
    static const float2 trianglePositions[3] =
    {
        float2( 0.0,  0.80),
        float2( 0.80, -0.75),
        float2(-0.80, -0.75)
    };

    static const float2 fullscreenPositions[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };

    const float2 position = gFullscreen != 0
        ? fullscreenPositions[vertexID]
        : trianglePositions[vertexID];
    return float4(position, 0.0, 1.0);
}
)";

		static constexpr char ourPixelShader[] = R"(
cbuffer PushConstants : register(b0)
{
    uint gColorIndex;
    uint gFullscreen;
};

float4 main() : SV_Target0
{
    static const float3 colors[8] =
    {
        float3(0.04, 0.12, 0.22),
        float3(0.15, 0.75, 1.00),
        float3(0.20, 1.00, 0.45),
        float3(0.90, 0.25, 1.00),
        float3(0.12, 0.12, 0.14),
        float3(0.30, 0.30, 0.34),
        float3(1.00, 0.50, 0.05),
        float3(0.32, 0.04, 0.04)
    };

    return float4(colors[gColorIndex], 1.0);
}
)";

		RenderPipelineDesc desc{};
		desc.vertexShader.source = ourVertexShader;
		desc.vertexShader.profile = "vs_6_6";
		desc.fragmentShader.source = ourPixelShader;
		desc.fragmentShader.profile = "ps_6_6";
		desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	void DrawShape( ICommandBuffer& commands, uint32_t colorIndex, bool fullscreen )
	{
		const DrawConstants constants{ colorIndex, fullscreen ? 1u : 0u };
		commands.CmdPushConstants( &constants, sizeof( constants ) );
		commands.CmdDraw( 3 );
	}

	void RenderFrame( AppState& app )
	{
		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		const uint32_t width = app.deviceManager->GetWidth();
		const uint32_t height = app.deviceManager->GetHeight();
		const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

		RenderPass renderPass{};
		renderPass.color[ 0 ].loadOp = LoadOp::Clear;
		renderPass.color[ 0 ].clearColor = { 0.04f, 0.04f, 0.06f, 1.0f };

		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = backbuffer;

		ICommandBuffer& commands = device.AcquireCommandBuffer();
		commands.CmdBeginRendering( renderPass, framebuffer );
		commands.CmdBindRenderPipeline( app.pipeline );

		if( width >= 320 && height >= 240 )
		{
			const int32_t windowWidth = static_cast<int32_t>( width );
			const int32_t windowHeight = static_cast<int32_t>( height );
			const int32_t halfWidth = windowWidth / 2;
			const int32_t margin = 24;
			const int32_t gap = 12;
			const int32_t leftViewportWidth = halfWidth - margin * 2;
			const int32_t leftViewportHeight = ( windowHeight - margin * 2 - gap * 2 ) / static_cast<int32_t>( ourRegionCount );

			commands.CmdSetScissor( 0, 0, windowWidth, windowHeight );
			for( uint32_t index = 0; index < ourRegionCount; ++index )
			{
				const int32_t top = margin + static_cast<int32_t>( index ) * ( leftViewportHeight + gap );
				commands.CmdSetViewport( static_cast<float>( margin ), static_cast<float>( top ), static_cast<float>( leftViewportWidth ), static_cast<float>( leftViewportHeight ) );
				DrawShape( commands, 0, true );
				DrawShape( commands, index + 1u, false );
			}

			const int32_t rightLeft = halfWidth + margin;
			const int32_t rightTop = margin;
			const int32_t rightRight = windowWidth - margin;
			const int32_t rightBottom = windowHeight - margin;
			const int32_t rightWidth = rightRight - rightLeft;
			const int32_t rightHeight = rightBottom - rightTop;
			commands.CmdSetViewport( static_cast<float>( rightLeft ), static_cast<float>( rightTop ), static_cast<float>( rightWidth ), static_cast<float>( rightHeight ) );
			commands.CmdSetScissor( rightLeft, rightTop, rightRight, rightBottom );
			DrawShape( commands, 4, true );
			DrawShape( commands, 5, false );

			const int32_t scissorLeft = rightLeft + rightWidth / 4;
			const int32_t scissorTop = rightTop + rightHeight / 4;
			const int32_t scissorRight = rightRight - rightWidth / 4;
			const int32_t scissorBottom = rightBottom - rightHeight / 4;
			commands.CmdSetScissor( scissorLeft, scissorTop, scissorRight, scissorBottom );
			DrawShape( commands, 7, true );
			DrawShape( commands, 6, false );
		}

		commands.CmdEndRendering();
		device.Submit( commands, backbuffer );
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof( WNDCLASSEXW );
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = L"Ldx12ViewportScissorWindow";
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		if( RegisterClassExW( &windowClass ) == 0 )
		{
			throw std::runtime_error( "Failed to register the Win32 window class." );
		}

		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		HWND hwnd = CreateWindowExW(
			0,
			windowClass.lpszClassName,
			L"Left: three Viewports | Right: orange triangle clipped by the red Scissor",
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
			throw std::runtime_error( "Failed to create the Win32 window." );
		}

		AppState app{};
		SetWindowLongPtr( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &app ) );
		ShowWindow( hwnd, showCommand );

		ContextDesc context{};
		context.enableDebugLayer = true;

		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( hwnd );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;

		app.deviceManager = &DeviceManager::Initialize( context, swapchain );
		app.pipeline = CreateTrianglePipeline( *app.deviceManager->GetRenderDevice() );

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

			if( app.running && !app.minimized )
			{
				RenderFrame( app );
			}
		}

		SetWindowLongPtr( hwnd, GWLP_USERDATA, 0 );
		app.deviceManager->WaitIdle();
		app.pipeline = {};
		DeviceManager::ShutdownSingleton();
		app.deviceManager = nullptr;
		DestroyWindow( hwnd );
		UnregisterClassW( windowClass.lpszClassName, instance );
		return 0;
	}
	catch( const std::exception& exception )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, exception.what(), "Ldx12 ViewportScissor", MB_ICONERROR | MB_OK );
		return 1;
	}
}
