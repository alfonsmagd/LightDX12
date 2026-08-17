#include "FrontierPhysics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace frontier
{
	namespace
	{
		constexpr std::array<Vec2, 16> kEnemySpawns = {
			Vec2{ 930.0f, 570.0f }, Vec2{ 1230.0f, 640.0f },
			Vec2{ 1510.0f, 545.0f }, Vec2{ 1780.0f, 610.0f },
			Vec2{ 2070.0f, 650.0f }, Vec2{ 2300.0f, 560.0f },
			Vec2{ 2580.0f, 625.0f }, Vec2{ 2830.0f, 540.0f },
			Vec2{ 3100.0f, 655.0f }, Vec2{ 3340.0f, 580.0f },
			Vec2{ 3580.0f, 630.0f }, Vec2{ 3820.0f, 550.0f },
			Vec2{ 4090.0f, 605.0f }, Vec2{ 4330.0f, 655.0f },
			Vec2{ 4570.0f, 565.0f }, Vec2{ 4780.0f, 625.0f },
		};

		float Length( Vec2 value )
		{
			return std::sqrt( value.x * value.x + value.y * value.y );
		}

		Vec2 Normalize( Vec2 value )
		{
			const float length = Length( value );
			return length > 0.0001f ? Vec2{ value.x / length, value.y / length } : Vec2{};
		}

		bool Overlaps( Vec2 a, Vec2 aHalfSize, Vec2 b, Vec2 bHalfSize )
		{
			return std::abs( a.x - b.x ) <= aHalfSize.x + bHalfSize.x && std::abs( a.y - b.y ) <= aHalfSize.y + bHalfSize.y;
		}

		float MoveTowards( float current, float target, float maximumDelta )
		{
			if( std::abs( target - current ) <= maximumDelta ) return target;
			return current + std::copysign( maximumDelta, target - current );
		}

		Vec2 ActorCenter( Vec2 feetPosition )
		{
			return { feetPosition.x, feetPosition.y - 62.0f };
		}
	}

	FrontierPhysics::FrontierPhysics(): random_( 0xC0FFEEu )
	{
	}

	void FrontierPhysics::Reset()
	{
		state_ = GameState{};
		events_.Clear();
	}

	GameState& FrontierPhysics::World() noexcept
	{
		return state_;
	}

	const GameState& FrontierPhysics::World() const noexcept
	{
		return state_;
	}

	const FrameEvents& FrontierPhysics::Events() const noexcept
	{
		return events_;
	}

	float FrontierPhysics::RandomRange( float minimum, float maximum )
	{
		return std::uniform_real_distribution<float>( minimum, maximum )( random_ );
	}

	void FrontierPhysics::SpawnBurst( Vec2 position, Color color, uint32_t count )
	{
		for( uint32_t index = 0; index < count; ++index )
		{
			const float angle = RandomRange( 0.0f, 6.2831853f );
			const float speed = RandomRange( 55.0f, 190.0f );
			const float life = RandomRange( 0.2f, 0.55f );
			state_.particles.push_back( { position, { std::cos( angle ) * speed, std::sin( angle ) * speed }, life, life, color } );
		}
	}

	void FrontierPhysics::SpawnDeathBlood( Vec2 feetPosition, Vec2 impactDirection, Vec2 facingDirection, Color bodyTint )
	{
		constexpr std::array<Color, 5> bloodColors = {
			Color{ 0.72f, 0.015f, 0.01f, 1.0f },
			Color{ 0.48f, 0.006f, 0.008f, 1.0f },
			Color{ 0.92f, 0.035f, 0.015f, 1.0f },
			Color{ 0.34f, 0.004f, 0.006f, 1.0f },
			Color{ 0.62f, 0.025f, 0.018f, 1.0f },
		};
		impactDirection = Normalize( impactDirection );
		if( Length( impactDirection ) < 0.01f ) impactDirection = { 1.0f, 0.0f };
		const Vec2 perpendicular{ -impactDirection.y, impactDirection.x };
		const Vec2 center = ActorCenter( feetPosition );
		const float bodyLife = 4.8f;
		state_.deadBodies.push_back( {
			feetPosition,
			facingDirection,
			bodyTint,
			impactDirection.x >= 0.0f ? 1.42f : -1.42f,
			bodyLife,
			bodyLife,
		} );

		for( uint32_t index = 0; index < 145u; ++index )
		{
			Particle particle;
			particle.position = {
				center.x + impactDirection.x * 18.0f + RandomRange( -7.0f, 7.0f ),
				center.y + impactDirection.y * 15.0f + RandomRange( -9.0f, 9.0f ),
			};
			if( index < 108u )
			{
				const float forwardSpeed = RandomRange( 115.0f, 410.0f );
				const float lateralSpeed = RandomRange( -125.0f, 125.0f );
				particle.velocity = {
					impactDirection.x * forwardSpeed + perpendicular.x * lateralSpeed,
					impactDirection.y * forwardSpeed + perpendicular.y * lateralSpeed - RandomRange( 35.0f, 145.0f ),
				};
			}
			else
			{
				const float angle = RandomRange( 0.0f, 6.2831853f );
				const float mistSpeed = RandomRange( 45.0f, 185.0f );
				particle.velocity = {
					std::cos( angle ) * mistSpeed,
					std::sin( angle ) * mistSpeed - RandomRange( 20.0f, 90.0f ),
				};
			}
			const float life = RandomRange( 0.7f, 1.75f );
			particle.life = life;
			particle.maximumLife = life;
			particle.color = bloodColors[index % bloodColors.size()];
			particle.gravity = RandomRange( 470.0f, 720.0f );
			const float dropSize = RandomRange( 2.0f, 5.8f );
			particle.size = { dropSize * RandomRange( 0.8f, 1.25f ), dropSize };
			particle.groundY = feetPosition.y + RandomRange( -4.0f, 7.0f );
			particle.isBlood = true;
			particle.depositsStain = index % 3u != 0u;
			state_.particles.push_back( particle );
		}

		for( uint32_t index = 0; index < 6u; ++index )
		{
			const float angle = RandomRange( 0.0f, 6.2831853f );
			const float radius = RandomRange( 2.0f, 24.0f );
			BloodStain stain;
			stain.position = {
				feetPosition.x + std::cos( angle ) * radius,
				feetPosition.y - 2.0f + std::sin( angle ) * radius * 0.20f,
			};
			stain.size = { RandomRange( 7.0f, 22.0f ), RandomRange( 2.0f, 6.0f ) };
			stain.color = bloodColors[( index * 3u ) % bloodColors.size()];
			stain.color.a = RandomRange( 0.58f, 0.92f );
			stain.rotation = RandomRange( -0.7f, 0.7f );
			state_.bloodStains.push_back( stain );
		}
		constexpr size_t maximumBloodStains = 420u;
		if( state_.bloodStains.size() > maximumBloodStains )
		{
			const size_t excess = state_.bloodStains.size() - maximumBloodStains;
			state_.bloodStains.erase( state_.bloodStains.begin(), state_.bloodStains.begin() + static_cast<std::ptrdiff_t>( excess ) );
		}
	}

	void FrontierPhysics::DamagePlayer( Vec2 impactDirection )
	{
		if( state_.invulnerability > 0.0f || state_.gameOver ) return;
		state_.health -= 25;
		state_.invulnerability = 0.8f;
		SpawnBurst( ActorCenter( state_.playerPosition ), { 1.0f, 0.22f, 0.08f, 1.0f }, 14 );
		if( state_.health > 0 ) return;
		SpawnDeathBlood( state_.playerPosition, impactDirection, state_.movementDirection, {} );
		--state_.lives;
		if( state_.lives <= 0 )
		{
			state_.health = 0;
			state_.gameOver = true;
			return;
		}
		state_.health = 100;
		const float checkpoint = std::floor( state_.playerPosition.x / 1000.0f ) * 1000.0f + 250.0f;
		state_.playerPosition = { std::clamp( checkpoint, 250.0f, kLevelGoalX - 500.0f ), 620.0f };
		state_.playerVelocity = {};
		state_.cameraX = std::clamp( state_.playerPosition.x - 360.0f, 0.0f, kLevelWidth - kLogicalWidth );
		state_.invulnerability = 1.8f;
	}

	void FrontierPhysics::SpawnEnemy( Vec2 position )
	{
		Enemy enemy;
		enemy.position = position;
		enemy.speed = RandomRange( 48.0f, 68.0f );
		enemy.fireCooldown = RandomRange( 1.4f, 2.8f );
		state_.enemies.push_back( enemy );
	}

	void FrontierPhysics::FireBullet( Vec2 position, Vec2 direction, bool fromPlayer )
	{
		direction = Normalize( direction );
		const float speed = fromPlayer ? 660.0f : 270.0f;
		state_.bullets.push_back( { position, { direction.x * speed, direction.y * speed }, fromPlayer, true } );
		events_.bulletsFired.push_back( { position, direction, fromPlayer } );
	}

	void FrontierPhysics::StepEffects( float deltaSeconds )
	{
		for( Particle& particle : state_.particles )
		{
			particle.life -= deltaSeconds;
			particle.position.x += particle.velocity.x * deltaSeconds;
			particle.position.y += particle.velocity.y * deltaSeconds;
			particle.velocity.y += particle.gravity * deltaSeconds;
			particle.velocity.x *= std::max( 0.0f, 1.0f - deltaSeconds * 0.32f );
			if( particle.isBlood && particle.velocity.y > 0.0f && particle.position.y >= particle.groundY )
			{
				particle.position.y = particle.groundY;
				particle.life = 0.0f;
				if( particle.depositsStain )
				{
					BloodStain stain;
					stain.position = particle.position;
					stain.size = {
						particle.size.x * RandomRange( 1.5f, 4.2f ),
						particle.size.y * RandomRange( 0.45f, 1.15f ),
					};
					stain.color = {
						particle.color.r * RandomRange( 0.42f, 0.68f ),
						particle.color.g * 0.35f,
						particle.color.b * 0.35f,
						RandomRange( 0.55f, 0.88f ),
					};
					stain.rotation = RandomRange( -1.25f, 1.25f );
					state_.bloodStains.push_back( stain );
				}
			}
		}
		std::erase_if( state_.particles, []( const Particle& particle ) { return particle.life <= 0.0f; } );
		for( DeadBody& body : state_.deadBodies ) body.life -= deltaSeconds;
		std::erase_if( state_.deadBodies, []( const DeadBody& body ) { return body.life <= 0.0f; } );
		constexpr size_t maximumBloodStains = 420u;
		if( state_.bloodStains.size() > maximumBloodStains )
		{
			const size_t excess = state_.bloodStains.size() - maximumBloodStains;
			state_.bloodStains.erase( state_.bloodStains.begin(), state_.bloodStains.begin() + static_cast<std::ptrdiff_t>( excess ) );
		}
	}

	void FrontierPhysics::StepPlayer( float deltaSeconds, const InputState& input )
	{
		const Vec2 movement = Normalize( input.movement );
		const Vec2 desiredVelocity{ movement.x * 255.0f, movement.y * 255.0f };
		const float response = Length( movement ) > 0.0f ? 1900.0f : 2600.0f;
		state_.playerVelocity.x = MoveTowards( state_.playerVelocity.x, desiredVelocity.x, response * deltaSeconds );
		state_.playerVelocity.y = MoveTowards( state_.playerVelocity.y, desiredVelocity.y, response * deltaSeconds );
		state_.moving = Length( state_.playerVelocity ) > 8.0f;
		if( Length( movement ) > 0.0f ) state_.movementDirection = movement;
		state_.playerPosition.x += state_.playerVelocity.x * deltaSeconds;
		state_.playerPosition.y += state_.playerVelocity.y * deltaSeconds;
		if( state_.moving ) state_.walkCycle += Length( state_.playerVelocity ) * deltaSeconds / 38.0f;
		state_.playerPosition.x = std::clamp( state_.playerPosition.x, 80.0f, kLevelWidth - 80.0f );
		state_.playerPosition.y = std::clamp( state_.playerPosition.y, 535.0f, 665.0f );

		const float playerScreenX = state_.playerPosition.x - state_.cameraX;
		if( playerScreenX > 520.0f ) state_.cameraX = state_.playerPosition.x - 520.0f;
		else if( playerScreenX < 300.0f ) state_.cameraX = state_.playerPosition.x - 300.0f;
		state_.cameraX = std::clamp( state_.cameraX, 0.0f, kLevelWidth - kLogicalWidth );
		if( state_.playerPosition.x >= kLevelGoalX )
		{
			state_.gameOver = true;
			state_.victory = true;
			state_.score += 1000;
			return;
		}

		Vec2 fireDirection = Normalize( input.fireDirection );
		if( Length( fireDirection ) > 0.0f ) state_.aimDirection = fireDirection;
		else if( input.fireUsingLastDirection ) fireDirection = state_.aimDirection;
		state_.playerFireCooldown -= deltaSeconds;
		if( Length( fireDirection ) > 0.0f && state_.playerFireCooldown <= 0.0f )
		{
			const Vec2 playerCenter = ActorCenter( state_.playerPosition );
			FireBullet( { playerCenter.x + state_.aimDirection.x * 42.0f, playerCenter.y + state_.aimDirection.y * 34.0f }, state_.aimDirection, true );
			state_.playerFireCooldown = 0.16f;
		}
	}

	void FrontierPhysics::SpawnEncounters()
	{
		while( state_.nextEnemySpawn < kEnemySpawns.size() &&
			kEnemySpawns[state_.nextEnemySpawn].x <= state_.playerPosition.x + 700.0f )
		{
			SpawnEnemy( kEnemySpawns[state_.nextEnemySpawn] );
			++state_.nextEnemySpawn;
		}
	}

	void FrontierPhysics::StepEnemies( float deltaSeconds )
	{
		for( Enemy& enemy : state_.enemies )
		{
			if( !enemy.alive ) continue;
			enemy.hitFlash = std::max( 0.0f, enemy.hitFlash - deltaSeconds );
			const Vec2 toPlayer{ state_.playerPosition.x - enemy.position.x, state_.playerPosition.y - enemy.position.y };
			const Vec2 direction = Normalize( toPlayer );
			const float distanceToPlayer = Length( toPlayer );
			enemy.facingDirection = direction;
			const float approach = distanceToPlayer > 345.0f ? 1.0f : ( distanceToPlayer < 225.0f ? -0.65f : 0.0f );
			Vec2 enemyMovement{ direction.x * approach, direction.y * approach * 0.55f };
			if( distanceToPlayer < 520.0f )
				enemyMovement.y += std::sin( state_.elapsed * 1.7f + enemy.position.x * 0.011f ) * 0.32f;
			enemyMovement = Normalize( enemyMovement );
			enemy.position.x += enemyMovement.x * enemy.speed * deltaSeconds;
			enemy.position.y += enemyMovement.y * enemy.speed * deltaSeconds;
			enemy.position.y = std::clamp( enemy.position.y, 535.0f, 665.0f );
			if( Length( enemyMovement ) > 0.05f ) enemy.walkCycle += enemy.speed * deltaSeconds / 38.0f;
			enemy.fireCooldown -= deltaSeconds;
			if( enemy.fireCooldown <= 0.0f && distanceToPlayer < 760.0f )
			{
				const Vec2 enemyCenter = ActorCenter( enemy.position );
				FireBullet( { enemyCenter.x + direction.x * 38.0f, enemyCenter.y + direction.y * 30.0f }, direction, false );
				enemy.fireCooldown = RandomRange( 1.2f, 2.5f );
			}
			if( Overlaps( ActorCenter( state_.playerPosition ), { 25.0f, 48.0f }, ActorCenter( enemy.position ), { 27.0f, 48.0f } ) )
			{
				enemy.position.x -= direction.x * 72.0f;
				enemy.position.y = std::clamp( enemy.position.y - direction.y * 38.0f, 535.0f, 665.0f );
				DamagePlayer( direction );
			}
		}
	}

	void FrontierPhysics::StepBullets( float deltaSeconds )
	{
		for( Bullet& bullet : state_.bullets )
		{
			if( !bullet.alive ) continue;
			bullet.position.x += bullet.velocity.x * deltaSeconds;
			bullet.position.y += bullet.velocity.y * deltaSeconds;
			if( bullet.position.x < -40.0f || bullet.position.x > kLevelWidth + 40.0f || bullet.position.y < 95.0f || bullet.position.y > kLogicalHeight + 40.0f )
			{
				bullet.alive = false;
				continue;
			}
			if( bullet.fromPlayer )
			{
				for( Enemy& enemy : state_.enemies )
				{
					if( !enemy.alive || !Overlaps( bullet.position, { 10.0f, 6.0f }, ActorCenter( enemy.position ), { 29.0f, 50.0f } ) ) continue;
					bullet.alive = false;
					enemy.hitFlash = 0.09f;
					--enemy.health;
					SpawnBurst( bullet.position, { 1.0f, 0.72f, 0.18f, 1.0f }, 6 );
					if( enemy.health <= 0 )
					{
						enemy.alive = false;
						state_.score += 100;
						SpawnDeathBlood( enemy.position, Normalize( bullet.velocity ), enemy.facingDirection,
							{ 0.72f, 0.82f, 0.92f, 1.0f } );
					}
					break;
				}
			}
			else if( Overlaps( bullet.position, { 9.0f, 6.0f }, ActorCenter( state_.playerPosition ), { 25.0f, 50.0f } ) )
			{
				bullet.alive = false;
				DamagePlayer( Normalize( bullet.velocity ) );
			}
		}
		std::erase_if( state_.bullets, []( const Bullet& bullet ) { return !bullet.alive; } );
		std::erase_if( state_.enemies, [this]( const Enemy& enemy ) { return !enemy.alive || enemy.position.x < state_.cameraX - 220.0f; } );
	}

	void FrontierPhysics::Step( float deltaSeconds, const InputState& input )
	{
		events_.Clear();
		state_.animationTime += deltaSeconds;
		state_.invulnerability = std::max( 0.0f, state_.invulnerability - deltaSeconds );
		StepEffects( deltaSeconds );
		if( state_.gameOver ) return;

		state_.elapsed += deltaSeconds;
		StepPlayer( deltaSeconds, input );
		if( state_.gameOver ) return;

		SpawnEncounters();
		StepEnemies( deltaSeconds );
		StepBullets( deltaSeconds );
	}
}
