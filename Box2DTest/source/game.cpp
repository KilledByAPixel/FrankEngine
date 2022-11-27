////////////////////////////////////////////////////////////////////////////////////////
/*
	Startup for Frank Engine
	Copyright 2013 - Frank Force
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"

////////////////////////////////////////////////////////////////////////////////////////
// Main Entry Point
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
	// startup the frank engine
	FrankEngineStartup(gameTitle);
	
	// init frank engine with custom objects
	const int startWidth = 1280;
	const int startHeight = 720;
	g_gameControl = new GameControl();
	g_gameGui = new GameGui();
	Camera* camera = new Camera();
	FrankEngineInit(startWidth, startHeight, g_gameControl, g_gameGui, camera);

	// frank engine main loop
	FrankEngineLoop();

	// shutdown frank engine
	FrankEngineShutdown();

	// exit the program
	return DXUTGetExitCode();
}