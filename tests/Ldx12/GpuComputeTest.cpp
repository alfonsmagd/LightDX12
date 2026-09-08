#include "TestTemplate.hpp"
#include "Ldx12/Ldx12Native.hpp"

#include <array>
#include <iostream>
#include <string>

namespace ldx12::tests
{
	void TestGpuComputeUav()
	{
		DeviceManagerGuard guard;
		ContextDesc context{};
		context.enableDebugLayer = true;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();
		D3D12Native native = device.GetNative();
		// Deliberately leave a partial final thread group to exercise shader bounds checks.
		constexpr uint32_t count = 257;
		constexpr uint32_t groups = ( count + 63 ) / 64;
		const char* shader = R"(
cbuffer Constants : register(b0)
{
    uint inputUav;
    uint inputSrv;
    uint outputUav;
    uint count;
};
[numthreads(64, 1, 1)]
void Seed(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= count) return;
    RWStructuredBuffer<uint> values = ResourceDescriptorHeap[inputUav];
    values[id.x] = id.x * 3 + 7;
}
[numthreads(64, 1, 1)]
void Update(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= count) return;
    RWStructuredBuffer<uint> values = ResourceDescriptorHeap[inputUav];
    values[id.x] = values[id.x] * 2 + 1;
}
[numthreads(64, 1, 1)]
void Copy(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= count) return;
    StructuredBuffer<uint> values = ResourceDescriptorHeap[inputSrv];
    RWByteAddressBuffer result = ResourceDescriptorHeap[outputUav];
    result.Store(id.x * 4, values[count - 1 - id.x]);
}
)";
		ComputePipelineDesc pipelineDesc{};
		pipelineDesc.computeShader.source = shader;
		pipelineDesc.computeShader.entryPoint = "Seed";
		ComputePipelineState seed = device.CreateComputePipeline( pipelineDesc );
		pipelineDesc.computeShader.entryPoint = "Update";
		ComputePipelineState update = device.CreateComputePipeline( pipelineDesc );
		pipelineDesc.computeShader.entryPoint = "Copy";
		ComputePipelineState copy = device.CreateComputePipeline( pipelineDesc );
		Require( seed.Valid() && update.Valid() && copy.Valid(), "Compute pipelines were not created." );

		BufferDesc desc{};
		desc.debugName = "Ldx12Tests compute structured input";
		desc.size = count * sizeof( uint32_t );
		desc.type = BufferType::Structured;
		desc.stride = sizeof( uint32_t );
		desc.unorderedAccess = true;
		const BufferHandle input = device.CreateBuffer( desc );
		desc.debugName = "Ldx12Tests compute raw output";
		desc.type = BufferType::Raw;
		desc.stride = 0;
		const BufferHandle output = device.CreateBuffer( desc );
		const std::array<uint32_t, 4> constants = { device.GetUnorderedAccessIndex( input ),
			device.GetBindlessIndex( input ),
			device.GetUnorderedAccessIndex( output ),
			count };
		Require( constants[ 0 ] != LDX12_DESCRIPTOR_SLOT_INVALID && constants[ 1 ] != LDX12_DESCRIPTOR_SLOT_INVALID &&
				constants[ 2 ] != LDX12_DESCRIPTOR_SLOT_INVALID,
			"Compute buffers did not receive valid UAV/SRV descriptors." );
		Require( constants[ 0 ] != constants[ 1 ] && constants[ 0 ] != constants[ 2 ] && constants[ 1 ] != constants[ 2 ],
			"Compute UAV/SRV descriptors must occupy distinct slots." );

		// Native readback is local to this test; dispatch and barriers use Ldx12.
		D3D12_HEAP_PROPERTIES heap{};
		heap.Type = D3D12_HEAP_TYPE_READBACK;
		D3D12_RESOURCE_DESC readbackDesc{};
		readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		readbackDesc.Width = desc.size;
		readbackDesc.Height = 1;
		readbackDesc.DepthOrArraySize = 1;
		readbackDesc.MipLevels = 1;
		readbackDesc.SampleDesc.Count = 1;
		readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		Microsoft::WRL::ComPtr<ID3D12Resource> readback;
		Require( SUCCEEDED( native.GetDevice()->CreateCommittedResource( &heap,
					 D3D12_HEAP_FLAG_NONE,
					 &readbackDesc,
					 D3D12_RESOURCE_STATE_COPY_DEST,
					 nullptr,
					 IID_PPV_ARGS( readback.GetAddressOf() ) ) ),
			"Compute test readback creation failed." );

		CommandBuffer& producer = device.AcquireCommandBuffer();
		producer.CmdTransitionBuffer( input, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
		producer.CmdPushConstants( constants.data(), sizeof( constants ) );
		producer.CmdBindComputePipeline( seed );
		producer.CmdDispatch( groups );
		producer.CmdUavBarrier( input );
		producer.CmdBindComputePipeline( update );
		producer.CmdDispatch( groups );

		// Record both lists before submission to exercise buffer-state fixups.
		CommandBuffer& consumer = device.AcquireCommandBuffer();
		consumer.CmdTransitionBuffer( input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
		consumer.CmdTransitionBuffer( output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
		consumer.CmdPushConstants( constants.data(), sizeof( constants ) );
		consumer.CmdBindComputePipeline( copy );
		consumer.CmdDispatch( groups );
		consumer.CmdTransitionBuffer( output, D3D12_RESOURCE_STATE_COPY_SOURCE );
		native.GetCommandList( consumer )->CopyBufferRegion( readback.Get(), 0, native.GetResource( output ), 0, desc.size );
		CommandBuffer* batch[] = { &producer, &consumer };
		const SubmitHandle submission = device.SubmitBatch( batch, 2 );
		Require( !submission.Empty(), "Compute batch returned an empty submission handle." );
		device.Wait( submission );
		Require( device.IsReady( submission ), "Compute batch did not complete after waiting." );

		void* mapped = nullptr;
		const D3D12_RANGE range = { 0, static_cast<SIZE_T>( desc.size ) };
		Require( SUCCEEDED( readback->Map( 0, &range, &mapped ) ), "Compute test readback mapping failed." );
		std::array<uint32_t, count> actual{};
		const uint32_t* values = static_cast<const uint32_t*>( mapped );
		for( uint32_t index = 0; index < count; ++index )
		{
			actual[ index ] = values[ index ];
		}
		const D3D12_RANGE noWrites = { 0, 0 };
		readback->Unmap( 0, &noWrites );
		device.Destroy( input );
		device.Destroy( output );
		device.WaitIdle();

		for( uint32_t index = 0; index < count; ++index )
		{
			const uint32_t expected = ( ( count - 1 - index ) * 3 + 7 ) * 2 + 1;
			if( actual[ index ] != expected )
			{
				throw std::runtime_error( "Compute readback mismatch at index " + std::to_string( index ) +
					": GPU value " + std::to_string( actual[ index ] ) + ", expected " + std::to_string( expected ) + "." );
			}
		}
		std::cout << "Compute readback verified: " << count << " values across " << groups << " thread groups.\n";
	}
}
