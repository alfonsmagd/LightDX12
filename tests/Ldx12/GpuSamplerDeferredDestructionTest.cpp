#include "TestTemplate.hpp"
#include "Ldx12/Ldx12Native.hpp"

#include <array>

namespace ldx12::tests
{
	namespace
	{
		class PausedGpuQueue final
		{
		public:
			explicit PausedGpuQueue( const D3D12Native& native )
			{
				Require( SUCCEEDED( native.GetDevice()->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( fence_.GetAddressOf() ) ) ),
					"Failed to create the queue pause fence." );
				Require( SUCCEEDED( native.GetCommandQueue()->Wait( fence_.Get(), 1 ) ), "Failed to pause the GPU queue." );
			}

			~PausedGpuQueue()
			{
				fence_->Signal( 1 );
			}

			void Resume()
			{
				Require( SUCCEEDED( fence_->Signal( 1 ) ), "Failed to resume the GPU queue." );
			}

		private:
			ComPtr<ID3D12Fence> fence_;
		};
	}

	void TestGpuSamplerDeferredDestruction()
	{
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;
		context.bindlessCapacity = 128;
		context.rtvCapacity = 16;
		context.dsvCapacity = 8;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();
		D3D12Native native = device.GetNative();

		Require( native.GetDevice() != nullptr, "D3D12 device creation failed." );
		Require( native.GetCommandQueue() != nullptr, "D3D12 command queue creation failed." );

		SamplerDesc samplerDesc{};
		std::array<SamplerHandle, ourCustomSamplerCount> samplers{};
		for( uint32_t index = 0; index < ourCustomSamplerCount; ++index )
		{
			samplers[ index ] = device.CreateSampler( samplerDesc );
			Require( device.GetSamplerIndex( samplers[ index ] ) == LDX12_CUSTOM_SAMPLER_SLOT_FIRST + index,
				"Custom sampler was not created in its reserved descriptor slot." );
		}

		RequireThrows<std::length_error>( [ &device, &samplerDesc ] { device.CreateSampler( samplerDesc ); },
			"More than four runtime custom samplers were created." );

		const uint32_t recycledSamplerIndex = device.GetSamplerIndex( samplers[ 1 ] );
		const SamplerHandle retiredSampler = samplers[ 1 ];

		CommandBuffer& samplerCommands = device.AcquireCommandBuffer();

		PausedGpuQueue pausedQueue( native );
		const SubmitHandle samplerSubmission = device.Submit( samplerCommands );

		Require( !device.IsReady( samplerSubmission ), "Sampler submission completed while the GPU queue was paused." );
		Require( device.Destroy( retiredSampler ), "Failed to retire a sampler after submission." );
		Require( !device.IsAlive( retiredSampler ), "Destroyed sampler handle remained alive." );
		RequireThrows<std::length_error>( [ &device, &samplerDesc ] { device.CreateSampler( samplerDesc ); },
			"Sampler creation recycled a descriptor while its submission was pending." );
		Require( !device.IsReady( samplerSubmission ), "Sampler submission did not remain pending during descriptor exhaustion." );

		pausedQueue.Resume();
		device.Wait( samplerSubmission );

		Require( device.IsReady( samplerSubmission ), "Sampler submission did not complete after releasing the queue gate." );

		samplers[ 1 ] = device.CreateSampler( samplerDesc );

		Require( device.GetSamplerIndex( samplers[ 1 ] ) == recycledSamplerIndex, "Destroyed custom sampler slot was not recycled." );
		Require( samplers[ 1 ].Index() == retiredSampler.Index() && samplers[ 1 ].Gen() != retiredSampler.Gen(),
			"Recreated sampler did not reuse its slot with a new generation." );
		Require( !device.IsAlive( retiredSampler ), "Retired sampler became valid after descriptor reuse." );

		for( SamplerHandle sampler : samplers )
		{
			device.Destroy( sampler );
		}
		device.WaitIdle();
	}
}
