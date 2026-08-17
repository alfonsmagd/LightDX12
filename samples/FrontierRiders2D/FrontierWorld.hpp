#pragma once

#include <cstdint>
#include <vector>

namespace frontier
{
	inline constexpr float kLogicalWidth = 1280.0f;
	inline constexpr float kLogicalHeight = 720.0f;
	inline constexpr float kLevelWidth = 5200.0f;
	inline constexpr float kLevelGoalX = 4950.0f;

	struct Vec2
	{
		float x = 0.0f;
		float y = 0.0f;
	};

	struct Color
	{
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;
	};

	struct InputState
	{
		Vec2 movement;
		Vec2 fireDirection;
		bool fireUsingLastDirection = false;
	};

	struct Bullet
	{
		Vec2 position;
		Vec2 velocity;
		bool fromPlayer = true;
		bool alive = true;
	};

	struct Enemy
	{
		Vec2 position;
		Vec2 facingDirection{ -1.0f, 0.0f };
		float speed = 70.0f;
		float fireCooldown = 1.0f;
		float hitFlash = 0.0f;
		float walkCycle = 0.0f;
		int health = 4;
		bool alive = true;
	};

	struct Particle
	{
		Vec2 position;
		Vec2 velocity;
		float life = 0.0f;
		float maximumLife = 0.0f;
		Color color;
		float gravity = 150.0f;
		Vec2 size{ 6.0f, 6.0f };
		float groundY = 0.0f;
		bool isBlood = false;
		bool depositsStain = false;
	};

	struct BloodStain
	{
		Vec2 position;
		Vec2 size;
		Color color;
		float rotation = 0.0f;
	};

	struct DeadBody
	{
		Vec2 position;
		Vec2 facingDirection;
		Color tint;
		float finalRotation = 0.0f;
		float life = 0.0f;
		float maximumLife = 0.0f;
	};

	struct GameState
	{
		Vec2 playerPosition{ 250.0f, 620.0f };
		Vec2 playerVelocity;
		Vec2 aimDirection{ 1.0f, 0.0f };
		Vec2 movementDirection{ 1.0f, 0.0f };
		std::vector<Bullet> bullets;
		std::vector<Enemy> enemies;
		std::vector<Particle> particles;
		std::vector<BloodStain> bloodStains;
		std::vector<DeadBody> deadBodies;
		float elapsed = 0.0f;
		float animationTime = 0.0f;
		float walkCycle = 0.0f;
		float cameraX = 0.0f;
		float playerFireCooldown = 0.0f;
		float invulnerability = 0.0f;
		int health = 100;
		int lives = 3;
		int score = 0;
		uint32_t nextEnemySpawn = 0;
		bool moving = false;
		bool gameOver = false;
		bool victory = false;
	};
}
