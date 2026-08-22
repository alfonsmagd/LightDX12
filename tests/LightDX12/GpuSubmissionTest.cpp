#include "TestTemplate.hpp"

#include <array>

namespace lightd3d12::tests
{
	void TestGpuSubmissionSynchronization()
	{
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		const SubmitHandle emptySubmission{};
		Require( device.IsReady( emptySubmission ),
			"An empty submission handle was not reported as ready." );
		device.Wait( emptySubmission );

		RequireThrows<std::invalid_argument>(
			[&device] { device.SubmitBatch( nullptr, 0 ); },
			"SubmitBatch accepted an empty batch." );

		TextureDesc batchTextureDesc{};
		batchTextureDesc.debugName = "LightDX12Tests batch state tracking";
		const TextureHandle batchTexture = device.CreateTexture( batchTextureDesc );

		ICommandBuffer& firstBatchCommandBuffer = device.AcquireCommandBuffer();
		ICommandBuffer& secondBatchCommandBuffer = device.AcquireCommandBuffer();
		ICommandBuffer& thirdBatchCommandBuffer = device.AcquireCommandBuffer();
		ICommandBuffer& fourthBatchCommandBuffer = device.AcquireCommandBuffer();
		firstBatchCommandBuffer.CmdTransitionTexture(
			batchTexture, D3D12_RESOURCE_STATE_COPY_DEST );
		secondBatchCommandBuffer.CmdTransitionTexture(
			batchTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
		thirdBatchCommandBuffer.CmdTransitionTexture(
			batchTexture, D3D12_RESOURCE_STATE_COPY_SOURCE );
		fourthBatchCommandBuffer.CmdTransitionTexture(
			batchTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );

		ICommandBuffer* batchWithNull[] = { &firstBatchCommandBuffer, nullptr };
		RequireThrows<std::invalid_argument>(
			[&device, &batchWithNull]
			{
				device.SubmitBatch( batchWithNull, static_cast<uint32_t>( std::size( batchWithNull ) ) );
			},
			"SubmitBatch accepted a null command buffer." );

		ICommandBuffer* duplicateBatch[] = {
			&firstBatchCommandBuffer,
			&firstBatchCommandBuffer
		};
		RequireThrows<std::invalid_argument>(
			[&device, &duplicateBatch]
			{
				device.SubmitBatch( duplicateBatch, static_cast<uint32_t>( std::size( duplicateBatch ) ) );
			},
			"SubmitBatch accepted a duplicate command buffer." );

		ICommandBuffer* oversizedBatch[] = { &firstBatchCommandBuffer };
		RequireThrows<std::length_error>(
			[&device, &oversizedBatch]
			{
				device.SubmitBatch( oversizedBatch, 5 );
			},
			"SubmitBatch accepted more command buffers than the supported maximum." );

		ICommandBuffer* commandBufferBatch[] = {
			&firstBatchCommandBuffer,
			&secondBatchCommandBuffer,
			&thirdBatchCommandBuffer,
			&fourthBatchCommandBuffer
		};
		const SubmitHandle batchSubmission = device.SubmitBatch(
			commandBufferBatch,
			static_cast<uint32_t>( std::size( commandBufferBatch ) ) );
		Require( !batchSubmission.Empty(),
			"Submitting a command-buffer batch returned an empty submission handle." );
		device.Wait( batchSubmission );
		Require( device.IsReady( batchSubmission ),
			"A waited command-buffer batch was not reported as ready." );
		device.Destroy( batchTexture );

		ICommandBuffer& commandBuffer = device.AcquireCommandBuffer();
		const SubmitHandle submission = device.Submit( commandBuffer );
		Require( !submission.Empty(),
			"Submitting a command buffer returned an empty submission handle." );

		device.Wait( submission );
		Require( device.IsReady( submission ),
			"A waited submission was not reported as ready." );

		constexpr uint32_t recycleSubmissionCount = 65;
		for( uint32_t index = 0; index < recycleSubmissionCount; ++index )
		{
			ICommandBuffer& recycledCommandBuffer = device.AcquireCommandBuffer();
			const SubmitHandle recycledSubmission = device.Submit( recycledCommandBuffer );
			Require( !recycledSubmission.Empty(),
				"Command-buffer recycling returned an empty submission handle." );
			device.Wait( recycledSubmission );
		}

		Require( device.IsReady( submission ),
			"A completed submission became pending after command-buffer recycling." );
	}
}
