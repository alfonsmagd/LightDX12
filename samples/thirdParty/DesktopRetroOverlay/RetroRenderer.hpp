#pragma once
#include <Ldx12/Ldx12.hpp>
#include <memory>
#include "EffectPresets.hpp"
namespace desktop_retro
{
	struct CapturedFrame
	{
		ldx12::TextureHandle texture;
		uint32_t width, height;
	};
	// Owns all effect GPU resources. Device must outlive this object.
	// Records into a caller-owned command buffer and output target. Input is returned
	// to COMMON. The caller submits, presents if needed, then signals capture completion.
	class RetroRenderer final
	{
	public:
		RetroRenderer( ldx12::RenderDevice&, DXGI_FORMAT );
		~RetroRenderer();
		RetroRenderer( const RetroRenderer& ) = delete;
		RetroRenderer& operator=( const RetroRenderer& ) = delete;
		void Record( ldx12::CommandBuffer&, ldx12::TextureHandle output, const CapturedFrame&, const EffectSettings&, float elapsedSeconds );
		void ResetHistory() noexcept;

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
