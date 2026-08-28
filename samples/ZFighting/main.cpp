#include "App/imgui_impl_ldx12.h"
#include "Ldx12/Ldx12.hpp"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"

#include <DirectXMath.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>

using namespace DirectX;
using namespace ldx12;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

namespace
{
	constexpr std::array<float, 5> ourDepthOffsets = {
		0.0f,
		1e-6f,
		1e-5f,
		1e-4f,
		1e-3f
	};
	constexpr uint32_t ourFramesInFlight = 3;
	constexpr uint32_t ourSampleCount = 4;

	struct PushConstants
	{
		XMFLOAT4X4 viewProjection{};
		XMFLOAT4 translation{};
		XMFLOAT4 color{};
	};

	struct SceneTargets
	{
		TextureHandle multisampleColor{};
		TextureHandle multisampleDepth{};
		TextureHandle singleSampleDepth{};
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct AppState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState singleSamplePipeline;
		RenderPipelineState multisamplePipeline;
		SceneTargets sceneTargets;
		bool running = true;
		bool minimized = false;
	};

	LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if( ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) )
		{
			return 1;
		}

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

	RenderPipelineState CreatePipeline( RenderDevice& device, uint32_t sampleCount )
	{
		static constexpr char ourVertexShader[] = R"(
cbuffer PushConstants : register(b0)
{
    row_major float4x4 viewProjection;
    float4 translation;
    float4 color;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    const float3 positions[6] =
    {
        float3(-0.75, -1.0, 0.0), float3( 0.75, -1.0, 0.0), float3(-0.75, 1.0, 0.0),
        float3(-0.75,  1.0, 0.0), float3( 0.75, -1.0, 0.0), float3( 0.75, 1.0, 0.0)
    };

    const float4 worldPosition = float4(positions[vertexId] + translation.xyz, 1.0);

    VertexOutput output;
    output.position = mul(worldPosition, viewProjection);
    output.color = color;
    return output;
}
)";

		static constexpr char ourPixelShader[] = R"(
float4 PSMain(float4 position : SV_Position, float4 color : COLOR0) : SV_Target0
{
    return color;
}
)";

		RenderPipelineDesc desc{};
		desc.vertexShader.source = ourVertexShader;
		desc.vertexShader.entryPoint = "VSMain";
		desc.vertexShader.profile = "vs_6_6";
		desc.vertexShader.sourceName = "ZFightingVS";
		desc.fragmentShader.source = ourPixelShader;
		desc.fragmentShader.entryPoint = "PSMain";
		desc.fragmentShader.profile = "ps_6_6";
		desc.fragmentShader.sourceName = "ZFightingPS";
		desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.sampleCount = sampleCount;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	void DestroySceneTargets( RenderDevice& device, SceneTargets& targets )
	{
		if( targets.multisampleColor.Valid() )
		{
			device.Destroy( targets.multisampleColor );
			targets.multisampleColor = {};
		}
		if( targets.multisampleDepth.Valid() )
		{
			device.Destroy( targets.multisampleDepth );
			targets.multisampleDepth = {};
		}
		if( targets.singleSampleDepth.Valid() )
		{
			device.Destroy( targets.singleSampleDepth );
			targets.singleSampleDepth = {};
		}
		targets.width = 0;
		targets.height = 0;
	}

	void RecreateSceneTargets( AppState& app )
	{
		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		const uint32_t width = app.deviceManager->GetWidth();
		const uint32_t height = app.deviceManager->GetHeight();
		if( app.sceneTargets.multisampleColor.Valid() &&
			app.sceneTargets.multisampleDepth.Valid() &&
			app.sceneTargets.singleSampleDepth.Valid() &&
			app.sceneTargets.width == width &&
			app.sceneTargets.height == height )
		{
			return;
		}

		DestroySceneTargets( device, app.sceneTargets );

		TextureDesc colorDesc{};
		colorDesc.debugName = "Z-fighting MSAA color";
		colorDesc.width = width;
		colorDesc.height = height;
		colorDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		colorDesc.sampleCount = ourSampleCount;
		colorDesc.usage = TextureUsage::RenderTarget;
		colorDesc.useClearValue = true;
		colorDesc.clearValue.Format = colorDesc.format;
		colorDesc.clearValue.Color[ 0 ] = 0.025f;
		colorDesc.clearValue.Color[ 1 ] = 0.03f;
		colorDesc.clearValue.Color[ 2 ] = 0.04f;
		colorDesc.clearValue.Color[ 3 ] = 1.0f;
		app.sceneTargets.multisampleColor = device.CreateTexture( colorDesc );

		TextureDesc depthDesc{};
		depthDesc.debugName = "Z-fighting MSAA depth";
		depthDesc.width = width;
		depthDesc.height = height;
		depthDesc.format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.sampleCount = ourSampleCount;
		depthDesc.usage = TextureUsage::DepthStencil;
		depthDesc.useClearValue = true;
		depthDesc.clearValue.Format = depthDesc.format;
		depthDesc.clearValue.DepthStencil.Depth = 1.0f;
		app.sceneTargets.multisampleDepth = device.CreateTexture( depthDesc );

		depthDesc.debugName = "Z-fighting single-sample depth";
		depthDesc.sampleCount = 1;
		app.sceneTargets.singleSampleDepth = device.CreateTexture( depthDesc );
		app.sceneTargets.width = width;
		app.sceneTargets.height = height;
	}

	PushConstants BuildConstants( float cameraTime, float aspectRatio, float nearPlane, float farPlane )
	{
		const XMVECTOR eye = XMVectorSet(
			std::sin( cameraTime ) * 0.8f,
			0.25f,
			-6.5f + std::cos( cameraTime ) * 0.35f,
			1.0f );
		const XMVECTOR target = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
		const XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
		const XMMATRIX view = XMMatrixLookAtLH( eye, target, up );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH(
			XMConvertToRadians( 48.0f ),
			aspectRatio,
			nearPlane,
			farPlane );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.viewProjection, view * projection );
		return constants;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	HWND window = nullptr;
	bool imguiWin32Initialized = false;
	bool imguiLdx12Initialized = false;

	try
	{
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof( WNDCLASSEXW );
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = L"Ldx12ZFightingWindow";
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		RegisterClassExW( &windowClass );

		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		window = CreateWindowExW(
			0,
			windowClass.lpszClassName,
			L"Ldx12 - Z-fighting depth offsets: 0, 1e-6, 1e-5, 1e-4, 1e-3",
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
			throw std::runtime_error( "Failed to create the Z-fighting window." );
		}

		AppState app{};
		SetWindowLongPtr( window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &app ) );
		ShowWindow( window, showCommand );
		UpdateWindow( window );

		ContextDesc context{};
		context.enableDebugLayer = true;
		context.swapchainBufferCount = ourFramesInFlight;
		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( window );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;
		app.deviceManager = &DeviceManager::Initialize( context, swapchain );

		RenderDevice& device = *app.deviceManager->GetRenderDevice();
		if( !device.SupportsSampleCount( context.swapchainFormat, ourSampleCount ) ||
			!device.SupportsSampleCount( DXGI_FORMAT_D32_FLOAT, ourSampleCount ) )
		{
			throw std::runtime_error( "This GPU does not support the formats required for MSAA x4." );
		}
		app.singleSamplePipeline = CreatePipeline( device, 1 );
		app.multisamplePipeline = CreatePipeline( device, ourSampleCount );
		RecreateSceneTargets( app );

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
		imguiInfo.renderTargetFormat = context.swapchainFormat;
		imguiInfo.depthFormat = DXGI_FORMAT_D32_FLOAT;
		if( !ImGui_ImplLdx12_Init( imguiInfo ) )
		{
			throw std::runtime_error( "ImGui Ldx12 initialization failed." );
		}
		imguiLdx12Initialized = true;

		const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
		std::array<SubmitHandle, ourFramesInFlight> frameSubmissions{};
		std::array<bool, ourDepthOffsets.size()> enabledOffsets = { true, true, true, true, true };
		bool enableMsaa = true;
		float nearPlane = 0.1f;
		float farPlane = 100.0f;
		uint32_t frameIndex = 0;
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

			device.Wait( frameSubmissions[ frameIndex ] );
			ImGui_ImplLdx12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			const ImVec2 controlPosition( viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + 12.0f );
			const ImVec2 controlSize( 340.0f, 295.0f );
			const ImGuiWindowFlags controlFlags =
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoSavedSettings;
			ImGui::SetNextWindowPos( controlPosition, ImGuiCond_Always );
			ImGui::SetNextWindowSize( controlSize, ImGuiCond_Always );
			ImGui::Begin( "Depth offsets", nullptr, controlFlags );
			ImGui::Checkbox( "Enable MSAA x4", &enableMsaa );
			ImGui::TextUnformatted( enableMsaa ? "Rendering with 4 samples" : "Rendering with 1 sample" );
			ImGui::Separator();
			ImGui::Checkbox( "0.0", &enabledOffsets[ 0 ] );
			ImGui::Checkbox( "1e-6", &enabledOffsets[ 1 ] );
			ImGui::Checkbox( "1e-5", &enabledOffsets[ 2 ] );
			ImGui::Checkbox( "1e-4", &enabledOffsets[ 3 ] );
			ImGui::Checkbox( "1e-3", &enabledOffsets[ 4 ] );
			ImGui::Separator();
			ImGui::SliderFloat( "Near plane", &nearPlane, 0.01f, 5.0f, "%.3f" );
			ImGui::SliderFloat( "Far plane", &farPlane, 10.0f, 500.0f, "%.1f" );
			ImGui::End();
			ImGui::Render();

			RecreateSceneTargets( app );
			const float elapsedTime = std::chrono::duration<float>(
				std::chrono::steady_clock::now() - startTime ).count();
			const float aspectRatio =
				static_cast<float>( app.deviceManager->GetWidth() ) /
				static_cast<float>( app.deviceManager->GetHeight() );
			PushConstants constants = BuildConstants(
				elapsedTime * 0.20f,
				aspectRatio,
				nearPlane,
				farPlane );

			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.025f, 0.03f, 0.04f, 1.0f };
			renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
			renderPass.depthStencil.clearDepth = 1.0f;
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = enableMsaa ? app.sceneTargets.multisampleColor : backbuffer;
			framebuffer.depthStencil.texture = enableMsaa ? app.sceneTargets.multisampleDepth : app.sceneTargets.singleSampleDepth;

			ICommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdBeginRendering( renderPass, framebuffer );
			const RenderPipelineState& pipeline = enableMsaa ? app.multisamplePipeline : app.singleSamplePipeline;
			commands.CmdBindRenderPipeline( pipeline );

			for( uint32_t index = 0; index < ourDepthOffsets.size(); ++index )
			{
				if( !enabledOffsets[ index ] )
				{
					continue;
				}

				const float x = ( static_cast<float>( index ) - 2.0f ) * 1.8f;
				constants.translation = { x, 0.0f, 0.0f, 0.0f };
				constants.color = { 0.95f, 0.15f, 0.10f, 1.0f };
				commands.CmdPushConstants( &constants, sizeof( constants ) );
				commands.CmdDraw( 6 );

				constants.translation = { x, 0.0f, -ourDepthOffsets[ index ], 0.0f };
				constants.color = { 0.10f, 0.55f, 1.0f, 1.0f };
				commands.CmdPushConstants( &constants, sizeof( constants ) );
				commands.CmdDraw( 6 );
			}

			commands.CmdEndRendering();
			if( enableMsaa )
			{
				commands.CmdResolveTexture( app.sceneTargets.multisampleColor, backbuffer );
			}

			RenderPass uiRenderPass{};
			Framebuffer uiFramebuffer{};
			uiFramebuffer.color[ 0 ].texture = backbuffer;

			commands.CmdBeginRendering( uiRenderPass, uiFramebuffer );
			ImGui_ImplLdx12_RenderDrawData( ImGui::GetDrawData(), commands );
			commands.CmdEndRendering();

			frameSubmissions[ frameIndex ] = device.Submit( commands, backbuffer );
			frameIndex = ( frameIndex + 1u ) % ourFramesInFlight;
		}

		SetWindowLongPtr( window, GWLP_USERDATA, 0 );
		device.WaitIdle();

		ImGui_ImplLdx12_Shutdown();
		imguiLdx12Initialized = false;
		ImGui_ImplWin32_Shutdown();
		imguiWin32Initialized = false;

		ImGui::DestroyContext();

		DestroySceneTargets( device, app.sceneTargets );

		app.singleSamplePipeline = {};
		app.multisamplePipeline = {};

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
		MessageBoxA( nullptr, error.what(), "Ldx12 Z-fighting failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
