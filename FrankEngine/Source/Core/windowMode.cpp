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

static const char* windowStateFilename = "windowState.cfg";

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
// maps onto whichever fullscreen mode this run uses
ConsoleFunction(startFullscreen)
{
	int wantsFullscreen = 0;
	swscanf_s(text.c_str(), L"%d", &wantsFullscreen);
	WindowControl::SetMode(wantsFullscreen? WindowControl::GetFullscreenMode() : WindowDisplayMode::Windowed);
}

//--------------------------------------------------------------------------------------

WindowDisplayMode WindowControl::GetFullscreenMode()
{
	// resolved once: the answer must not change mid run, or the toggle would strand
	// the player between two different fullscreen modes
	static int fullscreenMode = -1;
	if (fullscreenMode < 0)
	{
		const WCHAR* commandLine = GetCommandLineW();
		const bool wantsExclusive = commandLine &&
			(wcsstr(commandLine, L"-exclusivefullscreen") || wcsstr(commandLine, L"-exclusive"));
		fullscreenMode = int(wantsExclusive ? WindowDisplayMode::Fullscreen : WindowDisplayMode::Borderless);
	}
	return WindowDisplayMode(fullscreenMode);
}

//--------------------------------------------------------------------------------------

const WCHAR* WindowControl::GetModeName(WindowDisplayMode queryMode)
{
	// name the mode the window is actually in. only two of these are ever reachable in
	// a run (windowed plus GetFullscreenMode), so the player still sees a two way
	// toggle - it just says which kind of fullscreen this build gave them
	switch (queryMode)
	{
		case WindowDisplayMode::Borderless:	return L"Borderless Mode";
		case WindowDisplayMode::Fullscreen:	return L"Full Screen Mode";
		default:							return L"Window Mode";
	}
}

void WindowControl::CycleMode()
{
	// two options only: windowed and this run's fullscreen mode
	SetMode(mode == WindowDisplayMode::Windowed ? GetFullscreenMode() : WindowDisplayMode::Windowed);
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
	FILE* f = NULL;
	if (fopen_s(&f, windowStateFilename, "r") != 0 || !f)
		return;

	int savedMode = 0;
	if (fscanf_s(f, "%d", &savedMode) == 1)
	{
		// legacy files stored 0 = windowed and 1 = exclusive fullscreen
		// 1 now means borderless, which is a deliberate upgrade for existing players
		if (savedMode >= 0 && savedMode < int(WindowDisplayMode::Count))
		{
			// any saved fullscreen mode maps onto THIS run's fullscreen mode, so a file
			// written with -exclusivefullscreen cannot strand the next launch without it
			mode = (WindowDisplayMode(savedMode) == WindowDisplayMode::Windowed)
				? WindowDisplayMode::Windowed : GetFullscreenMode();
		}
	}
	fclose(f);
}

void WindowControl::Save()
{
	FILE* f = NULL;
	if (fopen_s(&f, windowStateFilename, "w") != 0 || !f)
		return;

	fprintf(f, "%d", int(mode));
	fclose(f);
}
