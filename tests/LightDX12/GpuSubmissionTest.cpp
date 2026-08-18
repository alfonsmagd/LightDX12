#include "TestTemplate.hpp"

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
