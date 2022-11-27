////////////////////////////////////////////////////////////////////////////////////////
/*
	High Level Game Control
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"
#include "gameControl.h"
#include "Tests/Pinball.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Basic objects from engine
*/
////////////////////////////////////////////////////////////////////////////////////////

GAME_OBJECT_DEFINITION(Light, Texture_Arrow, Color::White(0.5f));
GAME_OBJECT_DEFINITION(ParticleEmitter, Texture_Smoke, Color::White(0.5f));
GAME_OBJECT_DEFINITION(ObjectSpawner, Texture_Arrow, Color::Purple());
GAME_OBJECT_DEFINITION(MusicGameObject, Texture_Arrow, Color::Red(0.5f));
GAME_OBJECT_DEFINITION(TextDecal, Texture_Font1, Color::White());
GAME_OBJECT_DEFINITION(TileImageDecal, Texture_Arrow, Color::Cyan());
GAME_OBJECT_DEFINITION(TriggerBox, Texture_Invalid, Color::Green(0.1f));
GAME_OBJECT_DEFINITION(ShootTrigger, Texture_Arrow, Color::Green());
GAME_OBJECT_DEFINITION(MiniMapGameObject, Texture_Arrow, Color::Magenta());

////////////////////////////////////////////////////////////////////////////////////////
//
//	Player class
//
////////////////////////////////////////////////////////////////////////////////////////

Player::Player(const XForm2& xf) : GameObject(xf) { g_player = this; }
Player::~Player() { g_player = NULL; }

////////////////////////////////////////////////////////////////////////////////////////
//
//	Game Control Member Functions
//
////////////////////////////////////////////////////////////////////////////////////////

GameControl::GameControl()
{
	{
		// terrain settings
		Terrain::patchLayers		= 2;				// how many layers there are
		Terrain::fullSize			= IntVector2(10);	// how many patches per terrain
		Terrain::patchSize			= 16;				// how many tiles per patch
		Terrain::enableStreaming	= false;
		Terrain::gravity = Vector2(0, -10);
	}
	{
		// rendering settings
		DeferredRender::lightEnable				= false;
		DeferredRender::normalMappingEnable		= false;
		g_backBufferClearColor = Color::Grey(1, 0.05f);
	}

	// enable dev mode for demo
	GameControlBase::devMode = true;
	GameControlBase::alwayShowMouseCursor = true;
	Camera::followPlayer = false;
	ObjectEditor::snapToGridSize = 1;
	Editor::gridPatchColor = Color::Red(0.2f);
	Editor::gridColorTile = Color::White(0.1f);
	
	// count test entries
	testCount = 0;
	while (g_testEntries[testCount].createFcn)
		++testCount;
}

GameControl::~GameControl()
{
	SAFE_DELETE(test);
}

void GameControl::SetupInput()
{
	GameControlBase::SetupInput();

	g_input->SetButton(GB_MoveUp,			'W');
	g_input->SetButton(GB_MoveUp,			VK_UP);
	g_input->SetButton(GB_MoveDown,			'S');
	g_input->SetButton(GB_MoveDown,			VK_DOWN);
	g_input->SetButton(GB_MoveRight,		'D');
	g_input->SetButton(GB_MoveRight,		VK_RIGHT);
	g_input->SetButton(GB_MoveLeft,			'A');
	g_input->SetButton(GB_MoveLeft,			VK_LEFT);
	g_input->SetButton(GB_Trigger1,			VK_LBUTTON);
	g_input->SetButton(GB_Trigger2,			VK_RBUTTON);

	g_input->SetButton(GB_Restart,			'R');
	g_input->SetButton(GB_Pause,			VK_ESCAPE);
	g_input->SetButton(GB_TestDown,			'Q');
	g_input->SetButton(GB_TestUp,			'E');
	g_input->SetButton(GB_TestBomb,			VK_SPACE);
	g_input->SetButton(GB_TestBomb,			VK_RBUTTON);
	g_input->SetButton(GB_TestCenter,		'C');
	g_input->SetButton(GB_TestSingleStep,	'O');
	g_input->SetButton(GB_TestPause,		'P');
}

void GameControl::LoadTextures()
{
	// load the basic textures
	g_render->LoadTexture( L"lightMask",		Texture_LightMask);
	g_render->LoadTexture( L"font1",			Texture_Font1);
	
	// load tile sheets
	g_render->LoadTexture( L"smoke",			Texture_Smoke);
	g_render->LoadTexture( L"circle",			Texture_Circle);
	g_render->LoadTexture( L"dot",				Texture_Dot);
}

void GameControl::InitDeviceObjects()
{
	GameControlBase::InitDeviceObjects();
	
	g_gameFont = new FrankFont(Texture_Font1, L"font1.fnt");
}

void GameControl::DestroyDeviceObjects()
{
	GameControlBase::DestroyDeviceObjects();
	
	SAFE_DELETE(g_gameFont);
}

void GameControl::Reset()
{
	GameControlBase::Reset();

	new Player(g_terrain->GetPlayerEditorStartPos());
	
	static bool first = true;
	if (first)
	{
		// setup the game camera
		first = false;
		g_cameraBase->SetMinGameplayZoom(1);
		g_cameraBase->SetMaxGameplayZoom(1000);
		g_cameraBase->SetZoom(150);
		g_cameraBase->SetPosWorld(Vector2(0, 20));
	}
	
	g_terrain->GetLayerRender(0)->SetRenderGroup(RenderGroup_ForegroundTerrain);
	g_terrain->GetLayerRender(1)->SetRenderGroup(RenderGroup_BackgroundTerrain);
	
	SAFE_DELETE(test);
	entry = g_testEntries + testIndex;
	test = entry->createFcn();
}

void GameControl::UpdateFrame(float delta)
{
	if (IsEditMode())
		return;

	if (g_input->WasJustPushed(GB_Pause))
	{
		// handle pausing
		SetPaused(!IsPaused());

		if (IsPaused())
			g_gameGui->Reset();
	}

	if (g_input->WasJustPushed(GB_Restart))
	{
		Reset();
		return;
	}

	// track how many updates there were to do in render pass
	++updatesNeeded;
	
	{
		// route input to to box2d testbed
		static bool keyWasDown[256] = { false };
		for (int i = 0; i < 255; ++i)
		{
			bool isDown = g_input->IsKeyDown(i);
			if (isDown && !keyWasDown[i])
				test->Keyboard(i);
			else if (!isDown && !keyWasDown[i])
				test->KeyboardUp(i);
			keyWasDown[i] = isDown;
		}

		bool shiftIsDown = g_input->IsDown(GB_Shift);
		Vector2 mousePos = g_input->GetMousePosWorldSpace();
		if (g_input->WasJustPushed(GB_MouseLeft) && !shiftIsDown)
			test->MouseDown(mousePos);
		if (g_input->WasJustPushed(GB_MouseLeft) && shiftIsDown)
			test->ShiftMouseDown(mousePos);
		if (g_input->WasJustReleased(GB_MouseLeft))
			test->MouseUp(mousePos);
		test->MouseMove(mousePos);
	
	}
	
	// testbed commands
	if (g_input->IsDown(GB_MouseMiddle))
		g_cameraBase->SetPosWorld( g_cameraBase->GetPosWorld() - g_input->GetMouseDeltaWorldSpace());
	if (g_input->WasJustPushed(GB_TestBomb))
		test->LaunchBomb();
	if (g_input->WasJustPushed(GB_TestCenter))
	{
		g_cameraBase->SetZoom(150);
		g_cameraBase->SetPosWorld(Vector2(0, 20));
	}
	if (g_input->WasJustPushed(GB_TestSingleStep))
	{
		settings.pause = true;
		settings.singleStep = true;
	}
	if (g_input->WasJustPushed(GB_TestPause))
		settings.pause = !settings.pause;

	// cycle test
	if (g_input->WasJustPushed(GB_TestDown))
		testSelection = (testSelection - 1 + testCount) % testCount;
	if (g_input->WasJustPushed(GB_TestUp))
		testSelection = (testSelection + 1 + testCount) % testCount;
	if (testSelection != testIndex)
	{
		testIndex = testSelection;
		SAFE_DELETE(test);
		entry = g_testEntries + testIndex;
		test = entry->createFcn();
	}
}

void GameControl::RenderPost()
{
	if (IsGameplayMode())
	{
		// HACK: do updates and render at the same time to let the box3d tests work without modifications
		updatesNeeded = Cap(updatesNeeded, 1, 20);
		while (updatesNeeded-- > 0)
		{
			if (updatesNeeded == 0)
			{
				// only render anything for final update
				PhysicsRender::alpha = 1;

				g_editor.RenderGrid();
				if (test && entry)
					test->DrawTitle(entry->name);
			}
			else
				PhysicsRender::alpha = 0;

			test->Step(&settings);
		}
	}

	GameControlBase::RenderPost();
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	surface info
//
////////////////////////////////////////////////////////////////////////////////////////

void GameControl::InitSurfaceInfoArray()
{
	// setup all the surface infos for the terrain
	// set defaults
	for(int id = 0; id < 5; ++id)
	{
		ASSERT(id < TERRAIN_UNIQUE_TILE_COUNT);
		GameSurfaceInfo& info = GameSurfaceInfo::Get(id);

		info.name = L"Unnamed Tile Type";
		info.ti = Texture_Invalid;
		info.color = Color::White();
		info.backgroundColor = Color::White();
		info.emissiveColor = Color::White();
		info.flags = GSI_Collision|GSI_Destructible;
		info.materialIndex = GameMaterialIndex(0);
	}

	{
		int id = 0;
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Clear";
			info.color = Color::White();
			info.backgroundColor = Color::Grey(1, 0.5f);
			info.flags = GSI_None;
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.backgroundColor = info.color = Color::White();
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.backgroundColor = info.color = Color::Grey();
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.backgroundColor = info.color = Color::Black();
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.backgroundColor = info.color = Color::Red();
		}
	}

	// add new surface infos here
	// note: order is important in this array!
	// if you do need to rerange, there is a replaceTile console command

	GameControlBase::InitSurfaceInfoArray();
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Render debug text
//
////////////////////////////////////////////////////////////////////////////////////////

void GameControl::RenderDebugText()
{
	GameControlBase::RenderDebugText();

	if (!showDebugInfo)
		return;

	g_textHelper->Begin();
	// add debug text here
	g_textHelper->End();
}