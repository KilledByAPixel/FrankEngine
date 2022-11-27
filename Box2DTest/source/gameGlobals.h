////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Globals
	Copyright 2013 Frank Force - http://www.frankforce.com

	All files in the game framework should include this file.
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

#define gameTitle		(L"Box2D Testbed")
#define gameVersion		(L"2.3.0")
#define gameWebsite		(L"https://box2d.org/")

#include "../../FrankEngine/source/frankEngine.h"

extern class GameControl*	g_gameControl;
extern class GameGui*		g_gameGui;
extern class Player*		g_player;
extern struct FrankFont*	g_gameFont;
extern PhysicsRender		g_debugDraw;

////////////////////////////////////////////////////////////////////////////////////////
// object types

enum GameObjectType
{
	GOT_Invalid = 0,
	GOT_First,
	GOT_Light = GOT_First,
	GOT_ParticleEmitter,
	GOT_TileImageDecal,
	GOT_TextDecal,
	GOT_MusicGameObject,
	GOT_TriggerBox,
	GOT_ShootTrigger,
	GOT_ObjectSpawner,
	GOT_MiniMapGameObject,
	
	// add new game object types here
	// this array is used to save out object stub ids
	// removing object types from this enum will throw eventhing below that off!

	GOT_Count
};

////////////////////////////////////////////////////////////////////////////////////////
// textures

enum TextureID
{
	// Texture_Invalid
	Texture_StartGameTextures_ = Texture_StartGameTextures,
	Texture_Font1,
	
	// tile sheets
	Texture_Tiles0,		// main tile sheet for objects
	Texture_Terrain0,	// terrain tile sheet

	// textures

	Texture_Count
};

////////////////////////////////////////////////////////////////////////////////////////
// sounds

enum SoundID
{
	// Sound_Invalid
	Sound_Test = 1,
	
	// add new sounds here

	Sound_Count
};

////////////////////////////////////////////////////////////////////////////////////////
// game buttons

enum GameButtonIndex
{
	// GB_Invalid
	GB_Reset = GB_StartGameButtons,
	GB_MoveUp,
	GB_MoveDown,
	GB_MoveRight,
	GB_MoveLeft,
	GB_Trigger1,
	GB_Trigger2,
	GB_Pause,
	GB_TestDown,
	GB_TestUp,
	GB_TestCenter,
	GB_TestBomb,
	GB_TestSingleStep,
	GB_TestPause,
	
	// add new game buttons here

	GB_Count,
};

////////////////////////////////////////////////////////////////////////////////////////
// physics groups
// object will not collide with others in the same group
// exept for group 0 which collides with everything

enum PhysicsGroup
{
	PhysicsGroup_None = 0,

	// add new physics groups here

	PhysicsGroup_Count
};

////////////////////////////////////////////////////////////////////////////////////////
// damage types

enum GameDamageType
{
	//GameDamageType_Default
};

////////////////////////////////////////////////////////////////////////////////////////
// render groups

enum RenderGroup
{
	RenderGroup_SuperLow = -10000,
	RenderGroup_BackgroundTerrain = -1000,
	RenderGroup_Default = 0,
	RenderGroup_ForegroundTerrain = 1000,
	RenderGroup_SuperHigh = 10000
};

////////////////////////////////////////////////////////////////////////////////////////

// global game includes
#include "gameControl.h"
#include "gameGui.h"
