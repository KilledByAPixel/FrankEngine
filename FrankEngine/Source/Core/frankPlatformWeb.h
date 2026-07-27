////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Web Platform Header
	Copyright 2013 Frank Force - http://www.frankforce.com

	- included by frankEngine.h instead of DXUT.h when FRANK_PLATFORM_WEB is defined
	- supplies the win32 typedefs the engine headers rely on, plus FAKE D3D9 handle
	  types (void pointers) so shared headers parse unmodified; the D3D-using
	  translation units are excluded from the web build and stubbed instead
	- never included by the Windows build
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef FRANK_PLATFORM_WEB
#error frankPlatformWeb.h is web-build only
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <cstdarg>
#include <cmath>
#include <cassert>
#include <string>

////////////////////////////////////////////////////////////////////////////////////////
// win32 basic types
////////////////////////////////////////////////////////////////////////////////////////

typedef float				FLOAT;
typedef double				DOUBLE;
typedef int					INT;
typedef unsigned int		UINT;
typedef long				LONG;
typedef unsigned long		ULONG;
typedef unsigned long		DWORD;
typedef unsigned short		WORD;
typedef unsigned char		BYTE;
typedef int					BOOL;
typedef char				CHAR;
typedef wchar_t				WCHAR;
typedef const wchar_t*		LPCWSTR;
typedef wchar_t*			LPWSTR;
typedef const char*			LPCSTR;
typedef char*				LPSTR;
typedef void*				HANDLE;
typedef void*				LPVOID;
typedef void*				HWND;
typedef void*				HINSTANCE;
typedef void*				HMONITOR;
typedef long				HRESULT;
typedef unsigned char		UINT8;
typedef unsigned short		UINT16;
typedef unsigned int		UINT32;
typedef unsigned long long	UINT64;
typedef char				INT8;
typedef short				INT16;
typedef int					INT32;
typedef long long			INT64;
typedef size_t				SIZE_T;
typedef size_t				UINT_PTR;
typedef ptrdiff_t			INT_PTR;
typedef UINT_PTR			WPARAM;
typedef INT_PTR				LPARAM;
typedef INT_PTR				LRESULT;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#ifndef S_OK
#define S_OK ((HRESULT)0)
#define E_FAIL ((HRESULT)0x80004005L)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

struct RECT { LONG left; LONG top; LONG right; LONG bottom; };
struct POINT { LONG x; LONG y; };
struct FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; };

////////////////////////////////////////////////////////////////////////////////////////
// MSVC secure-CRT shims (emscripten/musl has no *_s functions)
////////////////////////////////////////////////////////////////////////////////////////

// msvc's wide printf family treats %s/%c as WIDE args; musl follows c99 where they
// are narrow (%ls/%lc are wide). the engine's format strings follow msvc, so wide
// formatting goes through a rewriter or wide args print as a single character.
inline void FrankWebFixWideFormat(const wchar_t* fmt, wchar_t* out, size_t outSize)
{
	size_t o = 0, i = 0;
	while (fmt[i] && o + 4 < outSize)
	{
		if (fmt[i] != L'%') { out[o++] = fmt[i++]; continue; }
		out[o++] = fmt[i++];	// '%'
		while (fmt[i] && wcschr(L"-+ #.*0123456789", fmt[i]) && o + 4 < outSize)
			out[o++] = fmt[i++];	// flags/width/precision
		if (fmt[i] == L's' || fmt[i] == L'c')			{ out[o++] = L'l'; out[o++] = fmt[i++]; }
		else if (fmt[i] == L'S')						{ out[o++] = L's'; ++i; }
		else if (fmt[i] == L'h' && fmt[i+1] == L's')	{ out[o++] = L's'; i += 2; }
		else if (fmt[i])								{ out[o++] = fmt[i++]; }	// %%, %d, %f, %ls...
	}
	out[o] = 0;
}
inline int FrankWebVSWPrintf(wchar_t* dst, size_t n, const wchar_t* fmt, va_list args)
{
	wchar_t fixed[1024];
	FrankWebFixWideFormat(fmt, fixed, 1024);
	return vswprintf(dst, n, fixed, args);
}
inline int FrankWebSWPrintf(wchar_t* dst, size_t n, const wchar_t* fmt, ...)
{
	va_list a; va_start(a, fmt);
	const int r = FrankWebVSWPrintf(dst, n, fmt, a);
	va_end(a);
	return r;
}

#define sprintf_s(buf, size, ...)			snprintf(buf, size, __VA_ARGS__)
#define swprintf_s(buf, size, ...)			FrankWebSWPrintf(buf, size, __VA_ARGS__)
#define vsprintf_s(buf, size, fmt, args)	vsnprintf(buf, size, fmt, args)
#define vswprintf_s(buf, size, fmt, args)	FrankWebVSWPrintf(buf, size, fmt, args)
#define sscanf_s							sscanf
#define swscanf_s							swscanf
#define _snprintf							snprintf
#define _snwprintf							FrankWebSWPrintf
#define _vsnwprintf							FrankWebVSWPrintf

inline int strcpy_s(char* dst, size_t size, const char* src)			{ if (!dst || !src || !size) return 1; strncpy(dst, src, size - 1); dst[size - 1] = 0; return 0; }
inline int strncpy_s(char* dst, size_t size, const char* src, size_t n)	{ if (!dst || !src || !size) return 1; size_t c = n < size - 1 ? n : size - 1; strncpy(dst, src, c); dst[c] = 0; return 0; }
inline int strcat_s(char* dst, size_t size, const char* src)			{ if (!dst || !src || !size) return 1; strncat(dst, src, size - strlen(dst) - 1); return 0; }
inline int wcscpy_s(wchar_t* dst, size_t size, const wchar_t* src)		{ if (!dst || !src || !size) return 1; wcsncpy(dst, src, size - 1); dst[size - 1] = 0; return 0; }
inline int wcsncpy_s(wchar_t* dst, size_t size, const wchar_t* src, size_t n) { if (!dst || !src || !size) return 1; size_t c = n < size - 1 ? n : size - 1; wcsncpy(dst, src, c); dst[c] = 0; return 0; }
inline int wcscat_s(wchar_t* dst, size_t size, const wchar_t* src)		{ if (!dst || !src || !size) return 1; wcsncat(dst, src, size - wcslen(dst) - 1); return 0; }
template <size_t N> inline int strcpy_s(char (&dst)[N], const char* src)		{ return strcpy_s(dst, N, src); }
template <size_t N> inline int strncpy_s(char (&dst)[N], const char* src, size_t count)	{ return strncpy_s(dst, N, src, count); }
template <size_t N> inline int wcsncpy_s(wchar_t (&dst)[N], const wchar_t* src, size_t count)	{ return wcsncpy_s(dst, N, src, count); }
template <size_t N> inline int strcat_s(char (&dst)[N], const char* src)		{ return strcat_s(dst, N, src); }
template <size_t N> inline int wcscpy_s(wchar_t (&dst)[N], const wchar_t* src)	{ return wcscpy_s(dst, N, src); }
template <size_t N> inline int wcscat_s(wchar_t (&dst)[N], const wchar_t* src)	{ return wcscat_s(dst, N, src); }
#define _wcsicmp wcscasecmp
#define _stricmp strcasecmp
inline int fopen_s(FILE** f, const char* name, const char* mode) { if (!f) return 1; *f = fopen(name, mode); return *f ? 0 : 1; }
inline void* _aligned_malloc(size_t size, size_t alignment) { void* p = NULL; if (posix_memalign(&p, alignment, size)) return NULL; return p; }
inline void _aligned_free(void* p) { free(p); }
inline int wcstombs_s(size_t* pReturn, char* dst, size_t dstSize, const wchar_t* src, size_t count)
{
	// convert per-character, dropping anything non-ascii: emscripten's wcstombs is
	// ascii-only and fails the WHOLE string on one exotic char (e.g. the save-menu
	// heart glyphs), which blanked entire labels when this shim zeroed the output
	if (!dst || !src || !dstSize) return 1;
	const size_t maxCopy = count < dstSize - 1 ? count : dstSize - 1;
	size_t o = 0;
	for (size_t i = 0; src[i] && o < maxCopy; ++i)
	{
		if (src[i] > 0 && src[i] < 128)
			dst[o++] = (char)src[i];
	}
	dst[o] = 0;
	if (pReturn) *pReturn = o + 1;
	return 0;
}
inline int mbstowcs_s(size_t* pReturn, wchar_t* dst, size_t dstSize, const char* src, size_t count)
{
	if (!dst || !src || !dstSize) return 1;
	const size_t maxCopy = count < dstSize - 1 ? count : dstSize - 1;
	const size_t n = mbstowcs(dst, src, maxCopy);
	dst[n == (size_t)-1 ? 0 : n] = 0;
	if (pReturn) *pReturn = (n == (size_t)-1 ? 0 : n + 1);
	return 0;
}
inline HRESULT StringCchVPrintf(wchar_t* dst, size_t n, const wchar_t* fmt, va_list args) { FrankWebVSWPrintf(dst, n, fmt, args); return 0; }
inline HRESULT StringCchVPrintf(char* dst, size_t n, const char* fmt, va_list args) { vsnprintf(dst, n, fmt, args); return 0; }
inline HRESULT StringCchPrintf(wchar_t* dst, size_t n, const wchar_t* fmt, ...) { va_list a; va_start(a, fmt); FrankWebVSWPrintf(dst, n, fmt, a); va_end(a); return 0; }
inline HRESULT StringCchPrintf(char* dst, size_t n, const char* fmt, ...) { va_list a; va_start(a, fmt); vsnprintf(dst, n, fmt, a); va_end(a); return 0; }

// narrow a wide filename for libc++ file streams (no wide-path ctors off Windows);
// used via the FRANK_FILENAME macro in frankEngine.h
inline const char* FrankTempNarrow(const wchar_t* w)
{
	static char buf[512];
	const size_t n = wcstombs(buf, w, 511);
	buf[n == (size_t)-1 ? 0 : n] = 0;
	return buf;
}
inline const char* FrankTempNarrow(const std::wstring& w) { return FrankTempNarrow(w.c_str()); }

// clipboard fakes - OpenClipboard reports failure so callers bail out naturally
typedef void* HGLOBAL;
#define CF_UNICODETEXT 13
#define GHND 0x0042
inline BOOL OpenClipboard(HWND) { return FALSE; }
inline BOOL CloseClipboard() { return TRUE; }
inline BOOL EmptyClipboard() { return TRUE; }
inline HANDLE SetClipboardData(UINT, HANDLE) { return NULL; }
inline HANDLE GetClipboardData(UINT) { return NULL; }
inline HGLOBAL GlobalAlloc(UINT, size_t) { return NULL; }
inline void* GlobalLock(HGLOBAL) { return NULL; }
inline BOOL GlobalUnlock(HGLOBAL) { return TRUE; }
inline HGLOBAL GlobalFree(HGLOBAL) { return NULL; }

// filesystem / resource / misc win32 fakes - all report absence so callers fall back
#define INVALID_HANDLE_VALUE ((HANDLE)(INT_PTR)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10
struct WIN32_FIND_DATA { DWORD dwFileAttributes; FILETIME ftLastWriteTime; WCHAR cFileName[MAX_PATH]; };
inline BOOL CreateDirectory(const WCHAR*, void*) { return FALSE; }
inline HANDLE FindFirstFile(const WCHAR*, WIN32_FIND_DATA*) { return INVALID_HANDLE_VALUE; }
inline BOOL FindNextFile(HANDLE, WIN32_FIND_DATA*) { return FALSE; }
inline BOOL FindClose(HANDLE) { return TRUE; }
inline BOOL DeleteFile(const WCHAR*) { return FALSE; }
typedef void* HRSRC;
typedef void* HMODULE;
#define MAKEINTRESOURCE(i) ((const WCHAR*)(UINT_PTR)(i))
#define RT_RCDATA MAKEINTRESOURCE(10)
inline HMODULE GetModuleHandle(const WCHAR*) { return NULL; }
inline HRSRC FindResource(HMODULE, const WCHAR*, const WCHAR*) { return NULL; }
inline HGLOBAL LoadResource(HMODULE, HRSRC) { return NULL; }
inline void* LockResource(HGLOBAL) { return NULL; }
inline DWORD SizeofResource(HMODULE, HRSRC) { return 0; }
#define UnlockResource(h) ((BOOL)TRUE)
inline BOOL FreeResource(HGLOBAL) { return TRUE; }
inline void Sleep(DWORD) {}
inline HRESULT DXUTSnapD3D9Screenshot(const WCHAR*, BOOL = TRUE) { return -1; }

// XInput types + the browser-backed implementation declared below
#define ERROR_SUCCESS 0L
#define ERROR_DEVICE_NOT_CONNECTED 1167L
#define XUSER_MAX_COUNT 4
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB 0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD 30
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE 7849
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
typedef short SHORT;
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000
struct XINPUT_GAMEPAD { WORD wButtons; BYTE bLeftTrigger; BYTE bRightTrigger; short sThumbLX; short sThumbLY; short sThumbRX; short sThumbRY; };
struct XINPUT_STATE { DWORD dwPacketNumber; XINPUT_GAMEPAD Gamepad; };
struct XINPUT_VIBRATION { WORD wLeftMotorSpeed; WORD wRightMotorSpeed; };
// real implementations over the emscripten/browser Gamepad API (frankEngineWeb.cpp);
// browsers only surface a pad after its first button press - that's normal
DWORD XInputGetState(DWORD index, XINPUT_STATE* state);
DWORD XInputSetState(DWORD index, XINPUT_VIBRATION* vibration);

#define StringCchVPrintfA StringCchVPrintf
#define StringCchPrintfA StringCchPrintf
#define StringCchVPrintfW StringCchVPrintf
#define StringCchPrintfW StringCchPrintf

struct D3DCAPS9 { DWORD AlphaCmpCaps; DWORD MaxPointSize; DWORD Caps; DWORD Caps2; DWORD TextureCaps; };
#define D3DPCMPCAPS_NEVER 0x01
#define D3DPCMPCAPS_LESS 0x02
#define D3DPCMPCAPS_EQUAL 0x04
#define D3DPCMPCAPS_LESSEQUAL 0x08
#define D3DPCMPCAPS_GREATER 0x10
#define D3DPCMPCAPS_NOTEQUAL 0x20
#define D3DPCMPCAPS_GREATEREQUAL 0x40
#define D3DPCMPCAPS_ALWAYS 0x80
inline const D3DCAPS9* DXUTGetD3D9DeviceCaps() { static D3DCAPS9 caps = { 0, 0, 0, 0, 0 }; return &caps; }
#define _wtoi(s) ((int)wcstol(s, NULL, 10))
#define _wtof(s) wcstod(s, NULL)

////////////////////////////////////////////////////////////////////////////////////////
// fake D3D9 / D3DX / DXUT types - handles only; the code that calls through these
// lives in excluded TUs. Enough for shared headers to parse.
////////////////////////////////////////////////////////////////////////////////////////

// captured device state the web renderer consumes: transforms, render states, texture
// stage states, sampler states, bound textures/shader, current render target. The engine
// sets all of it through the fake device exactly like it would through D3D, so the pass
// composition code in deferredRender.cpp runs unmodified (phase 5).
struct FrankWebTexture;
struct FrankWebPixelShader;
struct FrankWebDeviceState
{
	float world[16]; float view[16]; float projection[16];
	float textureTransform[16];					// D3DTS_TEXTURE0 (scrolling/tile uv transform)
	unsigned long renderStates[256];
	unsigned long textureStageStates[4][32];	// COLOROP/ALPHAOP/TEXTURETRANSFORMFLAGS per stage
	unsigned long samplerStates[4][16];			// filter + addressing per stage
	FrankWebTexture* textures[4];				// bound texture per stage (stage 0 drives draws)
	FrankWebTexture* renderTarget;				// NULL = default framebuffer (the canvas)
	FrankWebPixelShader* pixelShader;			// NULL = ubershader

	void ResetPerFrame()
	{
		// mirrors the state block at the top of OnD3D9FrameRender (frankEngine.cpp:467-500)
		renderStates[19] = 5;		// D3DRS_SRCBLEND = SRCALPHA
		renderStates[20] = 6;		// D3DRS_DESTBLEND = INVSRCALPHA
		renderStates[171] = 1;		// D3DRS_BLENDOP = ADD
		renderStates[27] = 1;		// D3DRS_ALPHABLENDENABLE = TRUE
		renderStates[137] = 1;		// D3DRS_LIGHTING = TRUE
		renderStates[139] = 0x00FFFFFF;	// D3DRS_AMBIENT = white
		for (int s = 0; s < 4; ++s)
		{
			textureStageStates[s][1] = (s == 0) ? 4ul : 1ul;	// COLOROP: stage0 MODULATE, others DISABLE
			textureStageStates[s][4] = (s == 0) ? 4ul : 1ul;	// ALPHAOP: stage0 MODULATE, others DISABLE
			textureStageStates[s][24] = 0;						// TEXTURETRANSFORMFLAGS = DISABLE
			samplerStates[s][5] = samplerStates[s][6] = 2;		// MAG/MIN filter = LINEAR
			samplerStates[s][7] = 2;							// MIP filter = LINEAR
			samplerStates[s][1] = samplerStates[s][2] = 1;		// ADDRESSU/V = WRAP
			textures[s] = NULL;
		}
		pixelShader = NULL;
	}

	FrankWebDeviceState()
	{
		for (int i = 0; i < 16; ++i)
			world[i] = view[i] = projection[i] = textureTransform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
		for (int i = 0; i < 256; ++i) renderStates[i] = 0;
		for (int s = 0; s < 4; ++s) for (int i = 0; i < 32; ++i) textureStageStates[s][i] = 0;
		for (int s = 0; s < 4; ++s) for (int i = 0; i < 16; ++i) samplerStates[s][i] = 0;
		renderTarget = NULL;
		ResetPerFrame();
	}
};
inline FrankWebDeviceState& FrankWebGetDeviceState() { static FrankWebDeviceState state; return state; }

// GL-touching device operations live in webRender.cpp (no-ops without a GL context)
HRESULT FrankWebDeviceCreateTexture(UINT width, UINT height, DWORD usage, int format, FrankWebTexture** out);
void FrankWebDeviceSetRenderTarget(FrankWebTexture* surface);
void FrankWebDeviceStretchRect(FrankWebTexture* src, FrankWebTexture* dst);
void FrankWebDeviceClear(DWORD color);

// the fake device: real captures/implementations for what the engine actually uses,
// variadic no-ops for the rest so inline device calls in shared headers compile
struct FrankWebFakeD3DDevice
{
	HRESULT SetRenderState(int state, unsigned long value)
	{
		if (state >= 0 && state < 256)
			FrankWebGetDeviceState().renderStates[state] = value;
		return 0;
	}
	HRESULT GetRenderState(int state, unsigned long* value)
	{
		if (value && state >= 0 && state < 256)
			*value = FrankWebGetDeviceState().renderStates[state];
		return 0;
	}
	HRESULT SetTransform(int state, const void* matrix)
	{
		FrankWebDeviceState& s = FrankWebGetDeviceState();
		float* dst = (state == 256) ? s.world : (state == 2) ? s.view : (state == 3) ? s.projection
			: (state == 16 || state == 17) ? s.textureTransform : NULL;
		if (dst && matrix)
			memcpy(dst, matrix, 16 * sizeof(float));
		return 0;
	}
	template <class M> HRESULT GetTransform(int state, M* matrix)
	{
		FrankWebDeviceState& s = FrankWebGetDeviceState();
		const float* src = (state == 2) ? s.view : (state == 3) ? s.projection : s.world;
		if (matrix)
			memcpy(matrix, src, 16 * sizeof(float));
		return 0;
	}
	HRESULT SetTextureStageState(DWORD stage, int state, unsigned long value)
	{
		if (stage < 4 && state >= 0 && state < 32)
			FrankWebGetDeviceState().textureStageStates[stage][state] = value;
		return 0;
	}
	HRESULT GetTextureStageState(DWORD stage, int state, unsigned long* value)
	{
		if (value && stage < 4 && state >= 0 && state < 32)
			*value = FrankWebGetDeviceState().textureStageStates[stage][state];
		return 0;
	}
	HRESULT SetSamplerState(DWORD stage, int state, unsigned long value)
	{
		if (stage < 4 && state >= 0 && state < 16)
			FrankWebGetDeviceState().samplerStates[stage][state] = value;
		return 0;
	}
	HRESULT SetTexture(DWORD stage, FrankWebTexture* texture)
	{
		if (stage < 4)
			FrankWebGetDeviceState().textures[stage] = texture;
		return 0;
	}
	HRESULT SetPixelShader(FrankWebPixelShader* shader)
	{
		FrankWebGetDeviceState().pixelShader = shader;
		return 0;
	}
	HRESULT CreateTexture(UINT width, UINT height, UINT levels, DWORD usage, int format, int pool, FrankWebTexture** out, void* shared)
	{
		return FrankWebDeviceCreateTexture(width, height, usage, format, out);
	}
	HRESULT SetRenderTarget(DWORD index, FrankWebTexture* surface)
	{
		FrankWebGetDeviceState().renderTarget = surface;
		FrankWebDeviceSetRenderTarget(surface);
		return 0;
	}
	HRESULT GetRenderTarget(DWORD index, FrankWebTexture** out)
	{
		if (out) *out = FrankWebGetDeviceState().renderTarget;
		return 0;
	}
	HRESULT StretchRect(FrankWebTexture* src, const RECT* srcRect, FrankWebTexture* dst, const RECT* dstRect, int filter)
	{
		FrankWebDeviceStretchRect(src, dst);
		return 0;
	}
	HRESULT Clear(DWORD count, const void* rects, DWORD flags, DWORD color, float z, DWORD stencil)
	{
		FrankWebDeviceClear(color);
		return 0;
	}
	template <class... A> HRESULT SetMaterial(A...)				{ return 0; }
	template <class... A> HRESULT SetStreamSource(A...)			{ return 0; }
	template <class... A> HRESULT SetFVF(A...)					{ return 0; }
	template <class... A> HRESULT DrawPrimitive(A...)			{ return 0; }
	template <class... A> HRESULT DrawPrimitiveUP(A...)			{ return 0; }
	template <class... A> HRESULT DrawIndexedPrimitive(A...)	{ return 0; }
	template <class... A> HRESULT CreateVertexBuffer(A...)		{ return -1; }
	template <class... A> HRESULT CreateOffscreenPlainSurface(A...)	{ return -1; }
	template <class... A> HRESULT GetRenderTargetData(A...)		{ return -1; }
	template <class... A> HRESULT SetViewport(A...)				{ return 0; }
	template <class... A> HRESULT GetViewport(A...)				{ return 0; }
	template <class... A> HRESULT SetScissorRect(A...)			{ return 0; }
	template <class... A> HRESULT SetVertexShader(A...)			{ return 0; }
	template <class... A> HRESULT SetIndices(A...)				{ return 0; }
	template <class... A> HRESULT BeginScene(A...)				{ return 0; }
	template <class... A> HRESULT EndScene(A...)				{ return 0; }
	template <class... A> HRESULT LightEnable(A...)				{ return 0; }
	template <class... A> HRESULT SetLight(A...)				{ return 0; }
};
typedef FrankWebFakeD3DDevice IDirect3DDevice9;
typedef FrankWebFakeD3DDevice* LPDIRECT3DDEVICE9;
inline FrankWebFakeD3DDevice* DXUTGetD3D9Device() { static FrankWebFakeD3DDevice device; return &device; }
#define SAFE_RELEASE(p) { if (p) { (p) = NULL; } }
#define SAFE_DESTROY(p) { if ((p) != NULL) { (p)->Destroy(); (p)=NULL; } }
inline int _mkdir(const char* path) { return -1; }	// saves persist via a different path on web
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = NULL; } }
#define SAFE_DELETE_ARRAY(p) { if (p) { delete[] (p); (p) = NULL; } }

typedef void* LPDIRECT3D9;
typedef void* LPDIRECT3DINDEXBUFFER9;
typedef void* LPDIRECT3DVERTEXSHADER9;
typedef void* LPD3DXSPRITE;
typedef void* LPD3DXFONT;
typedef void* LPD3DXBUFFER;
typedef void VOID;

// phase 5: textures are real objects carrying a GL texture (and an FBO when created as a
// render target); "surfaces" are the same object, GetSurfaceLevel returns this. The GL work
// happens in webRender.cpp; headless builds keep gl ids 0 and every operation no-ops.
struct FrankWebTexture
{
	unsigned int glTexture;
	unsigned int glFBO;			// 0 unless created as a render target
	int width;
	int height;
	int format;
	bool hasMipmaps;
	// render target with real RGBA8 storage standing in for a d3d format that has NO
	// alpha (X8R8G8B8). alpha writes are masked off while it is the target and it was
	// cleared to 1, so samples and dst-alpha reads see 1 exactly as d3d would. Emulated
	// RGB8 storage is what drops draws on angle-d3d11 - see WebUseRGB8Targets.
	bool alphaPinned;

	FrankWebTexture() : glTexture(0), glFBO(0), width(0), height(0), format(0), hasMipmaps(false), alphaPinned(false) {}
	HRESULT GetSurfaceLevel(UINT level, FrankWebTexture** surface) { if (surface) *surface = this; return 0; }
	template <class... A> HRESULT LockRect(A...)	{ return -1; }
	template <class... A> HRESULT UnlockRect(A...)	{ return -1; }
	template <class... A> HRESULT GetDesc(A...)		{ return -1; }
};
typedef FrankWebTexture IDirect3DTexture9;
typedef FrankWebTexture* LPDIRECT3DTEXTURE9;
typedef FrankWebTexture* LPDIRECT3DSURFACE9;

// phase 5: vertex buffers are real memory so shared code (deferredRender's light mask,
// FrankFont's glyph strip) can Lock/write/Unlock; the web draw path parses the FVF layout
struct FrankWebVertexBuffer
{
	unsigned char* data;
	unsigned int size;

	FrankWebVertexBuffer(unsigned int _size) : size(_size) { data = (unsigned char*)calloc(1, _size ? _size : 1); }
	~FrankWebVertexBuffer() { free(data); }
	HRESULT Lock(UINT offset, UINT sizeToLock, void** ptr, DWORD flags) { if (ptr) *ptr = data + offset; return 0; }
	HRESULT Unlock() { return 0; }
};
typedef FrankWebVertexBuffer* LPDIRECT3DVERTEXBUFFER9;
#define D3DLOCK_DISCARD  0x00002000L
#define D3DLOCK_READONLY 0x00000010L

// phase 5: pixel "shaders" map to GL programs (only the blur shader has a real one; the
// rest are inert success handles so init logic keeps its windows behavior)
struct FrankWebPixelShader
{
	unsigned int glProgram;		// 0 = inert (draws fall back to the ubershader)
	FrankWebPixelShader() : glProgram(0) {}
};
typedef FrankWebPixelShader* LPDIRECT3DPIXELSHADER9;

// constant tables set uniforms by the same names the HLSL used (impls in webRender.cpp)
void FrankWebSetUniformFloat(unsigned int glProgram, const char* name, float value);
void FrankWebSetUniformVec4(unsigned int glProgram, const char* name, const float* v);
void FrankWebSetUniformMatrix(unsigned int glProgram, const char* name, const float* m);
struct FrankWebConstantTable
{
	unsigned int glProgram;
	FrankWebConstantTable() : glProgram(0) {}
	template <class DEV> HRESULT SetFloat(DEV*, const char* name, float value)			{ FrankWebSetUniformFloat(glProgram, name, value); return 0; }
	template <class DEV, class V> HRESULT SetVector(DEV*, const char* name, const V* v)	{ FrankWebSetUniformVec4(glProgram, name, (const float*)v); return 0; }
	template <class DEV, class M> HRESULT SetMatrix(DEV*, const char* name, const M* m)	{ FrankWebSetUniformMatrix(glProgram, name, (const float*)m); return 0; }
};
typedef FrankWebConstantTable* LPD3DXCONSTANTTABLE;

// D3DX vector stand-ins (deferredRender.cpp uses them textually)
struct D3DXVECTOR3
{
	float x, y, z;
	D3DXVECTOR3() : x(0), y(0), z(0) {}
	D3DXVECTOR3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
};
struct D3DXVECTOR4
{
	float x, y, z, w;
	D3DXVECTOR4() : x(0), y(0), z(0), w(0) {}
	D3DXVECTOR4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
};
inline D3DXVECTOR4* D3DXVec4Normalize(D3DXVECTOR4* out, const D3DXVECTOR4* v)
{
	const float length = sqrtf(v->x*v->x + v->y*v->y + v->z*v->z + v->w*v->w);
	if (length == 0) { *out = D3DXVECTOR4(0, 0, 0, 0); return out; }
	const float i = 1.0f / length;
	*out = D3DXVECTOR4(v->x*i, v->y*i, v->z*i, v->w*i);
	return out;
}

////////////////////////////////////////////////////////////////////////////////////////
// sound fakes (phase 6) - the real soundControl.cpp and musicControl.cpp compile on
// web against these; implementations over OpenAL live in Source/Web/webSound.cpp
////////////////////////////////////////////////////////////////////////////////////////

typedef int GUID;
#define GUID_NULL 0
#define IID_IDirectSound3DBuffer 0
#define DSBVOLUME_MIN (-10000)
#define DSBVOLUME_MAX 0
#define DSBFREQUENCY_MIN 100
#define DSBFREQUENCY_MAX 200000
#define DSBPLAY_LOOPING 0x1
#define DSBLOCK_ENTIREBUFFER 0x2
#define DSBCAPS_CTRL3D 0x10
#define DSBCAPS_CTRLFREQUENCY 0x20
#define DSBCAPS_CTRLPAN 0x40
#define DSBCAPS_CTRLVOLUME 0x80
#define DSSCL_PRIORITY 2
#define DS3DMODE_NORMAL 0
#define DS3DMODE_DISABLE 3
#define DS3D_IMMEDIATE 0
#define DS3D_MINCONEANGLE 0
#define DS3D_MAXCONEANGLE 360
#define DS3D_DEFAULTCONEOUTSIDEVOLUME 0
#define CO_E_NOTINITIALIZED ((HRESULT)0x800401F0L)

inline LONG CompareFileTime(const FILETIME* a, const FILETIME* b)
{
	if (a->dwHighDateTime != b->dwHighDateTime)
		return a->dwHighDateTime < b->dwHighDateTime ? -1 : 1;
	if (a->dwLowDateTime != b->dwLowDateTime)
		return a->dwLowDateTime < b->dwLowDateTime ? -1 : 1;
	return 0;
}

struct WAVEFORMATEX
{
	WORD wFormatTag; WORD nChannels; DWORD nSamplesPerSec; DWORD nAvgBytesPerSec;
	WORD nBlockAlign; WORD wBitsPerSample; WORD cbSize;
};
struct DSBUFFERDESC
{
	DWORD dwSize; DWORD dwFlags; DWORD dwBufferBytes; DWORD dwReserved; WAVEFORMATEX* lpwfxFormat;
};

// D3DVECTOR and the DS 3D param structs - field ORDER matters, soundControl.cpp
// initializes them with aggregate braces
typedef D3DXVECTOR3 D3DVECTOR;
struct DS3DBUFFER
{
	DWORD dwSize; D3DVECTOR vPosition; D3DVECTOR vVelocity;
	DWORD dwInsideConeAngle; DWORD dwOutsideConeAngle; D3DVECTOR vConeOrientation;
	LONG lConeOutsideVolume; float flMinDistance; float flMaxDistance; DWORD dwMode;
};
typedef DS3DBUFFER* LPDS3DBUFFER;
struct DS3DLISTENER
{
	DWORD dwSize; D3DVECTOR vPosition; D3DVECTOR vVelocity;
	D3DVECTOR vOrientFront; D3DVECTOR vOrientTop;
	float flDistanceFactor; float flRolloffFactor; float flDopplerFactor;
};

// one playing sfx instance (an OpenAL source from its CSound's pool).
// QueryInterface/Release exist because soundControl.cpp uses the COM idiom to reach
// the 3D interface - both resolve to this same object; Release here is the COM
// no-op (BufferWrapper::Release is what actually frees the source).
struct FrankWebSoundSource
{
	unsigned int alSource = 0;
	bool inUse = false;
	class CSound* owner = NULL;
	HRESULT QueryInterface(GUID iid, void** out) { if (out) *out = this; return 0; }
	HRESULT SetAllParameters(const DS3DBUFFER* params, DWORD flags);
	void Release() {}
};
typedef FrankWebSoundSource* LPDIRECTSOUND3DBUFFER;

// listener fake (positions/orientation/doppler onto the AL listener)
struct IDirectSound3DListener
{
	HRESULT SetAllParameters(const DS3DLISTENER* params, DWORD flags);
	void Release() {}
};
typedef IDirectSound3DListener* LPDIRECTSOUND3DLISTENER;

// the music streaming buffer fake: musicControl.cpp's half-buffer refill logic runs
// unmodified against a ring whose virtual play cursor advances as AL chunks complete
struct IDirectSoundBuffer
{
	unsigned char* ring = NULL;
	DWORD ringSize = 0;
	DWORD feedPos = 0;			// next ring byte to hand to AL
	DWORD consumed = 0;			// total bytes AL has finished playing
	unsigned int alSource = 0;
	unsigned int alBuffers[16] = { 0 };	// webSound.cpp queues webMusicQueueDepth of these
	int channels = 2;
	int sampleRate = 44100;
	bool playing = false;
	bool primed = false;

	HRESULT Lock(DWORD offset, DWORD size, void** ptr1, DWORD* size1, void** ptr2, DWORD* size2, DWORD flags);
	HRESULT Unlock(void* ptr1, DWORD size1, void* ptr2, DWORD size2);
	HRESULT Play(DWORD reserved, DWORD priority, DWORD flags);
	HRESULT Stop();
	HRESULT SetVolume(LONG centibels);
	HRESULT SetFrequency(DWORD hz);
	HRESULT GetFormat(WAVEFORMATEX* format, DWORD size, DWORD* written);
	HRESULT GetCurrentPosition(DWORD* playPos, DWORD* writePos);
	void Release() {}
};
typedef IDirectSoundBuffer* LPDIRECTSOUNDBUFFER;

struct IDirectSound8
{
	HRESULT CreateSoundBuffer(const DSBUFFERDESC* desc, IDirectSoundBuffer** out, void* unused);
};
typedef IDirectSound8* LPDIRECTSOUND8;

// Frank's modified SDKsound API (DXUT/Optional/SDKsound.h), reimplemented over OpenAL
class CSound
{
public:
	struct BufferWrapper
	{
		FrankWebSoundSource* buffer = NULL;
		float volumePercent = 1;
		float frequencyScale = 1;
		CSound* sound = NULL;

		void Stop();
		void Release();
		bool IsPlaying() const;
		void UpdateTimeScale(float scale);
		void SetVolume(float volume);
		void SetFrequency(float frequency);
		void Restart();
	};

	unsigned int alBuffer = 0;
	int sampleRate = 44100;
	DWORD creationFlags = 0;
	DWORD sourceCount = 0;
	FrankWebSoundSource* sources = NULL;

	~CSound();
	BOOL Is3D() const { return (creationFlags & DSBCAPS_CTRL3D) != 0; }
	HRESULT Play(DWORD dwPriority = 0, DWORD dwFlags = 0, float volumePercent = 1.0f, float frequencyScale = 1.0f, LONG lPan = 0, BufferWrapper* buffer = NULL, float frequencyTimeScale = 1);
	HRESULT Play3D(LPDS3DBUFFER p3DBuffer, DWORD dwPriority = 0, DWORD dwFlags = 0, float volumePercent = 1.0f, float frequencyScale = 0, BufferWrapper* buffer = NULL, float frequencyTimeScale = 1, float volumeScale = 1);

private:
	FrankWebSoundSource* GetFreeSource();
	friend struct BufferWrapper;
};

class CSoundManager
{
public:
	HRESULT Initialize(HWND hWnd, DWORD dwCoopLevel);
	IDirectSound8* GetDirectSound();
	HRESULT Get3DListenerInterface(IDirectSound3DListener** ppDSListener);
	HRESULT Create(CSound** ppSound, LPWSTR strWaveFileName, DWORD dwCreationFlags = 0, GUID guid3DAlgorithm = GUID_NULL, DWORD dwNumBuffers = 1);
};

// D3DXSaveSurfaceToFile is debug-only on windows (shadowSaveTextures); on web it
// dumps the surface as ascii art to the console (glReadPixels), which is how the
// deferred passes get inspected in a headless browser
void FrankWebDumpSurface(const wchar_t* name, FrankWebTexture* surface);
typedef int D3DXIMAGE_FILEFORMAT;
enum { D3DXIFF_BMP = 0, D3DXIFF_JPG = 1, D3DXIFF_PNG = 3 };
inline HRESULT D3DXSaveSurfaceToFile(const WCHAR* name, int format, FrankWebTexture* surface, void*, void*)
{
	FrankWebDumpSurface(name, surface);
	return 0;
}

typedef DWORD D3DCOLOR;
#define D3DCOLOR_ARGB(a,r,g,b) ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#define D3DCOLOR_RGBA(r,g,b,a) D3DCOLOR_ARGB(a,r,g,b)

// enums that appear in shared signatures/members; values irrelevant for stubs
typedef int D3DFORMAT;
typedef int D3DPRIMITIVETYPE;
typedef int D3DTRANSFORMSTATETYPE;
typedef int D3DRENDERSTATETYPE;
typedef int D3DPOOL;
enum { D3DFMT_UNKNOWN = 0, D3DFMT_A8R8G8B8 = 21 };
enum { D3DPT_POINTLIST = 1, D3DPT_LINELIST = 2, D3DPT_LINESTRIP = 3, D3DPT_TRIANGLELIST = 4, D3DPT_TRIANGLESTRIP = 5, D3DPT_TRIANGLEFAN = 6 };
enum { D3DPOOL_DEFAULT = 0, D3DPOOL_MANAGED = 1, D3DPOOL_SYSTEMMEM = 2 };
enum { D3DTS_VIEW = 2, D3DTS_PROJECTION = 3, D3DTS_TEXTURE0 = 16, D3DTS_TEXTURE1 = 17, D3DTS_WORLD = 256 };
enum { D3DFMT_X8R8G8B8 = 22, D3DFMT_R5G6B5 = 23, D3DFMT_A1R5G5B5 = 25, D3DFMT_A16B16G16R16F = 113 };
enum { D3DTTFF_DISABLE = 0, D3DTTFF_COUNT2 = 2 };
enum
{
	// render states (values match d3d9 for reference; the fake device ignores them)
	D3DRS_ZENABLE = 7, D3DRS_FILLMODE = 8, D3DRS_ZWRITEENABLE = 14, D3DRS_ALPHATESTENABLE = 15,
	D3DRS_SRCBLEND = 19, D3DRS_DESTBLEND = 20, D3DRS_CULLMODE = 22, D3DRS_ALPHAREF = 24,
	D3DRS_ALPHAFUNC = 25, D3DRS_DITHERENABLE = 26, D3DRS_ALPHABLENDENABLE = 27, D3DRS_FOGENABLE = 28,
	D3DRS_SPECULARENABLE = 29, D3DRS_STENCILENABLE = 52,
	D3DRS_TEXTUREFACTOR = 60, D3DRS_LIGHTING = 137, D3DRS_AMBIENT = 139, D3DRS_COLORWRITEENABLE = 168,
	D3DRS_BLENDOP = 171, D3DRS_SCISSORTESTENABLE = 174
};
enum
{
	D3DTSS_COLOROP = 1, D3DTSS_COLORARG1 = 2, D3DTSS_COLORARG2 = 3, D3DTSS_ALPHAOP = 4,
	D3DTSS_ALPHAARG1 = 5, D3DTSS_ALPHAARG2 = 6, D3DTSS_TEXCOORDINDEX = 11, D3DTSS_TEXTURETRANSFORMFLAGS = 24
};
enum
{
	D3DTOP_DISABLE = 1, D3DTOP_SELECTARG1 = 2, D3DTOP_SELECTARG2 = 3, D3DTOP_MODULATE = 4,
	D3DTOP_MODULATE2X = 5, D3DTOP_MODULATE4X = 6, D3DTOP_ADD = 7, D3DTOP_SUBTRACT = 10
};
enum { D3DTA_DIFFUSE = 0, D3DTA_CURRENT = 1, D3DTA_TEXTURE = 2, D3DTA_TFACTOR = 3 };
enum
{
	D3DBLEND_ZERO = 1, D3DBLEND_ONE = 2, D3DBLEND_SRCCOLOR = 3, D3DBLEND_INVSRCCOLOR = 4,
	D3DBLEND_SRCALPHA = 5, D3DBLEND_INVSRCALPHA = 6, D3DBLEND_DESTALPHA = 7, D3DBLEND_INVDESTALPHA = 8,
	D3DBLEND_DESTCOLOR = 9, D3DBLEND_INVDESTCOLOR = 10
};
enum { D3DBLENDOP_ADD = 1, D3DBLENDOP_SUBTRACT = 2, D3DBLENDOP_REVSUBTRACT = 3, D3DBLENDOP_MIN = 4, D3DBLENDOP_MAX = 5 };
enum { D3DSAMP_ADDRESSU = 1, D3DSAMP_ADDRESSV = 2, D3DSAMP_ADDRESSW = 3, D3DSAMP_BORDERCOLOR = 4, D3DSAMP_MAGFILTER = 5, D3DSAMP_MINFILTER = 6, D3DSAMP_MIPFILTER = 7 };
enum { D3DTEXF_NONE = 0, D3DTEXF_POINT = 1, D3DTEXF_LINEAR = 2, D3DTEXF_ANISOTROPIC = 3 };
enum { D3DTADDRESS_WRAP = 1, D3DTADDRESS_MIRROR = 2, D3DTADDRESS_CLAMP = 3, D3DTADDRESS_BORDER = 4 };
enum { D3DCULL_NONE = 1, D3DCULL_CW = 2, D3DCULL_CCW = 3 };
enum { D3DFILL_POINT = 1, D3DFILL_WIREFRAME = 2, D3DFILL_SOLID = 3 };
enum { D3DCMP_NEVER = 1, D3DCMP_LESS = 2, D3DCMP_EQUAL = 3, D3DCMP_LESSEQUAL = 4, D3DCMP_GREATER = 5, D3DCMP_NOTEQUAL = 6, D3DCMP_GREATEREQUAL = 7, D3DCMP_ALWAYS = 8 };

struct D3DCOLORVALUE { float r, g, b, a; };
struct D3DMATERIAL9 { D3DCOLORVALUE Diffuse; D3DCOLORVALUE Ambient; D3DCOLORVALUE Specular; D3DCOLORVALUE Emissive; float Power; };
#define D3DCLEAR_TARGET  0x00000001L
#define D3DCLEAR_ZBUFFER 0x00000002L
#define D3DCLEAR_STENCIL 0x00000004L
#define D3DUSAGE_RENDERTARGET 0x00000001L
#define D3DUSAGE_WRITEONLY 0x00000008L
#define D3DUSAGE_DYNAMIC   0x00000200L
#define D3DFVF_XYZ      0x002
#define D3DFVF_XYZRHW   0x004
#define D3DFVF_NORMAL   0x010
#define D3DFVF_DIFFUSE  0x040
#define D3DFVF_TEX1     0x100

struct D3DVIEWPORT9 { DWORD X, Y, Width, Height; float MinZ, MaxZ; };
struct D3DSURFACE_DESC { D3DFORMAT Format; UINT Width; UINT Height; };
struct D3DPRESENT_PARAMETERS_WEB { int unused; };

// win32 virtual key codes (values match winuser.h; input handling is inert on web until phase 4+)
enum
{
	VK_BACK = 0x08, VK_TAB = 0x09, VK_RETURN = 0x0D, VK_SHIFT = 0x10, VK_CONTROL = 0x11,
	VK_MENU = 0x12, VK_PAUSE = 0x13, VK_CAPITAL = 0x14, VK_ESCAPE = 0x1B, VK_SPACE = 0x20,
	VK_PRIOR = 0x21, VK_NEXT = 0x22, VK_END = 0x23, VK_HOME = 0x24,
	VK_LEFT = 0x25, VK_UP = 0x26, VK_RIGHT = 0x27, VK_DOWN = 0x28,
	VK_SNAPSHOT = 0x2C, VK_INSERT = 0x2D, VK_DELETE = 0x2E,
	VK_LBUTTON = 0x01, VK_RBUTTON = 0x02, VK_MBUTTON = 0x04,
	VK_F1 = 0x70, VK_F2 = 0x71, VK_F3 = 0x72, VK_F4 = 0x73, VK_F5 = 0x74, VK_F6 = 0x75,
	VK_F7 = 0x76, VK_F8 = 0x77, VK_F9 = 0x78, VK_F10 = 0x79, VK_F11 = 0x7A, VK_F12 = 0x7B,
	VK_NUMPAD0 = 0x60, VK_NUMPAD1 = 0x61, VK_NUMPAD2 = 0x62, VK_NUMPAD3 = 0x63, VK_NUMPAD4 = 0x64,
	VK_NUMPAD5 = 0x65, VK_NUMPAD6 = 0x66, VK_NUMPAD7 = 0x67, VK_NUMPAD8 = 0x68, VK_NUMPAD9 = 0x69,
	VK_OEM_3 = 0xC0, VK_OEM_PLUS = 0xBB, VK_OEM_MINUS = 0xBD, VK_OEM_COMMA = 0xBC, VK_OEM_PERIOD = 0xBE,
	VK_MULTIPLY = 0x6A, VK_ADD = 0x6B, VK_SUBTRACT = 0x6D, VK_DECIMAL = 0x6E, VK_DIVIDE = 0x6F,
	VK_OEM_1 = 0xBA, VK_OEM_2 = 0xBF, VK_OEM_4 = 0xDB, VK_OEM_5 = 0xDC, VK_OEM_6 = 0xDD, VK_OEM_7 = 0xDE
};
#define _TRUNCATE ((size_t)-1)
#define GMEM_MOVEABLE 0x0002
#define CALLBACK
#define WINAPI
#define CopyMemory(dst, src, len) memcpy(dst, src, len)
#define ZeroMemory(dst, len) memset(dst, 0, len)

// win32 / DXUT function stand-ins used by shared TUs; inert until the web shell owns them
inline HWND DXUTGetHWND() { return NULL; }
inline HWND GetFocus() { return NULL; }
inline short GetKeyState(int) { return 0; }
inline short GetAsyncKeyState(int) { return 0; }
inline int ShowCursor(BOOL) { return 0; }
// the engine polls the cursor each frame; the web shell pushes browser mouse
// positions into this fake cursor so the polling path works unchanged
void FrankWebNoteFrameDelta(float delta);	// raw rAF delta -> refresh-rate estimator (webGui.cpp)
inline POINT& FrankWebGetCursor() { static POINT cursor = { 0, 0 }; return cursor; }
// left mouse button state pushed by the web shell; read by the web gui for clicks
inline bool& FrankWebGetMouseLeftDown() { static bool down = false; return down; }
inline BOOL GetCursorPos(POINT* p) { if (p) *p = FrankWebGetCursor(); return TRUE; }
inline BOOL SetCursorPos(int x, int y) { FrankWebGetCursor().x = x; FrankWebGetCursor().y = y; return TRUE; }
inline DWORD timeGetTime() { return 0; }
inline double DXUTGetTime() { return 0; }
inline float DXUTGetElapsedTime() { return 0; }
inline BOOL DXUTIsWindowed() { return TRUE; }
inline void DXUTPause(bool, bool) {}
inline float DXUTGetFPS() { return 60.0f; }

inline void DXUTSetHotkeyHandling(bool, bool, bool) {}
inline BOOL DXUTIsVsyncEnabled() { return TRUE; }
inline void DXUTToggleFullScreen() {}
inline int DXUTGetRefreshRate() { return 60; }
inline void DXUTShutdown(int = 0) {}
inline void DXUTSetCursorSettings(bool, bool) {}

// WindowControl (Core/windowMode.h) is excluded on web; maps onto browser
// fullscreen (webGui.cpp) so the game's Full Screen button works
class WindowControl
{
public:
	enum Mode { Windowed = 0, Borderless = 1, Fullscreen = 2 };
	static Mode GetMode();
	static void SetMode(Mode) {}
	static void Toggle() {}
	static void CycleMode();
	static bool IsCoveringScreen() { return GetMode() != Windowed; }
	static int GetRefreshRate();	// measured from rAF cadence (webGui.cpp) - feeds delta smoothing
	static Mode GetFullscreenMode() { return Borderless; }
	// false where the browser has no fullscreen api for elements - ios safari on
	// iphone, and ipad depending on version. games should hide their fullscreen
	// button when this is false rather than offer a control that cannot work
	static bool IsFullscreenAvailable();
	static const WCHAR* GetModeName() { return GetModeName(GetMode()); }
	// the browser has no borderless mode - its fullscreen IS full screen, so the web
	// build only ever shows these two (windowMode.cpp also names "Borderless Mode")
	static const WCHAR* GetModeName(Mode m) { return m == Windowed ? L"Window Mode" : L"Full Screen Mode"; }
	static void Update() {}
	static void Init() {}
	static void Save() {}
	static void Load() {}
};

// Editor surface (Editor/*.h excluded on web); enough of a stand-in that
// gameControlBase.cpp compiles unmodified - the web build never enters edit mode
class FakeSubEditor
{
public:
	template <class... A> void Update(A...) {}
	template <class... A> void Render(A...) {}
	bool HasSelection() { return false; }
	template <class... A> void ClearSelection(A...) {}
	int GetLayer() { return 0; }
	template <class... A> void SetLayer(A...) {}
	int GetNewStubType() { return 0; }
	int GetDisplayNewStubType() { return 0; }
	int GetDrawSurfaceIndex() { return 0; }
	template <class... A> void Resurface(A...) {}
	template <class... A> bool IsSelected(A...) { return false; }
};
class Editor
{
public:
	template <class... A> void Update(A...) {}
	template <class... A> void Render(A...) {}
	template <class... A> void Init(A...) {}
	template <class... A> void Reset(A...) {}
	template <class... A> void ResetEditor(A...) {}
	template <class... A> void Shutdown(A...) {}
	template <class... A> void SaveState(A...) {}
	template <class... A> void ClearSelection(A...) {}
	template <class... A> void RemoveFromSelection(A...) {}
	bool HasSelection() { return false; }
	bool IsRectangleSelecting() { return false; }
	bool GetIsInQuickPickMode() { return false; }
	bool IsTileEdit() { return false; }
	bool IsObjectEdit() { return false; }
	FakeSubEditor& GetTileEditor() { static FakeSubEditor e; return e; }
	FakeSubEditor& GetObjectEditor() { static FakeSubEditor e; return e; }
};
extern Editor g_editor;

// win32 window messages referenced by shared message-handling code (inert on web)
enum
{
	WM_KEYDOWN = 0x0100, WM_KEYUP = 0x0101, WM_CHAR = 0x0102,
	WM_SYSKEYDOWN = 0x0104, WM_SYSKEYUP = 0x0105,
	WM_MOUSEMOVE = 0x0200, WM_LBUTTONDOWN = 0x0201, WM_LBUTTONUP = 0x0202,
	WM_LBUTTONDBLCLK = 0x0203, WM_RBUTTONDOWN = 0x0204, WM_RBUTTONUP = 0x0205,
	WM_RBUTTONDBLCLK = 0x0206, WM_MBUTTONDOWN = 0x0207, WM_MBUTTONUP = 0x0208,
	WM_MBUTTONDBLCLK = 0x0209, WM_MOUSEWHEEL = 0x020A, WM_SIZE = 0x0005,
	WM_CLOSE = 0x0010, WM_ACTIVATEAPP = 0x001C, WM_SETFOCUS = 0x0007, WM_KILLFOCUS = 0x0008
};
#define HIWORD(l) ((WORD)((((UINT_PTR)(l)) >> 16) & 0xffff))
#define LOWORD(l) ((WORD)(((UINT_PTR)(l)) & 0xffff))
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#define WHEEL_DELTA 120
inline BOOL ScreenToClient(HWND, POINT*) { return TRUE; }
inline BOOL ClientToScreen(HWND, POINT*) { return TRUE; }
inline LONG SendMessage(HWND, UINT, WPARAM, LPARAM) { return 0; }	// WM_CLOSE etc: no window to close

// DrawText format flags (gui text alignment) and font weights
enum
{
	DT_TOP = 0x0000, DT_LEFT = 0x0000, DT_CENTER = 0x0001, DT_RIGHT = 0x0002,
	DT_VCENTER = 0x0004, DT_BOTTOM = 0x0008, DT_NOCLIP = 0x0100
};
#define FW_NORMAL 400
#define FW_BOLD 700

// ShellExecuteEx opens urls in a new browser tab (webGui.cpp)
#define SEE_MASK_ASYNCOK 0x00100000
#define SW_SHOWNORMAL 1
struct SHELLEXECUTEINFOW
{
	DWORD cbSize; DWORD fMask; HWND hwnd;
	const WCHAR* lpVerb; const WCHAR* lpFile; const WCHAR* lpParameters; const WCHAR* lpDirectory;
	int nShow; void* hInstApp; void* lpIDList; const WCHAR* lpClass; void* hkeyClass;
	DWORD dwHotKey; void* hIcon; void* hProcess;
};
typedef SHELLEXECUTEINFOW SHELLEXECUTEINFO;
BOOL ShellExecuteEx(SHELLEXECUTEINFOW* info);

#define DXUT_PERFEVENTCOLOR 0
template <class... A> inline void DXUT_BeginPerfEvent(A...) {}
inline void DXUT_EndPerfEvent() {}
class CDXUTPerfEventGenerator
{
public:
	template <class... A> CDXUTPerfEventGenerator(A...) {}
};

////////////////////////////////////////////////////////////////////////////////////////
// functional DXUT gui subset for web (statics + buttons - all piroot's menus use).
// heavy methods live in Web/webGui.cpp; rendering goes through FrankFont + g_render.
////////////////////////////////////////////////////////////////////////////////////////

#include <string>
#include <vector>

struct CDXUTElement { DWORD dwTextFormat = 0; };

class CDXUTDialog;
class CDXUTControl
{
public:
	virtual ~CDXUTControl() {}
	void SetVisible(bool v)					{ visible = v; }
	void SetEnabled(bool e)					{ enabled = e; }
	bool GetEnabled()						{ return enabled; }
	void SetText(const WCHAR* t)			{ text = t ? t : L""; }
	void SetTextColor(DWORD c)				{ textColor = c; }
	void SetFont(int f)						{ fontIndex = f; }
	void SetLocation(int _x, int _y)		{ x = _x; y = _y; }
	void SetSize(int w, int h)				{ width = w; height = h; }
	CDXUTElement* GetElement(int)			{ return &element; }
	// real implementation (webGui.cpp): the engine spoofs a VK_SPACE key event at the
	// focused control to press it - see GameControlBase::UpdatePost
	bool HandleKeyboard(UINT msg, WPARAM key, LPARAM lParam);

	int id = 0;
	int x = 0, y = 0, width = 0, height = 0;
	bool visible = true, enabled = true;
	bool isButton = false, isDefault = false;
	bool keyPressed = false;				// mid press from a spoofed key event
	CDXUTDialog* owner = NULL;				// dialog to raise the click callback on
	int fontIndex = 0;
	DWORD textColor = 0xFFFFFFFF;
	std::wstring text;
	CDXUTElement element;
};
class CDXUTStatic : public CDXUTControl {};
class CDXUTButton : public CDXUTStatic {};

class CDXUTDialogResourceManager { };

typedef void (*PCALLBACKDXUTGUIEVENT)(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);
#define EVENT_BUTTON_CLICKED 0x0101

class CDXUTDialog
{
public:
	~CDXUTDialog() { for (CDXUTControl* c : controls) delete c; }
	void Init(CDXUTDialogResourceManager*) {}
	void SetCallback(PCALLBACKDXUTGUIEVENT cb, void* userContext = NULL) { callback = cb; callbackContext = userContext; }
	void SetFont(int index, const WCHAR*, int height, int) { if (index >= 0 && index < 4) fontHeights[index] = height; }
	void EnableKeyboardInput(bool) {}
	HRESULT AddStatic(int id, const WCHAR* text, int x, int y, int w, int h, bool isDefault = false, CDXUTStatic** created = NULL);
	HRESULT AddButton(int id, const WCHAR* text, int x, int y, int w, int h, UINT hotkey = 0, bool isDefault = false, CDXUTButton** created = NULL);
	CDXUTControl* GetControl(int id) { for (CDXUTControl* c : controls) if (c->id == id) return c; return NULL; }
	CDXUTButton* GetButton(int id) { return (CDXUTButton*)GetControl(id); }
	CDXUTStatic* GetStatic(int id) { return (CDXUTStatic*)GetControl(id); }
	void SetLocation(int x, int y) { dialogX = x; dialogY = y; }
	void SetSize(int w, int h) { dialogW = w; dialogH = h; }
	// gamepad menu navigation: GameControlBase::UpdatePost cycles focus from the
	// GB_MainMenu_* buttons and presses the focused control, exactly as on windows.
	// implemented in webGui.cpp (focus is process-wide, matching dxut)
	void FocusDefaultControl();
	void ClearFocus();
	static CDXUTControl* GetFocus();
	void OnCycleFocus(bool goForward);
	template <class... A> bool MsgProc(A...) { return false; }
	HRESULT OnRender(float delta);
	template <class... A> void SetVisible(A...) {}
	template <class... A> bool GetVisible(A...) { return false; }
	template <class... A> HRESULT AddCheckBox(A...) { return 0; }
	template <class... A> HRESULT AddSlider(A...) { return 0; }
	template <class... A> HRESULT AddComboBox(A...) { return 0; }
	template <class... A> void SetBackgroundColors(A...) {}
	template <class... A> void EnableNonUserEvents(A...) {}
	template <class... A> void RemoveAllControls(A...) {}

	std::vector<CDXUTControl*> controls;
	PCALLBACKDXUTGUIEVENT callback = NULL;
	void* callbackContext = NULL;
	int fontHeights[4] = { 36, 36, 36, 36 };
	int dialogX = 0, dialogY = 0, dialogW = 0, dialogH = 0;
	bool mouseWasDown = false;
};

// editorGui.h is excluded on web; g_editorGui is declared in frankEngine.h
class EditorGui
{
public:
	template <class... A> void Init(A...) {}
	template <class... A> void OnResetDevice(A...) {}
	template <class... A> void OnLostDevice(A...) {}
	template <class... A> HRESULT Render(A...) { return 0; }
	template <class... A> void Refresh(A...) {}
	template <class... A> void Reset(A...) {}
	template <class... A> bool MsgProc(A...) { return false; }
	template <class... A> void Update(A...) {}
	template <class... A> void ClearFocus(A...) {}
	CDXUTDialog& GetDialog() { static CDXUTDialog d; return d; }
	bool IsMouseOverGui() { return false; }
	bool IsEditBoxFocused() { return false; }
};

// functional text helper drawing through FrankFont (Web/webGui.cpp) - the console,
// debug-message panel, and info overlays all print through this
class CDXUTTextHelper
{
public:
	void Begin() {}
	void End() {}
	void SetInsertionPos(int x, int y) { posX = x; posY = y; }
	template <class C> void SetForegroundColor(const C& c) { color = (DWORD)c; }
	HRESULT DrawTextLine(const WCHAR* text, bool advanceUp = false, bool outline = true);
	HRESULT DrawFormattedTextLine(const WCHAR* format, ...);
	int GetLineHeight() { return 18; }
	POINT GetInsertionPos() { POINT p = { posX, posY }; return p; }

	int posX = 0, posY = 0;
	DWORD color = 0xFFFFFFFF;
};

// frankProfiler.h holds a CDXUTTimer by value - minimal stand-in
class CDXUTTimer
{
public:
	void Reset() {}
	void Start() {}
	void Stop() {}
	void Advance() {}
	double GetAbsoluteTime() { return 0; }
	double GetTime() { return 0; }
	double GetElapsedTime() { return 0; }
	bool IsStopped() { return true; }
};
