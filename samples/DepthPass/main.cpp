#include "App/imgui_impl_ldx12.h"
#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/Geometry.hpp"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"

#include <DirectXMath.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

using namespace DirectX;
using namespace ldx12;

namespace
{
	constexpr uint32_t ourFramesInFlight = 3;

	struct PushConstants
	{
		XMFLOAT4X4 modelViewProjection = {};
		XMFLOAT4 color = {};
	};

	struct Cube
	{
		XMFLOAT3 position = {};
		float phase = 0.0f;
		XMFLOAT4 color = {};
	};

	const std::array<Cube, 3> ourCubes = { Cube{ { 1.2f, 0.0f, -3.0f }, 0.35f, { 1.0f, 0.2f, 0.15f, 1.0f } },
		Cube{ { -0.3f, 0.0f, -6.0f }, -0.45f, { 0.2f, 0.9f, 0.35f, 1.0f } },
		Cube{ { -2.4f, 0.0f, -10.0f }, 0.65f, { 0.2f, 0.45f, 1.0f, 1.0f } } };

	bool HandleImGuiMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam, void* )
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) != 0;
	}

	RenderPipelineState CreateDepthPipeline( RenderDevice& device )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/DepthPass.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/DepthPass.hlsl", "ps_6_6", "PSDepth" );
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.rasterizerState.FrontCounterClockwise = TRUE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;

		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateColorPipeline( RenderDevice& device )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/DepthPass.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/DepthPass.hlsl", "ps_6_6", "PSColor" );
		desc.color[ 0 ].format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.rasterizerState.FrontCounterClockwise = TRUE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
		desc.depthStencilState.StencilEnable = FALSE;

		return device.CreateRenderPipeline( desc );
	}

	TextureHandle CreateDepthTarget( RenderDevice& device, uint32_t width, uint32_t height )
	{
		TextureDesc desc{};
		desc.debugName = "Depth pass sampled depth target";
		desc.width = width;
		desc.height = height;
		desc.format = DXGI_FORMAT_D32_FLOAT;
		desc.usage = TextureUsage::DepthStencil | TextureUsage::Sampled;
		desc.useClearValue = true;
		desc.clearValue.Format = desc.format;
		desc.clearValue.DepthStencil.Depth = 1.0f;

		return device.CreateTexture( desc );
	}

	PushConstants BuildConstants( const Cube& cube, const XMMATRIX& viewProjection, float time )
	{
		const float y = cube.position.y + std::sin( time * 1.2f + cube.phase ) * 0.35f;
		const XMMATRIX model = XMMatrixRotationY( time * 0.65f + cube.phase ) * XMMatrixTranslation( cube.position.x, y, cube.position.z );
		PushConstants constants{};
		XMStoreFloat4x4( &constants.modelViewProjection, XMMatrixTranspose( model * viewProjection ) );
		constants.color = cube.color;
		return constants;
	}

	void DrawCubes( CommandBuffer& commands,
		const RenderPipelineState& pipeline,
		const utils::GeometryBuffers& cubeGeometry,
		const XMMATRIX& viewProjection,
		float time )
	{
		commands.CmdBindRenderPipeline( pipeline );
		commands.CmdBindVertexBuffer( cubeGeometry.vertexBuffer );
		commands.CmdBindIndexBuffer( cubeGeometry.indexBuffer );

		for( const Cube& cube : ourCubes )
		{
			const PushConstants constants = BuildConstants( cube, viewProjection, time );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDrawIndexed( cubeGeometry.indexCount );
		}
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
		appDesc.className = L"Ldx12DepthPassWindow";
		appDesc.title = L"Ldx12 - Depth pass + Color pass";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		appDesc.messageHandler = HandleImGuiMessage;

		utils::AppLdx app( appDesc );

		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc contextDesc{};
		contextDesc.swapchainBufferCount = ourFramesInFlight;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = initialWidth;
		swapchainDesc.height = initialHeight;
		swapchainDesc.vsync = true;

		DeviceManager& manager = DeviceManager::Initialize( contextDesc, swapchainDesc );

		app.SetDeviceManager( manager );
		RenderDevice& device = *manager.GetRenderDevice();

		RenderPipelineState depthPipeline = CreateDepthPipeline( device );
		RenderPipelineState colorPipeline = CreateColorPipeline( device );
		utils::GeometryBuffers cubeGeometry = utils::CreateCube( device );
		uint32_t depthWidth = manager.GetWidth();
		uint32_t depthHeight = manager.GetHeight();
		TextureHandle depthTarget = CreateDepthTarget( device, depthWidth, depthHeight );

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
		imguiInfo.renderTargetFormat = contextDesc.swapchainFormat;
		if( !ImGui_ImplLdx12_Init( imguiInfo ) )
		{
			throw std::runtime_error( "ImGui Ldx12 initialization failed." );
		}
		imguiLdx12Initialized = true;

		std::array<SubmitHandle, ourFramesInFlight> frameSubmissions{};
		uint32_t frameIndex = 0;
		const std::chrono::steady_clock::time_point animationStart = std::chrono::steady_clock::now();

		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			device.Wait( frameSubmissions[ frameIndex ] );
			const uint32_t width = manager.GetWidth();
			const uint32_t height = manager.GetHeight();

			if( width != depthWidth || height != depthHeight )
			{
				manager.WaitIdle();
				device.Destroy( depthTarget );
				depthTarget = CreateDepthTarget( device, width, height );
				depthWidth = width;
				depthHeight = height;
			}

			const float time = std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();
			const XMMATRIX projection =
				XMMatrixPerspectiveFovRH( XMConvertToRadians( 60.0f ), static_cast<float>( width ) / static_cast<float>( height ), 1.0f, 15.0f );
			const XMMATRIX viewProjection = XMMatrixIdentity() * projection;

			ImGui_ImplLdx12_NewFrame();
			ImGui_ImplWin32_NewFrame();
	

			CommandBuffer& commands = device.AcquireCommandBuffer();

			RenderPass depthPass{};
			depthPass.depthStencil.depthLoadOp = LoadOp::Clear;
			depthPass.depthStencil.clearDepth = 1.0f;
			Framebuffer depthFramebuffer{};
			depthFramebuffer.depthStencil.texture = depthTarget;

			//Depth pass
			{
				commands.CmdBeginRendering( depthPass, depthFramebuffer );
				DrawCubes( commands, depthPipeline, cubeGeometry, viewProjection, time );
				commands.CmdEndRendering();
			}

			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
			RenderPass colorPass{};
			colorPass.color[ 0 ].loadOp = LoadOp::Clear;
			colorPass.color[ 0 ].clearColor = { 0.025f, 0.03f, 0.045f, 1.0f };
			colorPass.depthStencil.depthLoadOp = LoadOp::Load;
			Framebuffer colorFramebuffer{};
			colorFramebuffer.color[ 0 ].texture = backbuffer;
			colorFramebuffer.depthStencil.texture = depthTarget;

			{ //Color pass
				commands.CmdBeginRendering( colorPass, colorFramebuffer );
				DrawCubes( commands, colorPipeline, cubeGeometry, viewProjection, time );
				commands.CmdEndRendering();
			}

			commands.CmdTransitionTexture( depthTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

			RenderPass uiPass{};
			uiPass.color[ 0 ].loadOp = LoadOp::Load;
			Framebuffer uiFramebuffer{};
			uiFramebuffer.color[ 0 ].texture = backbuffer;

			{ //UI pass
				commands.CmdBeginRendering( uiPass, uiFramebuffer );
				ImGui::NewFrame();
				ImGui::SetNextWindowPos( ImVec2( 8.0f, 8.0f ), ImGuiCond_Always );
				ImGui::SetNextWindowSize( ImVec2( 460.0f, 300.0f ), ImGuiCond_Always );
				ImGui::Begin( "Depth buffer", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse );
				ImGui::TextUnformatted( "Near is darker, far is lighter" );
				ImGui::Image( depthTarget, ImVec2( 420.0f, 420.0f * static_cast<float>( height ) / static_cast<float>( width ) ) );
				ImGui::End();
				ImGui::Render();
				ImGui_ImplLdx12_RenderDrawData( ImGui::GetDrawData(), commands );
				commands.CmdEndRendering();
			}

			frameSubmissions[ frameIndex ] = device.Submit( commands, backbuffer );
			frameIndex = ( frameIndex + 1u ) % ourFramesInFlight;
		}

		manager.WaitIdle();

		ImGui_ImplLdx12_Shutdown();
		imguiLdx12Initialized = false;
		ImGui_ImplWin32_Shutdown();
		imguiWin32Initialized = false;
		ImGui::DestroyContext();

		device.Destroy( depthTarget );

		utils::DestroyGeometry( device, cubeGeometry );
		colorPipeline = {};
		depthPipeline = {};
		DeviceManager::ShutdownSingleton();
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
		MessageBoxA( nullptr, error.what(), "Ldx12 DepthPass failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
