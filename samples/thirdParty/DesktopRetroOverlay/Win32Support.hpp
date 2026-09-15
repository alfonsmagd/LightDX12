#pragma once
#include <Windows.h>
#include <cstdio>
#include <stdexcept>
namespace desktop_retro
{
	inline void ThrowIfFailed( HRESULT result, const char* message )
	{
		if( FAILED( result ) )
		{
			char diagnostic[ 256 ]{};
			sprintf_s( diagnostic, "%s (HRESULT 0x%08X)", message, static_cast<unsigned int>( result ) );
			throw std::runtime_error( diagnostic );
		}
	}

	inline RECT GetMonitorRect( HMONITOR monitor )
	{
		MONITORINFO info{};
		info.cbSize = sizeof( info );
		if( !GetMonitorInfoW( monitor, &info ) )
		{
			throw std::runtime_error( "Failed to get the primary monitor bounds." );
		}

		return info.rcMonitor;
	}

	inline HMONITOR GetPrimaryMonitor()
	{
		const HMONITOR monitor = MonitorFromPoint( POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY );
		if( monitor == nullptr )
		{
			throw std::runtime_error( "Failed to locate the primary monitor." );
		}

		return monitor;
	}

}
