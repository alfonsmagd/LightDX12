#include "FrontierApplication.hpp"

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand )
{
	return frontier::RunApplication( instance, commandLine, showCommand );
}
