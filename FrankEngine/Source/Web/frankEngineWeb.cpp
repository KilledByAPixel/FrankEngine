////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Web Shell
	Copyright 2013 Frank Force - http://www.frankforce.com

	- replaces frankEngine.cpp (the DXUT shell) on the web build
	- FrankEngineStartup/Init/Shutdown mirror the windows flow minus device creation
	- FrankEngineLoop drives the fixed-timestep update via emscripten_set_main_loop
	- phase 3 scope: headless boot - stub renderer/sound/input, heartbeat logging
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "../frankEngine.h"

#ifndef FRANK_PLATFORM_WEB
#error frankEngineWeb.cpp is web-build only
#endif

#include <emscripten.h>
#include <emscripten/html5.h>
#include <ctime>

//--------------------------------------------------------------------------------------
// ?cvar= on the url runs console commands at startup, right after autoexec.cfg and so
// overriding it. semicolons separate commands, '+' or %20 separates a command from its
// value:  ?cvar=webTerrainCache+0;webFlushEveryDraws+0
// There are no config data files in this engine - every tunable is a console var - so
// this is how a web build gets A/B'd without rebuilding the 12MB data package.
// NOTE: no backslashes in here. EM_JS stringifies the body through the C preprocessor,
// so a regex escape like \+ is eaten as an unknown C escape and the JS breaks at runtime
EM_JS(void, WebJsGetUrlCvars, (char* out, int maxSize), {
	out[0] = 0;
	var search = (location.search || "") + (location.hash || "");
	var start = search.indexOf("cvar=");
	if (start < 0)
		return;
	var text = search.substring(start + 5);
	var amp = text.indexOf("&");
	if (amp >= 0) text = text.substring(0, amp);
	var hash = text.indexOf("#");
	if (hash >= 0) text = text.substring(0, hash);
	stringToUTF8(decodeURIComponent(text.split("+").join(" ")), out, maxSize);
});

static void WebRunUrlConsoleCommands()
{
	char cvars[1024];
	WebJsGetUrlCvars(cvars, sizeof(cvars));

	for (char* command = cvars; *command; )
	{
		char* end = strchr(command, ';');
		if (end)
			*end = 0;

		// the console takes wide strings; these are ascii command lines
		wstring line;
		for (const char* c = command; *c; ++c)
			line += (wchar_t)*c;
		if (!line.empty())
		{
			printf("webUrlCvar: %s\n", command);
			GetDebugConsole().ParseInputLine(line, true);
		}

		command = end ? end + 1 : command + strlen(command);
	}
}

//--------------------------------------------------------------------------------------
// Frank Engine Globals (mirrors frankEngine.cpp)
int								g_backBufferWidth = 0;
int								g_backBufferHeight = 0;
float							g_aspectRatio = 0;
GameControlBase*				g_gameControlBase = NULL;
GuiBase*						g_guiBase = NULL;
Camera*							g_cameraBase = NULL;
FrankRender*					g_render = NULL;
Physics*						g_physics = NULL;
EditorGui						g_editorGui;
Editor							g_editor;
CDXUTTextHelper*				g_textHelper = NULL;
GameTimer						g_lostFocusTimer;

bool	g_usePointFiltering = false;
float	g_tileSetUVScale = 1 - (1/32.0f);	// matches frankRender.cpp default
ConsoleCommand(g_tileSetUVScale, tileSetUVScale);	// same console name as windows
Color	g_backBufferClearColor			= Color::Grey(1, 0.1f);
ConsoleCommand(g_backBufferClearColor, backBufferClearColor);
Color	g_editorBackBufferClearColor	= Color::Grey(1, 0.2f);
ConsoleCommand(g_editorBackBufferClearColor, editorBackBufferClearColor);

//--------------------------------------------------------------------------------------
// save persistence: .sav files live in the in-memory FS, so they are mirrored into
// localStorage (base64) - restored before the game loads saves, synced periodically

static void FrankWebRestoreSaves()
{
	EM_ASM({
		try {
			if (typeof localStorage === 'undefined') return;
			for (var i = 0; i < localStorage.length; ++i) {
				var key = localStorage.key(i);
				if (!key || key.indexOf('frankfs/') !== 0) continue;
				var name = key.slice(8);
				var bin = atob(localStorage.getItem(key));
				var data = new Uint8Array(bin.length);
				for (var j = 0; j < bin.length; ++j) data[j] = bin.charCodeAt(j);
				FS.writeFile('/' + name, data);
				out('restored ' + name + ' from localStorage (' + data.length + ' bytes)');
			}
		} catch (e) { out('save restore failed: ' + e); }
	});
}

static void FrankWebPersistSaves()
{
	EM_ASM({
		try {
			if (typeof localStorage === 'undefined') return;
			var files = FS.readdir('/');
			for (var i = 0; i < files.length; ++i) {
				var name = files[i];
				if (name.slice(-4) !== '.sav') continue;
				var data = FS.readFile('/' + name);
				var s = "";
				for (var j = 0; j < data.length; j += 32768)
					s += String.fromCharCode.apply(null, data.subarray(j, j + 32768));
				var b64 = btoa(s);
				var key = 'frankfs/' + name;
				if (localStorage.getItem(key) !== b64)
					localStorage.setItem(key, b64);
			}
		} catch (e) {}
	});
}

//--------------------------------------------------------------------------------------
void FrankEngineStartup(const WCHAR* title, int maxObjectCount, size_t maxObjectSize)
{
	printf("FrankEngineStartup (web)\n");

	// pull saves out of localStorage before anything loads them
	FrankWebRestoreSaves();

	// set random seeds
	srand((unsigned)time(NULL));
	FrankRand::SetSeed((unsigned)time(NULL));

	// allocate memory for objects
	g_objectManager.Init(maxObjectCount, maxObjectSize);
}

//--------------------------------------------------------------------------------------
void FrankEngineInit(int width, int height, GameControlBase* gameControl, GuiBase* gameGui, Camera* camera)
{
	ASSERT(gameControl && !g_gameControlBase);
	ASSERT(gameGui && !g_guiBase);
	ASSERT(camera && !g_cameraBase);

	// parse the autoexec debug commands
	GetDebugConsole().ParseFile(L"autoexec.cfg", false);
	WebRunUrlConsoleCommands();		// ?cvar=... overrides, applied after autoexec
	GetDebugConsole().Init();

	// init high level game objects
	g_sound = new SoundControl();
	g_input = new InputControl();
	g_physics = new Physics();
	g_physics->Init();
	ParticleEmitter::InitParticleSystem();
	TerrainTile::BuildCache();
	g_cameraBase = camera;
	g_gameControlBase = gameControl;
	g_guiBase = gameGui;
	g_guiBase->Init();

	// create the WebGL2 context (headless runs continue without rendering)
	extern bool FrankWebRenderCreateContext();
	FrankWebRenderCreateContext();
	g_backBufferWidth = width;
	g_backBufferHeight = height;
	g_aspectRatio = (float)width / (float)height;
	g_gameControlBase->PreInit();
	g_guiBase->OnResetDevice();	// gui Init ran before the backbuffer size was known - lay out for real now

	// windows creates FrankRender in the device-created callback; do it here
	g_render = new FrankRender();
	g_textHelper = new CDXUTTextHelper();	// console/debug text draws through this

	// web lighting scope (phase 5): the deferred pipeline runs for real, but without
	// normal mapping (Piroot ships with it off; TestGame turns it on for windows) and
	// without the vision pass. this must happen before InitDeviceObjects so the
	// deferred init doesn't try to create normal-map targets.
	DeferredRender::normalMappingEnable = false;
	DeferredRender::visionEnable = false;

	// mirror the reset-device path so the game gets its init hooks
	// (this also runs the real DeferredRender::InitDeviceObjects - fbo targets, shaders)
	g_gameControlBase->InitDeviceObjects();

	// init the game control before gameplay starts
	g_gameControlBase->Init();

	printf("FrankEngineInit (web) done - %dx%d\n", width, height);
}

//--------------------------------------------------------------------------------------
static int webFrameCount = 0;
static double webLastTime = 0;
static bool webFirstUpdate = true;

static void FrankEngineWebFrame()
{
	const double now = emscripten_get_now() / 1000.0;
	float delta = (float)(now - webLastTime);
	webLastTime = now;
	FrankWebNoteFrameDelta(delta);	// raw cadence feeds the refresh-rate estimate
	if (delta > 0.25f)
		delta = 0.25f;

	if (!g_gameControlBase)
		return;

	if (webFirstUpdate)
	{
		webFirstUpdate = false;
		g_gameControlBase->ForceUpdate();
	}
	else
		g_gameControlBase->Update(delta);

	g_gameControlBase->UpdatePost();

	// render the frame, mirroring the OnD3D9FrameRender flow (frankEngine.cpp:454):
	// per-frame state reset -> SetupRender (deferred passes render to fbos) ->
	// restore the canvas target -> clear -> scene -> gui overlay
	extern bool FrankWebRenderIsActive();
	extern void FrankWebRenderBegin();
	extern void FrankWebRenderEnd();
	extern int webRenderMode;
	extern void WebMinimalRenderFrame();
	if (webRenderMode == 1 && FrankWebRenderIsActive())
	{
		// experiment: replace the whole frame with a self-verifying grid (webRender.cpp)
		WebMinimalRenderFrame();
	}
	else if (FrankWebRenderIsActive() && g_render)
	{
		extern void WebGridRenderEarly();
		WebGridRenderEarly();	// mode 3: grid into its own fbo, verified at end of frame

		g_render->ResetTotalSimpleVertsRendered();
		FrankWebRenderBegin();				// reset device state, bind canvas + viewport
		g_gameControlBase->SetupRender();	// DeferredRender::GlobalUpdate renders the light fbos
		FrankWebRenderBegin();				// re-bind the canvas (mirrors the backbuffer restore)
		g_render->BeginRender(true, g_backBufferClearColor, D3DCLEAR_TARGET);
		g_gameControlBase->Render();		// includes DeferredRender::GlobalRender overlay
		g_render->RenderSimpleVerts();
		if (g_guiBase)
			g_guiBase->Render(delta);	// menus render in every mode, like OnD3D9FrameRender
		g_gameControlBase->RenderPost();
		g_render->RenderSimpleVerts();
		extern void WebGridOverlayFrame();
		WebGridOverlayFrame();	// mode 2: verified grid on top of the real frame
		extern void WebGridVerifyLate();
		WebGridVerifyLate();	// mode 3: read back the fbo grid drawn before the frame

		g_render->EndRender();
		FrankWebRenderEnd();

		extern void WebFlickerProbeEndFrame();
		WebFlickerProbeEndFrame();
	}

	if (++webFrameCount % 60 == 0)
	{
		const int bodyCount = (g_physics && g_physics->GetPhysicsWorld()) ? g_physics->GetPhysicsWorld()->GetBodyCount() : -1;
		printf("heartbeat: frame %d, physics bodies %d\n", webFrameCount, bodyCount);
	}

	// mirror changed .sav files into localStorage every couple seconds (wall time,
	// not frames - frame counts stall under software rendering)
	static double lastPersistTime = 0;
	if (now - lastPersistTime > 2.0)
	{
		lastPersistTime = now;
		FrankWebPersistSaves();
	}

	// headless (node) gate from phase 3: prove 600 clean sim frames then exit;
	// in a browser (window exists) the loop runs forever
	if (webFrameCount == 600 && EM_ASM_INT({ return (typeof window === 'undefined') ? 1 : 0; }))
	{
		printf("PHASE 3 GATE PASS: 600 frames simulated headless\n");
		emscripten_force_exit(0);
	}
}

//--------------------------------------------------------------------------------------
// browser input -> the engine's own InputControl, mirroring the DXUT callbacks.
// browser keyCode values match win32 VK codes for the keys games use.

static EM_BOOL WebOnKey(int eventType, const EmscriptenKeyboardEvent* e, void* userData)
{
	if (!g_input)
		return EM_FALSE;
	const bool down = (eventType == EMSCRIPTEN_EVENT_KEYDOWN);
	if (e->keyCode < 256)
		g_input->OnKeyboard(e->keyCode, down, e->altKey, NULL);
	// let F-keys and browser chords through; swallow the rest (arrows scroll the page)
	return (e->keyCode >= VK_F1 && e->keyCode <= VK_F12) ? EM_FALSE : EM_TRUE;
}

static void WebUpdateMousePos(long targetX, long targetY)
{
	// css pixels -> backbuffer pixels (canvas may be scaled by style)
	double cssW, cssH;
	emscripten_get_element_css_size("#canvas", &cssW, &cssH);
	const float x = (cssW > 0) ? (float)(targetX * g_backBufferWidth / cssW) : (float)targetX;
	const float y = (cssH > 0) ? (float)(targetY * g_backBufferHeight / cssH) : (float)targetY;
	// feed the fake win32 cursor - InputControl::Update polls GetCursorPos each frame
	FrankWebGetCursor().x = (LONG)x;
	FrankWebGetCursor().y = (LONG)y;
	g_input->SetMousePosScreenSpace(Vector2(x, y));
}

////////////////////////////////////////////////////////////////////////////////////////
// XInput over the browser Gamepad API - inputControl.cpp's gamepad path runs unmodified
////////////////////////////////////////////////////////////////////////////////////////

DWORD XInputGetState(DWORD index, XINPUT_STATE* state)
{
	if (!state)
		return ERROR_DEVICE_NOT_CONNECTED;
	memset(state, 0, sizeof(XINPUT_STATE));

	// headless node (and browsers before the first button press) report no pads
	if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS)
		return ERROR_DEVICE_NOT_CONNECTED;
	EmscriptenGamepadEvent pad;
	if (emscripten_get_gamepad_status((int)index, &pad) != EMSCRIPTEN_RESULT_SUCCESS || !pad.connected)
		return ERROR_DEVICE_NOT_CONNECTED;

	// w3c "standard" mapping -> xinput. non-standard pads get the same best-effort map.
	static const struct { int button; WORD flag; } buttonMap[] =
	{
		{ 0,  XINPUT_GAMEPAD_A },
		{ 1,  XINPUT_GAMEPAD_B },
		{ 2,  XINPUT_GAMEPAD_X },
		{ 3,  XINPUT_GAMEPAD_Y },
		{ 4,  XINPUT_GAMEPAD_LEFT_SHOULDER },
		{ 5,  XINPUT_GAMEPAD_RIGHT_SHOULDER },
		{ 8,  XINPUT_GAMEPAD_BACK },
		{ 9,  XINPUT_GAMEPAD_START },
		{ 10, XINPUT_GAMEPAD_LEFT_THUMB },
		{ 11, XINPUT_GAMEPAD_RIGHT_THUMB },
		{ 12, XINPUT_GAMEPAD_DPAD_UP },
		{ 13, XINPUT_GAMEPAD_DPAD_DOWN },
		{ 14, XINPUT_GAMEPAD_DPAD_LEFT },
		{ 15, XINPUT_GAMEPAD_DPAD_RIGHT },
	};
	for (const auto& m : buttonMap)
	{
		if (m.button < pad.numButtons && pad.digitalButton[m.button])
			state->Gamepad.wButtons |= m.flag;
	}

	// triggers are analog buttons 6/7 in the standard mapping
	if (pad.numButtons > 6) state->Gamepad.bLeftTrigger  = (BYTE)(pad.analogButton[6] * 255.0);
	if (pad.numButtons > 7) state->Gamepad.bRightTrigger = (BYTE)(pad.analogButton[7] * 255.0);

	// sticks: browser y is down-positive, xinput is up-positive
	const auto toThumb = [](double v) { return (short)(Cap(v, -1.0, 1.0) * 32767.0); };
	if (pad.numAxes > 0) state->Gamepad.sThumbLX = toThumb(pad.axis[0]);
	if (pad.numAxes > 1) state->Gamepad.sThumbLY = toThumb(-pad.axis[1]);
	if (pad.numAxes > 2) state->Gamepad.sThumbRX = toThumb(pad.axis[2]);
	if (pad.numAxes > 3) state->Gamepad.sThumbRY = toThumb(-pad.axis[3]);

	state->dwPacketNumber = (DWORD)pad.timestamp;
	return ERROR_SUCCESS;
}

DWORD XInputSetState(DWORD index, XINPUT_VIBRATION* vibration)
{
	if (!vibration)
		return ERROR_SUCCESS;
	EM_ASM({
		try
		{
			if (typeof navigator === 'undefined' || !navigator.getGamepads) return;
			var p = navigator.getGamepads()[$0];
			if (p && p.vibrationActuator)
				p.vibrationActuator.playEffect('dual-rumble', { duration: 150, strongMagnitude: $1, weakMagnitude: $2 });
		} catch (e) {}
	}, (int)index, vibration->wLeftMotorSpeed / 65535.0, vibration->wRightMotorSpeed / 65535.0);
	return ERROR_SUCCESS;
}

static EM_BOOL WebOnMouse(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
	if (!g_input)
		return EM_FALSE;
	WebUpdateMousePos(e->targetX, e->targetY);
	switch (eventType)
	{
	case EMSCRIPTEN_EVENT_MOUSEDOWN:
		g_input->OnMouseMessage(e->button == 2 ? WM_RBUTTONDOWN : e->button == 1 ? WM_MBUTTONDOWN : WM_LBUTTONDOWN, 0);
		if (e->button == 0)
			FrankWebGetMouseLeftDown() = true;	// gui click tracking
		// returning EM_TRUE preventDefaults the click, which suppresses the browser's
		// native focus-on-click - inside an iframe (itch.io) keyboard focus then stays
		// with the parent page and the game never sees another key. refocus explicitly.
		EM_ASM({
			if (typeof document !== 'undefined')
			{
				var c = document.getElementById('canvas');
				if (c && c.focus) c.focus();
			}
		});
		break;
	case EMSCRIPTEN_EVENT_MOUSEUP:
		g_input->OnMouseMessage(e->button == 2 ? WM_RBUTTONUP : e->button == 1 ? WM_MBUTTONUP : WM_LBUTTONUP, 0);
		if (e->button == 0)
			FrankWebGetMouseLeftDown() = false;
		break;
	}
	return EM_TRUE;
}

static EM_BOOL WebOnVisibilityChange(int eventType, const EmscriptenVisibilityChangeEvent* e, void* userData)
{
	// rAF stops while the tab is hidden, so the frame loop can't apply this -
	// freeze/unfreeze all sounds synchronously from the event (same path as pause)
	SoundControl::appHasFocus = !e->hidden;
	if (g_sound)
		g_sound->UpdateTimeScale(e->hidden ? 0.0f : 1.0f);
	return EM_TRUE;
}

static EM_BOOL WebOnWheel(int eventType, const EmscriptenWheelEvent* e, void* userData)
{
	if (!g_input)
		return EM_FALSE;
	// deltaY in browser is positive when scrolling down; win32 wheel is the opposite
	const short wheel = (short)(-e->deltaY);
	g_input->OnMouseMessage(WM_MOUSEWHEEL, (WPARAM)((DWORD)((WORD)wheel) << 16));
	return EM_TRUE;
}

void FrankEngineLoop()
{
	printf("FrankEngineLoop (web) - starting main loop\n");

	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, WebOnKey);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, WebOnKey);
	emscripten_set_mousedown_callback("#canvas", NULL, EM_TRUE, WebOnMouse);
	emscripten_set_mouseup_callback("#canvas", NULL, EM_TRUE, WebOnMouse);
	emscripten_set_mousemove_callback("#canvas", NULL, EM_TRUE, WebOnMouse);
	emscripten_set_wheel_callback("#canvas", NULL, EM_TRUE, WebOnWheel);
	emscripten_set_visibilitychange_callback(NULL, EM_TRUE, WebOnVisibilityChange);

	webLastTime = emscripten_get_now() / 1000.0;
	emscripten_set_main_loop(FrankEngineWebFrame, 0, 1);
}

//--------------------------------------------------------------------------------------
void FrankEngineShutdown()
{
	GetDebugConsole().Save();

	if (g_gameControlBase)
	{
		g_gameControlBase->Shutdown();
		g_gameControlBase->DestroyDeviceObjects();
		delete g_gameControlBase;
		g_gameControlBase = NULL;
	}

	SAFE_DELETE(g_guiBase);
	SAFE_DELETE(g_sound);
	SAFE_DELETE(g_input);
	SAFE_DELETE(g_physics);
}
