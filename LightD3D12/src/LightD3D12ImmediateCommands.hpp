#pragma once

#include "LightD3D12Internal.hpp"

#include <array>
#include <span>

namespace lightd3d12
{
	class ImmediateCommands final
	{
	public:
		static constexpr uint32_t ourMaxCommandBuffers = 64;

		struct CommandListWrapper
		{
			ComPtr<ID3D12CommandAllocator> allocator_;
			ComPtr<ID3D12GraphicsCommandList4> commandList_;
			ComPtr<ID3D12Fence> fence_;
			SubmitHandle handle_{};
			HANDLE fenceEvent_ = nullptr;
			uint64_t fenceValue_ = 0;
			bool isEncoding_ = false;
		};

		struct CommandBufferSubmission final
		{
			CommandListWrapper* commandBuffer_ = nullptr;
			ID3D12CommandList* stateFixupCommandList_ = nullptr;
		};

		ImmediateCommands( ID3D12Device* device, ID3D12CommandQueue* queue, uint32_t numContexts );
		~ImmediateCommands();
		ImmediateCommands( const ImmediateCommands& ) = delete;
		ImmediateCommands& operator=( const ImmediateCommands& ) = delete;

		CommandListWrapper& Acquire();
		SubmitHandle Submit( CommandListWrapper& wrapper );
		SubmitHandle Submit( CommandListWrapper& wrapper, ID3D12CommandList* stateFixupCommandList );
		SubmitHandle SubmitBatch( std::span<const CommandBufferSubmission> commandBuffers );
		SubmitHandle GetLastSubmitHandle() const noexcept;
		SubmitHandle GetNextSubmitHandle() const noexcept;
		bool IsReady( SubmitHandle handle, bool fastCheckNoD3D12 = false ) const;
		void Wait( SubmitHandle handle );
		void WaitAll();

	private:
		std::span<CommandListWrapper> Buffers() noexcept
		{
			return { buffers_.data(), bufferCount_ };
		}

		std::span<const CommandListWrapper> Buffers() const noexcept
		{
			return { buffers_.data(), bufferCount_ };
		}

		CommandListWrapper* FindOldestSubmittedBuffer() noexcept;
		void WaitForFirstAvailable();
		void Purge();

	private:
		ID3D12Device* device_ = nullptr;
		ID3D12CommandQueue* queue_ = nullptr;
		std::array<CommandListWrapper, ourMaxCommandBuffers> buffers_ = {};
		uint32_t bufferCount_ = 0;
		SubmitHandle lastSubmitHandle_ = {};
		SubmitHandle nextSubmitHandle_ = {};
		uint32_t numAvailableCommandBuffers_ = 0;
		uint32_t fenceCounter_ = 1;
	};
}
