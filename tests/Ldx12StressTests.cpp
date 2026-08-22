#include "Ldx12/TestTemplate.hpp"

#include <array>
#include <chrono>
#include <exception>
#include <iostream>

// Exercises resource lifetime, bindless descriptor recycling, command batches and texture-state fixups under repeated allocation pressure.

namespace
{
	constexpr uint32_t ourResourceCount = 128;
	constexpr uint32_t ourRoundCount = 32;
	constexpr uint32_t ourBatchSize = 4;

	uint32_t GetDestructionSlot( uint32_t index ) noexcept
	{
		return ( index * 73u ) % ourResourceCount;
	}

	void RunGpuStressTest()
	{
		using namespace ldx12;
		using namespace ldx12::tests;

		const auto startTime = std::chrono::steady_clock::now();
		ContextDesc context{};
		context.enableDebugLayer = true;
		context.preferHighPerformanceAdapter = false;
		context.bindlessCapacity = LDX12_BINDLESS_DYNAMIC_SLOT_FIRST + ourResourceCount * 3u;
		context.rtvCapacity = 4;
		context.dsvCapacity = 4;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		TextureDesc textureDesc{};
		textureDesc.debugName = "Ldx12 stress sampled UAV texture";
		textureDesc.width = 4;
		textureDesc.height = 4;
		textureDesc.usage = TextureUsage::Sampled | TextureUsage::UnorderedAccess;

		BufferDesc bufferDesc{};
		bufferDesc.debugName = "Ldx12 stress upload SRV buffer";
		bufferDesc.size = 256;
		bufferDesc.stride = 16;
		bufferDesc.heapType = D3D12_HEAP_TYPE_UPLOAD;
		bufferDesc.createShaderResourceView = true;

		constexpr std::array<D3D12_RESOURCE_STATES, ourBatchSize> batchStates = {
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
		};

		std::array<TextureHandle, ourResourceCount> textures{};
		std::array<BufferHandle, ourResourceCount> buffers{};
		for( uint32_t round = 0; round < ourRoundCount; ++round )
		{
			for( uint32_t index = 0; index < ourResourceCount; ++index )
			{
				textures[ index ] = device.CreateTexture( textureDesc );
				buffers[ index ] = device.CreateBuffer( bufferDesc );
				Require( textures[ index ].Valid() && buffers[ index ].Valid(), "Stress resource creation returned an invalid handle." );
				Require( device.GetBindlessIndex( textures[ index ] ) != LDX12_DESCRIPTOR_SLOT_INVALID, "Stress texture did not receive an SRV descriptor." );
				Require( device.GetUnorderedAccessIndex( textures[ index ] ) != LDX12_DESCRIPTOR_SLOT_INVALID, "Stress texture did not receive a UAV descriptor." );
				Require( device.GetBindlessIndex( buffers[ index ] ) != LDX12_DESCRIPTOR_SLOT_INVALID, "Stress buffer did not receive an SRV descriptor." );

				const std::array<uint32_t, 4> data = { round, index, round ^ index, round + index };
				device.WriteBuffer( buffers[ index ], 0, data.data(), sizeof( data ) );
			}

			if( round == 0 )
			{
				RequireThrows<std::runtime_error>( [&device, &bufferDesc] { device.CreateBuffer( bufferDesc ); }, "Stress test did not exhaust the configured bindless heap." );
			}

			std::array<ICommandBuffer*, ourBatchSize> commandBuffers{};
			for( uint32_t batchIndex = 0; batchIndex < ourBatchSize; ++batchIndex )
			{
				ICommandBuffer& commandBuffer = device.AcquireCommandBuffer();
				for( TextureHandle texture : textures )
				{
					commandBuffer.CmdTransitionTexture( texture, batchStates[ batchIndex ] );
				}
				commandBuffers[ batchIndex ] = &commandBuffer;
			}

			const SubmitHandle submission = device.SubmitBatch( commandBuffers.data(), static_cast<uint32_t>( commandBuffers.size() ) );
			Require( !submission.Empty(), "Stress batch returned an empty submission handle." );

			for( uint32_t destructionIndex = 0; destructionIndex < ourResourceCount; ++destructionIndex )
			{
				const uint32_t slot = GetDestructionSlot( destructionIndex );
				const TextureHandle texture = textures[ slot ];
				const BufferHandle buffer = buffers[ slot ];
				Require( device.Destroy( texture ) && device.Destroy( buffer ), "Stress resource destruction failed." );
				Require( !device.IsAlive( texture ) && !device.IsAlive( buffer ), "A destroyed stress handle remained alive." );
				textures[ slot ] = {};
				buffers[ slot ] = {};
			}

			device.Wait( submission );
			Require( device.IsReady( submission ), "A waited stress submission remained pending." );
		}

		const TextureHandle recycledTexture = device.CreateTexture( textureDesc );
		const BufferHandle recycledBuffer = device.CreateBuffer( bufferDesc );
		Require( device.GetBindlessIndex( recycledTexture ) == LDX12_BINDLESS_DYNAMIC_SLOT_FIRST, "Stress cleanup did not coalesce the bindless descriptor range." );
		Require( device.Destroy( recycledTexture ) && device.Destroy( recycledBuffer ), "Final stress cleanup failed." );
		device.WaitIdle();
		DeviceManager::ShutdownSingleton();
		guard.active = false;

		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - startTime );
		constexpr uint32_t createdResourceCount = ourRoundCount * ourResourceCount * 2u + 2u;
		constexpr uint32_t commandBufferCount = ourRoundCount * ourBatchSize;
		constexpr uint32_t transitionCount = ourRoundCount * ourResourceCount * ourBatchSize;
		std::cout << "[STRESS] " << createdResourceCount << " resources, " << commandBufferCount << " command buffers, " << ourRoundCount << " batches and " << transitionCount << " transitions completed in " << elapsed.count() << " ms.\n";
	}
}

int main()
{
	try
	{
		RunGpuStressTest();
		return 0;
	}
	catch( const std::exception& exception )
	{
		std::cerr << "[FAIL] Ldx12 GPU stress test: " << exception.what() << '\n';
		return 1;
	}
}
