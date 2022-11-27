////////////////////////////////////////////////////////////////////////////////////////
/*
	Camera
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../objects/camera.h"

static bool clampRenderingToGameWindow = false;
ConsoleCommand(clampRenderingToGameWindow, clampRenderingToGameWindow);

bool Camera::showGameplayCameraWindow = true;
ConsoleCommand(Camera::showGameplayCameraWindow, showGameplayCameraWindow);

bool Camera::followPlayer = true;
ConsoleCommand(Camera::followPlayer, cameraFollowPlayer);

bool Camera::cameraManualOverride = false;
ConsoleCommand(Camera::cameraManualOverride, cameraManualOverride);

float Camera::cameraAngleLerp = 0.03f;
ConsoleCommand(Camera::cameraAngleLerp, cameraAngleLerp);

////////////////////////////////////////////////////////////////////////////////////////
/*
	Camera Member Functions
*/
////////////////////////////////////////////////////////////////////////////////////////

Camera::Camera(const XForm2& xf, float _minGameplayZoom, float _maxGameplayZoom, float _nearClip, float _farClip) :
	GameObject(xf),
	aspectRatio(1.0f),
	nearClip(_nearClip),
	farClip(_farClip),
	xfView(0),
	xfViewInterp(0),
	cameraMax(Vector2::Zero()),
	cameraGameMax(Vector2::Zero()),
	offset(Vector2::Zero()),
	minGameplayZoom(_minGameplayZoom),
	maxGameplayZoom(_maxGameplayZoom),
	lockedAspectRatio(0),
	zoom(_maxGameplayZoom),
	zoomLast(_maxGameplayZoom),
	matrixProjection(Matrix44::Identity()),
	fixedRotation(!Terrain::isCircularPlanet),
	allowZoom(true)
{
	ASSERT(!g_cameraBase);
}

void Camera::Reset()
{
	PrepForUpdate();
	Update();
	UpdateTransforms();
	ResetLastWorldTransforms();
}

void Camera::UpdateTransforms()
{
	Vector2 followPos = GetPosWorld();
	if (followPlayer && !g_gameControlBase->IsEditMode())
	{
		if (g_gameControlBase->GetPlayer())
		{
			if (g_gameControlBase->GetPlayer()->HasPhysics())
				followPos = g_gameControlBase->GetPlayer()->GetXFPhysics().position;
			else
				followPos = g_gameControlBase->GetPlayer()->GetPosWorld();
		}
	}
	if (g_gameControlBase->IsEditMode())
		offset.ZeroThis();

	SetPosWorld(followPos + offset);

	if (g_gameControlBase->IsEditMode() && fixedRotation)
	{
		// set angle to 0 when in fixed rotation mode for map editing
		//SetAngleWorld(0);
	}
	else if (Terrain::isCircularPlanet)
	{
		// rotate around the origin
		
		Vector2 up;
		if (g_gameControlBase->IsEditMode())
			up = followPos.Normalize();
		else
		 up = -g_gameControlBase->GetGravity(followPos);

		if (g_gameControlBase->IsEditMode() || g_gameControlBase->GetResetTime() < 0.1f)
			SetAngleWorld(up.GetAngle());
		else
		{
			float a = GetAngleWorld();
			float aDesired = up.GetAngle();
			float aDelta = CapAngle((aDesired - a));
			SetAngleWorld(Lerp(cameraAngleLerp, a, a + aDelta));
		}
	}

	// set my view transform
	xfView = GetXFWorld().Inverse();
	xfViewInterp = GetXFInterpolated().Inverse();
	
	GameObject::UpdateTransforms();
}

void Camera::PrepareForRender(const XForm2& xf, float zoom)
{
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

	// set the view transform
	xfViewInterp = xf.Inverse();
	const Matrix44 matrixView = Matrix44::BuildXFormZ(xfViewInterp.position, xfViewInterp.angle, 10.0f);
	pd3dDevice->SetTransform( D3DTS_VIEW, &matrixView.GetD3DXMatrix() );
	UpdateProjectionMatrix(zoom);
	pd3dDevice->SetTransform( D3DTS_PROJECTION, &matrixProjection.GetD3DXMatrix() );

	// update the camera culling window
	CalculateCameraWindow(true, &zoom);
}

void Camera::UpdateProjectionMatrix(float zoom)
{
    // Set the projection matrix
	const float height = zoom * GetAspectFix();
	const float width = height * aspectRatio;
	D3DXMatrixOrthoLH( &matrixProjection.GetD3DXMatrix(), width, height, nearClip, farClip );
}

void Camera::PrepForUpdate()
{
	// update camera aspect ratio
	SetAspectRatio(g_aspectRatio);
	
	zoomLast = GetZoom();

	// allow zooming with mouse wheel
	if (allowZoom || g_gameControlBase->IsEditMode())
	{
		// mouse zoom
		float zoomMin = minGameplayZoom;
		float zoomMax = maxGameplayZoom;
		if (cameraManualOverride || g_gameControlBase->IsEditMode() || g_gameControlBase->showDebugInfo)
		{
			zoomMin = 1;
			zoomMax = 1000.0f;
		}

		const float lastZoom = zoom;
		if (g_input->IsDown(GB_Editor_MouseDrag) && g_input->IsDown(GB_MouseRight))
		{
			const float zoomSpeed = 0.2f;
			zoom *= powf(NATURAL_LOG, g_input->GetMouseDeltaScreenSpace().y * zoomSpeed * GAME_TIME_STEP);
		}
		else
		{
			const float zoomSpeed = -0.03f;
			zoom *= powf(NATURAL_LOG, g_input->GetMouseWheel() * zoomSpeed * GAME_TIME_STEP);
		}

		//if (fabs(zoom - maxGameplayZoom) < 0.1f)
		//	zoom = maxGameplayZoom;
		//else 
		if (lastZoom < maxGameplayZoom && zoom > maxGameplayZoom || lastZoom > maxGameplayZoom && zoom < maxGameplayZoom)
		{
			// always use gameplay zoom when crossing the threshold
			// this is for debugging when zooming out to make it easier to get it back to the normal gameplay zoom
			zoom = maxGameplayZoom;
		}

		zoom = Cap(zoom, zoomMin, zoomMax);
	}

	CalculateCameraWindow();	

	if (!g_gameControlBase->IsEditMode() && zoom > maxGameplayZoom && showGameplayCameraWindow && (!cameraManualOverride || g_gameControlBase->showDebugInfo))
	{
		// show where the camera would clip
		Vector2 cameraMax = 0.5f * Vector2(maxGameplayZoom * aspectRatio, maxGameplayZoom);

		const Vector2 c1 = GetXFWorld().TransformCoord(-cameraMax);
		const Vector2 c2 = GetXFWorld().TransformCoord(Vector2(-cameraMax.x, cameraMax.y));
		const Vector2 c3 = GetXFWorld().TransformCoord(cameraMax);
		const Vector2 c4 = GetXFWorld().TransformCoord(Vector2(cameraMax.x, -cameraMax.y));
		
		g_debugRender.RenderLine(c1, c2, Color::Red(0.5f), 0);
		g_debugRender.RenderLine(c2, c3, Color::Red(0.5f), 0);
		g_debugRender.RenderLine(c3, c4, Color::Red(0.5f), 0);
		g_debugRender.RenderLine(c4, c1, Color::Red(0.5f), 0);
	}
}

void Camera::UpdateAABB()
{
	const Vector2 p1 = GetXFWorld().TransformCoord(-cameraMax);
	const Vector2 p2 = GetXFWorld().TransformCoord(Vector2(-cameraMax.x, cameraMax.y));
	const Vector2 p3 = GetXFWorld().TransformCoord(cameraMax);
	const Vector2 p4 = GetXFWorld().TransformCoord(Vector2(cameraMax.x, -cameraMax.y));
		
	cameraWorldAABBox.lowerBound.x = Min(Min(Min(p1.x, p2.x), p3.x), p4.x);
	cameraWorldAABBox.upperBound.x = Max(Max(Max(p1.x, p2.x), p3.x), p4.x);
	cameraWorldAABBox.lowerBound.y = Min(Min(Min(p1.y, p2.y), p3.y), p4.y);
	cameraWorldAABBox.upperBound.y = Max(Max(Max(p1.y, p2.y), p3.y), p4.y);
}

void Camera::CalculateCameraWindow(bool interpolate, const float* zoomOverride)
{
	const float actualAspectRatio = GetAspectFix() * aspectRatio;

	if (zoomOverride)
	{
		const float zoom = (*zoomOverride);
		cameraMax = 0.5f * Vector2(zoom * actualAspectRatio, zoom);
	}
	else if (interpolate)
	{
		// use the larger zoom for this interpolation frame
		const float zoom = GetZoomInterpolated();
		cameraMax = 0.5f * Vector2(zoom * actualAspectRatio, zoom);
	}
	else
	{
		// use the larger zoom for this interpolation frame
		const float zoomLarger = zoom > zoomLast? zoom : zoomLast;
		cameraMax = 0.5f * Vector2(zoomLarger * actualAspectRatio, zoomLarger);
	}
	
	if (!zoomOverride)
	{
		cameraGameMax = 0.5f * Vector2(maxGameplayZoom * actualAspectRatio, maxGameplayZoom);
	}

	if (clampRenderingToGameWindow)
	{
		// clamp to what would be rendered during gameplay
		cameraMax = cameraGameMax;
	}

	UpdateAABB();
}

void Camera::SaveScreenshot() const
{
	WCHAR filename[256];
	#define screenshotDirectory	L"screenshots/"
	#define screenshotPrefix	L"screenshot_"
	#define screenshotFileType	L"png"
	WIN32_FIND_DATA fileData;
	CreateDirectory(screenshotDirectory, NULL);
	HANDLE hFind = FindFirstFile( screenshotDirectory screenshotPrefix L"*." screenshotFileType, &fileData);

	// find the next one in sequence
	int count = 0;
	if (hFind  != INVALID_HANDLE_VALUE)
	{
		WCHAR lastFilename[255];

		do
		{
			swprintf_s(lastFilename, 256, L"%s", fileData.cFileName);
		}
		while (FindNextFile(hFind, &fileData));

		swscanf_s(lastFilename, screenshotPrefix L"%d%*s", &count);
		++count;
	}

	if (count < 10)
		swprintf_s(filename, 256, screenshotDirectory screenshotPrefix L"000%d." screenshotFileType, count);
	else if (count < 100)
		swprintf_s(filename, 256, screenshotDirectory screenshotPrefix L"00%d." screenshotFileType, count);
	else if (count < 1000)
		swprintf_s(filename, 256, screenshotDirectory screenshotPrefix L"0%d." screenshotFileType, count);
	else if (count < 10000)
		swprintf_s(filename, 256, screenshotDirectory screenshotPrefix L"%d." screenshotFileType, count);

	DXUTSnapD3D9Screenshot( filename );

	//g_debugMessageSystem.AddFormatted(L"Screenshot saved to \"%s\"", filename);
}

void Camera::RenderPost()
{
	if (lockedAspectRatio == 0 || g_gameControlBase->IsEditMode())
		return;

	// render black bars to cover area outside of limited aspect ratio
	const float w = (float)g_backBufferWidth;
	const float h = (float)g_backBufferHeight;

	if (aspectRatio > lockedAspectRatio)
	{
		Vector2 size(w * (1 - lockedAspectRatio / aspectRatio)/4, h/2);
		Vector2 pos1(w - size.x, h/2);
		Vector2 pos2(size.x, h/2);

		g_render->RenderScreenSpaceQuad(pos1, size, Color::Black());
		g_render->RenderScreenSpaceQuad(pos2, size, Color::Black());
	}
	else if (aspectRatio < lockedAspectRatio)
	{
		Vector2 size(w/2, h * (1 - aspectRatio / lockedAspectRatio)/4);
		Vector2 pos1(w/2, h - size.y);
		Vector2 pos2(w/2, size.y);

		g_render->RenderScreenSpaceQuad(pos1, size, Color::Black());
		g_render->RenderScreenSpaceQuad(pos2, size, Color::Black());
	}
}

float Camera::GetAspectFix() const
{
	if (g_gameControlBase->IsEditMode())
		return 1; // use normal aspect ratio when in edit mode

	// when using a locked aspect ratio we need to fix the aspect ratio so black bars can appear
	return (aspectRatio > lockedAspectRatio)? 1 : lockedAspectRatio / aspectRatio;
}

Vector2 Camera::GetBackBufferLockedCornerOffset() const
{
	const float w = (float)g_backBufferWidth;
	const float h = (float)g_backBufferHeight;
	Vector2 offset = GetBackBufferCenter();
	
	if (aspectRatio > lockedAspectRatio)
		offset.x -= w * (1 - lockedAspectRatio / aspectRatio) / 2;
	else if (aspectRatio < lockedAspectRatio)
		offset.y -= h * (1 - aspectRatio / lockedAspectRatio) / 2;
	return offset;
}

bool Camera::CameraConeTest(const XForm2& xf, float radius, float coneAngle) const
{
	if (!CameraTest(xf.position, radius))
		return false;	// cone out of range

	if (coneAngle >= 2*PI)
		return true;	// cone is circle
	
	if (CameraTest(xf.position))
		return true;	// cone center is on screen

	// todo: make this better
	// cast rays around the cone to check
	const float angleStep = PI/8;
	float angle = -coneAngle;
	
	// check around outside of cone
	Line2 testLine(xf.position, xf.position);
	while (true)
	{
		bool isLast = angle >= coneAngle;
		if (isLast)
			angle = coneAngle;

		swap(testLine.p1, testLine.p2);
		testLine.p2 = XForm2(xf.position, angle + xf.angle).TransformCoord(Vector2(0, radius));
		//testLine.RenderDebug();
		if (CameraTest(testLine))
			return true; // cone outline is on screen

		if (isLast)
			break;
		angle += angleStep;
	}
	
	// check final line
	swap(testLine.p1, testLine.p2);
	testLine.p2 = xf.position;
	//testLine.RenderDebug();
	if (CameraTest(testLine))
		return true;
	
	// camera is either fully inside or outside cone
	Vector2 coneCameraDirection = GetPosWorld() - xf.position;
	float coneCameraAngle = coneCameraDirection.GetAngle();
	float deltaAngle = CapAngle(coneCameraAngle - xf.angle);
	return (fabs(deltaAngle) < coneAngle);
}

// returns true if circle is at least partially on screen
bool Camera::CameraTest(const Vector2& pos, float radius) const
{
	const Vector2 localPos = xfView.TransformCoord(pos);

	// clamp to render window
	Vector2 nearestPos
	(
		Cap(localPos.x,-cameraMax.x, cameraMax.x),
		Cap(localPos.y,-cameraMax.y, cameraMax.y)
	);
	
	//Line2(pos, xfView.Inverse().TransformCoord(nearestPos)).RenderDebug();
	return ((localPos - nearestPos).LengthSquared() <= Square(radius));
}

// returns true if circle is at least partially on screen
bool Camera::CameraGameTest(const Vector2& pos, float radius) const
{
	const Vector2 localPos = xfView.TransformCoord(pos);
	
	Vector2 cameraBounds = Vector2
	(
		Max(cameraGameMax.x, cameraMax.x),
		Max(cameraGameMax.y, cameraMax.y)
	);

	if (radius == 0)
		return localPos.x > -cameraBounds.x && localPos.x < cameraBounds.x && localPos.y > -cameraBounds.y && localPos.y < cameraBounds.y;

	// clamp to render window
	Vector2 nearestPos
	(
		Cap(localPos.x,-cameraBounds.x, cameraBounds.x),
		Cap(localPos.y,-cameraBounds.y, cameraBounds.y)
	);
	
	//Line2(pos, xfView.Inverse().TransformCoord(nearestPos)).RenderDebug();
	return ((localPos - nearestPos).LengthSquared() <= Square(radius));
}

float Camera::CameraWindowDistance(const Vector2& pos) const
{
	const Vector2 localPos = xfViewInterp.TransformCoord(pos);
	if (localPos.x < cameraMax.x && localPos.x > -cameraMax.x && localPos.y < cameraMax.y && localPos.y > -cameraMax.y)
		return 0;

	Vector2 nearestPos = localPos;
	nearestPos.x = Cap(nearestPos.x, -cameraMax.x, cameraMax.x);
	nearestPos.y = Cap(nearestPos.y, -cameraMax.y, cameraMax.y);

	return (localPos - nearestPos).Length();
}

// returns true if line segment is at least partially on screen
bool Camera::CameraTest(const Line2& line) const
{
	const Line2 l(xfView.TransformCoord(line.p1), xfView.TransformCoord(line.p2));

	if (Vector2::LineSegmentIntersection(l.p1, l.p2, Vector2( cameraMax.x, cameraMax.y), Vector2( cameraMax.x,-cameraMax.y)))
		return true;
	if (Vector2::LineSegmentIntersection(l.p1, l.p2, Vector2(-cameraMax.x, cameraMax.y), Vector2(-cameraMax.x,-cameraMax.y)))
		return true;
	if (Vector2::LineSegmentIntersection(l.p1, l.p2, Vector2(-cameraMax.x, cameraMax.y), Vector2( cameraMax.x, cameraMax.y)))
		return true;
	if (Vector2::LineSegmentIntersection(l.p1, l.p2, Vector2(-cameraMax.x,-cameraMax.y), Vector2( cameraMax.x,-cameraMax.y)))
		return true;
	
	// did not collide with edges, check if a point on line is inside or outside
	return (l.p1.x > -cameraMax.x && l.p1.x < cameraMax.x && l.p1.y > -cameraMax.y && l.p1.y < cameraMax.y);
}
