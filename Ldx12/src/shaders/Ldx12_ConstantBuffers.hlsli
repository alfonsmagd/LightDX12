#ifndef LDX12_CONSTANT_BUFFERS_HLSLI
#define LDX12_CONSTANT_BUFFERS_HLSLI

#include "Ldx12_Defines.hlsli"

// Reserved root constant, updated automatically by Ldx12.
cbuffer Ldx12ConstantBinding : register( b2 )
{
	uint ldx12ConstantBufferOffsets;
};

struct Ldx12ConstantBuffer
{
	uint descriptorIndex_;
	uint offsetShift_;

	template <typename T> T Load()
	{
		const uint offset = ( ldx12ConstantBufferOffsets >> offsetShift_ ) & 0xffffu;
		ByteAddressBuffer buffer = ResourceDescriptorHeap[ descriptorIndex_ ];

		return buffer.Load<T>( offset );
	}
};

static const Ldx12ConstantBuffer CB0 = { LDX12_CBV_SLOT_ENGINE_RINGBUFFER0, 0u };
static const Ldx12ConstantBuffer CB1 = { LDX12_CBV_SLOT_ENGINE_RINGBUFFER1, 16u };

#endif
