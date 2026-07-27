////////////////////////////////////////////////////////////////////////////////////////
/*
	Game GUI - web stub
	Copyright 2013 Frank Force - http://www.frankforce.com

	- compiled instead of gameGUI.cpp on the web build (FRANK_PLATFORM_WEB), where the
	  DXUT dialog system does not exist; a real web menu arrives with phase 7
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"

#ifndef FRANK_PLATFORM_WEB
#error gameGUIWeb.cpp is web-build only
#endif

void GameGui::Init() {}
void GameGui::OnResetDevice() {}
HRESULT GameGui::Render(float delta) { return 0; }
void GameGui::Refresh() {}
void GameGui::Reset() {}
void GameGui::RenderFadeOverlay() {}
