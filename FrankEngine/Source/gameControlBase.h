////////////////////////////////////////////////////////////////////////////////////////
/*
	Base Class for High Leve Game Control
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

enum GameMode
{
	GameMode_First = 0,
	GameMode_Normal = GameMode_First,
	GameMode_Edit,
	GameMode_EditPreview,
	GameMode_Count
};

class GameControlBase
{
public:

	GameControlBase();
	virtual ~GameControlBase();
	
	virtual void PreInit() {}
	virtual void Init();
	virtual void InitSurfaceInfoArray();
	virtual void Reset();
	virtual void LoadTextures() {}
	virtual void LoadSounds() {}
	virtual void SetupInput();

	virtual void Render();
	virtual void SetupRender();
	virtual void RenderPost();
	virtual void RenderVision() {}
	virtual void RenderInterpolatedObjects();
	virtual void RenderDebugText();
	
	virtual void Update(float delta);
	virtual void UpdatePost();
	void ForceUpdate() { updateTimer = GAME_TIME_STEP; Update(0); }

	virtual void InitDeviceObjects();
	virtual void DestroyDeviceObjects();
	virtual void Shutdown() {}

	InputControl& GetInputControl() { return *g_input; }

	UINT GetRenderFrameCount() const { return renderFrameCount; }
	UINT GetUpdateFrameCount() const { return updateFrameCount; }

	// gamemode functions
	void			SetGameMode(GameMode _gameMode);
	GameMode		GetGameMode() const			{ return gameMode; }
	bool			IsGameplayMode() const		{ return gameMode == GameMode_Normal || gameMode == GameMode_EditPreview; }
	bool			IsEditMode() const			{ return gameMode == GameMode_Edit || gameMode == GameMode_EditPreview; }
	bool			WasEditMode() const			{ return lastResetMode == GameMode_Edit || lastResetMode == GameMode_EditPreview; }
	bool			IsEditPreviewMode() const	{ return gameMode == GameMode_EditPreview; }
	bool			IsObjectEditMode() const;
	bool			IsTileEditMode() const;
	virtual bool	ShouldHideMouseCursor() const;
	bool			IsUsingGamepad() const					{ return usingGamepad; }

	float GetResetTime() const			{ return resetTime; }
	float GetTotalTime() const			{ return totalTimer; }
	float GetTimeScale() const			{ return timeScale; }
	float GetAppliedTimeScale() const	{ return timeScaleLast; }
	float CalculateInterpolationPercent();
	
	bool IsPaused() const				{ return paused; }
	void SetPaused(bool _paused)		{ paused = _paused; }

public: // gameplay stuff
	
	virtual float GetSoundFrequencyScale(const Vector2& pos) const	{ return 1.0f; }

	virtual Vector2 GetUserPosition() const;
	virtual Vector2 GetStreamCenter() const { return GetUserPosition(); }

	// hook from editor to generate random terrain
	virtual void RandomizeTerrain() {}
	virtual void TerrainDeformTileCallback(const Vector2& tilePos, const GameSurfaceInfo& surfaceInfo) {}

	// global function to get gravity at a point
	virtual Vector2 GetGravity(const Vector2& pos) const;

	// check if gamepad rumble is allowed
	virtual bool GamepadCanRumble(UINT gamepadIndex) const { return !IsPaused(); }

public: // bridge to gameside stuff

	virtual const WCHAR* GetGameTitle() const	{ return L"Untitled"; }
	virtual const WCHAR* GetGameVersion() const	{ return L"0.0"; }

	// access to game objects
	virtual GameObject*				GetPlayer()			{ return NULL; }
	virtual struct FrankFont*		GetGameFont(int i=0){ return NULL; }
	virtual class MiniMap*			GetMiniMap()		{ return NULL; }
	virtual class PathFindingBase*	GetPathFinding()	{ return NULL; }

protected:

	virtual void UpdateFrame(float delta) {}
	
	double updateTimer;			// store left over update time
	GameTimer resetTime;		// time since last reset
	GameTimer totalTimer;		// total amount of time it's been running
	GameMode gameMode;			// current game/edit mode
	GameMode lastResetMode;		// last game mode
	float timeScale;			// time dialation factor
	float timeScaleLast;		// time dialation factor
	bool usingGamepad;			// is player using gamepad or keyboard/mouse?
	bool paused;				// is the game in a paused state?
	bool resetWorldXForms;		// will cause all transforms to be reset at end of frame
	bool wasReset;				// was the game just reset?
	bool allowSecretDevKey;		// allows secret key to be held to enter dev mode
	UINT renderFrameCount;		// how many frames have been renderd
	UINT updateFrameCount;		// how many game updates have happened
	GameTimer droppedFrameTimer;// when last dropped a frame

public:

	static bool showDebugInfo;				// debug info is toggled by pressing F1
	static bool autoSaveTerrain;			// save terrain when exiting edit mode
	static bool editorMovePlayerToMouse;	// moves player to mouse when exiting edit mode
	static bool useFrameDeltaSmoothing;		// system to smooth frame rate when fullscreen
	static bool useRefreshRateAsFrameDelta;	// force use the refresh rate as the frame delta
	static bool devMode;					// allow engine function keys and level editor
	static bool updateWithoutWindowFocus;	// allow background updating
	static bool startInEditMode;			// launch edit mode on startup
	static bool showFps;					// display frame rate in corner of screen
	static bool alwayShowMouseCursor;		// set if mouse curser is hidden during gameplay
	static IntVector2 debugInfoPos;			// screen coords to show debug info
	static float debugTimeScale;			// time scale for debugging
	static Color debugTextColor;			// color for debug text rendering

protected:

	virtual void UpdateFrameInternal(float delta);
};
