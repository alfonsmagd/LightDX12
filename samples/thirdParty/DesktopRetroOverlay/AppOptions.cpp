#include "AppOptions.hpp"
#include <shellapi.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace desktop_retro
{
	AppOptions ParseAppOptions( PWSTR commandLine )
	{
		AppOptions options;
		const std::wstring fullCommandLine = std::wstring( L"DesktopRetroOverlay " ) + ( commandLine ? commandLine : L"" );
		int argumentCount = 0;
		wchar_t** arguments = CommandLineToArgvW( fullCommandLine.c_str(), &argumentCount );
		if( !arguments )
			throw std::runtime_error( "Could not parse the command line." );

		const auto releaseArguments = []( wchar_t** value ) { LocalFree( value ); };
		const std::unique_ptr<wchar_t*, decltype( releaseArguments )> ownedArguments( arguments, releaseArguments );
		for( int index = 1; index < argumentCount; ++index )
		{
			const std::wstring argument( arguments[ index ] );
			if( argument == L"--smoke-test" )
				options.smokeTest = true;
			else if( argument == L"--smoke-sequence" )
			{
				options.smokeTest = true;
				options.exerciseSequence = true;
			}
			else if( argument.size() == 10 && argument.starts_with( L"--preset=" ) && argument[ 9 ] >= L'0' && argument[ 9 ] <= L'4' )
				options.initialPreset = static_cast<RetroPreset>( argument[ 9 ] - L'0' );
			else
				throw std::invalid_argument( "Use --preset=0..4, --smoke-test or --smoke-sequence." );
		}
		if( options.exerciseSequence )
			options.initialPreset = RetroPreset::Crt;
		return options;
	}
}
