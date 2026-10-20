#include "TestTemplate.hpp"

#include <array>

namespace ldx12::tests
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

		TextureDesc targetDesc{};
		targetDesc.debugName = "Batched render target";
		targetDesc.width = 2;
		targetDesc.height = 2;
		targetDesc.usage = TextureUsage::RenderTarget;

		const TextureHandle target = device.CreateTexture( targetDesc );
		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = target;
		RenderPass renderPass{};
		renderPass.color[ 0 ].loadOp = LoadOp::Clear;
		renderPass.color[ 0 ].clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };

		// Discarded transitions must not affect the next recording of this resource.
		CommandBuffer& cancelled = device.AcquireCommandBuffer();
		cancelled.CmdTransitionTexture( target, D3D12_RESOURCE_STATE_COPY_DEST );
		device.Discard( cancelled );
		RequireThrows<std::invalid_argument>( [ & ] { device.Discard( cancelled ); }, "Discard accepted an inactive command buffer." );

		// Record both passes before submitting; the second pass must see the first pass's state.
		CommandBuffer& first = device.AcquireCommandBuffer();
		first.CmdBeginRendering( renderPass, framebuffer );
		first.CmdEndRendering();
		first.CmdTransitionTexture( target, D3D12_RESOURCE_STATE_COPY_SOURCE );

		renderPass.color[ 0 ].clearColor = { 0.0f, 1.0f, 0.0f, 1.0f };
		CommandBuffer& second = device.AcquireCommandBuffer();
		second.CmdBeginRendering( renderPass, framebuffer );
		second.CmdEndRendering();

		CommandBuffer* batch[] = { &first, &second };
		const SubmitHandle submission = device.SubmitBatch( batch, 2 );

		device.Wait( submission );
		std::array<uint32_t, 4> pixels{};

		device.DownloadTexture2D( target, pixels.data(), 2u * sizeof( uint32_t ), sizeof( pixels ) );
		const std::array<uint32_t, 4> expected = { 0xff00ff00u, 0xff00ff00u, 0xff00ff00u, 0xff00ff00u };

		Require( pixels == expected, "Batched passes did not leave the second pass's green pixels." );
		device.Destroy( target );
		device.WaitIdle();
	}
}
