#include "OverlayApp.hpp"

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR commandLine, int )
{
	return desktop_retro::RunOverlay( instance, commandLine );
}
