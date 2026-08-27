#include <Ldx12/Ldx12.hpp>
#include <Ldx12/Ldx12Native.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>

int main()
{
	try
	{
		ldx12::ContextDesc context{};
		context.preferHighPerformanceAdapter = false;

		ldx12::DeviceManager& manager = ldx12::DeviceManager::Initialize( context );
		ldx12::RenderDevice& device = *manager.GetRenderDevice();
		ldx12::D3D12Native native = device.GetNative();
		if( native.GetDevice() == nullptr || native.GetCommandQueue() == nullptr )
		{
			throw std::runtime_error( "Installed native D3D12 access is unavailable." );
		}

		ldx12::BufferDesc bufferDesc{};
		bufferDesc.debugName = "Installed Ldx12 example buffer";
		bufferDesc.size = 256;
		bufferDesc.memory = ldx12::BufferMemory::CpuToGpu;

		const ldx12::BufferHandle buffer = device.CreateBuffer( bufferDesc );
		if( native.GetResource( buffer ) == nullptr )
		{
			throw std::runtime_error( "Installed native D3D12 resource access is unavailable." );
		}
		device.Destroy( buffer );
		device.WaitIdle();
		ldx12::DeviceManager::ShutdownSingleton();

		std::cout << "Ldx12 installed package is working.\n";
		return 0;
	}
	catch( const std::exception& exception )
	{
		ldx12::DeviceManager::ShutdownSingleton();
		std::cerr << "Ldx12 initialization failed: " << exception.what() << '\n';
		return 1;
	}
}
