#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/Geometry.hpp"
#include "Ldx12Utils/TextureLoader.hpp"

#include <DirectXMath.h>

#include <chrono>
#include <cstdint>
#include <exception>

using namespace DirectX;
using namespace ldx12;

namespace
{
	struct PushConstants
	{
		XMFLOAT4X4 mvp{};
		uint32_t textureIndex = 0;
		uint32_t samplerIndex = 0;
	};

	struct GraphicsState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState pipeline;
		TextureHandle texture{};
		utils::GeometryBuffers cube;
	};

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
		desc.inputElements[ 1 ].alignedByteOffset = sizeof( XMFLOAT3 ) * 2;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	PushConstants BuildPushConstants( RenderDevice& device, TextureHandle texture, float time, float aspectRatio )
	{
		const XMMATRIX model = XMMatrixRotationX( time * 0.55f ) * XMMatrixRotationY( time ) * XMMatrixTranslation( 0.0f, 0.0f, 4.0f );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 45.0f ), aspectRatio, 0.1f, 100.0f );

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
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12TexturedCubeWindow";
		appDesc.title = L"Ldx12 - Textured Cube";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		utils::AppLdx app( appDesc );

		GraphicsState gfx{};

		ContextDesc context{};
		context.enableDebugLayer = true;
		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;
		gfx.deviceManager = &DeviceManager::Initialize( context, swapchain );
		app.SetDeviceManager( *gfx.deviceManager );

		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		gfx.pipeline = CreatePipeline( device );
		gfx.texture = utils::CreateCheckerTexture( device, 0xffa0a0a0u, 0xff101010u );
		gfx.cube = utils::CreateCube( device );

		const std::chrono::steady_clock::time_point animationStart = std::chrono::steady_clock::now();
		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			const float time = std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();
			const float aspectRatio = static_cast<float>( gfx.deviceManager->GetWidth() ) / static_cast<float>( gfx.deviceManager->GetHeight() );
			const PushConstants constants = BuildPushConstants( device, gfx.texture, time, aspectRatio );
			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.12f, 0.12f, 0.14f, 1.0f };
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;

			ICommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdBindRenderPipeline( gfx.pipeline );
			commands.CmdBindVertexBuffer( gfx.cube.vertexBuffer );
			commands.CmdBindIndexBuffer( gfx.cube.indexBuffer );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDrawIndexed( gfx.cube.indexCount );
			commands.CmdEndRendering();
			device.Submit( commands, backbuffer );
		}

		device.WaitIdle();
		utils::DestroyGeometry( device, gfx.cube );
		device.Destroy( gfx.texture );
		gfx.pipeline = {};
		DeviceManager::ShutdownSingleton();
		gfx.deviceManager = nullptr;
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Textured Cube failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
