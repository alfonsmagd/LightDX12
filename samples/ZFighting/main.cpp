#include "App/imgui_impl_ldx12.h"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"

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
	constexpr std::array<float, 5> ourDepthOffsets = { 0.0f, 1e-6f, 1e-5f, 1e-4f, 1e-3f };
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

	struct GraphicsState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState singleSamplePipeline;
		RenderPipelineState multisamplePipeline;
		SceneTargets sceneTargets;
	};

	bool HandleImGuiMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam, void* )
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) != 0;
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

	void RecreateSceneTargets( GraphicsState& gfx )
	{
		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		const uint32_t width = gfx.deviceManager->GetWidth();
		const uint32_t height = gfx.deviceManager->GetHeight();
		if( gfx.sceneTargets.multisampleColor.Valid() && gfx.sceneTargets.multisampleDepth.Valid() && gfx.sceneTargets.singleSampleDepth.Valid() &&
			gfx.sceneTargets.width == width && gfx.sceneTargets.height == height )
		{
			return;
		}

		DestroySceneTargets( device, gfx.sceneTargets );

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
		gfx.sceneTargets.multisampleColor = device.CreateTexture( colorDesc );

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
		gfx.sceneTargets.multisampleDepth = device.CreateTexture( depthDesc );

		depthDesc.debugName = "Z-fighting single-sample depth";
		depthDesc.sampleCount = 1;
		gfx.sceneTargets.singleSampleDepth = device.CreateTexture( depthDesc );
		gfx.sceneTargets.width = width;
		gfx.sceneTargets.height = height;
	}

	PushConstants BuildConstants( float cameraTime, float aspectRatio, float nearPlane, float farPlane )
	{
		const XMVECTOR eye = XMVectorSet( std::sin( cameraTime ) * 0.8f, 0.25f, -6.5f + std::cos( cameraTime ) * 0.35f, 1.0f );
		const XMVECTOR target = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
		const XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
		const XMMATRIX view = XMMatrixLookAtLH( eye, target, up );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 48.0f ), aspectRatio, nearPlane, farPlane );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.viewProjection, view * projection );
		return constants;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	bool imguiWin32Initialized = false;
	bool imguiLdx12Initialized = false;

	try
	{
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12ZFightingWindow";
		appDesc.title = L"Ldx12 - Z-fighting depth offsets: 0, 1e-6, 1e-5, 1e-4, 1e-3";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		appDesc.messageHandler = HandleImGuiMessage;
		utils::AppLdx app( appDesc );
		GraphicsState gfx{};

		ContextDesc context{};
		context.enableDebugLayer = true;
		context.swapchainBufferCount = ourFramesInFlight;
		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;
		gfx.deviceManager = &DeviceManager::Initialize( context, swapchain );
		app.SetDeviceManager( *gfx.deviceManager );

		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		if( !device.SupportsSampleCount( context.swapchainFormat, ourSampleCount ) || !device.SupportsSampleCount( DXGI_FORMAT_D32_FLOAT, ourSampleCount ) )
		{
			throw std::runtime_error( "This GPU does not support the formats required for MSAA x4." );
		}
		gfx.singleSamplePipeline = CreatePipeline( device, 1 );
		gfx.multisamplePipeline = CreatePipeline( device, ourSampleCount );
		RecreateSceneTargets( gfx );

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		if( !ImGui_ImplWin32_Init( app.GetWindow() ) )
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
		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
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
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
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

			RecreateSceneTargets( gfx );
			const float elapsedTime = std::chrono::duration<float>( std::chrono::steady_clock::now() - startTime ).count();
			const float aspectRatio = static_cast<float>( gfx.deviceManager->GetWidth() ) / static_cast<float>( gfx.deviceManager->GetHeight() );
			PushConstants constants = BuildConstants( elapsedTime * 0.20f, aspectRatio, nearPlane, farPlane );

			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.025f, 0.03f, 0.04f, 1.0f };
			renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
			renderPass.depthStencil.clearDepth = 1.0f;
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = enableMsaa ? gfx.sceneTargets.multisampleColor : backbuffer;
			framebuffer.depthStencil.texture = enableMsaa ? gfx.sceneTargets.multisampleDepth : gfx.sceneTargets.singleSampleDepth;

			CommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdBeginRendering( renderPass, framebuffer );
			const RenderPipelineState& pipeline = enableMsaa ? gfx.multisamplePipeline : gfx.singleSamplePipeline;
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
				commands.CmdResolveTexture( gfx.sceneTargets.multisampleColor, backbuffer );
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

		device.WaitIdle();

		ImGui_ImplLdx12_Shutdown();
		imguiLdx12Initialized = false;
		ImGui_ImplWin32_Shutdown();
		imguiWin32Initialized = false;

		ImGui::DestroyContext();

		DestroySceneTargets( device, gfx.sceneTargets );

		gfx.singleSamplePipeline = {};
		gfx.multisamplePipeline = {};

		DeviceManager::ShutdownSingleton();
		gfx.deviceManager = nullptr;

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
		MessageBoxA( nullptr, error.what(), "Ldx12 Z-fighting failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
