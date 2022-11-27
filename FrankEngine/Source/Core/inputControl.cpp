////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Input Processing
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"

#include <Xinput.h>
#include "core/debugConsole.h"

static const int gamepadMaxCount = 4;

bool InputControl::gamepadEnable = true;
ConsoleCommand(InputControl::gamepadEnable, gamepadEnable);

bool InputControl::gamepadDebug = false;
ConsoleCommand(InputControl::gamepadDebug, gamepadDebug);

float InputControl::gamepadRumbleScale = 1;
ConsoleCommand(InputControl::gamepadRumbleScale, gamepadRumbleScale);

bool InputControl::gamepadRumbleEnable = true;
ConsoleCommand(InputControl::gamepadRumbleEnable, gamepadRumbleEnable);

bool InputControl::gamepadRumbleTimeScale = true;
ConsoleCommand(InputControl::gamepadRumbleTimeScale, gamepadRumbleTimeScale);

float InputControl::gamepadDeadzoneMin = 0.3f;
ConsoleCommand(InputControl::gamepadDeadzoneMin, gamepadDeadzoneMin);

float InputControl::gamepadDeadzoneMax = 0.75f;
ConsoleCommand(InputControl::gamepadDeadzoneMax, gamepadDeadzoneMax);

bool InputControl::gamepadEnableSnesControllerEmulation = false;
ConsoleCommand(InputControl::gamepadEnableSnesControllerEmulation, gamepadEnableSnesControllerEmulation);

float InputControl::gamepadStickButtonMin = 0.7f;
ConsoleCommand(InputControl::gamepadStickButtonMin, gamepadStickButtonMin);

float InputControl::gamepadTriggerButtonMin = 0.1f;
ConsoleCommand(InputControl::gamepadTriggerButtonMin, gamepadTriggerButtonMin);

ConsoleFunction(testRumble)
{
	wstringstream inputStream(text.c_str());
	float left = 0;
	float right = 0;
	float duration = 1;
	int gamepad = 0;
	inputStream >> left;
	inputStream >> right;
	inputStream >> duration;
	inputStream >> gamepad;
	if (g_input)
	{
		GetDebugConsole().AddFormatted(Color::White(), L"Rumble( left: %f, right %f, duration %f, gamepad %d )", left, right, duration, gamepad);
		g_input->ApplyRumbleBoth(left, right, duration, gamepad);
	}
}

struct GamepadInfo
{
	XINPUT_STATE inputState	= {0};
	bool connected			= false;
	float leftTrigger		= 0;
	float rightTrigger		= 0;
	Vector2 leftStick		= Vector2(0);
	Vector2 rightStick		= Vector2(0);
	float leftRumble		= 0;
	float rightRumble		= 0;
	int leftRumblePriority	= 0;
	int rightRumblePriority	= 0;
	GameTimer leftRumbleTime;
	GameTimer rightRumbleTime;
};

static GamepadInfo gamepadInfo[gamepadMaxCount] = {NULL};

GamepadInfo& GetGamepadInfo(int gamepadIndex) 
{ 
	ASSERT(gamepadIndex < gamepadMaxCount);
	return gamepadInfo[gamepadIndex];  
}

InputControl::InputControl() :
	wasCleared(false)
{
	ASSERT(!g_input);
	InitGamepads();
	Clear();
}

InputControl::~InputControl()
{
	ClearRumble();
}

void InputControl::Clear()
{
	// if we lost focus reset the input
	ZeroMemory(&mouseInput, sizeof(mouseInput));
	ZeroMemory(&keyboardInput, sizeof(keyboardInput));
	ZeroMemory(&keyboardInputSubFrame, sizeof(keyboardInputSubFrame));

	// clear all the input buttons
	for (int i = 0; i < GB_MaxCount; ++i)
		gameButtonInfo[i].Reset();

	wasCleared = true;
	
	for (int i = 0; i < gamepadMaxCount; ++i)
		GetGamepadInfo(i) = GamepadInfo();
	
	// update mouse inputs to current values
	if ((GetKeyState(VK_LBUTTON) & 0x100) != 0)
		mouseInput.leftDown = true;
	if ((GetKeyState(VK_MBUTTON) & 0x100) != 0)
		mouseInput.middleDown = true;
	if ((GetKeyState(VK_RBUTTON) & 0x100) != 0)
		mouseInput.rightDown = true;
}

void InputControl::Update()
{
	// handle mouse input
	POINT ptCursor;
	GetCursorPos( &ptCursor );
	ScreenToClient( DXUTGetHWND(), &ptCursor );

	// update mouse pos
	mouseInput.pos = Vector2((float)(ptCursor.x), (float)(ptCursor.y));
	
	// update gamepads
	for(int i = 0; i < gamepadMaxCount; ++i)
		UpdateGamepad(i);

	for (int i = 0; i < GB_MaxCount; ++i)
	{
		GameButtonInfo& buttonInfo = gameButtonInfo[i];
		buttonInfo.isDownUI = false;
	}
}

void InputControl::SaveCamerXF()
{
	// save camera transforms
	g_cameraBase->UpdateProjectionMatrix(g_cameraBase->GetZoom());
	matrixProjection.SaveLast();
	matrixProjection = g_cameraBase->GetMatrixProjection();
	cameraXF.SaveLast();
	cameraXF = g_cameraBase->GetXFWorld();
}

void InputControl::UpdateFrame()
{
	if (!GetMouseDeltaScreenSpace().IsZero() || !mouseLastMovedTimer.IsSet())
		mouseLastMovedTimer.Set();

	// save off this frames mouse pos and last frame
	mouseInput.posLast = mouseInput.posFrame;
	mouseInput.posFrame = mouseInput.pos;

	if (wasCleared)
		mouseInput.posLast = mouseInput.pos;
	
	// update all the input buttons
	for (int i = 0; i < GB_MaxCount; ++i)
	{
		GameButtonInfo& buttonInfo = gameButtonInfo[i];
		const bool wasDown = buttonInfo.isDown;
		buttonInfo.isDown = false;
		buttonInfo.wasJustPushed = false;
		buttonInfo.wasJustDoubleClicked = false;

		// check if any of the keys for this button are down
		for (UINT key : buttonInfo.keys) 
		{
			if (key == VK_LBUTTON)
			{
				buttonInfo.isDown |= mouseInput.leftDown;
				buttonInfo.wasJustDoubleClicked |= mouseInput.leftDoubleClick;
				buttonInfo.wasJustPushed |= mouseInput.leftWentDownSubFrame;
			}
			else if (key == VK_MBUTTON)
			{
				buttonInfo.isDown |= mouseInput.middleDown;
				buttonInfo.wasJustDoubleClicked |= mouseInput.middleDoubleClick;
				buttonInfo.wasJustPushed |= mouseInput.middleWentDownSubFrame;
			}
			else if (key == VK_RBUTTON)
			{
				buttonInfo.isDown |= mouseInput.rightDown;
				buttonInfo.wasJustDoubleClicked |= mouseInput.rightDoubleClick;
				buttonInfo.wasJustPushed |= mouseInput.rightWentDownSubFrame;
			}
			else if (key < KEYBOARD_INPUT_SIZE)
			{
				if (GetDebugConsole().IsOpen() && (key < VK_F1 || key > VK_F12))
					continue; // skip keyboard input when in debug console

				buttonInfo.isDown |= IsKeyDown(key);

				// make sure it registered as just pushed if it went down during the sub frame
				buttonInfo.wasJustPushed |= !wasDown && keyboardInputSubFrame[key];
			}
			else if (key >= GAMEPAD_BUTTON_START && key < GAMEPAD_BUTTON_START + GAMEPAD_BUTTON_GROUP_COUNT*gamepadMaxCount)
			{
				UINT actualKey = key;
				UINT gamepadIndex = 0;
				GetGamepadKey(actualKey, gamepadIndex);
				buttonInfo.isDown |= IsGamepadButtonDown(GamepadButtons(actualKey), gamepadIndex);
			}
		}

		if (buttonInfo.wasJustDoubleClicked)
			buttonInfo.isDownFromDoubleClick = true;
		else if (!buttonInfo.isDown)
			buttonInfo.isDownFromDoubleClick = false;

		buttonInfo.wasJustPushed = buttonInfo.wasJustPushed || (!wasDown && buttonInfo.isDown);
		buttonInfo.wasJustReleased = wasDown && !buttonInfo.isDown;

		// handle holding keys buttons for ui stuff
		buttonInfo.isDownUI = false;
		if (buttonInfo.isDown)
		{
			if (buttonInfo.timeUI == 0)
			{
				static const int buttonHoldTimeStart = 20;
				static const int buttonHoldTimeNormal = 5;
				buttonInfo.isDownUI = true;
				buttonInfo.timeUI = buttonInfo.wasJustPushed? buttonHoldTimeStart : buttonHoldTimeNormal;
			}
			else
				--buttonInfo.timeUI;
		}
		else
			buttonInfo.timeUI = 0;
	}

	// reset some of the mouse states
	mouseInput.leftWentDownSubFrame = false;
	mouseInput.middleWentDownSubFrame = false;
	mouseInput.rightWentDownSubFrame = false;
	mouseInput.leftDoubleClick = false;
	mouseInput.middleDoubleClick = false;
	mouseInput.rightDoubleClick = false;
	mouseInput.wheelLast = mouseInput.wheel;
	mouseInput.wheel = 0;
	
	// clear sub frame input
	ZeroMemory(&keyboardInputSubFrame, sizeof(keyboardInputSubFrame));
	wasCleared = false;
}

void InputControl::Render()
{
	if (!gamepadDebug)
		return;
	
	DXUTGetD3D9Device()->SetRenderState(D3DRS_LIGHTING, TRUE);
	const float w = (float)g_backBufferWidth;
	const float h = (float)g_backBufferHeight;
	Vector2 pos, size;
	GamepadInfo& info = GetGamepadInfo(0);
	WCHAR text[256];

	{
		// left stick
		pos = Vector2(w/3, h/2);
		size = Vector2(w/12);
		g_render->RenderScreenSpaceQuad(pos, size, Color::Blue(0.2f), Texture_Invalid, Color::Blue());
		Color c = IsGamepadButtonDown(GAMEPAD_XBOX_LTHUMB, 0) ? Color::Red() : Color::Yellow();
		g_render->RenderScreenSpaceQuad(pos + Vector2(1,-1)*info.leftStick*size.x, size/10, c.ScaleAlpha(0.5), Texture_Invalid, c);
		swprintf_s(text, 256, L"(%.02f, %.02f)", info.leftStick.x, info.leftStick.y);
		g_debugRender.ImmediateRenderTextScreenSpace(pos, text, Color::White());
	}
	{
		// right stick
		pos = Vector2((2*w)/3, h/2);
		size = Vector2(w/12);
		g_render->RenderScreenSpaceQuad(pos, size, Color::Blue(0.2f), Texture_Invalid, Color::Blue());
		Color c = IsGamepadButtonDown(GAMEPAD_XBOX_RTHUMB, 0) ? Color::Red() : Color::Yellow();
		g_render->RenderScreenSpaceQuad(pos + Vector2(1,-1)*info.rightStick*size.x, size/10, c.ScaleAlpha(0.5), Texture_Invalid, c);
		swprintf_s(text, 256, L"(%.02f, %.02f)", info.rightStick.x, info.rightStick.y);
		g_debugRender.ImmediateRenderTextScreenSpace(pos, text, Color::White());
	}
	{
		// left trigger
		pos = Vector2(w/3, h/8);
		size = Vector2(w/12, h / 20);
		g_render->RenderScreenSpaceQuad(pos, size, Color::Blue(0.2f), Texture_Invalid, Color::Blue());
		g_render->RenderScreenSpaceQuad(pos + Vector2(-size.x + 2*info.leftTrigger*size.x, 0), size*Vector2(0.1f, 1), Color::Yellow(0.5f), Texture_Invalid, Color::Yellow());
		swprintf_s(text, 256, L"%.02f", info.leftTrigger);
		g_debugRender.ImmediateRenderTextScreenSpace(pos, text, Color::White());
	}
	{
		// right trigger
		pos = Vector2((2*w)/3, h/8);
		g_render->RenderScreenSpaceQuad(pos, size, Color::Blue(0.2f), Texture_Invalid, Color::Blue());
		g_render->RenderScreenSpaceQuad(pos +  Vector2(-size.x + 2*info.rightTrigger*size.x, 0), size*Vector2(0.1f, 1), Color::Yellow(0.5f), Texture_Invalid, Color::Yellow());
		swprintf_s(text, 256, L"%.02f", info.rightTrigger);
		g_debugRender.ImmediateRenderTextScreenSpace(pos, text, Color::White());
	}
	{
		// left bumper
		pos = Vector2(w/3, h/4);
		size = Vector2(w/12, h / 20);
		Color c = IsGamepadButtonDown(GAMEPAD_XBOX_LB, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos, size, c.ScaleAlpha(0.5), Texture_Invalid, c);
	}
	{
		// right bumper
		pos = Vector2((2*w)/3, h/4);
		size = Vector2(w/12, h / 20);
		Color c = IsGamepadButtonDown(GAMEPAD_XBOX_RB, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos, size, c.ScaleAlpha(0.5), Texture_Invalid, c);
	}
	
	{
		// dpad
		pos = Vector2(w/3, (5*h)/6);
		size = Vector2(w/40);
		Color c;
		c = IsGamepadButtonDown(GAMEPAD_XBOX_DPAD_UP, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos + 2*Vector2(0,-size.y), size, c.ScaleAlpha(0.5), Texture_Invalid, c);
		c = IsGamepadButtonDown(GAMEPAD_XBOX_DPAD_DOWN, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos + 2*Vector2(0, size.y), size, c.ScaleAlpha(0.5), Texture_Invalid, c);
		c = IsGamepadButtonDown(GAMEPAD_XBOX_DPAD_LEFT, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos + 2*Vector2(-size.x,0), size, c.ScaleAlpha(0.5), Texture_Invalid, c);
		c = IsGamepadButtonDown(GAMEPAD_XBOX_DPAD_RIGHT, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos + 2*Vector2(size.x, 0), size, c.ScaleAlpha(0.5), Texture_Invalid, c);
	}
	
	{
		// buttons
		pos = Vector2((2*w)/3, (5*h)/6);
		size = Vector2(w/40);
		Color c;
		c = IsGamepadButtonDown(GAMEPAD_XBOX_Y, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos + 2*Vector2(0,-size.y), size, c.ScaleAlpha(0.5), Texture_Invalid, c);
		c = IsGamepadButtonDown(GAMEPAD_XBOX_A, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos + 2*Vector2(0, size.y), size, c.ScaleAlpha(0.5), Texture_Invalid, c);
		c = IsGamepadButtonDown(GAMEPAD_XBOX_X, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos + 2*Vector2(-size.x,0), size, c.ScaleAlpha(0.5), Texture_Invalid, c);
		c = IsGamepadButtonDown(GAMEPAD_XBOX_B, 0) ? Color::Red() : Color::Blue();
		g_render->RenderScreenSpaceQuad(pos + 2*Vector2(size.x, 0), size, c.ScaleAlpha(0.5), Texture_Invalid, c);
	}


}

void InputControl::OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
	keyboardInput[nChar] = bKeyDown;
	if (bKeyDown)
	{
		keyboardInputSubFrame[nChar] = true;
		GetDebugConsole().OnKeyboard(nChar);
	}
}

void InputControl::SetMousePosScreenSpace(const Vector2& mousePos)
{
	// reset the mouse pos
	mouseInput.pos = mousePos;
	POINT pos = {(long)mousePos.x, (long)mousePos.y};
	ClientToScreen(DXUTGetHWND(), &pos);
	SetCursorPos(pos.x, pos.y);
}

void InputControl::SetMousePosWorldSpace(const Vector2& mousePosWorldSpace)
{
	const Vector2 mousePosLocalSpace = cameraXF.GetInterpolated().Inverse().TransformCoord(mousePosWorldSpace);
    const D3DXMATRIX matrixProjectionD3D = matrixProjection.GetInterpolated().GetD3DXMatrix();
	const Vector2 mousePosScreenSpace
	(
		(mousePosLocalSpace.x * matrixProjectionD3D._11 + 1) * g_backBufferWidth / 2.0f,
		(-mousePosLocalSpace.y * matrixProjectionD3D._22 + 1) * g_backBufferHeight / 2.0f
	);
	// reset the mouse pos
	mouseInput.pos = mousePosScreenSpace;
	POINT pos = {(long)mousePosScreenSpace.x, (long)mousePosScreenSpace.y};
	ClientToScreen(DXUTGetHWND(), &pos);
	SetCursorPos(pos.x, pos.y);
}

void InputControl::OnMouseMessage(UINT uMsg, WPARAM wParam)
{
	switch( uMsg )
	{
		case WM_MOUSEWHEEL:
		{
			mouseInput.wheel += (short)HIWORD(wParam);
			break;
		}

		case WM_LBUTTONDOWN:
		{
			mouseInput.leftDown = true;
			mouseInput.leftWentDownSubFrame = true;
			break;
		}

		case WM_LBUTTONUP:
		{
			mouseInput.leftDown = false;
			break;
		}

		case WM_LBUTTONDBLCLK:
		{
			mouseInput.leftDoubleClick = true;
			mouseInput.leftDown = true;
			break;
		}

		case WM_MBUTTONDOWN:
		{
			mouseInput.middleDown = true;
			mouseInput.middleWentDownSubFrame = true;
			break;
		}

		case WM_MBUTTONUP:
		{
			mouseInput.middleDown = false;
			break;
		}

		case WM_MBUTTONDBLCLK:
		{
			mouseInput.middleDoubleClick = true;
			mouseInput.middleDown = true;
			break;
		}

		case WM_RBUTTONDOWN:
		{
			mouseInput.rightDown = true;
			mouseInput.rightWentDownSubFrame = true;
			break;
		}

		case WM_RBUTTONUP:
		{
			mouseInput.rightDown = false;
			break;
		}

		case WM_RBUTTONDBLCLK:
		{
			mouseInput.rightDoubleClick = true;
			mouseInput.rightDown = true;
			break;
		}

		default:
		{
			ASSERT(true); // this was not a mouse message
			break;
		}
	}
	
	if (g_lostFocusTimer.IsSet() && g_lostFocusTimer < 0.25f && g_gameControlBase->IsEditMode())
	{
		// ignore mouse input just after we regain focus when editing so user doesnt mess up their work
		mouseInput.leftDown = false;
		mouseInput.leftWentDownSubFrame = false;
		mouseInput.leftDoubleClick = false;
		mouseInput.rightDown = false;
		mouseInput.rightWentDownSubFrame = false;
		mouseInput.rightDoubleClick = false;
	}
}

Vector2 InputControl::GetMousePosLocalSpace() const
{
	// FF: code was adapted DirectX SDK Pick Sample

    // store the curser point and projection matrix
    const Vector2& mousePos = mouseInput.pos;
    const D3DXMATRIX matrixProjectionD3D = matrixProjection.GetInterpolated().GetD3DXMatrix();

    // compute the vector of the pick ray in local space
    Vector2 mousePosLocalSpace
	(
		 (((2.0f * mousePos.x) / g_backBufferWidth) - 1) / matrixProjectionD3D._11,
		-(((2.0f * mousePos.y) / g_backBufferHeight) - 1) / matrixProjectionD3D._22
	);

	return mousePosLocalSpace;
}

Vector2 InputControl::GetLastMousePosLocalSpace() const
{
	// FF: code was adapted DirectX SDK Pick Sample

    // store the curser point and projection matrix
    const Vector2& mousePos = mouseInput.posLast;
    const D3DXMATRIX matrixProjectionD3D = matrixProjection.GetInterpolated().GetD3DXMatrix();

    // compute the vector of the pick ray in screen space
    Vector2 mousePosLocalSpace
	(
		 (((2.0f * mousePos.x) / g_backBufferWidth) - 1) / matrixProjectionD3D._11,
		-(((2.0f * mousePos.y) / g_backBufferHeight) - 1) / matrixProjectionD3D._22
	);

	return mousePosLocalSpace;
}

Vector2 InputControl::GetMousePosWorldSpace() const
{
	return cameraXF.GetInterpolated().TransformCoord(GetMousePosLocalSpace());
}

Vector2 InputControl::GetLastMousePosWorldSpace() const
{
	return cameraXF.GetInterpolated().TransformCoord(GetLastMousePosLocalSpace());
}

Vector2 InputControl::GetMouseDeltaWorldSpace() const
{
	return GetMousePosWorldSpace() - GetLastMousePosWorldSpace();
}

////////////////////////////////////////////////////////////////////////////////////////

bool InputControl::HadAnyInput() const
{
	// check if anything was down or is down
	bool hadInput = false;
	for (GameButtonIndex i = GB_Invalid; i < GB_MaxCount; i = (GameButtonIndex)(i + 1))
	{
		const GameButtonInfo& buttonInfo = gameButtonInfo[i];
		// check if any of the keys for this button are down
		for (UINT key : buttonInfo.keys) 
		{
			if (key == VK_LBUTTON)
			{
				hadInput |= mouseInput.leftDown;
				hadInput |= mouseInput.leftDoubleClick;
				hadInput |= mouseInput.leftWentDownSubFrame;
			}
			else if (key == VK_MBUTTON)
			{
				hadInput |= mouseInput.middleDown;
				hadInput |= mouseInput.middleDoubleClick;
				hadInput |= mouseInput.middleWentDownSubFrame;
			}
			else if (key == VK_RBUTTON)
			{
				hadInput |= mouseInput.rightDown;
				hadInput |= mouseInput.rightDoubleClick;
				hadInput |= mouseInput.rightWentDownSubFrame;
			}
			else if (key < KEYBOARD_INPUT_SIZE)
			{
				hadInput |= IsKeyDown(key);
				hadInput |= IsKeyDownSubFrame(key);
			}
		}
	}
	
	for(int i = 0; i < gamepadMaxCount; ++i)
		hadInput |= HasGamepadInput(i);

	return hadInput;
}

void InputControl::UpdateSubFrame()
{
	// for updating mouse pos and other stuff when game is running in slow motion

	// handle mouse input
	POINT ptCursor;
	GetCursorPos( &ptCursor );
	ScreenToClient( DXUTGetHWND(), &ptCursor );
	
	// update mouse pos
	mouseInput.pos = Vector2((float)(ptCursor.x), (float)(ptCursor.y));
	mouseInput.posLast = mouseInput.pos;
	
	for(int i = 0; i < gamepadMaxCount; ++i)
		UpdateGamepad(i);
}

void InputControl::ConsumeInput(GameButtonIndex gbi)
{
	ASSERT(gbi < GB_MaxCount);

	// clear every button that uses any of the same keys
	GameButtonInfo& buttonInfo = gameButtonInfo[gbi];
	for (int i = 0; i < GB_MaxCount; ++i)
	{
		GameButtonInfo& otherButtonInfo = gameButtonInfo[i];
		
		bool matchFound = false;
		for (UINT key : buttonInfo.keys)
		for (UINT otherKey : otherButtonInfo.keys)
		{
			if (key == otherKey)
			{
				matchFound = true;
				break;
			}
		}

		if (matchFound)
		{
			otherButtonInfo.wasJustPushed = false;
			otherButtonInfo.wasJustReleased = false;
			otherButtonInfo.wasJustDoubleClicked = false;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// gamepad stuff

void InputControl::InitGamepads()
{
	if (!gamepadEnable)
		return;
}

bool InputControl::HasGamepad(UINT gamepadIndex) const
{
	if (gamepadIndex >= gamepadMaxCount)
		return false;

	return GetGamepadInfo(gamepadIndex).connected;
}

void InputControl::UpdateGamepad(UINT gamepadIndex)
{
	if (!gamepadEnable)
		return;
	
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	XINPUT_STATE& inputState = info.inputState;
    ZeroMemory(&inputState, sizeof(XINPUT_STATE));
 
    // Get the state of the gamepad
    DWORD hr = XInputGetState(gamepadIndex, &inputState);
	info.connected = (hr == ERROR_SUCCESS);
	if (!info.connected)
		return;
	
	if (gamepadEnableSnesControllerEmulation && GetIsUsingSnesStyleGamepad(gamepadIndex))
	{
		// for gamepads without triggers, use the shoulder buttons as triggers
		info.leftTrigger = IsGamepadButtonDown(GAMEPAD_XBOX_LB, gamepadIndex) ? 1.0f : 0.0f;
		info.rightTrigger = IsGamepadButtonDown(GAMEPAD_XBOX_RB, gamepadIndex) ? 1.0f : 0.0f;
		SetGamepadButtonIsState(GAMEPAD_XBOX_LB, false, gamepadIndex);
		SetGamepadButtonIsState(GAMEPAD_XBOX_RB, false, gamepadIndex);
	}
	else
	{
		// triggers
		BYTE leftTrigger = inputState.Gamepad.bLeftTrigger;
		if (leftTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
			leftTrigger = 0;
		info.leftTrigger = Percent(leftTrigger / 255.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);
		BYTE rightTrigger = inputState.Gamepad.bRightTrigger;
		if (rightTrigger <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
			rightTrigger = 0;
		info.rightTrigger = Percent(rightTrigger / 255.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);
	}

	// left stick
	const SHORT leftThumbX = inputState.Gamepad.sThumbLX;
	if (leftThumbX <= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE && leftThumbX >= -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		info.leftStick.x = 0;
	else if (leftThumbX > 0)
		info.leftStick.x = Percent(leftThumbX / 32767.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);
	else
		info.leftStick.x = -Percent(leftThumbX / -32768.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);

	const SHORT leftThumbY = inputState.Gamepad.sThumbLY;
	if (leftThumbY <= XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE && leftThumbY >= -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		info.leftStick.y = 0;
	else if (leftThumbY > 0)
		info.leftStick.y = Percent(leftThumbY / 32767.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);
	else
		info.leftStick.y = -Percent(leftThumbY / -32768.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);

	// right stick
	if (gamepadEnableSnesControllerEmulation && GetIsUsingSnesStyleGamepad(gamepadIndex))
	{
		// for gamepads without a right stick like a snes controller, use the face buttons to emualte a right stick
		Vector2 v(0);
		if (IsGamepadButtonDown(GAMEPAD_XBOX_Y, gamepadIndex))
			v.y = 1;
		else if (IsGamepadButtonDown(GAMEPAD_XBOX_A, gamepadIndex))
			v.y = -1;
		if (IsGamepadButtonDown(GAMEPAD_XBOX_X, gamepadIndex))
			v.x = -1;
		else if (IsGamepadButtonDown(GAMEPAD_XBOX_B, gamepadIndex))
			v.x = 1;
		info.rightStick = v;

		SetGamepadButtonIsState(GAMEPAD_XBOX_A, false, gamepadIndex);
		SetGamepadButtonIsState(GAMEPAD_XBOX_B, false, gamepadIndex);
		SetGamepadButtonIsState(GAMEPAD_XBOX_X, false, gamepadIndex);
		SetGamepadButtonIsState(GAMEPAD_XBOX_Y, false, gamepadIndex);
	}
	else
	{
		const SHORT rightThumbX = inputState.Gamepad.sThumbRX;
		if (rightThumbX <= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE && rightThumbX >= -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
			info.rightStick.x = 0;
		else if (rightThumbX > 0)
			info.rightStick.x = Percent(rightThumbX / 32767.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);
		else
			info.rightStick.x = -Percent(rightThumbX / -32768.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);

		const SHORT rightThumbY = inputState.Gamepad.sThumbRY;
		if (rightThumbY <= XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE && rightThumbY >= -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
			info.rightStick.y = 0;
		else if (rightThumbY > 0)
			info.rightStick.y = Percent(rightThumbY / 32767.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);
		else
			info.rightStick.y = -Percent(rightThumbY / -32768.0f, gamepadDeadzoneMin, gamepadDeadzoneMax);
	}

	// rumble
	if (gamepadDebug)
	{
		info.leftRumble = info.leftTrigger;
		info.rightRumble = info.rightTrigger;
		info.leftRumbleTime.SetTimeRemaining(0.1f);
		info.rightRumbleTime.SetTimeRemaining(0.1f);
	}

	UpdateRumble(gamepadIndex);
}
 
void InputControl::ClearRumble()
{
	for(int i = 0; i < gamepadMaxCount; ++i)
		SetRumble(0, 0, i);
}
 
void InputControl::UpdateRumble(UINT gamepadIndex)
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	if (!gamepadRumbleEnable || !g_gameControlBase->IsUsingGamepad())
		info.leftRumble = info.rightRumble = 0;

	float leftRumble = info.leftRumbleTime.HasTimeRemaining() ? info.leftRumble : 0;
	float rightRumble = info.rightRumbleTime.HasTimeRemaining() ? info.rightRumble : 0;
	if (!g_gameControlBase->GamepadCanRumble(gamepadIndex))
		leftRumble = rightRumble = 0;

	if (gamepadRumbleTimeScale)
	{
		leftRumble *= Lerp(g_gameControlBase->GetAppliedTimeScale(), 0.2f, 1.0f);
		rightRumble *= Lerp(g_gameControlBase->GetAppliedTimeScale(), 0.2f, 1.0f);
	}

	leftRumble *= gamepadRumbleScale;
	rightRumble *= gamepadRumbleScale;

	SetRumble(leftRumble, rightRumble, gamepadIndex);
}


void InputControl::SetRumble(float left, float right, UINT gamepadIndex)
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);

	// set rumble via xinput
	XINPUT_VIBRATION VibrationState;
    ZeroMemory(&VibrationState, sizeof(XINPUT_VIBRATION));
    VibrationState.wLeftMotorSpeed  = int(left * 65535.0f);
    VibrationState.wRightMotorSpeed	= int(right * 65535.0f);
    XInputSetState(gamepadIndex, &VibrationState);
}
 
void InputControl::ApplyRumbleLeft(float intensity, float duration, UINT gamepadIndex)
{
	ASSERT(intensity >= 0 && intensity <= 1 && duration >= 0);
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);

	if (info.leftRumbleTime.HasTimeRemaining() && intensity < info.leftRumble)
		return;

	info.leftRumble = intensity;
	if (duration == 0)
		info.leftRumbleTime.Unset();
	else
		info.leftRumbleTime.SetTimeRemaining(duration);

	UpdateRumble(gamepadIndex);
}
 
void InputControl::ApplyRumbleRight(float intensity, float duration, UINT gamepadIndex)
{
	ASSERT(intensity >= 0 && intensity <= 1 && duration >= 0);
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	
	if (info.rightRumbleTime.HasTimeRemaining() && intensity < info.rightRumble)
		return;

	info.rightRumble = intensity;
	if (duration == 0)
		info.rightRumbleTime.Unset();
	else
		info.rightRumbleTime.SetTimeRemaining(duration);

	UpdateRumble(gamepadIndex);
}
 
void InputControl::ApplyRumbleBoth(float intensityLeft, float intensityRight, float duration, UINT gamepadIndex)
{
	ApplyRumbleLeft(intensityLeft, duration, gamepadIndex);
	ApplyRumbleRight(intensityRight, duration, gamepadIndex);
	UpdateRumble(gamepadIndex);
}
 
void InputControl::ApplyRumble(float intensity, float duration, UINT gamepadIndex)
{
	ApplyRumbleLeft(intensity, duration, gamepadIndex);
	ApplyRumbleRight(intensity, duration, gamepadIndex);
	UpdateRumble(gamepadIndex);
}
 
void InputControl::StopRumble(UINT gamepadIndex)
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	info.leftRumble = 0;
	info.rightRumble = 0;
	info.leftRumbleTime.Unset();
	info.rightRumbleTime.Unset();
	UpdateRumble(gamepadIndex);
}

bool InputControl::IsRumbling(UINT gamepadIndex) const
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	return (info.leftRumble > 0 && info.leftRumbleTime.HasTimeRemaining()) || (info.rightRumble > 0 && info.rightRumbleTime.HasTimeRemaining());
}

bool InputControl::GetIsUsingSnesStyleGamepad(UINT gamepadIndex) const
{
	/*
	XINPUT_CAPABILITIES capabilities;
	DWORD hr = XInputGetCapabilities(gamepadIndex, 0, &capabilities);
	if (FAILED(hr))
		return false;
	
	bool isSnesGamepad = true;

	// check buttons
	isSnesGamepad &= (capabilities.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
	isSnesGamepad &= (capabilities.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
	isSnesGamepad &= (capabilities.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
	isSnesGamepad &= (capabilities.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
	isSnesGamepad &= (capabilities.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
	isSnesGamepad &= (capabilities.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
	isSnesGamepad &= (capabilities.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
	isSnesGamepad &= (capabilities.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
	
	// check sticks
	isSnesGamepad &= (capabilities.Gamepad.sThumbLX != 0);
	isSnesGamepad &= (capabilities.Gamepad.sThumbLY != 0);
	isSnesGamepad &= (capabilities.Gamepad.sThumbRX == 0);
	isSnesGamepad &= (capabilities.Gamepad.sThumbRY == 0);

	return isSnesGamepad;*/

	// currently xinput only supports xbox 360 style controllers
	return false;
}

bool InputControl::IsGamepadButtonDown(GamepadButtons actualKey, UINT gamepadIndex) const
{
	ASSERT(actualKey >= GAMEPAD_BUTTON_START && actualKey < GAMEPAD_BUTTON_END);
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	if (!gamepadEnable || !info.connected)
		return false;

	if (actualKey <= GAMEPAD_BUTTON_LAST)
	{
		// get the gamepad button input
		const UINT button = actualKey - GAMEPAD_BUTTON_START;
		return (info.inputState.Gamepad.wButtons & (1 << button)) != 0;
	}
	else
	{
		// treat the stick input as a button press
		switch (actualKey)
		{
			case GAMEPAD_BUTTON_UP:				return info.leftStick.y > gamepadStickButtonMin;
			case GAMEPAD_BUTTON_DOWN:			return info.leftStick.y < -gamepadStickButtonMin;
			case GAMEPAD_BUTTON_RIGHT:			return info.leftStick.x > gamepadStickButtonMin;
			case GAMEPAD_BUTTON_LEFT:			return info.leftStick.x < -gamepadStickButtonMin;
			case GAMEPAD_BUTTON_LEFT_TRIGGER:	return info.leftTrigger > gamepadTriggerButtonMin;
			case GAMEPAD_BUTTON_RIGHT_TRIGGER:	return info.rightTrigger > gamepadTriggerButtonMin;
		}
	}

	return false;
}

void InputControl::SetGamepadButtonIsState(GamepadButtons actualKey, bool isDown, UINT gamepadIndex)
{
	ASSERT(actualKey >= GAMEPAD_BUTTON_START && actualKey < GAMEPAD_BUTTON_END);
	ASSERT(actualKey <= GAMEPAD_BUTTON_LAST);
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	if (!gamepadEnable || !info.connected)
		return;

	// get the gamepad button input
	const UINT button = actualKey - GAMEPAD_BUTTON_START;
	if (isDown)
		info.inputState.Gamepad.wButtons |= (1 << button);
	else
		info.inputState.Gamepad.wButtons &= ~(1 << button);
}

bool InputControl::HasGamepadInput(UINT gamepadIndex) const
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	if (!gamepadEnable || !info.connected)
		return false;

	bool hadInput = false;
	hadInput |= !info.leftStick.IsZero();
	hadInput |= !info.rightStick.IsZero();
	hadInput |= info.leftTrigger != 0;
	hadInput |= info.rightTrigger != 0;
	
	for (int button = 0; button <= GAMEPAD_BUTTON_LAST - GAMEPAD_BUTTON_START; ++button)
		hadInput |= (info.inputState.Gamepad.wButtons & (1 << button)) != 0;

	return hadInput;
}

Vector2 InputControl::GetGamepadDPad(UINT gamepadIndex) const					
{ 
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	if (!gamepadEnable || !info.connected)
		return Vector2(0);

	Vector2 v(0);
	if (IsGamepadButtonDown(GAMEPAD_BUTTON_DPAD_UP, gamepadIndex))
		v.y = 1;
	else if (IsGamepadButtonDown(GAMEPAD_BUTTON_DPAD_DOWN, gamepadIndex))
		v.y = -1;
	if (IsGamepadButtonDown(GAMEPAD_BUTTON_DPAD_LEFT, gamepadIndex))
		v.x = -1;
	else if (IsGamepadButtonDown(GAMEPAD_BUTTON_DPAD_RIGHT, gamepadIndex))
		v.x = 1;
	return v;
}

Vector2 InputControl::GetGamepadLeftStick(UINT gamepadIndex) const					
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	return info.leftStick; 
}

Vector2 InputControl::GetGamepadRightStick(UINT gamepadIndex) const				
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	return info.rightStick; 
}

float InputControl::GetGamepadLeftTrigger(UINT gamepadIndex) const					
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	return info.leftTrigger; 
}

float InputControl::GetGamepadRightTrigger(UINT gamepadIndex) const		
{
	GamepadInfo& info = GetGamepadInfo(gamepadIndex);
	return info.rightTrigger; 
}

void InputControl::SetButton(GameButtonIndex gbi, UINT key, UINT gamepadIndex)
{
	ASSERT(gbi < GB_MaxCount);
	ASSERT(key >= GAMEPAD_BUTTON_START && key < GAMEPAD_BUTTON_END);
	ASSERT(gamepadIndex < gamepadMaxCount);
	gameButtonInfo[gbi].keys.push_back(key+GAMEPAD_BUTTON_GROUP_COUNT*gamepadIndex);
}

void InputControl::GetGamepadKey(UINT& key, UINT& gamepadIndex) const
{
	ASSERT(key >= GAMEPAD_BUTTON_START);

	key -= GAMEPAD_BUTTON_START;
	gamepadIndex = (key / GAMEPAD_BUTTON_GROUP_COUNT);
	key -= GAMEPAD_BUTTON_GROUP_COUNT * gamepadIndex;
	key += GAMEPAD_BUTTON_START;

	ASSERT(key >= GAMEPAD_BUTTON_START && key < GAMEPAD_BUTTON_END);
	ASSERT(gamepadIndex < gamepadMaxCount);
}