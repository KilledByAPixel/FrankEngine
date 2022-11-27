////////////////////////////////////////////////////////////////////////////////////////
/*
	High Level Game Control
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"
#include "gameControl.h"
#include "gameObjects.h"

////////////////////////////////////////////////////////////////////////////////////////
//
//	Game Control Member Functions
//
////////////////////////////////////////////////////////////////////////////////////////

GameControl::GameControl()
{
	// this example supports both tile sheet and non tile sheet rendering
	static const bool exampleUseTileSheets = false;
	if (exampleUseTileSheets)
	{
		// terrain tile set
		Terrain::SetTileSetInfo(0, L"", Texture_Terrain0);
		Terrain::SetTileSetInfo(1, L"", Texture_Tiles0);
		g_usePointFiltering = true;
		g_tileSetUVScale = 0.99f;	// squeeze tiles a tiny bit to prevent cracks
	}
	{
		// physics settings
		Physics::defaultFriction			= 0.3f;				// how much friction an object has
		Physics::defaultRestitution			= 0.3f;				// how bouncy objects are
		Physics::defaultDensity				= 1.0f;				// how heavy objects are
		Physics::defaultLinearDamping		= 0.2f;				// how quickly objects come to rest
		Physics::defaultAngularDamping		= 0.1f;				// how quickly objects stop rotating
		Physics::worldSize					= Vector2(5000);	// maximum extents of physics world
	}
	{
		// terrain settings
		Terrain::patchLayers		= 2;				// how many layers there are
		Terrain::fullSize			= IntVector2(32);	// how many patches per terrain
		Terrain::patchSize			= 16;				// how many tiles per patch
		Terrain::windowSize			= 1;				// size of the terrain stream window
		Terrain::renderWindowSize	= 1;				// size of the terrain render window
		Terrain::gravity			= Vector2(0,-10);	// acceleartion due to gravity
	}
	{
		// rendering settings
		DeferredRender::lightEnable				= true;
		DeferredRender::normalMappingEnable		= true;
		DeferredRender::emissiveBackgroundColor	= Color::Grey(1, 0.2f);
		DeferredRender::ambientLightColor		= Color::Grey(1, 0.1f);
		g_backBufferClearColor					= Color(0.3f, 0.5f, 1.0f);
	}
	{
		// particle system settings
		ParticleEmitter::defaultRenderGroup = RenderGroup_Effect;
		ParticleEmitter::defaultAdditiveRenderGroup = RenderGroup_AdditiveEffect;
	}

	// enable dev mode for demo
	GameControlBase::devMode = true;

	// allow colors to work on terrain (may cause glitching)
	TerrainRender::enableDiffuseLighting = true;
}

void GameControl::SetupInput()
{
	GameControlBase::SetupInput();

	g_input->SetButton(GB_MoveUp,			'W');
	g_input->SetButton(GB_MoveUp,			VK_UP);
	g_input->SetButton(GB_MoveUp,			GAMEPAD_XBOX_DPAD_UP);
	g_input->SetButton(GB_MoveDown,			'S');
	g_input->SetButton(GB_MoveDown,			VK_DOWN);
	g_input->SetButton(GB_MoveDown,			GAMEPAD_XBOX_DPAD_DOWN);
	g_input->SetButton(GB_MoveRight,		'D');
	g_input->SetButton(GB_MoveRight,		VK_RIGHT);
	g_input->SetButton(GB_MoveRight,		GAMEPAD_XBOX_DPAD_RIGHT);
	g_input->SetButton(GB_MoveLeft,			'A');
	g_input->SetButton(GB_MoveLeft,			VK_LEFT);
	g_input->SetButton(GB_MoveLeft,			GAMEPAD_XBOX_DPAD_LEFT);

	g_input->SetButton(GB_Trigger1,			VK_LBUTTON);
	g_input->SetButton(GB_Trigger2,			VK_RBUTTON);
	g_input->SetButton(GB_Trigger1,			GAMEPAD_BUTTON_RIGHT_TRIGGER);
	g_input->SetButton(GB_Trigger2,			GAMEPAD_BUTTON_LEFT_TRIGGER);

	g_input->SetButton(GB_TimeScale,		GAMEPAD_BUTTON_0);
	g_input->SetButton(GB_TimeScale,		VK_SPACE);
	g_input->SetButton(GB_Restart,			'R');
	g_input->SetButton(GB_Pause,			GAMEPAD_XBOX_START);
	g_input->SetButton(GB_Pause,			VK_ESCAPE);
	g_input->SetButton(GB_Test,				'T');
}

void GameControl::LoadSounds()
{
	g_sound->LoadSound(L"sound_test",			Sound_Test);
}

void GameControl::LoadTextures()
{
	// load the basic textures
	g_render->LoadTexture( L"lightMask",		Texture_LightMask);
	g_render->LoadTexture( L"font1",			Texture_Font1);
	g_render->LoadTexture(L"tiles0",			Texture_Tiles0);
	g_render->LoadTexture(L"terrain0",			Texture_Terrain0);
	
	// load tile sheets
	g_render->LoadTexture( L"smoke",			Texture_Smoke);
	g_render->LoadTexture( L"circle",			Texture_Circle);
	g_render->LoadTexture( L"dot",				Texture_Dot);
	g_render->LoadTexture( L"lightGel0",		Texture_LightGel0);

	// load tiles
	g_render->SetTextureTile( L"crate",			Texture_Tiles0,	Texture_Crate,		ByteVector2(4,0),	ByteVector2(8));
	g_render->SetTextureTile( L"arrow",			Texture_Tiles0,	Texture_Arrow,		ByteVector2(3,0),	ByteVector2(8));

	if (Terrain::tileSetCount == 0)
	{
		// load terrain textures
		g_render->LoadTexture( L"tile_test1",		Texture_tile_test1);
		g_render->LoadTexture( L"tile_test2",		Texture_tile_test2);
		g_render->LoadTexture( L"tile_test3",		Texture_tile_test3);
	}
}

void GameControl::InitDeviceObjects()
{
	GameControlBase::InitDeviceObjects();
	
	if (g_cameraBase)
	{
		// lock camera to anything between 16/9 and 16/10 aspect ratio
		float aspect = Cap(g_aspectRatio, 16/10.0f, 16/9.0f);
		g_cameraBase->SetLockedAspectRatio(aspect);
	}

	g_gameFont = new FrankFont(Texture_Font1, L"font1.fnt");
}

void GameControl::DestroyDeviceObjects()
{
	GameControlBase::DestroyDeviceObjects();
	
	SAFE_DELETE(g_gameFont);
}

void GameControl::UpdateFrame(float delta)
{
	if (g_input->WasJustPushed(GB_Pause))
	{
		// handle pausing
		SetPaused(!IsPaused());

		if (IsPaused())
			g_gameGui->Reset();
	}

	if (g_input->IsDown(GB_Test) && !IsEditMode())
	{
		// throw down a random light when the test button is pressed
		//Light* light = new Light(g_input->GetMousePosWorldSpace(), NULL, RAND_BETWEEN(9, 15), Color::RandBetweenComponents(Color::Grey(1, 0.3f), Color::Grey(1, 0.8f)), true);
		//light->SetFadeTimer(20.0f);
		//light->SetHaloRadius(0.8f);
		//light->SetOverbrightRadius(0.8f);
		//light->SetRenderGroup(RenderGroup_high);
	}

	{
		// allow player to slow down time
		const float changeRate = 0.02f;
		if (g_input->IsDown(GB_TimeScale))
			timeScale -= changeRate;
		else
			timeScale += changeRate;
		if (g_gameControlBase->IsEditMode())
			timeScale = 1;
		timeScale = Cap(timeScale, 0.25f, 1.0f);
	}

	if (!IsEditMode() && g_input->WasJustPushed(GB_Restart))
	{
		Reset();
		return;
	}

	if (g_player->GetDeadTime() > 3.0f)
	{
		// automatically reset after player dies
		Reset();
		return;
	}
}

void GameControl::Reset()
{
	const bool wasEditMode = WasEditMode();

	// call the base level reset function
	// this clears all objects set to destroy on reset (basically everything except the camera and terrain)
	// it will also automatically save/load the terrain
	GameControlBase::Reset();

	new Player(g_terrain->GetPlayerEditorStartPos());
	
	if (!g_gameControl->IsEditMode() && !wasEditMode)
	{
		// setup the game camera
		g_cameraBase->SetMinGameplayZoom(7);
		g_cameraBase->SetMaxGameplayZoom(14);
		g_cameraBase->SetZoom(14);
	}
	g_camera->Restart();
	
	g_terrain->GetLayerRender(0)->SetRenderGroup(RenderGroup_ForegroundTerrain);
	g_terrain->GetLayerRender(1)->SetRenderGroup(RenderGroup_BackgroundTerrain);
}

void GameControl::RenderInterpolatedObjects()
{
	ConsoleCommandSimple(bool, skyEnable, true);
	if (skyEnable && !IsEditMode())
	{
		// render a simple time of day system by modifying background directional light settings
		ConsoleCommandSimple(float, skyOrbitSpeed, 0.1f);
		const Vector2 direction = Vector2::BuildFromAngle(-1.5f + GetResetTime()*skyOrbitSpeed);
		const float timeOfDay = Percent(direction.y, -1.0f, 1.0f);
	
		// set background light direction
		const Vector2 lightDirectionSun = 1.0f*direction;
		const Vector2 lightDirectionMoon = -1.0f*direction;
		const Vector2 lightDirection = PercentLerp(timeOfDay, 0.3f, 0.7f, lightDirectionSun, lightDirectionMoon);
		DeferredRender::directionalLightDirection = lightDirection;
	
		// set bakground light color
		const float sunBackgroundColor = 1;
		const float moonBackgroundColor = 0.5f;
		const float lightBackgroundColor = Lerp(timeOfDay, sunBackgroundColor, moonBackgroundColor);
		DeferredRender::directionalLightColor = Color::Grey(1, lightBackgroundColor);

		// set background buffer clear color
		const Color sunClearColor(0.3f, 0.5f, 1.0f, 1.0f);
		const Color moonClearColor(0.03f, 0.0f, 0.2f, 1.0f);
		const Color lightClearColor = Lerp(timeOfDay, sunClearColor, moonClearColor);
		g_backBufferClearColor = lightClearColor;

		// pass some emissive through the background
		const Color sunEmissiveColor = Color::Grey(1, 0.15f);
		const Color moonEmissiveColor = Color::Grey(1, 0.05f);
		DeferredRender::emissiveBackgroundColor = Lerp(timeOfDay, sunEmissiveColor, moonEmissiveColor);

		// draw sun and moon
		DeferredRender::EmissiveRenderBlock emissiveRenderBlock;
		DeferredRender::AdditiveRenderBlock additiveRenderBlock;
		const float radius = 9;
		const float horizonOffset = 2;
		const Vector2 centerPos = g_cameraBase->GetXFInterpolated().position - Vector2(0, horizonOffset);
		{
			// sun
			const Vector2 offset = -radius*direction;
			float alpha = Percent(timeOfDay, 0.8f, 0.0f);
			g_render->RenderQuad(centerPos + offset, Vector2(12), Color::White(0.2f*alpha), Texture_Dot);
			g_render->RenderQuad(centerPos + offset, Vector2(1.7f), Color::White(0.5f*alpha), Texture_Dot);
		}
		{
			// moon
			const Vector2 offset = radius*direction;
			float alpha = Percent(timeOfDay, 0.3f, 1.0f);
			g_render->RenderQuad(centerPos + offset, Vector2(5), Color::White(0.2f*alpha), Texture_Dot);
			g_render->RenderQuad(centerPos + offset, Vector2(1), Color::White(0.2f*alpha), Texture_Circle);
		}

		/*{
			// draw static background image behind everything but in front of the sun/moon
			// this is where you would implement paralx backgrounds
			DeferredRender::EmissiveRenderBlock emissiveRenderBlock;
			Vector2 position = g_player->GetXFInterpolated().position;
			Vector2 size(130);
			g_render->RenderQuad(position, size, Color::White(), Texture_Crate );
		}*/
	}

	GameControlBase::RenderInterpolatedObjects();
}

void GameControl::RenderPost()
{
	if (!usingGamepad && IsGameplayMode() && !IsPaused() && !GameControlBase::alwayShowMouseCursor)
	{
		// special display for the mouse cursor
		ConsoleCommandSimple(float, mouseAlpha,		0.5f);
		ConsoleCommandSimple(float, mouseSize,		0.1f);
		ConsoleCommandSimple(float, mouseLinesSize,	20);
		
		const XForm2 xf = g_input->GetMousePosWorldSpace();
		g_render->RenderQuad(xf, Vector2(mouseSize*3), Color(1,1,1,mouseAlpha), Texture_Circle);
		g_render->RenderQuad(xf, Vector2(mouseSize*1.5f), Color(1,1,1,mouseAlpha), Texture_Circle);

		if (mouseLinesSize > 0)
		{
			const float scale = g_cameraBase->GetZoomInterpolated() / 100;
			const Vector2 p0t = xf.position;
			const Vector2 p1t = xf.TransformCoord(scale*Vector2::YAxis() * mouseLinesSize);
			const Vector2 p2t = xf.TransformCoord(-scale*Vector2::YAxis() * mouseLinesSize);
			const Vector2 p3t = xf.TransformCoord(scale*Vector2::XAxis() * mouseLinesSize);
			const Vector2 p4t = xf.TransformCoord(-scale*Vector2::XAxis() * mouseLinesSize);

			g_render->CapLineVerts(p1t);
			g_render->AddPointToLineVerts(p1t, Color(1,1,1,0));
			g_render->AddPointToLineVerts(p0t, Color(1,1,1,mouseAlpha));
			g_render->AddPointToLineVerts(p2t, Color(1,1,1,0));
			g_render->CapLineVerts(p2t);
			g_render->CapLineVerts(p3t);
			g_render->AddPointToLineVerts(p3t, Color(1,1,1,0));
			g_render->AddPointToLineVerts(p0t, Color(1,1,1,mouseAlpha));
			g_render->AddPointToLineVerts(p4t, Color(1,1,1,0));
			g_render->CapLineVerts(p4t);
		}
	}

	GameControlBase::RenderPost();
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	surface info
//
////////////////////////////////////////////////////////////////////////////////////////

// surfaces can have a special callback for when a tile is created
void CreateLightTileCallback(const GameSurfaceInfo& tileInfo, const Vector2& pos, int layer)
{
	// automatically spawn a light whenever this tile type gets created
	// note: be frugal with this, only a few lights are necessary to light a scene
	Light* light = new Light(pos, NULL, 10, Color::White(0.75f), true);
	light->SetOverbrightRadius(0.8f);
	light->SetHaloRadius(0.8f);

	//TerrainTile* tile = g_terrain->GetTile(pos);
	//pos.RenderDebug(Color::Red(), 0.5f, 5);
}

void GameControl::InitSurfaceInfoArray()
{
	// setup all the surface infos for the terrain
	// set defaults
	for(int id = 0; id < 255; ++id)
	{
		ASSERT(id < TERRAIN_UNIQUE_TILE_COUNT);
		GameSurfaceInfo& info = GameSurfaceInfo::Get(id);

		info.name = L"Unnamed Tile Type";
		info.ti = Texture_Invalid;
		info.color = Color::White();
		info.backgroundColor = Color::White();
		info.emissiveColor = Color::White();
		info.flags = GSI_Collision|GSI_Destructible;
		info.materialIndex = GMI_Normal;
	}

	if (Terrain::tileSetCount > 0)
	{
		{
			// default transparent colors on the bottom row
			int i = 7;
			GameSurfaceInfo::Get(i  ).name = L"Red Transparent";
			GameSurfaceInfo::Get(i++).shadowColor = Color::Red();
			GameSurfaceInfo::Get(i  ).name = L"Yellow Transparent";
			GameSurfaceInfo::Get(i++).shadowColor = Color::Yellow();
			GameSurfaceInfo::Get(i  ).name = L"Green Transparent";
			GameSurfaceInfo::Get(i++).shadowColor = Color::Green();
			GameSurfaceInfo::Get(i  ).name = L"Blue Transparent";
			GameSurfaceInfo::Get(i++).shadowColor = Color::Blue();
			GameSurfaceInfo::Get(i  ).name = L"Clear Transparent";
			GameSurfaceInfo::Get(i++).shadowColor = Color::White(0);
		}

		/*{
			// automatically create a light for this tile type
			GameSurfaceInfo& info = surfaceInfoArray[12];
			info.name = L"Auto Light";
			info.tileCreateCallback = CreateLightTileCallback;
		}*/
	}
	else
	{
		// non-tile set style terrain
		int id = 0;
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Clear";
			info.ti = Texture_Invalid;
			info.color = Color::White();
			info.backgroundColor = Color::Grey(1, 0.5f);
			info.flags = GSI_None;
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Tile Example";
			info.ti = Texture_tile_test1;
			info.color = Color(0.6f, 0.3f, 0.1f);
			info.backgroundColor = Color(0.6f, 0.3f, 0.1f);
			//info.textureWrapSize = Vector2(0.5f);
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Tile Example Background";
			info.ti = Texture_tile_test1;
			info.color = Color(0.8f, 0.75f, 0.65f);
			info.backgroundColor = Color(0.8f, 0.75f, 0.65f);
			//info.textureWrapSize = Vector2(0.5f);
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Tile Example 2";
			info.ti = Texture_tile_test2;
			info.color = Color::Grey(1, 0.5f);
			info.backgroundColor = Color::Grey(1, 0.8f);
			info.shadowColor = Color::Black();
			info.emissiveColor = Color::Black();
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Black";
			info.ti = Texture_tile_test3;
			info.color = Color::Black();
			info.backgroundColor = Color::Black();
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Grey";
			info.ti = Texture_tile_test3;
			info.color = Color::Grey();
			info.backgroundColor = Color::Grey(1, 0.3f);
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"White";
			info.ti = Texture_tile_test3;
			info.color = Color::White();
			info.backgroundColor = Color::White();
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Red Transparent";
			info.ti = Texture_tile_test3;
			info.color = Color::Red(0.5f);
			info.backgroundColor = Color::Red(0.5f);
			info.shadowColor = Color::Red();
			info.emissiveColor = Color::Black(0.5f);
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Yellow Transparent";
			info.ti = Texture_tile_test3;
			info.color = Color::Yellow(0.5f);
			info.backgroundColor = Color::Yellow(0.5f);
			info.shadowColor = Color::Yellow();
			info.emissiveColor = Color::Black(0.5f);
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Green Transparent";
			info.ti = Texture_tile_test3;
			info.color = Color::Green(0.5f);
			info.backgroundColor = Color::Green(0.5f);
			info.shadowColor = Color::Green();
			info.emissiveColor = Color::Black(0.5f);
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Blue Transparent";
			info.ti = Texture_tile_test3;
			info.color = Color::Blue(0.5f);
			info.backgroundColor = Color::Blue(0.5f);
			info.shadowColor = Color::Blue();
			info.emissiveColor = Color::Blue(0.5f);
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Clear Transparent";
			info.color = Color::White(0.2f);
			info.backgroundColor = Color::White(0.5f);
			info.shadowColor = Color::White(0);
			info.emissiveColor = Color::Black(0);
		}
		{
			GameSurfaceInfo& info = GameSurfaceInfo::Get(id++);
			info.name = L"Auto Light";
			info.color = Color::White();
			info.backgroundColor = Color::White();
			info.shadowColor = Color::Black();
			info.emissiveColor = Color::Black();
			info.tileCreateCallback = CreateLightTileCallback;
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
	{
		g_textHelper->DrawTextLine( L"------------------------" );
		g_textHelper->DrawFormattedTextLine( L"Health: %.1f / %.1f", g_player->GetHealth(),  g_player->GetMaxHealth());
		g_textHelper->DrawFormattedTextLine( L"Time: %.2f", g_player->GetLifeTime());

		//float lightValue = 0;
		//DeferredRender::GetLightValue(g_input->GetMousePosWorldSpace(), lightValue);
		//g_textHelper->DrawFormattedTextLine( L"Light Value Test: %.2f", lightValue);
	}
	g_textHelper->End();
}