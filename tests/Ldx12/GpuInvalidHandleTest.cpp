#include "TestTemplate.hpp"
#include "Ldx12/Ldx12Native.hpp"

namespace ldx12::tests
{
	void TestGpuInvalidHandleSafety()
	{
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;
		context.bindlessCapacity = 64;
		context.rtvCapacity = 8;
		context.dsvCapacity = 4;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();
		D3D12Native native = device.GetNative();

		Require( !device.IsAlive( BufferHandle{} ),
			"An empty buffer handle was reported as alive." );
		Require( !device.IsAlive( TextureHandle{} ),
			"An empty texture handle was reported as alive." );
		Require( !device.Destroy( BufferHandle{} ),
			"Destroy reported success for an empty buffer handle." );
		Require( !device.Destroy( TextureHandle{} ),
			"Destroy reported success for an empty texture handle." );

		BufferDesc bufferDesc{};
		bufferDesc.debugName = "Ldx12Tests invalid-handle buffer";
		bufferDesc.size = 64;
		bufferDesc.memory = BufferMemory::CpuToGpu;
		const BufferHandle staleBuffer = device.CreateBuffer( bufferDesc );
		Require( device.IsAlive( staleBuffer ),
			"A newly created buffer was not reported as alive." );
		Require( device.Destroy( staleBuffer ),
			"Destroy failed for a live buffer handle." );
		Require( !device.IsAlive( staleBuffer ),
			"A destroyed buffer handle remains alive." );
		Require( !device.Destroy( staleBuffer ),
			"Double buffer destruction reported success." );

		const BufferHandle replacementBuffer = device.CreateBuffer( bufferDesc );
		Require( replacementBuffer.Index() == staleBuffer.Index() &&
			replacementBuffer.Gen() != staleBuffer.Gen(),
			"A replacement buffer did not reuse the slot with a new generation." );
		Require( !device.Destroy( staleBuffer ),
			"A stale buffer handle destroyed its replacement." );
		Require( device.IsAlive( replacementBuffer ),
			"A stale buffer operation invalidated the replacement." );
		RequireThrows<std::runtime_error>(
			[&device, staleBuffer] { device.GetBindlessIndex( staleBuffer ); },
			"A stale buffer handle was accepted by a resource query." );

		TextureDesc textureDesc{};
		textureDesc.debugName = "Ldx12Tests invalid-handle texture";
		textureDesc.width = 8;
		textureDesc.height = 8;
		textureDesc.usage = TextureUsage::Sampled;
		const TextureHandle staleTexture = device.CreateTexture( textureDesc );
		Require( device.IsAlive( staleTexture ),
			"A newly created texture was not reported as alive." );
		Require( device.Destroy( staleTexture ),
			"Destroy failed for a live texture handle." );
		Require( !device.IsAlive( staleTexture ),
			"A destroyed texture handle remains alive." );
		Require( !device.Destroy( staleTexture ),
			"Double texture destruction reported success." );

		const TextureHandle replacementTexture = device.CreateTexture( textureDesc );
		Require( replacementTexture.Index() == staleTexture.Index() &&
			replacementTexture.Gen() != staleTexture.Gen(),
			"A replacement texture did not reuse the slot with a new generation." );
		Require( !device.Destroy( staleTexture ),
			"A stale texture handle destroyed its replacement." );
		Require( device.IsAlive( replacementTexture ),
			"A stale texture operation invalidated the replacement." );
		RequireThrows<std::runtime_error>(
			[&native, staleTexture] { static_cast<void>( native.GetResource( staleTexture ) ); },
			"A stale texture handle was accepted by a resource query." );

		Require( device.Destroy( replacementBuffer ),
			"Cleanup failed for the replacement buffer." );
		Require( device.Destroy( replacementTexture ),
			"Cleanup failed for the replacement texture." );
		device.WaitIdle();
	}
}
