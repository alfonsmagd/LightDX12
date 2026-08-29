#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"

#include <exception>

using namespace ldx12;

namespace
{
	struct GraphicsState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState trianglePipeline;
	};

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
		constexpr uint32_t kInitialWidth = 1280;
		constexpr uint32_t kInitialHeight = 720;

		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12TriangleWindow";
		appDesc.title = L"Ldx12 Triangle";
		appDesc.width = kInitialWidth;
		appDesc.height = kInitialHeight;

		utils::AppLdx app( appDesc );

		GraphicsState gfx{};

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;
		contextDesc.swapchainBufferCount = 3;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = kInitialWidth;
		swapchainDesc.height = kInitialHeight;
		swapchainDesc.vsync = true;

		gfx.deviceManager = &DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.SetDeviceManager( *gfx.deviceManager );
		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		gfx.trianglePipeline = CreateTrianglePipeline( device );

		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			ICommandBuffer& buffer = device.AcquireCommandBuffer();
			const TextureHandle currentTexture = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = currentTexture;

			{
				buffer.CmdBeginRendering( renderPass, framebuffer );
				buffer.CmdBindRenderPipeline( gfx.trianglePipeline );
				buffer.CmdPushDebugGroupLabel( "Render Triangle", 0xff0000ff );
				buffer.CmdDraw( 3 );
				buffer.CmdPopDebugGroupLabel();
				buffer.CmdEndRendering();
			}
			device.Submit( buffer, currentTexture );
		}

		gfx.deviceManager->WaitIdle();
		gfx.trianglePipeline = {};
		DeviceManager::ShutdownSingleton();
		gfx.deviceManager = nullptr;
		return 0;
	}
	catch( const std::exception& )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, "Ldx12 Triangle failed.", "Ldx12", MB_ICONERROR | MB_OK );
		return 1;
	}
}





