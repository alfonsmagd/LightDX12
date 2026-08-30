#include "Ldx12Utils/AppLdx.hpp"

#include <windowsx.h>

#include <stdexcept>

namespace ldx12::utils
{
	AppLdx::AppLdx( const AppLdxDesc& desc )
		: instance_( desc.instance ), className_( desc.className ), messageHandler_( desc.messageHandler ), messageUserData_( desc.messageUserData ),
		  width_( desc.width ), height_( desc.height )
	{
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof( WNDCLASSEXW );
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance_;
		windowClass.lpszClassName = className_;
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		if( RegisterClassExW( &windowClass ) == 0 )
		{
			throw std::runtime_error( "Failed to register the Win32 window class." );
		}

		window_ = CreateWindowExW( 0,
			className_,
			desc.title,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			static_cast<int>( desc.width ),
			static_cast<int>( desc.height ),
			nullptr,
			nullptr,
			instance_,
			this );
		if( window_ == nullptr )
		{
			UnregisterClassW( className_, instance_ );
			throw std::runtime_error( "Failed to create the Win32 window." );
		}

		ShowWindow( window_, desc.showCommand );
		UpdateWindow( window_ );
	}

	AppLdx::~AppLdx()
	{
		deviceManager_ = nullptr;
		if( window_ != nullptr )
		{
			SetWindowLongPtr( window_, GWLP_USERDATA, 0 );
			if( IsWindow( window_ ) != FALSE )
			{
				DestroyWindow( window_ );
			}
		}
		UnregisterClassW( className_, instance_ );
	}

	HWND AppLdx::GetWindow() const noexcept
	{
		return window_;
	}

	bool AppLdx::PumpMessages()
	{
		mouseDeltaX_ = 0;
		mouseDeltaY_ = 0;
		for( bool& keyPressed : keyPressed_ )
		{
			keyPressed = false;
		}

		MSG message{};
		while( PeekMessage( &message, nullptr, 0, 0, PM_REMOVE ) )
		{
			if( message.message == WM_QUIT )
			{
				running_ = false;
				break;
			}
			TranslateMessage( &message );
			DispatchMessage( &message );
		}
		return running_;
	}

	bool AppLdx::IsWindowMinimized() const noexcept
	{
		return minimized_;
	}

	bool AppLdx::IsLeftMouseButtonDown() const noexcept
	{
		return leftMouseButtonDown_;
	}

	bool AppLdx::WasKeyPressed( uint32_t virtualKey ) const noexcept
	{
		return virtualKey < 256 && keyPressed_[ virtualKey ];
	}

	int32_t AppLdx::GetMouseDeltaX() const noexcept
	{
		return mouseDeltaX_;
	}

	int32_t AppLdx::GetMouseDeltaY() const noexcept
	{
		return mouseDeltaY_;
	}

	uint32_t AppLdx::GetWidth() const noexcept
	{
		return width_;
	}

	uint32_t AppLdx::GetHeight() const noexcept
	{
		return height_;
	}

	void AppLdx::SetDeviceManager( DeviceManager& deviceManager ) noexcept
	{
		deviceManager_ = &deviceManager;
	}

	LRESULT CALLBACK AppLdx::WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		AppLdx* app = reinterpret_cast<AppLdx*>( GetWindowLongPtr( window, GWLP_USERDATA ) );
		if( message == WM_NCCREATE )
		{
			const CREATESTRUCTW* createInfo = reinterpret_cast<const CREATESTRUCTW*>( lParam );
			app = static_cast<AppLdx*>( createInfo->lpCreateParams );
			SetWindowLongPtr( window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( app ) );
		}
		if( app != nullptr )
		{
			switch( message )
			{
			case WM_LBUTTONDOWN:
				app->leftMouseButtonDown_ = true;
				app->mouseX_ = GET_X_LPARAM( lParam );
				app->mouseY_ = GET_Y_LPARAM( lParam );
				app->mousePositionInitialized_ = true;
				SetCapture( window );
				break;

			case WM_LBUTTONUP:
				app->leftMouseButtonDown_ = false;
				ReleaseCapture();
				break;

			case WM_MOUSEMOVE:
			{
				const int32_t mouseX = GET_X_LPARAM( lParam );
				const int32_t mouseY = GET_Y_LPARAM( lParam );
				if( app->mousePositionInitialized_ )
				{
					app->mouseDeltaX_ += mouseX - app->mouseX_;
					app->mouseDeltaY_ += mouseY - app->mouseY_;
				}
				app->mouseX_ = mouseX;
				app->mouseY_ = mouseY;
				app->mousePositionInitialized_ = true;
				break;
			}

			case WM_CAPTURECHANGED:
			case WM_KILLFOCUS:
				app->leftMouseButtonDown_ = false;
				break;

			case WM_KEYDOWN:
				if( wParam < 256 && ( lParam & ( 1LL << 30 ) ) == 0 )
				{
					app->keyPressed_[ wParam ] = true;
				}
				break;

			default:
				break;
			}
		}
		if( app != nullptr && app->messageHandler_ != nullptr && app->messageHandler_( window, message, wParam, lParam, app->messageUserData_ ) )
		{
			return 1;
		}

		switch( message )
		{
		case WM_SIZE:
			if( app != nullptr )
			{
				const uint32_t width = LOWORD( lParam );
				const uint32_t height = HIWORD( lParam );
				app->width_ = width;
				app->height_ = height;
				app->minimized_ = width == 0 || height == 0;
				if( !app->minimized_ && app->deviceManager_ != nullptr )
				{
					app->deviceManager_->Resize( width, height );
				}
			}
			return 0;

		case WM_CLOSE:
			if( app != nullptr )
			{
				app->running_ = false;
			}
			return 0;

		case WM_DESTROY:
			if( app != nullptr )
			{
				app->running_ = false;
			}
			PostQuitMessage( 0 );
			return 0;

		default:
			return DefWindowProc( window, message, wParam, lParam );
		}
	}
}
