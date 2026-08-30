#pragma once

#include "Ldx12Internal.hpp"

#include <array>

namespace ldx12
{
	struct CommandListWrapper final
	{
		ComPtr<ID3D12CommandAllocator> allocator_;
		ComPtr<ID3D12GraphicsCommandList4> commandList_;
		ComPtr<ID3D12Fence> fence_;
		SubmitHandle handle_{};
		HANDLE fenceEvent_ = nullptr;
		uint64_t fenceValue_ = 0;
		bool isEncoding_ = false;
	};

	class CommandBufferImpl final : public ICommandBuffer
	{
	public:
		CommandBufferImpl() = default;
		void Begin( DeviceManager& manager, CommandListWrapper& wrapper ) noexcept;
		void Release() noexcept;

		void CmdBeginRendering( const RenderPass& renderPass, const Framebuffer& framebuffer ) override;
		void CmdEndRendering() override;
		void CmdSetViewport( float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f ) override;
		void CmdSetScissor( int32_t left, int32_t top, int32_t right, int32_t bottom ) override;
		void CmdTransitionTexture( TextureHandle texture, D3D12_RESOURCE_STATES newState ) override;
		void CmdResolveTexture( TextureHandle source, TextureHandle destination ) override;
		void CmdBindRenderPipeline( const RenderPipelineState& pipeline ) override;
		void CmdBindComputePipeline( const ComputePipelineState& pipeline ) override;
		void CmdBindVertexBuffer( BufferHandle buffer, uint32_t stride = 0, uint32_t offset = 0, uint32_t slot = 0 ) override;
		void CmdBindIndexBuffer( BufferHandle buffer, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT, uint32_t offset = 0 ) override;
		void CmdPushConstants( const void* data, uint32_t sizeBytes, uint32_t offset32BitValues = 0 ) override;
		void CmdPushDebugGroupLabel( const char* label, uint32_t color ) override;
		void CmdPopDebugGroupLabel() override;
		void CmdDraw( uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0 ) override;
		void CmdDrawIndexed( uint32_t indexCount,
			uint32_t instanceCount = 1,
			uint32_t firstIndex = 0,
			int32_t vertexOffset = 0,
			uint32_t firstInstance = 0 ) override;
		void CmdDrawIndexedIndirect( BufferHandle indirectBuffer, uint32_t drawCount, uint64_t byteOffset = 0 ) override;
		void CmdDispatch( uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1 ) override;

		struct TrackedTextureState final
		{
			TextureHandle handle_ = {};
			D3D12_RESOURCE_STATES initialState_ = D3D12_RESOURCE_STATE_COMMON;
			D3D12_RESOURCE_STATES currentState_ = D3D12_RESOURCE_STATE_COMMON;
		};

		CommandListWrapper& Wrapper() noexcept
		{
			return *wrapper_;
		}

		bool IsActive() const noexcept
		{
			return active_;
		}

		bool IsRendering() const noexcept
		{
			return isRendering_;
		}

		const TrackedTextureState* GetTrackedTextures() const noexcept
		{
			return trackedTextures_.data();
		}

		uint32_t GetTrackedTextureCount() const noexcept
		{
			return trackedTextureCount_;
		}

		CommandListWrapper* BuildSubmitFixup( CommandBufferImpl* const* previousCommandBuffers = nullptr, uint32_t previousCommandBufferCount = 0 );
		void CommitSubmittedTextureStates();

	private:
		ID3D12GraphicsCommandList* GetNativeGraphicsCommandList() override;
		TrackedTextureState& GetTrackedTextureState( TextureHandle texture );
		void TransitionTexture( TextureHandle texture, TextureResource& resource, D3D12_RESOURCE_STATES newState );

		DeviceManager* manager_ = nullptr;
		CommandListWrapper* wrapper_ = nullptr;
		bool isRendering_ = false;
		bool active_ = false;
		uint32_t debugGroupDepth_ = 0;
		std::array<TrackedTextureState, ourMaxTrackedTexturesPerCommandBuffer> trackedTextures_ = {};
		uint32_t trackedTextureCount_ = 0;
	};
}
