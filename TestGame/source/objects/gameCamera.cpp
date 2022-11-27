////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Camera
	Copyright 2018 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"
#include "gameCamera.h"

static bool cameraOffsetEnable = true;
ConsoleCommand(cameraOffsetEnable, cameraOffsetEnable);

GameCamera::GameCamera(const XForm2& xf, float _minGameplayZoom, float _maxGameplayZoom, float _nearClip, float _farClip) :
	Camera(xf, _minGameplayZoom, _maxGameplayZoom, _nearClip, _farClip)
{ 
	desiredCameraOffset.ZeroThis();
}

void GameCamera::Update()
{
	if (g_gameControl->IsEditMode())
	{
		SetOffset(Vector2(0));
		Camera::Update();
		return;
	}

	// move camera away from where player is aiming
	if (!g_player || g_player->IsDead() || !cameraOffsetEnable || g_gameControl->IsEditMode())
	{
		desiredCameraOffset = Lerp(0.01f, desiredCameraOffset, Vector2(0));
	}
	else if (g_gameControl->IsUsingGamepad())
	{
		float aimedAtObjectPercent = 0;
		const float fadeOnTime = 1.0f;
		const float fadeOffTime = 0.2f;

		// gamepad control
		Vector2 gamepadRightStick = g_input->GetGamepadRightStick(0);
		Vector2 gamepadLeftStick = g_input->GetGamepadLeftStick(0);
		Vector2 stick = gamepadRightStick;
		if (stick.IsZero())
			stick = gamepadLeftStick;

		Vector2 deltaPos = stick * 5 * Vector2(1.0f, 2.0f);
		desiredCameraOffset = Lerp((1 - aimedAtObjectPercent)*0.02f*stick.Length(), desiredCameraOffset, deltaPos);
	}
	else if (!g_player->IsDead())
	{
		// mouse control
		const Vector2 mousePos = g_input->GetMousePosWorldSpace();
		Vector2 deltaPos = (mousePos - g_player->GetPosWorld());
		deltaPos *= Vector2(0.2f, 0.55f);
		desiredCameraOffset = Lerp(0.05f, desiredCameraOffset, 1.0f*deltaPos);
	}
	
	float scale = GetZoom() / GetMaxGameplayZoom();
	float maxX = scale * 2;
	float maxY = scale * 4;
	desiredCameraOffset.x = Cap(desiredCameraOffset.x, -maxX, maxX);
	desiredCameraOffset.y = Cap(desiredCameraOffset.y, -maxY, maxY);
	SetOffset(desiredCameraOffset);

	Camera::Update();
}

void GameCamera::Restart()
{
	desiredCameraOffset.ZeroThis();
}