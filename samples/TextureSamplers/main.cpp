#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/TextureLoader.hpp"

#include <cstdint>
#include <exception>

using namespace ldx12;

namespace
{
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

	struct GraphicsState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState pipeline;
		TextureHandle checkerTexture{};
		SamplerHandle pointMirrorOnceSampler{};
		SamplerHandle redBorderSampler{};
	};

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

	void RenderFrame( GraphicsState& gfx )
	{
		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		const uint32_t width = gfx.deviceManager->GetWidth();
		const uint32_t height = gfx.deviceManager->GetHeight();
		const TextureHandle backBuffer = device.GetCurrentSwapchainTexture();

		// The same texture is deliberately paired with six different sampler slots.
		const TextureSample samples[ ourDrawCount ] = {
			TextureSample{ gfx.checkerTexture, ToSamplerIndex( SamplerSlot::LinearClamp ) },
			TextureSample{ gfx.checkerTexture, ToSamplerIndex( SamplerSlot::LinearWrap ) },
			TextureSample{ gfx.checkerTexture, ToSamplerIndex( SamplerSlot::PointClamp ) },
			TextureSample{ gfx.checkerTexture, ToSamplerIndex( SamplerSlot::ShadowComparison ) },
			TextureSample{ gfx.checkerTexture, device.GetSamplerIndex( gfx.pointMirrorOnceSampler ) },
			TextureSample{ gfx.checkerTexture, device.GetSamplerIndex( gfx.redBorderSampler ) }
		};

		RenderPass renderPass{};
		renderPass.color[ 0 ].loadOp = LoadOp::Clear;
		renderPass.color[ 0 ].clearColor = { 0.025f, 0.03f, 0.045f, 1.0f };
		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = backBuffer;

		ICommandBuffer& commands = device.AcquireCommandBuffer();
		commands.CmdBeginRendering( renderPass, framebuffer );
		commands.CmdBindRenderPipeline( gfx.pipeline );

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
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;

		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12TextureSamplersWindow";
		appDesc.title = L"Ldx12 - One texture, six samplers";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;

		utils::AppLdx app( appDesc );

		GraphicsState gfx{};

		ContextDesc context{};
		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;
		gfx.deviceManager = &DeviceManager::Initialize( context, swapchain );
		app.SetDeviceManager( *gfx.deviceManager );

		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		gfx.pipeline = CreateTexturePipeline( device );
		gfx.checkerTexture = utils::CreateCheckerTexture( device, 0xffffffffu, 0xff101010u );

		SamplerDesc pointMirrorOnceDesc{};
		pointMirrorOnceDesc.filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		pointMirrorOnceDesc.addressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		pointMirrorOnceDesc.addressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		pointMirrorOnceDesc.addressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
		gfx.pointMirrorOnceSampler = device.CreateSampler( pointMirrorOnceDesc );

		SamplerDesc redBorderDesc{};
		redBorderDesc.addressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		redBorderDesc.addressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		redBorderDesc.addressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		redBorderDesc.borderColor = { 0.85f, 0.05f, 0.05f, 1.0f };
		gfx.redBorderSampler = device.CreateSampler( redBorderDesc );

		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}
			RenderFrame( gfx );
		}

		device.Destroy( gfx.pointMirrorOnceSampler );
		device.Destroy( gfx.redBorderSampler );
		device.Destroy( gfx.checkerTexture );
		gfx.pipeline = {};
		DeviceManager::ShutdownSingleton();
		gfx.deviceManager = nullptr;
		return 0;
	}
	catch( const std::exception& exception )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, exception.what(), "Ldx12 TextureSamplers", MB_ICONERROR | MB_OK );
		return 1;
	}
}
