#include "TestTemplate.hpp"
#include "Ldx12/Ldx12Native.hpp"

#include <array>

namespace ldx12::tests
{
	void TestGpuResourceLifecycleAndProperties()
	{
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;
		context.framesInFlight = 3;
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

		BufferDesc genericBufferDesc{};
		genericBufferDesc.debugName = "Ldx12Tests generic buffer";
		genericBufferDesc.size = 64;
		genericBufferDesc.heapType = D3D12_HEAP_TYPE_UPLOAD;
		const BufferHandle genericBuffer = device.CreateBuffer( genericBufferDesc );
		Require( genericBuffer.Valid(), "Generic buffer creation returned an invalid handle." );
		Require( native.GetResource( genericBuffer ) != nullptr,
			"Buffer does not expose a native D3D12 resource." );
		Require( device.GetBindlessIndex( genericBuffer ) == LDX12_DESCRIPTOR_SLOT_INVALID,
			"A buffer without an SRV unexpectedly owns a bindless descriptor." );
		Require( device.GetConstantBufferIndex( genericBuffer ) == LDX12_DESCRIPTOR_SLOT_INVALID,
			"A buffer without a CBV unexpectedly owns a constant-buffer descriptor." );

		const std::array<uint32_t, 4> updateData = { 11u, 22u, 33u, 44u };
		device.WriteBuffer( genericBuffer, 16u, updateData.data(), sizeof( updateData ) );
		RequireThrows<std::runtime_error>(
			[&device, genericBuffer, &updateData]
			{
				device.WriteBuffer( genericBuffer, 60u, updateData.data(), sizeof( updateData ) );
			},
			"WriteBuffer accepted a range outside the resource." );

		BufferDesc structuredBufferDesc{};
		structuredBufferDesc.debugName = "Ldx12Tests structured buffer";
		structuredBufferDesc.size = 128;
		structuredBufferDesc.stride = 16;
		structuredBufferDesc.createShaderResourceView = true;
		const BufferHandle structuredBuffer = device.CreateBuffer( structuredBufferDesc );
		const uint32_t structuredSrv = device.GetBindlessIndex( structuredBuffer );
		Require( structuredBuffer.Valid(), "Structured buffer creation returned an invalid handle." );
		Require( structuredSrv >= LDX12_BINDLESS_DYNAMIC_SLOT_FIRST &&
			structuredSrv < context.bindlessCapacity,
			"Structured buffer SRV index is outside the bindless heap." );

		BufferDesc constantBufferDesc{};
		constantBufferDesc.debugName = "Ldx12Tests constant buffer";
		constantBufferDesc.size = 100;
		constantBufferDesc.heapType = D3D12_HEAP_TYPE_UPLOAD;
		const BufferHandle constantBuffer =
			device.CreateBuffer( constantBufferDesc, ConstantBufferSlot::FreeCB0 );
		Require( device.GetConstantBufferIndex( constantBuffer ) ==
			ToSlotIndex( ConstantBufferSlot::FreeCB0 ),
			"Constant buffer was not created in its requested fixed slot." );

		TextureDesc textureDesc{};
		textureDesc.debugName = "Ldx12Tests sampled UAV texture";
		textureDesc.width = 32;
		textureDesc.height = 16;
		textureDesc.countMipMap = 3;
		textureDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.usage = TextureUsage::Sampled | TextureUsage::UnorderedAccess;
		const TextureHandle texture = device.CreateTexture( textureDesc );
		Require( texture.Valid(), "Texture creation returned an invalid handle." );
		const uint32_t textureSrv = device.GetBindlessIndex( texture );
		const uint32_t textureUav = device.GetUnorderedAccessIndex( texture );
		Require( textureSrv >= LDX12_BINDLESS_DYNAMIC_SLOT_FIRST &&
			textureSrv < context.bindlessCapacity,
			"Texture SRV index is outside the bindless heap." );
		Require( textureUav >= LDX12_BINDLESS_DYNAMIC_SLOT_FIRST &&
			textureUav < context.bindlessCapacity && textureUav != textureSrv,
			"Texture UAV index is invalid or aliases its SRV." );

		ID3D12Resource* nativeTexture = native.GetResource( texture );
		Require( nativeTexture != nullptr, "Texture does not expose a native D3D12 resource." );
		const D3D12_RESOURCE_DESC nativeTextureDesc = nativeTexture->GetDesc();
		Require( nativeTextureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D,
			"Native texture dimension is incorrect." );
		Require( nativeTextureDesc.Width == textureDesc.width &&
			nativeTextureDesc.Height == textureDesc.height,
			"Native texture dimensions differ from TextureDesc." );
		Require( nativeTextureDesc.MipLevels == textureDesc.countMipMap,
			"Native texture mip count differs from TextureDesc." );
		Require( nativeTextureDesc.Format == textureDesc.format,
			"Native texture format differs from TextureDesc." );
		Require( ( nativeTextureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS ) != 0,
			"Native UAV texture is missing its D3D12 resource flag." );

		TextureDesc invalidTextureDesc{};
		invalidTextureDesc.usage = TextureUsage::RenderTarget | TextureUsage::DepthStencil;
		RequireThrows<std::runtime_error>(
			[&device, &invalidTextureDesc] { device.CreateTexture( invalidTextureDesc ); },
			"Texture creation accepted incompatible render-target and depth usage." );

		const uint32_t oldTextureIndex = texture.Index();
		const uint32_t oldTextureGeneration = texture.Gen();
		device.WaitIdle();
		device.Destroy( texture );
		const TextureHandle replacementTexture = device.CreateTexture( textureDesc );
		Require( replacementTexture.Index() == oldTextureIndex,
			"Texture destruction did not release its SlotMap entry." );
		Require( replacementTexture.Gen() != oldTextureGeneration,
			"Recreated texture did not receive a new handle generation." );
		Require( device.GetBindlessIndex( replacementTexture ) == textureSrv,
			"Recreated texture did not recycle the released SRV descriptor." );
		Require( device.GetUnorderedAccessIndex( replacementTexture ) == textureUav,
			"Recreated texture did not recycle the released UAV descriptor." );

		const uint32_t oldBufferIndex = structuredBuffer.Index();
		const uint32_t oldBufferGeneration = structuredBuffer.Gen();
		device.Destroy( structuredBuffer );
		const BufferHandle replacementBuffer = device.CreateBuffer( structuredBufferDesc );
		Require( replacementBuffer.Index() == oldBufferIndex,
			"Buffer destruction did not release its SlotMap entry." );
		Require( replacementBuffer.Gen() != oldBufferGeneration,
			"Recreated buffer did not receive a new handle generation." );

		device.Destroy( replacementBuffer );
		device.Destroy( replacementTexture );
		device.Destroy( constantBuffer );
		const BufferHandle reusedConstantBuffer =
			device.CreateBuffer( constantBufferDesc, ConstantBufferSlot::FreeCB0 );
		Require( device.GetConstantBufferIndex( reusedConstantBuffer ) ==
			ToSlotIndex( ConstantBufferSlot::FreeCB0 ),
			"Destroyed fixed descriptor slot could not be reused." );
		device.Destroy( reusedConstantBuffer );
		device.Destroy( genericBuffer );
		device.WaitIdle();
	}
}
