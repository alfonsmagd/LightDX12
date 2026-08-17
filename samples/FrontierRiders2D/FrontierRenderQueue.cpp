#include "FrontierRenderQueue.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace frontier
{
	namespace
	{
		uint32_t CheckedIndex( size_t index )
		{
			if( index > std::numeric_limits<uint32_t>::max() )
				throw std::runtime_error( "La cola de render ha superado su capacidad." );
			return static_cast<uint32_t>( index );
		}
	}

	void RenderQueue::Reserve( size_t spriteCount, size_t particleCount, size_t textCount )
	{
		sprites_.reserve( spriteCount );
		particles_.reserve( particleCount );
		texts_.reserve( textCount );
		drawOrder_.reserve( spriteCount + particleCount + textCount );
	}

	void RenderQueue::Clear() noexcept
	{
		sprites_.clear();
		particles_.clear();
		texts_.clear();
		drawOrder_.clear();
	}

	void RenderQueue::RecordSprite( uint32_t textureIndex, const RectF& rect, const UvRect& uv,
		const Color& tint, float rotation )
	{
		const uint32_t index = CheckedIndex( sprites_.size() );
		sprites_.push_back( { textureIndex, rect, uv, tint, rotation } );
		drawOrder_.push_back( { RenderCommandType::Sprite, index } );
	}

	void RenderQueue::RecordParticle( const RectF& rect, const Color& tint, float rotation )
	{
		const uint32_t index = CheckedIndex( particles_.size() );
		particles_.push_back( { rect, tint, rotation } );
		drawOrder_.push_back( { RenderCommandType::Particle, index } );
	}

	void RenderQueue::RecordText( uint32_t fontTextureIndex, float x, float y, float scale,
		std::string text, const Color& color )
	{
		const uint32_t index = CheckedIndex( texts_.size() );
		texts_.push_back( { fontTextureIndex, x, y, scale, std::move( text ), color } );
		drawOrder_.push_back( { RenderCommandType::Text, index } );
	}
}
