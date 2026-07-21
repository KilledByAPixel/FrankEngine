////////////////////////////////////////////////////////////////////////////////////////
/*
	Window Mode Control
	Copyright 2025 Frank Force - http://www.frankforce.com

	Controls how the game window is presented.

		Windowed	- normal window with a title bar and border
		Borderless	- frameless window covering the monitor it is on
		Fullscreen	- legacy exclusive fullscreen, changes the display mode via dxut

	Borderless is the preferred mode. Exclusive fullscreen has to change the display
	mode, which makes it slow to enter and painful to alt tab out of. Borderless keeps
	the device windowed as far as directx is concerned, so switching is just a window
	resize and the display mode is never touched.

	To the player this is a two state toggle, Window and Full Screen. Which one
	"Full Screen" means is decided at launch: normally it is Borderless, but passing
	-exclusivefullscreen (or -exclusive) on the command line makes it the old
	exclusive mode instead, for anyone who needs it for capture or latency reasons.
	Do not use dxut's own -fullscreen or -windowed flags, they override the device
	underneath this class and leave the mode here stale.

	IMPORTANT: do not use DXUTIsWindowed() to test for fullscreen anymore. In borderless
	mode the device really is windowed, so that check returns true even though the window
	is covering the whole screen. Use IsCoveringScreen() instead.
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

enum class WindowDisplayMode
{
	Windowed,
	Borderless,
	Fullscreen,
	Count
};

class WindowControl
{
public:

	static WindowDisplayMode GetMode() { return mode; }

	// change the window mode, does nothing if already in that mode
	static void SetMode(WindowDisplayMode newMode);

	// toggles between windowed and whatever fullscreen means this run
	static void CycleMode();

	// go windowed or fullscreen without caring which fullscreen mode is active
	static void SetFullscreen(bool fullscreen);

	// true if launched with -exclusivefullscreen
	static bool IsExclusiveFullscreenAllowed() { return allowExclusiveFullscreen; }

	// display name for a mode, for gui buttons
	static const WCHAR* GetModeName() { return GetModeName(mode); }
	static const WCHAR* GetModeName(WindowDisplayMode queryMode);

	// true when the window covers the whole screen, borderless or exclusive
	static bool IsCoveringScreen() { return mode != WindowDisplayMode::Windowed; }

	// refresh rate of the monitor the window is actually on
	// prefer this over DXUTGetRefreshRate(), which reports the adapter's desktop
	// mode and so returns the wrong monitor on a mixed refresh rate setup
	static int GetRefreshRate();

	// load and save the mode to windowState.cfg
	static void Load();
	static void Save();

	// apply the loaded mode, called once after the device is created
	static void ApplyStartupMode();

private:

	static void ApplyBorderless();
	static void RestoreWindowed();

	// which mode the "Full Screen" option maps to this run
	static WindowDisplayMode GetFullscreenMode();

	static WindowDisplayMode mode;
	static bool allowExclusiveFullscreen;

	// window placement to restore when leaving borderless
	static RECT windowedRect;
	static LONG windowedStyle;
	static bool hasSavedPlacement;
};
