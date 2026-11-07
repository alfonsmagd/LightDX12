#include "Ldx12CommandBatch.hpp"

#include "Ldx12ImmediateCommands.hpp"
#include "Ldx12Swapchain.hpp"

#include <cassert>

namespace ldx12
{
	SubmitHandle SubmitCommandBufferBatch( DeviceManager& manager,
		CommandBuffer* const* commandBuffers,
		uint32_t commandBufferCount,
		TextureHandle presentTexture )
	{
		assert( commandBuffers != nullptr && commandBufferCount > 0 && commandBufferCount <= ourMaxCommandBufferBatch );

		DeviceManager::QueueContext& graphicsQueue = manager.GetGraphicsQueueContext();
		Swapchain* owningSwapchain = nullptr;

		if( presentTexture.Valid() )
		{
			owningSwapchain = manager.GetOwningSwapchain( presentTexture );

			assert( owningSwapchain != nullptr );

			commandBuffers[ commandBufferCount - 1 ]->CmdTransitionTexture( presentTexture, D3D12_RESOURCE_STATE_PRESENT );
		}

		const SubmitHandle handle = graphicsQueue.immediateCommands_->SubmitCommandBuffers( commandBuffers, commandBufferCount );

		manager.DeferConstantBufferRanges( commandBuffers, commandBufferCount, handle );
		graphicsQueue.immediateCommands_->RecycleSubmittedCommandBuffers( commandBuffers, commandBufferCount );

		if( owningSwapchain != nullptr )
		{
			owningSwapchain->Present();
		}

		manager.ProcessDeferredReleases();
		return handle;
	}
}
