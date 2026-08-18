#include "LightD3D12/HandleSlotMap.hpp"
#include "LightD3D12/LightD3D12.hpp"

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace lightd3d12;

namespace
{
	void Require( bool condition, const char* message )
	{
		if( !condition )
		{
			throw std::runtime_error( message );
		}
	}

	template<typename ExceptionType, typename Function>
	void RequireThrows( Function&& function, const char* message )
	{
		try
		{
			function();
		}
		catch( const ExceptionType& )
		{
			return;
		}

		throw std::runtime_error( message );
	}

	struct TestObject final
	{
		uint32_t id = 0;
		float weight = 0.0f;
	};

	class VirtualObject
	{
	public:
		virtual ~VirtualObject() = default;
		virtual uint32_t Id() const noexcept = 0;
		virtual float Property() const noexcept = 0;
	};

	class DerivedVirtualObject final: public VirtualObject
	{
	public:
		DerivedVirtualObject( uint32_t id, float property, uint32_t& destructionCount ) noexcept:
			id_( id ),
			property_( property ),
			destructionCount_( &destructionCount )
		{
		}

		~DerivedVirtualObject() override
		{
			++(*destructionCount_);
		}

		uint32_t Id() const noexcept override { return id_; }
		float Property() const noexcept override { return property_; }

	private:
		uint32_t id_ = 0;
		float property_ = 0.0f;
		uint32_t* destructionCount_ = nullptr;
	};

	void TestSlotMapCreationAndProperties()
	{
		SlotMap<TestObject, 4> objects;
		const Handle<TestObject> first = objects.Create( TestObject{ 10u, 1.5f } );
		const Handle<TestObject> second = objects.Create( TestObject{ 20u, 2.5f } );

		Require( first.Valid() && second.Valid(), "SlotMap returned an invalid handle." );
		Require( first != second, "Different objects received the same handle." );
		Require( first.Index() == 0u && second.Index() == 1u,
			"SlotMap did not allocate sequential initial slots." );
		Require( first.Gen() == 1u && second.Gen() == 1u,
			"Initial SlotMap generations are incorrect." );
		Require( objects.Size() == 2u && objects.NumObjects() == 2u,
			"SlotMap object count is incorrect." );

		TestObject* firstObject = objects.Get( first );
		Require( firstObject != nullptr, "Cannot retrieve a live SlotMap object." );
		Require( firstObject->id == 10u && firstObject->weight == 1.5f,
			"SlotMap did not preserve object properties." );
		Require( objects.GetByIndex( second.Index() )->id == 20u,
			"SlotMap index lookup returned the wrong object." );
		Require( objects.Find( firstObject ) == first,
			"SlotMap reverse lookup returned the wrong handle." );

		uint32_t visitedCount = 0;
		uint32_t idSum = 0;
		objects.ForEach( [&visitedCount, &idSum]( const TestObject& object )
			{
				++visitedCount;
				idSum += object.id;
			} );
		Require( visitedCount == 2u && idSum == 30u,
			"SlotMap iteration did not visit every live object." );
	}

	void TestSlotMapDestroyAndReuse()
	{
		SlotMap<TestObject, 3> objects;
		const Handle<TestObject> first = objects.Create( TestObject{ 1u, 1.0f } );
		const Handle<TestObject> removed = objects.Create( TestObject{ 2u, 2.0f } );
		objects.Create( TestObject{ 3u, 3.0f } );

		objects.Destroy( removed );
		Require( objects.Size() == 2u, "SlotMap destruction did not update the count." );
		Require( objects.GetByIndex( removed.Index() ) == nullptr,
			"Destroyed SlotMap storage remains occupied." );

		const Handle<TestObject> replacement =
			objects.Create( TestObject{ 42u, 4.2f } );
		Require( replacement.Index() == removed.Index(),
			"SlotMap did not reuse a released slot." );
		Require( replacement.Gen() != removed.Gen(),
			"Reused SlotMap slot did not change generation." );
		Require( replacement != removed,
			"A stale handle compares equal to its replacement." );
		Require( objects.Get( replacement )->id == 42u,
			"Replacement object properties are incorrect." );

		objects.Clear();
		Require( objects.Size() == 0u, "SlotMap Clear did not remove every object." );
		const Handle<TestObject> afterClear =
			objects.Create( TestObject{ 99u, 9.9f } );
		Require( afterClear.Index() == 0u, "SlotMap did not restart at slot zero after Clear." );
		Require( afterClear != first, "SlotMap Clear did not invalidate old generations." );
	}

	void TestSlotMapStaleHandleSafety()
	{
		SlotMap<TestObject, 2> objects;
		Require( objects.Empty(), "A new SlotMap is not empty." );
		Require( objects.MaxSize() == 2u, "SlotMap does not expose its fixed capacity." );
		Require( !objects.Contains( Handle<TestObject>{} ),
			"SlotMap accepted an empty handle." );
		Require( !objects.Destroy( Handle<TestObject>{} ),
			"Destroy reported success for an empty handle." );

		const Handle<TestObject> removed = objects.Create( TestObject{ 7u, 1.0f } );
		Require( objects.Contains( removed ), "SlotMap does not contain a newly created handle." );
		Require( objects.Destroy( removed ), "Destroy failed for a live handle." );
		Require( objects.Empty(), "Destroy did not empty the SlotMap." );
		Require( !objects.Contains( removed ), "Destroyed handle remains alive." );
		Require( objects.Get( removed ) == nullptr, "Destroyed handle still resolves to an object." );
		Require( !objects.Destroy( removed ), "Double destruction reported success." );
		Require( objects.Size() == 0u, "Double destruction corrupted the object count." );

		const Handle<TestObject> replacement = objects.Create( TestObject{ 9u, 2.0f } );
		Require( replacement.Index() == removed.Index(), "Released slot was not recycled." );
		Require( replacement.Gen() != removed.Gen(), "Recycled slot retained its old generation." );
		Require( !objects.Contains( removed ), "Stale handle aliases the replacement object." );
		Require( objects.Get( removed ) == nullptr, "Stale handle retrieved the replacement object." );
		Require( !objects.Destroy( removed ), "Stale handle destroyed the replacement object." );
		Require( objects.Contains( replacement ), "Stale destruction invalidated the replacement handle." );
		Require( objects.Get( replacement )->id == 9u, "Replacement object was modified by a stale handle." );
		Require( objects.Destroy( replacement ), "Destroy failed for the replacement object." );
		Require( objects.Empty(), "SlotMap is not empty after destroying every object." );
	}

	void TestSlotMapCapacity()
	{
		SlotMap<TestObject, 2> objects;
		objects.Create( TestObject{ 1u, 1.0f } );
		objects.Create( TestObject{ 2u, 2.0f } );
		RequireThrows<std::length_error>(
			[&objects] { objects.Create( TestObject{ 3u, 3.0f } ); },
			"SlotMap accepted more objects than its fixed capacity." );
	}

	void TestSlotMapVirtualObjects()
	{
		using VirtualPointer = std::unique_ptr<VirtualObject>;
		SlotMap<VirtualPointer, 3> objects;
		uint32_t destructionCount = 0;

		VirtualPointer firstObject =
			std::make_unique<DerivedVirtualObject>( 7u, 3.5f, destructionCount );
		const Handle<VirtualPointer> first = objects.Create( std::move( firstObject ) );
		Require( firstObject == nullptr, "SlotMap did not take ownership of a virtual object." );
		VirtualPointer* storedObject = objects.Get( first );
		Require( storedObject != nullptr && *storedObject != nullptr,
			"SlotMap lost the virtual object." );
		Require( (*storedObject)->Id() == 7u && (*storedObject)->Property() == 3.5f,
			"Virtual dispatch or stored properties are incorrect." );

		objects.Destroy( first );
		Require( destructionCount == 1u,
			"Destroy did not release the owned virtual object." );

		objects.Create( std::make_unique<DerivedVirtualObject>( 8u, 4.0f, destructionCount ) );
		objects.Create( std::make_unique<DerivedVirtualObject>( 9u, 4.5f, destructionCount ) );
		objects.Clear();
		Require( destructionCount == 3u,
			"Clear did not release every owned virtual object." );
	}

	void TestPublicArrayProperties()
	{
		ShaderStageSource shader{};
		RenderPipelineDesc pipeline{};
		ContextDesc context{};

		Require( shader.includeDirectories.size() == ourMaxShaderIncludeDirectories,
			"Shader include array capacity differs from the public constant." );
		Require( pipeline.inputElements.size() == ourMaxVertexInputElements,
			"Vertex input array capacity differs from the public constant." );
		Require( context.bindlessCapacity == ourMaxBindlessDescriptors,
			"Default bindless capacity differs from the fixed array limit." );
		Require( context.rtvCapacity == ourMaxRtvDescriptors,
			"Default RTV capacity differs from the fixed array limit." );
		Require( context.dsvCapacity == ourMaxDsvDescriptors,
			"Default DSV capacity differs from the fixed array limit." );
		Require( sizeof( Handle<TestObject> ) == sizeof( uint64_t ),
			"LightD3D12 handles are not 64-bit values." );
	}

	struct DeviceManagerGuard final
	{
		~DeviceManagerGuard()
		{
			if( active )
			{
				DeviceManager::ShutdownSingleton();
			}
		}

		bool active = false;
	};

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
		Require( device.GetNativeDevice() != nullptr, "D3D12 device creation failed." );
		Require( device.GetNativeCommandQueue() != nullptr, "D3D12 command queue creation failed." );

		BufferDesc genericBufferDesc{};
		genericBufferDesc.debugName = "LightDX12Tests generic buffer";
		genericBufferDesc.size = 64;
		genericBufferDesc.heapType = D3D12_HEAP_TYPE_UPLOAD;
		const BufferHandle genericBuffer = device.CreateBuffer( genericBufferDesc );
		Require( genericBuffer.Valid(), "Generic buffer creation returned an invalid handle." );
		Require( device.GetBindlessIndex( genericBuffer ) == LIGHTD3D12_DESCRIPTOR_SLOT_INVALID,
			"A buffer without an SRV unexpectedly owns a bindless descriptor." );
		Require( device.GetConstantBufferIndex( genericBuffer ) == LIGHTD3D12_DESCRIPTOR_SLOT_INVALID,
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
		structuredBufferDesc.debugName = "LightDX12Tests structured buffer";
		structuredBufferDesc.size = 128;
		structuredBufferDesc.stride = 16;
		structuredBufferDesc.createShaderResourceView = true;
		const BufferHandle structuredBuffer = device.CreateBuffer( structuredBufferDesc );
		const uint32_t structuredSrv = device.GetBindlessIndex( structuredBuffer );
		Require( structuredBuffer.Valid(), "Structured buffer creation returned an invalid handle." );
		Require( structuredSrv >= LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST &&
			structuredSrv < context.bindlessCapacity,
			"Structured buffer SRV index is outside the bindless heap." );

		BufferDesc constantBufferDesc{};
		constantBufferDesc.debugName = "LightDX12Tests constant buffer";
		constantBufferDesc.size = 100;
		constantBufferDesc.heapType = D3D12_HEAP_TYPE_UPLOAD;
		const BufferHandle constantBuffer =
			device.CreateBuffer( constantBufferDesc, ConstantBufferSlot::FreeCB0 );
		Require( device.GetConstantBufferIndex( constantBuffer ) ==
			ToSlotIndex( ConstantBufferSlot::FreeCB0 ),
			"Constant buffer was not created in its requested fixed slot." );

		TextureDesc textureDesc{};
		textureDesc.debugName = "LightDX12Tests sampled UAV texture";
		textureDesc.width = 32;
		textureDesc.height = 16;
		textureDesc.countMipMap = 3;
		textureDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.usage = TextureUsage::Sampled | TextureUsage::UnorderedAccess;
		const TextureHandle texture = device.CreateTexture( textureDesc );
		Require( texture.Valid(), "Texture creation returned an invalid handle." );
		const uint32_t textureSrv = device.GetBindlessIndex( texture );
		const uint32_t textureUav = device.GetUnorderedAccessIndex( texture );
		Require( textureSrv >= LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST &&
			textureSrv < context.bindlessCapacity,
			"Texture SRV index is outside the bindless heap." );
		Require( textureUav >= LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST &&
			textureUav < context.bindlessCapacity && textureUav != textureSrv,
			"Texture UAV index is invalid or aliases its SRV." );

		ID3D12Resource* nativeTexture = device.GetNativeTextureResource( texture );
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

		Require( !device.IsAlive( BufferHandle{} ),
			"An empty buffer handle was reported as alive." );
		Require( !device.IsAlive( TextureHandle{} ),
			"An empty texture handle was reported as alive." );
		Require( !device.Destroy( BufferHandle{} ),
			"Destroy reported success for an empty buffer handle." );
		Require( !device.Destroy( TextureHandle{} ),
			"Destroy reported success for an empty texture handle." );

		BufferDesc bufferDesc{};
		bufferDesc.debugName = "LightDX12Tests invalid-handle buffer";
		bufferDesc.size = 64;
		bufferDesc.heapType = D3D12_HEAP_TYPE_UPLOAD;
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
		textureDesc.debugName = "LightDX12Tests invalid-handle texture";
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
			[&device, staleTexture] { device.GetNativeTextureResource( staleTexture ); },
			"A stale texture handle was accepted by a resource query." );

		Require( device.Destroy( replacementBuffer ),
			"Cleanup failed for the replacement buffer." );
		Require( device.Destroy( replacementTexture ),
			"Cleanup failed for the replacement texture." );
		device.WaitIdle();
	}

	void TestGpuSubmissionSynchronization()
	{
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		const SubmitHandle emptySubmission{};
		Require( device.IsReady( emptySubmission ),
			"An empty submission handle was not reported as ready." );
		device.Wait( emptySubmission );

		ICommandBuffer& commandBuffer = device.AcquireCommandBuffer();
		const SubmitHandle submission = device.Submit( commandBuffer );
		Require( !submission.Empty(),
			"Submitting a command buffer returned an empty submission handle." );

		device.Wait( submission );
		Require( device.IsReady( submission ),
			"A waited submission was not reported as ready." );

		constexpr uint32_t recycleSubmissionCount = 65;
		for( uint32_t index = 0; index < recycleSubmissionCount; ++index )
		{
			ICommandBuffer& recycledCommandBuffer = device.AcquireCommandBuffer();
			const SubmitHandle recycledSubmission = device.Submit( recycledCommandBuffer );
			Require( !recycledSubmission.Empty(),
				"Command-buffer recycling returned an empty submission handle." );
			device.Wait( recycledSubmission );
		}

		Require( device.IsReady( submission ),
			"A completed submission became pending after command-buffer recycling." );
	}

	void TestGpuDescriptorRecycling()
	{
		constexpr uint32_t dynamicDescriptorCount = 8;
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;
		context.bindlessCapacity =
			LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST + dynamicDescriptorCount;
		context.rtvCapacity = 4;
		context.dsvCapacity = 3;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		TextureDesc sampledDesc{};
		sampledDesc.debugName = "LightDX12Tests recycled sampled texture";
		sampledDesc.width = 4;
		sampledDesc.height = 4;
		sampledDesc.usage = TextureUsage::Sampled;

		std::array<TextureHandle, dynamicDescriptorCount> sampledTextures{};
		std::array<uint32_t, dynamicDescriptorCount> sampledIndices{};
		for( uint32_t index = 0; index < dynamicDescriptorCount; ++index )
		{
			sampledTextures[ index ] = device.CreateTexture( sampledDesc );
			sampledIndices[ index ] = device.GetBindlessIndex( sampledTextures[ index ] );
			Require( sampledIndices[ index ] ==
				LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST + index,
				"Dynamic SRV allocation did not use the expected contiguous range." );
		}

		RequireThrows<std::runtime_error>(
			[&device, &sampledDesc] { device.CreateTexture( sampledDesc ); },
			"The bindless heap accepted more dynamic SRVs than its capacity." );

		constexpr std::array<uint32_t, 4> fragmentedSlots = { 1u, 3u, 5u, 7u };
		for( uint32_t slot : fragmentedSlots )
		{
			Require( device.Destroy( sampledTextures[ slot ] ),
				"Failed to destroy a sampled texture during descriptor fragmentation." );
			sampledTextures[ slot ] = {};
		}

		for( uint32_t replacementIndex = 0;
			replacementIndex < fragmentedSlots.size(); ++replacementIndex )
		{
			const uint32_t slot = fragmentedSlots[ replacementIndex ];
			sampledTextures[ slot ] = device.CreateTexture( sampledDesc );
			Require( device.GetBindlessIndex( sampledTextures[ slot ] ) ==
				sampledIndices[ slot ],
				"A released dynamic SRV descriptor was not recycled." );
		}

		constexpr std::array<uint32_t, dynamicDescriptorCount> destructionOrder = {
			3u, 0u, 7u, 2u, 5u, 1u, 6u, 4u
		};
		for( uint32_t slot : destructionOrder )
		{
			Require( device.Destroy( sampledTextures[ slot ] ),
				"Failed to destroy a sampled texture during range coalescing." );
			sampledTextures[ slot ] = {};
		}

		constexpr uint32_t stressRoundCount = 12;
		for( uint32_t round = 0; round < stressRoundCount; ++round )
		{
			for( uint32_t index = 0; index < dynamicDescriptorCount; ++index )
			{
				sampledTextures[ index ] = device.CreateTexture( sampledDesc );
				Require( device.GetBindlessIndex( sampledTextures[ index ] ) ==
					LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST + index,
					"A coalesced bindless range did not allocate contiguously." );
			}

			for( uint32_t slot : destructionOrder )
			{
				Require( device.Destroy( sampledTextures[ slot ] ),
					"Descriptor stress destruction failed." );
				sampledTextures[ slot ] = {};
			}
		}

		TextureDesc sampledUavDesc = sampledDesc;
		sampledUavDesc.debugName = "LightDX12Tests recycled sampled UAV texture";
		sampledUavDesc.usage = TextureUsage::Sampled | TextureUsage::UnorderedAccess;
		std::array<TextureHandle, dynamicDescriptorCount / 2u> sampledUavTextures{};
		for( TextureHandle& texture : sampledUavTextures )
		{
			texture = device.CreateTexture( sampledUavDesc );
			Require( device.GetBindlessIndex( texture ) !=
				device.GetUnorderedAccessIndex( texture ),
				"A sampled UAV texture aliased its SRV and UAV descriptors." );
		}
		RequireThrows<std::runtime_error>(
			[&device, &sampledUavDesc] { device.CreateTexture( sampledUavDesc ); },
			"The bindless heap accepted another SRV/UAV pair after exhaustion." );
		for( TextureHandle texture : sampledUavTextures )
		{
			Require( device.Destroy( texture ),
				"Failed to recycle a sampled UAV texture descriptor pair." );
		}

		std::array<uint32_t, 8u * 8u> mipPixels{};
		TextureDesc mipDesc{};
		mipDesc.debugName = "LightDX12Tests recycled mip descriptor range";
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
		Require( device.GetBindlessIndex( firstMipTexture ) ==
			LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST,
			"The first mipmapped texture did not allocate at the start of the dynamic heap." );
		Require( device.GetBindlessIndex( secondMipTexture ) ==
			LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST + 4u,
			"Mip UAV descriptor ranges were not allocated contiguously." );
		Require( device.Destroy( firstMipTexture ) && device.Destroy( secondMipTexture ),
			"Failed to destroy mipmapped textures." );

		const TextureHandle recycledMipTexture = device.CreateTexture( mipDesc );
		Require( device.GetBindlessIndex( recycledMipTexture ) ==
			LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST,
			"Released mip UAV descriptor ranges were not coalesced." );
		Require( device.Destroy( recycledMipTexture ),
			"Failed to destroy the recycled mipmapped texture." );

		TextureDesc renderTargetDesc{};
		renderTargetDesc.debugName = "LightDX12Tests recycled RTV";
		renderTargetDesc.width = 8;
		renderTargetDesc.height = 8;
		renderTargetDesc.usage = TextureUsage::RenderTarget;
		std::array<TextureHandle, 4> renderTargets{};
		for( TextureHandle& renderTarget : renderTargets )
		{
			renderTarget = device.CreateTexture( renderTargetDesc );
		}
		RequireThrows<std::runtime_error>(
			[&device, &renderTargetDesc] { device.CreateTexture( renderTargetDesc ); },
			"The RTV heap accepted more descriptors than its capacity." );
		Require( device.Destroy( renderTargets[ 1 ] ),
			"Failed to release an RTV descriptor." );
		renderTargets[ 1 ] = device.CreateTexture( renderTargetDesc );
		for( TextureHandle renderTarget : renderTargets )
		{
			Require( device.Destroy( renderTarget ),
				"Failed to recycle an RTV descriptor." );
		}

		TextureDesc depthDesc{};
		depthDesc.debugName = "LightDX12Tests recycled DSV";
		depthDesc.width = 8;
		depthDesc.height = 8;
		depthDesc.format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.usage = TextureUsage::DepthStencil;
		std::array<TextureHandle, 3> depthTextures{};
		for( TextureHandle& depthTexture : depthTextures )
		{
			depthTexture = device.CreateTexture( depthDesc );
		}
		RequireThrows<std::runtime_error>(
			[&device, &depthDesc] { device.CreateTexture( depthDesc ); },
			"The DSV heap accepted more descriptors than its capacity." );
		Require( device.Destroy( depthTextures[ 1 ] ),
			"Failed to release a DSV descriptor." );
		depthTextures[ 1 ] = device.CreateTexture( depthDesc );
		for( TextureHandle depthTexture : depthTextures )
		{
			Require( device.Destroy( depthTexture ),
				"Failed to recycle a DSV descriptor." );
		}

		BufferDesc fixedBufferDesc{};
		fixedBufferDesc.debugName = "LightDX12Tests recycled fixed CBV";
		fixedBufferDesc.size = 256;
		fixedBufferDesc.heapType = D3D12_HEAP_TYPE_UPLOAD;
		const BufferHandle fixedBuffer =
			device.CreateBuffer( fixedBufferDesc, ConstantBufferSlot::FreeCB1 );
		RequireThrows<std::runtime_error>(
			[&device, &fixedBufferDesc]
			{
				device.CreateBuffer( fixedBufferDesc, ConstantBufferSlot::FreeCB1 );
			},
			"A fixed CBV slot was allocated twice." );
		Require( device.Destroy( fixedBuffer ),
			"Failed to release a fixed CBV descriptor." );
		const BufferHandle recycledFixedBuffer =
			device.CreateBuffer( fixedBufferDesc, ConstantBufferSlot::FreeCB1 );
		Require( device.GetConstantBufferIndex( recycledFixedBuffer ) ==
			ToSlotIndex( ConstantBufferSlot::FreeCB1 ),
			"A released fixed CBV descriptor was not recycled." );
		Require( device.Destroy( recycledFixedBuffer ),
			"Failed to destroy the recycled fixed-CBV buffer." );

		device.WaitIdle();
	}

	struct TestCase final
	{
		const char* name = nullptr;
		void (*function)() = nullptr;
	};
}

int main()
{
	const std::array<TestCase, 10> tests = {
		TestCase{ "SlotMap creation and properties", TestSlotMapCreationAndProperties },
		TestCase{ "SlotMap destruction and reuse", TestSlotMapDestroyAndReuse },
		TestCase{ "SlotMap stale-handle safety", TestSlotMapStaleHandleSafety },
		TestCase{ "SlotMap fixed capacity", TestSlotMapCapacity },
		TestCase{ "SlotMap virtual objects", TestSlotMapVirtualObjects },
		TestCase{ "Public fixed-array properties", TestPublicArrayProperties },
		TestCase{ "GPU resource lifecycle and properties", TestGpuResourceLifecycleAndProperties },
		TestCase{ "GPU invalid-handle safety", TestGpuInvalidHandleSafety },
		TestCase{ "GPU submission synchronization", TestGpuSubmissionSynchronization },
		TestCase{ "GPU descriptor recycling", TestGpuDescriptorRecycling }
	};

	uint32_t passedCount = 0;
	for( const TestCase& test : tests )
	{
		try
		{
			test.function();
			++passedCount;
			std::cout << "[PASS] " << test.name << '\n';
		}
		catch( const std::exception& exception )
		{
			std::cerr << "[FAIL] " << test.name << ": " << exception.what() << '\n';
			return 1;
		}
		catch( ... )
		{
			std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
			return 1;
		}
	}

	std::cout << "LightDX12 core tests passed: " << passedCount << '/' << tests.size() << ".\n";
	return 0;
}
