#pragma once

#include "LightD3D12Internal.hpp"
#include "LightD3D12CommandBuffer.hpp"
#include "LightD3D12ImmediateCommands.hpp"
#include "LightD3D12Resources.hpp"

#include <array>
#include <deque>
#include <functional>
#include <memory>

namespace lightd3d12
{
	class StagingDevice;
	class Swapchain;
	class BaseMips;
	struct SwapchainResource final
	{
		SwapchainDesc desc_ = {};
		std::unique_ptr<Swapchain> swapchain_;
	};

	class DeviceManager::Impl final
	{
	public:
		static constexpr uint32_t ourMaxActiveCommandBuffers = 64;
		static constexpr std::size_t ourMaxSwapchains = 16;
		static constexpr std::size_t ourMaxBuffers = 4096;
		static constexpr std::size_t ourMaxTextures = 4096;

		struct DeferredRelease final
		{
			SubmitHandle handle_;
			std::function<void()> release_;
		};

		struct QueueContext final
		{
			QueueContext() = default;

			explicit QueueContext( QueueType type ) noexcept:
				type_( type )
			{
			}

			QueueType type_ = QueueType::Graphics;
			ComPtr<ID3D12CommandQueue> commandQueue_;
			ComPtr<ID3D12Fence> queueIdleFence_;
			HANDLE queueIdleEvent_ = nullptr;
			uint64_t queueIdleFenceValue_ = 0;
			std::unique_ptr<ImmediateCommands> immediateCommands_;
			std::array<std::unique_ptr<CommandBufferImpl>, ourMaxActiveCommandBuffers> activeCommandBuffers_ = {};
			std::deque<DeferredRelease> deferredReleases_;
		};

		explicit Impl( const ContextDesc& desc );
		~Impl();

		void Initialize();
		void InitializeFactory();
		void InitializeDevice();
		void InitializeCommandQueues();
		void InitializeQueueContext( QueueContext& context, D3D12_COMMAND_LIST_TYPE type );
		void InitializeDescriptorHeaps();
		void InitializeRootSignature();
		void InitializeCommandSignature();
		QueueContext& GetQueueContext( QueueType type ) noexcept;
		const QueueContext& GetQueueContext( QueueType type ) const noexcept;
		QueueContext& GetGraphicsQueueContext() noexcept;
		const QueueContext& GetGraphicsQueueContext() const noexcept;
		SwapchainHandle CreateSwapchain( const SwapchainDesc& desc );
		void DestroySwapchain( SwapchainHandle swapchain ) noexcept;
		Swapchain* GetSwapchain( SwapchainHandle swapchain ) noexcept;
		const Swapchain* GetSwapchain( SwapchainHandle swapchain ) const noexcept;
		SwapchainDesc* GetSwapchainDesc( SwapchainHandle swapchain ) noexcept;
		const SwapchainDesc* GetSwapchainDesc( SwapchainHandle swapchain ) const noexcept;
		Swapchain* GetOwningSwapchain( TextureHandle texture ) noexcept;

		uint32_t AllocateBindlessDescriptor();
		uint32_t AllocateBindlessDescriptorRange( uint32_t count );
		uint32_t AllocateFixedBindlessDescriptor( uint32_t index );
		uint32_t AllocateRtvDescriptor();
		uint32_t AllocateDsvDescriptor();
		void FreeBindlessDescriptor( uint32_t index );
		void FreeBindlessDescriptorRange( uint32_t index, uint32_t count );
		void EraseFreeBindlessRange( uint32_t rangeIndex ) noexcept;
		void FreeRtvDescriptor( uint32_t index );
		void FreeDsvDescriptor( uint32_t index );

		BufferResource& GetBufferResource( BufferHandle handle );
		const BufferResource& GetBufferResource( BufferHandle handle ) const;
		TextureResource& GetTextureResource( TextureHandle handle );
		const TextureResource& GetTextureResource( TextureHandle handle ) const;

		void AddDeferredRelease( SubmitHandle handle, std::function<void()>&& release );
		void ProcessDeferredReleases();
		void ProcessDeferredReleases( QueueContext& context );
		void WaitForQueueIdle();
		void WaitForQueueIdle( QueueContext& context );
		void WaitIdle();
		void Shutdown() noexcept;
		void ReportLiveObjects() noexcept;

		ContextDesc desc_;
		ComPtr<IDXGIFactory6> factory_;
		ComPtr<IDXGIAdapter1> adapter_;
		ComPtr<ID3D12Device> device_;

		QueueContext graphicsQueue_ = QueueContext( QueueType::Graphics );
#ifndef LIGHTD3D12_SINGLE_DIRECT_QUEUE
		QueueContext computeQueue_ = QueueContext( QueueType::Compute );
		QueueContext copyQueue_ = QueueContext( QueueType::Copy );
#endif
		ComPtr<ID3D12DescriptorHeap> bindlessHeap_;
		ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		ComPtr<ID3D12DescriptorHeap> dsvHeap_;
		uint32_t bindlessDescriptorSize_ = 0;
		uint32_t rtvDescriptorSize_ = 0;
		uint32_t dsvDescriptorSize_ = 0;
		struct DescriptorRange final
		{
			uint32_t start_ = 0;
			uint32_t count_ = 0;
		};
		struct BindingSlotMasks final
		{
			uint32_t constantBuffer_ = 0;
			uint32_t shaderResource_ = 0;
			uint32_t readWriteResource_ = 0;
		};
		std::array<DescriptorRange, ourMaxBindlessDescriptors> freeBindlessRanges_ = {};
		std::array<uint8_t, LIGHTD3D12_BINDLESS_DYNAMIC_SLOT_FIRST> fixedBindlessDescriptorUsed_ = {};
		std::array<uint32_t, ourMaxRtvDescriptors> freeRtvDescriptors_ = {};
		std::array<uint32_t, ourMaxDsvDescriptors> freeDsvDescriptors_ = {};
		uint32_t freeBindlessRangeCount_ = 0;
		uint32_t freeRtvDescriptorCount_ = 0;
		uint32_t freeDsvDescriptorCount_ = 0;
		ComPtr<ID3D12RootSignature> rootSignature_;
		ComPtr<ID3D12CommandSignature> commandSignature_;
		SlotMap<SwapchainResource, ourMaxSwapchains> slotMapSwapchains_;
		SlotMap<BufferResource, ourMaxBuffers> slotMapBuffers_;
		SlotMap<TextureResource, ourMaxTextures> slotMapTextures_;
		std::unique_ptr<StagingDevice> stagingDevice_;
		std::unique_ptr<BaseMips> baseMips_;
		BindingSlotMasks allocatedFreeBindingSlots_;
		bool bindlessSupported_ = false;
	};
}
