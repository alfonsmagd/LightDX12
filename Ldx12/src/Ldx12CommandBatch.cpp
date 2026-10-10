#include "Ldx12CommandBatch.hpp"

#include "Ldx12CommandBuffer.hpp"
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
		assert( commandBuffers != nullptr );
		assert( commandBufferCount > 0 && commandBufferCount <= ourMaxCommandBufferBatch );

		for( uint32_t index = 0; index < commandBufferCount; ++index )
		{
			CommandBuffer* commandBuffer = commandBuffers[ index ];

			assert( commandBuffer != nullptr );
			assert( commandBuffer->manager_ == &manager );
			assert( commandBuffer->active_ );
			assert( !commandBuffer->IsRendering() );

			for( uint32_t previousIndex = 0; previousIndex < index; ++previousIndex )
			{
				assert( commandBuffers[ previousIndex ] != commandBuffer );
			}
		}

		DeviceManager::QueueContext& graphicsQueue = manager.GetGraphicsQueueContext();
		Swapchain* owningSwapchain = nullptr;

		if( presentTexture.Valid() )
		{
			owningSwapchain = manager.GetOwningSwapchain( presentTexture );

			assert( owningSwapchain != nullptr );

			commandBuffers[ commandBufferCount - 1 ]->CmdTransitionTexture( presentTexture, D3D12_RESOURCE_STATE_PRESENT );
		}

		std::array<CommandListWrapper*, ourMaxCommandBufferBatch * 2> wrappers = {};
		uint32_t wrapperCount = 0;

		for( uint32_t index = 0; index < commandBufferCount; ++index )
		{
			CommandBuffer* commandBuffer = commandBuffers[ index ];

			if( CommandListWrapper* fixup = commandBuffer->BuildSubmitFixup( commandBuffers, index ) )
			{
				wrappers[ wrapperCount++ ] = fixup;
			}
			wrappers[ wrapperCount++ ] = &commandBuffer->Wrapper();
		}

		const SubmitHandle handle = graphicsQueue.immediateCommands_->SubmitBatch( wrappers.data(), wrapperCount );

		assert( !handle.Empty() );

		manager.CommitConstantBufferRanges( commandBuffers, commandBufferCount, handle );

		for( uint32_t index = 0; index < commandBufferCount; ++index )
		{
			commandBuffers[ index ]->CommitSubmittedResourceStates();
			graphicsQueue.immediateCommands_->ReleaseCommandBuffer( *commandBuffers[ index ] );
		}

		if( owningSwapchain != nullptr )
		{
			owningSwapchain->Present();
		}

		manager.ProcessDeferredReleases();
		return handle;
	}
}
