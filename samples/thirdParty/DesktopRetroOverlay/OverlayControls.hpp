#pragma once

#include "EffectPresets.hpp"

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <string>

namespace desktop_retro
{
	struct WindowEvents final
	{
		bool quitRequested = false;
		bool captureRecreateRequested = false;
		bool effectHistoryResetRequested = false;
		uint32_t newWidth = 0;
		uint32_t newHeight = 0;
	};

	// Owns all Win32 UI details. The application sees only settings and events.
	class OverlayControls final
	{
	public:
		OverlayControls();
		~OverlayControls();

		OverlayControls( const OverlayControls& ) = delete;
		OverlayControls& operator=( const OverlayControls& ) = delete;

		void Initialize( HINSTANCE instance, HMONITOR monitor, bool headless );
		void PumpMessages();
		WindowEvents ConsumeEvents();

		[[nodiscard]] HWND Window() const noexcept;
		[[nodiscard]] HMONITOR Monitor() const noexcept;
		[[nodiscard]] bool IsVisible() const noexcept;
		[[nodiscard]] const EffectSettings& Settings() const noexcept;

		void SelectPreset( RetroPreset preset );
		void MoveToMonitor( HMONITOR monitor );
		void ConcealUntilFirstFrame();
		void Reveal();
		void SetStatusText( const std::wstring& text );

	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
	};
}
