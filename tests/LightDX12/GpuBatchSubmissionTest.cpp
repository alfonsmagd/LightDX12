#include "TestTemplate.hpp"

#include <array>

namespace lightd3d12::tests
{
	void TestGpuBatchSubmission()
	{
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		const std::array<ICommandBuffer*, 0> emptyBatch{};
		Require( device.SubmitBatch( emptyBatch ).Empty(), "An empty batch returned a submission handle." );

		TextureDesc textureDesc{};
		textureDesc.debugName = "LightDX12Tests batch state tracking";
		textureDesc.width = 16;
		textureDesc.height = 16;
		textureDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.usage = TextureUsage::Sampled;
		const TextureHandle texture = device.CreateTexture( textureDesc );

		ICommandBuffer& first = device.AcquireCommandBuffer();
		ICommandBuffer& second = device.AcquireCommandBuffer();
		ICommandBuffer& third = device.AcquireCommandBuffer();
		first.CmdTransitionTexture( texture, D3D12_RESOURCE_STATE_COPY_DEST );
		second.CmdTransitionTexture( texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
		third.CmdTransitionTexture( texture, D3D12_RESOURCE_STATE_COPY_SOURCE );

		const std::array<ICommandBuffer*, 3> commandBuffers = { &first, &second, &third };
		const SubmitHandle submission = device.SubmitBatch( commandBuffers );
		Require( !submission.Empty(), "Submitting a command-buffer batch returned an empty handle." );
		device.Wait( submission );
		Require( device.IsReady( submission ), "A waited batch submission was not reported as ready." );

		ICommandBuffer& duplicate = device.AcquireCommandBuffer();
		const std::array<ICommandBuffer*, 2> duplicateBatch = { &duplicate, &duplicate };
		RequireThrows<std::invalid_argument>( [&] { device.SubmitBatch( duplicateBatch ); },
			"Submitting the same command buffer twice did not fail." );
		device.Wait( device.Submit( duplicate ) );

		const std::array<ICommandBuffer*, 1> nullBatch = { nullptr };
		RequireThrows<std::invalid_argument>( [&] { device.SubmitBatch( nullBatch ); },
			"Submitting a null command buffer did not fail." );

		ICommandBuffer& recycledFirst = device.AcquireCommandBuffer();
		ICommandBuffer& recycledSecond = device.AcquireCommandBuffer();
		ICommandBuffer& recycledThird = device.AcquireCommandBuffer();
		const std::array<ICommandBuffer*, 3> recycledBatch = { &recycledFirst, &recycledSecond, &recycledThird };
		const SubmitHandle recycledSubmission = device.SubmitBatch( recycledBatch );
		device.Wait( recycledSubmission );
		Require( device.IsReady( recycledSubmission ), "Recycled command buffers did not complete as a batch." );

		Require( device.Destroy( texture ), "The batch state-tracking texture was not destroyed." );
	}
}
