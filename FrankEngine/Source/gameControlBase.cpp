////////////////////////////////////////////////////////////////////////////////////////
/*
	Base Class for High Leve Game Control
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"

////////////////////////////////////////////////////////////////////////////////////////
//
//	Game Globals
//
////////////////////////////////////////////////////////////////////////////////////////

InputControl*		g_input		= NULL;
Terrain*			g_terrain	= NULL;
	
bool GameControlBase::showDebugInfo = false;
ConsoleCommand(GameControlBase::showDebugInfo, debugInfoEnable);

bool GameControlBase::autoSaveTerrain = true;
ConsoleCommand(GameControlBase::autoSaveTerrain, autoSaveTerrain);

bool GameControlBase::editorMovePlayerToMouse = true;
ConsoleCommand(GameControlBase::editorMovePlayerToMouse, editorMovePlayerToMouse);

bool GameControlBase::useFrameDeltaSmoothing = true;
ConsoleCommand(GameControlBase::useFrameDeltaSmoothing, useFrameDeltaSmoothing);

bool GameControlBase::useRefreshRateAsFrameDelta = false;
ConsoleCommand(GameControlBase::useRefreshRateAsFrameDelta, useRefreshRateAsFrameDelta);

bool GameControlBase::updateWithoutWindowFocus = false;
ConsoleCommand(GameControlBase::updateWithoutWindowFocus, updateWithoutWindowFocus);

float GameControlBase::debugTimeScale = 1;
ConsoleCommand(GameControlBase::debugTimeScale, timeScale);

bool GameControlBase::alwayShowMouseCursor = false;
ConsoleCommand(GameControlBase::alwayShowMouseCursor, alwayShowMouseCursor);

#ifdef DEBUG
bool GameControlBase::devMode = true;
#else
bool GameControlBase::devMode = false;
#endif // DEBUG
ConsoleCommand(GameControlBase::devMode, devMode);

IntVector2 GameControlBase::debugInfoPos(8, 40);
ConsoleCommand(GameControlBase::debugInfoPos, debugInfoPos);

bool GameControlBase::showFps = false;
ConsoleCommandPinned(GameControlBase::showFps, showFps);

Color GameControlBase::debugTextColor = Color::White(0.8f);
ConsoleCommand(GameControlBase::debugTextColor, debugTextColor);

bool GameControlBase::startInEditMode = false;
ConsoleCommandPinned(GameControlBase::startInEditMode, startInEditMode);

////////////////////////////////////////////////////////////////////////////////////////
//
//	Game Control Member Functions
//
////////////////////////////////////////////////////////////////////////////////////////

GameControlBase::GameControlBase() :
	updateTimer(0),
	totalTimer(0),
	timeScale(1),
	timeScaleLast(1),
	gameMode(GameMode_Normal),
	lastResetMode(GameMode_Normal),
	usingGamepad(false),
	paused(false),
	resetWorldXForms(false),
	wasReset(false),
	allowSecretDevKey(true),
	renderFrameCount(0),
	updateFrameCount(0)
{
	ASSERT(!g_gameControlBase);
}

GameControlBase::~GameControlBase()
{
	// we must remove all objects from the world
	g_objectManager.RemoveAll();

	ASSERT(!GetPathFinding());	// pathfinding must be deleted
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Init, reset and respawn functions
//
////////////////////////////////////////////////////////////////////////////////////////

void GameControlBase::Init() 
{
	Reset();

	if (startInEditMode)
		SetGameMode(GameMode_Edit);
}

void GameControlBase::Reset()
{
	// reload our suface info incase it changed when debugging
	InitSurfaceInfoArray();

	if (GetPlayer() && g_terrain)
		g_terrain->SetPlayerEditorStartPos(GetPlayer()->GetPosWorld());

	g_editor.ResetEditor();
	g_sound->GetMusicPlayer().Reset();

	SetPaused(false);

	resetTime.Set();
	wasReset = true;

	// reset the world, removing all objects set to destroy on reset
	g_objectManager.Reset();
	
	// trigger reset of transforms
	resetWorldXForms = true;

	if (!g_terrain)
	{
		// create some terrain
		const Vector2 pos = -(Vector2(Terrain::fullSize) * TerrainTile::GetSize() * float(Terrain::patchSize)/2.0f) * Vector2(1);
		g_terrain = new Terrain(pos);
		
		// terrain must be loaded before other objects start getting created
		g_terrain->Load(Terrain::terrainFilename);
	}
	else if (GameControlBase::autoSaveTerrain)
	{
		// reload terrain on reset
		g_terrain->Load(Terrain::terrainFilename);
	}

	lastResetMode = gameMode;
	
	// pause music when editing
	g_sound->GetMusicPlayer().Pause(IsEditMode());

	{
		// tell directx what hot keys to handle
		const bool altEnterToToggleFullscreen = true;
		const bool escapeToQuit = IsEditMode();
		const bool pauseToToggleTimePause = false;
		DXUTSetHotkeyHandling(altEnterToToggleFullscreen, escapeToQuit, pauseToToggleTimePause);
	}
}

void GameControlBase::InitSurfaceInfoArray()
{
	// init the surface infos
	GameSurfaceInfo::Init();
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Updating
//
////////////////////////////////////////////////////////////////////////////////////////

float wasteCycleTestValue = 0;

void GameControlBase::Update(float delta)
{
	FrankProfilerEntryDefine(L"GameControlBase::Update()", Color::Yellow(), 1);

	// low frame rate test
	ConsoleCommandSimple(float, wasteCycles, 0);
	wasteCycles = Cap(wasteCycles, 0.0f, 20.0f);
	if (wasteCycles > 0)
	{ for(int i = 0; i < 400000*wasteCycles; ++i) { wasteCycleTestValue += sqrt((float)i); } }

	if (DXUTIsVsyncEnabled() && DXUTGetRefreshRate() > 0)
	{
		if (useFrameDeltaSmoothing)
		{
			// show the actual delta in the output window
			//static WCHAR buffer[64];
			//swprintf_s( buffer, 64, L"%f\n",delta );
			//OutputDebugString(buffer);

			// this buffer keeps track of the extra bits of time
			static float deltaBuffer = 0;

			// add in whatever time we currently have saved in the buffer
			delta += deltaBuffer;

			// save off the delta, we need it later to update the buffer
			const float oldDelta = delta;

			// use refresh rate as delta
			const int refreshRate = DXUTGetRefreshRate();
			delta = 1.0f / refreshRate;

			// update delta buffer so we keep the same time
			deltaBuffer = oldDelta - delta;
			float frames = deltaBuffer / delta;
			frames = floor(frames);
			
			// clamp buffer if it gets too big
			int maxFrames = 3;
			if (frames > maxFrames)
			{
				frames = frames - maxFrames;
				deltaBuffer -= frames * delta;
				delta += frames * delta;
				droppedFrameTimer.Set();
			}

			if (deltaBuffer < -1)
			{
				g_debugMessageSystem.AddError(L"Fast Vsync Detected!\nDelta Smoothing Disabled.");
				useFrameDeltaSmoothing = false;
				deltaBuffer = 0;
				delta = 0;
			}
		}

		if (useRefreshRateAsFrameDelta)
		{
			// the easy way works if we know we can render at the refersh rate
			const int fps = DXUTGetRefreshRate();
			delta = 1 / (float)fps;
		}
	}

	// clamp min frame rate, anything below this will get slowed down
	static float maxDelta = 1/15.0f;
	if (delta > maxDelta)
	{ 
		delta = maxDelta;
		droppedFrameTimer.Set();
	}
	
	GamePauseTimer::UpdateGlobal(delta);
	GetDebugConsole().Update(delta);
	
	static bool debugPauseSkip = false;
	static bool debugPauseSingleStep = false;
	static bool debugPause = false;
	if (devMode)
	{
		// special handling for debug pause because normal input won't work when delta is 0
		static bool pauseWasDown = false;
		const bool pauseIsDown = (GetKeyState(VK_PAUSE) & 0x8000);
		if (pauseIsDown && !pauseWasDown && !IsEditMode())
			debugPause = !debugPause;
		if (debugPause && (GetKeyState(VK_F4) & 0x8000))
			debugPause = false; // allow edit to exit pause
		pauseWasDown = pauseIsDown;
		
		debugPauseSkip = (GetKeyState(VK_ADD) & 0x8000);
		static bool slowWasDown = false;
		const bool slowIsDown = (GetKeyState(VK_SUBTRACT) & 0x8000);
		if (slowIsDown && !slowWasDown)
			debugPauseSingleStep = true;
		slowWasDown = slowIsDown;
	}
	else if (allowSecretDevKey)
	{
		static float devKeyUnlock = 0;
		if (GetKeyState(VK_HOME) & 0x8000)
		{
			// hold home to enter dev mode
			devKeyUnlock += delta;
			if (devKeyUnlock > 3)
			{
				g_debugMessageSystem.Add(L"Dev mode unlocked. Have a nice day.");
				devMode = true;
			}
		}
		else
			devKeyUnlock = 0;
	}

	// apply time scale
	float appliedTimeScale = debugTimeScale;
	if (debugPauseSingleStep)
		appliedTimeScale = 100;
	else if (debugPause && !debugPauseSkip)
		appliedTimeScale = 0;

	if (!paused && !debugPause)
	{
		appliedTimeScale *= timeScale;
		if (devMode)
		{
			if (g_input->IsDown(GB_DebugTimeFast))
				appliedTimeScale *= 5;
			if (g_input->IsDown(GB_DebugTimeSlow))
				appliedTimeScale /= 5;
		}
	}
	delta *= appliedTimeScale;

	// update input
	g_input->Update();

	// step it in fixed increments
	updateTimer += delta;
	while (updateTimer >= GAME_TIME_STEP) 
	{
		updateTimer -= GAME_TIME_STEP;
		delta -= GAME_TIME_STEP;
		if (!paused)
			GameTimer::UpdateGlobal(GAME_TIME_STEP);

		UpdateFrameInternal(GAME_TIME_STEP);
		if (debugPauseSingleStep)
		{
			updateTimer = 0;
			debugPauseSingleStep = false;
		}
	}

	// update interpolation percent
	g_interpolatePercent = (IsGameplayMode()? CalculateInterpolationPercent() : 0);

	// update the sound listener
	g_sound->Update();
	
	// keep track of left over time, this will be used for interpolation during rendering
	if (!paused)
		GameTimer::UpdateInterpolatedGlobal(float(updateTimer));

	const float timeScaleInterpolated = Lerp(g_interpolatePercent, appliedTimeScale, timeScaleLast);
	g_sound->UpdateTimeScale(timeScaleInterpolated);
	g_sound->GetMusicPlayer().SetFrequencyScale(timeScaleInterpolated);
	timeScaleLast = appliedTimeScale;
	
	if (wasReset)
	{
		// init things that must happen after everything else is loaded
		wasReset = false;

		if (GetMiniMap())
			GetMiniMap()->Init();

		// make sure camera is updated after game is reset
		g_cameraBase->Reset();
	}
}

void GameControlBase::UpdateFrameInternal(float delta)
{
	// update the frame count
	if (!paused)
		++updateFrameCount;

	// disable interpolation during update
	g_interpolatePercent = 0;

	g_input->UpdateFrame();
	
	if (devMode)
	{
		if (g_input->WasJustPushed(GB_InfoDisplay))
			showDebugInfo = !showDebugInfo;
		
		{
			int debugMode = 0;
			if (g_input->WasJustPushed(GB_PhysicsDebug))
			{
				debugMode = 1;
				g_physicsRender.SetDebugRender(!g_physicsRender.GetDebugRender());
				g_debugMessageSystem.AddFormatted(L"Physics Debug: %s", g_physicsRender.GetDebugRender() ? L"Enable" : L"Disable");
			}
			if (g_input->WasJustPushed(GB_ParticleDebug))
			{
				debugMode = 2;
				ParticleEmitter::particleDebug = !ParticleEmitter::particleDebug;
				g_debugMessageSystem.AddFormatted(L"Particle Debug: %s", ParticleEmitter::particleDebug ? L"Enable" : L"Disable");
			}
			if (g_input->WasJustPushed(GB_LightDebug))
			{
				debugMode = 3;
				DeferredRender::lightDebug = !DeferredRender::lightDebug;
				g_debugMessageSystem.AddFormatted(L"Light Debug: %s", DeferredRender::lightDebug ? L"Enable" : L"Disable");
			}
			if (g_input->WasJustPushed(GB_ShowRenderPass))
			{
				debugMode = 4;
				DeferredRender::CycleShowTexture();
				g_debugMessageSystem.AddFormatted(L"Light Show Texture: %s", DeferredRender::GetShowTextureModeString());
			}
			if (g_input->WasJustPushed(GB_Profiler))
			{
				debugMode = 5;
				FrankProfiler::ToggleDisplay();
			}
			if (g_input->WasJustPushed(GB_SoundDebug))
			{
				debugMode = 6;
				SoundControl::soundDebug = !SoundControl::soundDebug;
				g_debugMessageSystem.AddFormatted(L"Sound Debug: %s", SoundControl::soundDebug ? L"Enable" : L"Disable");
			}
			if (g_input->WasJustPushed(GB_GamepadDebug))
			{
				debugMode = 7;
				InputControl::gamepadDebug = !InputControl::gamepadDebug;
				g_debugMessageSystem.AddFormatted(L"Gamepad Debug: %s", InputControl::gamepadDebug ? L"Enable" : L"Disable");
			}
			if (debugMode > 0)
			{
				// disable other modes
				if (debugMode != 1)
					g_physicsRender.SetDebugRender(false);
				if (debugMode != 2)
					ParticleEmitter::particleDebug = false;
				if (debugMode != 3)
					DeferredRender::lightDebug = false;
				if (debugMode != 4)
					DeferredRender::showTexture = 0;
				if (debugMode != 5)
					FrankProfiler::showProfileDisplay = false;
				if (debugMode != 6)
					SoundControl::soundDebug = false;
				if (debugMode != 7)
					InputControl::gamepadDebug = false;
			}
		}
	}
	
	if (IsEditMode())
	{
		if (IsEditPreviewMode() && !g_input->IsDown(GB_Editor_Preview))
			SetGameMode(GameMode_Edit);
		else if (g_input->IsDown(GB_Editor_Preview) && gameMode == GameMode_Edit)
		{
			if (editorMovePlayerToMouse)
			{
				// set player to cursor pos before returning to game
				const Vector2 pos = g_input->GetMousePosWorldSpace();
				if (g_physics->IsInWorld(pos))
					g_gameControlBase->GetPlayer()->SetPosWorld(pos);
			}
			if (autoSaveTerrain)
			{
				g_editor.ClearSelection();
				g_terrain->Save(Terrain::terrainFilename);
			}
			SetGameMode(GameMode_EditPreview);
		}
		else if (g_input->WasJustReleased(GB_Editor_Preview) && gameMode == GameMode_EditPreview)
			SetGameMode(GameMode_Edit);
	}

	// toggle editor
	if (g_input->WasJustPushed(GB_Editor) && !IsEditPreviewMode())
	{
		if (IsGameplayMode())
		{
			if (devMode)
				SetGameMode(GameMode_Edit);
		}
		else
		{
			g_editorGui.Reset();

			if (editorMovePlayerToMouse)
			{
				// set player to cursor pos before returning to game
				const Vector2 pos = g_input->GetMousePosWorldSpace();
				if (g_physics->IsInWorld(pos))
					g_gameControlBase->GetPlayer()->SetPosWorld(pos);
			}

			if (autoSaveTerrain)
			{
				g_editor.ClearSelection();
				g_terrain->Save(Terrain::terrainFilename);
			}

			SetGameMode(GameMode_Normal);
		}
	}

	if (g_input->WasJustPushed(GB_RefreshResources))
	{
		g_render->ReloadModifiedTextures();
		DestroyDeviceObjects();
		InitDeviceObjects();
		g_debugMessageSystem.AddFormatted(L"Resources refreshed");
	}

	if (g_input->WasJustPushed(GB_Screenshot))
		g_cameraBase->SaveScreenshot();

	{
		ConsoleCommandSimple(int, gamepadScreenShotButton, -1);
		if (gamepadScreenShotButton >= 0 && g_input->IsGamepadButtonDown(GamepadButtons(GAMEPAD_BUTTON_0 + gamepadScreenShotButton), 0))
			g_cameraBase->SaveScreenshot();
	}

	{
		// setting to automatically take a screenshot every few seconds while playing
		ConsoleCommandSimple(float, autoScreenShotTime, 0);
		if (autoScreenShotTime > 0 && IsGameplayMode())
		{
			static GameTimer screenShotSaveTimer;
			if (!screenShotSaveTimer.IsSet() || screenShotSaveTimer > autoScreenShotTime)
			{
				g_cameraBase->SaveScreenshot();
				screenShotSaveTimer.Set();
			}
		}
	}

	g_debugRender.Update(delta);

	if (!paused)
	{
		g_objectManager.SaveLastWorldTransforms();

		if (IsGameplayMode())
		{
			g_terrain->UpdateActiveWindow();
			g_physics->Update(delta);
			g_objectManager.UpdateTransforms();
		}

		if (IsEditMode())
			g_editor.Update();
		
		// update the camera window
		g_cameraBase->PrepForUpdate();
		g_input->SaveCamerXF();
		
		g_objectManager.Update();
	}

	// check gamepad
	if (!usingGamepad)
		usingGamepad = g_input->HasGamepadInput(0);
	else if 
	(
		g_input->GetMouseDeltaScreenSpace().LengthSquared() > 100 ||
		g_input->WasJustPushed(GB_MouseLeft) ||
		g_input->WasJustPushed(GB_MouseRight) ||
		g_input->WasJustPushed(GB_MouseMiddle)
	)
		usingGamepad = false;
	
	if (GetPathFinding())
		GetPathFinding()->Update();
	if (GetMiniMap())
		GetMiniMap()->Update();
	UpdateFrame(delta);

	if (!paused || resetWorldXForms)
	{
		// update transforms again after object update
		g_objectManager.UpdateTransforms();

		if (resetWorldXForms)
		{
			// reset all transforms when game is reset
			g_objectManager.SaveLastWorldTransforms();
			resetWorldXForms = false;
			g_terrainRender.ClearCache();
		}

		g_input->SaveCamerXF();
	}
	
	if (g_gameControlBase->IsGameplayMode())
		g_guiBase->Refresh();
	g_editorGui.Refresh();
	
	g_debugMessageSystem.Update();

	if (!paused && IsGameplayMode())
		g_terrain->UpdatePost();
}

void GameControlBase::UpdatePost()
{
	if (g_input->IsDownUI(GB_MainMenu_Up))
		g_guiBase->mainDialog.OnCycleFocus(false);
	else if (g_input->IsDownUI(GB_MainMenu_Down))
		g_guiBase->mainDialog.OnCycleFocus(true);

	CDXUTControl* focus = CDXUTDialog::GetFocus();
	if (focus)
	{
		// hack: spoof keyboard event when select button is pushed
		static bool wasDown = false;
		bool isDown = g_input->IsDown(GB_MainMenu_Select);
		if (isDown && !wasDown)
			focus->HandleKeyboard( WM_KEYDOWN, VK_SPACE, 0 );
		else if (!isDown && wasDown)
			focus->HandleKeyboard( WM_KEYUP, VK_SPACE, 0 );
		wasDown = isDown;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Rendering
//
////////////////////////////////////////////////////////////////////////////////////////

ConsoleCommandSimple(bool, enableFrameInterpolation, true);

float GameControlBase::CalculateInterpolationPercent()
{
	if (!enableFrameInterpolation || paused)
		return 0;

	float interpolation = 1 - (float)(updateTimer * GAME_FPS);
	//interpolation = interpolation - floorf(interpolation);

	ASSERT(interpolation >= 0 && interpolation <= 1);
	return CapPercent(interpolation);
}

void GameControlBase::SetupRender()
{
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"GameControlBase::SetupRender()" );

	if (wasReset)
	{
		if (GetMiniMap())
			GetMiniMap()->Init();

		// make sure camera is updated after game is reset
		g_cameraBase->Reset();
		g_cameraBase->Update();
		g_cameraBase->PrepForUpdate();
		g_cameraBase->UpdateTransforms();
		g_cameraBase->ResetLastWorldTransforms();
	}

	// create the list of objects to be rendered sorted by render group
	g_objectManager.CreateRenderList();

	// update the render count
	++renderFrameCount;

	// update terrain rendering
	g_terrainRender.Update();

	// update lights
	DeferredRender::GlobalUpdate();
	
	// update minmap
	if (GetMiniMap())
		GetMiniMap()->SetupRender();
}

void GameControlBase::Render()
{
	FrankProfilerEntryDefine(L"GameControlBase::Render()", Color::Yellow(), 1);

	g_cameraBase->SetAspectRatio(g_aspectRatio);
	g_cameraBase->PrepareForRender();

	// render foreground objects
	RenderInterpolatedObjects();

	// renderLights
	DeferredRender::GlobalRender();

	g_cameraBase->RenderPost();

	if (IsEditMode() && !IsEditPreviewMode())
	{
		g_editor.Render();
		g_render->RenderSimpleVerts();
	}

	g_objectManager.RenderPost();
	
	if (GetMiniMap())
		GetMiniMap()->Render();
}

void GameControlBase::RenderPost()
{
	FrankProfilerEntryDefine(L"RenderPost()", Color::Yellow(), 0);

	g_input->Render();
	g_physics->Render();
	g_debugRender.Render();
	g_debugMessageSystem.Render();
	RenderDebugText();
	FrankProfiler::Render();
	GetDebugConsole().Render();

	if (showFps && !IsEditPreviewMode())
	{
		g_textHelper->Begin();
		g_textHelper->SetInsertionPos( 4, (IsEditMode()? 44 : 4) );
		float fps = DXUTGetFPS();
		Color c = (droppedFrameTimer.IsSet() && droppedFrameTimer < 1.0f)? Color::Red(0.5f) : Color::White(0.5f);
		g_textHelper->SetForegroundColor(c);
		const float totalFrameTime = 1000*FrankProfiler::GetTotalFrameTime();
		g_textHelper->DrawFormattedTextLine( L"%.0f fps - %.2f ms", fps, totalFrameTime );
		g_textHelper->End();
	}

	if (DeferredRender::showTexture != 0)
	{
		g_textHelper->Begin();
		g_textHelper->SetInsertionPos(g_backBufferWidth/2, 5);
		g_textHelper->SetForegroundColor(Color::Red(0.5f));
		g_textHelper->DrawFormattedTextLine(L"Light Texture Debug: %s", DeferredRender::GetShowTextureModeString() );
		g_textHelper->End();
	}
}

void GameControlBase::RenderInterpolatedObjects()
{
	// render out the main object group
	g_objectManager.Render();
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Init & destroy device objects
//
////////////////////////////////////////////////////////////////////////////////////////

void GameControlBase::InitDeviceObjects()
{
	ASSERT(g_render);

	// init the rendering
	g_render->InitDeviceObjects();
	g_debugRender.InitDeviceObjects();
	g_terrainRender.InitDeviceObjects();
	DeferredRender::InitDeviceObjects();
	FrankFont::InitDeviceObjects();

	// load assets
	LoadSounds();
	LoadTextures();

	// setup input
	static bool inputWasSetup = false;
	if (!inputWasSetup)
	{
		inputWasSetup = true;
		SetupInput();
	}

	if (GetMiniMap())
		GetMiniMap()->InitDeviceObjects();
}

void GameControlBase::DestroyDeviceObjects()
{
	if (g_render)
		g_render->DestroyDeviceObjects();
	if (g_sound)
		g_sound->ReleaseSounds();
	g_debugRender.DestroyDeviceObjects();
	g_terrainRender.DestroyDeviceObjects();
	DeferredRender::DestroyDeviceObjects();
	FrankFont::DestroyDeviceObjects();

	if (GetMiniMap())
		GetMiniMap()->DestroyDeviceObjects();
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Game mode functions
//
////////////////////////////////////////////////////////////////////////////////////////

void GameControlBase::SetGameMode(GameMode _gameMode)
{
	gameMode = _gameMode;

	Reset();
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Setup and loading
//
////////////////////////////////////////////////////////////////////////////////////////

void GameControlBase::SetupInput()
{
	// basic controls
	g_input->SetButton(GB_MouseLeft,		VK_LBUTTON);
	g_input->SetButton(GB_MouseMiddle,		VK_MBUTTON);
	g_input->SetButton(GB_MouseRight,		VK_RBUTTON);
	g_input->SetButton(GB_Control,			VK_CONTROL);
	g_input->SetButton(GB_Shift,			VK_SHIFT);
	g_input->SetButton(GB_Alt,				VK_MENU);
	g_input->SetButton(GB_Tab,				VK_TAB);
	g_input->SetButton(GB_Space,			' ');
	g_input->SetButton(GB_Up,				VK_UP);
	g_input->SetButton(GB_Down,				VK_DOWN);
	g_input->SetButton(GB_Right,			VK_RIGHT);
	g_input->SetButton(GB_Left,				VK_LEFT);
	
	// engine controls
	g_input->SetButton(GB_InfoDisplay,		VK_F1);
	g_input->SetButton(GB_PhysicsDebug,		VK_F2);
	g_input->SetButton(GB_RefreshResources,	VK_F3);
	g_input->SetButton(GB_Editor,			VK_F4);
	g_input->SetButton(GB_Screenshot,		VK_F5);
	g_input->SetButton(GB_Profiler,			VK_F6);
	g_input->SetButton(GB_LightDebug,		VK_F7);
	g_input->SetButton(GB_ShowRenderPass,	VK_F8);
	g_input->SetButton(GB_ParticleDebug,	VK_F9);
	g_input->SetButton(GB_SoundDebug,		VK_F10);
	g_input->SetButton(GB_GamepadDebug,		VK_F11);

	// editor controls
	g_input->SetButton(GB_Editor_MouseDrag,			' ');
	g_input->SetButton(GB_Editor_Mode_Terrain1,		'1');
	g_input->SetButton(GB_Editor_Mode_Terrain2,		'2');
	g_input->SetButton(GB_Editor_Mode_Object,		'3');
	g_input->SetButton(GB_Editor_Cut,				'X');
	g_input->SetButton(GB_Editor_Copy,				'C');
	g_input->SetButton(GB_Editor_Paste,				'V');
	g_input->SetButton(GB_Editor_Undo,				'Z');
	g_input->SetButton(GB_Editor_Redo,				'Y');
	g_input->SetButton(GB_Editor_Add,				'Z');
	g_input->SetButton(GB_Editor_Remove,			'X');
	g_input->SetButton(GB_Editor_Insert,			VK_INSERT);
	g_input->SetButton(GB_Editor_Delete,			VK_DELETE);
	g_input->SetButton(GB_Editor_Up,				'W');
	g_input->SetButton(GB_Editor_Down,				'S');
	g_input->SetButton(GB_Editor_Left,				'A');
	g_input->SetButton(GB_Editor_Right,				'D');
	g_input->SetButton(GB_Editor_Save,				'S');
	g_input->SetButton(GB_Editor_Load,				'L');
	g_input->SetButton(GB_Editor_RotateCW,			'E');
	g_input->SetButton(GB_Editor_RotateCCW,			'Q');
	g_input->SetButton(GB_Editor_Mirror,			'F');
	g_input->SetButton(GB_Editor_Preview,			VK_TAB);
	g_input->SetButton(GB_MainMenu_Up,				GAMEPAD_XBOX_DPAD_UP);
	g_input->SetButton(GB_MainMenu_Up,				GAMEPAD_XBOX_DPAD_LEFT);
	g_input->SetButton(GB_MainMenu_Down,			GAMEPAD_XBOX_DPAD_DOWN);
	g_input->SetButton(GB_MainMenu_Down,			GAMEPAD_XBOX_DPAD_RIGHT);
	g_input->SetButton(GB_MainMenu_Select,			GAMEPAD_XBOX_A);
	g_input->SetButton(GB_DebugTimeFast,			VK_ADD);
	g_input->SetButton(GB_DebugTimeSlow,			VK_SUBTRACT);
	
	g_input->SetButton(GB_Editor_Grid,				'G');
	g_input->SetButton(GB_Editor_BlockEdit,			'B');

	// disabled controls
	//g_input->SetButton(GB_Editor_MovePlayer,		'M');
	//g_input->SetButton(GB_Editor_TileSetUp,		'T');
	//g_input->SetButton(GB_Editor_TileSetDown,		'R');
	
}

////////////////////////////////////////////////////////////////////////////////////////
//
//	Render debug text
//
////////////////////////////////////////////////////////////////////////////////////////

void GameControlBase::RenderDebugText()
{
	if (!showDebugInfo)
		return;

	g_textHelper->Begin();
	g_textHelper->SetInsertionPos( debugInfoPos.x, debugInfoPos.y );
	g_textHelper->SetForegroundColor( debugTextColor );
	{
		{
			g_textHelper->DrawFormattedTextLine( L"%s v%s", GetGameTitle(), GetGameVersion() );
			g_textHelper->DrawFormattedTextLine( L"%s v%s", frankEngineName, frankEngineVersion );
			//g_textHelper->DrawTextLine( L"Copyright © 2010 - Frank Force" );
		}

		{
			g_textHelper->DrawTextLine( L"------------------------" );
			g_textHelper->DrawFormattedTextLine( L"fps: %.0f", DXUTGetFPS() );

			const int objectCount = g_objectManager.GetObjectCount();
			const int dynamicObjectCount = g_objectManager.GetDynamicObjectCount();
			if (dynamicObjectCount > 0)
				g_textHelper->DrawFormattedTextLine( L"objects: %d / %d", objectCount - dynamicObjectCount, dynamicObjectCount);
			else
				g_textHelper->DrawFormattedTextLine( L"objects: %d", objectCount);
			//g_textHelper->DrawFormattedTextLine( L"start handle: %d", g_terrain->GetStartHandle());
			g_textHelper->DrawFormattedTextLine( L"largest block: %d / %d", g_objectManager.GetLargestObjectSize(), g_objectManager.GetBlockSize());
			g_textHelper->DrawFormattedTextLine( L"particles: %d / %d", ParticleEmitter::GetTotalEmitterCount(), ParticleEmitter::GetTotalParticleCount());
			g_textHelper->DrawFormattedTextLine( L"lights: %d / %d", DeferredRender::GetSimpleLightCount(), DeferredRender::GetDynamicLightCount());
			g_textHelper->DrawFormattedTextLine( L"sounds: %d", g_sound->GetSoundObjectCount());
			g_textHelper->DrawFormattedTextLine( L"simple verts: %d", g_render->GetTotalSimpleVertsRendered());
			g_textHelper->DrawFormattedTextLine( L"terrain batches: %d", g_terrainRender.renderedBatchCount);
			if (GetPathFinding())
				g_textHelper->DrawFormattedTextLine( L"pathfind cost: %d", GetPathFinding()->GetPathFindCostLastFrame());
			//g_textHelper->DrawFormattedTextLine( L"terrain prims/batches: %d / %d", g_terrainRender.renderedPrimitiveCount, g_terrainRender.renderedBatchCount);
			//g_textHelper->DrawFormattedTextLine( L"next handle: %d", GameObject::GetNextHandle() );
			//g_textHelper->DrawFormattedTextLine( L"start handle: %d", g_terrain->GetStartHandle() );
			//const Vector2 rightStick = g_input->GetGamepadRightStick(0);
			//const Vector2 leftStick = g_input->GetGamepadLeftStick(0);
			const float rightTrigger =  g_input->GetGamepadRightTrigger(0);
			const float leftTrigger = g_input->GetGamepadLeftTrigger(0);
			//g_textHelper->DrawFormattedTextLine( L"rightStick: %f %f %f", rightStick.x, rightStick.y, rightStick.Length() );
			//g_textHelper->DrawFormattedTextLine( L"leftStick: %f %f %f", leftStick.x, leftStick.y, leftStick.Length() );
			//g_textHelper->DrawFormattedTextLine( L"triggers: %0.2f %.2f", leftTrigger, rightTrigger );
			g_textHelper->DrawFormattedTextLine(L"zoom: %.2f", g_cameraBase->GetZoom());

			if (GetPlayer())
			{
				const Vector2 pos = g_input->GetMousePosWorldSpace();
				g_textHelper->DrawFormattedTextLine( L"pos: (%.2f, %.2f)", pos.x, pos.y );

				if (g_terrain)
				{
					int x, y;
					int layer = 0;
					if (IsEditMode() && g_editor.IsTileEdit())
						layer = g_editor.GetTileEditor().GetLayer();
					TerrainTile* tile = g_terrain->GetTile(pos, x, y, layer);
					g_textHelper->DrawFormattedTextLine( L"tile: (%d, %d)", x, y );
					if (tile && IsEditMode())
					{
						int s = tile->GetTileSet();
						int r = tile->GetRotation();
						int m = (tile->GetMirror() ? 1 : 0);
						g_textHelper->DrawFormattedTextLine(L"s: %d  r: %d  m: %d", s, r, m);
					}

					const IntVector2 patchIndex = g_terrain->GetPatchIndex(pos);
					g_textHelper->DrawFormattedTextLine( L"patch: (%d, %d)", patchIndex.x, patchIndex.y );
				}

				if (IsGameplayMode())
					g_textHelper->DrawFormattedTextLine( L"speed: %.2f, %.2f°", GetPlayer()->GetSpeed(), GetPlayer()->GetAngularVelocity() );
			}
		}

		if (g_physicsRender.GetDebugRender())
		{
			g_textHelper->DrawTextLine( L"------------------------" );
			//g_textHelper->DrawFormattedTextLine( L"proxys: %d (%d)", g_physics->GetPhysicsWorld()->GetProxyCount(), b2_maxProxies );
			//g_textHelper->DrawFormattedTextLine( L"pairs: %d (%d)", g_physics->GetPhysicsWorld()->GetPairCount(), b2_maxPairs );
			g_textHelper->DrawFormattedTextLine( L"proxys: %d", g_physics->GetPhysicsWorld()->GetProxyCount() );
			//g_textHelper->DrawFormattedTextLine( L"pairs: %d", g_physics->GetPhysicsWorld()->GetPairCount() );
			g_textHelper->DrawFormattedTextLine( L"bodys: %d", g_physics->GetPhysicsWorld()->GetBodyCount() );
			g_textHelper->DrawFormattedTextLine( L"contacts: %d", g_physics->GetPhysicsWorld()->GetContactCount() );
			g_textHelper->DrawFormattedTextLine( L"joints: %d", g_physics->GetPhysicsWorld()->GetJointCount() );
			g_textHelper->DrawFormattedTextLine( L"raycasts: %d", g_physics->GetRaycastCount() );
			g_textHelper->DrawFormattedTextLine( L"collide checks: %d", g_physics->GetCollideCheckCount() );
			//g_textHelper->DrawFormattedTextLine( L"heap bytes: %d", b2_byteCount );
		}	
	}
	g_textHelper->End();
}

bool GameControlBase::IsObjectEditMode() const	
{ 
	return IsEditMode() && !IsEditPreviewMode() && g_editor.IsObjectEdit(); 
}

bool GameControlBase::IsTileEditMode() const	
{ 
	return IsEditMode() && !IsEditPreviewMode() && g_editor.IsTileEdit(); 
}

bool GameControlBase::ShouldHideMouseCursor() const	
{ 
	return !alwayShowMouseCursor && !IsPaused() && IsGameplayMode();
}

// return the positon of the current user
// in game mode this is the player
// in level edit mode this is the cursor
// position should be interpolated so this function can be called during render
Vector2 GameControlBase::GetUserPosition() const
{
	if (g_gameControlBase->IsEditPreviewMode())
		return g_cameraBase->GetXFInterpolated().position;
	else if (g_gameControlBase->IsEditMode())
		return g_input->GetMousePosWorldSpace();
	else
		return g_gameControlBase->GetPlayer()->GetXFInterpolated().position;
}

Vector2 GameControlBase::GetGravity(const Vector2& pos) const
{
	if (!g_terrain->isCircularPlanet)
		return g_terrain->gravity;

	// use direction from origin
	return g_terrain->gravity.Rotate(pos.GetAngle());

}