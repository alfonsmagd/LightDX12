#pragma once

#include "AppOptions.hpp"
#include <Windows.h>
#include <memory>

namespace desktop_retro
{
	class OverlayApplication final
	{
	public:
		OverlayApplication( HINSTANCE instance, AppOptions options );
		~OverlayApplication();

		OverlayApplication( const OverlayApplication& ) = delete;
		OverlayApplication& operator=( const OverlayApplication& ) = delete;

		void Run();

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
