#include "Ldx12Internal.hpp"
#include "Ldx12/Ldx12Native.hpp"
#include "Ldx12/TestTemplate.hpp"

#include <iostream>

namespace ldx12
{
	struct DeferredReleaseTestAccess
	{
		static void Run( DeviceManager& manager )
		{
			using tests::Require;
			RenderDevice& device = *manager.GetRenderDevice();
			D3D12Native native = device.GetNative();
			const char* shader = R"(
#include "Ldx12_ConstantBuffers.hlsli"
cbuffer OutputBinding : register(b0) { uint outputIndex; uint outputOffset; };
struct Data { uint4 values; };
[numthreads(1, 1, 1)]
void ReadBoth()
{
    Data first = CB0.Load<Data>();
    Data second = CB1.Load<Data>();
    RWByteAddressBuffer output = ResourceDescriptorHeap[outputIndex];
    output.Store4(outputOffset, uint4(first.values.x, second.values.x, first.values.w, second.values.w));
}
[numthreads(1, 1, 1)]
void ReadReset()
{
    Data first = CB0.Load<Data>();
    RWByteAddressBuffer output = ResourceDescriptorHeap[outputIndex];
    output.Store4(outputOffset, uint4(first.values.x, first.values.w, ldx12ConstantBufferOffsets >> 16, 0));
}
)";
			ComputePipelineDesc pipelineDesc{};
			pipelineDesc.computeShader.source = shader;
			pipelineDesc.computeShader.entryPoint = "ReadBoth";
			pipelineDesc.computeShader.includeDirectories[ 0 ] = LDX12_RING_SHADER_DIRECTORY;
			ComputePipelineState bothPipeline = device.CreateComputePipeline( pipelineDesc );
			pipelineDesc.computeShader.entryPoint = "ReadReset";
			ComputePipelineState resetPipeline = device.CreateComputePipeline( pipelineDesc );

			BufferDesc outputDesc{};
			outputDesc.type = BufferType::Raw;
			outputDesc.size = 64;
			outputDesc.unorderedAccess = true;
			const BufferHandle output = device.CreateBuffer( outputDesc );

			const CD3DX12_HEAP_PROPERTIES heap( D3D12_HEAP_TYPE_READBACK );
			const D3D12_RESOURCE_DESC readbackDesc = BufferResource::CreateNativeDesc( outputDesc.size );
			ComPtr<ID3D12Resource> readback;
			C_RESULT( native.GetDevice()->CreateCommittedResource( &heap,
						  D3D12_HEAP_FLAG_NONE,
						  &readbackDesc,
						  D3D12_RESOURCE_STATE_COPY_DEST,
						  nullptr,
						  IID_PPV_ARGS( readback.GetAddressOf() ) ),
				"Readback creation failed." );

			const std::array<uint32_t, 4> firstData{ 10, 11, 12, 13 };
			const std::array<uint32_t, 4> secondData{ 20, 21, 22, 23 };
			const std::array<uint32_t, 4> updatedFirst{ 30, 31, 32, 33 };
			const std::array<uint32_t, 4> updatedSecond{ 40, 41, 42, 43 };
			std::array<uint32_t, ourMaxPushConstant32BitValues> outputBinding{ device.GetUnorderedAccessIndex( output ), 0 };

			CommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdTransitionBuffer( output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
			commands.CmdBindComputePipeline( bothPipeline );
			commands.SetConstantBuffer( firstData );
			commands.SetConstantBuffer( secondData.data(), sizeof( secondData ), 1 );
			commands.CmdPushConstants( outputBinding );
			commands.CmdDispatch( 1 );
			commands.CmdUavBarrier( output );

			commands.SetConstantBuffer( updatedFirst, 0 );
			outputBinding[ 1 ] = 16;
			commands.CmdPushConstants( outputBinding );
			commands.CmdDispatch( 1 );
			commands.CmdUavBarrier( output );

			commands.SetConstantBuffer( updatedSecond, 1 );
			outputBinding[ 1 ] = 32;
			commands.CmdPushConstants( outputBinding );
			commands.CmdDispatch( 1 );

			// A real pending public submission must own the ranges after returning.
			ComPtr<ID3D12Fence> gate;
			C_RESULT( native.GetDevice()->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( gate.GetAddressOf() ) ), "Gate creation failed." );
			DeviceManager::DeferredRelease::OnFailure releaseGate( [ &gate ]() noexcept { gate->Signal( 1 ); } );
			C_RESULT( native.GetCommandQueue()->Wait( gate.Get(), 1 ), "Queue gate failed." );
			const SubmitHandle firstSubmission = device.Submit( commands );
			manager.ProcessDeferredReleases();
			Require( !device.IsReady( firstSubmission ), "Gated submission completed early." );
			Require( manager.graphicsQueue_.deferredReleases_.size() == 1, "Public Submit did not defer the ranges." );
			Require( manager.constantBufferRings_[ 0 ].ranges_.size() == 2, "Pending ring 0 data was retired." );
			Require( manager.constantBufferRings_[ 1 ].ranges_.size() == 2, "Pending ring 1 data was retired." );

			CommandBuffer& reused = device.AcquireCommandBuffer();
			Require( &reused == &commands, "Command buffer was not reused." );
			reused.CmdTransitionBuffer( output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
			reused.CmdUavBarrier( output );
			reused.CmdBindComputePipeline( resetPipeline );
			reused.SetConstantBuffer( firstData, 0 );
			outputBinding[ 1 ] = 48;
			reused.CmdPushConstants( outputBinding );
			reused.CmdDispatch( 1 );
			reused.CmdTransitionBuffer( output, D3D12_RESOURCE_STATE_COPY_SOURCE );
			native.GetCommandList( reused )->CopyBufferRegion( readback.Get(), 0, native.GetResource( output ), 0, outputDesc.size );
			CommandBuffer* batch[] = { &reused };
			const SubmitHandle secondSubmission = device.SubmitBatch( batch, 1 );
			Require( manager.graphicsQueue_.deferredReleases_.size() == 2, "Second recording did not own its own task." );

			C_RESULT( gate->Signal( 1 ), "Gate release failed." );
			device.Wait( secondSubmission );
			Require( manager.constantBufferRings_[ 0 ].ranges_.empty(), "Public submit did not reclaim ring 0." );
			Require( manager.constantBufferRings_[ 1 ].ranges_.empty(), "Public submit did not reclaim ring 1." );

			void* mapped = nullptr;
			const D3D12_RANGE readRange{ 0, 64 };
			C_RESULT( readback->Map( 0, &readRange, &mapped ), "Readback mapping failed." );
			std::array<uint32_t, 16> actual{};
			std::memcpy( actual.data(), mapped, sizeof( actual ) );
			const D3D12_RANGE noWrites{ 0, 0 };
			readback->Unmap( 0, &noWrites );
			const std::array<uint32_t, 16> expected{ 10, 20, 13, 23, 30, 20, 33, 23, 30, 40, 33, 43, 10, 13, 0, 0 };
			Require( actual == expected, "GPU constants changed across dispatches or Begin retained the previous ring 1 offset." );
			device.Destroy( output );
			std::cout << "[PASS] public SetConstantBuffer, header GPU loads, per-dispatch offsets, packed offset preservation, Begin reset and actual Submit "
						 "retirement\n";
		}
	};
}

int main()
{
	try
	{
		ldx12::tests::DeviceManagerGuard guard;
		ldx12::ContextDesc context{};
		context.enableDebugLayer = true;
		ldx12::DeviceManager& manager = ldx12::DeviceManager::Initialize( context );
		guard.active = true;
		ldx12::DeferredReleaseTestAccess::Run( manager );
		return 0;
	}
	catch( const std::exception& error )
	{
		std::cerr << "[FAIL] " << error.what() << '\n';
		return 1;
	}
}
