#include "Ldx12/Ldx12.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <stdexcept>

using namespace ldx12;

namespace
{
	constexpr uint32_t ourTextureWidth = 64;
	constexpr uint32_t ourTextureHeight = 64;
	constexpr uint32_t ourCheckerSize = 8;
	constexpr uint32_t ourDrawCount = 6;

	struct DrawConstants
	{
		uint32_t textureIndex = 0;
		uint32_t samplerIndex = 0;
	};

	struct TextureSample
	{
		TextureHandle texture{};
		uint32_t samplerIndex = 0;
	};

	struct AppState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState pipeline;
		TextureHandle checkerTexture{};
		SamplerHandle pointMirrorOnceSampler{};
		SamplerHandle redBorderSampler{};
		bool running = true;
		bool minimized = false;
	};

	LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		AppState* app = reinterpret_cast<AppState*>( GetWindowLongPtr( window, GWLP_USERDATA ) );
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
				return DefWindowProc( window, message, wParam, lParam );
		}
	}

	RenderPipelineState CreateTexturePipeline( RenderDevice& device )
	{
		static constexpr char ourVertexShader[] = R"(
struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    const float2 positions[6] =
    {
        float2(-0.92,  0.88), float2( 0.92,  0.88), float2(-0.92, -0.88),
        float2(-0.92, -0.88), float2( 0.92,  0.88), float2( 0.92, -0.88)
    };
    const float2 uvs[6] =
    {
        float2(-0.35, -0.35), float2(2.35, -0.35), float2(-0.35, 2.35),
        float2(-0.35,  2.35), float2(2.35, -0.35), float2( 2.35, 2.35)
    };

    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = uvs[vertexId];
    return output;
}
)";

		static constexpr char ourPixelShader[] = R"(
cbuffer DrawConstants : register(b0)
{
    uint gTextureIndex;
    uint gSamplerIndex;
};

struct PixelInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 PSMain(PixelInput input) : SV_Target0
{
    Texture2D<float4> textureResource = ResourceDescriptorHeap[gTextureIndex];
    if (gSamplerIndex == 3u)
    {
        SamplerComparisonState shadowSampler = SamplerDescriptorHeap[gSamplerIndex];
        const float comparison = textureResource.SampleCmpLevelZero(shadowSampler, input.uv, 0.5);
        return float4(comparison.xxx, 1.0);
    }

    SamplerState sampler = SamplerDescriptorHeap[gSamplerIndex];
    return textureResource.Sample(sampler, input.uv);
}
)";

		RenderPipelineDesc desc{};
		desc.vertexShader.source = ourVertexShader;
		desc.vertexShader.entryPoint = "VSMain";
		desc.vertexShader.profile = "vs_6_6";
		desc.vertexShader.sourceName = "TextureSamplersVS";
		desc.fragmentShader.source = ourPixelShader;
		desc.fragmentShader.entryPoint = "PSMain";
		desc.fragmentShader.profile = "ps_6_6";
		desc.fragmentShader.sourceName = "TextureSamplersPS";
		desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.depthFormat = DXGI_FORMAT_UNKNOWN;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	TextureHandle CreateCheckerTexture( RenderDevice& device )
	{
		std::array<uint32_t, ourTextureWidth * ourTextureHeight> pixels{};
		for( uint32_t y = 0; y < ourTextureHeight; ++y )
		{
			for( uint32_t x = 0; x < ourTextureWidth; ++x )
			{
				const bool white = ( ( x / ourCheckerSize ) + ( y / ourCheckerSize ) ) % 2u == 0u;
				pixels[ y * ourTextureWidth + x ] = white ? 0xffffffffu : 0xff101010u;
			}
		}

		TextureDesc desc{};
		desc.debugName = "64x64 checker texture";
		desc.width = ourTextureWidth;
		desc.height = ourTextureHeight;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.usage = TextureUsage::Sampled;
		desc.data = pixels.data();
		desc.rowPitch = ourTextureWidth * sizeof( uint32_t );
		desc.slicePitch = desc.rowPitch * ourTextureHeight;
		return device.CreateTexture( desc );
	}

	void RenderFrame( AppState& app )
	{
		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		const uint32_t width = app.deviceManager->GetWidth();
		const uint32_t height = app.deviceManager->GetHeight();
		const TextureHandle backBuffer = device.GetCurrentSwapchainTexture();

		// The same texture is deliberately paired with six different sampler slots.
		const TextureSample samples[ ourDrawCount ] = {
			TextureSample{ app.checkerTexture, ToSamplerIndex( SamplerSlot::LinearClamp ) },
			TextureSample{ app.checkerTexture, ToSamplerIndex( SamplerSlot::LinearWrap ) },
			TextureSample{ app.checkerTexture, ToSamplerIndex( SamplerSlot::PointClamp ) },
			TextureSample{ app.checkerTexture, ToSamplerIndex( SamplerSlot::ShadowComparison ) },
			TextureSample{ app.checkerTexture, device.GetSamplerIndex( app.pointMirrorOnceSampler ) },
			TextureSample{ app.checkerTexture, device.GetSamplerIndex( app.redBorderSampler ) }
		};

		RenderPass renderPass{};
		renderPass.color[ 0 ].loadOp = LoadOp::Clear;
		renderPass.color[ 0 ].clearColor = { 0.025f, 0.03f, 0.045f, 1.0f };
		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = backBuffer;

		ICommandBuffer& commands = device.AcquireCommandBuffer();
		commands.CmdBeginRendering( renderPass, framebuffer );
		commands.CmdBindRenderPipeline( app.pipeline );

		const int32_t margin = 18;
		const int32_t gap = 12;
		const int32_t columnCount = 3;
		const int32_t rowCount = 2;
		const int32_t panelWidth = ( static_cast<int32_t>( width ) - margin * 2 - gap * ( columnCount - 1 ) ) / columnCount;
		const int32_t panelHeight = ( static_cast<int32_t>( height ) - margin * 2 - gap * ( rowCount - 1 ) ) / rowCount;

		if( panelWidth > 0 && panelHeight > 0 )
		{
			for( uint32_t index = 0; index < ourDrawCount; ++index )
			{
				const int32_t column = static_cast<int32_t>( index % static_cast<uint32_t>( columnCount ) );
				const int32_t row = static_cast<int32_t>( index / static_cast<uint32_t>( columnCount ) );
				const int32_t left = margin + column * ( panelWidth + gap );
				const int32_t top = margin + row * ( panelHeight + gap );
				const int32_t right = left + panelWidth;
				const int32_t bottom = top + panelHeight;

				const DrawConstants constants = {
					device.GetBindlessIndex( samples[ index ].texture ),
					samples[ index ].samplerIndex
				};
				commands.CmdSetViewport(
					static_cast<float>( left ),
					static_cast<float>( top ),
					static_cast<float>( panelWidth ),
					static_cast<float>( panelHeight ) );
				commands.CmdSetScissor( left, top, right, bottom );
				commands.CmdPushConstants( &constants, sizeof( constants ) );
				commands.CmdDraw( 6 );
			}
		}

		commands.CmdEndRendering();
		device.Submit( commands, backBuffer );
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
		windowClass.lpszClassName = L"Ldx12TextureSamplersWindow";
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		if( RegisterClassExW( &windowClass ) == 0 )
		{
			throw std::runtime_error( "Failed to register the Win32 window class." );
		}

		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		HWND window = CreateWindowExW(
			0,
			windowClass.lpszClassName,
			L"Ldx12 - One texture, six samplers",
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
			throw std::runtime_error( "Failed to create the Win32 window." );
		}

		AppState app{};
		SetWindowLongPtr( window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &app ) );
		ShowWindow( window, showCommand );
		UpdateWindow( window );

		ContextDesc context{};
		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( window );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;
		app.deviceManager = &DeviceManager::Initialize( context, swapchain );

		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		app.pipeline = CreateTexturePipeline( device );
		app.checkerTexture = CreateCheckerTexture( device );

		SamplerDesc pointMirrorOnceDesc{};
		pointMirrorOnceDesc.filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		pointMirrorOnceDesc.addressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		pointMirrorOnceDesc.addressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		pointMirrorOnceDesc.addressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		app.pointMirrorOnceSampler = device.CreateSampler( pointMirrorOnceDesc );

		SamplerDesc redBorderDesc{};
		redBorderDesc.addressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		redBorderDesc.addressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		redBorderDesc.addressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		redBorderDesc.borderColor = { 0.85f, 0.05f, 0.05f, 1.0f };
		app.redBorderSampler = device.CreateSampler( redBorderDesc );

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

		SetWindowLongPtr( window, GWLP_USERDATA, 0 );
		device.Destroy( app.pointMirrorOnceSampler );
		device.Destroy( app.redBorderSampler );
		device.Destroy( app.checkerTexture );
		app.pipeline = {};
		DeviceManager::ShutdownSingleton();
		app.deviceManager = nullptr;
		if( IsWindow( window ) != FALSE )
		{
			DestroyWindow( window );
		}
		UnregisterClassW( windowClass.lpszClassName, instance );
		return 0;
	}
	catch( const std::exception& exception )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, exception.what(), "Ldx12 TextureSamplers", MB_ICONERROR | MB_OK );
		return 1;
	}
}
