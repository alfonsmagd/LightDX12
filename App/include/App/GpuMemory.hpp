#pragma once

#include <cstdint>

struct ID3D12Device;

namespace App
{
	struct GpuMemoryInfo
	{
		uint64_t localUsage = 0;
		uint64_t localBudget = 0;
		bool valid = false;
	};

	GpuMemoryInfo QueryGpuMemory( ID3D12Device* device );
}
