#include "Ldx12/TestTemplate.hpp"
#include "Ldx12/Ldx12Native.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <iostream>

// Keeps submitting command buffers while earlier GPU work is pending to validate automatic command-buffer and allocator recycling.

namespace
{
	constexpr uint32_t ourSubmissionCount = 10'000;
	constexpr uint32_t ourActiveCommandBufferCount = 64;
	constexpr uint32_t ourBatchFixupReserve = 4;
	constexpr uint32_t ourImmediatePoolSize = ourActiveCommandBufferCount + ourBatchFixupReserve;

	void RunCommandStreamStressTest()
	{
		using namespace ldx12;
		using namespace ldx12::tests;
		using Clock = std::chrono::steady_clock;

		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;
		context.bindlessCapacity = LDX12_BINDLESS_DYNAMIC_SLOT_FIRST + 8u;
		context.rtvCapacity = 1;
		context.dsvCapacity = 1;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();
		D3D12Native native = device.GetNative();
		ID3D12Device* nativeDevice = native.GetDevice();
		ID3D12CommandQueue* nativeQueue = native.GetCommandQueue();
		Require( nativeDevice != nullptr && nativeQueue != nullptr, "Command-stream stress requires a native device and queue." );

		ComPtr<ID3D12Fence> gateFence;
		Require( SUCCEEDED( nativeDevice->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( gateFence.GetAddressOf() ) ) ), "Failed to create the command-stream gate fence." );
		Require( SUCCEEDED( nativeQueue->Wait( gateFence.Get(), 1 ) ), "Failed to block the command queue for command-stream saturation." );

		TextureDesc textureDesc{};
		textureDesc.debugName = "Ldx12 command-stream texture";
		textureDesc.width = 4;
		textureDesc.height = 4;
		textureDesc.usage = TextureUsage::Sampled;
		const TextureHandle texture = device.CreateTexture( textureDesc );

		std::array<bool, ourImmediatePoolSize> occupiedPoolSlots{};
		SubmitHandle firstSubmission{};
		SubmitHandle saturatedSubmission{};
		SubmitHandle lastSubmission{};
		std::chrono::nanoseconds totalAcquireTime{};
		std::chrono::nanoseconds slowestAcquireTime{};
		std::chrono::nanoseconds firstRecycleAcquireTime{};
		const auto loopStart = Clock::now();

		for( uint32_t submissionIndex = 0; submissionIndex < ourSubmissionCount; ++submissionIndex )
		{
			if( submissionIndex == ourImmediatePoolSize )
			{
				Require( !device.IsReady( firstSubmission ) && !device.IsReady( saturatedSubmission ), "The gated GPU unexpectedly completed a command buffer." );
				for( bool occupied : occupiedPoolSlots )
				{
					Require( occupied, "The command stream did not occupy every immediate pool slot." );
				}
				Require( SUCCEEDED( gateFence->Signal( 1 ) ), "Failed to release the command-stream gate fence." );
			}

			const auto acquireStart = Clock::now();
			ICommandBuffer& commands = device.AcquireCommandBuffer();
			const auto acquireTime = Clock::now() - acquireStart;
			totalAcquireTime += acquireTime;
			slowestAcquireTime = std::max( slowestAcquireTime, std::chrono::duration_cast<std::chrono::nanoseconds>( acquireTime ) );
			if( submissionIndex == ourImmediatePoolSize )
			{
				firstRecycleAcquireTime = acquireTime;
			}

			const D3D12_RESOURCE_STATES state = ( submissionIndex & 1u ) == 0u ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			commands.CmdTransitionTexture( texture, state );
			lastSubmission = device.Submit( commands );
			Require( !lastSubmission.Empty(), "Command-stream submission returned an empty handle." );

			if( submissionIndex == 0 )
			{
				firstSubmission = lastSubmission;
			}
			if( submissionIndex < ourImmediatePoolSize )
			{
				Require( lastSubmission.bufferIndex_ < occupiedPoolSlots.size(), "A command-stream handle referenced a pool slot outside the expected range." );
				Require( !occupiedPoolSlots[ lastSubmission.bufferIndex_ ], "A command-buffer pool slot was reused while the GPU queue was blocked." );
				occupiedPoolSlots[ lastSubmission.bufferIndex_ ] = true;
				saturatedSubmission = lastSubmission;
			}
		}

		const auto loopEnd = Clock::now();
		const auto drainStart = Clock::now();
		device.Wait( lastSubmission );
		const auto drainTime = Clock::now() - drainStart;
		Require( device.IsReady( lastSubmission ), "The final command-stream submission remained pending after the drain." );
		Require( device.Destroy( texture ), "Command-stream texture destruction failed." );
		device.WaitIdle();
		DeviceManager::ShutdownSingleton();
		guard.active = false;

		const auto loopTime = std::chrono::duration_cast<std::chrono::microseconds>( loopEnd - loopStart );
		const auto acquireTime = std::chrono::duration_cast<std::chrono::microseconds>( totalAcquireTime );
		const auto firstRecycleTime = std::chrono::duration_cast<std::chrono::microseconds>( firstRecycleAcquireTime );
		const auto slowestTime = std::chrono::duration_cast<std::chrono::microseconds>( slowestAcquireTime );
		const auto drainedTime = std::chrono::duration_cast<std::chrono::microseconds>( drainTime );
		const double averageSubmissionMicroseconds = static_cast<double>( loopTime.count() ) / ourSubmissionCount;
		const double submissionsPerSecond = loopTime.count() > 0 ? static_cast<double>( ourSubmissionCount ) * 1'000'000.0 / static_cast<double>( loopTime.count() ) : 0.0;

		std::cout << "[COMMAND STREAM] " << ourSubmissionCount << " acquire/record/submit operations completed with no explicit wait inside the loop.\n";
		std::cout << "[COMMAND STREAM] Saturated " << ourImmediatePoolSize << " immediate slots (" << ourActiveCommandBufferCount << " public + " << ourBatchFixupReserve << " batch-fixup reserve) before releasing the GPU queue.\n";
		std::cout << "[COMMAND STREAM] Loop " << loopTime.count() / 1000.0 << " ms, average " << averageSubmissionMicroseconds << " us/submission, throughput " << submissionsPerSecond << " submissions/s.\n";
		std::cout << "[COMMAND STREAM] Total acquire " << acquireTime.count() / 1000.0 << " ms; first recycled acquire " << firstRecycleTime.count() << " us; slowest acquire " << slowestTime.count() << " us; final drain " << drainedTime.count() / 1000.0 << " ms.\n";
	}
}

int main()
{
	try
	{
		RunCommandStreamStressTest();
		return 0;
	}
	catch( const std::exception& exception )
	{
		std::cerr << "[FAIL] Ldx12 command-stream stress test: " << exception.what() << '\n';
		return 1;
	}
}
