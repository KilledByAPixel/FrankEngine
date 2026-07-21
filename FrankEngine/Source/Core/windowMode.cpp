////////////////////////////////////////////////////////////////////////////////////////
/*
	Window Mode Control
	Copyright 2025 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"

#include "core/windowMode.h"
#include "core/debugConsole.h"

// debug builds start windowed, release builds start borderless
#if defined(DEBUG) | defined(_DEBUG) | defined(PROFILE)
WindowDisplayMode WindowControl::mode = WindowDisplayMode::Windowed;
#else
WindowDisplayMode WindowControl::mode = WindowDisplayMode::Borderless;
#endif

RECT WindowControl::windowedRect = { 0, 0, 0, 0 };
LONG WindowControl::windowedStyle = 0;
bool WindowControl::hasSavedPlacement = false;
bool WindowControl::allowExclusiveFullscreen = false;

static const char* windowStateFilename = "windowState.cfg";

// case insensitive search of the raw command line, good enough for a launch flag
static bool HasCommandLineFlag(const WCHAR* flag)
{
	const WCHAR* commandLine = GetCommandLineW();
	if (!commandLine || !flag)
		return false;

	for (const WCHAR* c = commandLine; *c; ++c)
	{
		int i = 0;
		while (flag[i] && towlower(c[i]) == towlower(flag[i]))
			++i;
		if (!flag[i])
			return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------
// console commands
//--------------------------------------------------------------------------------------

// set the window mode directly, 0 = windowed, 1 = borderless, 2 = fullscreen
ConsoleFunction(windowMode)
{
	int newMode = int(WindowControl::GetMode());
	swscanf_s(text.c_str(), L"%d", &newMode);
	WindowControl::SetMode(WindowDisplayMode(newMode));
}

// legacy alias, kept because several games still set this in autoexec.cfg
// selects whichever fullscreen mode is active this run, normally borderless
ConsoleFunction(startFullscreen)
{
	int wantsFullscreen = 0;
	swscanf_s(text.c_str(), L"%d", &wantsFullscreen);
	WindowControl::SetFullscreen(wantsFullscreen != 0);
}

//--------------------------------------------------------------------------------------

const WCHAR* WindowControl::GetModeName(WindowDisplayMode queryMode)
{
	// the player only ever sees two options, borderless and exclusive both
	// present as "Full Screen" since only one of them is reachable per run
	return (queryMode == WindowDisplayMode::Windowed)? L"Window" : L"Full Screen";
}

WindowDisplayMode WindowControl::GetFullscreenMode()
{
	return allowExclusiveFullscreen? WindowDisplayMode::Fullscreen : WindowDisplayMode::Borderless;
}

void WindowControl::SetFullscreen(bool fullscreen)
{
	SetMode(fullscreen? GetFullscreenMode() : WindowDisplayMode::Windowed);
}

void WindowControl::CycleMode()
{
	SetFullscreen(mode == WindowDisplayMode::Windowed);
}

void WindowControl::SetMode(WindowDisplayMode newMode)
{
	if (newMode == mode || newMode < WindowDisplayMode::Windowed || newMode >= WindowDisplayMode::Count)
		return;

	const WindowDisplayMode oldMode = mode;
	mode = newMode;

	// the device has not been created yet, so just record the mode
	// this happens when autoexec.cfg sets the mode during startup
	// ApplyStartupMode() applies it once the device exists
	if (!DXUTGetD3D9Device() || !DXUTGetHWND())
		return;

	// leaving exclusive fullscreen has to go through dxut, it owns the display mode
	if (oldMode == WindowDisplayMode::Fullscreen && !DXUTIsWindowed())
		DXUTToggleFullScreen();

	switch (newMode)
	{
		case WindowDisplayMode::Windowed:
			RestoreWindowed();
			break;

		case WindowDisplayMode::Borderless:
			ApplyBorderless();
			break;

		case WindowDisplayMode::Fullscreen:
			// get back to a normal window before handing the display over to dxut
			RestoreWindowed();
			if (DXUTIsWindowed())
				DXUTToggleFullScreen();
			break;
	}
}

int WindowControl::GetRefreshRate()
{
	HWND hWnd = DXUTGetHWND();
	if (hWnd)
	{
		MONITORINFOEX monitorInfo;
		ZeroMemory(&monitorInfo, sizeof(monitorInfo));
		monitorInfo.cbSize = sizeof(monitorInfo);

		if (GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &monitorInfo))
		{
			DEVMODE devMode;
			ZeroMemory(&devMode, sizeof(devMode));
			devMode.dmSize = sizeof(devMode);

			// 0 and 1 both mean "hardware default" rather than a real rate
			if (EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode)
				&& devMode.dmDisplayFrequency > 1)
				return int(devMode.dmDisplayFrequency);
		}
	}

	// fall back to whatever dxut thinks
	return int(DXUTGetRefreshRate());
}

void WindowControl::ApplyStartupMode()
{
	// exclusive fullscreen is handled when the device is created, nothing to do here
	if (mode == WindowDisplayMode::Borderless)
		ApplyBorderless();
}

void WindowControl::ApplyBorderless()
{
	HWND hWnd = DXUTGetHWND();
	if (!hWnd)
		return;

	// remember where the window was so it can be put back later
	if (!hasSavedPlacement)
	{
		windowedStyle = GetWindowLong(hWnd, GWL_STYLE);
		GetWindowRect(hWnd, &windowedRect);
		hasSavedPlacement = true;
	}

	// cover the monitor the window is currently on
	MONITORINFO monitorInfo;
	monitorInfo.cbSize = sizeof(monitorInfo);
	if (!GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &monitorInfo))
		return;

	// use rcMonitor not rcWork so the window also covers the taskbar
	const RECT& rect = monitorInfo.rcMonitor;

	SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);

	// deliberately not topmost, that makes alt tab and dialogs misbehave
	SetWindowPos(hWnd, HWND_NOTOPMOST, rect.left, rect.top,
		rect.right - rect.left, rect.bottom - rect.top,
		SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	// the resize above sends WM_SIZE, dxut resizes the back buffer to match
}

void WindowControl::RestoreWindowed()
{
	HWND hWnd = DXUTGetHWND();
	if (!hWnd || !hasSavedPlacement)
		return;

	SetWindowLong(hWnd, GWL_STYLE, windowedStyle);
	SetWindowPos(hWnd, HWND_NOTOPMOST, windowedRect.left, windowedRect.top,
		windowedRect.right - windowedRect.left,
		windowedRect.bottom - windowedRect.top,
		SWP_FRAMECHANGED | SWP_SHOWWINDOW);

	hasSavedPlacement = false;
}

void WindowControl::Load()
{
	// decide what "Full Screen" means for this run before anything else
	allowExclusiveFullscreen =
		HasCommandLineFlag(L"-exclusivefullscreen") || HasCommandLineFlag(L"-exclusive");

	FILE* f = NULL;
	if (fopen_s(&f, windowStateFilename, "r") == 0 && f)
	{
		int savedMode = 0;
		if (fscanf_s(f, "%d", &savedMode) == 1)
		{
			if (savedMode >= 0 && savedMode < int(WindowDisplayMode::Count))
				mode = WindowDisplayMode(savedMode);
		}
		fclose(f);
	}

	// clamp the saved mode onto the two states available this run, so a mode
	// saved with the flag set does not strand the next launch without it
	if (mode != WindowDisplayMode::Windowed)
		mode = GetFullscreenMode();
}

void WindowControl::Save()
{
	FILE* f = NULL;
	if (fopen_s(&f, windowStateFilename, "w") != 0 || !f)
		return;

	fprintf(f, "%d", int(mode));
	fclose(f);
}
