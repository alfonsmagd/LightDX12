#pragma once

#include "EffectPresets.hpp"
#include <Windows.h>

namespace desktop_retro
{
	struct AppOptions final
	{
		bool smokeTest = false;
		bool exerciseSequence = false;
		RetroPreset initialPreset = RetroPreset::Ps2Clean;
	};

	AppOptions ParseAppOptions( PWSTR commandLine );
}
