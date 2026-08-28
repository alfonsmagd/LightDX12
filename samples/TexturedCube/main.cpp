#include "Ldx12/Ldx12.hpp"

#include <DirectXMath.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>

using namespace DirectX;
using namespace ldx12;

namespace
{
	constexpr uint32_t ourTextureSize = 64;
	constexpr uint32_t ourCheckerSize = 8;

	struct PushConstants
	{
		XMFLOAT4X4 mvp{};
		uint32_t textureIndex = 0;
		uint32_t samplerIndex = 0;
	};

	struct Vertex
	{
		XMFLOAT3 position{};
		XMFLOAT2 uv{};
	};

	struct AppState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState pipeline;
		TextureHandle texture{};
		BufferHandle vertexBuffer{};
		BufferHandle indexBuffer{};
		bool running = true;
		bool minimized = false;
	};

	const std::array<Vertex, 24> ourVertices = {
		Vertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } },
		Vertex{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f } },
		Vertex{ {  1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f } },
		Vertex{ {  1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } },
		Vertex{ {  1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f } },
		Vertex{ {  1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f } },
		Vertex{ { -1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f } },
		Vertex{ { -1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f } },
		Vertex{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f } },
		Vertex{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f } },
		Vertex{ { -1.0f,  1.0f, -1.0f }, { 1.0f, 0.0f } },
		Vertex{ { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f } },
		Vertex{ {  1.0f, -1.0f, -1.0f }, { 0.0f, 1.0f } },
		Vertex{ {  1.0f,  1.0f, -1.0f }, { 0.0f, 0.0f } },
		Vertex{ {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f } },
		Vertex{ {  1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f } },
		Vertex{ { -1.0f,  1.0f, -1.0f }, { 0.0f, 1.0f } },
		Vertex{ { -1.0f,  1.0f,  1.0f }, { 0.0f, 0.0f } },
		Vertex{ {  1.0f,  1.0f,  1.0f }, { 1.0f, 0.0f } },
		Vertex{ {  1.0f,  1.0f, -1.0f }, { 1.0f, 1.0f } },
		Vertex{ { -1.0f, -1.0f,  1.0f }, { 0.0f, 1.0f } },
		Vertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f } },
		Vertex{ {  1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f } },
		Vertex{ {  1.0f, -1.0f,  1.0f }, { 1.0f, 1.0f } }
	};

	constexpr std::array<uint32_t, 36> ourIndices = {
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11,
		12, 13, 14, 12, 14, 15,
		16, 17, 18, 16, 18, 19,
		20, 21, 22, 20, 22, 23
	};

	LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		AppState* app = reinterpret_cast<AppState*>( GetWindowLongPtr( window, GWLP_USERDATA ) );
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
				return DefWindowProc( window, message, wParam, lParam );
		}
	}

	RenderPipelineState CreatePipeline( RenderDevice& device )
	{
		static constexpr char ourVertexShader[] = R"(
cbuffer PushConstants : register(b0)
{
    float4x4 mvp;
    uint textureIndex;
    uint samplerIndex;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(mvp, float4(input.position, 1.0));
    output.uv = input.uv;
    return output;
}
)";

		static constexpr char ourPixelShader[] = R"(
cbuffer PushConstants : register(b0)
{
    float4x4 mvp;
    uint textureIndex;
    uint samplerIndex;
};

float4 PSMain(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    Texture2D<float4> textureResource = ResourceDescriptorHeap[textureIndex];
    SamplerState textureSampler = SamplerDescriptorHeap[samplerIndex];
    return textureResource.Sample(textureSampler, uv);
}
)";

		RenderPipelineDesc desc{};
		desc.vertexShader.source = ourVertexShader;
		desc.vertexShader.entryPoint = "VSMain";
		desc.vertexShader.profile = "vs_6_6";
		desc.vertexShader.sourceName = "TexturedCubeVS";
		desc.fragmentShader.source = ourPixelShader;
		desc.fragmentShader.entryPoint = "PSMain";
		desc.fragmentShader.profile = "ps_6_6";
		desc.fragmentShader.sourceName = "TexturedCubePS";
		desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.depthFormat = DXGI_FORMAT_UNKNOWN;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 0 ].alignedByteOffset = 0;
		desc.inputElements[ 1 ].semanticName = "TEXCOORD";
		desc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32_FLOAT;
		desc.inputElements[ 1 ].alignedByteOffset = sizeof( XMFLOAT3 );
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	TextureHandle CreateCheckerTexture( RenderDevice& device )
	{
		std::array<uint32_t, ourTextureSize * ourTextureSize> pixels{};
		for( uint32_t y = 0; y < ourTextureSize; ++y )
		{
			for( uint32_t x = 0; x < ourTextureSize; ++x )
			{
				const bool gray = ( ( x / ourCheckerSize ) + ( y / ourCheckerSize ) ) % 2u == 0u;
				pixels[ y * ourTextureSize + x ] = gray ? 0xffa0a0a0u : 0xff101010u;
			}
		}

		TextureDesc desc{};
		desc.debugName = "Textured cube checker";
		desc.width = ourTextureSize;
		desc.height = ourTextureSize;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.usage = TextureUsage::Sampled;
		desc.data = pixels.data();
		desc.rowPitch = ourTextureSize * sizeof( uint32_t );
		desc.slicePitch = desc.rowPitch * ourTextureSize;
		return device.CreateTexture( desc );
	}

	void CreateCubeBuffers( AppState& app, RenderDevice& device )
	{
		BufferDesc vertexDesc{};
		vertexDesc.debugName = "Textured cube vertices";
		vertexDesc.size = sizeof( ourVertices );
		vertexDesc.stride = sizeof( Vertex );
		vertexDesc.type = BufferType::Vertex;
		vertexDesc.initialData = ourVertices.data();
		app.vertexBuffer = device.CreateBuffer( vertexDesc );

		BufferDesc indexDesc{};
		indexDesc.debugName = "Textured cube indices";
		indexDesc.size = sizeof( ourIndices );
		indexDesc.stride = sizeof( uint32_t );
		indexDesc.type = BufferType::Index;
		indexDesc.initialData = ourIndices.data();
		app.indexBuffer = device.CreateBuffer( indexDesc );
	}

	PushConstants BuildPushConstants( RenderDevice& device, TextureHandle texture, float time, float aspectRatio )
	{
		const XMMATRIX model =
			XMMatrixRotationX( time * 0.55f ) *
			XMMatrixRotationY( time ) *
			XMMatrixTranslation( 0.0f, 0.0f, 4.0f );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH(
			XMConvertToRadians( 45.0f ),
			aspectRatio,
			0.1f,
			100.0f );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.mvp, XMMatrixTranspose( model * projection ) );
		constants.textureIndex = device.GetBindlessIndex( texture );
		constants.samplerIndex = ToSamplerIndex( SamplerSlot::LinearWrap );
		return constants;
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
		windowClass.lpszClassName = L"Ldx12TexturedCubeWindow";
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		RegisterClassExW( &windowClass );

		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		HWND window = CreateWindowExW(
			0,
			windowClass.lpszClassName,
			L"Ldx12 - Textured Cube",
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
			throw std::runtime_error( "Failed to create the Textured Cube window." );
		}

		AppState app{};
		SetWindowLongPtr( window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &app ) );
		ShowWindow( window, showCommand );
		UpdateWindow( window );

		ContextDesc context{};
		context.enableDebugLayer = true;
		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( window );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;
		app.deviceManager = &DeviceManager::Initialize( context, swapchain );

		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		app.pipeline = CreatePipeline( device );
		app.texture = CreateCheckerTexture( device );
		CreateCubeBuffers( app, device );

		const std::chrono::steady_clock::time_point animationStart = std::chrono::steady_clock::now();
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

			const float time = std::chrono::duration<float>(
				std::chrono::steady_clock::now() - animationStart ).count();
			const float aspectRatio =
				static_cast<float>( app.deviceManager->GetWidth() ) /
				static_cast<float>( app.deviceManager->GetHeight() );
			const PushConstants constants = BuildPushConstants( device, app.texture, time, aspectRatio );
			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.12f, 0.12f, 0.14f, 1.0f };
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;

			ICommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdBindRenderPipeline( app.pipeline );
			commands.CmdBindVertexBuffer( app.vertexBuffer );
			commands.CmdBindIndexBuffer( app.indexBuffer );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDrawIndexed( static_cast<uint32_t>( ourIndices.size() ) );
			commands.CmdEndRendering();
			device.Submit( commands, backbuffer );
		}

		SetWindowLongPtr( window, GWLP_USERDATA, 0 );
		device.WaitIdle();
		device.Destroy( app.indexBuffer );
		device.Destroy( app.vertexBuffer );
		device.Destroy( app.texture );
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
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Textured Cube failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
