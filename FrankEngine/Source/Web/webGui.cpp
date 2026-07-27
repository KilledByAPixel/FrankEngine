////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Web GUI
	Copyright 2013 Frank Force - http://www.frankforce.com

	- functional implementations for the DXUT gui subset declared in frankPlatformWeb.h
	  (statics + buttons - everything piroot's menus use)
	- CDXUTTextHelper draws through FrankFont, lighting up the console, the
	  debug-message panel, and the F1 info overlay
	- WindowControl maps onto browser fullscreen; ShellExecuteEx opens a browser tab
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "../frankEngine.h"

#ifndef FRANK_PLATFORM_WEB
#error webGui.cpp is web-build only
#endif

#include <emscripten.h>
#include <emscripten/html5.h>
#include <cstdarg>
#include <algorithm>

// dxut resource manager referenced by game gui files (state lives per-dialog here)
CDXUTDialogResourceManager g_dialogResourceManager;

static FrankFont* WebGuiFont()
{
	return g_gameControlBase ? g_gameControlBase->GetGameFont(0) : NULL;
}

static Color WebGuiColor(DWORD c)
{
	return Color(FrankColorBase((unsigned long)c));
}

////////////////////////////////////////////////////////////////////////////////////////
// text helper
////////////////////////////////////////////////////////////////////////////////////////

HRESULT CDXUTTextHelper::DrawTextLine(const WCHAR* text, bool advanceUp, bool outline)
{
	FrankFont* font = WebGuiFont();
	const int lineHeight = GetLineHeight();
	if (font && text && text[0])
	{
		const float size = (float)lineHeight;
		if (outline)
			font->RenderScreenSpace(text, XForm2(Vector2((float)posX + 1, (float)posY + 1)), size, Color::Black(WebGuiColor(color).a));
		font->RenderScreenSpace(text, XForm2(Vector2((float)posX, (float)posY)), size, WebGuiColor(color));
	}
	posY += advanceUp ? -lineHeight : lineHeight;
	return 0;
}

HRESULT CDXUTTextHelper::DrawFormattedTextLine(const WCHAR* format, ...)
{
	WCHAR buffer[512];
	va_list args;
	va_start(args, format);
	FrankWebVSWPrintf(buffer, 512, format, args);	// msvc-style %s = wide
	va_end(args);
	buffer[511] = 0;
	return DrawTextLine(buffer);
}

////////////////////////////////////////////////////////////////////////////////////////
// dialog
////////////////////////////////////////////////////////////////////////////////////////

HRESULT CDXUTDialog::AddStatic(int id, const WCHAR* text, int x, int y, int w, int h, bool isDefault, CDXUTStatic** created)
{
	CDXUTStatic* c = new CDXUTStatic();
	c->id = id;
	c->text = text ? text : L"";
	c->x = x; c->y = y; c->width = w; c->height = h;
	c->isDefault = isDefault;
	c->owner = this;
	controls.push_back(c);
	if (created)
		*created = c;
	return 0;
}

HRESULT CDXUTDialog::AddButton(int id, const WCHAR* text, int x, int y, int w, int h, UINT hotkey, bool isDefault, CDXUTButton** created)
{
	CDXUTButton* c = new CDXUTButton();
	c->id = id;
	c->text = text ? text : L"";
	c->x = x; c->y = y; c->width = w; c->height = h;
	c->isDefault = isDefault;
	c->isButton = true;
	c->owner = this;
	controls.push_back(c);
	if (created)
		*created = c;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// keyboard/gamepad focus
//
// GameControlBase::UpdatePost drives menus from the gamepad on every platform: it calls
// OnCycleFocus from GB_MainMenu_Up/Down (dpad, with the engine's ui auto-repeat) and
// spoofs a VK_SPACE key event at CDXUTDialog::GetFocus() for GB_MainMenu_Select. On
// windows dxut supplies all of that; these were empty stubs on web, which is why the
// pad moved nothing in the menus. Focus is a single process-wide pointer, matching
// dxut's own static focus.
////////////////////////////////////////////////////////////////////////////////////////

static CDXUTControl* webGuiFocus = NULL;

// only enabled, visible buttons can take focus - statics are labels
static bool WebGuiCanFocus(const CDXUTControl* c)
{
	return c && c->isButton && c->visible && c->enabled;
}

CDXUTControl* CDXUTDialog::GetFocus()
{
	// a control hidden or disabled since it took focus must not stay focused, or the
	// select button would press an invisible button from a closed menu
	if (!WebGuiCanFocus(webGuiFocus))
		webGuiFocus = NULL;
	return webGuiFocus;
}

void CDXUTDialog::ClearFocus()
{
	webGuiFocus = NULL;
}

void CDXUTDialog::FocusDefaultControl()
{
	CDXUTControl* first = NULL;
	for (CDXUTControl* c : controls)
	{
		if (!WebGuiCanFocus(c))
			continue;
		if (!first)
			first = c;
		if (c->isDefault)
		{
			webGuiFocus = c;
			return;
		}
	}
	webGuiFocus = first;
}

void CDXUTDialog::OnCycleFocus(bool goForward)
{
	// controls are added in menu-bar order, so list order IS left-to-right order
	std::vector<CDXUTControl*> focusable;
	for (CDXUTControl* c : controls)
	{
		if (WebGuiCanFocus(c))
			focusable.push_back(c);
	}
	if (focusable.empty())
		return;

	int index = -1;
	for (size_t i = 0; i < focusable.size(); ++i)
	{
		if (focusable[i] == GetFocus())
			index = (int)i;
	}

	// no focus yet (or focus belongs to another dialog): entering from the top of the
	// list going forward, the bottom going back, so the first press lands somewhere sane
	const int count = (int)focusable.size();
	if (index < 0)
		index = goForward ? 0 : count - 1;
	else
		index = (index + (goForward ? 1 : count - 1)) % count;

	webGuiFocus = focusable[index];
}

// dxut presses on key UP after a key DOWN on the same control, so a press that started
// on one menu cannot fire on whatever replaces it
bool CDXUTControl::HandleKeyboard(UINT msg, WPARAM key, LPARAM)
{
	if (!isButton || !visible || !enabled)
		return false;
	if (key != VK_SPACE && key != VK_RETURN)
		return false;

	if (msg == WM_KEYDOWN)
	{
		keyPressed = true;
		return true;
	}
	if (msg == WM_KEYUP && keyPressed)
	{
		keyPressed = false;
		if (owner && owner->callback)
			owner->callback(EVENT_BUTTON_CLICKED, id, this, owner->callbackContext);
		return true;
	}
	return false;
}

HRESULT CDXUTDialog::OnRender(float delta)
{
	if (!g_render)
		return 0;

	FrankFont* font = WebGuiFont();
	const POINT& cursor = FrankWebGetCursor();
	const bool mouseDown = FrankWebGetMouseLeftDown();
	const bool clicked = mouseDown && !mouseWasDown;
	mouseWasDown = mouseDown;

	for (CDXUTControl* c : controls)
	{
		if (!c->visible || c->text.empty())
			continue;

		const float cx = float(dialogX + c->x), cy = float(dialogY + c->y);
		const float w = (float)c->width, h = (float)c->height;
		float textSize = fontHeights[c->fontIndex & 3] * 0.66f;
		Color textColor = WebGuiColor(c->textColor);

		if (c->isButton)
		{
			const bool hover = c->enabled && cursor.x >= cx && cursor.x < cx + w && cursor.y >= cy && cursor.y < cy + h;
			// the gamepad's current control is lit like a hover, so the pad and the
			// mouse read the same way
			const bool focused = (c == CDXUTDialog::GetFocus());

			Color fill = (hover || focused) ? Color(0.35f, 0.35f, 0.45f, 0.85f) : Color(0, 0, 0, 0.6f);
			Color outline = Color::White((hover || focused) ? 1.0f : 0.6f);
			if (!c->enabled)
			{
				fill = Color(0, 0, 0, 0.3f);
				outline = Color::Grey(0.5f, 0.4f);
				textColor = textColor.ScaleColor(0.5f);
			}
			const Vector2 center(cx + w/2, cy + h/2);
			g_render->RenderScreenSpaceQuad(XForm2(center), Vector2(w/2, h/2), fill, Texture_Invalid, outline);
			if (focused && c->enabled)
			{
				// a second ring just outside the button, so a focused button is still
				// distinguishable from a moused-over one
				g_render->RenderScreenSpaceQuad(XForm2(center), Vector2(w/2 + 3, h/2 + 3),
					Color(0, 0, 0, 0), Texture_Invalid, Color::Yellow(0.9f));
			}

			if (font)
			{
				// shrink long labels to stay inside the button
				textSize = Min(textSize, h * 0.7f);
				char narrow[256];
				wcstombs_s(NULL, narrow, 256, c->text.c_str(), 255);
				const Box2AABB bbox = font->GetLocalBBox(narrow, textSize);
				const float textW = bbox.upperBound.x - bbox.lowerBound.x;
				if (textW > w * 0.92f && textW > 0)
					textSize *= w * 0.92f / textW;
				font->RenderScreenSpace(c->text.c_str(), XForm2(center), textSize, textColor, FontFlag_Center);
			}

			if (hover && clicked && callback)
			{
				// clicking also moves gamepad focus here, so picking up the pad after
				// using the mouse continues from the button that was just used
				webGuiFocus = c;
				callback(EVENT_BUTTON_CLICKED, c->id, c, callbackContext);
			}
		}
		else if (font)
		{
			// static: honor the dwTextFormat alignment relative to the control rect
			const DWORD f = c->element.dwTextFormat;
			FontFlags flags = FontFlag_None;
			float tx = cx, ty = cy;
			if (f & DT_CENTER)		{ flags = flags | FontFlag_CenterX; tx = cx + w/2; }
			else if (f & DT_RIGHT)	{ flags = flags | FontFlag_AlignRight; tx = cx + w; }
			if (f & DT_BOTTOM)		ty = cy + h - textSize;
			font->RenderScreenSpace(c->text.c_str(), XForm2(Vector2(tx, ty)), textSize, textColor, flags);
		}
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////////////
// window control -> browser fullscreen
////////////////////////////////////////////////////////////////////////////////////////

// headless node has no document; the fullscreen apis throw there
static bool WebHasDocument()
{
	static const bool has = EM_ASM_INT({ return (typeof document !== 'undefined') ? 1 : 0; }) != 0;
	return has;
}

// refresh rate measured from rAF cadence - the browser gives no direct query, but
// rAF is vsync'd, so the median frame interval IS the monitor rate. this feeds the
// engine's shared delta smoothing (gameControlBase.cpp), which otherwise assumed 60
// and mis-paced high-refresh monitors.
static float webFrameDeltas[64];
static int webFrameDeltaCount = 0;
static int webFrameDeltaPos = 0;

void FrankWebNoteFrameDelta(float delta)
{
	if (delta <= 0 || delta > 0.25f)
		return;	// tab-switch stalls etc - not cadence
	webFrameDeltas[webFrameDeltaPos] = delta;
	webFrameDeltaPos = (webFrameDeltaPos + 1) % 64;
	if (webFrameDeltaCount < 64)
		++webFrameDeltaCount;
}

int WindowControl::GetRefreshRate()
{
	if (webFrameDeltaCount < 32)
		return 60;	// not enough samples yet - assume the common case

	float sorted[64];
	memcpy(sorted, webFrameDeltas, sizeof(float) * webFrameDeltaCount);
	std::sort(sorted, sorted + webFrameDeltaCount);
	const float median = sorted[webFrameDeltaCount / 2];
	if (median <= 0)
		return 60;

	const float fps = 1.0f / median;
	static const int commonRates[] = { 60, 75, 85, 90, 100, 120, 144, 165, 240 };
	for (const int r : commonRates)
	{
		if (fabsf(fps - r) < r * 0.05f)
			return r;
	}
	return (int)(fps + 0.5f);
}

WindowControl::Mode WindowControl::GetMode()
{
	if (!WebHasDocument())
		return Windowed;
	EmscriptenFullscreenChangeEvent fs;
	if (emscripten_get_fullscreen_status(&fs) == EMSCRIPTEN_RESULT_SUCCESS && fs.isFullscreen)
		return Borderless;
	return Windowed;
}

void WindowControl::CycleMode()
{
	if (!WebHasDocument())
		return;
	if (GetMode() != Windowed)
	{
		emscripten_exit_fullscreen();
		return;
	}

	// keep the 1280x720 backbuffer and let css scale it - the mouse path already
	// converts css coords through emscripten_get_element_css_size
	EmscriptenFullscreenStrategy strategy;
	memset(&strategy, 0, sizeof(strategy));
	strategy.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_ASPECT;
	strategy.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_NONE;
	strategy.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
	emscripten_request_fullscreen_strategy("#canvas", EM_TRUE, &strategy);
}

////////////////////////////////////////////////////////////////////////////////////////
// shell execute -> new browser tab (the steam button)
////////////////////////////////////////////////////////////////////////////////////////

BOOL ShellExecuteEx(SHELLEXECUTEINFOW* info)
{
	if (!info || !info->lpFile)
		return FALSE;

	char url[512];
	wcstombs(url, info->lpFile, sizeof(url) - 1);
	url[sizeof(url) - 1] = 0;
	EM_ASM({ window.open(UTF8ToString($0), '_blank'); }, url);
	return TRUE;
}
