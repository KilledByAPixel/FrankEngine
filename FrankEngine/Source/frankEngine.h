////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Core Functionality
	Copyright 2013 Frank Force - http://www.frankforce.com

	All files in the frank engine framework should include this file.
	This file should be set as the precomiled header.

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#define FRANK_ENGINE
#define frankEngineName			(L"Frank Engine")
#define frankEngineVersion		(L"0.842")

// library includes
#include <string>
#include <list>
#include <set>
#include "../DXUT/core/DXUT.h"
#include "../../Box2D/Box2D/Box2D.h"
#include "core/frankMath.h"
using namespace FrankMath;

// frank engine global objects
extern class GameControlBase*	g_gameControlBase;
extern class Camera*			g_cameraBase;
extern class GuiBase*			g_guiBase;
extern class FrankRender*		g_render;
extern class InputControl*		g_input;
extern class Terrain*			g_terrain;
extern class SoundControl*		g_sound;
extern class Physics*			g_physics;
extern class EditorGui			g_editorGui;
extern class CDXUTTextHelper*	g_textHelper;
#define defaultTerrainFilename	(L"terrain.2dt")

// render setting globals
extern float g_aspectRatio;
extern int g_backBufferWidth;
extern int g_backBufferHeight;
extern Color g_backBufferClearColor;
extern Color g_editorBackBufferClearColor;
extern bool g_usePointFiltering;
extern float g_tileSetUVScale;
extern Color g_gridColor;
extern GameTimer g_lostFocusTimer;
extern float g_interpolatePercent;

// frank engine defines
#define FILENAME_STRING_LENGTH	(64)
#define DEFAULT_STRING_LENGTH	(255)
#define GAME_FPS				(60)
#define GAME_TIME_STEP			(1.0f/GAME_FPS)

// frank engine global functions
void FrankEngineStartup(const WCHAR* title, int maxObjectCount = 2000, size_t maxObjectSize = 32*50);
void FrankEngineInit(int width, int height, GameControlBase* gameControl, GuiBase* gameGui, Camera* camera);
void FrankEngineLoop();
void FrankEngineShutdown();

// frank engine global types
enum GameTeam;
enum PhysicsGroup;
enum GameDamageType;
const GameDamageType GameDamageType_Default = GameDamageType(0);

// all game engine header files are included here for the precompiled header
#include "core/frankProfiler.h"
#include "core/debugMessage.h"
#include "core/inputControl.h"
#include "core/perlinNoise.h"
#include "core/debugConsole.h"
#include "core/pathFindingBase.h"
#include "objects/gameObject.h"
#include "objects/camera.h"
#include "gameControlBase.h"
#include "rendering/frankRender.h"
#include "rendering/frankDebugRender.h"
#include "rendering/frankFont.h"
#include "rendering/deferredRender.h"
#include "rendering/miniMap.h"
#include "gui/guiBase.h"
#include "gui/editorGui.h"
#include "editor/objectEditor.h"
#include "editor/tileEditor.h"
#include "editor/editor.h"
#include "objects/actor.h"
#include "objects/basicObjects.h"
#include "objects/gameObjectBuilder.h"
#include "objects/gameObjectManager.h"
#include "objects/particleSystem.h"
#include "objects/light.h"
#include "objects/projectile.h"
#include "objects/weapon.h"
#include "physics/physics.h"
#include "physics/physicsRender.h"
#include "sound/soundControl.h"
#include "sound/musicControl.h"
#include "terrain/terrain.h"
#include "terrain/terrainRender.h"
#include "terrain/terrainSurface.h"
#include "terrain/terrainTile.h"
#include "core/frankUtil.h"
