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

		CommandBufferImpl& AcquireCommandBuffer( DeviceManager& manager );
		CommandBufferImpl* FindActiveCommandBuffer( ICommandBuffer* commandBuffer ) noexcept;
		void ReleaseCommandBuffer( CommandBufferImpl& commandBuffer ) noexcept;
		void ReleaseAllCommandBuffers() noexcept;
		CommandListWrapper& Acquire();
		SubmitHandle Submit( CommandListWrapper& wrapper );
		SubmitHandle SubmitBatch( CommandListWrapper* const* wrappers, uint32_t commandListCount );
		SubmitHandle GetLastSubmitHandle() const noexcept;
		SubmitHandle GetNextSubmitHandle() const noexcept;
		bool IsReady( SubmitHandle handle, bool fastCheckNoD3D12 = false ) const;
		void Wait( SubmitHandle handle );
		void WaitAll();

	private:
		CommandListWrapper* FindOldestSubmittedBuffer() noexcept;
		void WaitForFirstAvailable();
		void Purge();

	private:
		ID3D12Device* device_ = nullptr;
		ID3D12CommandQueue* queue_ = nullptr;
		std::array<CommandListWrapper, ourMaxImmediateCommandBuffers> buffers_ = {};
		std::array<CommandBufferImpl, ourMaxActiveCommandBuffers> commandBuffers_ = {};
		uint32_t bufferCount_ = 0;
		SubmitHandle lastSubmitHandle_ = {};
		SubmitHandle nextSubmitHandle_ = {};
		uint32_t numAvailableCommandBuffers_ = 0;
		uint32_t fenceCounter_ = 1;
	};
}
