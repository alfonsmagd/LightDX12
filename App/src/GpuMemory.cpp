#include "App/GpuMemory.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace App
{
	GpuMemoryInfo QueryGpuMemory( ID3D12Device* device )
	{
		GpuMemoryInfo result;
		if( !device ) return result;
		Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
		Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
		Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter3;
		if( FAILED( device->QueryInterface( IID_PPV_ARGS( &dxgiDevice ) ) ) ) return result;
		if( FAILED( dxgiDevice->GetAdapter( &adapter ) ) ) return result;
		if( FAILED( adapter.As( &adapter3 ) ) ) return result;
		DXGI_QUERY_VIDEO_MEMORY_INFO info{};
		if( FAILED( adapter3->QueryVideoMemoryInfo( 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info ) ) ) return result;
		result.localUsage = info.CurrentUsage;
		result.localBudget = info.Budget;
		result.valid = true;
		return result;
	}
}
