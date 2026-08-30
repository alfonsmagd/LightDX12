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

		SamplerDesc samplerDesc{};
		std::array<SamplerHandle, ourCustomSamplerCount> samplers{};
		for( uint32_t index = 0; index < ourCustomSamplerCount; ++index )
		{
			samplers[ index ] = device.CreateSampler( samplerDesc );
			Require( device.GetSamplerIndex( samplers[ index ] ) == LDX12_CUSTOM_SAMPLER_SLOT_FIRST + index,
				"Custom sampler was not created in its reserved descriptor slot." );
		}

		bool samplerLimitReached = false;
		try
		{
			device.CreateSampler( samplerDesc );
		}
		catch( const std::length_error& )
		{
			samplerLimitReached = true;
		}
		Require( samplerLimitReached, "More than four runtime custom samplers were created." );

		const uint32_t recycledSamplerIndex = device.GetSamplerIndex( samplers[ 1 ] );
		device.Destroy( samplers[ 1 ] );
		Require( !device.IsAlive( samplers[ 1 ] ), "Destroyed sampler handle remained alive." );
		samplers[ 1 ] = device.CreateSampler( samplerDesc );
		Require( device.GetSamplerIndex( samplers[ 1 ] ) == recycledSamplerIndex, "Destroyed custom sampler slot was not recycled." );

		const std::array<uint32_t, 16> initialBufferData = {};
		BufferDesc genericBufferDesc{};
		genericBufferDesc.debugName = "Ldx12Tests generic buffer";
		genericBufferDesc.size = 64;
		genericBufferDesc.memory = BufferMemory::CpuToGpu;
		genericBufferDesc.initialData = initialBufferData.data();
		const BufferHandle genericBuffer = device.CreateBuffer( genericBufferDesc );
		Require( genericBuffer.Valid(), "Generic buffer creation returned an invalid handle." );
		ID3D12Resource* genericNativeResource = native.GetResource( genericBuffer );
		Require( genericNativeResource != nullptr, "Buffer does not expose a native D3D12 resource." );
		D3D12_HEAP_PROPERTIES genericHeapProperties{};
		D3D12_HEAP_FLAGS genericHeapFlags = D3D12_HEAP_FLAG_NONE;
		Require( SUCCEEDED( genericNativeResource->GetHeapProperties( &genericHeapProperties, &genericHeapFlags ) ) &&
					 genericHeapProperties.Type == D3D12_HEAP_TYPE_UPLOAD,
			"CpuToGpu buffer was not created in an upload heap." );
		Require( device.GetBindlessIndex( genericBuffer ) == LDX12_DESCRIPTOR_SLOT_INVALID,
			"A buffer without an SRV unexpectedly owns a bindless descriptor." );
		Require( device.GetConstantBufferIndex( genericBuffer ) == LDX12_DESCRIPTOR_SLOT_INVALID,
			"A buffer without a CBV unexpectedly owns a constant-buffer descriptor." );

		const std::array<uint32_t, 4> updateData = { 11u, 22u, 33u, 44u };
		device.WriteBuffer( genericBuffer, 16u, updateData.data(), sizeof( updateData ) );
		RequireThrows<std::runtime_error>( [ &device, genericBuffer, &updateData ]
			{ device.WriteBuffer( genericBuffer, 60u, updateData.data(), sizeof( updateData ) ); },
			"WriteBuffer accepted a range outside the resource." );

		BufferDesc structuredBufferDesc{};
		structuredBufferDesc.debugName = "Ldx12Tests structured buffer";
		structuredBufferDesc.size = 128;
		structuredBufferDesc.stride = 16;
		structuredBufferDesc.type = BufferType::Structured;
		const BufferHandle structuredBuffer = device.CreateBuffer( structuredBufferDesc );
		const uint32_t structuredSrv = device.GetBindlessIndex( structuredBuffer );
		Require( structuredBuffer.Valid(), "Structured buffer creation returned an invalid handle." );
		D3D12_HEAP_PROPERTIES structuredHeapProperties{};
		D3D12_HEAP_FLAGS structuredHeapFlags = D3D12_HEAP_FLAG_NONE;
		Require( SUCCEEDED( native.GetResource( structuredBuffer )->GetHeapProperties( &structuredHeapProperties, &structuredHeapFlags ) ) &&
					 structuredHeapProperties.Type == D3D12_HEAP_TYPE_DEFAULT,
			"GpuLocal buffer was not created in a default heap." );
		Require( structuredSrv >= LDX12_BINDLESS_DYNAMIC_SLOT_FIRST && structuredSrv < context.bindlessCapacity,
			"Structured buffer SRV index is outside the bindless heap." );

		BufferDesc constantBufferDesc{};
		constantBufferDesc.debugName = "Ldx12Tests constant buffer";
		constantBufferDesc.size = 100;
		constantBufferDesc.type = BufferType::Constant;
		constantBufferDesc.memory = BufferMemory::CpuToGpu;
		const BufferHandle constantBuffer = device.CreateBuffer( constantBufferDesc, ConstantBufferSlot::FreeCB0 );
		Require( device.GetConstantBufferIndex( constantBuffer ) == ToSlotIndex( ConstantBufferSlot::FreeCB0 ),
			"Constant buffer was not created in its requested fixed slot." );
		Require( native.GetResource( constantBuffer )->GetDesc().Width == 256, "Constant buffer allocation was not aligned to 256 bytes." );

		BufferDesc rawBufferDesc{};
		rawBufferDesc.debugName = "Ldx12Tests raw buffer";
		rawBufferDesc.size = 64;
		rawBufferDesc.type = BufferType::Raw;
		const BufferHandle rawBuffer = device.CreateBuffer( rawBufferDesc );
		Require( device.GetBindlessIndex( rawBuffer ) != LDX12_DESCRIPTOR_SLOT_INVALID, "Raw buffer did not receive a shader-resource descriptor." );

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
		Require( textureSrv >= LDX12_BINDLESS_DYNAMIC_SLOT_FIRST && textureSrv < context.bindlessCapacity, "Texture SRV index is outside the bindless heap." );
		Require( textureUav >= LDX12_BINDLESS_DYNAMIC_SLOT_FIRST && textureUav < context.bindlessCapacity && textureUav != textureSrv,
			"Texture UAV index is invalid or aliases its SRV." );

		ID3D12Resource* nativeTexture = native.GetResource( texture );
		Require( nativeTexture != nullptr, "Texture does not expose a native D3D12 resource." );
		const D3D12_RESOURCE_DESC nativeTextureDesc = nativeTexture->GetDesc();
		Require( nativeTextureDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D, "Native texture dimension is incorrect." );
		Require( nativeTextureDesc.Width == textureDesc.width && nativeTextureDesc.Height == textureDesc.height,
			"Native texture dimensions differ from TextureDesc." );
		Require( nativeTextureDesc.MipLevels == textureDesc.countMipMap, "Native texture mip count differs from TextureDesc." );
		Require( nativeTextureDesc.Format == textureDesc.format, "Native texture format differs from TextureDesc." );
		Require( ( nativeTextureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS ) != 0, "Native UAV texture is missing its D3D12 resource flag." );

		const std::array<uint32_t, 8> arrayPixels = { 0xffff0000u, 0xffff0000u, 0xffff0000u, 0xffff0000u, 0xff00ff00u, 0xff00ff00u, 0xff00ff00u, 0xff00ff00u };
		TextureDesc arrayTextureDesc{};
		arrayTextureDesc.debugName = "Ldx12Tests Texture2DArray";
		arrayTextureDesc.width = 2;
		arrayTextureDesc.height = 2;
		arrayTextureDesc.depthOrArraySize = 2;
		arrayTextureDesc.dimension = TextureDimension::Texture2DArray;
		arrayTextureDesc.data = arrayPixels.data();
		arrayTextureDesc.rowPitch = 2 * sizeof( uint32_t );
		arrayTextureDesc.slicePitch = arrayTextureDesc.rowPitch * 2;
		const TextureHandle arrayTexture = device.CreateTexture( arrayTextureDesc );
		const D3D12_RESOURCE_DESC nativeArrayDesc = native.GetResource( arrayTexture )->GetDesc();
		Require( nativeArrayDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && nativeArrayDesc.DepthOrArraySize == arrayTextureDesc.depthOrArraySize,
			"Texture2DArray did not create the requested native array slices." );
		Require( device.GetBindlessIndex( arrayTexture ) != LDX12_DESCRIPTOR_SLOT_INVALID, "Texture2DArray did not receive a bindless SRV." );

		std::array<uint32_t, ourCubeMapFaceCount * 4> cubePixels{};
		for( uint32_t face = 0; face < ourCubeMapFaceCount; ++face )
		{
			for( uint32_t pixel = 0; pixel < 4; ++pixel )
			{
				cubePixels[ face * 4 + pixel ] = 0xff000000u | ( face + 1u ) * 0x00202020u;
			}
		}
		TextureDesc cubeTextureDesc{};
		cubeTextureDesc.debugName = "Ldx12Tests TextureCube";
		cubeTextureDesc.width = 2;
		cubeTextureDesc.height = 2;
		cubeTextureDesc.depthOrArraySize = ourCubeMapFaceCount;
		cubeTextureDesc.dimension = TextureDimension::TextureCube;
		cubeTextureDesc.data = cubePixels.data();
		cubeTextureDesc.rowPitch = 2 * sizeof( uint32_t );
		cubeTextureDesc.slicePitch = cubeTextureDesc.rowPitch * 2;
		const TextureHandle cubeTexture = device.CreateTexture( cubeTextureDesc );
		const D3D12_RESOURCE_DESC nativeCubeDesc = native.GetResource( cubeTexture )->GetDesc();
		Require( nativeCubeDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && nativeCubeDesc.DepthOrArraySize == ourCubeMapFaceCount,
			"TextureCube did not create its six native array slices." );
		Require( device.GetBindlessIndex( cubeTexture ) != LDX12_DESCRIPTOR_SLOT_INVALID, "TextureCube did not receive a bindless SRV." );

		TextureDesc invalidTextureDesc{};
		invalidTextureDesc.usage = TextureUsage::RenderTarget | TextureUsage::DepthStencil;
		RequireThrows<std::runtime_error>( [ &device, &invalidTextureDesc ] { device.CreateTexture( invalidTextureDesc ); },
			"Texture creation accepted incompatible render-target and depth usage." );

		const uint32_t oldTextureIndex = texture.Index();
		const uint32_t oldTextureGeneration = texture.Gen();
		device.WaitIdle();
		device.Destroy( texture );
		const TextureHandle replacementTexture = device.CreateTexture( textureDesc );
		Require( replacementTexture.Index() == oldTextureIndex, "Texture destruction did not release its SlotMap entry." );
		Require( replacementTexture.Gen() != oldTextureGeneration, "Recreated texture did not receive a new handle generation." );
		Require( device.GetBindlessIndex( replacementTexture ) == textureSrv, "Recreated texture did not recycle the released SRV descriptor." );
		Require( device.GetUnorderedAccessIndex( replacementTexture ) == textureUav, "Recreated texture did not recycle the released UAV descriptor." );

		const uint32_t oldBufferIndex = structuredBuffer.Index();
		const uint32_t oldBufferGeneration = structuredBuffer.Gen();
		device.Destroy( structuredBuffer );
		const BufferHandle replacementBuffer = device.CreateBuffer( structuredBufferDesc );
		Require( replacementBuffer.Index() == oldBufferIndex, "Buffer destruction did not release its SlotMap entry." );
		Require( replacementBuffer.Gen() != oldBufferGeneration, "Recreated buffer did not receive a new handle generation." );

		device.Destroy( replacementBuffer );
		device.Destroy( replacementTexture );
		device.Destroy( cubeTexture );
		device.Destroy( arrayTexture );
		device.Destroy( rawBuffer );
		device.Destroy( constantBuffer );
		const BufferHandle reusedConstantBuffer = device.CreateBuffer( constantBufferDesc, ConstantBufferSlot::FreeCB0 );
		Require( device.GetConstantBufferIndex( reusedConstantBuffer ) == ToSlotIndex( ConstantBufferSlot::FreeCB0 ),
			"Destroyed fixed descriptor slot could not be reused." );
		device.Destroy( reusedConstantBuffer );
		device.Destroy( genericBuffer );
		for( SamplerHandle sampler : samplers )
		{
			device.Destroy( sampler );
		}
		device.WaitIdle();
	}
}
