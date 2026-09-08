#include "OverlayApp.hpp"
#include "AppOptions.hpp"
#include "Diagnostics.hpp"
#include "OverlayApplication.hpp"
#include <winrt/Windows.Foundation.h>
#include <fstream>

namespace desktop_retro
{
	namespace
	{
		struct WinRtApartment final
		{
			WinRtApartment()
			{
				winrt::init_apartment( winrt::apartment_type::multi_threaded );
			}
			~WinRtApartment()
			{
				winrt::uninit_apartment();
			}
		};
	}

	int RunOverlay( HINSTANCE instance, PWSTR commandLine )
	{
		bool silentErrors = commandLine && wcsstr( commandLine, L"--smoke-" );
		try
		{
			const AppOptions options = ParseAppOptions( commandLine );
			silentErrors = options.smokeTest;
			if( options.smokeTest )
			{
				std::ofstream( "DesktopRetroOverlay-error.txt", std::ios::trunc );
				std::ofstream( "DesktopRetroOverlay-status.txt", std::ios::trunc );
			}
			WinRtApartment apartment;
			OverlayApplication application( instance, options );
			application.Run();
			return EXIT_SUCCESS;
		}
		catch( const winrt::hresult_error& error )
		{
			return ReportFatalError( winrt::to_string( error.message() ), silentErrors );
		}
		catch( const std::exception& error )
		{
			return ReportFatalError( error.what(), silentErrors );
		}
	}
}
