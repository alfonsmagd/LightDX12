#include "OverlayControls.hpp"
#include "Win32Support.hpp"
#include <shellapi.h>
#include <algorithm>
#include <string>
namespace desktop_retro
{
	namespace
	{
		constexpr DWORD kWindowDisplayAffinityExcludeFromCapture = 0x00000011;
		constexpr UINT kTrayCallbackMessage = WM_APP + 1;
		constexpr UINT kTrayIconId = 1;

		enum TrayCommandId : UINT
		{
			TrayToggle = 100,
			TrayCrt,
			TrayPs2,
			TrayNewPixie,
			TrayAmber,
			TrayGreen,
			TraySettings,
			TrayQuit,
		};

		enum HotkeyId : int
		{
			HotkeyToggle = 1,
			HotkeyCrt,
			HotkeyPs2,
			HotkeyNewPixie,
			HotkeyAmber,
			HotkeyGreen,
			HotkeyMoreIntensity,
			HotkeyLessIntensity,
			HotkeySettings,
			HotkeyQuit,
		};

		enum SettingsCommandId : UINT
		{
			SettingsIntensityMore = 200,
			SettingsIntensityLess,
			SettingsHidePanel,
		};

		struct OverlayState
		{
			HWND hwnd = nullptr;
			HWND settingsHwnd = nullptr;
			HMONITOR monitor = nullptr;
			bool running = true;
			bool overlayVisible = true;
			bool headless = false;
			bool requestCaptureRecreate = false;
			bool resetHistory = false;
			uint32_t pendingWidth = 0;
			uint32_t pendingHeight = 0;
			EffectSettings settings{};
		};

		void PositionOverlay( OverlayState& );
		void ApplyPreset( OverlayState&, uint32_t );
		void AddTrayIcon( HWND );
		void RemoveTrayIcon( HWND ) noexcept;
		HWND CreateSettingsPanel( HINSTANCE, OverlayState& );
		LRESULT CALLBACK WindowProc( HWND, UINT, WPARAM, LPARAM );
		void RegisterOverlayHotkeys( HWND ) noexcept;
		void UnregisterOverlayHotkeys( HWND ) noexcept;

		void PositionOverlay( OverlayState& app )
		{
			app.monitor = GetPrimaryMonitor();
			const RECT bounds = GetMonitorRect( app.monitor );
			SetWindowPos( app.hwnd,
				HWND_TOPMOST,
				bounds.left,
				bounds.top,
				bounds.right - bounds.left,
				bounds.bottom - bounds.top,
				SWP_NOACTIVATE | ( app.headless ? 0 : SWP_SHOWWINDOW ) );
		}

		void SetOverlayVisible( OverlayState& app, bool visible )
		{
			app.resetHistory = true;
			app.overlayVisible = visible;
			if( visible )
			{
				PositionOverlay( app );
			}
			else
			{
				ShowWindow( app.hwnd, SW_HIDE );
			}
		}

		void ApplyPreset( OverlayState& app, uint32_t mode )
		{
			if( mode >= kPresets.size() )
				return;
			app.settings = GetPreset( static_cast<RetroPreset>( mode ) ).settings;
			app.resetHistory = true;
		}

		void ShowSettingsPanel( OverlayState& app )
		{
			if( app.settingsHwnd == nullptr || !IsWindow( app.settingsHwnd ) )
			{
				return;
			}
			SetWindowPos( app.settingsHwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW );
			SetForegroundWindow( app.settingsHwnd );
		}

		void ExecuteTrayCommand( OverlayState& app, UINT command )
		{
			switch( command )
			{
			case TrayToggle:
				SetOverlayVisible( app, !app.overlayVisible );
				break;
			case TrayCrt:
				ApplyPreset( app, 0 );
				break;
			case TrayPs2:
				ApplyPreset( app, 3 );
				break;
			case TrayNewPixie:
				ApplyPreset( app, 4 );
				break;
			case TrayAmber:
				ApplyPreset( app, 1 );
				break;
			case TrayGreen:
				ApplyPreset( app, 2 );
				break;
			case TraySettings:
				ShowSettingsPanel( app );
				break;
			case TrayQuit:
				app.running = false;
				break;
			}
		}

		void AddTrayIcon( HWND hwnd )
		{
			NOTIFYICONDATAW trayIcon{};
			trayIcon.cbSize = sizeof( trayIcon );
			trayIcon.hWnd = hwnd;
			trayIcon.uID = kTrayIconId;
			trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
			trayIcon.uCallbackMessage = kTrayCallbackMessage;
			trayIcon.hIcon = LoadIconW( nullptr, IDI_APPLICATION );
			wcsncpy_s( trayIcon.szTip, L"Ldx12 Desktop Retro Overlay", _TRUNCATE );
			if( !Shell_NotifyIconW( NIM_ADD, &trayIcon ) )
			{
				throw std::runtime_error( "Failed to create the Desktop Retro Overlay tray icon." );
			}
		}

		void RemoveTrayIcon( HWND hwnd ) noexcept
		{
			NOTIFYICONDATAW trayIcon{};
			trayIcon.cbSize = sizeof( trayIcon );
			trayIcon.hWnd = hwnd;
			trayIcon.uID = kTrayIconId;
			Shell_NotifyIconW( NIM_DELETE, &trayIcon );
		}

		void ShowTrayMenu( OverlayState& app )
		{
			HMENU menu = CreatePopupMenu();
			if( menu == nullptr )
			{
				return;
			}

			AppendMenuW( menu, MF_STRING | ( app.overlayVisible ? MF_CHECKED : MF_UNCHECKED ), TrayToggle, L"Enable retro overlay" );
			AppendMenuW( menu, MF_SEPARATOR, 0, nullptr );
			AppendMenuW( menu, MF_STRING | ( static_cast<uint32_t>( app.settings.preset ) == 0 ? MF_CHECKED : MF_UNCHECKED ), TrayCrt, L"Simple CRTV" );
			AppendMenuW( menu, MF_STRING | ( static_cast<uint32_t>( app.settings.preset ) == 3 ? MF_CHECKED : MF_UNCHECKED ), TrayPs2, L"PS2 Clean (480i)" );
			AppendMenuW( menu,
				MF_STRING | ( static_cast<uint32_t>( app.settings.preset ) == 4 ? MF_CHECKED : MF_UNCHECKED ),
				TrayNewPixie,
				L"PS2 NewPixie CRT" );
			AppendMenuW( menu, MF_STRING | ( static_cast<uint32_t>( app.settings.preset ) == 1 ? MF_CHECKED : MF_UNCHECKED ), TrayAmber, L"Amber terminal" );
			AppendMenuW( menu, MF_STRING | ( static_cast<uint32_t>( app.settings.preset ) == 2 ? MF_CHECKED : MF_UNCHECKED ), TrayGreen, L"Green phosphor" );
			AppendMenuW( menu, MF_SEPARATOR, 0, nullptr );
			AppendMenuW( menu, MF_STRING, TraySettings, L"Open controls..." );
			AppendMenuW( menu, MF_STRING, TrayQuit, L"Quit" );

			POINT cursor{};
			GetCursorPos( &cursor );
			SetForegroundWindow( app.hwnd );
			const UINT command = TrackPopupMenu( menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, cursor.x, cursor.y, 0, app.hwnd, nullptr );
			PostMessageW( app.hwnd, WM_NULL, 0, 0 );
			DestroyMenu( menu );
			ExecuteTrayCommand( app, command );
		}

		LRESULT CALLBACK SettingsWindowProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
		{
			if( message == WM_NCCREATE )
			{
				const auto* create = reinterpret_cast<const CREATESTRUCTW*>( lParam );
				SetWindowLongPtrW( hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( create->lpCreateParams ) );
				return TRUE;
			}

			auto* app = reinterpret_cast<OverlayState*>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );
			switch( message )
			{
			case WM_COMMAND:
				if( app == nullptr )
				{
					return 0;
				}
				switch( LOWORD( wParam ) )
				{
				case TrayToggle:
				case TrayCrt:
				case TrayPs2:
				case TrayNewPixie:
				case TrayAmber:
				case TrayGreen:
				case TrayQuit:
					ExecuteTrayCommand( *app, LOWORD( wParam ) );
					break;
				case SettingsIntensityMore:
					app->settings.intensity = std::min( 1.0f, app->settings.intensity + 0.05f );
					break;
				case SettingsIntensityLess:
					app->settings.intensity = std::max( 0.0f, app->settings.intensity - 0.05f );
					break;
				case SettingsHidePanel:
					ShowWindow( hwnd, SW_HIDE );
					break;
				}
				return 0;

			case WM_CLOSE:
				// Closing this panel keeps the desktop filter running; use "Quit overlay"
				// when the user wants to terminate the process.
				ShowWindow( hwnd, SW_HIDE );
				return 0;

			case WM_DESTROY:
				if( app != nullptr && app->settingsHwnd == hwnd )
				{
					app->settingsHwnd = nullptr;
				}
				return 0;

			default:
				return DefWindowProcW( hwnd, message, wParam, lParam );
			}
		}

		HWND CreateSettingsPanel( HINSTANCE instance, OverlayState& app )
		{
			WNDCLASSEXW windowClass{};
			windowClass.cbSize = sizeof( windowClass );
			windowClass.hInstance = instance;
			windowClass.lpfnWndProc = SettingsWindowProc;
			windowClass.hCursor = LoadCursorW( nullptr, IDC_ARROW );
			windowClass.hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW + 1 );
			windowClass.lpszClassName = L"Ldx12DesktopRetroOverlayControls";
			if( RegisterClassExW( &windowClass ) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS )
			{
				throw std::runtime_error( "Failed to register the retro overlay controls window." );
			}

			const RECT monitorBounds = GetMonitorRect( app.monitor );
			const int width = 340;
			const int height = 310;
			const int left = std::max( monitorBounds.left, monitorBounds.right - width - 36 );
			const int top = monitorBounds.top + 36;
			const DWORD style = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
			HWND settings = CreateWindowExW( WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
				windowClass.lpszClassName,
				L"Retro overlay controls",
				style | WS_VISIBLE,
				left,
				top,
				width,
				height,
				nullptr,
				nullptr,
				instance,
				&app );
			if( settings == nullptr )
			{
				throw std::runtime_error( "Failed to create the retro overlay controls window." );
			}

			CreateWindowW( L"STATIC", L"Retro post-processing", WS_CHILD | WS_VISIBLE, 16, 14, 260, 24, settings, nullptr, instance, nullptr );
			CreateWindowW( L"STATIC",
				L"The filter remains click-through; use these controls at runtime.",
				WS_CHILD | WS_VISIBLE,
				16,
				40,
				305,
				24,
				settings,
				nullptr,
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"Simple CRTV",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				16,
				74,
				96,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( TrayCrt ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"PS2 Clean",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				122,
				74,
				96,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( TrayPs2 ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"NewPixie",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				228,
				74,
				96,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( TrayNewPixie ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"Amber",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				16,
				116,
				150,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( TrayAmber ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"Green",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				174,
				116,
				150,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( TrayGreen ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"More intensity",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				16,
				158,
				150,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( SettingsIntensityMore ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"Less intensity",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				174,
				158,
				150,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( SettingsIntensityLess ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"Toggle overlay",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				16,
				200,
				150,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( TrayToggle ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"Hide controls",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				174,
				200,
				150,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( SettingsHidePanel ) ),
				instance,
				nullptr );
			CreateWindowW( L"BUTTON",
				L"Quit overlay",
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				16,
				242,
				308,
				30,
				settings,
				reinterpret_cast<HMENU>( static_cast<INT_PTR>( TrayQuit ) ),
				instance,
				nullptr );
			return settings;
		}

		LRESULT CALLBACK WindowProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
		{
			auto* app = reinterpret_cast<OverlayState*>( GetWindowLongPtrW( hwnd, GWLP_USERDATA ) );

			switch( message )
			{
			case WM_NCHITTEST:
				return HTTRANSPARENT;

			case WM_ERASEBKGND:
				return 1;

			case WM_HOTKEY:
				if( app == nullptr )
				{
					return 0;
				}
				switch( static_cast<int>( wParam ) )
				{
				case HotkeyToggle:
					SetOverlayVisible( *app, !app->overlayVisible );
					break;
				case HotkeyCrt:
					ApplyPreset( *app, 0 );
					break;
				case HotkeyPs2:
					ApplyPreset( *app, 3 );
					break;
				case HotkeyNewPixie:
					ApplyPreset( *app, 4 );
					break;
				case HotkeyAmber:
					ApplyPreset( *app, 1 );
					break;
				case HotkeyGreen:
					ApplyPreset( *app, 2 );
					break;
				case HotkeyMoreIntensity:
					app->settings.intensity = std::min( 1.0f, app->settings.intensity + 0.05f );
					break;
				case HotkeyLessIntensity:
					app->settings.intensity = std::max( 0.0f, app->settings.intensity - 0.05f );
					break;
				case HotkeySettings:
					ShowSettingsPanel( *app );
					break;
				case HotkeyQuit:
					app->running = false;
					break;
				}
				return 0;

			case kTrayCallbackMessage:
				if( app != nullptr )
				{
					const UINT trayEvent = static_cast<UINT>( lParam );
					if( trayEvent == WM_LBUTTONUP )
					{
						SetOverlayVisible( *app, !app->overlayVisible );
					}
					else if( trayEvent == WM_RBUTTONUP || trayEvent == WM_CONTEXTMENU )
					{
						ShowTrayMenu( *app );
					}
				}
				return 0;

			case WM_DISPLAYCHANGE:
				if( app != nullptr )
				{
					app->requestCaptureRecreate = true;
					if( app->overlayVisible )
					{
						PositionOverlay( *app );
					}
				}
				return 0;

			case WM_SIZE:
				if( app != nullptr && !app->headless )
				{
					const uint32_t width = LOWORD( lParam );
					const uint32_t height = HIWORD( lParam );
					if( width > 0 && height > 0 )
					{
						app->pendingWidth = width;
						app->pendingHeight = height;
					}
				}
				return 0;

			case WM_CLOSE:
				if( app != nullptr )
				{
					app->running = false;
				}
				return 0;

			case WM_DESTROY:
				PostQuitMessage( 0 );
				return 0;

			default:
				return DefWindowProcW( hwnd, message, wParam, lParam );
			}
		}

		void RegisterOverlayHotkeys( HWND hwnd ) noexcept
		{
			constexpr UINT modifiers = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
			RegisterHotKey( hwnd, HotkeyToggle, modifiers, 'R' );
			RegisterHotKey( hwnd, HotkeyCrt, modifiers, '1' );
			RegisterHotKey( hwnd, HotkeyPs2, modifiers, '4' );
			RegisterHotKey( hwnd, HotkeyNewPixie, modifiers, '5' );
			RegisterHotKey( hwnd, HotkeyAmber, modifiers, '2' );
			RegisterHotKey( hwnd, HotkeyGreen, modifiers, '3' );
			RegisterHotKey( hwnd, HotkeyMoreIntensity, modifiers, VK_UP );
			RegisterHotKey( hwnd, HotkeyLessIntensity, modifiers, VK_DOWN );
			RegisterHotKey( hwnd, HotkeySettings, modifiers, 'P' );
			RegisterHotKey( hwnd, HotkeyQuit, modifiers, 'Q' );
		}

		void UnregisterOverlayHotkeys( HWND hwnd ) noexcept
		{
			for( int hotkey = HotkeyToggle; hotkey <= HotkeyQuit; ++hotkey )
			{
				UnregisterHotKey( hwnd, hotkey );
			}
		}
	}

	struct OverlayControls::Impl final
	{
		OverlayState state;
	};

	OverlayControls::OverlayControls() : impl_( std::make_unique<Impl>() )
	{
	}

	OverlayControls::~OverlayControls()
	{
		OverlayState& state = impl_->state;
		if( state.settingsHwnd && IsWindow( state.settingsHwnd ) )
			DestroyWindow( state.settingsHwnd );
		if( state.hwnd && IsWindow( state.hwnd ) )
		{
			SetWindowLongPtrW( state.hwnd, GWLP_USERDATA, 0 );
			if( !state.headless )
			{
				UnregisterOverlayHotkeys( state.hwnd );
				RemoveTrayIcon( state.hwnd );
			}
			DestroyWindow( state.hwnd );
		}
	}

	void OverlayControls::Initialize( HINSTANCE instance, HMONITOR monitor, bool headless )
	{
		OverlayState& state = impl_->state;
		state.monitor = monitor;
		state.headless = headless;
		const RECT bounds = GetMonitorRect( monitor );

		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof( windowClass );
		windowClass.hInstance = instance;
		windowClass.lpfnWndProc = WindowProc;
		windowClass.lpszClassName = L"Ldx12RetroDesktopExample";
		if( !RegisterClassExW( &windowClass ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS )
			throw std::runtime_error( "Could not register the overlay window." );

		state.hwnd = CreateWindowExW( WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED,
			windowClass.lpszClassName,
			L"Desktop Retro Overlay - thirdParty example",
			WS_POPUP,
			bounds.left,
			bounds.top,
			bounds.right - bounds.left,
			bounds.bottom - bounds.top,
			nullptr,
			nullptr,
			instance,
			nullptr );
		if( !state.hwnd )
			throw std::runtime_error( "Could not create the overlay window." );

		SetWindowLongPtrW( state.hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &state ) );
		if( !SetLayeredWindowAttributes( state.hwnd, 0, 0, LWA_ALPHA ) || !SetWindowDisplayAffinity( state.hwnd, kWindowDisplayAffinityExcludeFromCapture ) )
			throw std::runtime_error( "Windows could not exclude the overlay from capture." );

		if( !headless )
		{
			state.settingsHwnd = CreateSettingsPanel( instance, state );
			AddTrayIcon( state.hwnd );
			RegisterOverlayHotkeys( state.hwnd );
			PositionOverlay( state );
		}
	}

	void OverlayControls::PumpMessages()
	{
		MSG message{};
		while( PeekMessageW( &message, nullptr, 0, 0, PM_REMOVE ) )
		{
			if( message.message == WM_QUIT )
			{
				impl_->state.running = false;
				break;
			}
			TranslateMessage( &message );
			DispatchMessageW( &message );
		}
	}

	WindowEvents OverlayControls::ConsumeEvents()
	{
		OverlayState& state = impl_->state;
		WindowEvents events{ !state.running, state.requestCaptureRecreate, state.resetHistory, state.pendingWidth, state.pendingHeight };
		state.requestCaptureRecreate = false;
		state.resetHistory = false;
		state.pendingWidth = 0;
		state.pendingHeight = 0;
		return events;
	}

	HWND OverlayControls::Window() const noexcept
	{
		return impl_->state.hwnd;
	}
	HMONITOR OverlayControls::Monitor() const noexcept
	{
		return impl_->state.monitor;
	}
	bool OverlayControls::IsVisible() const noexcept
	{
		return impl_->state.overlayVisible;
	}
	const EffectSettings& OverlayControls::Settings() const noexcept
	{
		return impl_->state.settings;
	}

	void OverlayControls::SelectPreset( RetroPreset preset )
	{
		ApplyPreset( impl_->state, static_cast<uint32_t>( preset ) );
	}

	void OverlayControls::MoveToMonitor( HMONITOR monitor )
	{
		impl_->state.monitor = monitor;
		PositionOverlay( impl_->state );
	}

	void OverlayControls::ConcealUntilFirstFrame()
	{
		SetLayeredWindowAttributes( impl_->state.hwnd, 0, 0, LWA_ALPHA );
	}

	void OverlayControls::Reveal()
	{
		SetLayeredWindowAttributes( impl_->state.hwnd, 0, 255, LWA_ALPHA );
	}
	void OverlayControls::SetStatusText( const std::wstring& text )
	{
		SetWindowTextW( impl_->state.hwnd, text.c_str() );
	}
}
