#pragma once

#include "Ldx12CommandBuffer.hpp"

#include <array>

namespace ldx12
{
	class ImmediateCommands final
	{
	public:
		ImmediateCommands( ID3D12Device* device, ID3D12CommandQueue* queue, uint32_t numContexts );
		~ImmediateCommands();
		ImmediateCommands( const ImmediateCommands& ) = delete;
		ImmediateCommands& operator=( const ImmediateCommands& ) = delete;

		CommandBuffer& AcquireCommandBuffer( DeviceManager& manager );
		void DiscardCommandBuffer( DeviceManager& manager, CommandBuffer& commandBuffer );
		void ReleaseCommandBuffer( CommandBuffer& commandBuffer ) noexcept;
		void RecycleSubmittedCommandBuffers( CommandBuffer* const* commandBuffers, uint32_t commandBufferCount );
		void ReleaseAllCommandBuffers() noexcept;
		CommandListWrapper& Acquire();
		// Prepare state fixups and submit the native lists in recording order.
		SubmitHandle SubmitCommandBuffers( CommandBuffer* const* commandBuffers, uint32_t commandBufferCount );
		SubmitHandle Submit( CommandListWrapper& wrapper );
		SubmitHandle SubmitBatch( CommandListWrapper* const* wrappers, uint32_t commandListCount );
		SubmitHandle GetLastSubmitHandle() const noexcept;
		SubmitHandle GetNextSubmitHandle() const noexcept;
		bool IsReady( SubmitHandle handle, bool fastCheckNoD3D12 = false ) const;
		void Wait( SubmitHandle handle );
		void WaitAll() noexcept;

	private:
		CommandListWrapper* FindOldestSubmittedBuffer() noexcept;
		void WaitForFirstAvailable();
		void Purge();

	private:
		ID3D12Device* device_ = nullptr;
		ID3D12CommandQueue* queue_ = nullptr;
		std::array<CommandListWrapper, ourMaxImmediateCommandBuffers> buffers_ = {};
		std::array<CommandBuffer, ourMaxActiveCommandBuffers> commandBuffers_ = {};
		uint32_t bufferCount_ = 0;
		SubmitHandle lastSubmitHandle_ = {};
		SubmitHandle nextSubmitHandle_ = {};
		uint32_t numAvailableCommandBuffers_ = 0;
		uint64_t preparationId_ = 0;
		uint32_t fenceCounter_ = 1;
	};
}
