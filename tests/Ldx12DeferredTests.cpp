#include "Ldx12Internal.hpp"
#include "Ldx12ImmediateCommands.hpp"
#include "Ldx12/Ldx12Native.hpp"
#include "Ldx12/TestTemplate.hpp"

#include <iostream>
#include <stdexcept>

namespace ldx12
{
	// Access the private cleanup guard and ring lifetime operations under test.
	struct DeferredReleaseTestAccess
	{
		static SubmitHandle SubmitRingBatch( DeviceManager& manager, CommandBuffer* const* commands, uint32_t count, uint32_t lastBufferIndex )
		{
			SubmitHandle expectedSubmission;
			expectedSubmission.bufferIndex_ = lastBufferIndex;
			expectedSubmission.submitId_ = manager.graphicsQueue_.immediateCommands_->GetLastSubmitHandle().submitId_ + 1;

			// Exercise Commit explicitly until the public submission path integrates it.
			manager.CommitConstantBufferRanges( commands, count, expectedSubmission );

			const SubmitHandle submission = manager.GetRenderDevice()->SubmitBatch( commands, count );

			tests::Require( submission.Handle() == expectedSubmission.Handle(), "Unexpected ring batch submission handle." );
			return submission;
		}

		static void RunRingRanges()
		{
			ContextDesc context{};
			context.enableDebugLayer = true;
			context.preferHighPerformanceAdapter = false;

			tests::DeviceManagerGuard guard;
			DeviceManager& manager = DeviceManager::Initialize( context );
			guard.active = true;

			RenderDevice& device = *manager.GetRenderDevice();
			D3D12Native native = device.GetNative();
			DeviceManager::ConstantBufferRing& ring0 = manager.constantBufferRings_[ 0 ];
			DeviceManager::ConstantBufferRing& ring1 = manager.constantBufferRings_[ 1 ];
			constexpr uint32_t alignment = LDX12_CONSTANT_BUFFER_RING_ALIGNMENT;
			const uint32_t value = 0x13572468u;
			const uint32_t nextValue = 0x24681357u;

			CommandBuffer& first = device.AcquireCommandBuffer();
			CommandBuffer& pending = device.AcquireCommandBuffer();
			CommandBuffer& last = device.AcquireCommandBuffer();
			const uint32_t lastBufferIndex = manager.graphicsQueue_.immediateCommands_->GetNextSubmitHandle().bufferIndex_;

			// Two commands in one batch surround a different recording's allocation.
			tests::Require( manager.UploadConstantBuffer( first, &value, sizeof( value ), 0 ) == 0, "Unexpected first ring offset." );
			tests::Require( manager.UploadConstantBuffer( pending, &value, sizeof( value ), 0 ) == alignment, "Unexpected pending ring offset." );
			tests::Require( manager.UploadConstantBuffer( last, &value, sizeof( value ), 0 ) == 2 * alignment, "Unexpected last ring offset." );
			tests::Require( manager.UploadConstantBuffer( first, &value, sizeof( value ), 0 ) == 3 * alignment, "Unexpected repeated upload offset." );
			tests::Require( manager.UploadConstantBuffer( first, &value, sizeof( value ), 1 ) == 0, "Ring 1 did not allocate independently." );

			ComPtr<ID3D12Fence> gate;

			tests::Require( SUCCEEDED( native.GetDevice()->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( gate.GetAddressOf() ) ) ),
				"Failed to create the GPU gate." );
			DeviceManager::DeferredRelease::OnFailure releaseGate( [ &gate ]() noexcept { gate->Signal( 1 ); } );
			tests::Require( SUCCEEDED( native.GetCommandQueue()->Wait( gate.Get(), 1 ) ), "Failed to pause the GPU queue." );

			CommandBuffer* firstBatch[] = { &first, &last };
			const SubmitHandle firstSubmission = SubmitRingBatch( manager, firstBatch, 2, lastBufferIndex );

			// Repeated cleanup passes represent subsequent frames with the GPU still busy.
			for( uint32_t frameIndex = 0; frameIndex < 3; ++frameIndex )
			{
				manager.ProcessDeferredReleases();

				tests::Require( !device.IsReady( firstSubmission ), "The gated submission completed early." );
				tests::Require( manager.graphicsQueue_.deferredReleases_.size() == 1, "A pending batch lost its deferred task." );
				tests::Require( ring0.ranges_.size() == 4 && ring0.tail_ == 0, "Pending ring 0 ranges were reclaimed." );
				tests::Require( ring1.ranges_.size() == 1 && ring1.tail_ == 0, "Pending ring 1 range was reclaimed." );

				for( const DeviceManager::ConstantBufferRing::RangeState& range : ring0.ranges_ )
				{
					tests::Require( !range.completed_, "A range was marked complete before its submission finished." );
				}
			}

			CommandBuffer& reused = device.AcquireCommandBuffer();
			const uint32_t reusedBufferIndex = manager.graphicsQueue_.immediateCommands_->GetNextSubmitHandle().bufferIndex_;

			tests::Require( &reused == &first, "The test did not reuse the submitted command buffer." );
			tests::Require( manager.UploadConstantBuffer( reused, &nextValue, sizeof( nextValue ), 0 ) == 4 * alignment,
				"Reusing a command buffer overwrote pending ring 0 data." );
			tests::Require( manager.UploadConstantBuffer( reused, &nextValue, sizeof( nextValue ), 1 ) == alignment,
				"Reusing a command buffer overwrote pending ring 1 data." );
			tests::Require( std::memcmp( ring1.mappedPtr_, &value, sizeof( value ) ) == 0, "Previous recording data changed before GPU completion." );

			tests::Require( SUCCEEDED( gate->Signal( 1 ) ), "Failed to resume the GPU queue." );
			device.Wait( firstSubmission );

			tests::Require( manager.graphicsQueue_.deferredReleases_.empty(), "Completed batch retained its deferred task." );
			tests::Require( ring0.tail_ == alignment && ring0.ranges_.size() == 4, "Completion reclaimed an interleaved pending range." );
			tests::Require( !ring0.ranges_[ 0 ].completed_ && ring0.ranges_[ 1 ].completed_ && ring0.ranges_[ 2 ].completed_ && !ring0.ranges_[ 3 ].completed_,
				"The batch did not complete exactly its own ranges." );
			tests::Require( ring1.ranges_.size() == 1 && !ring1.ranges_.front().completed_, "The old batch released the reused command's range." );
			tests::Require( std::memcmp( ring1.mappedPtr_ + alignment, &nextValue, sizeof( nextValue ) ) == 0, "The new recording lost its data." );

			CommandBuffer* secondBatch[] = { &pending, &reused };
			device.Wait( SubmitRingBatch( manager, secondBatch, 2, reusedBufferIndex ) );

			tests::Require( ring0.ranges_.empty() && ring0.tail_ == ring0.head_, "Completed batches did not reclaim ring 0." );
			tests::Require( ring1.ranges_.empty() && ring1.tail_ == ring1.head_, "Completed batches did not reclaim ring 1." );
			std::cout << "[PASS] Deferred ring ranges survive cleanup passes, interleaved batches and command buffer reuse\n";

			CommandBuffer& full = device.AcquireCommandBuffer();
			const uint32_t fullBufferIndex = manager.graphicsQueue_.immediateCommands_->GetNextSubmitHandle().bufferIndex_;
			std::array<uint8_t, LDX12_CONSTANT_BUFFER_RING_SIZE_BYTES> fullData{};

			tests::Require( manager.UploadConstantBuffer( full, fullData.data(), static_cast<uint32_t>( fullData.size() ), 0 ) == 0,
				"Reclaimed ring could not fit a full-capacity allocation after wrapping." );

			const uint64_t fullHead = ring0.head_;

			tests::Require( manager.UploadConstantBuffer( full, &value, sizeof( value ), 0 ) == UINT32_MAX, "A full ring accepted another allocation." );
			tests::Require( ring0.head_ == fullHead && ring0.ranges_.size() == 1, "A failed upload changed the ring reservations." );

			CommandBuffer* fullBatch[] = { &full };
			device.Wait( SubmitRingBatch( manager, fullBatch, 1, fullBufferIndex ) );
			manager.ProcessDeferredReleases();

			tests::Require( ring0.ranges_.empty() && ring0.tail_ == ring0.head_, "Full-capacity allocation was not reclaimed." );
			tests::Require( manager.graphicsQueue_.deferredReleases_.empty(), "Completed ring tasks were not removed." );
			std::cout << "[PASS] Retired ring space supports full-capacity reuse and rejects overflow\n";
		}

		static void Run()
		{
			using Deferred = DeviceManager::DeferredRelease;
			int cleanupCount = 0;
			{
				Deferred::OnFailure cleanup( [ &cleanupCount ]() noexcept { ++cleanupCount; } );
			}
			if( cleanupCount != 0 )
				throw std::runtime_error( "Normal exit ran cleanup." );
			std::cout << "[PASS] Normal exit retains the resource\n";

			struct ExpectedFailure{};
			bool propagated = false;
			try
			{
				Deferred::OnFailure cleanup( [ &cleanupCount ]() noexcept { ++cleanupCount; } );
				throw ExpectedFailure{};
			}
			catch( const ExpectedFailure& )
			{
				propagated = true;
				if( cleanupCount != 1 )
					throw std::runtime_error( "Cleanup did not run exactly once before the handler." );
			}
			if( !propagated )
				throw std::runtime_error( "The original exception was swallowed." );
			std::cout << "[PASS] Exception runs cleanup once and continues to the caller\n";

			{
				Deferred::OnFailure cleanup( [ &cleanupCount ]() noexcept { ++cleanupCount; } );
				try
				{
					throw ExpectedFailure{};
				}
				catch( const ExpectedFailure& )
				{
					std::cout << "[PASS] incremented ++ cleanupcountg \n";
				}
			}
			if( cleanupCount != 1 )
				throw std::runtime_error( "A locally handled exception ran cleanup." );
			std::cout << "[PASS] Locally handled exception retains the resource\n";
		}
	};
}

int main()
{
	try
	{
		ldx12::DeferredReleaseTestAccess::Run();
		ldx12::DeferredReleaseTestAccess::RunRingRanges();
		return 0;
	}
	catch( const std::exception& error )
	{
		std::cerr << "[FAIL] " << error.what() << '\n';
		return 1;
	}
}
