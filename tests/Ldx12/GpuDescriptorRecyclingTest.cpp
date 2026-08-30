#include "TestTemplate.hpp"

#include <array>

namespace ldx12::tests
{
	void TestGpuDescriptorRecycling()
	{
		constexpr uint32_t dynamicDescriptorCount = 8;
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;
		context.bindlessCapacity = LDX12_BINDLESS_DYNAMIC_SLOT_FIRST + dynamicDescriptorCount;
		context.rtvCapacity = 4;
		context.dsvCapacity = 3;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		TextureDesc sampledDesc{};
		sampledDesc.debugName = "Ldx12Tests recycled sampled texture";
		sampledDesc.width = 4;
		sampledDesc.height = 4;
		sampledDesc.usage = TextureUsage::Sampled;

		std::array<TextureHandle, dynamicDescriptorCount> sampledTextures{};
		std::array<uint32_t, dynamicDescriptorCount> sampledIndices{};
		for( uint32_t index = 0; index < dynamicDescriptorCount; ++index )
		{
			sampledTextures[ index ] = device.CreateTexture( sampledDesc );
			sampledIndices[ index ] = device.GetBindlessIndex( sampledTextures[ index ] );
			Require( sampledIndices[ index ] == LDX12_BINDLESS_DYNAMIC_SLOT_FIRST + index,
				"Dynamic SRV allocation did not use the expected contiguous range." );
		}

		RequireThrows<std::runtime_error>( [ &device, &sampledDesc ] { device.CreateTexture( sampledDesc ); },
			"The bindless heap accepted more dynamic SRVs than its capacity." );

		constexpr std::array<uint32_t, 4> fragmentedSlots = { 1u, 3u, 5u, 7u };
		for( uint32_t slot : fragmentedSlots )
		{
			Require( device.Destroy( sampledTextures[ slot ] ), "Failed to destroy a sampled texture during descriptor fragmentation." );
			sampledTextures[ slot ] = {};
		}

		for( uint32_t replacementIndex = 0; replacementIndex < fragmentedSlots.size(); ++replacementIndex )
		{
			const uint32_t slot = fragmentedSlots[ replacementIndex ];
			sampledTextures[ slot ] = device.CreateTexture( sampledDesc );
			Require( device.GetBindlessIndex( sampledTextures[ slot ] ) == sampledIndices[ slot ], "A released dynamic SRV descriptor was not recycled." );
		}

		constexpr std::array<uint32_t, dynamicDescriptorCount> destructionOrder = { 3u, 0u, 7u, 2u, 5u, 1u, 6u, 4u };
		for( uint32_t slot : destructionOrder )
		{
			Require( device.Destroy( sampledTextures[ slot ] ), "Failed to destroy a sampled texture during range coalescing." );
			sampledTextures[ slot ] = {};
		}

		constexpr uint32_t stressRoundCount = 12;
		for( uint32_t round = 0; round < stressRoundCount; ++round )
		{
			for( uint32_t index = 0; index < dynamicDescriptorCount; ++index )
			{
				sampledTextures[ index ] = device.CreateTexture( sampledDesc );
				Require( device.GetBindlessIndex( sampledTextures[ index ] ) == LDX12_BINDLESS_DYNAMIC_SLOT_FIRST + index,
					"A coalesced bindless range did not allocate contiguously." );
			}

			for( uint32_t slot : destructionOrder )
			{
				Require( device.Destroy( sampledTextures[ slot ] ), "Descriptor stress destruction failed." );
				sampledTextures[ slot ] = {};
			}
		}

		TextureDesc sampledUavDesc = sampledDesc;
		sampledUavDesc.debugName = "Ldx12Tests recycled sampled UAV texture";
		sampledUavDesc.usage = TextureUsage::Sampled | TextureUsage::UnorderedAccess;
		std::array<TextureHandle, dynamicDescriptorCount / 2u> sampledUavTextures{};
		for( TextureHandle& texture : sampledUavTextures )
		{
			texture = device.CreateTexture( sampledUavDesc );
			Require( device.GetBindlessIndex( texture ) != device.GetUnorderedAccessIndex( texture ),
				"A sampled UAV texture aliased its SRV and UAV descriptors." );
		}
		RequireThrows<std::runtime_error>( [ &device, &sampledUavDesc ] { device.CreateTexture( sampledUavDesc ); },
			"The bindless heap accepted another SRV/UAV pair after exhaustion." );
		for( TextureHandle texture : sampledUavTextures )
		{
			Require( device.Destroy( texture ), "Failed to recycle a sampled UAV texture descriptor pair." );
		}

		std::array<uint32_t, 8u * 8u> mipPixels{};
		TextureDesc mipDesc{};
		mipDesc.debugName = "Ldx12Tests recycled mip descriptor range";
		mipDesc.width = 8;
		mipDesc.height = 8;
		mipDesc.countMipMap = 4;
		mipDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		mipDesc.usage = TextureUsage::Sampled;
		mipDesc.data = mipPixels.data();
		mipDesc.rowPitch = 8u * sizeof( uint32_t );
		mipDesc.slicePitch = static_cast<uint32_t>( sizeof( mipPixels ) );

		const TextureHandle firstMipTexture = device.CreateTexture( mipDesc );
		const TextureHandle secondMipTexture = device.CreateTexture( mipDesc );
		Require( device.GetBindlessIndex( firstMipTexture ) == LDX12_BINDLESS_DYNAMIC_SLOT_FIRST,
			"The first mipmapped texture did not allocate at the start of the dynamic heap." );
		Require( device.GetBindlessIndex( secondMipTexture ) == LDX12_BINDLESS_DYNAMIC_SLOT_FIRST + 4u,
			"Mip UAV descriptor ranges were not allocated contiguously." );
		Require( device.Destroy( firstMipTexture ) && device.Destroy( secondMipTexture ), "Failed to destroy mipmapped textures." );

		const TextureHandle recycledMipTexture = device.CreateTexture( mipDesc );
		Require( device.GetBindlessIndex( recycledMipTexture ) == LDX12_BINDLESS_DYNAMIC_SLOT_FIRST, "Released mip UAV descriptor ranges were not coalesced." );
		Require( device.Destroy( recycledMipTexture ), "Failed to destroy the recycled mipmapped texture." );

		TextureDesc renderTargetDesc{};
		renderTargetDesc.debugName = "Ldx12Tests recycled RTV";
		renderTargetDesc.width = 8;
		renderTargetDesc.height = 8;
		renderTargetDesc.usage = TextureUsage::RenderTarget;
		std::array<TextureHandle, 4> renderTargets{};
		for( TextureHandle& renderTarget : renderTargets )
		{
			renderTarget = device.CreateTexture( renderTargetDesc );
		}
		RequireThrows<std::runtime_error>( [ &device, &renderTargetDesc ] { device.CreateTexture( renderTargetDesc ); },
			"The RTV heap accepted more descriptors than its capacity." );
		Require( device.Destroy( renderTargets[ 1 ] ), "Failed to release an RTV descriptor." );
		renderTargets[ 1 ] = device.CreateTexture( renderTargetDesc );
		for( TextureHandle renderTarget : renderTargets )
		{
			Require( device.Destroy( renderTarget ), "Failed to recycle an RTV descriptor." );
		}

		TextureDesc depthDesc{};
		depthDesc.debugName = "Ldx12Tests recycled DSV";
		depthDesc.width = 8;
		depthDesc.height = 8;
		depthDesc.format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.usage = TextureUsage::DepthStencil;
		std::array<TextureHandle, 3> depthTextures{};
		for( TextureHandle& depthTexture : depthTextures )
		{
			depthTexture = device.CreateTexture( depthDesc );
		}
		RequireThrows<std::runtime_error>( [ &device, &depthDesc ] { device.CreateTexture( depthDesc ); },
			"The DSV heap accepted more descriptors than its capacity." );
		Require( device.Destroy( depthTextures[ 1 ] ), "Failed to release a DSV descriptor." );
		depthTextures[ 1 ] = device.CreateTexture( depthDesc );
		for( TextureHandle depthTexture : depthTextures )
		{
			Require( device.Destroy( depthTexture ), "Failed to recycle a DSV descriptor." );
		}

		BufferDesc fixedBufferDesc{};
		fixedBufferDesc.debugName = "Ldx12Tests recycled fixed CBV";
		fixedBufferDesc.size = 256;
		fixedBufferDesc.type = BufferType::Constant;
		fixedBufferDesc.memory = BufferMemory::CpuToGpu;
		const BufferHandle fixedBuffer = device.CreateBuffer( fixedBufferDesc, ConstantBufferSlot::FreeCB1 );
		RequireThrows<std::runtime_error>( [ &device, &fixedBufferDesc ] { device.CreateBuffer( fixedBufferDesc, ConstantBufferSlot::FreeCB1 ); },
			"A fixed CBV slot was allocated twice." );
		Require( device.Destroy( fixedBuffer ), "Failed to release a fixed CBV descriptor." );
		const BufferHandle recycledFixedBuffer = device.CreateBuffer( fixedBufferDesc, ConstantBufferSlot::FreeCB1 );
		Require( device.GetConstantBufferIndex( recycledFixedBuffer ) == ToSlotIndex( ConstantBufferSlot::FreeCB1 ),
			"A released fixed CBV descriptor was not recycled." );
		Require( device.Destroy( recycledFixedBuffer ), "Failed to destroy the recycled fixed-CBV buffer." );

		device.WaitIdle();
	}
}
