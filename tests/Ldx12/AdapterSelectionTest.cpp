#include "TestTemplate.hpp"

#include <iostream>

namespace ldx12::tests
{
	void TestGpuAdapterSelection()
	{
		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = false;
		contextDesc.preferHighPerformanceAdapter = false;
		contextDesc.preferredAdapterVendor = LDX12_GPU_VENDOR_DEFAULT;
		contextDesc.adapterIndex = UINT32_MAX;

		DeviceManager& defaultManager = DeviceManager::Initialize( contextDesc );
		const DeviceProperties defaultProperties = defaultManager.GetDeviceProperties();

		DeviceManager::ShutdownSingleton();

		std::cout << "Default adapter: " << defaultProperties.adapterName << '\n'
				  << "  Dedicated video memory: " << defaultProperties.dedicatedVideoMemoryBytes / ( 1024 * 1024 ) << " MiB\n"
				  << "  Dedicated system memory: " << defaultProperties.dedicatedSystemMemoryBytes / ( 1024 * 1024 ) << " MiB\n"
				  << "  Shared system memory: " << defaultProperties.sharedSystemMemoryBytes / ( 1024 * 1024 ) << " MiB\n";

		if( defaultProperties.shaderModel < D3D_SHADER_MODEL_6_6 )
		{
			throw std::runtime_error( "The selected device does not support SM 6.6." );
		}

		contextDesc.adapterIndex = UINT32_MAX - 1;

		DeviceManager& fallbackManager = DeviceManager::Initialize( contextDesc );
		const DeviceProperties fallbackProperties = fallbackManager.GetDeviceProperties();

		DeviceManager::ShutdownSingleton();

		if( fallbackProperties.adapterLuid.LowPart != defaultProperties.adapterLuid.LowPart ||
			fallbackProperties.adapterLuid.HighPart != defaultProperties.adapterLuid.HighPart )
		{
			throw std::runtime_error( "An invalid index did not fall back to the default adapter." );
		}

		std::cout << "Invalid index selected the default adapter: " << fallbackProperties.adapterName << '\n';
	}
}
