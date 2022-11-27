////////////////////////////////////////////////////////////////////////////////////////
/*
	Camera
	Copyright 2013 Frank Force - http://www.frankforce.com

	- can be attached to an object
	- culls out stuff that isn't onscreen
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../objects/gameObject.h"

class Camera : public GameObject
{
public:

	Camera(const XForm2& xf = XForm2::Identity(), float _minGameplayZoom = 5, float _maxGameplayZoom = 15, float _nearClip = 1.0f, float _farClip = 1000.0f);

	void SetNearClip(float _nearClip) { nearClip = _nearClip; }
	void SetFarClip(float _farClip) { farClip = _farClip; }

	void SetZoom(const float _zoom) { zoom = _zoom; }
	void SetZoomLast(const float _zoom) { zoomLast = _zoom; }
	float GetZoom() const { return zoom; }
	float GetZoomInterpolated() const { return Lerp(g_interpolatePercent, zoom, zoomLast); }
	const Matrix44& GetMatrixProjection() const { return matrixProjection; }
	
	void SetMinGameplayZoom(float zoom) { minGameplayZoom = zoom; }
	void SetMaxGameplayZoom(float zoom) { maxGameplayZoom = zoom; }
	float GetMinGameplayZoom() const { return minGameplayZoom; }
	float GetMaxGameplayZoom() const { return maxGameplayZoom; }

	// returns true if circle is at least partially on screen
	bool CameraTest(const Vector2& pos, float radius = 0) const;
	bool CameraTest(const Circle& circle) const { return CameraTest(circle.position, circle.radius); }
	bool CameraTest(const Line2& line) const;
	bool CameraConeTest(const XForm2& xf, float radius = 0, float coneAngle = 0) const;
	bool CameraGameTest(const Vector2& pos, float radius = 0) const;
	bool CameraTest(const Box2AABB& aabbox) const { return aabbox.PartiallyContains(cameraWorldAABBox); }
	const Box2AABB& GetCameraBBox() const { return cameraWorldAABBox; }

	float CameraWindowDistance(const Vector2& pos) const;
	void CalculateCameraWindow(bool interpolate = false, const float* zoom = NULL);

	void PrepareForRender() { PrepareForRender(GetXFInterpolated(), GetZoomInterpolated()); }
	void PrepareForRender(const XForm2& xf, float zoom);
	void SaveScreenshot() const;
	bool ShouldStreamOut() const { return false; }
	bool DestroyOnWorldReset() const { return false; }
	void UpdateProjectionMatrix(float zoom);

	void SetAllowZoom(bool _allowZoom) { allowZoom = _allowZoom; }
	bool GetAllowZoom() const { return allowZoom; }

	void SetFixedRotation(bool _fixedRotation) { fixedRotation = _fixedRotation; }
	bool GetFixedRotation() const { return fixedRotation; }
	
	Vector2 GetOffset() const { return offset; }
	void SetOffset(const Vector2& _offset) { offset = _offset; }
	
	void SetAspectRatio(float _aspectRatio) { aspectRatio = _aspectRatio; }
	float GetAspectRatio() const { return aspectRatio; }
	Vector2 GetAspectRatioScale() const { return Vector2(aspectRatio, 1); }

	void SetLockedAspectRatio(float a) { lockedAspectRatio = a; }
	float GetLockedAspectRatio() const { return lockedAspectRatio; }
	float GetAspectFix() const;
	
	Vector2 GetBackBufferCenter() const { return Vector2((float)g_backBufferWidth, (float)g_backBufferHeight) / 2; }
	Vector2 GetBackBufferLockedCornerOffset() const;
	
	void Reset();
	void Update() override {}
	void RenderPost() override;
	void PrepForUpdate();
	void UpdateTransforms() override;
	
	static bool followPlayer;
	static bool showGameplayCameraWindow;
	static bool cameraManualOverride;
	static float cameraAngleLerp;

protected:
	
	void UpdateAABB();

	XForm2 xfView;
	XForm2 xfViewInterp;
	Vector2 cameraMax;
	Vector2 cameraGameMax;
	Vector2 offset;
	Box2AABB cameraWorldAABBox;

	float zoom;
	float zoomLast;
	float minGameplayZoom;
	float maxGameplayZoom;
	float lockedAspectRatio;

	float aspectRatio;
	float nearClip;
	float farClip;

	bool fixedRotation;
	bool allowZoom;

	Matrix44 matrixProjection;
};