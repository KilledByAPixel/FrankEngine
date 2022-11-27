////////////////////////////////////////////////////////////////////////////////////////
/*
	High Leve Game Control
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "player.h"

class GameControl : public GameControlBase
{
public:

	GameControl();
	
	void InitSurfaceInfoArray() override;
	void Reset() override;
	void LoadTextures() override;
	void LoadSounds() override;
	void SetupInput() override;
	void InitDeviceObjects() override;
	void DestroyDeviceObjects() override;
	
	void UpdateFrame(float delta) override;
	void RenderInterpolatedObjects() override;
	void RenderPost() override;
	void RenderDebugText() override;

public: // game stuff

	const WCHAR*		GetGameTitle() const override	{ return gameTitle; }
	const WCHAR*		GetGameVersion() const override	{ return gameVersion; }
	GameObject*			GetPlayer() override			{ return g_player; }
	FrankFont*			GetGameFont(int i) override		{ return g_gameFont; }
	class MiniMap*		GetMiniMap() override			{ return &miniMap; }
	PathFindingBase*	GetPathFinding() override		{ return &pathFinding; }

private:

	MiniMap miniMap;
	PathFindingBase pathFinding;
};
