#include "TestTemplate.hpp"

#include <array>

namespace ldx12::tests
{
	void TestGpuTextureArrayAndCubeViews()
	{
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		const std::array<uint32_t, 2> arrayPixels = {
			0xff0000ffu,
			0xff00ff00u
		};
		TextureDesc arrayDesc{};
		arrayDesc.debugName = "Ldx12Tests shader Texture2DArray";
		arrayDesc.width = 1;
		arrayDesc.height = 1;
		arrayDesc.depthOrArraySize = 2;
		arrayDesc.dimension = TextureDimension::Texture2DArray;
		arrayDesc.data = arrayPixels.data();
		arrayDesc.rowPitch = sizeof( uint32_t );
		arrayDesc.slicePitch = sizeof( uint32_t );
		const TextureHandle arrayTexture = device.CreateTexture( arrayDesc );

		const std::array<uint32_t, ourCubeMapFaceCount> cubePixels = {
			0xff0000ffu,
			0xff00ffffu,
			0xffff00ffu,
			0xffffff00u,
			0xffff0000u,
			0xff00ff00u
		};
		TextureDesc cubeDesc{};
		cubeDesc.debugName = "Ldx12Tests shader TextureCube";
		cubeDesc.width = 1;
		cubeDesc.height = 1;
		cubeDesc.depthOrArraySize = ourCubeMapFaceCount;
		cubeDesc.dimension = TextureDimension::TextureCube;
		cubeDesc.data = cubePixels.data();
		cubeDesc.rowPitch = sizeof( uint32_t );
		cubeDesc.slicePitch = sizeof( uint32_t );
		const TextureHandle cubeTexture = device.CreateTexture( cubeDesc );

		TextureDesc targetDesc{};
		targetDesc.debugName = "Ldx12Tests texture view target";
		targetDesc.width = 2;
		targetDesc.height = 1;
		targetDesc.usage = TextureUsage::RenderTarget;
		const TextureHandle target = device.CreateTexture( targetDesc );

		static constexpr char vertexShader[] = R"(
float4 VSMain(uint vertexId : SV_VertexID) : SV_Position
{
    const float2 positions[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0,  3.0),
        float2( 3.0, -1.0)
    };
    return float4(positions[vertexId], 0.0, 1.0);
}
)";

		static constexpr char pixelShader[] = R"(
cbuffer PushConstants : register(b0)
{
    uint textureArrayIndex;
    uint cubeTextureIndex;
    uint samplerIndex;
};

float4 PSMain(float4 position : SV_Position) : SV_Target0
{
    if (position.x < 1.0)
    {
        Texture2DArray<float4> textureArray = ResourceDescriptorHeap[textureArrayIndex];
        return textureArray.Load(int4(0, 0, 1, 0));
    }

    TextureCube<float4> cubeTexture = ResourceDescriptorHeap[cubeTextureIndex];
    SamplerState textureSampler = SamplerDescriptorHeap[samplerIndex];
    return cubeTexture.SampleLevel(textureSampler, float3(1.0, 0.0, 0.0), 0.0);
}
)";

		RenderPipelineDesc pipelineDesc{};
		pipelineDesc.vertexShader.source = vertexShader;
		pipelineDesc.vertexShader.entryPoint = "VSMain";
		pipelineDesc.fragmentShader.source = pixelShader;
		pipelineDesc.fragmentShader.entryPoint = "PSMain";
		pipelineDesc.colorFormat = targetDesc.format;
		pipelineDesc.depthFormat = DXGI_FORMAT_UNKNOWN;
		pipelineDesc.depthStencilState.DepthEnable = FALSE;
		pipelineDesc.depthStencilState.StencilEnable = FALSE;
		RenderPipelineState pipeline = device.CreateRenderPipeline( pipelineDesc );

		const std::array<uint32_t, 3> constants = {
			device.GetBindlessIndex( arrayTexture ),
			device.GetBindlessIndex( cubeTexture ),
			ToSamplerIndex( SamplerSlot::PointClamp )
		};
		RenderPass renderPass{};
		renderPass.color[ 0 ].loadOp = LoadOp::Clear;
		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = target;

		ICommandBuffer& commands = device.AcquireCommandBuffer();
		commands.CmdBeginRendering( renderPass, framebuffer );
		commands.CmdBindRenderPipeline( pipeline );
		commands.CmdPushConstants( constants.data(), sizeof( constants ) );
		commands.CmdDraw( 3 );
		commands.CmdEndRendering();
		const SubmitHandle submission = device.Submit( commands );
		device.Wait( submission );

		std::array<uint8_t, 8> result{};
		device.DownloadTexture2D( target, result.data(), 8, 8 );
		Require(
			result[ 0 ] == 0u && result[ 1 ] == 255u &&
			result[ 2 ] == 0u && result[ 3 ] == 255u,
			"Texture2DArray SRV did not sample the requested array slice." );
		Require(
			result[ 4 ] == 255u && result[ 5 ] == 0u &&
			result[ 6 ] == 0u && result[ 7 ] == 255u,
			"TextureCube SRV did not sample the +X face." );

		device.Destroy( target );
		device.Destroy( cubeTexture );
		device.Destroy( arrayTexture );
		device.WaitIdle();
	}
}
