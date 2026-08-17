#pragma once

#include "FrontierRenderQueue.hpp"
#include "FrontierWorld.hpp"
#include "LightD3D12/LightD3D12.hpp"

namespace frontier
{
	class FrontierRenderer final
	{
	public:
		static FrontierRenderer& Get() noexcept;

		FrontierRenderer( const FrontierRenderer& ) = delete;
		FrontierRenderer& operator=( const FrontierRenderer& ) = delete;
		FrontierRenderer( FrontierRenderer&& ) = delete;
		FrontierRenderer& operator=( FrontierRenderer&& ) = delete;

		void Initialize( lightd3d12::RenderDevice& device, DXGI_FORMAT colorFormat );
		void Shutdown();
		void Draw( lightd3d12::ICommandBuffer& commands, const GameState& world );

	private:
		FrontierRenderer() = default;

		lightd3d12::RenderDevice* device_ = nullptr;
		lightd3d12::RenderPipelineState spritePipeline_;
		lightd3d12::TextureHandle whiteTexture_;
		lightd3d12::TextureHandle fontTexture_;
		lightd3d12::TextureHandle backgroundTexture_;
		lightd3d12::TextureHandle midgroundTexture_;
		lightd3d12::TextureHandle foregroundTexture_;
		lightd3d12::TextureHandle cowboyTexture_;
		uint32_t whiteIndex_ = 0;
		uint32_t fontIndex_ = 0;
		uint32_t backgroundIndex_ = 0;
		uint32_t midgroundIndex_ = 0;
		uint32_t foregroundIndex_ = 0;
		uint32_t cowboyIndex_ = 0;
		RenderQueue renderQueue_;
	};
}
