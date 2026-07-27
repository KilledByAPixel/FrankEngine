////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Web Stubs
	Copyright 2013 Frank Force - http://www.frankforce.com

	- no-op implementations of the windows-only subsystems excluded from the web build
	  (renderer, sound, input, terrain render, minimap, font, debug render)
	- phase 3 scope: enough for the engine to boot and simulate headless; real GL/OpenAL
	  backends replace these in phases 4-6
	- grown iteratively from linker errors; bodies return neutral values and never crash
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "../frankEngine.h"

#ifndef FRANK_PLATFORM_WEB
#error webStubs.cpp is web-build only
#endif

//--------------------------------------------------------------------------------------
// globals normally defined in excluded TUs
// (g_sound is defined in soundControl.cpp, which compiles on web as of phase 6;
//  g_input is defined in gameControlBase.cpp, which compiles on web)
float g_interpolatePercent = 0;
DebugRender g_debugRender;
TerrainRender g_terrainRender;

//--------------------------------------------------------------------------------------
// FrankRender lifecycle lives in webRender.cpp as of phase 5

//--------------------------------------------------------------------------------------
// DebugRender (frankDebugRender.cpp)
void DebugRender::Update(float delta) {}
void DebugRender::Render() {}
void DebugRender::Clear() {}
void DebugRender::InitDeviceObjects() {}
void DebugRender::DestroyDeviceObjects() {}
void DebugRender::RenderPoint(const Vector3& point, const Color& color, float scale, float time) {}
void DebugRender::RenderLine(const Vector3& start, const Vector3& end, const Color& color, float time) {}
void DebugRender::RenderBox(const XForm2& xf, const Vector2& size, const Color& color, float time) {}
void DebugRender::RenderBox(const XForm2& xf, const Box2AABB& box, const Color& color, float time) {}
void DebugRender::RenderBox(const Box2AABB& box, const Color& color, float time) {}
void DebugRender::RenderCircle(const Circle& circle, const Color& color, float time, int sides) {}
void DebugRender::RenderCone(const Cone& cone, const Color& color, float time, int sides) {}
void DebugRender::RenderText(const Vector2& position, const wstring& text, const Color& color, bool center, float time, bool screenSpace) {}
void DebugRender::RenderTextFormatted(const Vector2& position, const Color& color, bool center, float time, wstring messageFormat, ...) {}
void DebugRender::ImmediateRenderTextScreenSpace(const Vector2& position, const WCHAR* text, const Color& color, bool center, CDXUTTextHelper* textHelper) {}

//--------------------------------------------------------------------------------------
// DeferredRender + FrankFont are the REAL deferredRender.cpp / frankFont.cpp as of phase 5

//--------------------------------------------------------------------------------------
// InputControl now compiles for web (inputControl.cpp with XInput dormant)

//--------------------------------------------------------------------------------------
// SoundControl / MusicControl are the REAL soundControl.cpp / musicControl.cpp as of
// phase 6 (over the OpenAL fakes in webSound.cpp)

//--------------------------------------------------------------------------------------
// GuiBase (guiBase.cpp)
GuiBase::GuiBase() {}

//--------------------------------------------------------------------------------------
// TerrainRender / MiniMap (terrainRender.cpp, miniMap.cpp)
DebugRender::DebugRender() {}
DebugRender::~DebugRender() {}
TerrainRender::TerrainRender() {}
TerrainRender::~TerrainRender() {}
// TerrainLayerRender::Render() lives in webRender.cpp as of phase 4
bool TerrainRender::enableDiffuseLighting = true;
float TerrainRender::textureWrapScale = 8;	// matches terrainRender.cpp default
void TerrainRender::Update() {}
// ClearCache / RefereshCached are real on web now - they invalidate the static terrain
// vertex cache in webRender.cpp (the window-based eviction Update() does on windows is
// handled there too, by evicting the patch farthest from the stream center on demand)
void TerrainRender::InitDeviceObjects() {}
void TerrainRender::DestroyDeviceObjects() {}
bool TerrainRender::IsPatchCached(TerrainPatch& patch) { return true; }	// no cache on web; lights must not wait on it

// real ctor logic from terrainRender.cpp:1096 (phase 4 left layer/flags uninitialized)
TerrainLayerRender::TerrainLayerRender(const Vector2& pos, int _layer, int _renderGroup) :
	GameObject(XForm2(pos)),
	layer(_layer),
	lightShadows(false),
	directionalShadows(false),
	emissiveLighting(true)
{
	SetRenderGroup(_renderGroup);
	if (layer == 0)
		lightShadows = true;	// layer 0 casts light shadows by default
	if (layer == 1)
		directionalShadows = true;	// layer 1 casts directional shadows by default
}

// MiniMap: the real Rendering\miniMap.cpp is compiled for web (phase 7)
