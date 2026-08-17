#pragma once

#include "FrontierEvents.hpp"

#include <random>

namespace frontier
{
	class FrontierPhysics final
	{
	public:
		FrontierPhysics();

		void Reset();
		void Step( float deltaSeconds, const InputState& input );
		GameState& World() noexcept;
		const GameState& World() const noexcept;
		const FrameEvents& Events() const noexcept;

	private:
		void DamagePlayer( Vec2 impactDirection = {} );
		void FireBullet( Vec2 position, Vec2 direction, bool fromPlayer );
		void SpawnBurst( Vec2 position, Color color, uint32_t count );
		void SpawnDeathBlood( Vec2 feetPosition, Vec2 impactDirection, Vec2 facingDirection, Color bodyTint );
		void SpawnEnemy( Vec2 position );
		void StepEffects( float deltaSeconds );
		void StepPlayer( float deltaSeconds, const InputState& input );
		void SpawnEncounters();
		void StepEnemies( float deltaSeconds );
		void StepBullets( float deltaSeconds );
		float RandomRange( float minimum, float maximum );

		GameState state_;
		FrameEvents events_;
		std::mt19937 random_;
	};
}
