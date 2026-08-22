#include "Ldx12/TestTemplate.hpp"

#include <array>
#include <chrono>
#include <exception>
#include <iostream>

// Simulates 10,000 offscreen frames with three reusable slots and waits only before reusing a slot that is still busy.

namespace
{
	constexpr uint32_t ourFrameCount = 10'000;
	constexpr uint32_t ourFramesInFlight = 3;

	void RunFrameStressTest()
	{
		using namespace ldx12;
		using namespace ldx12::tests;
		using Clock = std::chrono::steady_clock;

		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;
		context.framesInFlight = ourFramesInFlight;
		context.bindlessCapacity = LDX12_BINDLESS_DYNAMIC_SLOT_FIRST + 32u;
		context.rtvCapacity = ourFramesInFlight;
		context.dsvCapacity = 1;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		TextureDesc frameTargetDesc{};
		frameTargetDesc.debugName = "Ldx12 frame stress render target";
		frameTargetDesc.width = 64;
		frameTargetDesc.height = 64;
		frameTargetDesc.usage = TextureUsage::RenderTarget;

		TextureDesc transientTextureDesc{};
		transientTextureDesc.debugName = "Ldx12 frame stress transient texture";
		transientTextureDesc.width = 4;
		transientTextureDesc.height = 4;
		transientTextureDesc.usage = TextureUsage::Sampled;

		std::array<TextureHandle, ourFramesInFlight> frameTargets{};
		for( TextureHandle& frameTarget : frameTargets )
		{
			frameTarget = device.CreateTexture( frameTargetDesc );
			Require( frameTarget.Valid(), "Frame stress render-target creation failed." );
		}

		std::array<SubmitHandle, ourFramesInFlight> frameSubmissions{};
		std::chrono::nanoseconds throttleWaitTime{};
		std::chrono::nanoseconds recordSubmitTime{};
		uint32_t throttleWaitCount = 0;
		uint32_t readyReuseCount = 0;
		const auto loopStart = Clock::now();

		for( uint32_t frameIndex = 0; frameIndex < ourFrameCount; ++frameIndex )
		{
			const uint32_t frameSlot = frameIndex % ourFramesInFlight;
			const SubmitHandle previousSubmission = frameSubmissions[ frameSlot ];
			if( !previousSubmission.Empty() )
			{
				if( device.IsReady( previousSubmission ) )
				{
					++readyReuseCount;
				}
				else
				{
					const auto waitStart = Clock::now();
					device.Wait( previousSubmission );
					throttleWaitTime += Clock::now() - waitStart;
					++throttleWaitCount;
				}
			}

			const auto recordStart = Clock::now();
			const TextureHandle transientTexture = device.CreateTexture( transientTextureDesc );
			ICommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdTransitionTexture( transientTexture, D3D12_RESOURCE_STATE_COPY_DEST );

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = {
				static_cast<float>( frameIndex & 1u ),
				static_cast<float>( ( frameIndex >> 1u ) & 1u ),
				static_cast<float>( ( frameIndex >> 2u ) & 1u ),
				1.0f
			};

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = frameTargets[ frameSlot ];
			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdEndRendering();
			frameSubmissions[ frameSlot ] = device.Submit( commands );
			Require( !frameSubmissions[ frameSlot ].Empty(), "Frame stress submission returned an empty handle." );
			Require( device.Destroy( transientTexture ), "Frame stress transient destruction failed." );
			recordSubmitTime += Clock::now() - recordStart;
		}

		const auto loopEnd = Clock::now();
		const auto drainStart = Clock::now();
		for( SubmitHandle submission : frameSubmissions )
		{
			if( !submission.Empty() && !device.IsReady( submission ) )
			{
				device.Wait( submission );
			}
		}
		const auto drainTime = Clock::now() - drainStart;

		for( TextureHandle frameTarget : frameTargets )
		{
			Require( device.Destroy( frameTarget ), "Frame stress render-target destruction failed." );
		}
		device.WaitIdle();
		DeviceManager::ShutdownSingleton();
		guard.active = false;

		const auto loopTime = std::chrono::duration_cast<std::chrono::microseconds>( loopEnd - loopStart );
		const auto recordedTime = std::chrono::duration_cast<std::chrono::microseconds>( recordSubmitTime );
		const auto waitedTime = std::chrono::duration_cast<std::chrono::microseconds>( throttleWaitTime );
		const auto drainedTime = std::chrono::duration_cast<std::chrono::microseconds>( drainTime );
		const double averageFrameMicroseconds = static_cast<double>( loopTime.count() ) / ourFrameCount;
		const double framesPerSecond = loopTime.count() > 0 ? static_cast<double>( ourFrameCount ) * 1'000'000.0 / static_cast<double>( loopTime.count() ) : 0.0;

		std::cout << "[FRAME STRESS] " << ourFrameCount << " frames with " << ourFramesInFlight << " frames in flight completed in " << loopTime.count() / 1000.0 << " ms.\n";
		std::cout << "[FRAME STRESS] Average " << averageFrameMicroseconds << " us/frame, throughput " << framesPerSecond << " frames/s.\n";
		std::cout << "[FRAME STRESS] Record/create/submit/destroy " << recordedTime.count() / 1000.0 << " ms; slot waits " << throttleWaitCount << " (" << waitedTime.count() / 1000.0 << " ms); already-ready reuses " << readyReuseCount << "; final drain " << drainedTime.count() / 1000.0 << " ms.\n";
	}
}

int main()
{
	try
	{
		RunFrameStressTest();
		return 0;
	}
	catch( const std::exception& exception )
	{
		std::cerr << "[FAIL] Ldx12 frame stress test: " << exception.what() << '\n';
		return 1;
	}
}
