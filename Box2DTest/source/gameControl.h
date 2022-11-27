////////////////////////////////////////////////////////////////////////////////////////
/*
	High Leve Game Control
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Test.h"

class Player : public GameObject
{
public:
	Player(const XForm2& xf);
	~Player();
	
	bool IsPlayer() const override			{ return true; }
	bool IsOwnedByPlayer() const override	{ return true; }
	bool ShouldStreamOut() const override	{ return false; }
};

class GameControl : public GameControlBase
{
public:

	GameControl();
	~GameControl();
	
	void InitSurfaceInfoArray() override;
	void Reset() override;
	void LoadTextures() override;
	void SetupInput() override;
	void InitDeviceObjects() override;
	void DestroyDeviceObjects() override;
	
	void UpdateFrame(float delta) override;
	void RenderPost() override;
	void RenderDebugText() override;

public: // game stuff

	const WCHAR*		GetGameTitle() const override	{ return gameTitle; }
	const WCHAR*		GetGameVersion() const override	{ return gameVersion; }
	GameObject*			GetPlayer() override			{ return g_player; }
	FrankFont*			GetGameFont(int i) override		{ return g_gameFont; }

private:

	class Test* test = NULL;
	TestEntry* entry = NULL;
	Settings settings;
	int testIndex = 0;
	int testSelection = 0;
	int testCount = 0;
	int updatesNeeded = 0;
};
