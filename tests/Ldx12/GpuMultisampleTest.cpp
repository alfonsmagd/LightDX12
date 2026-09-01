#include "TestTemplate.hpp"
#include "Ldx12/Ldx12Native.hpp"

#include <array>

namespace ldx12::tests
{
	void TestGpuMultisampleRendering()
	{
		constexpr uint32_t width = 16;
		constexpr uint32_t height = 16;
		constexpr uint32_t sampleCount = 4;

		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();
		D3D12Native native = device.GetNative();

		Require( device.SupportsSampleCount( DXGI_FORMAT_R8G8B8A8_UNORM, sampleCount ), "RGBA8 does not support the required MSAA x4 test configuration." );
		Require( device.SupportsSampleCount( DXGI_FORMAT_D32_FLOAT, sampleCount ), "D32 does not support the required MSAA x4 test configuration." );

		TextureDesc colorDesc{};
		colorDesc.debugName = "Ldx12Tests MSAA color";
		colorDesc.width = width;
		colorDesc.height = height;
		colorDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		colorDesc.sampleCount = sampleCount;
		colorDesc.usage = TextureUsage::RenderTarget;
		const TextureHandle multisampleColor = device.CreateTexture( colorDesc );

		TextureDesc depthDesc{};
		depthDesc.debugName = "Ldx12Tests MSAA depth";
		depthDesc.width = width;
		depthDesc.height = height;
		depthDesc.format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.sampleCount = sampleCount;
		depthDesc.usage = TextureUsage::DepthStencil;
		const TextureHandle multisampleDepth = device.CreateTexture( depthDesc );

		TextureDesc resolveDesc{};
		resolveDesc.debugName = "Ldx12Tests MSAA resolve";
		resolveDesc.width = width;
		resolveDesc.height = height;
		resolveDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		const TextureHandle resolvedColor = device.CreateTexture( resolveDesc );

		Require( native.GetResource( multisampleColor )->GetDesc().SampleDesc.Count == sampleCount, "The color texture was not created with four samples." );
		Require( native.GetResource( multisampleDepth )->GetDesc().SampleDesc.Count == sampleCount, "The depth texture was not created with four samples." );
		Require( native.GetResource( resolvedColor )->GetDesc().SampleDesc.Count == 1, "The resolve texture must contain one sample per pixel." );

		static constexpr char vertexShader[] = R"(
float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    const float2 positions[3] =
    {
        float2(-1.0, -1.0),
        float2( 0.0,  1.0),
        float2( 1.0, -1.0)
    };
    return float4(positions[vertexId], 0.0, 1.0);
}
)";

		static constexpr char pixelShader[] = R"(
float4 PSMain() : SV_Target0
{
    return float4(1.0, 1.0, 1.0, 1.0);
}
)";

		RenderPipelineDesc pipelineDesc{};
		pipelineDesc.vertexShader.source = vertexShader;
		pipelineDesc.vertexShader.entryPoint = "VSMain";
		pipelineDesc.fragmentShader.source = pixelShader;
		pipelineDesc.fragmentShader.entryPoint = "PSMain";
		pipelineDesc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		pipelineDesc.sampleCount = sampleCount;
		RenderPipelineState pipeline = device.CreateRenderPipeline( pipelineDesc );
		Require( pipeline.Valid(), "The MSAA x4 graphics pipeline was not created." );

		RenderPass renderPass{};
		renderPass.color[ 0 ].loadOp = LoadOp::Clear;
		renderPass.color[ 0 ].clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = multisampleColor;
		framebuffer.depthStencil.texture = multisampleDepth;

		CommandBuffer& commands = device.AcquireCommandBuffer();
		commands.CmdBeginRendering( renderPass, framebuffer );
		commands.CmdBindRenderPipeline( pipeline );
		commands.CmdEndRendering();
		commands.CmdResolveTexture( multisampleColor, resolvedColor );
		const SubmitHandle submission = device.Submit( commands );
		device.Wait( submission );

		constexpr uint32_t rowPitch = width * 4u;
		std::array<uint8_t, rowPitch * height> pixels{};
		device.DownloadTexture2D( resolvedColor, pixels.data(), rowPitch, static_cast<uint32_t>( pixels.size() ) );

		for( uint32_t offset = 0; offset < pixels.size(); offset += 4u )
		{
			Require( pixels[ offset ] == 255u && pixels[ offset + 1u ] == 0u && pixels[ offset + 2u ] == 0u && pixels[ offset + 3u ] == 255u,
				"Resolving the MSAA render target produced unexpected pixels." );
		}

		device.Destroy( resolvedColor );
		device.Destroy( multisampleDepth );
		device.Destroy( multisampleColor );
		device.WaitIdle();
	}
}
