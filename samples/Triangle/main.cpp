#include "Ldx12/Ldx12.hpp"

#include <stdexcept>

using namespace ldx12;

namespace
{
	struct AppState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState trianglePipeline;
		bool running = true;
		bool minimized = false;
	};

	LRESULT CALLBACK WindowProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		auto* app = reinterpret_cast< AppState* >( GetWindowLongPtr( hwnd, GWLP_USERDATA ) );

		switch( message )
		{
			case WM_SIZE:
			{
				if( app != nullptr && app->deviceManager )
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

			case WM_DESTROY:
				PostQuitMessage( 0 );
				return 0;

			case WM_CLOSE:
			{
				if( app != nullptr )
				{
					app->running = false;
					app->minimized = true;
				}
				return 0;
			}

			default:
				return DefWindowProc( hwnd, message, wParam, lParam );
		}
	}

	RenderPipelineState CreateTrianglePipeline( RenderDevice& ctx )
	{
		static constexpr char ourVertexShader[] = R"(
struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;

    const float2 positions[3] =
    {
        float2( 0.0,  0.55),
        float2( 0.5, -0.45),
        float2(-0.5, -0.45)
    };

    const float3 colors[3] =
    {
        float3(1.0, 0.2, 0.2),
        float3(0.2, 1.0, 0.2),
        float3(0.2, 0.2, 1.0)
    };

    output.position = float4(positions[vertexID], 0.0, 1.0);
    output.color = colors[vertexID];
    return output;
}
)";

		static constexpr char ourPixelShader[] = R"(
float4 main(float4 position : SV_Position, float3 color : COLOR0) : SV_Target0
{
    return float4(color, 1.0);
}
)";

		RenderPipelineDesc desc{};
		desc.vertexShader.source = ourVertexShader;
		desc.vertexShader.entryPoint = "main";
		desc.vertexShader.profile = "vs_6_6";
		desc.fragmentShader.source = ourPixelShader;
		desc.fragmentShader.entryPoint = "main";
		desc.fragmentShader.profile = "ps_6_6";
		desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.depthFormat = DXGI_FORMAT_UNKNOWN;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return ctx.CreateRenderPipeline( desc );
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
		windowClass.lpszClassName = L"Ldx12TriangleWindow";
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		RegisterClassExW( &windowClass );

		constexpr uint32_t kInitialWidth = 1280;
		constexpr uint32_t kInitialHeight = 720;

		HWND hwnd = CreateWindowExW(
			0,
			windowClass.lpszClassName,
			L"Ldx12 Triangle",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			static_cast< int >(kInitialWidth),
			static_cast< int >(kInitialHeight),
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
		SetWindowLongPtr( hwnd, GWLP_USERDATA, reinterpret_cast< LONG_PTR >( &app ) );

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;
		contextDesc.swapchainBufferCount = 3;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( hwnd );
		swapchainDesc.width = kInitialWidth;
		swapchainDesc.height = kInitialHeight;
		swapchainDesc.vsync = true;

		app.deviceManager = &DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.trianglePipeline = CreateTrianglePipeline( *app.deviceManager->GetRenderDevice() );

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

			RenderDevice* ctx = app.deviceManager ? app.deviceManager->GetRenderDevice() : nullptr;
			if( !app.running || app.minimized || ctx == nullptr )
			{
				continue;
			}

			auto& buffer = ctx->AcquireCommandBuffer();
			const TextureHandle currentTexture = ctx->GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = currentTexture;

			buffer.CmdBeginRendering( renderPass, framebuffer );
			buffer.CmdBindRenderPipeline( app.trianglePipeline );
			buffer.CmdPushDebugGroupLabel( "Render Triangle", 0xff0000ff );
			buffer.CmdDraw( 3 );
			buffer.CmdPopDebugGroupLabel();
			buffer.CmdEndRendering();
			ctx->Submit( buffer, currentTexture );
		}

		SetWindowLongPtr( hwnd, GWLP_USERDATA, 0 );
		if( app.deviceManager )
		{
			app.deviceManager->WaitIdle();
		}
		app.trianglePipeline = {};
		DeviceManager::ShutdownSingleton();
		app.deviceManager = nullptr;
		if( IsWindow( hwnd ) != FALSE )
		{
			DestroyWindow( hwnd );
		}
		UnregisterClassW( windowClass.lpszClassName, instance );
		return 0;
	}
	catch( const std::exception& )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, "Ldx12 Triangle failed.", "Ldx12", MB_ICONERROR | MB_OK );
		return 1;
	}
}





