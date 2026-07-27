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

	THE PLAYER ONLY EVER SEES TWO MODES. Three exist internally, but exactly one of
	them is the "full screen" of any given run, chosen by GetFullscreenMode():
	borderless normally, or exclusive fullscreen when launched with
	-exclusivefullscreen (alias -exclusive). So the toggle is always
	windowed <-> one fullscreen mode, never a three way cycle.

	GetModeName names whichever mode is actually in use - "Window Mode",
	"Borderless Mode" or "Full Screen Mode" - so the button tells the player what they
	really have. The web build has no borderless mode at all (the browser only offers
	real fullscreen), so it shows only Window Mode and Full Screen Mode.

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

	// toggle windowed <-> whichever fullscreen mode this run uses
	static void CycleMode();

	// the fullscreen mode for this run: exclusive only when the command line asked
	// for it, borderless otherwise. fixed at startup, so the toggle is stable
	static WindowDisplayMode GetFullscreenMode();

	// always true on windows; the web build can run where the browser has no
	// fullscreen api at all (ios), so gui code should check before showing a toggle
	static bool IsFullscreenAvailable() { return true; }

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

	static WindowDisplayMode mode;

	// window placement to restore when leaving borderless
	static RECT windowedRect;
	static LONG windowedStyle;
	static bool hasSavedPlacement;
};
