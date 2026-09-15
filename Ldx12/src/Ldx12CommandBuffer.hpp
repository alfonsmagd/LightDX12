#pragma once

#include "Ldx12Internal.hpp"

#include <array>

namespace ldx12
{
	struct CommandListWrapper final
	{
		ComPtr<ID3D12CommandAllocator> allocator_;
		ComPtr<ID3D12GraphicsCommandList4> commandList_;
		ComPtr<ID3D12Fence> fence_;
		SubmitHandle handle_{};
		HANDLE fenceEvent_ = nullptr;
		uint64_t fenceValue_ = 0;
		bool isEncoding_ = false;
	};

}
