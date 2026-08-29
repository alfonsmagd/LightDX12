#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Ldx12/HandleSlotMap.hpp"
#include "Ldx12/Ldx12_Defines.hpp"

namespace ldx12
{
	using Microsoft::WRL::ComPtr;
	static constexpr uint32_t ourMaxColorAttachments = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
	static constexpr uint32_t ourMaxShaderIncludeDirectories = 8;
	static constexpr uint32_t ourMaxVertexInputElements = 16;
	static constexpr uint32_t ourMaxBindlessDescriptors = 4096;
	static constexpr uint32_t ourMaxRtvDescriptors = 256;
	static constexpr uint32_t ourMaxDsvDescriptors = 64;
	static constexpr uint32_t ourMaxActiveCommandBuffers = 64;
	static constexpr uint32_t ourMaxCommandBufferBatch = 4;
	static constexpr uint32_t ourMaxImmediateCommandBuffers = ourMaxActiveCommandBuffers + ourMaxCommandBufferBatch;
	static constexpr uint32_t ourMaxTrackedTexturesPerCommandBuffer = 256;
	static constexpr uint32_t ourMaxPushConstant32BitValues = 63;
	static constexpr uint32_t ourCubeMapFaceCount = 6;
	static constexpr uint32_t ourBuiltInSamplerCount = LDX12_BUILT_IN_SAMPLER_COUNT;
	static constexpr uint32_t ourCustomSamplerCount = LDX12_CUSTOM_SAMPLER_COUNT;
	static constexpr uint32_t ourMaxSamplers = LDX12_SAMPLER_COUNT;

	struct BufferResource;
	struct SamplerResource;
	struct TextureResource;
	struct SwapchainResource;
	class BaseMips;
	class CommandBufferImpl;
	class D3D12Native;
	class ImmediateCommands;
	class StagingDevice;
	class Swapchain;

	struct NativeWindowHandle
	{
		enum class Type
		{
			Win32Hwnd
		};

		Type type = Type::Win32Hwnd;
		void* value = nullptr;

		constexpr bool Valid() const noexcept
		{
			return value != nullptr;
		}
	};

	inline NativeWindowHandle MakeWin32WindowHandle( HWND hwnd ) noexcept
	{
		return { NativeWindowHandle::Type::Win32Hwnd, hwnd };
	}

	// Call this before creating DeviceManager if you want PIX GPU capture attach support.
	// Returns false when WinPixGpuCapturer.dll cannot be found in the installed PIX versions.
	bool TryLoadPixGpuCapturer() noexcept;
	bool IsPixGpuCapturerLoaded() noexcept;

	using TextureHandle = Handle<TextureResource>;
	using BufferHandle = Handle<BufferResource>;
	using SamplerHandle = Handle<SamplerResource>;
	using SwapchainHandle = Handle<SwapchainResource>;

	inline std::string BuildScopedCommandLabel( const char* functionSignature )
	{
		if( functionSignature == nullptr || functionSignature[ 0 ] == '\0' )
		{
			return {};
		}

		std::string_view signature( functionSignature );
		std::string_view functionName = signature;
		if( const size_t openParenthesis = signature.find( '(' ); openParenthesis != std::string_view::npos )
		{
			functionName = signature.substr( 0, openParenthesis );
		}

		while( !functionName.empty() && functionName.back() == ' ' )
		{
			functionName.remove_suffix( 1 );
		}

		if( const size_t lastSpace = functionName.rfind( ' ' ); lastSpace != std::string_view::npos )
		{
			functionName.remove_prefix( lastSpace + 1 );
		}

		return std::string( functionName );
	}

	struct SubmitHandle
	{
		uint32_t bufferIndex_ = 0;
		uint32_t submitId_ = 0;

		SubmitHandle() = default;

		explicit SubmitHandle( uint64_t handle ) noexcept:
			bufferIndex_( static_cast< uint32_t >( handle & 0xffffffffu ) ),
			submitId_( static_cast< uint32_t >( handle >> 32u ) )
		{
		}

		constexpr bool Empty() const noexcept
		{
			return submitId_ == 0;
		}

		constexpr uint64_t Handle() const noexcept
		{
			return ( static_cast< uint64_t >( submitId_ ) << 32u ) + bufferIndex_;
		}
	};

	enum class LoadOp : uint8_t
	{
		Load,
		Clear,
		DontCare
	};

	enum class StoreOp : uint8_t
	{
		Store,
		DontCare
	};

	struct ColorAttachmentDesc
	{
		LoadOp loadOp = LoadOp::Load;
		StoreOp storeOp = StoreOp::Store;
		std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	struct DepthStencilAttachmentDesc
	{
		LoadOp depthLoadOp = LoadOp::Load;
		StoreOp depthStoreOp = StoreOp::Store;
		LoadOp stencilLoadOp = LoadOp::Load;
		StoreOp stencilStoreOp = StoreOp::Store;
		float clearDepth = 1.0f;
		uint8_t clearStencil = 0;
	};

	struct RenderPass
	{
		std::array<ColorAttachmentDesc, ourMaxColorAttachments> color = {};
		DepthStencilAttachmentDesc depthStencil = {};
	};

	struct ColorAttachment
	{
		TextureHandle texture = {};
	};

	struct DepthStencilAttachment
	{
		TextureHandle texture = {};
	};

	struct Framebuffer
	{
		std::array<ColorAttachment, ourMaxColorAttachments> color = {};
		DepthStencilAttachment depthStencil = {};
	};

	struct RenderPipelineColorAttachmentDesc
	{
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	};

	struct VertexInputElementDesc
	{
		std::string semanticName;
		uint32_t semanticIndex = 0;
		DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT;
		uint32_t inputSlot = 0;
		uint32_t alignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
		D3D12_INPUT_CLASSIFICATION inputClassification = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		uint32_t instanceDataStepRate = 0;
	};

	struct ShaderStageSource
	{
		const char* source = nullptr;
		const char* entryPoint = "main";
		const char* profile = nullptr;
		std::string sourceName;
		std::array<std::string, ourMaxShaderIncludeDirectories> includeDirectories = {};
	};

	struct RenderPipelineDesc
	{
		RenderPipelineDesc() noexcept;

		std::array<RenderPipelineColorAttachmentDesc, ourMaxColorAttachments> color = {};
		std::array<VertexInputElementDesc, ourMaxVertexInputElements> inputElements = {};
		ShaderStageSource vertexShader = {};
		ShaderStageSource fragmentShader = {};
		D3D12_BLEND_DESC blendState = {};
		D3D12_RASTERIZER_DESC rasterizerState = {};
		D3D12_DEPTH_STENCIL_DESC depthStencilState = {};
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
		uint32_t sampleCount = 1;
	};

	struct ComputePipelineDesc
	{
		ShaderStageSource computeShader = {};
	};

	enum class BufferType : uint8_t
	{
		Generic,
		Vertex,
		Index,
		Constant,
		Structured,
		Raw,
		Indirect,
	};

	enum class BufferMemory : uint8_t
	{
		GpuLocal,
		CpuToGpu,
	};

	struct BufferDesc
	{
		std::string debugName;
		uint64_t size = 0;
		uint32_t stride = 0;
		BufferType type = BufferType::Generic;
		BufferMemory memory = BufferMemory::GpuLocal;
		const void* initialData = nullptr;
	};

	enum class TextureUsage : uint32_t
	{
		None = 0,
		Sampled = 1u << 0,
		RenderTarget = 1u << 1,
		DepthStencil = 1u << 2,
		UnorderedAccess = 1u << 3,
	};

	constexpr TextureUsage operator|( TextureUsage lhs, TextureUsage rhs ) noexcept
	{
		return static_cast<TextureUsage>( static_cast<uint32_t>( lhs ) | static_cast<uint32_t>( rhs ) );
	}

	constexpr TextureUsage operator&( TextureUsage lhs, TextureUsage rhs ) noexcept
	{
		return static_cast<TextureUsage>( static_cast<uint32_t>( lhs ) & static_cast<uint32_t>( rhs ) );
	}

	inline TextureUsage& operator|=( TextureUsage& lhs, TextureUsage rhs ) noexcept
	{
		lhs = lhs | rhs;
		return lhs;
	}

	constexpr bool HasTextureUsage( TextureUsage usage, TextureUsage bit ) noexcept
	{
		return ( usage & bit ) != TextureUsage::None;
	}

	enum class TextureDimension : uint8_t
	{
		Texture2D,
		Texture2DArray,
		TextureCube,
		Texture3D,
	};

	struct TextureDesc
	{
		std::string debugName;
		uint32_t width = 1;
		uint32_t height = 1;
		uint16_t countMipMap = 1;
		uint16_t depthOrArraySize = 1;
		TextureDimension dimension = TextureDimension::Texture2D;
		DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint32_t sampleCount = 1;
		TextureUsage usage = TextureUsage::Sampled;
		// Optional flags for interoperating with another D3D API. Only
		// D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS is accepted by the
		// lightweight resource layer; normal render/depth/UAV flags continue to
		// be inferred from usage above.
		D3D12_RESOURCE_FLAGS additionalResourceFlags = D3D12_RESOURCE_FLAG_NONE;
		D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
		// Texture2DArray data contains tightly packed base-mip slices and
		// slicePitch advances one slice. TextureCube uses six slices in D3D12
		// order: +X, -X, +Y, -Y, +Z, -Z.
		const void* data = nullptr;
		uint32_t rowPitch = 0;
		uint32_t slicePitch = 0;
		bool useClearValue = false;
		D3D12_CLEAR_VALUE clearValue{};
	};

	struct PixSettings
	{
		bool enableGpuCapture = false;
		bool showGpuCaptureHud = false;
	};

	struct DeviceProperties
	{
		std::string adapterName;
		uint64_t dedicatedVideoMemoryBytes = 0;
		uint64_t dedicatedSystemMemoryBytes = 0;
		uint64_t sharedSystemMemoryBytes = 0;
		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
		D3D_SHADER_MODEL shaderModel = D3D_SHADER_MODEL_5_1;
		D3D12_RESOURCE_BINDING_TIER resourceBindingTier = D3D12_RESOURCE_BINDING_TIER_1;
	};

	struct SamplerDesc
	{
		D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		float mipLodBias = 0.0f;
		uint32_t maxAnisotropy = 1;
		D3D12_COMPARISON_FUNC comparisonFunction = D3D12_COMPARISON_FUNC_ALWAYS;
		std::array<float, 4> borderColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		float minLod = 0.0f;
		float maxLod = D3D12_FLOAT32_MAX;

	};

	struct ContextDesc
	{
		bool enableDebugLayer = true;
		bool preferHighPerformanceAdapter = true;
		bool allowTearing = true;
		PixSettings pixSettings = {};
		uint32_t bindlessCapacity = ourMaxBindlessDescriptors;
		uint32_t rtvCapacity = ourMaxRtvDescriptors;
		uint32_t dsvCapacity = ourMaxDsvDescriptors;
		uint32_t swapchainBufferCount = 3;
		DXGI_FORMAT swapchainFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_12_0;
	};

	struct SwapchainDesc
	{
		NativeWindowHandle window = {};
		uint32_t width = 1;
		uint32_t height = 1;
		bool vsync = true;
	};

	struct SamplerResource final
	{
		uint32_t descriptorIndex_ = UINT32_MAX;
	};

	struct BufferResource final
	{
		[[nodiscard]] uint8_t* GetMappedPtr() const noexcept
		{
			return static_cast<uint8_t*>( mappedPtr_ );
		}

		[[nodiscard]] bool IsMapped() const noexcept
		{
			return mappedPtr_ != nullptr;
		}

		void BufferSubData( size_t offset, size_t size, const void* data );
		[[nodiscard]] D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView( uint32_t stride = 0 ) const noexcept;
		[[nodiscard]] D3D12_INDEX_BUFFER_VIEW GetIndexBufferView( DXGI_FORMAT format = DXGI_FORMAT_R32_UINT ) const noexcept;

		static D3D12_RESOURCE_DESC CreateNativeDesc( uint64_t size ) noexcept;

		ComPtr<ID3D12Resource> resource_;
		D3D12_GPU_VIRTUAL_ADDRESS gpuAddress_ = 0;
		uint64_t bufferSize_ = 0;
		uint32_t bufferStride_ = 0;
		BufferType type_ = BufferType::Generic;
		D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;
		D3D12_RESOURCE_DESC desc_ = {};
		BufferMemory memory_ = BufferMemory::GpuLocal;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle_{ 0 };
		D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle_{ 0 };
		uint32_t srvIndex_ = UINT32_MAX;
		uint32_t cbvIndex_ = UINT32_MAX;
		void* mappedPtr_ = nullptr;
	};

	struct TextureResource final
	{
		struct ResolvedFormats final
		{
			DXGI_FORMAT resource_ = DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT srv_ = DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT uav_ = DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT rtv_ = DXGI_FORMAT_UNKNOWN;
			DXGI_FORMAT dsv_ = DXGI_FORMAT_UNKNOWN;
		};

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const noexcept
		{
			return rtvHandle_;
		}

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const noexcept
		{
			return dsvHandle_;
		}

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const noexcept
		{
			return srvHandle_;
		}

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetUAV() const noexcept
		{
			return uavHandle_;
		}

		static bool IsDepthFormat( DXGI_FORMAT format ) noexcept;
		static bool IsDepthStencilFormat( DXGI_FORMAT format ) noexcept;

		ComPtr<ID3D12Resource> resource_;
		D3D12_RESOURCE_FLAGS usageFlags_ = D3D12_RESOURCE_FLAG_NONE;
		D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;
		DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
		ResolvedFormats formats_ = {};
		D3D12_RESOURCE_DESC desc_ = {};
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{ 0 };
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{ 0 };
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandle_{ 0 };
		D3D12_CPU_DESCRIPTOR_HANDLE uavHandle_{ 0 };
		uint32_t rtvIndex_ = UINT32_MAX;
		uint32_t dsvIndex_ = UINT32_MAX;
		uint32_t srvIndex_ = UINT32_MAX;
		uint32_t uavIndex_ = UINT32_MAX;
		uint32_t baseMipsUavBaseIndex_ = UINT32_MAX;
		uint32_t width_ = 0;
		uint32_t height_ = 0;
		uint16_t mipLevels_ = 1;
		uint16_t depthOrArraySize_ = 1;
		uint16_t baseMipsUavCount_ = 0;
		TextureDimension dimension_ = TextureDimension::Texture2D;
		bool isDepthFormat_ = false;
		bool isStencilFormat_ = false;
		bool isSwapchainImage_ = false;
		SwapchainHandle swapchain_ = {};
	};

	struct SwapchainResource final
	{
		SwapchainResource();
		~SwapchainResource();
		SwapchainResource( SwapchainResource&& other ) noexcept;
		SwapchainResource& operator=( SwapchainResource&& other ) noexcept;
		SwapchainResource( const SwapchainResource& ) = delete;
		SwapchainResource& operator=( const SwapchainResource& ) = delete;

		SwapchainDesc desc_ = {};
		std::unique_ptr<Swapchain> swapchain_;
	};

	class RenderPipelineState
	{
	public:
		RenderPipelineState() = default;
		RenderPipelineState( RenderPipelineState&& other ) noexcept;
		RenderPipelineState& operator=( RenderPipelineState&& other ) noexcept;
		RenderPipelineState( const RenderPipelineState& ) = delete;
		RenderPipelineState& operator=( const RenderPipelineState& ) = delete;
		~RenderPipelineState() = default;

		bool Valid() const noexcept;

	private:
		friend class Context;
		friend class RenderDevice;
		friend class CommandBufferImpl;

		ComPtr<ID3D12PipelineState> pipelineState_;
		D3D_PRIMITIVE_TOPOLOGY topology_ = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	};

	class ComputePipelineState
	{
	public:
		ComputePipelineState() = default;
		ComputePipelineState( ComputePipelineState&& other ) noexcept;
		ComputePipelineState& operator=( ComputePipelineState&& other ) noexcept;
		ComputePipelineState( const ComputePipelineState& ) = delete;
		ComputePipelineState& operator=( const ComputePipelineState& ) = delete;
		~ComputePipelineState() = default;

		bool Valid() const noexcept;

	private:
		friend class Context;
		friend class RenderDevice;
		friend class CommandBufferImpl;

		ComPtr<ID3D12PipelineState> pipelineState_;
	};

	class ICommandBuffer
	{
	public:
		virtual ~ICommandBuffer() = default;

		virtual void CmdBeginRendering( const RenderPass& renderPass, const Framebuffer& framebuffer ) = 0;
		virtual void CmdEndRendering() = 0;
		virtual void CmdSetViewport( float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f ) = 0;
		virtual void CmdSetScissor( int32_t left, int32_t top, int32_t right, int32_t bottom ) = 0;
		virtual void CmdTransitionTexture( TextureHandle texture, D3D12_RESOURCE_STATES newState ) = 0;
		virtual void CmdResolveTexture( TextureHandle source, TextureHandle destination ) = 0;
		virtual void CmdBindRenderPipeline( const RenderPipelineState& pipeline ) = 0;
		virtual void CmdBindComputePipeline( const ComputePipelineState& pipeline ) = 0;
		virtual void CmdBindVertexBuffer( BufferHandle buffer, uint32_t stride = 0, uint32_t offset = 0, uint32_t slot = 0 ) = 0;
		virtual void CmdBindIndexBuffer( BufferHandle buffer, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT, uint32_t offset = 0 ) = 0;
		virtual void CmdPushConstants( const void* data, uint32_t sizeBytes, uint32_t offset32BitValues = 0 ) = 0;
		virtual void CmdPushDebugGroupLabel( const char* label, uint32_t color ) = 0;
		virtual void CmdPopDebugGroupLabel() = 0;
		virtual void CmdDraw( uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0 ) = 0;
		virtual void CmdDrawIndexed( uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0 ) = 0;
		virtual void CmdDrawIndexedIndirect( BufferHandle indirectBuffer, uint32_t drawCount, uint64_t byteOffset = 0 ) = 0;
		virtual void CmdDispatch( uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1 ) = 0;

	private:
		friend class D3D12Native;

		virtual ID3D12GraphicsCommandList* GetNativeGraphicsCommandList() = 0;
	};

	class ScopedCommandDebugGroup final
	{
	public:
		ScopedCommandDebugGroup( ICommandBuffer& commandBuffer, std::string label, uint32_t color = 0xff4cc9f0u ):
			commandBuffer_( &commandBuffer ),
			label_( std::move( label ) ),
			active_( !label_.empty() )
		{
			if( active_ )
			{
				commandBuffer_->CmdPushDebugGroupLabel( label_.c_str(), color );
			}
		}

		ScopedCommandDebugGroup( ICommandBuffer& commandBuffer, const char* label, uint32_t color = 0xff4cc9f0u ):
			ScopedCommandDebugGroup( commandBuffer, label != nullptr ? std::string( label ) : std::string{}, color )
		{
		}

		~ScopedCommandDebugGroup()
		{
			if( active_ )
			{
				commandBuffer_->CmdPopDebugGroupLabel();
			}
		}

		ScopedCommandDebugGroup( const ScopedCommandDebugGroup& ) = delete;
		ScopedCommandDebugGroup& operator=( const ScopedCommandDebugGroup& ) = delete;

	private:
		ICommandBuffer* commandBuffer_ = nullptr;
		std::string label_;
		bool active_ = false;
	};

	class DeviceManager;

	class RenderDevice
	{
	public:
		ICommandBuffer& AcquireCommandBuffer();
		TextureHandle GetCurrentSwapchainTexture( SwapchainHandle swapchain = {} ) const;
		// Submits commandBuffers[0..commandBufferCount) in array order as one queue batch.
		// If presentTexture is valid, the last command buffer transitions and presents it.
		SubmitHandle SubmitBatch( ICommandBuffer* const* commandBuffers, uint32_t commandBufferCount, TextureHandle presentTexture = {} ) const;
		SubmitHandle Submit( ICommandBuffer& buffer, TextureHandle presentTexture );
		SubmitHandle Submit( ICommandBuffer& buffer ) const;
		SubmitHandle SubmitAndPresent( ICommandBuffer& buffer, SwapchainHandle swapchain );
		void Present( SwapchainHandle swapchain ) const;
		bool IsReady( SubmitHandle submission ) const;
		void Wait( SubmitHandle submission ) const;

		RenderPipelineState CreateRenderPipeline( const RenderPipelineDesc& desc );
		ComputePipelineState CreateComputePipeline( const ComputePipelineDesc& desc );
		BufferHandle CreateBuffer( const BufferDesc& desc );
		BufferHandle CreateBuffer( const BufferDesc& desc, ConstantBufferSlot slot );
		BufferHandle CreateBuffer( const BufferDesc& desc, ShaderResourceSlot slot );
		void WriteBuffer( BufferHandle buffer, uint64_t offset, const void* data, uint64_t size );
		TextureHandle CreateTexture( const TextureDesc& desc );
		SamplerHandle CreateSampler( const SamplerDesc& desc );
		void DownloadTexture2D( TextureHandle texture, void* outData, uint32_t rowPitch, uint32_t slicePitch );
		ConstantBufferSlot GetAvailableConstantBuffer();
		ShaderResourceSlot GetAvailableShaderResource();
		ReadWriteResourceSlot GetAvailableReadWriteResource();
		uint32_t GetConstantBufferIndex( BufferHandle buffer ) const;
		uint32_t GetBindlessIndex( BufferHandle buffer ) const;
		uint32_t GetBindlessIndex( TextureHandle texture ) const;
		uint32_t GetUnorderedAccessIndex( TextureHandle texture ) const;
		uint32_t GetSamplerIndex( SamplerHandle sampler ) const;
		bool SupportsSampleCount( DXGI_FORMAT format, uint32_t sampleCount ) const noexcept;
		[[nodiscard]] D3D12Native GetNative() noexcept;
		bool BindlessSupported() const noexcept;
		bool IsAlive( BufferHandle buffer ) const noexcept;
		bool IsAlive( TextureHandle texture ) const noexcept;
		bool IsAlive( SamplerHandle sampler ) const noexcept;
		void WaitIdle();
		bool Destroy( BufferHandle buffer );
		bool Destroy( TextureHandle texture );
		bool Destroy( SamplerHandle sampler );

	private:
		friend class DeviceManager;
		friend class D3D12Native;

		explicit RenderDevice( DeviceManager& manager ) noexcept;
		void ValidateBufferDesc( const BufferDesc& desc ) const;
		BufferResource CreateBufferResource( const BufferDesc& desc );
		void CreateBufferDescriptors( BufferResource& resource, const BufferDesc& desc, uint32_t constantBufferSlot, uint32_t shaderResourceSlot );
		BufferHandle CreateBufferInternal( const BufferDesc& desc, uint32_t constantBufferSlot, uint32_t shaderResourceSlot );

		DeviceManager* manager_ = nullptr;
	};

	class DeviceManager
	{
	public:
		static DeviceManager& Initialize( const ContextDesc& desc );
		static DeviceManager& Initialize( const ContextDesc& desc, const SwapchainDesc& primarySwapchainDesc );
		static DeviceManager& Get();
		static void ShutdownSingleton();

		~DeviceManager();
		DeviceManager( DeviceManager&& ) = delete;
		DeviceManager& operator=( DeviceManager&& ) = delete;
		DeviceManager( const DeviceManager& ) = delete;
		DeviceManager& operator=( const DeviceManager& ) = delete;

		SwapchainHandle CreateSwapchain( const SwapchainDesc& desc );
		void DestroySwapchain( SwapchainHandle swapchain );
		RenderDevice* GetRenderDevice() noexcept;
		const RenderDevice* GetRenderDevice() const noexcept;
		const DeviceProperties& GetDeviceProperties() const noexcept;

		void Resize( uint32_t width, uint32_t height );
		void Resize( SwapchainHandle swapchain, uint32_t width, uint32_t height );
		uint32_t GetWidth() const noexcept;
		uint32_t GetWidth( SwapchainHandle swapchain ) const noexcept;
		uint32_t GetHeight() const noexcept;
		uint32_t GetHeight( SwapchainHandle swapchain ) const noexcept;
		bool IsVsyncEnabled() const noexcept;
		bool IsVsyncEnabled( SwapchainHandle swapchain ) const noexcept;
		void SetVsync( bool enabled ) noexcept;
		void SetVsync( SwapchainHandle swapchain, bool enabled ) noexcept;
		void WaitIdle();

	private:
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
			QueueContext();
			~QueueContext();
			QueueContext( const QueueContext& ) = delete;
			QueueContext& operator=( const QueueContext& ) = delete;

			ComPtr<ID3D12CommandQueue> commandQueue_;
			ComPtr<ID3D12Fence> queueIdleFence_;
			HANDLE queueIdleEvent_ = nullptr;
			uint64_t queueIdleFenceValue_ = 0;
			std::unique_ptr<ImmediateCommands> immediateCommands_;
			std::deque<DeferredRelease> deferredReleases_;
		};

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

		explicit DeviceManager( const ContextDesc& desc );
		SwapchainHandle RequirePrimarySwapchain() const;
		void Initialize();
		void InitializeFactory();
		void InitializeDevice();
		bool CheckCapabilities( std::string& failureReason );
		void InitializeCommandQueues();
		void InitializeQueueContext( QueueContext& context, D3D12_COMMAND_LIST_TYPE type );
		void InitializeDescriptorHeaps();
		void WriteSamplerDescriptor( uint32_t index, const SamplerDesc& desc );
		void InitializeRootSignature();
		void InitializeCommandSignature();
		QueueContext& GetGraphicsQueueContext() noexcept;
		const QueueContext& GetGraphicsQueueContext() const noexcept;
		SwapchainHandle CreateSwapchainInternal( const SwapchainDesc& desc );
		void DestroySwapchainInternal( SwapchainHandle swapchain ) noexcept;
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
		void Shutdown() noexcept;
		void ReportLiveObjects() noexcept;
		void CreateCommittedTextureResource( const TextureDesc& desc, TextureResource& resource );
		D3D12_CPU_DESCRIPTOR_HANDLE MakeBindlessCpuHandle( uint32_t index ) noexcept;
		void CreateTextureShaderResourceView( TextureResource& resource );
		void CreateTextureBaseMipViews( TextureResource& resource );
		void CreateTextureUnorderedAccessView( TextureResource& resource );
		void CreateTextureRenderTargetView( TextureResource& resource );
		void CreateTextureDepthStencilView( TextureResource& resource );

		friend class RenderDevice;
		friend class D3D12Native;
		friend class BaseMips;
		friend class CommandBufferImpl;
		friend class StagingDevice;
		friend class Swapchain;
		friend SubmitHandle SubmitCommandBufferBatch( DeviceManager& manager, ICommandBuffer* const* commandBuffers, uint32_t commandBufferCount, TextureHandle presentTexture );

		ContextDesc desc_;
		DeviceProperties deviceProperties_;
		ComPtr<IDXGIFactory6> factory_;
		ComPtr<IDXGIAdapter1> adapter_;
		ComPtr<ID3D12Device> device_;
		QueueContext graphicsQueue_;
		ComPtr<ID3D12DescriptorHeap> bindlessHeap_;
		ComPtr<ID3D12DescriptorHeap> samplerHeap_;
		ComPtr<ID3D12DescriptorHeap> rtvHeap_;
		ComPtr<ID3D12DescriptorHeap> dsvHeap_;
		uint32_t bindlessDescriptorSize_ = 0;
		uint32_t samplerDescriptorSize_ = 0;
		uint32_t rtvDescriptorSize_ = 0;
		uint32_t dsvDescriptorSize_ = 0;
		std::array<DescriptorRange, ourMaxBindlessDescriptors> freeBindlessRanges_ = {};
		std::array<uint8_t, LDX12_BINDLESS_DYNAMIC_SLOT_FIRST> fixedBindlessDescriptorUsed_ = {};
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
		SlotMap<SamplerResource, ourCustomSamplerCount> slotMapSamplers_;
		std::unique_ptr<StagingDevice> stagingDevice_;
		std::unique_ptr<BaseMips> baseMips_;
		BindingSlotMasks allocatedFreeBindingSlots_;
		SwapchainHandle primarySwapchain_ = {};
		RenderDevice renderDevice_;
	};
}

#define LDX12_DETAIL_CONCAT_INNER( a, b ) a##b
#define LDX12_DETAIL_CONCAT( a, b ) LDX12_DETAIL_CONCAT_INNER( a, b )
#if defined( _MSC_VER )
	#define LDX12_DETAIL_FUNCTION_SIGNATURE __FUNCSIG__
#elif defined( __clang__ ) || defined( __GNUC__ )
	#define LDX12_DETAIL_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#else
	#define LDX12_DETAIL_FUNCTION_SIGNATURE __FUNCTION__
#endif
#define LDX12_CMD_SCOPE( commandBuffer ) ::ldx12::ScopedCommandDebugGroup LDX12_DETAIL_CONCAT( ldx12CmdScope_, __LINE__ )( commandBuffer, ::ldx12::BuildScopedCommandLabel( LDX12_DETAIL_FUNCTION_SIGNATURE ) )
#define LDX12_CMD_SCOPE_NAMED( commandBuffer, label, color ) ::ldx12::ScopedCommandDebugGroup LDX12_DETAIL_CONCAT( ldx12CmdScope_, __LINE__ )( commandBuffer, label, color )

