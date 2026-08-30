#pragma once

#include "Ldx12/Ldx12.hpp"

#include <cstdint>

namespace ldx12::utils
{
	using AppLdxMessageHandler = bool ( * )( HWND window, UINT message, WPARAM wParam, LPARAM lParam, void* userData );

	struct AppLdxDesc
	{
		HINSTANCE instance = nullptr;
		int showCommand = SW_SHOWDEFAULT;
		const wchar_t* className = L"Ldx12SampleWindow";
		const wchar_t* title = L"Ldx12 Sample";
		uint32_t width = 1280;
		uint32_t height = 720;
		AppLdxMessageHandler messageHandler = nullptr;
		void* messageUserData = nullptr;
	};

	class AppLdx final
	{
	public:
		explicit AppLdx( const AppLdxDesc& desc );
		~AppLdx();

		AppLdx( const AppLdx& ) = delete;
		AppLdx& operator=( const AppLdx& ) = delete;

		HWND GetWindow() const noexcept;
		bool PumpMessages();
		bool IsWindowMinimized() const noexcept;
		bool IsLeftMouseButtonDown() const noexcept;
		bool WasKeyPressed( uint32_t virtualKey ) const noexcept;
		int32_t GetMouseDeltaX() const noexcept;
		int32_t GetMouseDeltaY() const noexcept;
		uint32_t GetWidth() const noexcept;
		uint32_t GetHeight() const noexcept;
		void SetDeviceManager( DeviceManager& deviceManager ) noexcept;

	private:
		static LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

		HINSTANCE instance_ = nullptr;
		const wchar_t* className_ = nullptr;
		HWND window_ = nullptr;
		DeviceManager* deviceManager_ = nullptr;
		AppLdxMessageHandler messageHandler_ = nullptr;
		void* messageUserData_ = nullptr;
		int32_t mouseX_ = 0;
		int32_t mouseY_ = 0;
		int32_t mouseDeltaX_ = 0;
		int32_t mouseDeltaY_ = 0;
		uint32_t width_ = 0;
		uint32_t height_ = 0;
		bool keyPressed_[ 256 ] = {};
		bool running_ = true;
		bool minimized_ = false;
		bool mousePositionInitialized_ = false;
		bool leftMouseButtonDown_ = false;
	};
}
