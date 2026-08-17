#include "FrontierRun.hpp"
#include "FrontierRenderer.hpp"
#include "AudioManager2D.hpp"

#include <algorithm>
#include <cmath>

namespace frontier
{
	namespace
	{
		void PlayAudioEvents( const FrameEvents& events, const GameState& world )
		{
			AudioManager2D& audio = AudioManager2D::Get();
			for( const BulletFiredEvent& event : events.bulletsFired )
			{
				const Vec2 listenerPosition = world.playerPosition;
				const float offsetX = event.position.x - listenerPosition.x;
				const float offsetY = event.position.y - listenerPosition.y;
				const float distance = std::sqrt( offsetX * offsetX + offsetY * offsetY );
				const float attenuation = std::clamp( 1.0f - distance / 1500.0f, 0.18f, 1.0f );
				AudioManager2D::PlayDesc sound;
				sound.volume = ( event.fromPlayer ? 0.95f : 0.62f ) * attenuation;
				sound.pan = std::clamp( offsetX / 700.0f, -1.0f, 1.0f );
				audio.Play( AudioManager2D::Sound::RevolverShot, sound );
			}
		}
	}

	void FrontierRun::Initialize( lightd3d12::RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		FrontierRenderer::Get().Initialize( device, colorFormat );
		AudioManager2D::Get().Initialize();
	}

	void FrontierRun::Shutdown()
	{
		AudioManager2D::Get().Shutdown();
		FrontierRenderer::Get().Shutdown();
	}

	void FrontierRun::Reset()
	{
		physics_.Reset();
	}

	void FrontierRun::Physics( float deltaSeconds, const InputState& input )
	{
		physics_.Step( deltaSeconds, input );
		PlayAudioEvents( physics_.Events(), physics_.World() );
	}

	void FrontierRun::Render( lightd3d12::ICommandBuffer& commands )
	{
		FrontierRenderer::Get().Draw( commands, physics_.World() );
	}

	const GameState& FrontierRun::World() const noexcept
	{
		return physics_.World();
	}
}
