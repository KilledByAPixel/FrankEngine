////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Input Processing
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#define KEYBOARD_INPUT_SIZE			(256)
#define GAMEPAD_BUTTON_GROUP_COUNT	(100)

enum GamepadButtons
{
	GAMEPAD_BUTTON_START = KEYBOARD_INPUT_SIZE,
	GAMEPAD_BUTTON_DPAD_UP = GAMEPAD_BUTTON_START,
	GAMEPAD_BUTTON_DPAD_DOWN,
	GAMEPAD_BUTTON_DPAD_LEFT,
	GAMEPAD_BUTTON_DPAD_RIGHT,
	GAMEPAD_BUTTON_0,
	GAMEPAD_BUTTON_1,
	GAMEPAD_BUTTON_2,
	GAMEPAD_BUTTON_3,
	GAMEPAD_BUTTON_4,
	GAMEPAD_BUTTON_5,
	GAMEPAD_BUTTON_6,
	GAMEPAD_BUTTON_7,
	GAMEPAD_BUTTON_8,
	GAMEPAD_BUTTON_9,
	GAMEPAD_BUTTON_10,
	GAMEPAD_BUTTON_11,
	GAMEPAD_BUTTON_LAST = GAMEPAD_BUTTON_11,
	GAMEPAD_BUTTON_UP,
	GAMEPAD_BUTTON_DOWN,
	GAMEPAD_BUTTON_LEFT,
	GAMEPAD_BUTTON_RIGHT,
	GAMEPAD_BUTTON_LEFT_TRIGGER,
	GAMEPAD_BUTTON_RIGHT_TRIGGER,
	GAMEPAD_BUTTON_END,

	// aliases for XBOX 360 controller
	GAMEPAD_XBOX_DPAD_UP	= GAMEPAD_BUTTON_DPAD_UP,
	GAMEPAD_XBOX_DPAD_DOWN	= GAMEPAD_BUTTON_DPAD_DOWN,
	GAMEPAD_XBOX_DPAD_LEFT	= GAMEPAD_BUTTON_DPAD_LEFT,
	GAMEPAD_XBOX_DPAD_RIGHT	= GAMEPAD_BUTTON_DPAD_RIGHT,
	GAMEPAD_XBOX_START		= GAMEPAD_BUTTON_0,
	GAMEPAD_XBOX_BACK		= GAMEPAD_BUTTON_1,
	GAMEPAD_XBOX_LTHUMB		= GAMEPAD_BUTTON_2,
	GAMEPAD_XBOX_RTHUMB		= GAMEPAD_BUTTON_3,
	GAMEPAD_XBOX_LB			= GAMEPAD_BUTTON_4,
	GAMEPAD_XBOX_RB			= GAMEPAD_BUTTON_5,
	GAMEPAD_XBOX_A			= GAMEPAD_BUTTON_8,
	GAMEPAD_XBOX_B			= GAMEPAD_BUTTON_9,
	GAMEPAD_XBOX_X			= GAMEPAD_BUTTON_10,
	GAMEPAD_XBOX_Y			= GAMEPAD_BUTTON_11
};

// hack because we want some game buttons accessable in the editor
enum GameButtonIndex;
const GameButtonIndex GB_Invalid				= GameButtonIndex(0);
const GameButtonIndex GB_MouseLeft				= GameButtonIndex(1);
const GameButtonIndex GB_MouseMiddle			= GameButtonIndex(2);
const GameButtonIndex GB_MouseRight				= GameButtonIndex(3);
const GameButtonIndex GB_Editor					= GameButtonIndex(4);
const GameButtonIndex GB_InfoDisplay			= GameButtonIndex(5);
const GameButtonIndex GB_PhysicsDebug			= GameButtonIndex(6);
const GameButtonIndex GB_RefreshResources		= GameButtonIndex(7);
const GameButtonIndex GB_Screenshot				= GameButtonIndex(8);
const GameButtonIndex GB_Profiler				= GameButtonIndex(9);
const GameButtonIndex GB_Control				= GameButtonIndex(10);
const GameButtonIndex GB_Shift					= GameButtonIndex(11);
const GameButtonIndex GB_Alt					= GameButtonIndex(12);
const GameButtonIndex GB_Tab					= GameButtonIndex(13);
const GameButtonIndex GB_Space					= GameButtonIndex(14);
const GameButtonIndex GB_Restart				= GameButtonIndex(15);
const GameButtonIndex GB_Up						= GameButtonIndex(16);
const GameButtonIndex GB_Down					= GameButtonIndex(17);
const GameButtonIndex GB_Right					= GameButtonIndex(18);
const GameButtonIndex GB_Left					= GameButtonIndex(19);
const GameButtonIndex GB_Editor_Mode_Object		= GameButtonIndex(20);
const GameButtonIndex GB_Editor_Mode_Terrain1	= GameButtonIndex(21);
const GameButtonIndex GB_Editor_Mode_Terrain2	= GameButtonIndex(22);
const GameButtonIndex GB_Editor_Cut				= GameButtonIndex(23);
const GameButtonIndex GB_Editor_Copy			= GameButtonIndex(24);
const GameButtonIndex GB_Editor_Paste			= GameButtonIndex(25);
const GameButtonIndex GB_Editor_Undo			= GameButtonIndex(26);
const GameButtonIndex GB_Editor_Redo			= GameButtonIndex(27);
const GameButtonIndex GB_Editor_MovePlayer		= GameButtonIndex(28);
const GameButtonIndex GB_Editor_Add				= GameButtonIndex(29);
const GameButtonIndex GB_Editor_Remove			= GameButtonIndex(30);
const GameButtonIndex GB_Editor_Up				= GameButtonIndex(31);
const GameButtonIndex GB_Editor_Down			= GameButtonIndex(32);
const GameButtonIndex GB_Editor_Left			= GameButtonIndex(33);
const GameButtonIndex GB_Editor_Right			= GameButtonIndex(34);
const GameButtonIndex GB_Editor_RotateCW		= GameButtonIndex(35);
const GameButtonIndex GB_Editor_RotateCCW		= GameButtonIndex(36);
const GameButtonIndex GB_Editor_Mirror			= GameButtonIndex(37);
const GameButtonIndex GB_Editor_TileSetUp		= GameButtonIndex(38);
const GameButtonIndex GB_Editor_TileSetDown		= GameButtonIndex(39);
const GameButtonIndex GB_Editor_Save			= GameButtonIndex(40);
const GameButtonIndex GB_Editor_Load			= GameButtonIndex(41);
const GameButtonIndex GB_Editor_Insert			= GameButtonIndex(42);
const GameButtonIndex GB_Editor_Delete			= GameButtonIndex(43);
const GameButtonIndex GB_Editor_Grid			= GameButtonIndex(44);
const GameButtonIndex GB_Editor_BlockEdit		= GameButtonIndex(45);
const GameButtonIndex GB_Editor_Preview			= GameButtonIndex(46);
const GameButtonIndex GB_Editor_MouseDrag		= GameButtonIndex(47);
const GameButtonIndex GB_MainMenu_Up			= GameButtonIndex(48);
const GameButtonIndex GB_MainMenu_Down			= GameButtonIndex(49);
const GameButtonIndex GB_MainMenu_Select		= GameButtonIndex(50);
const GameButtonIndex GB_ShowRenderPass			= GameButtonIndex(51);
const GameButtonIndex GB_ParticleDebug			= GameButtonIndex(52);
const GameButtonIndex GB_LightDebug				= GameButtonIndex(53);
const GameButtonIndex GB_SoundDebug				= GameButtonIndex(54);
const GameButtonIndex GB_GamepadDebug			= GameButtonIndex(55);
const GameButtonIndex GB_DebugTimeFast			= GameButtonIndex(56);
const GameButtonIndex GB_DebugTimeSlow			= GameButtonIndex(57);
const GameButtonIndex GB_StartGameButtons		= GameButtonIndex(58);	// this is where gameside button ids start
const GameButtonIndex GB_MaxCount				= GameButtonIndex(256);	// total amount of allowable buttons

class InputControl
{
public:

	InputControl();
	~InputControl();

	void Update();
	void UpdateFrame();
	void UpdateSubFrame();
	void Render();
	void Clear();
	void ClearRumble();
	bool HadAnyInput() const;
	void SaveCamerXF();

	void SetButton(GameButtonIndex buttonIndex, UINT key);
	void SetButton(GameButtonIndex buttonIndex, UINT key, UINT gamepadIndex);
	void ClearButtons(GameButtonIndex buttonIndex);
	bool IsDown(GameButtonIndex gbi) const;
	bool WasJustPushed(GameButtonIndex gbi) const;
	bool WasJustReleased(GameButtonIndex gbi) const;
	bool IsDownFromDoubleClick(GameButtonIndex gbi) const;
	bool WasJustDoubleClicked(GameButtonIndex gbi) const;
	void ConsumeInput(GameButtonIndex gbi);
	bool IsDownUI(GameButtonIndex gbi) const;
	void ClearInput(GameButtonIndex gbi);

	// keyboard input
	bool IsKeyDown(UINT key) const { ASSERT(key < KEYBOARD_INPUT_SIZE); return keyboardInput[key]; }
	bool IsKeyDownSubFrame(UINT key) const { ASSERT(key < KEYBOARD_INPUT_SIZE); return keyboardInputSubFrame[key]; }

	// gamepad input
	bool HasGamepad(UINT gamepadIndex = 0) const;
	bool HasGamepadInput(UINT gamepadIndex = 0) const;
	void GetGamepadKey(UINT& key, UINT& gamepadIndex) const;
	bool IsGamepadButtonDown(GamepadButtons button, UINT gamepadIndex = 0) const;
	void SetGamepadButtonIsState(GamepadButtons button, bool isDown, UINT gamepadIndex = 0);
	Vector2 GetGamepadDPad(UINT gamepadIndex = 0) const;
	Vector2 GetGamepadLeftStick(UINT gamepadIndex = 0) const;
	Vector2 GetGamepadRightStick(UINT gamepadIndex = 0) const;
	float GetGamepadLeftTrigger(UINT gamepadIndex = 0) const;
	float GetGamepadRightTrigger(UINT gamepadIndex = 0) const;
	bool GetIsUsingSnesStyleGamepad(UINT gamepadIndex = 0) const;

	// gamepad rumble
	// for 360 controllers the left is the heavy and the right is light
	void ApplyRumbleLeft(float intensity, float duration, UINT gamepadIndex = 0);
	void ApplyRumbleRight(float intensity, float duration, UINT gamepadIndex = 0);
	void ApplyRumbleBoth(float intensityLeft, float intensityRight, float duration, UINT gamepadIndex = 0);
	void ApplyRumble(float intensity, float duration, UINT gamepadIndex = 0);
	void StopRumble(UINT gamepadIndex = 0);
	bool IsRumbling(UINT gamepadIndex = 0) const;
	void UpdateRumble(UINT gamepadIndex);

	// mouse input
	float GetMouseWheel() const;
	Vector2 GetMousePosScreenSpace() const { return mouseInput.pos; }
	Vector2 GetMouseDeltaScreenSpace() const { return mouseInput.pos - mouseInput.posLast; }
	void SetMousePosScreenSpace(const Vector2& mousePos);
	void SetMousePosWorldSpace(const Vector2& mousePos);
	float GetMouseLastMovedTime() const { return mouseLastMovedTimer; }

	Vector2 GetMousePosLocalSpace() const;
	Vector2 GetLastMousePosLocalSpace() const;
	Vector2 GetMousePosWorldSpace() const;
	Vector2 GetLastMousePosWorldSpace() const;
	Vector2 GetMouseDeltaWorldSpace() const;

public:

	void OnMouseMessage(UINT uMsg, WPARAM wParam);
	void OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
	void SetRumble(float left, float right, UINT gamepadIndex = 0);

	static bool gamepadEnable;
	static bool gamepadDebug;
	static bool gamepadRumbleEnable;
	static float gamepadRumbleScale;
	static bool gamepadRumbleTimeScale;
	static float gamepadDeadzoneMin;
	static float gamepadDeadzoneMax;
	static bool gamepadEnableSnesControllerEmulation;
	static float gamepadStickButtonMin;
	static float gamepadTriggerButtonMin;

private:
	
	void InitGamepads();
	void UpdateGamepad(UINT gamepadIndex = 0);
	void UpdateGamepadRumble(UINT gamepadIndex = 0);

	struct MouseInput
	{
		bool rightDown;
		bool rightDoubleClick;
		bool middleDown;
		bool middleDoubleClick;
		bool leftDown;
		bool leftDoubleClick;

		bool rightWentDownSubFrame;
		bool middleWentDownSubFrame;
		bool leftWentDownSubFrame;

		Vector2 pos;
		Vector2 posLast;
		Vector2 posFrame;
		float wheelLast;
		float wheel;
	};

	struct GameButtonInfo
	{
		GameButtonInfo() { Reset(); }
		void Reset() 
		{
			timeUI = 0; 
			isDown = false;
			isDownUI = false; 
			wasJustPushed = false;
			wasJustReleased = false;
			wasJustDoubleClicked = false;
			isDownFromDoubleClick = false;
		}

		list<UINT> keys;
		
		int timeUI;
		bool isDown;
		bool isDownUI;
		bool wasJustPushed;
		bool wasJustReleased;
		bool wasJustDoubleClicked;
		bool isDownFromDoubleClick;
	};

	MouseInput mouseInput;
	bool keyboardInput[KEYBOARD_INPUT_SIZE];
	bool keyboardInputSubFrame[KEYBOARD_INPUT_SIZE];
	GameButtonInfo gameButtonInfo[GB_MaxCount];
	bool wasCleared;
	GameTimer mouseLastMovedTimer;

	ValueInterpolator<Matrix44> matrixProjection = Matrix44::Identity();
	ValueInterpolator<XForm2> cameraXF = XForm2::Identity();
};

// inline functions

inline void InputControl::ClearInput(GameButtonIndex gbi)
{
	ASSERT(gbi < GB_MaxCount);
	gameButtonInfo[gbi].Reset();
}

inline void InputControl::SetButton(GameButtonIndex gbi, UINT key)
{
	ASSERT(gbi < GB_MaxCount);
	gameButtonInfo[gbi].keys.push_back(key);
}

inline void InputControl::ClearButtons(GameButtonIndex gbi)
{
	ASSERT(gbi < GB_MaxCount);
	gameButtonInfo[gbi].keys.clear();
}

inline bool InputControl::IsDown(GameButtonIndex gbi) const
{
	ASSERT(gbi < GB_MaxCount);
	return gameButtonInfo[gbi].isDown;
}

inline bool InputControl::IsDownUI(GameButtonIndex gbi) const
{
	ASSERT(gbi < GB_MaxCount);
	return gameButtonInfo[gbi].isDownUI || WasJustPushed(gbi);
}

inline bool InputControl::WasJustPushed(GameButtonIndex gbi) const
{
	ASSERT(gbi < GB_MaxCount);
	return gameButtonInfo[gbi].wasJustPushed;
}

inline bool InputControl::WasJustReleased(GameButtonIndex gbi) const
{
	ASSERT(gbi < GB_MaxCount);
	return gameButtonInfo[gbi].wasJustReleased;
}

inline bool InputControl::IsDownFromDoubleClick(GameButtonIndex gbi) const
{
	ASSERT(gbi < GB_MaxCount);
	return gameButtonInfo[gbi].isDownFromDoubleClick;
}

inline bool InputControl::WasJustDoubleClicked(GameButtonIndex gbi) const
{
	ASSERT(gbi < GB_MaxCount);
	return gameButtonInfo[gbi].wasJustDoubleClicked;
}

inline float InputControl::GetMouseWheel() const 
{ 
	return mouseInput.wheelLast; 
}
