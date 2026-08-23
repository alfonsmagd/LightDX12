#include "Ldx12CommandBatch.hpp"

#include "Ldx12CommandBuffer.hpp"
#include "Ldx12Swapchain.hpp"

#include <cassert>

namespace ldx12
{
	SubmitHandle SubmitCommandBufferBatch( DeviceManager& manager, ICommandBuffer* const* commandBuffers, uint32_t commandBufferCount, TextureHandle presentTexture )
	{
		DeviceManager::QueueContext& graphicsQueue = manager.GetGraphicsQueueContext();
		std::array<CommandBufferImpl*, ourMaxCommandBufferBatch> validatedCommandBuffers = {};
		std::array<std::unique_ptr<CommandBufferImpl>*, ourMaxCommandBufferBatch> activeSlots = {};
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

			CommandBufferImpl* commandBuffer = dynamic_cast<CommandBufferImpl*>( commandBuffers[ index ] );
			if( commandBuffer == nullptr )
			{
				throw std::invalid_argument( "A command buffer in the batch does not belong to this render device." );
			}
			if( commandBuffer->IsRendering() )
			{
				throw std::logic_error( "Cannot submit a command buffer while a render pass is still active." );
			}

			for( uint32_t previousIndex = 0; previousIndex < index; ++previousIndex )
			{
				if( validatedCommandBuffers[ previousIndex ] == commandBuffer )
				{
					throw std::invalid_argument( "SubmitBatch cannot contain the same command buffer more than once." );
				}
			}

			for( std::unique_ptr<CommandBufferImpl>& activeCommandBuffer : graphicsQueue.activeCommandBuffers_ )
			{
				if( activeCommandBuffer.get() == commandBuffer )
				{
					activeSlots[ index ] = &activeCommandBuffer;
					break;
				}
			}
			if( activeSlots[ index ] == nullptr )
			{
				throw std::invalid_argument( "A command buffer in the batch does not belong to this render device." );
			}

			validatedCommandBuffers[ index ] = commandBuffer;
		}

		Swapchain* owningSwapchain = nullptr;
		if( presentTexture.Valid() )
		{
			owningSwapchain = manager.GetOwningSwapchain( presentTexture );
			if( owningSwapchain == nullptr )
			{
				throw std::invalid_argument( "Present texture does not belong to a swapchain." );
			}

			assert( validatedCommandBuffers[ commandBufferCount - 1 ] != nullptr );
			validatedCommandBuffers[ commandBufferCount - 1 ]->CmdTransitionTexture( presentTexture, D3D12_RESOURCE_STATE_PRESENT );
		}

		std::array<ImmediateCommands::CommandListWrapper*, ourMaxCommandBufferBatch * 2> wrappers = {};
		uint32_t wrapperCount = 0;

		for( uint32_t index = 0; index < commandBufferCount; ++index )
		{
			CommandBufferImpl* commandBuffer = validatedCommandBuffers[ index ];
			assert( commandBuffer != nullptr );
			if( ImmediateCommands::CommandListWrapper* fixup = commandBuffer->BuildSubmitFixup( validatedCommandBuffers.data(), index ) )
			{
				wrappers[ wrapperCount++ ] = fixup;
			}
			wrappers[ wrapperCount++ ] = &commandBuffer->Wrapper();
		}

		const SubmitHandle handle = graphicsQueue.immediateCommands_->SubmitBatch( wrappers.data(), wrapperCount );
		assert( !handle.Empty() );

		for( uint32_t index = 0; index < commandBufferCount; ++index )
		{
			assert( validatedCommandBuffers[ index ] != nullptr );
			assert( activeSlots[ index ] != nullptr );
			validatedCommandBuffers[ index ]->CommitSubmittedTextureStates();

			activeSlots[ index ]->reset();
		}

		if( owningSwapchain != nullptr )
		{
			owningSwapchain->Present();
		}
		manager.ProcessDeferredReleases();
		return handle;
	}
}
