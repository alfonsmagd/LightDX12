#pragma once

#include "FrontierWorld.hpp"

#include <vector>

namespace frontier
{
	struct BulletFiredEvent
	{
		Vec2 position;
		Vec2 direction;
		bool fromPlayer = true;
	};

	struct FrameEvents
	{
		std::vector<BulletFiredEvent> bulletsFired;

		void Clear() noexcept
		{
			bulletsFired.clear();
		}
	};
}
