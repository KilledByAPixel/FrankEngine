////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Camera
	Copyright 2018 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class GameCamera : public Camera
{
public:	// basic functionality

	GameCamera(const XForm2& xf = XForm2::Identity(), float _minGameplayZoom = 5, float _maxGameplayZoom = 15, float _nearClip = 1.0f, float _farClip = 1000.0f);

	void Update() override;
	void Restart();

private:

	Vector2 desiredCameraOffset;
};
