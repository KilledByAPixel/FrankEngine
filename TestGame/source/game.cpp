////////////////////////////////////////////////////////////////////////////////////////
/*
	Startup for Frank Engine
	Copyright 2013 - Frank Force
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"

////////////////////////////////////////////////////////////////////////////////////////
// Main Entry Point
#ifdef FRANK_PLATFORM_WEB
int main(int argc, char** argv)
#else
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
#endif
{
	// startup the frank engine
	FrankEngineStartup(gameTitle);

	// init frank engine with custom objects
	const int startWidth = 1280;
	const int startHeight = 720;
	g_gameControl = new GameControl();
	g_gameGui = new GameGui();
	g_camera = new GameCamera();
	FrankEngineInit(startWidth, startHeight, g_gameControl, g_gameGui, g_camera);

	// frank engine main loop
	FrankEngineLoop();

	// shutdown frank engine
	FrankEngineShutdown();

	// exit the program
	#ifdef FRANK_PLATFORM_WEB
	return 0;
	#else
	return DXUTGetExitCode();
	#endif
}