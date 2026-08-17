#pragma once

#include "FrontierWorld.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace frontier
{
	struct RectF
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
	};

	struct UvRect
	{
		float u0 = 0.0f;
		float v0 = 0.0f;
		float u1 = 1.0f;
		float v1 = 1.0f;
	};

	struct SpriteDrawCommand
	{
		uint32_t textureIndex = 0;
		RectF rect;
		UvRect uv;
		Color tint;
		float rotation = 0.0f;
	};

	struct ParticleDrawCommand
	{
		RectF rect;
		Color tint;
		float rotation = 0.0f;
	};

	struct TextDrawCommand
	{
		uint32_t fontTextureIndex = 0;
		float x = 0.0f;
		float y = 0.0f;
		float scale = 1.0f;
		std::string text;
		Color color;
	};

	enum class RenderCommandType : uint8_t
	{
		Sprite,
		Particle,
		Text,
	};

	struct RenderCommandReference
	{
		RenderCommandType type = RenderCommandType::Sprite;
		uint32_t index = 0;
	};

	class RenderQueue final
	{
	public:
		void Reserve( size_t spriteCount, size_t particleCount, size_t textCount );
		void Clear() noexcept;

		void RecordSprite( uint32_t textureIndex, const RectF& rect, const UvRect& uv = {},
			const Color& tint = {}, float rotation = 0.0f );
		void RecordParticle( const RectF& rect, const Color& tint, float rotation = 0.0f );
		void RecordText( uint32_t fontTextureIndex, float x, float y, float scale,
			std::string text, const Color& color );

		const std::vector<SpriteDrawCommand>& Sprites() const noexcept { return sprites_; }
		const std::vector<ParticleDrawCommand>& Particles() const noexcept { return particles_; }
		const std::vector<TextDrawCommand>& Texts() const noexcept { return texts_; }
		const std::vector<RenderCommandReference>& DrawOrder() const noexcept { return drawOrder_; }

	private:
		std::vector<SpriteDrawCommand> sprites_;
		std::vector<ParticleDrawCommand> particles_;
		std::vector<TextDrawCommand> texts_;
		std::vector<RenderCommandReference> drawOrder_;
	};
}
