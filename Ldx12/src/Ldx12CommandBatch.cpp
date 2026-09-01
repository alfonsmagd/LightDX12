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
		DeviceManager::QueueContext& graphicsQueue = manager.GetGraphicsQueueContext();
		if( commandBufferCount == 0 )
		{
			throw std::invalid_argument( "SubmitBatch requires at least one command buffer." );
		}
		if( commandBuffers == nullptr )
		{
			throw std::invalid_argument( "SubmitBatch requires a valid command buffer array." );
		}
		if( commandBufferCount > ourMaxCommandBufferBatch )
		{
			throw std::length_error( "SubmitBatch cannot contain more than " + std::to_string( ourMaxCommandBufferBatch ) + " command buffers." );
		}

		for( uint32_t index = 0; index < commandBufferCount; ++index )
		{
			if( commandBuffers[ index ] == nullptr )
			{
				throw std::invalid_argument( "SubmitBatch cannot contain null command buffers." );
			}

			CommandBuffer* commandBuffer = commandBuffers[ index ];
			if( commandBuffer->manager_ != &manager || !commandBuffer->active_ )
			{
				throw std::invalid_argument( "A command buffer in the batch does not belong to this render device." );
			}
			if( commandBuffer->IsRendering() )
			{
				throw std::logic_error( "Cannot submit a command buffer while a render pass is still active." );
			}

			for( uint32_t previousIndex = 0; previousIndex < index; ++previousIndex )
			{
				if( commandBuffers[ previousIndex ] == commandBuffer )
				{
					throw std::invalid_argument( "SubmitBatch cannot contain the same command buffer more than once." );
				}
			}
		}

		Swapchain* owningSwapchain = nullptr;
		if( presentTexture.Valid() )
		{
			owningSwapchain = manager.GetOwningSwapchain( presentTexture );
			if( owningSwapchain == nullptr )
			{
				throw std::invalid_argument( "Present texture does not belong to a swapchain." );
			}

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

		for( uint32_t index = 0; index < commandBufferCount; ++index )
		{
			commandBuffers[ index ]->CommitSubmittedTextureStates();
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
