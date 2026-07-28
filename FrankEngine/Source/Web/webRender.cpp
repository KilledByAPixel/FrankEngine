////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Web Renderer (WebGL2)
	Copyright 2013 Frank Force - http://www.frankforce.com

	- the real GL backend behind FrankRender's public API on the web build
	- phase 4: ubershader (pos x worldViewProj, uv transform, color), textures via the
	  emscripten preload plugins, tiles via uv-matrix uniform, streamed simple verts
	- phase 5: functional device emulation so the REAL deferredRender.cpp runs unmodified:
	  render-target textures (FBOs), blits, clears, full blend mapping, the material/ambient
	  color rule, texture-stage flags, sampler state, real vertex buffers parsed by FVF
	  (light-mask primitive + FrankFont glyphs), and the ported blur shader
	- geometry conventions match BuildQuad(): unit quad spans +-1, uv (0,0) at top-left
	- matrix convention proven in phase 1.5: row-major upload, transpose=false, M * v
	- FBO flip rule (phase 1.5): projection Y is flipped when rendering into an FBO so
	  render-target contents sample with the same orientation as uploaded images (D3D-like)
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "../frankEngine.h"

#ifndef FRANK_PLATFORM_WEB
#error webRender.cpp is web-build only
#endif

#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <algorithm>

////////////////////////////////////////////////////////////////////////////////////////
// GL context + shaders
////////////////////////////////////////////////////////////////////////////////////////

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webGLContext = 0;
static GLuint webProgram = 0;
static GLuint webBlurProgram = 0;
static GLuint webNormalMapProgram = 0;
static GLuint webLightProgram = 0;
static GLuint webDirLightProgram = 0;
static GLuint webQuadVAO = 0, webQuadVBO = 0;
// streamed vertex data goes into a pool of LARGE append-only buffers: each draw's
// verts are written at an advancing offset with glBufferSubData and drawn with a
// non-zero `first`, so no byte of buffer storage is ever rewritten while the gpu
// could still be reading it, and rotation across buffers keeps reuse several frames
// apart. this is the canonical webgl streaming pattern. it exists because chrome's
// shared gpu process, under contention from ANOTHER chrome instance, intermittently
// dropped draws whose buffer was updated per-draw (proven by the flicker probe:
// identical vert counts + framebuffer, missing pixels; firefox unaffected) - both
// with glBufferData orphaning and with fixed-slot glBufferSubData rewrites.
// routes vertex bytes to gl; see webNarrowUpload below for why this is indirected
static void WebUploadBufferSubData(GLenum target, GLintptr offset, const void* data, int size);

// bytes pushed through the streaming pool this frame. this is the traffic that goes into
// angle's shared d3d11 streaming ring - the path the dropped-draw research implicates -
// so it is the number batching work is really trying to move
static int webPoolBytes = 0;

// forward decl: every barrier that could reorder or restate has to drain the batch
void WebFlushSpriteBatch();
// forward decl: any path that binds a texture outside the draw path must drop the
// sampler object bound to that unit, or the texture's own parameters stay overridden
static void WebClearSamplers();

// artificially shrink the usable span of each pool buffer to force frequent reuse.
// 0 = use the full buffer. small values (128, 256) make the ring wrap several times a
// frame: if buffer recycling is the flicker mechanism, this makes it far WORSE, which
// is the strongest possible confirmation.
ConsoleCommandSimple(int, webPoolLimitKB, 0);

// EXPERIMENT (default OFF - do not enable): GL_STATIC_DRAW for the streaming pools.
// On angle/d3d11 the usage hint selects the whole buffer implementation rather than
// being a hint: STATIC binds our buffer directly and fills it lazily via a staging
// CopySubresourceRegion, DYNAMIC re-streams every draw through angle's shared ring.
// TESTED LIVE AND IT IS BAD: the direct path is far slower for a buffer we sub-update
// ~1300x/frame at advancing offsets, AND it renders stale/missing content (no lighting,
// no text, no background) - angle's VertexArray11 does not set a d3d11 dirty bit for
// bufferSubData on direct-storage attribs, so draws run against un-copied data.
// Kept only as a switch for further investigation. See the note in Upload().
ConsoleCommandSimple(bool, webPoolStaticUsage, false);

// Flicker derby D3, PROMOTED to the default path on apple gpus (2026-07-28): full
// storage respecification per upload instead of sub-updates into a long-lived buffer.
// glBufferData(STATIC, exactSize, data) replaces the buffer's storage every time.
//
// On angle/D3D11 this takes the staging-copy path (CopySubresourceRegion into a buffer
// it direct-binds) - the shared streaming ring is never involved. attribs are re-pointed
// after every respec because VertexArray11 does not raise a d3d dirty bit for new buffer
// data on direct storage (same workaround the terrain cache uses, proven correct there).
//
// On angle/METAL (safari AND chrome on every apple device) the trade INVERTS, and the
// append pool becomes the worst case in angle's buffer code: a glBufferSubData at a
// nonzero offset into a buffer the gpu is using is handled by allocating a NEW
// full-size MTLBuffer and GPU-BLITTING EVERY BYTE NOT OVERWRITTEN
// (BufferMtl::putDataInNewBufferAndStartUsingNewBuffer). With 8MB pool buffers and
// interleaved append->draw->append, every append after a frame's first drags ~8MB of
// hidden copy traffic to deliver a few hundred bytes of verts - at our ~1300
// sub-updates/frame that can eat an apple gpu's entire memory bandwidth, invisibly
// (no gl error, no draw count change; only a Metal trace shows it). Respec instead
// declares the old contents dead: nothing preserved, nothing blitted, and exact-size
// data skips angle's preserve guard entirely. See local/safari-macos26-round2 notes.
//
// -1 = auto: ON for tile-based (apple) gpus, OFF for d3d11-backed browsers, whose
// dropped-draw-under-contention bug is the reason the append pool exists at all.
// 0/1 force it, so ?cvar=webPoolRespec+0 (or +1) stays a one-url A/B on any machine.
ConsoleCommandSimple(int, webPoolRespec, -1);
static bool WebGpuIsTileBased();	// defined with the present/flush apple gating below
static bool WebPoolRespecActive()
{
	return webPoolRespec < 0 ? WebGpuIsTileBased() : webPoolRespec != 0;
}

// GL binding cache helpers (defined below with the rest of the state tracking) - the
// pool binds its vao/buffer through these so redundant binds cost nothing
static inline void WebBindVAO(GLuint v);
static inline void WebBindArrayBuffer(GLuint b);

struct WebChunkPool
{
	// 8 x 8MB: the game pushes ~2MB of verts per frame, so the old 4 x 2MB ring wrapped
	// every ~4 frames - if the gpu is running frames behind (contention), we would
	// overwrite vertex data it had not drawn yet. this depth makes a full wrap take
	// ~30 frames. buffers allocate lazily, so idle games pay nothing.
	static const int bufferCount = 8;
	static const int bufferBytes = 8 * 1024 * 1024;
	GLuint vaos[bufferCount];
	GLuint vbos[bufferCount];
	bool allocated[bufferCount];
	int cur;
	int cursor;		// byte offset of next write in the current buffer, multiple of stride
	int stride;

	// attribute layout for this pool's stride - duplicated from the init loops in
	// WebInitGL so webPoolRespec can re-establish it after a storage respecification
	void SetupAttribs()
	{
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
		if (stride == 24)
		{
			// textured verts: pos3 + uv2 + packed color
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)(5*sizeof(float)));
		}
		else
		{
			// simple verts: pos3 + packed color, uv constant
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void*)12);
			glDisableVertexAttribArray(1);
			glVertexAttrib2f(1, 0, 0);
		}
	}

	// binds the buffer the verts landed in and returns the `first` vertex index
	int Upload(const void* verts, int vertCount)
	{
		const int bytes = vertCount * stride;
		if (WebPoolRespecActive())
		{
			// derby D3: rotate buffers and replace storage outright. every upload gets
			// brand-new STATIC storage, so no draw can ever read a byte the gpu still
			// has in flight, and angle's streaming ring carries nothing for this draw.
			cur = (cur + 1) % bufferCount;
			cursor = 0;
			WebBindVAO(vaos[cur]);
			WebBindArrayBuffer(vbos[cur]);
			glBufferData(GL_ARRAY_BUFFER, bytes, verts, GL_STATIC_DRAW);
			allocated[cur] = false;	// dynamic path must re-allocate if toggled back on
			SetupAttribs();
			webPoolBytes += bytes;
			return 0;
		}
		const int limit = (webPoolLimitKB > 0)
			? Min(bufferBytes, Max(webPoolLimitKB * 1024, bytes)) : bufferBytes;
		if (cursor + bytes > limit)
		{
			cur = (cur + 1) % bufferCount;
			cursor = 0;
		}
		WebBindVAO(vaos[cur]);
		WebBindArrayBuffer(vbos[cur]);
		if (!allocated[cur])
		{
			// one-time storage allocation per buffer; never re-specified after.
			//
			// USAGE HINT IS LOAD-BEARING ON ANGLE/D3D11, not a hint at all:
			//   Buffer11::supportsDirectBinding() { return mUsage == STATIC; }
			// With DYNAMIC_DRAW, angle NEVER binds this buffer to the gpu. Every draw is
			// re-translated into angle's single shared ~1MB streaming ring, which does
			// MAP_WRITE_DISCARD -> rewind to offset 0 -> MAP_WRITE_NO_OVERWRITE writes,
			// with NO in-flight tracking whatsoever (no fence/query/DO_NOT_WAIT anywhere
			// in its buffer path). That is only safe if the driver renames on every
			// discard - and under gpu contention the in-flight window grows, which is
			// exactly when our dropped draws appear.
			// With STATIC_DRAW angle takes the direct path instead: our buffer is bound
			// as a real vertex buffer and filled via a staging copy. Different code path
			// entirely, and the one this pool was designed for (we allocate once and
			// sub-update; we are not re-specifying storage every frame).
			allocated[cur] = true;
			glBufferData(GL_ARRAY_BUFFER, bufferBytes, NULL,
				webPoolStaticUsage ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
		}
		WebUploadBufferSubData(GL_ARRAY_BUFFER, cursor, verts, bytes);
		webPoolBytes += bytes;
		const int first = cursor / stride;
		cursor += bytes;
		return first;
	}
};
static WebChunkPool webStreamPool;	// pos3 + packed color (16 bytes)
static WebChunkPool webTexPool;		// pos3 + uv + packed color (24 bytes)

// EXPERIMENT: how vertex bytes get handed to webgl.
// emscripten's webgl2 glBufferSubData passes the ENTIRE wasm heap as the source view
// (GLctx.bufferSubData(target, offset, HEAPU8, srcOffset, size)) - chrome then reads a
// small window out of a ~300MB view. plain js webgl content passes small arrays, which
// is a different path in chrome. every draw that glitches in this game is one that
// uploads verts; sprites (static quad, no upload) never glitch. if an upload silently
// fails, the untouched destination stays zeroed -> degenerate tris -> invisible draw,
// which is exactly the symptom.
// 0 = emscripten default, 1 = narrow subarray view, 2 = copy into a small staging array
ConsoleCommandSimple(int, webNarrowUpload, 0);

// EXPERIMENT: issue each draw N times back to back. if individual draws are being
// dropped independently, N=3 makes a visible drop ~p^3 (flicker should nearly vanish).
// if the flicker rate is unchanged, the loss is coarser than one draw call - a whole
// batch of commands or the present/composite step. opaque draws are idempotent;
// additive ones (glow, particles) will look brighter while this is on.
ConsoleCommandSimple(int, webDrawRepeat, 1);

// check glGetError after EVERY draw. webgl SKIPS a draw and raises INVALID_OPERATION
// when a texture bound to a sampler is also attached to the current framebuffer (a
// "feedback loop"). d3d9 had no such rule, so the emulated device can leave a stale
// texture bound while we render into that same texture - a skipped draw renders
// nothing, which is exactly the flicker symptom, and backends differ on enforcement.
ConsoleCommandSimple(bool, webCheckErrors, false);
static int webDrawErrorCount = 0;

// candidate FIX for the above: never sample the current render target
ConsoleCommandSimple(bool, webFixFeedback, true);
static int webFeedbackHits = 0;

static void WebCheckDrawError(const char* where, FrankWebTexture* tex0)
{
	if (!webCheckErrors)
		return;
	const GLenum err = glGetError();
	if (err == GL_NO_ERROR || webDrawErrorCount > 60)
		return;
	++webDrawErrorCount;
	GLint fbo = 0, boundTex = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &boundTex);
	printf("DRAWERROR #%d %s: gl error 0x%04x pass=%d fbo=%d boundTex=%d tex0=%u fbo0=%u%s\n",
		webDrawErrorCount, where, (unsigned)err, (int)DeferredRender::GetRenderPass(),
		fbo, boundTex, tex0 ? tex0->glTexture : 0, tex0 ? tex0->glFBO : 0,
		(err == GL_INVALID_OPERATION) ? "  <-- INVALID_OPERATION" : "");
}

// glGetError reports errors accumulated since the LAST check, from ANY call - so an
// error printed after a draw may have been raised elsewhere. draining with a site tag
// at key checkpoints makes each error name its true origin.
static void WebDrainErrors(const char* site)
{
	if (!webCheckErrors)
		return;
	for (int i = 0; i < 8; ++i)
	{
		const GLenum err = glGetError();
		if (err == GL_NO_ERROR)
			break;
		if (webDrawErrorCount > 60)
			continue;
		++webDrawErrorCount;
		GLint fbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
		printf("DRAWERROR #%d accumulated-at[%s]: gl error 0x%04x pass=%d fbo=%d\n",
			webDrawErrorCount, site, (unsigned)err, (int)DeferredRender::GetRenderPass(), fbo);
	}
}

// pass a tight view of just these bytes instead of the whole heap
EM_JS(void, WebJsNarrowSubData, (int target, int offset, const void* data, int size), {
	GLctx.bufferSubData(target, offset, HEAPU8.subarray(data, data + size));
});

// copy out of the wasm heap into a small dedicated array, then upload from that
EM_JS(void, WebJsStagedSubData, (int target, int offset, const void* data, int size), {
	if (!globalThis.__frankStage || globalThis.__frankStage.byteLength < size)
		globalThis.__frankStage = new Uint8Array(size < (1 << 20) ? (1 << 20) : size);
	var stage = globalThis.__frankStage;
	stage.set(HEAPU8.subarray(data, data + size));
	GLctx.bufferSubData(target, offset, stage, 0, size);
});

static void WebUploadBufferSubData(GLenum target, GLintptr offset, const void* data, int size)
{
	if (webNarrowUpload == 1)
		WebJsNarrowSubData((int)target, (int)offset, data, size);
	else if (webNarrowUpload == 2)
		WebJsStagedSubData((int)target, (int)offset, data, size);
	else
		glBufferSubData(target, offset, size, data);
}
static GLuint webWhiteTexture = 0;
// see the workaround block below; declared early because WebBindFBO uses it
extern int webFBOPingPong;
extern int webContextPump;
extern int webScrubTexUnits;
// scratch texture used by webTexturePingPong (declared here because WebInitGL creates it
// long before the workaround's own block); see that cvar for what it is for
static GLuint webPingPongTexture = 0;
static int webPingPongBinds = 0;
static void WebCheckNoMapParam();	// defined with webMapRender below

// current GL_FRAMEBUFFER binding, tracked instead of queried. glGetIntegerv is a
// synchronous round trip to chrome's gpu process; the draw path was issuing two per
// draw (~2600/frame) purely to ask "which fbo am I on". ALL framebuffer binds must go
// through WebBindFBO so this stays truthful.
static GLuint webCurrentFBO = 0;
static void WebContextPump();
static int webFBOPingPongs = 0;
static int webScrubs = 0;

// WORKAROUND 3 - the one aimed at the trigger the bisection actually found.
//
// webStripMask bisection (contention rig, 90s runs): stripping everything except the
// shadow pass gives 0 events; keeping ONLY the final lighting composite gives 103;
// keeping only the emissive pass gives 1; keeping both gives 530. The composite is the
// stage that SAMPLES the shadow map - and the next frame we render INTO that same
// texture. We never unbind it in between, so the texture is still sitting in a texture
// unit when it is bound as a render target.
//
// That is the exact precondition for the confirmed mechanism: d3d11 silently NULLs a
// shader-resource slot when the same resource is bound as a render target, while angle's
// pointer-guarded shadow still believes the texture is bound and skips the re-bind. Any
// later sample of it reads black. Unbinding both texture units before every render
// target change means our render targets are never resident in a sampler slot, so there
// is nothing for d3d11 to null and nothing for angle's shadow to get wrong.
// Costs 2 gl calls per render target change (~20/frame).
int webScrubTexUnits = 0;
ConsoleCommand(webScrubTexUnits, webScrubTexUnits);

static inline void WebBindFBO(GLuint fbo)
{
	// a pending sprite batch belongs to the framebuffer it was accumulated against, so
	// it has to be drawn before the target moves. this is the single most important
	// batch barrier, and putting it here means every render-target change gets it for
	// free (all framebuffer binds already have to route through this function)
	WebFlushSpriteBatch();
	if (webScrubTexUnits)
	{
		// no render target may be resident in a sampler slot when it becomes a render
		// target - see the webScrubTexUnits comment above
		// (the bind cache is declared below this point and is required to stay OFF - see
		// the !! SKIPPING IS DISABLED !! note - so there is nothing here to keep in sync)
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		++webScrubs;
	}
	if (webContextPump == 2)
		WebContextPump();
	if (webFBOPingPong && fbo != webCurrentFBO)
	{
		// same idea as webTexturePingPong but for the render target: bounce through the
		// default framebuffer so angle cannot elide the real OMSetRenderTargets. if the
		// stale state is the RENDER TARGET rather than a texture, our shadow-pass draws
		// are landing somewhere else entirely, which looks exactly like "the shadow map
		// did not get written" while every other pass looks fine
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		++webFBOPingPongs;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	webCurrentFBO = fbo;
}

// GL binding cache. the d3d-style draw path re-binds the program, vao, array buffer and
// both texture units on EVERY draw (~1300 draws/frame => ~9000 redundant calls). these
// wrappers skip the call when the binding is already current. every bind must route
// through them, and WebResetBindCache() must run on context (re)creation.
static GLuint webCurProgram = 0, webCurVAO = 0, webCurArrayBuffer = 0;
// one slot per texture unit the renderer can bind: 0/1 for the ubershader, 2 for the
// light shaders' shadow map. WebBindTextureUnit indexes this by unit even when the cache
// is off, so it must never be smaller than the highest unit in use.
static const int webTextureUnitCount = 3;
static GLuint webCurTexture[webTextureUnitCount] = { 0, 0, 0 };
static int webCurActiveUnit = -1;

// !! SKIPPING IS DISABLED !!  These were caches that skipped redundant binds, but the
// file still contains many raw glBindTexture/glActiveTexture/glUseProgram calls (texture
// creation, sampler state, blit scrubs, grid modes) that change real GL state without
// telling the cache. The cache then believed a binding was already current, skipped it,
// and draws sampled the wrong texture - live symptoms were missing lighting, missing
// font text, missing background terrain and red-tinted regions.
// Re-enabling requires routing EVERY bind in this file through these helpers first.
// webBindCache exists so that work can be A/B'd, but it must stay OFF until then.
ConsoleCommandSimple(bool, webBindCache, false);

static void WebResetBindCache()
{
	webCurProgram = webCurVAO = webCurArrayBuffer = 0;
	for (int unit = 0; unit < webTextureUnitCount; ++unit)
		webCurTexture[unit] = 0;
	webCurActiveUnit = -1;
	// sampler objects and their cache belong to the context that created them
	extern void WebResetSamplerCache();
	WebResetSamplerCache();
}
static inline void WebBindProgram(GLuint p)
{
	if (webBindCache && p == webCurProgram) return;
	glUseProgram(p);
	webCurProgram = p;
}
static inline void WebBindVAO(GLuint v)
{
	if (webBindCache && v == webCurVAO) return;
	glBindVertexArray(v);
	webCurVAO = v;
	webCurArrayBuffer = 0;	// vao carries its own attrib/buffer bindings
}
static inline void WebBindArrayBuffer(GLuint b)
{
	if (webBindCache && b == webCurArrayBuffer) return;
	glBindBuffer(GL_ARRAY_BUFFER, b);
	webCurArrayBuffer = b;
}
static inline void WebActiveUnit(int unit)
{
	if (webBindCache && unit == webCurActiveUnit) return;
	glActiveTexture(GL_TEXTURE0 + unit);
	webCurActiveUnit = unit;
}
static inline void WebBindTextureUnit(int unit, GLuint tex)
{
	if (webBindCache && webCurTexture[unit] == tex) return;
	WebActiveUnit(unit);
	glBindTexture(GL_TEXTURE_2D, tex);
	webCurTexture[unit] = tex;
}
// "the backbuffer" for engine rendering: the canvas, or the offscreen present target
// when webOffscreenPresent is on (defined below with the present machinery)
static GLuint WebDefaultFBO();
// blend state cache setters - EVERY blend state change must route through these
// (defined with WebApplyBlendState below; see the comment there for why)
static void WebSetBlendEnabled(bool enabled);
static void WebSetBlendFunc(GLenum src, GLenum dst);
static void WebResetBlendCache();
static int webDrawsSinceFlush = 0;	// see webFlushEveryDraws below
static void WebTerrainCacheBeginFrame();	// terrain vertex cache, defined with the terrain code

// every gl draw the frame issues, counted unconditionally. draw-call volume is the one
// variable that has consistently moved the chrome/angle dropped-draw rate, so batching
// work needs a headline number to move (reported by the TERRAINCACHE debug line)
static int webTotalDraws = 0;

// shader flag bits (must match the fragment shader)
enum
{
	WebFlag_IgnoreTexRGB	= 1,	// rgb from color only, alpha still modulated (SELECTARG2)
	WebFlag_IgnoreTexAlpha	= 2,	// alpha from color only (ALPHAOP disable)
	WebFlag_BorderClamp		= 4,	// sample opaque black outside [0,1] (BORDER addressing)
	WebFlag_Modulate2		= 8,	// modulate by second texture (emissive sprite pass)
	WebFlag_ForceFullBright	= 16,	// cpu-side only: skip the ambient multiply (never sent to gl)
	WebFlag_BorderClamp2	= 32,	// second texture samples transparent black outside [0,1]
	WebFlag_UseNormalMapShader = 64,// cpu-side only: draw through normalMapShader (never sent to gl)
};

// per-program uniform locations, looked up once at link. Every program shares the one
// vertex shader, so the vs-side names resolve for all of them; the fs-side ones come back
// -1 on programs that do not declare them and the apply path skips those.
struct WebProgramLocs
{
	GLint worldViewProj, uvMatrix, uvMatrix2, color, flags, texture, texture2, texture3;
};
static WebProgramLocs webLocs, webBlurLocs, webNormalMapLocs, webLightLocs, webDirLightLocs;

// program -> locs, so a draw can find its own uniform locations instead of assuming the
// blur shader's. Registered by WebCompileProgram; small enough that a linear scan is
// cheaper than anything smarter.
struct WebProgramEntry { GLuint program; const WebProgramLocs* locs; };
static WebProgramEntry webProgramTable[8];
static int webProgramTableCount = 0;

static const WebProgramLocs* WebFindProgramLocs(GLuint program)
{
	for (int i = 0; i < webProgramTableCount; ++i)
		if (webProgramTable[i].program == program)
			return webProgramTable[i].locs;
	return &webLocs;
}

static const char* webVS = R"GLSL(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform mat4 uWorldViewProj;
uniform mat4 uUVMatrix;
uniform mat4 uUVMatrix2;
out vec2 vUV;
out vec2 vUV2;
out vec4 vColor;
void main()
{
	gl_Position = uWorldViewProj * vec4(aPos, 1.0);
	vUV = (uUVMatrix * vec4(aUV, 1.0, 1.0)).xy;
	vUV2 = (uUVMatrix2 * vec4(aUV, 1.0, 1.0)).xy;
	vColor = aColor;
}
)GLSL";

static const char* webFS = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D uTexture;
uniform sampler2D uTexture2;
uniform vec4 uColor;
uniform int uFlags;
in vec2 vUV;
in vec2 vUV2;
in vec4 vColor;
out vec4 outColor;
void main()
{
	vec4 tex = texture(uTexture, vUV);
	if ((uFlags & 4) != 0 && (vUV.x < 0.0 || vUV.x > 1.0 || vUV.y < 0.0 || vUV.y > 1.0))
		tex = vec4(0.0, 0.0, 0.0, 1.0);
	if ((uFlags & 8) != 0)
	{
		vec4 tex2 = texture(uTexture2, vUV2);
		if ((uFlags & 32) != 0 && (vUV2.x < 0.0 || vUV2.x > 1.0 || vUV2.y < 0.0 || vUV2.y > 1.0))
			tex2 = vec4(0.0);
		tex *= tex2;
	}
	vec3 rgb = uColor.rgb * vColor.rgb * (((uFlags & 1) != 0) ? vec3(1.0) : tex.rgb);
	float a = uColor.a * vColor.a * (((uFlags & 2) != 0) ? 1.0 : tex.a);
	outColor = vec4(rgb, a);
}
)GLSL";

// blurShader.psh ported line for line (validated in the phase 1.5 spike);
// uniform names match the HLSL constants so the constant-table emulation finds them
static const char* webBlurFS = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D uTexture;
uniform float blurSize;
uniform float blurBrightness;
in vec2 vUV;
in vec4 vColor;
out vec4 outColor;
void main()
{
	float colorScale = blurBrightness * 0.25;
	outColor = vec4(0.0);
	outColor += colorScale * texture(uTexture, vUV + vec2(0.0, blurSize));
	outColor += colorScale * texture(uTexture, vUV + vec2(0.0, -blurSize));
	outColor += colorScale * texture(uTexture, vUV + vec2(blurSize, 0.0));
	outColor += colorScale * texture(uTexture, vUV + vec2(-blurSize, 0.0));
}
)GLSL";

// ---- normal mapping (deferredRender's normals pass and the two light shaders) --------
//
// All three are ported line for line from the .psh files in data/shaders, with the
// uniform names kept identical so the constant-table emulation (FrankWebSetUniform*,
// driven unchanged by deferredRender.cpp / frankRender.cpp) finds them by name.
//
// Matrix convention: D3DXMATRIX is row-major and HLSL's mul(vector, matrix) treats the
// vector as a ROW vector. Uploading that same memory with glUniformMatrix4fv(transpose =
// GL_FALSE) makes GLSL read it column-major, which yields the transpose - so GLSL's
// M * v reproduces HLSL's mul(v, M) exactly. This is the same convention the ubershader's
// uWorldViewProj already relies on (see WebComputeWVP).
//
// Sampling note: the .psh files sample everything with In.Texture, and the vertex shader
// only differs between vUV/vUV2 when the two uv transforms differ - which they never do
// on these draws. vUV is used throughout so the two can never desync.

// normalMapShader.psh - rotate a tangent-space normal into world space
static const char* webNormalMapFS = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D uTexture;		// diffuse map (sampled for its alpha only)
uniform sampler2D uTexture2;	// normal map
uniform mat4 transformMatrix;
uniform vec4 diffuseColor;
uniform float diffuseNormalAlphaPercent;
in vec2 vUV;
out vec4 outColor;
void main()
{
	// apply world space normal transform
	vec4 normal = texture(uTexture2, vUV);
	normal = 2.0 * normal - 1.0;
	normal = transformMatrix * normal;
	normal = 0.5 + 0.5 * normal;
	vec4 c = clamp(normal, 0.0, 1.0);

	// modulate in the diffuse texture alpha
	float alpha = texture(uTexture, vUV).w;
	alpha = 1.0 - (1.0 - alpha) * diffuseNormalAlphaPercent;
	c.w *= alpha;

	// modulate in the diffuse color
	outColor = c * diffuseColor;
}
)GLSL";

// deferredLightShader.psh - point light: N.L plus specular, masked by the shadow map
static const char* webLightFS = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D uTexture;		// normal map buffer
uniform sampler2D uTexture2;	// specular map buffer
uniform sampler2D uTexture3;	// shadow map / light gel
uniform float lightHeight;
uniform vec4 lightColor;
uniform mat4 normalMatrix;
uniform mat4 lightMatrix;
uniform float specularPower;
uniform float specularHeight;
uniform float specularAmount;
in vec2 vUV;
out vec4 outColor;
void main()
{
	// get the light direction (normalize on a float4 with w = 0 is a 3d normalize)
	vec4 lightDirection = vec4(vUV, 0.0, 1.0);
	lightDirection = lightMatrix * lightDirection;
	lightDirection.z = lightHeight;
	lightDirection.w = 0.0;
	lightDirection = normalize(lightDirection);

	// get the normal map coordinates
	vec4 normalCoords = vec4(vUV, 1.0, 1.0);
	normalCoords = normalMatrix * normalCoords;

	// get the normal
	vec3 normal = texture(uTexture, normalCoords.xy).xyz;
	normal = 2.0 * normal - 1.0;
	normal = normalize(normal);

	// calculate lighting using a dot product
	float brightness = clamp(dot(normal, lightDirection.xyz), 0.0, 1.0);

	// calculate specular contribution
	vec3 viewDirection = vec3(0.5, 0.5, 0.0) - normalCoords.xyz;
	viewDirection.z = specularHeight;
	viewDirection = normalize(viewDirection);
	vec4 specularColor = texture(uTexture2, normalCoords.xy);
	vec3 reflection = normalize(2.0 * brightness * normal - lightDirection.xyz);
	// the exponent is guarded because glsl leaves pow(x, 0) undefined where hlsl returns
	// 1: UpdateSimpleLight never sets these constants, so they are still 0 on the first
	// frames, and a NaN here would survive the specularAmount multiply and poison the pixel
	vec4 specular = specularColor * pow(clamp(dot(reflection, viewDirection), 0.0, 1.0), max(specularPower, 0.001));

	// get the shadow map color
	vec4 shadowMapColor = texture(uTexture3, vUV);

	// modulate with shadow map and light color
	outColor = (brightness + specularAmount * specular) * shadowMapColor * lightColor;
}
)GLSL";

// directionalLightShader.psh - same, with the light direction supplied directly
static const char* webDirLightFS = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D uTexture;		// normal map buffer
uniform sampler2D uTexture2;	// specular map buffer
uniform sampler2D uTexture3;	// shadow map
uniform vec4 lightDirection;
uniform vec4 lightColor;
uniform mat4 normalMatrix;
uniform float specularPower;
uniform float specularHeight;
uniform float specularAmount;
in vec2 vUV;
out vec4 outColor;
void main()
{
	// get the normal map coordinates
	vec4 normalCoords = vec4(vUV, 1.0, 1.0);
	normalCoords = normalMatrix * normalCoords;

	// get the normal
	vec3 normal = texture(uTexture, normalCoords.xy).xyz;
	normal = 2.0 * normal - 1.0;
	normal = normalize(normal);

	// calculate lighting using a dot product
	float brightness = clamp(dot(normal, lightDirection.xyz), 0.0, 1.0);

	// calculate reflection vector
	vec3 reflection = normalize(2.0 * brightness * normal - lightDirection.xyz);

	// calculate specular contribution (the hlsl assigns a float3 to a float, taking .x)
	vec3 viewDirection = vec3(0.5, 0.5, 0.0) - normalCoords.xyz;
	viewDirection.z = specularHeight;
	viewDirection = normalize(viewDirection);
	vec3 specularIntensity = texture(uTexture2, normalCoords.xy).xyz;
	// exponent guarded for the same reason as deferredLightShader above
	float specular = specularIntensity.x * pow(clamp(dot(reflection, viewDirection), 0.0, 1.0), max(specularPower, 0.001));

	// get the shadow map color
	vec4 shadowMapColor = texture(uTexture3, vUV);

	// modulate with shadow map and light color
	outColor = (brightness + specularAmount * specular) * shadowMapColor * lightColor;
}
)GLSL";

// row-major 4x4 multiply matching the engine's convention (a then b, row-vector order)
static void WebMatMul(float* out, const float* a, const float* b)
{
	float r[16];
	for (int i = 0; i < 4; ++i)
	for (int j = 0; j < 4; ++j)
		r[i*4+j] = a[i*4+0]*b[0*4+j] + a[i*4+1]*b[1*4+j] + a[i*4+2]*b[2*4+j] + a[i*4+3]*b[3*4+j];
	memcpy(out, r, sizeof(r));
}

static GLuint WebCompileProgram(const char* vs, const char* fs, const char* label, WebProgramLocs& locs)
{
	const GLuint program = glCreateProgram();
	const GLenum types[2] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	const char* sources[2] = { vs, fs };
	for (int i = 0; i < 2; ++i)
	{
		const GLuint s = glCreateShader(types[i]);
		glShaderSource(s, 1, &sources[i], NULL);
		glCompileShader(s);
		GLint ok = 0;
		glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
		if (!ok)
		{
			char log[1024];
			glGetShaderInfoLog(s, sizeof(log), NULL, log);
			printf("webRender: %s shader compile FAILED: %s\n", label, log);
		}
		glAttachShader(program, s);
	}
	glLinkProgram(program);
	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		char log[1024];
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		printf("webRender: %s program link FAILED: %s\n", label, log);
	}
	locs.worldViewProj = glGetUniformLocation(program, "uWorldViewProj");
	locs.uvMatrix = glGetUniformLocation(program, "uUVMatrix");
	locs.uvMatrix2 = glGetUniformLocation(program, "uUVMatrix2");
	locs.color = glGetUniformLocation(program, "uColor");
	locs.flags = glGetUniformLocation(program, "uFlags");
	locs.texture = glGetUniformLocation(program, "uTexture");
	locs.texture2 = glGetUniformLocation(program, "uTexture2");
	locs.texture3 = glGetUniformLocation(program, "uTexture3");

	if (webProgramTableCount < (int)(sizeof(webProgramTable)/sizeof(webProgramTable[0])))
	{
		webProgramTable[webProgramTableCount].program = program;
		webProgramTable[webProgramTableCount].locs = &locs;
		++webProgramTableCount;
	}
	return program;
}

bool FrankWebRenderCreateContext()
{
	EmscriptenWebGLContextAttributes attrs;
	emscripten_webgl_init_context_attributes(&attrs);
	attrs.majorVersion = 2;
	attrs.alpha = false;
	// emscripten defaults antialias ON: a multisampled default framebuffer + per-frame
	// msaa resolve that a point-filtered pixel-art game gets nothing from. off also
	// removes a context-level difference from typical webgl content while hunting the
	// nvidia/chrome dropped-draw flicker
	attrs.antialias = false;
	webGLContext = emscripten_webgl_create_context("#canvas", &attrs);
	if (webGLContext <= 0)
	{
		printf("webRender: FAILED to create WebGL2 context (headless?), rendering disabled\n");
		webGLContext = 0;
		return false;
	}
	emscripten_webgl_make_context_current(webGLContext);

	// A lost context is otherwise SILENT and permanent: every later gl call becomes a
	// no-op, the picture stops updating, and the game looks frozen with nothing in the
	// log. Browsers also only allow a context to be restored if the default action of
	// webglcontextlost is prevented - without this listener recovery is impossible even
	// in principle. Mobile safari can drop the context on a canvas resize, which is what
	// a fullscreen toggle does, so this is the likeliest way an ipad "freezes".
	EM_ASM({
		var c = document.getElementById("canvas");
		if (!c || c.__frankContextHooks) return;
		c.__frankContextHooks = 1;
		c.addEventListener("webglcontextlost", function(e) {
			e.preventDefault();		// required, or the context can never come back
			console.error("webRender: WEBGL CONTEXT LOST - rendering has stopped");
			if (typeof Module !== "undefined") Module.frankContextLost = 1;
		}, false);
		c.addEventListener("webglcontextrestored", function() {
			console.warn("webRender: webgl context restored by the browser");
			if (typeof Module !== "undefined") Module.frankContextRestored = 1;
		}, false);
	});

	webProgram = WebCompileProgram(webVS, webFS, "uber", webLocs);
	webBlurProgram = WebCompileProgram(webVS, webBlurFS, "blur", webBlurLocs);
	webNormalMapProgram = WebCompileProgram(webVS, webNormalMapFS, "normalMap", webNormalMapLocs);
	webLightProgram = WebCompileProgram(webVS, webLightFS, "deferredLight", webLightLocs);
	webDirLightProgram = WebCompileProgram(webVS, webDirLightFS, "directionalLight", webDirLightLocs);

	// unit quad matching BuildQuad(): triangle strip, +-1, uv y-down
	// layout: pos3, uv2, color4 (color = white on the static quad)
	const float quad[4][9] =
	{
		{ -1,  1, 0,  0, 0,  1, 1, 1, 1 },
		{  1,  1, 0,  1, 0,  1, 1, 1, 1 },
		{ -1, -1, 0,  0, 1,  1, 1, 1, 1 },
		{  1, -1, 0,  1, 1,  1, 1, 1, 1 },
	};
	glGenVertexArrays(1, &webQuadVAO);
	glBindVertexArray(webQuadVAO);
	glGenBuffers(1, &webQuadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, webQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(3*sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(5*sizeof(float)));

	// streaming pool for simple verts (pos3 + packed color)
	webStreamPool.stride = 16;
	glGenVertexArrays(WebChunkPool::bufferCount, webStreamPool.vaos);
	glGenBuffers(WebChunkPool::bufferCount, webStreamPool.vbos);
	for (int i = 0; i < WebChunkPool::bufferCount; ++i)
	{
		glBindVertexArray(webStreamPool.vaos[i]);
		glBindBuffer(GL_ARRAY_BUFFER, webStreamPool.vbos[i]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, (void*)0);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
		glDisableVertexAttribArray(1);
		glVertexAttrib2f(1, 0, 0);	// uv constant for untextured verts
	}

	// streaming pool for textured verts (terrain, parsed primitives, fonts)
	struct WebTexVertexLayout { float x, y, z, u, v; unsigned char r, g, b, a; };
	webTexPool.stride = sizeof(WebTexVertexLayout);
	glGenVertexArrays(WebChunkPool::bufferCount, webTexPool.vaos);
	glGenBuffers(WebChunkPool::bufferCount, webTexPool.vbos);
	for (int i = 0; i < WebChunkPool::bufferCount; ++i)
	{
		glBindVertexArray(webTexPool.vaos[i]);
		glBindBuffer(GL_ARRAY_BUFFER, webTexPool.vbos[i]);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WebTexVertexLayout), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(WebTexVertexLayout), (void*)(3*sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(WebTexVertexLayout), (void*)(5*sizeof(float)));
	}

	// 1x1 white fallback texture
	const unsigned char white[4] = { 255, 255, 255, 255 };
	glGenTextures(1, &webWhiteTexture);
	glBindTexture(GL_TEXTURE_2D, webWhiteTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// scratch texture for webTexturePingPong: must be a DIFFERENT object from any
	// texture we actually draw with (including the white fallback), or the srv pointer
	// would not change and angle would elide the bind we are trying to force
	glGenTextures(1, &webPingPongTexture);
	glBindTexture(GL_TEXTURE_2D, webPingPongTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	WebCheckNoMapParam();
	WebResetBlendCache();	// fresh context = unknown gl state
	WebResetBindCache();
	printf("webRender: WebGL2 context + shaders ready\n");
	return true;
}

bool FrankWebRenderIsActive() { return webGLContext != 0; }

////////////////////////////////////////////////////////////////////////////////////////
// device emulation - the GL side of FrankWebFakeD3DDevice (frankPlatformWeb.h)
////////////////////////////////////////////////////////////////////////////////////////

// THIS IS THE FIX FOR THE DROPPED-DRAW FLICKER. Measured on the contention rig with the
// async meter: RGB8 render targets 808 drop events in 90s, RGBA8 render targets ZERO
// (repeated at 90s and 120s, at identical frame rate, with the meter's self-test proving
// it still catches injected drops under RGBA8 - so the zero is real, not blindness).
//
// d3d11 has NO 24-bit render target format. RGB8 is therefore EMULATED by angle over
// RGBA8, with alpha-fixup work under every framebuffer operation, and blits between an
// emulated RGB8 and a real RGBA8 target miss angle's fast copy path. That emulation path
// is where the draws go missing. It also explains why essentially nobody else hits this:
// RGB8 render targets are rare on the web (canvas and the usual formats are RGBA8), and
// we only used them to imitate d3d9's X8R8G8B8.
//
// X8R8G8B8 semantics still have to hold: on d3d such a surface has NO alpha channel, so
// sampling it yields alpha 1 and dst-alpha blending reads 1. We keep that exactly by
// pinning the alpha channel of those targets to 1 - cleared to 1 at creation, and alpha
// writes masked off whenever one is the render target (see WebApplyAlphaPin). So the
// storage changes and the observable behaviour does not.
//
// ?rgb8 in the url restores the old emulated-RGB8 targets for A/B measurement.
// mask alpha writes while an alpha-pinned target is bound, so its alpha stays at the 1
// it was cleared to and behaves like a d3d surface with no alpha channel at all. Called
// on every render target change; the canvas itself is created with alpha:false, so it
// needs no pinning.
// A/B for the residual dropped-draw flicker. The ORIGINAL bug was angle emulating
// GL_RGB8 over rgba8, which forces a PARTIAL RenderTargetWriteMask (0x7, not 0xF) on
// every draw into such a target - and draws with a partial mask were the ones getting
// dropped. Switching to real rgba8 storage removed the emulation and measured 704
// events -> 0... but this alpha pin is itself a glColorMask with alpha off, which is
// ALSO a partial write mask. So the trigger may only be half removed.
// ON, and it has to be. An earlier pass turned this off after "auditing" the pipeline
// for alpha use and concluding nothing read it. That audit asked the wrong question: it
// looked for blending on DESTINATION alpha (there is none) and missed that the blur and
// downsample chain SAMPLES these targets, so the sampled alpha becomes the SOURCE alpha
// of the next blend. Pinned, that is always 1; unpinned it is whatever the last draw
// left, and the compositing blows out - TestGame's sky panels went from dark navy to
// near-white, which is what caught it.
//
// It is also not the flicker trigger, which was the other reason to remove it: tested
// directly, the dropped-draw bug reproduces with this both on and off. The fix for that
// is webOffscreenPresent.
//
// 0 is kept for A/B only. It is not a shipping configuration.
ConsoleCommandSimple(bool, webAlphaPin, true);

static bool webAlphaMaskClosed = false;
static bool webPresentAlphaPinned = false;	// set when the present target is RGBA8
static void WebApplyAlphaPin(const FrankWebTexture* target)
{
	// a NULL target means the default framebuffer, which is really the offscreen present
	// target when that is in use - it needs the same pin, for the same reason
	const bool wantClosed = webAlphaPin && (target ? target->alphaPinned : webPresentAlphaPinned);
	if (wantClosed == webAlphaMaskClosed)
		return;
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, wantClosed ? GL_FALSE : GL_TRUE);
	webAlphaMaskClosed = wantClosed;
}

static bool WebUseRGB8Targets()
{
	static int useRGB8 = -1;
	if (useRGB8 < 0)
	{
		useRGB8 = EM_ASM_INT({
			var q = (location.search || "") + (location.hash || "");
			return q.indexOf("rgb8") >= 0 && q.indexOf("rgba8") < 0 ? 1 : 0;
		});
		if (useRGB8)
			printf("webRender: ?rgb8 - emulated RGB8 render targets restored (EXPECT FLICKER)\n");
	}
	return useRGB8 != 0;
}

HRESULT FrankWebDeviceCreateTexture(UINT width, UINT height, DWORD usage, int format, FrankWebTexture** out)
{
	FrankWebTexture* t = new FrankWebTexture();
	t->width = (int)width;
	t->height = (int)height;
	t->format = format;

	if (webGLContext)
	{
		glGenTextures(1, &t->glTexture);
		glBindTexture(GL_TEXTURE_2D, t->glTexture);
		// X8R8G8B8/A1R5G5B5 have no alpha on d3d (samples and dst-alpha reads give 1).
		// We now give them REAL RGBA8 storage - emulated RGB8 is what drops draws - and
		// reproduce the missing-alpha semantics by pinning alpha to 1 instead.
		const bool formatHasAlpha = (format == D3DFMT_A8R8G8B8);
		const bool useRGB8 = !formatHasAlpha && WebUseRGB8Targets();
		t->alphaPinned = !formatHasAlpha && !useRGB8;
		glTexImage2D(GL_TEXTURE_2D, 0, useRGB8 ? GL_RGB8 : GL_RGBA8, width, height, 0,
			useRGB8 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glGenFramebuffers(1, &t->glFBO);
		WebBindFBO(t->glFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->glTexture, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			printf("webRender: FBO incomplete for %dx%d render target\n", (int)width, (int)height);

		if (t->alphaPinned)
		{
			// set alpha to 1 ONCE, with the mask open; from here on WebApplyAlphaPin
			// keeps alpha writes masked off so it can never change
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		// rebind whatever target was current
		const FrankWebTexture* rt = FrankWebGetDeviceState().renderTarget;
		WebBindFBO(rt ? rt->glFBO : WebDefaultFBO());
		WebApplyAlphaPin(rt);
	}

	*out = t;
	return 0;
}

void FrankWebDeviceSetRenderTarget(FrankWebTexture* surface)
{
	if (!webGLContext)
		return;
	WebBindFBO(surface ? surface->glFBO : WebDefaultFBO());
	WebApplyAlphaPin(surface);
	if (surface)
		glViewport(0, 0, surface->width, surface->height);
	else
		glViewport(0, 0, g_backBufferWidth, g_backBufferHeight);
}

// ANGLE only takes the cheap CopySubresourceRegion path for a blit when the source and
// destination agree on sample count, FORMAT ID, size and orientation; otherwise it runs a
// real shader blit that injects its own draw with its own MAP_WRITE_DISCARD vertex buffer
// and clobbers the d3d11 state shadow. GL_RGB8 has no d3d11 equivalent and is emulated
// over RGBA8, so an RGB8<->RGBA8 pair always misses the fast path - and every deferred
// light target here is X8R8G8B8 (=> RGB8) while textureFinal is A8R8G8B8 (=> RGBA8).
// This prints each distinct blit shape once so the fast/slow split can be read off
// directly and correlated against the per-target loss counts.
ConsoleCommandSimple(bool, webBlitSurvey, false);

static void WebSurveyBlit(const FrankWebTexture* src, const FrankWebTexture* dst)
{
	if (!webBlitSurvey)
		return;

	// remember shapes already reported so this stays one line per distinct blit
	static unsigned seen[64];
	static int seenCount = 0;
	const unsigned key = (unsigned)((src->width << 20) ^ (src->height << 12) ^
		(dst->width << 4) ^ dst->height ^ ((unsigned)src->format << 24) ^ ((unsigned)dst->format << 8));
	for (int i = 0; i < seenCount; ++i)
	{
		if (seen[i] == key)
			return;
	}
	if (seenCount < 64)
		seen[seenCount++] = key;

	const bool srcAlpha = (src->format == D3DFMT_A8R8G8B8);
	const bool dstAlpha = (dst->format == D3DFMT_A8R8G8B8);
	const bool stretched = (src->width != dst->width) || (src->height != dst->height);
	const bool formatMismatch = (srcAlpha != dstAlpha);
	printf("BLITSURVEY %dx%d %s -> %dx%d %s  %s%s%s\n",
		src->width, src->height, srcAlpha ? "RGBA8" : "RGB8 ",
		dst->width, dst->height, dstAlpha ? "RGBA8" : "RGB8 ",
		stretched ? "STRETCHED " : "", formatMismatch ? "FORMAT-MISMATCH " : "",
		(stretched || formatMismatch) ? "<-- angle shader-blit path" : "(fast copy path)");
}

void FrankWebDeviceStretchRect(FrankWebTexture* src, FrankWebTexture* dst)
{
	if (!webGLContext || !src || !dst)
		return;
	WebFlushSpriteBatch();	// the blit must see everything drawn into src
	WebSurveyBlit(src, dst);
	// release any sampler binding of the blit endpoints first. the deferred shadow
	// stretch loops blit INTO a texture that the previous draw just sampled (and will
	// sample again next draw) - legal gl, but on angle's d3d11 backend every such blit
	// forces SRV/RTV hazard resolution on the same resource, dozens of times per frame
	// scaling with light count. keep the roles disjoint so that machinery never runs.
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, webWhiteTexture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, webWhiteTexture);
	WebClearSamplers();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, src->glFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst->glFBO);
	glBlitFramebuffer(0, 0, src->width, src->height, 0, 0, dst->width, dst->height,
		GL_COLOR_BUFFER_BIT, (src->width == dst->width && src->height == dst->height) ? GL_NEAREST : GL_LINEAR);
	const FrankWebTexture* rt = FrankWebGetDeviceState().renderTarget;
	WebBindFBO(rt ? rt->glFBO : WebDefaultFBO());
}

// MECHANISM EXPERIMENT, only meaningful together with ?rgb8. On an emulated RGB8 target
// angle rewrites the clear alpha to 1.0 regardless of what we ask for ("check if the
// actual format has a channel that the internal format does not and set them to the
// default values"). Clearing to alpha 1 ourselves makes that rewrite a no-op. If the drop
// rate falls, the mechanism is the CLEAR VALUE - (0,0,0,1) is not a canonical fast-clear
// value, so it defeats render-target compression and forces read-modify-write. If the
// rate is unchanged, the mechanism is instead the per-draw partial write mask that
// emulated RGB8 forces on every draw.
ConsoleCommandSimple(bool, webClearAlphaOne, false);

void FrankWebDeviceClear(DWORD color)
{
	if (!webGLContext)
		return;
	WebFlushSpriteBatch();	// else the pending batch would land on top of the clear
	glClearColor(((color >> 16) & 0xff) / 255.0f, ((color >> 8) & 0xff) / 255.0f,
		(color & 0xff) / 255.0f,
		webClearAlphaOne ? 1.0f : ((color >> 24) & 0xff) / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

// ascii-art surface dump for shadowSaveTextures debugging (the web stand-in for
// D3DXSaveSurfaceToFile). row 0 of the readback is texture v=0 = content TOP in the
// engine's d3d-like convention, so it prints top-down like the texture samples.
void FrankWebDumpSurface(const wchar_t* name, FrankWebTexture* t)
{
	if (!webGLContext || !t || !t->glFBO)
		return;
	WebFlushSpriteBatch();	// readback must see the finished frame

	unsigned char* pixels = (unsigned char*)malloc((size_t)t->width * t->height * 4);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, t->glFBO);
	glReadPixels(0, 0, t->width, t->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	const FrankWebTexture* rt = FrankWebGetDeviceState().renderTarget;
	WebBindFBO(rt ? rt->glFBO : WebDefaultFBO());

	const int W = 96, H = 40;
	printf("---- surface %ls (%dx%d) ----\n", name, t->width, t->height);
	for (int gy = 0; gy < H; ++gy)
	{
		char line[W + 1];
		for (int gx = 0; gx < W; ++gx)
		{
			// sample the cell center
			const int px = (gx * t->width) / W + t->width / (2 * W);
			const int py = (gy * t->height) / H + t->height / (2 * H);
			const unsigned char* p = pixels + 4 * ((size_t)py * t->width + px);
			const int lum = (p[0] + p[1] + p[2]) / 3;
			line[gx] = " .:-=+*#%@"[(lum * 9) / 255];
		}
		line[W] = 0;
		printf("%s\n", line);
	}

	// also write a real image into the in-memory FS so the browser test harness can
	// pull it out for pixel-level comparison against the windows debug jpgs.
	// top-down BMP (negative height); readback row 0 is the content top.
	{
		char path[128];
		snprintf(path, sizeof(path), "/dump_%ls.bmp", name);
		FILE* f = fopen(path, "wb");
		if (f)
		{
			const int w = t->width, h = t->height;
			const int rowBytes = (w * 3 + 3) & ~3;
			const unsigned int dataSize = rowBytes * h;
			const unsigned int fileSize = 54 + dataSize;
			unsigned char header[54] = { 'B','M' };
			*(unsigned int*)(header + 2) = fileSize;
			*(unsigned int*)(header + 10) = 54;
			*(unsigned int*)(header + 14) = 40;
			*(int*)(header + 18) = w;
			*(int*)(header + 22) = -h;	// negative = top-down
			*(unsigned short*)(header + 26) = 1;
			*(unsigned short*)(header + 28) = 24;
			fwrite(header, 1, 54, f);
			unsigned char* row = (unsigned char*)malloc(rowBytes);
			memset(row, 0, rowBytes);
			for (int y = 0; y < h; ++y)
			{
				const unsigned char* src = pixels + 4 * (size_t)y * w;
				for (int x = 0; x < w; ++x)
				{
					row[x*3 + 0] = src[x*4 + 2];
					row[x*3 + 1] = src[x*4 + 1];
					row[x*3 + 2] = src[x*4 + 0];
				}
				fwrite(row, 1, rowBytes, f);
			}
			free(row);
			fclose(f);
			printf("webRender: wrote %s\n", path);
		}
	}
	free(pixels);
}

// constant-table emulation: set uniforms by hlsl name on the shader's program
static void WebDrainErrors(const char* site);

// these bind a program to set a uniform on it, so they drain any pending sprite batch
// first and go through WebBindProgram to keep the bind tracking honest
void FrankWebSetUniformFloat(unsigned int glProgram, const char* name, float value)
{
	if (!webGLContext || !glProgram) return;
	WebFlushSpriteBatch();
	WebDrainErrors("before-constant-table");
	WebBindProgram(glProgram);
	const GLint loc = glGetUniformLocation(glProgram, name);
	if (loc >= 0) glUniform1f(loc, value);
	WebDrainErrors("constant-table");
}
void FrankWebSetUniformVec4(unsigned int glProgram, const char* name, const float* v)
{
	if (!webGLContext || !glProgram) return;
	WebFlushSpriteBatch();
	WebBindProgram(glProgram);
	const GLint loc = glGetUniformLocation(glProgram, name);
	if (loc >= 0) glUniform4fv(loc, 1, v);
}
void FrankWebSetUniformMatrix(unsigned int glProgram, const char* name, const float* m)
{
	if (!webGLContext || !glProgram) return;
	WebFlushSpriteBatch();
	WebBindProgram(glProgram);
	const GLint loc = glGetUniformLocation(glProgram, name);
	if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, m);
}

////////////////////////////////////////////////////////////////////////////////////////
// frame begin/end, driven by the web shell
////////////////////////////////////////////////////////////////////////////////////////

// Offscreen present. Instead of drawing the scene straight into the canvas (which on
// chrome/angle-d3d11 is a dxgi swap-chain surface owned by the compositor), draw into an
// ordinary full-screen render target and blit it to the canvas once per frame.
// Pixel output is identical.
//
// ON, and treat it as the FIX rather than a mitigation. Tested directly: with everything
// else off the flicker reproduces immediately, and turning this one switch on stops it -
// on its own, without webShadowRender2. Two hypotheses died to get here. It is not the
// emulated GL_RGB8 targets on their own (real rgba8 helped a lot but left a residual),
// and it is not the partial write mask from the alpha pin (webAlphaPin 0 and 1 both
// still flicker).
//
// What is left is the DEFAULT FRAMEBUFFER itself. Every draw that lands on the canvas is
// landing on a swap-chain surface the compositor co-owns, and those are the draws that go
// missing. That fits the symptoms better than anything before it: what disappears is the
// light overlay composite - the multiply that darkens the scene - which is exactly the
// draw that targets the canvas. "The whole light channel is gone" is that one draw being
// dropped, not a shadow map half-rendered.
//
// Cost is one full-screen blit per frame - and on a TILE-BASED gpu that is a whole extra
// render pass, which apple's own guidance and the safari research both call out as
// expensive. The bug is specific to angle's D3D11 backend (a dxgi swap chain), and no
// apple device has one, so this is skipped there: see WebUseOffscreenPresent.
// That is a deliberate bet on hardware we cannot test here, made because the alternative
// is shipping a known extra full-screen pass to every mac and ipad player. It is one
// switch (webOffscreenPresent 1 forces it back on) and the first thing to try if an
// apple tester reports flicker.
ConsoleCommandSimple(bool, webOffscreenPresent, true);

// Apple gpus (every ipad, every apple silicon mac, through safari or chrome) are
// TILE-BASED. Two things key off this: mid-frame flushes (which force the tiler to
// resolve early, splitting one tile pass into several) and the offscreen present above.
static bool WebGpuIsTileBased()
{
	static int isTiled = -1;
	if (isTiled < 0)
	{
		// NOT glGetString(GL_RENDERER): browsers mask that for fingerprinting (it comes
		// back as "WebKit WebGL"), and the real string needs WEBGL_debug_renderer_info,
		// which is itself being restricted. The platform is what we actually care about
		// anyway - every current apple device is tile-based.
		isTiled = EM_ASM_INT({
			var n = navigator;
			if (/iPad|iPhone|iPod/.test(n.platform || "")) return 1;
			// ipadOS 13+ reports itself as a mac; touch points give it away
			if (/Mac/.test(n.platform || "") && (n.maxTouchPoints || 0) > 1) return 1;
			if (/Mac/.test(n.platform || "")) return 1;	// apple silicon; intel macs cost nothing here
			return 0;
		});
		printf("webRender: %s gpu - offscreen present %s, mid-frame flushes %s, buffer respec default %s\n",
			isTiled ? "apple/tile-based" : "immediate-mode",
			isTiled ? "OFF" : "on", isTiled ? "OFF" : "on", isTiled ? "ON" : "off");
	}
	return isTiled > 0;
}

// the dropped-draw bug lives in angle's d3d11 swap chain, which apple devices do not
// have - so they skip the blit and render straight to the canvas
static bool WebUseOffscreenPresent()
{
	return webOffscreenPresent && !WebGpuIsTileBased();
}
static GLuint webPresentFBO = 0, webPresentTex = 0;
static int webPresentW = 0, webPresentH = 0;

// the "default framebuffer" for all engine rendering: either the real canvas (0) or the
// offscreen present target. everything that used to bind 0 binds this instead.
static GLuint WebDefaultFBO()
{
	if (!WebUseOffscreenPresent())
		return 0;
	if (webPresentFBO && (webPresentW != g_backBufferWidth || webPresentH != g_backBufferHeight))
	{
		glDeleteFramebuffers(1, &webPresentFBO);
		glDeleteTextures(1, &webPresentTex);
		webPresentFBO = webPresentTex = 0;
	}
	if (!webPresentFBO)
	{
		webPresentW = g_backBufferWidth;
		webPresentH = g_backBufferHeight;
		glGenTextures(1, &webPresentTex);
		glBindTexture(GL_TEXTURE_2D, webPresentTex);
		// RGBA8, like every other render target now: emulated RGB8 is what loses draws
		// on angle-d3d11. The canvas is created with alpha:false and expects destination
		// alpha to read as 1, which the alpha pin below preserves - this target is
		// cleared to alpha 1 and never has alpha written again.
		const bool presentRGB8 = WebUseRGB8Targets();
		glTexImage2D(GL_TEXTURE_2D, 0, presentRGB8 ? GL_RGB8 : GL_RGBA8, webPresentW, webPresentH, 0,
			presentRGB8 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		webPresentAlphaPinned = !presentRGB8;
		glGenFramebuffers(1, &webPresentFBO);
		glBindFramebuffer(GL_FRAMEBUFFER, webPresentFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, webPresentTex, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			printf("webRender: present FBO incomplete (%dx%d) - falling back to direct canvas\n",
				webPresentW, webPresentH);
			glDeleteFramebuffers(1, &webPresentFBO);
			glDeleteTextures(1, &webPresentTex);
			webPresentFBO = webPresentTex = 0;
			webOffscreenPresent = false;
			return 0;
		}
		if (webPresentAlphaPinned)
		{
			// alpha to 1 once, then WebApplyAlphaPin keeps it masked off - the canvas is
			// alpha:false and expects dst alpha to read as 1
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			webAlphaMaskClosed = false;
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		printf("webRender: offscreen present target %dx%d (%s)\n", webPresentW, webPresentH,
			webPresentAlphaPinned ? "RGBA8, alpha pinned to 1" : "RGB8");
	}
	return webPresentFBO;
}

void FrankWebRenderBegin()
{
	if (!webGLContext)
		return;

	// resync pump: switching away to another gl context and back makes angle rebuild
	// its whole d3d11 binding state, repairing anything external code corrupted while
	// we were not looking. done here so the repair lands before any of this frame's work
	if (webContextPump == 1)
		WebContextPump();

	WebDrainErrors("frame-begin");

	// bind the canvas and reset per-frame device state, mirroring OnD3D9FrameRender
	FrankWebGetDeviceState().renderTarget = NULL;
	FrankWebGetDeviceState().ResetPerFrame();

	// OnD3D9FrameRender applies the game's filter mode over the linear defaults
	// (frankEngine.cpp:517) - without this, point-filtered games (piroot) sample
	// terrain tile sheets with trilinear mips and bleed thin cracks between tiles
	if (g_render)
		g_render->SetFiltering();
	const GLuint frameFBO = WebDefaultFBO();
	WebBindFBO(frameFBO);
	glViewport(0, 0, g_backBufferWidth, g_backBufferHeight);
	if (frameFBO)
	{
		// the real canvas is cleared automatically after every composite (the context is
		// created with preserveDrawingBuffer:false). an offscreen target is NOT, so
		// without this any frame the game does not fully overdraw would inherit stale
		// pixels - which makes a one-frame artifact persist across frames.
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	WebSetBlendEnabled(true);
	WebSetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	webDrawsSinceFlush = 0;		// see webFlushEveryDraws
	WebTerrainCacheBeginFrame();
	WebFlushSpriteBatch();		// nothing may ever survive across a frame boundary
	WebClearSamplers();
}

// cap how far the gpu can run behind the cpu. NOTE: the first version of this used
// glClientWaitSync with a 50ms timeout - ILLEGAL in webgl2 (chrome caps the allowed
// timeout at ZERO), so it raised INVALID_OPERATION every frame and never waited; the
// queue-depth experiment was never actually run. glFinish is the webgl-legal way.
// 0 = off, 1 = glFinish at frame end (full drain - costs fps, diagnostic)
ConsoleCommandSimple(int, webFramePacing, 0);

void FrankWebRenderEnd()
{
	if (!webGLContext)
		return;
	WebFlushSpriteBatch();	// the present blit must see the whole frame

	// present the offscreen target to the canvas in one blit (see webOffscreenPresent).
	// gated on the SETTING too: toggling it off mid-run must stop presenting, or the
	// stale target gets blitted over a frame that was already drawn to the canvas and
	// the picture appears frozen
	if (WebUseOffscreenPresent() && webPresentFBO)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, webPresentFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, webPresentW, webPresentH, 0, 0, g_backBufferWidth, g_backBufferHeight,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
		WebBindFBO(0);
	}

	if (webFramePacing == 1)
		glFinish();
}

bool FrankRender::BeginRender(bool needsClear, const Color& clearColor, DWORD clearFlags)
{
	isInRenderBlock = true;
	if (needsClear)
		FrankWebDeviceClear((DWORD)clearColor);
	return true;
}

void FrankRender::EndRender()
{
	isInRenderBlock = false;
}

void FrankRender::SetPointFilter(bool pointFilter)
{
	// same sampler-state writes the d3d path makes; captured and applied at draw
	const DWORD filter = pointFilter ? 1 : 2;	// POINT : LINEAR
	DXUTGetD3D9Device()->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
	DXUTGetD3D9Device()->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
	DXUTGetD3D9Device()->SetSamplerState(0, D3DSAMP_MIPFILTER, pointFilter ? 0 : 2);
}

////////////////////////////////////////////////////////////////////////////////////////
// blend + color pipeline rules from the captured device state
////////////////////////////////////////////////////////////////////////////////////////

static GLenum WebMapBlendFactor(DWORD d3dBlend, GLenum fallback)
{
	switch (d3dBlend)
	{
		case 1: return GL_ZERO;
		case 2: return GL_ONE;
		case 3: return GL_SRC_COLOR;
		case 4: return GL_ONE_MINUS_SRC_COLOR;
		case 5: return GL_SRC_ALPHA;
		case 6: return GL_ONE_MINUS_SRC_ALPHA;
		case 7: return GL_DST_ALPHA;
		case 8: return GL_ONE_MINUS_DST_ALPHA;
		case 9: return GL_DST_COLOR;
		case 10: return GL_ONE_MINUS_DST_COLOR;
	}
	return fallback;
}

// apply the blend mode the engine last set through the fake device
// blend state cache. the d3d path re-applies blend state per draw, so the naive
// translation emitted glEnable/glDisable + glBlendFunc + glBlendEquation on EVERY draw
// (~1000 redundant state changes per frame). command-stream bisection identified that
// churn as the trigger for the chrome/angle-d3d11 dropped-draw flicker: replaying the
// game's exact gl stream with those redundant calls removed dropped the glitch rate
// from 3-4 per cycle to zero, while every other category (uploads, binds, sampler
// state, draw volume) only scaled with total work.
// ALL blend state must go through these setters so the cache cannot go stale.
ConsoleCommandSimple(bool, webBlendCache, true);	// 0 = old always-emit behavior (A/B)
static int webGLBlendEnabled = -1;					// -1 unknown, 0 off, 1 on
static GLenum webGLBlendSrc = 0, webGLBlendDst = 0, webGLBlendEq = 0;
// REAL blend state changes reaching gl per frame (reported by the TERRAINCACHE line).
// blend churn is the one command category bisection tied directly to the dropped-draw
// bug, so this is the number any blend-related experiment is trying to move
static int webBlendChanges = 0;

static void WebResetBlendCache()
{
	webGLBlendEnabled = -1;
	webGLBlendSrc = webGLBlendDst = webGLBlendEq = 0;
}

// periodic glFlush inside the frame. command-stream bisection (replaying the game's
// exact gl stream with nothing else changed) showed that inserting a flush every ~2000
// commands HALVES the dropped-draw rate on chrome/angle-d3d11, with the detector fully
// sensitive and the rendered image bit-identical. it is a mitigation, not a cure - the
// remaining drops are unexplained - but it costs nothing visually.
// 0 = off; N = flush every N draw calls.
// The 2x was measured when the frame issued ~700-1300 draws and N was 100, i.e. ~7-13
// flushes per frame. The terrain cache and the sprite batcher cut the draw count ~3.7x,
// so N had to come down with it or the mitigation would quietly weaken to ~2 flushes a
// frame. 25 keeps the flush CADENCE where it was measured, not the divisor.
// LEFT ON, unlike the other mitigations, because turning it off measured WORSE here:
// 3 runs at ~36% cpu with it off against ~29% with it on, same 60fps either way. The
// theory says a mid-frame flush should be pure cost, and on a TILE-BASED gpu (every
// ipad, every apple silicon mac) it should be actively bad - it forces the tiler to
// resolve what it has accumulated, splitting one tile pass into several. But that is
// theory about hardware we cannot measure, against a measurement we can, so it stays
// until the apple-side research says otherwise. See local/mac-ios-webgl-perf-brief.md.
ConsoleCommandSimple(int, webFlushEveryDraws, 25);

static void WebCountDrawForFlush()
{
	if (webFlushEveryDraws <= 0 || WebGpuIsTileBased())
		return;
	if (++webDrawsSinceFlush >= webFlushEveryDraws)
	{
		webDrawsSinceFlush = 0;
		glFlush();
	}
}

static void WebSetBlendEnabled(bool enabled)
{
	const int want = enabled ? 1 : 0;
	if (webBlendCache && webGLBlendEnabled == want)
		return;
	if (enabled)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
	webGLBlendEnabled = want;
	++webBlendChanges;
}

static void WebSetBlendFunc(GLenum src, GLenum dst)
{
	if (webBlendCache && webGLBlendSrc == src && webGLBlendDst == dst)
		return;
	glBlendFunc(src, dst);
	webGLBlendSrc = src;
	webGLBlendDst = dst;
	++webBlendChanges;
}

static void WebSetBlendEquation(GLenum eq)
{
	if (webBlendCache && webGLBlendEq == eq)
		return;
	glBlendEquation(eq);
	webGLBlendEq = eq;
	++webBlendChanges;
}

// what WebApplyBlendState WOULD set, without touching gl. the sprite batcher needs this
// separately because blend state is part of its batch key and must be compared BEFORE
// any gl state is applied (applying it early would change the pending batch's blending)
struct WebBlendState { int enabled; GLenum src, dst, eq; };

static WebBlendState WebResolveBlendState()
{
	const FrankWebDeviceState& s = FrankWebGetDeviceState();
	WebBlendState out;
	out.enabled = s.renderStates[27] ? 1 : 0;	// D3DRS_ALPHABLENDENABLE
	if (!out.enabled)
	{
		// the func/equation are not applied when blending is off, so they must not
		// distinguish two otherwise identical batch keys either
		out.src = out.dst = out.eq = 0;
		return out;
	}
	out.src = WebMapBlendFactor(s.renderStates[19], GL_SRC_ALPHA);
	out.dst = WebMapBlendFactor(s.renderStates[20], GL_ONE_MINUS_SRC_ALPHA);
	const DWORD op = s.renderStates[171];		// D3DRS_BLENDOP
	out.eq = op == 2 ? GL_FUNC_SUBTRACT : op == 3 ? GL_FUNC_REVERSE_SUBTRACT :
		op == 4 ? GL_MIN : op == 5 ? GL_MAX : GL_FUNC_ADD;
	return out;
}

static void WebApplyBlendState()
{
	const FrankWebDeviceState& s = FrankWebGetDeviceState();
	if (!s.renderStates[27])	// D3DRS_ALPHABLENDENABLE
	{
		WebSetBlendEnabled(false);
		return;
	}
	WebSetBlendEnabled(true);
	WebSetBlendFunc(WebMapBlendFactor(s.renderStates[19], GL_SRC_ALPHA),
					WebMapBlendFactor(s.renderStates[20], GL_ONE_MINUS_SRC_ALPHA));
	const DWORD op = s.renderStates[171];	// D3DRS_BLENDOP
	WebSetBlendEquation(op == 2 ? GL_FUNC_SUBTRACT : op == 3 ? GL_FUNC_REVERSE_SUBTRACT :
		op == 4 ? GL_MIN : op == 5 ? GL_MAX : GL_FUNC_ADD);
}

// d3d material rule (frankRender.h:629): with LIGHTING on, output rgb =
// color.rgb x AMBIENT render state, alpha = color.a. this is how the emissive pass
// blacks out non-emissive objects and how EmissiveRenderBlock un-blacks them.
static Color WebApplyMaterialColor(const Color& color)
{
	const FrankWebDeviceState& s = FrankWebGetDeviceState();
	if (!s.renderStates[137])	// D3DRS_LIGHTING off: vertex/diffuse color path
		return color;
	const DWORD ambient = s.renderStates[139];
	if (ambient == 0x00FFFFFF)
		return color;
	Color out = color;
	out.r *= ((ambient >> 16) & 0xff) / 255.0f;
	out.g *= ((ambient >> 8) & 0xff) / 255.0f;
	out.b *= (ambient & 0xff) / 255.0f;
	return out;
}

// shader flags from the captured texture-stage and sampler state (stage 0)
static int WebGetStateFlags()
{
	const FrankWebDeviceState& s = FrankWebGetDeviceState();
	int flags = 0;
	if (s.textureStageStates[0][1] == 3)	// COLOROP = SELECTARG2 (color from diffuse)
		flags |= WebFlag_IgnoreTexRGB;
	if (s.textureStageStates[0][4] == 1)	// ALPHAOP = DISABLE
		flags |= WebFlag_IgnoreTexAlpha;
	if (s.samplerStates[0][1] == 4 || s.samplerStates[0][2] == 4)	// BORDER addressing
		flags |= WebFlag_BorderClamp;
	return flags;
}

// sampler and program selection now live with the sprite batcher, as WebResolveSamplerState
// / WebApplySampler and a plain field on WebResolvedDraw - both had to become resolvable
// without touching gl. (Only the blur shader has a real program; see LoadPixelShader.)

// compute world*view*proj with the FBO vertical flip applied when the current render
// target is a texture (see header comment)
static void WebComputeWVP(float* out, const float* worldMatrix)
{
	const FrankWebDeviceState& s = FrankWebGetDeviceState();
	float proj[16];
	memcpy(proj, s.projection, sizeof(proj));
	if (s.renderTarget)
	{
		// flip clip-space y: negate the second column (row-vector convention)
		proj[1] = -proj[1]; proj[5] = -proj[5]; proj[9] = -proj[9]; proj[13] = -proj[13];
	}
	float viewProj[16];
	WebMatMul(viewProj, s.view, proj);
	if (worldMatrix)
		WebMatMul(out, worldMatrix, viewProj);
	else
		memcpy(out, viewProj, sizeof(viewProj));
}

static const float webIdentity[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };

// the central draw: sets program, matrices, color (material rule), flags, textures,
// sampler + blend state, then draws either the static unit quad or streamed verts
struct WebTexVertex { float x, y, z; float u, v; unsigned char r, g, b, a; };
////////////////////////////////////////////////////////////////////////////////////////
// lighting flicker probe: per-pass counters that catch frames where terrain fails to
// reach the light shadow map while the rest of the scene renders normally. when it
// trips it dumps everything to the browser console (FLICKERPROBE lines).
////////////////////////////////////////////////////////////////////////////////////////

// OFF by default: this is diagnostics for a bug that is fixed (angle emulating GL_RGB8
// render targets over rgba8 - see WebUseRGB8Targets). It costs several SYNCHRONOUS
// glReadPixels per frame, which drain the gpu pipeline and dominated everything else -
// 54% of all cpu time in a profile of the shipped build. Turn it on from the console
// only while chasing a dropped-draw regression.
ConsoleCommandSimple(bool, webShadowProbe, false);

// workaround experiment for the nvidia/chrome dropped-draw flicker - forces command
// submission boundaries so a contending chrome instance can't preempt mid-batch.
// live-tunable from the console: 0=off, 1=glFlush after the shadow pass (default),
// 2=glFinish after the shadow pass (full sync - slow but diagnostic: if this fixes it
// the bug is 100% async scheduling), 3=level 1 plus glFlush after every terrain flush
// during the shadow pass, 4=glFlush after EVERY streamed draw in every pass (slowest,
// maximally granular - the f8 view showed the emissive map glitching too, so per-pass
// flushes only protected one of several victims)
// 0 (off) by default now - another mitigation from the flicker hunt rather than the fix
// (see webOffscreenPresent). A glFlush every frame forces a submission boundary, which
// costs on every backend and can stop metal batching work on safari.
ConsoleCommandSimple(int, webShadowFlush, 0);

// auto-download pngs of the glitched shadow map on capture (off: console lines only)
ConsoleCommandSimple(bool, webShadowCapture, false);

// live bisection for the dropped-draw hunt: skip whole draw categories without a
// rebuild. bits: 1=terrain batches, 2=other streamed (fonts/parsed prims), 4=static
// quads (sprites/gui boxes), 8=simple verts (debug lines/ropes). e.g. webDrawSkipMask 5
// removes terrain+quads. watch whether the flicker survives with each category gone.
ConsoleCommandSimple(int, webDrawSkipMask, 0);

struct WebProbeTerrainStats
{
	bool ran;										// TerrainLayerRender reached this pass at all
	int patchesPassed;								// patches that passed the camera test
	int tilesSubmitted;								// tiles that passed the per-tile camera test
	int vertsSubmitted;								// verts appended to the terrain batch
	int flushedVerts;								// verts that actually went through WebDraw
	int glErrors;									// glGetError hits after shadow-pass flushes
	float camL, camB, camR, camT;					// camera aabb used to cull this pass
};
static const int webProbePassCount = 7;				// DeferredRender::RenderPass enum size
static WebProbeTerrainStats webProbeTerrain[webProbePassCount][2];
static WebProbeTerrainStats webProbeTerrainPrev[webProbePassCount][2];
static WebProbeTerrainStats* webProbeCur = NULL;	// layer currently rendering, NULL outside terrain
static int webProbeDrawCalls[webProbePassCount], webProbeDrawVerts[webProbePassCount];
static int webProbeDrawCallsPrev[webProbePassCount], webProbeDrawVertsPrev[webProbePassCount];
static int webProbeFrame = 0;
static int webProbeDumpCount = 0;
// gpu-side shadow map watcher: cpu counters proved the terrain is SUBMITTED on glitch
// frames, so the loss is after glDrawArrays. sample the real shadow-map pixels each
// frame; when content vanishes while verts were stable, auto-download the map as a png.
static int webProbeShadowFBO = -1;				// fbo bound during shadow-pass draws this frame
static bool webProbeShadowFBOMismatch = false;	// a shadow draw used a different fbo
static int webProbeShadowW = 0, webProbeShadowH = 0;
static float webProbeStripAlpha[3] = { -1, -1, -1 };	// mean alpha of 3 sample strips
static float webProbeStripLum[3] = { -1, -1, -1 };		// mean luminance of same strips
static float webProbeStripAlphaPrev[3] = { -1, -1, -1 };
static float webProbeStripLumPrev[3] = { -1, -1, -1 };
static float webProbePreLum[3] = { -1, -1, -1 };		// strips right after the shadow pass, BEFORE blur
static float webProbePreLumPrev[3] = { -1, -1, -1 };
static int webProbeForeignDraws = 0;	// draws into the shadow fbo from other passes (not blur)
static int webProbeForeignPassLast = -1;
static int webProbeBlurDraws = 0;		// legit blur-shader writes into the shadow fbo
static int webProbeCaptures = 0;
static int webProbeLastCaptureFrame = -1000;
// fnv1a hash of every vertex BYTE the terrain submits to the shadow pass: with a
// static camera this must be identical every frame. differing on a glitch frame
// proves cpu-side data corruption; identical proves the data left our hands intact.
static unsigned webProbeShadowHash = 2166136261u;
static unsigned webProbeShadowHashPrev = 0;

// mean alpha/luminance of 3 horizontal strips of the currently bound framebuffer
static void WebProbeReadStrips(float* alphaOut, float* lumOut)
{
	WebFlushSpriteBatch();	// readback barrier
	static unsigned char strip[2048 * 4 * 4];
	const int w = Min(webProbeShadowW, 2048);
	for (int s = 0; s < 3; ++s)
	{
		const int y = webProbeShadowH * (s + 1) / 4;
		glReadPixels(0, y, w, 4, GL_RGBA, GL_UNSIGNED_BYTE, strip);
		int alphaSum = 0, lumSum = 0;
		const int count = w * 4;
		for (int i = 0; i < count; ++i)
		{
			alphaSum += strip[i*4 + 3];
			lumSum += strip[i*4] + strip[i*4 + 1] + strip[i*4 + 2];
		}
		if (alphaOut)
			alphaOut[s] = alphaSum / (float)(count * 255);
		lumOut[s] = lumSum / (float)(count * 3 * 255);
	}
}

// after ApplyBlur's pointer swap this records the fbo of the texture the lights will
// ACTUALLY sample - the frame-start fbo becomes the shared swap slot, which other
// stages legitimately churn, so end-of-frame reads of it misattributed the loss
static int webProbeShadowFBOFinal = -1;
void WebFlickerProbeShadowFinal(LPDIRECT3DTEXTURE9 texture)
{
	WebFlushSpriteBatch();	// readback barrier
	webProbeShadowFBOFinal = (texture && texture->glFBO) ? (int)texture->glFBO : -1;
}

// the workaround for the below-API loss: the pre-blur strip read detects a bad shadow
// map in the SAME frame, before the lights sample it - so RenderShadowMap re-renders
// the pass when this returns true. proven safe to call repeatedly: each call compares
// the latest strip read against last frame's.
// off by default: this is driven by the probe's strip readbacks, so it cannot do
// anything while webShadowProbe is off (its guard tests both), and the bug it retried
// around is fixed
ConsoleCommandSimple(bool, webShadowRetry, false);
int webShadowRetryCount = 0;

// Render the shadow pass twice every frame (union, no clear between): dropped work
// windows kill ADJACENT commands together, so a second full pass lands hundreds of
// commands later in a different window. This was effective against the dropped-draw
// flicker, but it is another MITIGATION rather than the fix (WebUseRGB8Targets is), and
// it costs a whole extra scene pass every frame - cpu to submit and gpu to rasterize,
// on every platform. Off by default now; re-enable from the console if shadow flicker
// ever comes back.
int webShadowRender2 = 0;
ConsoleCommand(webShadowRender2, webShadowRender2);

// during the second pass only TERRAIN is drawn: opaque terrain is idempotent, but
// alpha-blended casters (particle shadows) double-darken when drawn twice - they
// flickered between 1x and 2x intensity until this filter existed
bool webShadowPass2Active = false;
// flicker bisection: strip systems AROUND the shadow pass while its chunk detector
// keeps counting drops (RETRIES in the heartbeat) as an objective sensor, so removing
// content no longer removes the ability to see the bug. bits:
//   1 = no per-light rendering (light textures / simple lights)
//   2 = no directional pass    4 = no emissive pass    8 = no final lighting composite
//  16 = no draws at all outside the shadow pass (maximum strip: shadow pass + present only)
int webStripMask = 0;
ConsoleCommand(webStripMask, webStripMask);
// gate for the minimap full-planet render (miniMap.cpp): ~524k tile draws in one frame
// at startup/reset. console webMapRender 0 or ?nomap skips it (map stays black) -
// flicker bisection knob + keeps gl-trace captures small
int webMapRender = 1;
ConsoleCommand(webMapRender, webMapRender);
static void WebCheckNoMapParam()
{
	static bool checked = false;
	if (checked) return;
	checked = true;
	if (EM_ASM_INT({ return ((location.search || "") + (location.hash || "")).indexOf("nomap") >= 0 ? 1 : 0; }))
	{
		webMapRender = 0;
		printf("webRender: minimap render disabled via ?nomap\n");
	}
}

// set whenever TerrainLayerRender is on the stack - identifies terrain batches for the
// pass-2 filter WITHOUT depending on the probe being enabled (webProbeCur is null when
// webShadowProbe is off, and pass 2 must keep working in a probe-stripped build)
bool webTerrainBatchActive = false;
// set during pass-2/retry re-renders (deferredRender.cpp) - freezes probe stat
// accumulation so verts/hash reflect pass 1 only. without this a retry frame records
// ~3x verts and the detector's verts-stable gate blinds itself for the NEXT frame,
// letting back-to-back glitch frames through
bool webShadowRerenderActive = false;

// chunked coverage for the bad-map detector: 8 rows x 16 chunks, compared chunk-wise
// so a small dropped region makes a LARGE local delta (3 whole-strip means diluted
// small regions below threshold - that is why retries fired but flicker survived)
static const int webChunkRows = 8, webChunkCols = 16;
static float webProbeChunks[webChunkRows][webChunkCols];
static float webProbeChunksPrev[webChunkRows][webChunkCols];
static bool webProbeChunksValid = false, webProbeChunksPrevValid = false;

static void WebReadShadowChunks()
{
	WebFlushSpriteBatch();	// readback barrier
	static unsigned char row[2048 * 4 * 4];
	const int w = Min(webProbeShadowW, 2048);
	const int chunkW = w / webChunkCols;
	for (int s = 0; s < webChunkRows; ++s)
	{
		const int y = webProbeShadowH * (s * 2 + 1) / (webChunkRows * 2);
		glReadPixels(0, y, w, 4, GL_RGBA, GL_UNSIGNED_BYTE, row);
		for (int c = 0; c < webChunkCols; ++c)
		{
			int lumSum = 0;
			const int x0 = c * chunkW;
			for (int line = 0; line < 4; ++line)
			for (int x = 0; x < chunkW; ++x)
			{
				const int o = (line * w + x0 + x) * 4;
				lumSum += row[o] + row[o+1] + row[o+2];
			}
			webProbeChunks[s][c] = lumSum / (float)(chunkW * 4 * 3 * 255);
		}
	}
	webProbeChunksValid = true;
}

bool WebShadowMapLooksBad()
{
	if (!webShadowRetry || !webShadowProbe || webProbeFrame < 120)
		return false;
	if (!webProbeChunksValid || !webProbeChunksPrevValid)
		return false;
	// only meaningful when the terrain submission was stable vs last frame
	const int LS = (int)DeferredRender::RenderPass_lightShadow;
	const int vNow = webProbeTerrain[LS][0].vertsSubmitted + webProbeTerrain[LS][1].vertsSubmitted;
	const int vPrev = webProbeTerrainPrev[LS][0].vertsSubmitted + webProbeTerrainPrev[LS][1].vertsSubmitted;
	if (vPrev < 300 || vNow * 4 < vPrev * 3 || vPrev * 4 < vNow * 3)
		return false;
	for (int s = 0; s < webChunkRows; ++s)
	for (int c = 0; c < webChunkCols; ++c)
	{
		const float d = fabsf(webProbeChunks[s][c] - webProbeChunksPrev[s][c]);
		if (d > 0.15f)
			return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
// async flicker meter: counts dropped-shadow-map events WITHOUT the synchronous
// readbacks of webShadowProbe. the sync probe's glReadPixels drain the pipeline every
// frame, which both costs ~35% fps and SUPPRESSES the bug ~10x - so a probe-on build
// cannot measure the shipping build's real drop rate. this meter reads the same 8x16
// chunk grid through a ring of PIXEL_PACK pbos with fences and only collects a capture
// once its fence has signaled (2-3 frames later), so the gpu is never stalled.
//
// detection is a V-test on three CONSECUTIVE captures instead of the probe's
// verts-stable gate (cpu-side submission stats are probe-gated and 2 frames stale
// here): a chunk that deviates hard for exactly one frame and then returns to its
// old value is a drop; real scene changes persist. prints one line per event:
//   FLICKERMETER DROPEVENT frame=N chunk=r,c delta=D
// and a heartbeat every 10s. webFlickerMeter 1 to enable (default off, zero cost).
////////////////////////////////////////////////////////////////////////////////////////

ConsoleCommandSimple(bool, webFlickerMeter, false);
// diagnostic: print gl errors from the meter's own calls, tagged by phase. sync
// (glGetError round trips) - only for debugging the meter itself, never for measuring
ConsoleCommandSimple(bool, webFlickerMeterDebug, false);
// how often the collect step runs. captures are still issued EVERY frame (consecutive
// frames are what the V-test needs) but their readbacks are drained in one burst every
// N frames, so the gpu-process round trips - the drain that makes the sync probe
// suppress the bug ~10x - happen 1/N as often. must stay < the slot count.
ConsoleCommandSimple(int, webMeterCollectEvery, 6);
// SELF-TEST: deliberately drop the terrain draws from the light-shadow pass on every
// Nth frame, simulating exactly what the bug does. a meter reporting zero events is
// only meaningful once it has been shown to catch these - run with e.g.
// webMeterSelfTest 120 and confirm events tracks frames/120. 0 = off.
ConsoleCommandSimple(int, webMeterSelfTest, 0);
// percent that submitted verts/draws may vary across the three compared frames and
// still count as "the scene did not change". 0 = exact equality (too strict: emitters
// jitter constantly), 5 = generous. tune with webMeterSelfTest to check detection.
ConsoleCommandSimple(int, webMeterStableTolerance, 2);
// one glGetError per frame while the meter runs (cheap; webCheckErrors is per-draw and
// far too slow). prints the first 40 errors with frame numbers so they can be lined up
// against DROPEVENT frames
ConsoleCommandSimple(bool, webMeterErrorCheck, true);
static int webMeterErrorsSeen = 0;
static int webMeterSelfTestDrops = 0;
// true on frames the self-test is suppressing shadow terrain (read by WebDraw)
static bool WebMeterSelfTestActive();
static void WebMeterCheckError(const char* where)
{
	if (!webFlickerMeterDebug)
		return;
	for (GLenum e; (e = glGetError()) != GL_NO_ERROR; )
		printf("FLICKERMETER glerror 0x%x at %s\n", e, where);
}

// the chunk grid is produced ON THE GPU by successive halving blits (each halving is
// an exact 2x2 box average under LINEAR filtering), so the whole shadow map is folded
// into a webMeterGrid x webMeterGrid image and read back in ONE small glReadPixels.
// reading strips directly was the first design and it is wrong on angle: several
// readPixels into one pbo per frame make angle discard its readback shadow copy
// ("READ-usage buffer was written, then fenced, but written again before being read
// back" x256/run), which is exactly the kind of stall this meter exists to avoid.
// 8 pbos, not 3: with 3 a buffer is rewritten ~2 frames after being read, and angle
// then reports "READ-usage buffer was written, then fenced, but written again before
// being read back" every frame - it discards the shadow copy that makes the readback
// cheap, which would turn each collect into a gpu-process round trip (the exact
// pipeline drain that makes the sync probe suppress the bug)
static const int webMeterSlotCount = 8;
static const int webMeterGrid = 16;
static const int webMeterGridBytes = webMeterGrid * webMeterGrid * 4;
static struct { GLuint pbo; GLsync fence; int frame; bool pending; int verts, draws; } webMeterSlot[webMeterSlotCount];
// cpu-side submission signature for the light-shadow pass, accumulated every frame with
// no gl cost. an event is only real if the geometry we SUBMITTED was identical on all
// three frames - otherwise a content change is the game animating (the start screen's
// fire and smoke light the same region the drops appear in), not a lost draw
static int webMeterShadowVerts = 0, webMeterShadowDraws = 0;
// reduction chain: fbos at 256,128,64,32,16 - only the ones <= the shadow map are used
static const int webMeterChainSizes[] = { 256, 128, 64, 32, webMeterGrid };
static const int webMeterChainCount = sizeof(webMeterChainSizes) / sizeof(webMeterChainSizes[0]);
static GLuint webMeterChainFBO[webMeterChainCount] = { 0 };
static GLuint webMeterChainTex[webMeterChainCount] = { 0 };
static bool webMeterInited = false;
static GLuint webMeterFBO = 0;			// shadow fbo seen this frame (0 = none yet)
static GLuint webMeterFBOQueried = 0;	// fbo whose viewport we last measured
static int webMeterW = 0, webMeterH = 0;
static int webMeterFrame = 0, webMeterEvents = 0, webMeterCaptures = 0;
// asynchrony evidence: how often the oldest capture's fence was NOT yet signaled, and
// the average frame lag between issuing a capture and collecting it. if notReady stays
// 0 and lag is 0 the readback is really synchronous - i.e. the meter is draining the
// pipeline and therefore suppressing the very bug it is supposed to count
static int webMeterNotReady = 0, webMeterLagSum = 0, webMeterCollected = 0;
// last three ANALYZED captures for the V-test (0 = oldest)
static float webMeterChunks[3][webMeterGrid][webMeterGrid];
static int webMeterChunkFrame[3] = { -9, -9, -9 };
static int webMeterChunkVerts[3] = { -1, -1, -1 };
static int webMeterChunkDraws[3] = { -1, -1, -1 };
// events rejected because submission was NOT stable across the three frames - i.e.
// the content really did change. a high number here means the raw pixel signal is
// dominated by animation and the gate is doing its job
static int webMeterUnstable = 0;

// called on every shadow-pass draw (probe on or off): remember the target. the
// viewport query is a possible sync round trip, so it runs only when the shadow fbo
// CHANGES (once per session in practice) - never per frame
static void WebMeterNoteShadowDraw()
{
	const GLuint fbo = webCurrentFBO;
	if (!fbo)
		return;
	if (fbo != webMeterFBOQueried)
	{
		GLint vp[4] = { 0 };
		glGetIntegerv(GL_VIEWPORT, vp);
		webMeterFBOQueried = fbo;
		webMeterW = vp[2];
		webMeterH = vp[3];
	}
	webMeterFBO = fbo;
}

static bool WebMeterSelfTestActive()
{
	return webMeterSelfTest > 0 && webFlickerMeter &&
		(webMeterFrame % webMeterSelfTest) == 0 && webMeterFrame > 0;
}

static void WebMeterAnalyze(const unsigned char* data, int capturedFrame, int verts, int draws)
{
	// shift history and compute the new capture's chunk grid
	memcpy(webMeterChunks[0], webMeterChunks[1], sizeof(webMeterChunks[0]));
	memcpy(webMeterChunks[1], webMeterChunks[2], sizeof(webMeterChunks[0]));
	webMeterChunkFrame[0] = webMeterChunkFrame[1];
	webMeterChunkFrame[1] = webMeterChunkFrame[2];
	webMeterChunkFrame[2] = capturedFrame;
	webMeterChunkVerts[0] = webMeterChunkVerts[1];
	webMeterChunkVerts[1] = webMeterChunkVerts[2];
	webMeterChunkVerts[2] = verts;
	webMeterChunkDraws[0] = webMeterChunkDraws[1];
	webMeterChunkDraws[1] = webMeterChunkDraws[2];
	webMeterChunkDraws[2] = draws;

	// one texel per chunk: the reduction chain already averaged everything
	for (int s = 0; s < webMeterGrid; ++s)
	for (int c = 0; c < webMeterGrid; ++c)
	{
		const int o = (s * webMeterGrid + c) * 4;
		webMeterChunks[2][s][c] = (data[o] + data[o+1] + data[o+2]) / (3 * 255.0f);
	}

	// V-test on the middle capture: needs three consecutive frames
	if (webMeterChunkFrame[1] != webMeterChunkFrame[0] + 1 ||
		webMeterChunkFrame[2] != webMeterChunkFrame[1] + 1)
		return;
	for (int s = 0; s < webMeterGrid; ++s)
	for (int c = 0; c < webMeterGrid; ++c)
	{
		const float c0 = webMeterChunks[0][s][c], c1 = webMeterChunks[1][s][c], c2 = webMeterChunks[2][s][c];
		if (fabsf(c1 - c0) > 0.15f && fabsf(c2 - c1) > 0.10f && fabsf(c2 - c0) < 0.06f)
		{
			// the pixels changed and came back. only a lost draw if what we submitted
			// stayed put across all three frames - otherwise the game animated.
			// exact equality is too strict: live particle emitters jitter the count
			// by a few tenths of a percent every frame, which is nowhere near enough
			// geometry to explain a half-luminance swing in a whole chunk
			const int vMin = Min(webMeterChunkVerts[0], Min(webMeterChunkVerts[1], webMeterChunkVerts[2]));
			const int vMax = Max(webMeterChunkVerts[0], Max(webMeterChunkVerts[1], webMeterChunkVerts[2]));
			const int dMin = Min(webMeterChunkDraws[0], Min(webMeterChunkDraws[1], webMeterChunkDraws[2]));
			const int dMax = Max(webMeterChunkDraws[0], Max(webMeterChunkDraws[1], webMeterChunkDraws[2]));
			if (vMin <= 0 || (vMax - vMin) * 100 > vMax * webMeterStableTolerance ||
				(dMax - dMin) * 100 > dMax * webMeterStableTolerance)
			{
				++webMeterUnstable;
				return;
			}
			++webMeterEvents;
			printf("FLICKERMETER DROPEVENT frame=%d chunk=%d,%d delta=%.2f verts=%d draws=%d\n",
				webMeterChunkFrame[1], s, c, c1 - c0, webMeterChunkVerts[1], webMeterChunkDraws[1]);
			return;		// one event per frame
		}
	}
}

// runs right after the shadow pass each frame, before blur - the same point the sync
// probe reads at, so the two measure the same content
static void WebFlickerMeterUpdate()
{
	if (!webFlickerMeter || !webMeterFBO || webMeterW <= 8 || webMeterH <= 8)
		return;

	// ONE glGetError per frame (webCheckErrors does one per DRAW, ~185 sync round trips
	// a frame, which drops the game to a crawl and drains the pipeline - useless here).
	// This is looking for GL_INVALID_OPERATION from "Feedback loop formed between
	// Framebuffer and active Texture": webgl DISCARDS such a draw entirely, which would
	// produce missing content with correct geometry and no visible error - our exact
	// symptom. Frank's chrome://gpu dump contains 256 of them from a webgl context.
	if (webMeterErrorCheck)
	{
		const GLenum err = glGetError();
		if (err != GL_NO_ERROR && webMeterErrorsSeen < 40)
		{
			++webMeterErrorsSeen;
			printf("FLICKERMETER GLERROR frame=%d err=0x%x%s\n", webMeterFrame, err,
				err == GL_INVALID_OPERATION ? "  <-- INVALID_OPERATION (feedback loop?)" : "");
		}
	}

	WebFlushSpriteBatch();		// the reduction must see the finished shadow pass

	if (!webMeterInited)
	{
		webMeterInited = true;
		for (int i = 0; i < webMeterSlotCount; ++i)
		{
			glGenBuffers(1, &webMeterSlot[i].pbo);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, webMeterSlot[i].pbo);
			glBufferData(GL_PIXEL_PACK_BUFFER, webMeterGridBytes, NULL, GL_STREAM_READ);
			webMeterSlot[i].pending = false;
			webMeterSlot[i].fence = 0;
		}
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		for (int i = 0; i < webMeterChainCount; ++i)
		{
			const int size = webMeterChainSizes[i];
			glGenTextures(1, &webMeterChainTex[i]);
			glBindTexture(GL_TEXTURE_2D, webMeterChainTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glGenFramebuffers(1, &webMeterChainFBO[i]);
			glBindFramebuffer(GL_FRAMEBUFFER, webMeterChainFBO[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				webMeterChainTex[i], 0);
		}
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, webCurrentFBO);
		WebClearSamplers();		// raw texture binds above bypassed the sampler tracking
		WebMeterCheckError("init");
	}

	// collect every capture whose fence has signaled, oldest first, so analyzed
	// captures stay in frame order. glClientWaitSync with timeout 0 never blocks.
	static unsigned char cpuBuf[webMeterGridBytes];
	const int collectEvery = Max(1, Min(webMeterCollectEvery, webMeterSlotCount - 1));
	for (int step = 0; (webMeterFrame % collectEvery) == 0 && step < webMeterSlotCount; ++step)
	{
		int oldest = -1;
		for (int i = 0; i < webMeterSlotCount; ++i)
			if (webMeterSlot[i].pending && (oldest < 0 || webMeterSlot[i].frame < webMeterSlot[oldest].frame))
				oldest = i;
		if (oldest < 0)
			break;
		const GLenum r = glClientWaitSync(webMeterSlot[oldest].fence, 0, 0);
		WebMeterCheckError("clientWaitSync");
		if (r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED)
		{
			++webMeterNotReady;
			break;
		}
		webMeterLagSum += webMeterFrame - webMeterSlot[oldest].frame;
		++webMeterCollected;
		glDeleteSync(webMeterSlot[oldest].fence);
		webMeterSlot[oldest].fence = 0;
		webMeterSlot[oldest].pending = false;
		glBindBuffer(GL_PIXEL_PACK_BUFFER, webMeterSlot[oldest].pbo);
		// emscripten's glMapBufferRange read support is version-dependent; call the
		// webgl2 api directly. the fence already signaled, so no gpu wait remains.
		EM_ASM({ GLctx.getBufferSubData(0x88EB, 0, HEAPU8.subarray($0, $0 + $1)); },
			(int)(intptr_t)cpuBuf, webMeterGridBytes);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		WebMeterCheckError("getBufferSubData");
		WebMeterAnalyze(cpuBuf, webMeterSlot[oldest].frame,
			webMeterSlot[oldest].verts, webMeterSlot[oldest].draws);
	}

	// issue this frame's capture if its ring slot is free (if the gpu is 3+ frames
	// behind we skip a sample rather than ever waiting on it)
	const int slot = webMeterFrame % webMeterSlotCount;
	if (!webMeterSlot[slot].pending)
	{
		// GPU reduction: shadow map -> first chain level <= its width, then halve to
		// the grid. LINEAR halving is an exact 2x2 box average, so the final texel
		// grid is a true mean of the whole map - no cpu-side pixel walking.
		int level = 0;
		while (level + 1 < webMeterChainCount && webMeterChainSizes[level] > webMeterW)
			++level;
		int srcW = webMeterW, srcH = webMeterH;
		GLuint srcFBO = webMeterFBO;
		for (; level < webMeterChainCount; ++level)
		{
			const int size = webMeterChainSizes[level];
			glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, webMeterChainFBO[level]);
			glBlitFramebuffer(0, 0, srcW, srcH, 0, 0, size, size,
				GL_COLOR_BUFFER_BIT, GL_LINEAR);
			srcFBO = webMeterChainFBO[level];
			srcW = srcH = size;
		}
		WebMeterCheckError("blit");

		// read the final tiny level: ONE readPixels, 1KB, straight into the pbo
		glBindFramebuffer(GL_FRAMEBUFFER, webMeterChainFBO[webMeterChainCount - 1]);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, webMeterSlot[slot].pbo);
		glReadPixels(0, 0, webMeterGrid, webMeterGrid, GL_RGBA, GL_UNSIGNED_BYTE, (void*)0);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		WebMeterCheckError("readPixels");
		webMeterSlot[slot].fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		webMeterSlot[slot].frame = webMeterFrame;
		webMeterSlot[slot].verts = webMeterShadowVerts;
		webMeterSlot[slot].draws = webMeterShadowDraws;
		webMeterSlot[slot].pending = true;
		++webMeterCaptures;
		// restore the caller's target directly: WebBindFBO would flush a batch that
		// cannot exist here, and READ/DRAW were split above
		glBindFramebuffer(GL_FRAMEBUFFER, webCurrentFBO);
		WebMeterCheckError("restore");
	}

	webMeterShadowVerts = 0;
	webMeterShadowDraws = 0;
	++webMeterFrame;
	if (webMeterFrame % 600 == 0)
		printf("FLICKERMETER frames=%d captures=%d events=%d unstable=%d notReady=%d avgLag=%.2f selfTestDrops=%d\n",
			webMeterFrame, webMeterCaptures, webMeterEvents, webMeterUnstable, webMeterNotReady,
			webMeterCollected ? webMeterLagSum / (float)webMeterCollected : 0.f,
			webMeterSelfTestDrops);
}

// called from RenderShadowMap right after the light-shadow pass renders, before the
// blur and directional stages - pre/post comparison isolates which stage loses content
void WebFlickerProbeShadowPassDone()
{
	WebFlickerMeterUpdate();

	// flush workaround runs even with the probe off
	if (webShadowFlush == 1 || webShadowFlush == 3)
		glFlush();
	else if (webShadowFlush == 2)
		glFinish();

	if (!webShadowProbe || webProbeShadowFBO < 0 || webProbeShadowW <= 8 || webProbeShadowH <= 8)
		return;
	GLint prevFBO = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
	WebBindFBO((GLuint)webProbeShadowFBO);
	WebProbeReadStrips(NULL, webProbePreLum);
	WebReadShadowChunks();
	WebBindFBO((GLuint)prevFBO);
}

////////////////////////////////////////////////////////////////////////////////////////
// sprite batcher
////////////////////////////////////////////////////////////////////////////////////////
// The d3d-shaped draw path sends one draw call per sprite: the unit quad in a static
// vao, with the world transform, uv transform and color as uniforms. That is ~570 draws
// a frame on the start screen and ~1300 in gameplay, and a survey (webBatchSurvey) found
// ~88% of them are immediately preceded by another sprite draw that differs ONLY in
// those three uniforms. Folding all three into the vertex stream turns each such run
// into one draw.
//
// Merging is restricted to CONSECUTIVE draws - alpha blending is order dependent, so a
// run broken by a streamed draw, a render-target change or a readback cannot be
// recovered, and every one of those is a hard flush point.
//
// The batch is only correct if the state it was accumulated under is re-applied at flush
// time, so the draw path is split: WebResolveDraw does the pure computation (no gl at
// all), then either the state is applied and the draw issued, or the resolved state is
// stored and the geometry queued. Nothing in between may touch gl.
// WORKAROUND for the chrome/angle-d3d11 dropped-draw bug, and the mechanism behind it.
//
// Chrome runs ANGLE's WebGL device on the SAME ID3D11Device + immediate context that its
// video decode, compositing and SharedImage code use. Those subsystems issue raw d3d11
// calls on it. ANGLE shadows d3d11 binding state to skip redundant work, and the shader
// resource (texture) leg of that shadow is guarded by a RAW POINTER COMPARE:
//     if (record.view != reinterpret_cast<uintptr_t>(srv)) { PSSetShaderResources(...) }
// When external code changes what is really bound - or d3d11 itself nulls an srv slot
// because the same texture got bound as a render target - angle's shadow goes stale, the
// compare says "already bound", the re-bind is SKIPPED, and the draw samples a null srv.
// That is the observed signature exactly: content missing or wrong-colored, geometry
// never misplaced (the input-layout leg is guarded by a monotonic serial, which external
// code cannot fake), zero gl errors, d3d11 only.
//
// PROVEN, not theorized: running chrome with
//     --force-separate-egl-display-for-webgl-testing --enable-features=EGLDualGPURendering
// gives WebGL its own d3d11 device (firefox's topology, and firefox is immune) and the
// measured drop rate goes 661 -> 0 events per 120s under identical contention, at
// identical frame rate. We cannot ship chrome flags to players, so we defeat the pointer
// compare ourselves: bind a scratch texture, then the real one. The srv pointer provably
// changes, so angle must re-issue the bind, and any stale shadow is repaired.
// 0 = off, 1 = shadow passes only (the visible victim), 2 = every draw.
ConsoleCommandSimple(int, webTexturePingPong, 0);

// WORKAROUND 2, aimed at the same mechanism but from the other end: instead of forcing
// ONE binding to be re-issued, force angle to resync EVERYTHING.
//
// angle rebuilds its entire d3d11 binding state whenever the gl context is made current
// again - Context::makeCurrent does mState.setAllDirtyBits() + setAllDirtyObjects(),
// and StateManager11::onMakeCurrent drops its cached executable/vertex-array/framebuffer.
// Nothing else in angle has an invalidateEverything(). We cannot call makeCurrent from
// webgl... but chrome's gpu process makes a context current whenever it switches between
// command buffers, so touching a SECOND webgl context from javascript makes chrome switch
// away and back, and our next draw runs against freshly synced state.
// 0 = off, 1 = once per frame, 2 = at every render target change (worst case ~10/frame).
int webContextPump = 0;
ConsoleCommand(webContextPump, webContextPump);
static int webContextPumps = 0;

// force a real OMSetRenderTargets at every render target change (see WebBindFBO)
int webFBOPingPong = 0;
ConsoleCommand(webFBOPingPong, webFBOPingPong);

// NOTE: no backslashes in EM_JS bodies - the c preprocessor eats them (see frankEngineWeb)
EM_JS(void, WebJsContextPump, (), {
	if (!window.webPumpGL)
	{
		var c = document.createElement("canvas");
		c.width = 1;
		c.height = 1;
		window.webPumpGL = c.getContext("webgl2", { alpha: false, depth: false,
			stencil: false, antialias: false, preserveDrawingBuffer: false });
		if (!window.webPumpGL)
			return;
		window.webPumpGL.clearColor(0, 0, 0, 1);
	}
	var g = window.webPumpGL;
	// a real draw-ish command, so the gpu process must schedule this context's command
	// buffer rather than coalescing a no-op, then flush so the switch happens now
	g.clear(g.COLOR_BUFFER_BIT);
	g.flush();
});

static void WebContextPump()
{
	WebJsContextPump();
	++webContextPumps;
	// touching another context invalidates every binding assumption this file makes
	WebResetBindCache();
	WebResetBlendCache();
}

ConsoleCommandSimple(bool, webSpriteBatch, true);
// EXPERIMENT (flicker derby D1): 0 = never batch during the shadow passes. every quad
// there falls back to the STATIC unit-quad vao + uniforms - uploaded once at startup,
// direct-bound by angle, zero bytes through the pool or angle's streaming ring. with
// the terrain already on static cached vaos, the shadow map is then built almost
// entirely from static vertex data; if the drops stop, the streaming path is the
// mechanism and this (plus webPoolRespec for the remaining vert-list casters) is the fix
ConsoleCommandSimple(bool, webBatchShadowPass, true);

struct WebSamplerState { GLenum mag, minify, wrapS, wrapT; };

static WebSamplerState WebResolveSamplerState(const FrankWebTexture* t, int unit)
{
	const FrankWebDeviceState& s = FrankWebGetDeviceState();
	WebSamplerState out;
	const DWORD magFilter = s.samplerStates[unit][5];
	const DWORD minFilter = s.samplerStates[unit][6];
	const DWORD mipFilter = s.samplerStates[unit][7];
	out.mag = (magFilter == 1) ? GL_NEAREST : GL_LINEAR;
	if (minFilter == 1)
		out.minify = GL_NEAREST;
	else if (t && t->hasMipmaps && mipFilter != 0)
		out.minify = GL_LINEAR_MIPMAP_LINEAR;
	else
		out.minify = GL_LINEAR;

	const DWORD addressU = s.samplerStates[unit][1];
	const DWORD addressV = s.samplerStates[unit][2];
	// BORDER(4) has no GL analogue - clamp to edge, the shader blacks out-of-range uvs
	out.wrapS = (addressU == 2) ? GL_MIRRORED_REPEAT : (addressU >= 3) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	out.wrapT = (addressV == 2) ? GL_MIRRORED_REPEAT : (addressV >= 3) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	return out;
}

// Sampler objects. The d3d-shaped path re-sends filter and wrap state as four
// glTexParameteri calls per texture per draw, because in d3d sampler state is per stage
// and independent of the texture, while in GL it lives ON the texture object. WebGL2 has
// the right primitive for this: a sampler object bound to a texture unit overrides the
// texture's own parameters, so the whole set becomes one glBindSampler - and zero calls
// when the binding is already correct, which it almost always is.
//
// The game only ever uses a handful of distinct combinations, so they are created once
// and cached. Binding 0 means "use the texture's own parameters", which is what the
// fallback white texture needs (it has no mipmaps, and a mipmap min-filter on it would
// make it incomplete and sample black).
ConsoleCommandSimple(bool, webSamplerObjects, true);

static const int webSamplerCacheMax = 16;
static struct { WebSamplerState state; GLuint sampler; } webSamplerCache[webSamplerCacheMax];
static int webSamplerCacheCount = 0;
static const int webSamplerUnitCount = 3;	// unit 2 is the light shaders' shadow map / gel
static GLuint webBoundSampler[webSamplerUnitCount] = { 0, 0, 0 };

static GLuint WebGetSamplerObject(const WebSamplerState& want)
{
	for (int i = 0; i < webSamplerCacheCount; ++i)
	{
		if (memcmp(&webSamplerCache[i].state, &want, sizeof(want)) == 0)
			return webSamplerCache[i].sampler;
	}

	GLuint sampler = 0;
	glGenSamplers(1, &sampler);
	glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, want.mag);
	glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, want.minify);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, want.wrapS);
	glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, want.wrapT);
	if (webSamplerCacheCount < webSamplerCacheMax)
	{
		webSamplerCache[webSamplerCacheCount].state = want;
		webSamplerCache[webSamplerCacheCount].sampler = sampler;
		++webSamplerCacheCount;
	}
	return sampler;
}

static void WebBindSamplerUnit(int unit, GLuint sampler)
{
	if (webBoundSampler[unit] == sampler)
		return;
	glBindSampler(unit, sampler);
	webBoundSampler[unit] = sampler;
}

// every path that binds a texture without going through WebApplyResolvedDraw must clear
// the unit's sampler, or it inherits whichever one the last draw left bound
static void WebClearSamplers()
{
	for (int unit = 0; unit < webSamplerUnitCount; ++unit)
		WebBindSamplerUnit(unit, 0);
}

// a fresh context invalidates every sampler name we handed out
void WebResetSamplerCache()
{
	webSamplerCacheCount = 0;
	for (int unit = 0; unit < webSamplerUnitCount; ++unit)
		webBoundSampler[unit] = 0;
}

static void WebApplySampler(int unit, const WebSamplerState& sampler)
{
	if (webSamplerObjects)
	{
		WebBindSamplerUnit(unit, WebGetSamplerObject(sampler));
		return;
	}
	WebBindSamplerUnit(unit, 0);	// fall back to per-texture parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampler.mag);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampler.minify);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
}

// everything a draw needs, computed from the arguments and the fake device state with no
// gl calls at all
struct WebResolvedDraw
{
	GLuint program;
	const WebProgramLocs* locs;
	float wvp[16];			// world*view*proj, or view*proj alone when the world is baked
	float uv[16];
	float uv2[16];
	Color color;
	int flags;
	FrankWebTexture* tex0;
	FrankWebTexture* tex1;
	FrankWebTexture* tex2;		// third sampler, light shaders only (uTexture3)
	WebSamplerState sampler0, sampler1, sampler2;
	WebBlendState blend;
};

// what two draws must agree on to share a batch. the world transform, uv transform and
// color are deliberately absent - those are exactly what gets baked into the vertices
static bool WebBatchKeyMatches(const WebResolvedDraw& a, const WebResolvedDraw& b)
{
	return a.program == b.program && a.flags == b.flags &&
		a.tex0 == b.tex0 && a.tex1 == b.tex1 && a.tex2 == b.tex2 &&
		memcmp(a.wvp, b.wvp, sizeof(a.wvp)) == 0 &&
		memcmp(&a.sampler0, &b.sampler0, sizeof(a.sampler0)) == 0 &&
		memcmp(&a.sampler1, &b.sampler1, sizeof(a.sampler1)) == 0 &&
		memcmp(&a.sampler2, &b.sampler2, sizeof(a.sampler2)) == 0 &&
		memcmp(&a.blend, &b.blend, sizeof(a.blend)) == 0;
}

// `bakedUniforms` neutralizes the three per-draw uniforms because the batch carries them
// in the vertex stream instead: uColor.rgb * vColor.rgb is the same product either way
static void WebApplyResolvedDraw(const WebResolvedDraw& d, bool bakedUniforms)
{
	const WebProgramLocs* locs = d.locs;
	WebBindProgram(d.program);
	glUniformMatrix4fv(locs->worldViewProj, 1, GL_FALSE, d.wvp);
	if (locs->uvMatrix >= 0)
		glUniformMatrix4fv(locs->uvMatrix, 1, GL_FALSE, bakedUniforms ? webIdentity : d.uv);
	if (locs->color >= 0)
	{
		if (bakedUniforms)
			glUniform4f(locs->color, 1, 1, 1, 1);
		else
			glUniform4f(locs->color, d.color.r, d.color.g, d.color.b, d.color.a);
	}
	if (locs->uvMatrix2 >= 0)
		glUniformMatrix4fv(locs->uvMatrix2, 1, GL_FALSE, bakedUniforms ? webIdentity : d.uv2);
	if (locs->flags >= 0)
		glUniform1i(locs->flags, d.flags);

	glActiveTexture(GL_TEXTURE0);
	if (webTexturePingPong == 2 ||
		(webTexturePingPong == 1 && DeferredRender::GetRenderPassIsShadow()))
	{
		// WORKAROUND for the chrome/angle-d3d11 dropped-draw bug (see the block comment
		// on webTexturePingPong): bind a scratch texture first so the real bind cannot
		// be elided by angle's pointer compare, forcing a fresh PSSetShaderResources
		glBindTexture(GL_TEXTURE_2D, webPingPongTexture);
		++webPingPongBinds;
	}
	glBindTexture(GL_TEXTURE_2D, (d.tex0 && d.tex0->glTexture) ? d.tex0->glTexture : webWhiteTexture);
	if (d.tex0 && d.tex0->glTexture)
		WebApplySampler(0, d.sampler0);
	else
		WebBindSamplerUnit(0, 0);	// white fallback keeps its own (non-mipmapped) params
	if (locs->texture >= 0)
		glUniform1i(locs->texture, 0);

	if (webFixFeedback && !d.tex1)
	{
		// scrub unit 1: the shader always declares uTexture2, so webgl counts whatever
		// is bound there as "sampled" even when uFlags skips it. a STALE texture left
		// on unit 1 that is attached to the current framebuffer = feedback = the draw
		// is skipped. bind white so unit 1 can never alias the render target.
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, webWhiteTexture);
		WebBindSamplerUnit(1, 0);
		glActiveTexture(GL_TEXTURE0);
	}
	if (d.tex1 && locs->texture2 >= 0)
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, d.tex1->glTexture ? d.tex1->glTexture : webWhiteTexture);
		if (d.tex1->glTexture)
			WebApplySampler(1, d.sampler1);
		else
			WebBindSamplerUnit(1, 0);
		glUniform1i(locs->texture2, 1);
		glActiveTexture(GL_TEXTURE0);
	}

	if (locs->texture3 >= 0)
	{
		// the light shaders declare three samplers, so unit 2 must always hold something
		// real - an unbound (or stale, framebuffer-attached) texture there is a feedback
		// loop and webgl silently skips the whole draw
		glActiveTexture(GL_TEXTURE2);
		const bool haveTex2 = d.tex2 && d.tex2->glTexture;
		glBindTexture(GL_TEXTURE_2D, haveTex2 ? d.tex2->glTexture : webWhiteTexture);
		if (haveTex2)
			WebApplySampler(2, d.sampler2);
		else
			WebBindSamplerUnit(2, 0);
		glUniform1i(locs->texture3, 2);
		glActiveTexture(GL_TEXTURE0);
	}

	WebSetBlendEnabled(d.blend.enabled != 0);
	if (d.blend.enabled)
	{
		WebSetBlendFunc(d.blend.src, d.blend.dst);
		WebSetBlendEquation(d.blend.eq);
	}
}

static const int webBatchMaxQuads = 1024;
static WebTexVertex webBatchVerts[webBatchMaxQuads * 6];
static int webBatchQuads = 0;
static WebResolvedDraw webBatchState;
static int webBatchFlushes = 0, webBatchQuadsQueued = 0;
// how often a quad draw was kept out of a batch purely because its color would not
// survive the 8-bit vertex pack. expected to be 0 in this game - if it stays 0 the guard
// is free insurance, and if it does not, it is catching a real clipping regression
static int webBatchUnbakeableColors = 0;

void WebFlushSpriteBatch()
{
	if (!webBatchQuads)
		return;

	const int count = webBatchQuads * 6;
	webBatchQuads = 0;		// cleared first - the draw below must never re-enter this

	WebApplyResolvedDraw(webBatchState, true);
	const int first = webTexPool.Upload(webBatchVerts, count);
	for (int r = Max(1, webDrawRepeat); r--; )
		glDrawArrays(GL_TRIANGLES, first, count);
	WebCheckDrawError("spritebatch", webBatchState.tex0);
	if (webShadowFlush >= 4)
		glFlush();
	WebCountDrawForFlush();
	++webTotalDraws;
	++webBatchFlushes;
}

// append the unit quad, transformed into world space with its uvs and color folded in.
// row-vector convention throughout, matching WebMatMul and the row-major uniform upload
static void WebBatchAppendQuad(const float* world, const float* uv, const Color& color)
{
	// matches webQuadVAO exactly: +-1, uv y-down. as two triangles instead of a strip
	static const float corners[4][4] =
	{
		{ -1,  1,  0, 0 },
		{  1,  1,  1, 0 },
		{ -1, -1,  0, 1 },
		{  1, -1,  1, 1 },
	};
	static const int order[6] = { 0, 1, 2, 2, 1, 3 };

	const DWORD c = (DWORD)color;
	const unsigned char cr = (unsigned char)((c >> 16) & 0xff);
	const unsigned char cg = (unsigned char)((c >> 8) & 0xff);
	const unsigned char cb = (unsigned char)(c & 0xff);
	const unsigned char ca = (unsigned char)((c >> 24) & 0xff);

	WebTexVertex* out = &webBatchVerts[webBatchQuads * 6];
	for (int i = 0; i < 6; ++i)
	{
		const float x = corners[order[i]][0], y = corners[order[i]][1];
		const float u = corners[order[i]][2], v = corners[order[i]][3];
		WebTexVertex& vert = out[i];
		// quad z is 0, so the third row of the world matrix drops out
		vert.x = x*world[0] + y*world[4] + world[12];
		vert.y = x*world[1] + y*world[5] + world[13];
		vert.z = x*world[2] + y*world[6] + world[14];
		// the shader samples with vec4(aUV, 1.0, 1.0), so rows 3 AND 4 both translate
		vert.u = u*uv[0] + v*uv[4] + uv[8] + uv[12];
		vert.v = u*uv[1] + v*uv[5] + uv[9] + uv[13];
		vert.r = cr; vert.g = cg; vert.b = cb; vert.a = ca;
	}
	++webBatchQuads;
	++webBatchQuadsQueued;
}

// a color outside [0,1] cannot survive the saturating 8-bit vertex pack, and overbright
// colors are real here (additive glows, DeferredRender overbright) - those draw alone
static bool WebColorIsBakeable(const Color& color)
{
	return color.r >= 0 && color.r <= 1 && color.g >= 0 && color.g <= 1 &&
		color.b >= 0 && color.b <= 1 && color.a >= 0 && color.a <= 1;
}

// vertex source: either `verts` (streamed through the pool) or a pre-built static buffer
// (cachedVAO + cachedFirst, used by the terrain cache) or, with both null/zero, the unit
// quad. everything upstream of the draw call itself is identical in all three cases.
static void WebDraw(const float* worldMatrix, const Color& colorIn, FrankWebTexture* tex0,
	FrankWebTexture* tex1, const float* uvMatrix, int extraFlags, bool materialColor,
	const WebTexVertex* verts, int vertCount, GLenum mode,
	GLuint cachedVAO = 0, int cachedFirst = 0)
{
	if (!webGLContext)
		return;

	const bool hasVerts = (verts != NULL) || (cachedVAO != 0);

	// anything already in the error queue was raised OUTSIDE a draw - tag it as such
	// so the post-draw check below is guaranteed to be about THIS draw
	WebDrainErrors("pre-draw");

	// union pass 2: terrain batches only (see webShadowPass2Active)
	if (webShadowPass2Active && !(hasVerts && webTerrainBatchActive))
		return;

	// bisection maximum strip: only the shadow pass (and its sensor) draws anything
	if ((webStripMask & 16) && !DeferredRender::GetRenderPassIsShadow())
		return;

	if (webDrawSkipMask)
	{
		// bisection: drop this draw if its category is masked out
		const int bit = hasVerts ? (webTerrainBatchActive ? 1 : 2) : 4;
		if (webDrawSkipMask & bit)
			return;
	}

	// the async meter needs the shadow target discovered even with the probe off, plus
	// a cpu-side signature of what this frame submitted (its stability gate)
	if (webFlickerMeter && !webShadowRerenderActive &&
		(int)DeferredRender::GetRenderPass() == (int)DeferredRender::RenderPass_lightShadow)
	{
		WebMeterNoteShadowDraw();
		webMeterShadowVerts += hasVerts ? vertCount : 4;
		++webMeterShadowDraws;
	}

	// meter self-test: simulate the bug by losing shadow terrain for one frame. this
	// runs AFTER the submission counters on purpose - the real bug loses work after
	// glDrawArrays, so a faithful simulation must still count as submitted
	if (webMeterSelfTest > 0 && webTerrainBatchActive && !webShadowRerenderActive &&
		DeferredRender::GetRenderPassIsLight() && WebMeterSelfTestActive())
	{
		++webMeterSelfTestDrops;
		return;
	}

	if (webShadowProbe && !webShadowRerenderActive)
	{
		// count every draw per pass - discriminates "terrain missing" from "everything missing"
		const int pass = (int)DeferredRender::GetRenderPass();
		if (pass >= 0 && pass < webProbePassCount)
		{
			++webProbeDrawCalls[pass];
			webProbeDrawVerts[pass] += hasVerts ? vertCount : 4;
		}
		if (pass == (int)DeferredRender::RenderPass_lightShadow)
		{
			// remember which framebuffer the shadow pass renders into (and its size),
			// and flag any draw that lands on a different target mid-pass
			const GLint fbo = (GLint)webCurrentFBO;
			if (webProbeShadowFBO < 0)
			{
				webProbeShadowFBO = fbo;
				GLint vp[4] = { 0 };
				glGetIntegerv(GL_VIEWPORT, vp);
				webProbeShadowW = vp[2];
				webProbeShadowH = vp[3];
			}
			else if (fbo != webProbeShadowFBO)
				webProbeShadowFBOMismatch = true;
		}
		else if (webProbeShadowFBO >= 0)
		{
			// any OTHER pass drawing into the shadow fbo: the blur shader is the only
			// legitimate writer - anything else is scribbling over the shadow map
			const GLint fbo = (GLint)webCurrentFBO;
			if (fbo == webProbeShadowFBO)
			{
				const FrankWebDeviceState& ds = FrankWebGetDeviceState();
				if (ds.pixelShader && ds.pixelShader->glProgram == webBlurProgram)
					++webProbeBlurDraws;
				else
				{
					++webProbeForeignDraws;
					webProbeForeignPassLast = pass;
				}
			}
		}
	}

	// ---- resolve: pure computation from the arguments and the fake device state.
	//      NOTHING below may touch gl until the batch decision has been made, or a
	//      pending sprite batch would be drawn with this draw's state.
	const FrankWebDeviceState& s = FrankWebGetDeviceState();

	WebResolvedDraw draw;
	const bool usesPixelShader = (s.pixelShader && s.pixelShader->glProgram) != 0;
	draw.program = usesPixelShader ? s.pixelShader->glProgram : webProgram;
	draw.locs = usesPixelShader ? WebFindProgramLocs(draw.program) : &webLocs;

	// uv transform: explicit > captured texture transform (scrolling blocks) > identity
	const float* uv = uvMatrix;
	if (!uv)
		uv = (s.textureStageStates[0][24] != 0) ? s.textureTransform : webIdentity;

	draw.color = materialColor ? WebApplyMaterialColor(colorIn) : colorIn;
	draw.flags = WebGetStateFlags() | extraFlags;

	// device-driven second stage (minimap mask composite): a stage-1 texture set to
	// MODULATE multiplies stage 0, sampling through its own uv transform (the fake
	// device has one shared texture-transform slot; stage 1 was the last writer)
	const float* uv2 = uv;
	if (!tex1 && s.textures[1] && s.textureStageStates[1][1] == 4)	// COLOROP = MODULATE
	{
		tex1 = s.textures[1];
		if (s.textureStageStates[1][24] != 0)
			uv2 = s.textureTransform;
		if (s.samplerStates[1][1] == 4 || s.samplerStates[1][2] == 4)	// BORDER addressing
			draw.flags |= WebFlag_BorderClamp2;
	}
	if (tex1)
		draw.flags |= WebFlag_Modulate2;
	memcpy(draw.uv, uv, sizeof(draw.uv));
	memcpy(draw.uv2, uv2, sizeof(draw.uv2));

	// feedback-loop guard: sampling a texture ATTACHED TO the framebuffer being drawn
	// into is invalid in webgl - the draw is silently skipped and renders nothing
	// (confirmed live: INVALID_OPERATION on light-pass quads). d3d9 allowed this, so
	// the deferred renderer's texture ping-pong can alias source and destination.
	// compare by actual gl framebuffer identity, not object pointers - the swap dance
	// hides the aliasing at the pointer level.
	if (webFixFeedback)
	{
		const GLint curFBO = (GLint)webCurrentFBO;	// tracked, not queried (see WebBindFBO)
		if (curFBO)
		{
			if (tex0 && tex0->glFBO && tex0->glFBO == (GLuint)curFBO)
			{
				if (webFeedbackHits < 20)
					printf("FEEDBACK: tex0 %u is attached to draw fbo %d (pass %d) - substituted white\n",
						tex0->glTexture, curFBO, (int)DeferredRender::GetRenderPass());
				++webFeedbackHits;
				tex0 = NULL;
			}
			if (tex1 && tex1->glFBO && tex1->glFBO == (GLuint)curFBO)
			{
				if (webFeedbackHits < 20)
					printf("FEEDBACK: tex1 %u is attached to draw fbo %d (pass %d) - substituted white\n",
						tex1->glTexture, curFBO, (int)DeferredRender::GetRenderPass());
				++webFeedbackHits;
				tex1 = NULL;
			}
		}
	}
	// The light shaders take all three inputs straight off the device stages, the way
	// deferredRender.cpp binds them: 0 = normal buffer, 1 = specular buffer, 2 = shadow
	// map (point light) or light gel. Only they declare uTexture3, so keying off that
	// location leaves every other draw path untouched.
	FrankWebTexture* tex2 = NULL;
	if (draw.locs->texture3 >= 0)
	{
		if (!tex1)
			tex1 = s.textures[1];
		tex2 = s.textures[2];
		if (webFixFeedback && webCurrentFBO && tex2 && tex2->glFBO == (GLuint)webCurrentFBO)
		{
			if (webFeedbackHits < 20)
				printf("FEEDBACK: tex2 %u is attached to draw fbo %d (pass %d) - substituted white\n",
					tex2->glTexture, (int)webCurrentFBO, (int)DeferredRender::GetRenderPass());
			++webFeedbackHits;
			tex2 = NULL;
		}
	}

	draw.tex0 = tex0;
	draw.tex1 = tex1;
	draw.tex2 = tex2;
	draw.sampler0 = WebResolveSamplerState(tex0, 0);
	draw.sampler1 = WebResolveSamplerState(tex1, 1);
	draw.sampler2 = WebResolveSamplerState(tex2, 2);
	draw.blend = WebResolveBlendState();

	// ---- batch decision. only the unit-quad path through the ubershader can batch, and
	// only when both uv transforms agree (both are baked into the one uv attribute) and
	// the color survives an 8-bit pack
	const bool quadPath = webSpriteBatch && !cachedVAO && !verts && !usesPixelShader &&
		memcmp(uv, uv2, sizeof(draw.uv)) == 0 && worldMatrix != NULL &&
		(webBatchShadowPass || !DeferredRender::GetRenderPassIsShadow());
	const bool bakeable = WebColorIsBakeable(draw.color);
	if (quadPath && !bakeable)
		++webBatchUnbakeableColors;		// reported by the TERRAINCACHE debug line
	const bool batchable = quadPath && bakeable;
	WebComputeWVP(draw.wvp, batchable ? NULL : worldMatrix);

	if (batchable)
	{
		if (webBatchQuads &&
			(webBatchQuads >= webBatchMaxQuads || !WebBatchKeyMatches(draw, webBatchState)))
			WebFlushSpriteBatch();
		webBatchState = draw;
		WebBatchAppendQuad(worldMatrix, uv, draw.color);
		return;		// geometry only - the draw happens at flush
	}

	// any non-batchable draw is an ordering barrier
	WebFlushSpriteBatch();

	// ---- apply + draw
	WebApplyResolvedDraw(draw, false);

	if (cachedVAO)
	{
		// static geometry already resident on the gpu - no upload, no pool traffic
		WebBindVAO(cachedVAO);
		for (int r = Max(1, webDrawRepeat); r--; )
			glDrawArrays(mode, cachedFirst, vertCount);
		WebCheckDrawError("cached", tex0);
		if (webShadowFlush >= 4)
			glFlush();
	}
	else if (verts)
	{
		const int first = webTexPool.Upload(verts, vertCount);
		for (int r = Max(1, webDrawRepeat); r--; )
			glDrawArrays(mode, first, vertCount);
		WebCheckDrawError("streamed", tex0);
		if (webShadowFlush >= 4)
			glFlush();
	}
	else
	{
		glBindVertexArray(webQuadVAO);
		for (int r = Max(1, webDrawRepeat); r--; )
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		WebCheckDrawError("quad", tex0);
	}
	WebCountDrawForFlush();
	++webTotalDraws;
}

////////////////////////////////////////////////////////////////////////////////////////
// FVF-parsed primitives (real vertex buffers: light mask, fonts)
////////////////////////////////////////////////////////////////////////////////////////

static const int webMaxParsedVerts = 8192;
static WebTexVertex webParsedVerts[webMaxParsedVerts];

// parse a locked d3d vertex buffer into the streaming layout; returns vert count
static int WebParseVerts(const FrankWebVertexBuffer* vb, int primitiveCount, int primitiveType, UINT stride, DWORD fvf, GLenum& modeOut)
{
	int vertCount = 0;
	switch (primitiveType)
	{
		case D3DPT_LINELIST:		vertCount = primitiveCount * 2;	modeOut = GL_LINES; break;
		case D3DPT_LINESTRIP:		vertCount = primitiveCount + 1;	modeOut = GL_LINE_STRIP; break;
		case D3DPT_TRIANGLELIST:	vertCount = primitiveCount * 3;	modeOut = GL_TRIANGLES; break;
		case D3DPT_TRIANGLESTRIP:	vertCount = primitiveCount + 2;	modeOut = GL_TRIANGLE_STRIP; break;
		case D3DPT_TRIANGLEFAN:		vertCount = primitiveCount + 2;	modeOut = GL_TRIANGLE_FAN; break;
		default: return 0;
	}
	if (vertCount > webMaxParsedVerts)
		vertCount = webMaxParsedVerts;
	if ((UINT)vertCount * stride > vb->size)
		vertCount = vb->size / stride;

	const bool hasDiffuse = (fvf & D3DFVF_DIFFUSE) != 0;
	const bool hasUV = (fvf & D3DFVF_TEX1) != 0;
	const unsigned char* src = vb->data;
	for (int i = 0; i < vertCount; ++i, src += stride)
	{
		WebTexVertex& out = webParsedVerts[i];
		const float* pos = (const float*)src;
		out.x = pos[0]; out.y = pos[1]; out.z = pos[2];
		UINT offset = 12;
		if (hasDiffuse)
		{
			const DWORD c = *(const DWORD*)(src + offset);
			out.a = (unsigned char)((c >> 24) & 0xff);
			out.r = (unsigned char)((c >> 16) & 0xff);
			out.g = (unsigned char)((c >> 8) & 0xff);
			out.b = (unsigned char)(c & 0xff);
			offset += 4;
		}
		else
		{
			out.r = out.g = out.b = out.a = 255;
		}
		if (hasUV)
		{
			const float* uv = (const float*)(src + offset);
			out.u = uv[0]; out.v = uv[1];
		}
		else
		{
			out.u = out.v = 0;
		}
	}
	return vertCount;
}

// the raw draw behind the header-inline Render(matrix, color, texture, vb, ...) -
// this is the path all of deferredRender's pass-composition quads take
void FrankWebRenderRawPrimitive(const Matrix44& matrix, const Color& color, FrankWebTexture* texture,
	FrankWebVertexBuffer* vb, int primitiveCount, int primitiveType, UINT stride, DWORD fvf)
{
	if (!webGLContext || primitiveCount <= 0 || color.a == 0)
		return;

	if (vb && vb->data && stride)
	{
		GLenum mode = GL_TRIANGLE_STRIP;
		const int vertCount = WebParseVerts(vb, primitiveCount, primitiveType, stride, fvf, mode);
		if (vertCount >= 2)
			WebDraw(&matrix.GetMatrixBase()._11, color, texture, NULL, NULL, 0, true, webParsedVerts, vertCount, mode);
		return;
	}

	// no buffer data: legacy unit-quad fallback
	if (primitiveType == D3DPT_TRIANGLESTRIP)
		WebDraw(&matrix.GetMatrixBase()._11, color, texture, NULL, NULL, 0, true, NULL, 0, GL_TRIANGLE_STRIP);
}

// RenderQuadSimple links through this overload (only reached by normal-mapping paths)
void FrankRender::Render(const Matrix44& matrix, LPDIRECT3DVERTEXBUFFER9 vb, int primitiveCount, D3DPRIMITIVETYPE primitiveType, UINT stride, DWORD fvf)
{
	FrankWebRenderRawPrimitive(matrix, Color::White(), FrankWebGetDeviceState().textures[0], vb, primitiveCount, primitiveType, stride, fvf);
}

////////////////////////////////////////////////////////////////////////////////////////
// textures
////////////////////////////////////////////////////////////////////////////////////////

// multiply each emissive companion's alpha by its diffuse map's alpha at load time, so
// the emissive pass gets the tile's real silhouette (see the note in LoadTexture)
ConsoleCommandSimple(bool, webBakeEmissiveAlpha, true);

// upload raw RGBA pixels as a texture
static FrankWebTexture* WebCreateTextureFromPixels(const unsigned char* data, int w, int h)
{
	FrankWebTexture* t = new FrankWebTexture();
	t->width = w;
	t->height = h;
	t->hasMipmaps = true;
	glGenTextures(1, &t->glTexture);
	glBindTexture(GL_TEXTURE_2D, t->glTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, g_usePointFiltering ? GL_NEAREST : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	return t;
}

// probe the same locations the windows loader uses, png then jpg, with both absolute and
// relative forms (preload plugin path sensitivity). returns raw RGBA pixels; caller frees
static unsigned char* WebLoadImagePixels(const char* name, int& w, int& h)
{
	const char* patterns[] =
	{
		"/data/%s.png", "/data/%s.jpg", "/data/terrain/%s.png", "/data/terrain/%s.jpg",
		"/data/fonts/%s.png", "data/fonts/%s.png",
		"data/%s.png", "data/%s.jpg", "data/terrain/%s.png", "data/terrain/%s.jpg",
		"/%s.png", "%s.png", "/%s.jpg", "%s.jpg"
	};
	for (int i = 0; i < 14; ++i)
	{
		char path[256];
		snprintf(path, sizeof(path), patterns[i], name);
		w = h = 0;
		char* data = emscripten_get_preloaded_image_data(path, &w, &h);
		if (data && w > 0 && h > 0)
			return (unsigned char*)data;
		if (data)
			free(data);
	}
	return NULL;
}

static FrankWebTexture* WebLoadImageProbe(const char* name)
{
	int w = 0, h = 0;
	unsigned char* pixels = WebLoadImagePixels(name, w, h);
	if (!pixels)
		return NULL;
	FrankWebTexture* t = WebCreateTextureFromPixels(pixels, w, h);
	free(pixels);
	return t;
}

// Load a companion map (name_e / name_n / name_s) and bake the diffuse map's silhouette
// into its alpha, ONCE, at load. See the long note in LoadTexture for why.
static FrankWebTexture* WebLoadCompanionMap(const char* baseName, const char* suffix,
	const unsigned char* diffusePixels, int diffuseW, int diffuseH, bool& bakedAlphaOut)
{
	char name[FILENAME_STRING_LENGTH + 2];
	snprintf(name, sizeof(name), "%s%s", baseName, suffix);

	int w = 0, h = 0;
	unsigned char* pixels = WebLoadImagePixels(name, w, h);
	if (!pixels)
		return NULL;

	if (webBakeEmissiveAlpha && w == diffuseW && h == diffuseH)
	{
		const int pixelCount = w * h;
		for (int i = 0; i < pixelCount; ++i)
		{
			const int a = pixels[i*4 + 3] * diffusePixels[i*4 + 3];
			pixels[i*4 + 3] = (unsigned char)((a + 127) / 255);
		}
		bakedAlphaOut = true;
	}

	FrankWebTexture* t = WebCreateTextureFromPixels(pixels, w, h);
	free(pixels);
	return t;
}

bool FrankRender::LoadTexture(const WCHAR* textureName, TextureID ti)
{
	ASSERT(ti < MAX_TEXTURE_COUNT);
	if (!webGLContext)
		return false;

	char name[FILENAME_STRING_LENGTH];
	wcstombs(name, textureName, FILENAME_STRING_LENGTH - 1);
	name[FILENAME_STRING_LENGTH - 1] = 0;

	int diffuseW = 0, diffuseH = 0;
	unsigned char* diffusePixels = WebLoadImagePixels(name, diffuseW, diffuseH);
	if (!diffusePixels)
	{
		printf("webRender: texture NOT FOUND: %s\n", name);
		return false;
	}
	FrankWebTexture* tex = WebCreateTextureFromPixels(diffusePixels, diffuseW, diffuseH);

	// Companion maps (name_e emissive, name_n normal, name_s specular), mirroring the
	// windows loader; the deferred passes select them through GetTexture.
	//
	// Each one gets the diffuse map's silhouette baked into its alpha, ONCE, at load.
	// The windows path does this per draw: for the emissive/specular/normal passes it
	// binds the diffuse texture to stage 1 with ALPHAOP = MODULATE and takes colour from
	// stage 0 (terrainRender.cpp RenderCached, "use the alpha channel from the diffuse
	// map"). Terrain on web has its own cached draw path that never sets stage 1 and
	// samples the companion ALONE, so without this a tile whose art is partly transparent
	// covered a full opaque square in those buffers. Doing it at load keeps the draw path
	// completely untouched - no extra texture unit, no shader branch, no per-draw cost,
	// and nothing that can interact with the sprite batcher.
	//
	// Multiply rather than replace, so a companion that carries its own alpha still means
	// something. Sprites apply the diffuse alpha again themselves (the emissive pass via
	// its diffuse x emissive draw, the normal pass inside normalMapShader), so for them
	// it lands twice - a no-op for the hard 0/255 alpha of pixel art and a hair of extra
	// falloff on an antialiased edge. The one real casualty is
	// diffuseNormalAlphaPercent, which softens only the shader's copy and so cannot
	// widen a normal past the baked silhouette; nothing in TestGame or Piroot uses it
	// (only Perishable's decalEffect does).
	bool bakedAlpha = false;
	FrankWebTexture* texEmissive = WebLoadCompanionMap(name, "_e", diffusePixels, diffuseW, diffuseH, bakedAlpha);
	FrankWebTexture* texNormal   = WebLoadCompanionMap(name, "_n", diffusePixels, diffuseW, diffuseH, bakedAlpha);
	FrankWebTexture* texSpecular = WebLoadCompanionMap(name, "_s", diffusePixels, diffuseW, diffuseH, bakedAlpha);
	free(diffusePixels);

	printf("webRender: loaded texture %s -> gl id %u%s%s%s%s\n", name, tex->glTexture,
		texEmissive ? " (+emissive)" : "", texNormal ? " (+normal)" : "",
		texSpecular ? " (+specular)" : "", bakedAlpha ? " (alpha baked)" : "");

	textures[ti][TT_Diffuse].texture = tex;
	textures[ti][TT_Emissive].texture = texEmissive;
	textures[ti][TT_Normal].texture = texNormal;
	textures[ti][TT_Specular].texture = texSpecular;
	textureHashMap[wstring(textureName)] = ti;
	if (ti > highestLoadedTextureID)
		highestLoadedTextureID = ti;
	return true;
}

void FrankRender::ReloadModifiedTextures() {}

LPDIRECT3DTEXTURE9 FrankRender::GetTexture(TextureID ti, bool deferredCheck) const
{
	ASSERT(ti < MAX_TEXTURE_COUNT);
	if (textures[ti][TT_Diffuse].tileSheetTi)
		ti = textures[ti][TT_Diffuse].tileSheetTi;
	if (deferredCheck)
	{
		// same selection the windows loader makes (frankRender.cpp GetTexture). returning
		// NULL when a companion is missing is deliberate: callers key off it to fall back
		// to flat z-facing normals / black specular.
		if (DeferredRender::GetRenderPassIsNormalMap())
			return textures[ti][TT_Normal].texture;
		if (DeferredRender::GetRenderPassIsSpecular())
			return textures[ti][TT_Specular].texture;
		if (DeferredRender::GetRenderPassIsEmissive())
			return textures[ti][TT_Emissive].texture;
	}
	return textures[ti][TT_Diffuse].texture;
}

IntVector2 FrankRender::GetTextureSize(LPDIRECT3DTEXTURE9 texture) const
{
	return texture ? IntVector2(texture->width, texture->height) : IntVector2(0);
}

D3DFORMAT FrankRender::GetTextureFormat(LPDIRECT3DTEXTURE9 texture) const
{
	return texture ? texture->format : 0;
}

TextureID FrankRender::FindTextureID(const wstring& textureName) const
{
	unordered_map<wstring, int>::const_iterator it = textureHashMap.find(textureName);
	return (it != textureHashMap.end()) ? TextureID(it->second) : TextureID(0);
}

void FrankRender::SetTextureTile(const WCHAR* textureName, TextureID tileSheetTi, TextureID tileTi, const ByteVector2& tilePos, const ByteVector2& tileSize)
{
	ASSERT(tileTi < MAX_TEXTURE_COUNT);
	TextureWrapper& tw = textures[tileTi][TT_Diffuse];
	tw.tileSheetTi = tileSheetTi;
	tw.tilePos = tilePos;
	tw.tileSize = tileSize;
	if (textureName)
		textureHashMap[wstring(textureName)] = tileTi;
}

// Shader loading. The .psh files are never parsed on web - each known shader name maps to
// a GLSL port compiled at context creation. Anything unrecognised still gets an inert
// success handle (glProgram 0) so init logic keeps its windows behavior and its draws fall
// back to the ubershader; visionShader is the only one that lands there today.
bool FrankRender::LoadPixelShader(const WCHAR* name, LPDIRECT3DPIXELSHADER9& shader, LPD3DXCONSTANTTABLE& constants, bool loadFromResource)
{
	FrankWebPixelShader* ps = new FrankWebPixelShader();
	FrankWebConstantTable* ct = new FrankWebConstantTable();

	GLuint program = 0;
	if (wcsstr(name, L"blurShader"))				program = webBlurProgram;
	else if (wcsstr(name, L"normalMapShader"))		program = webNormalMapProgram;
	else if (wcsstr(name, L"deferredLightShader"))	program = webLightProgram;
	else if (wcsstr(name, L"directionalLightShader")) program = webDirLightProgram;

	ps->glProgram = program;
	ct->glProgram = program;
	shader = ps;
	constants = ct;
	return true;
}

// phase 5: create the shared quad primitive (real vertex data, same as the d3d
// BuildQuad) so raw-texture RenderQuad paths in deferredRender have verts to parse
void FrankRender::InitDeviceObjects()
{
	if (primitiveQuad.vb)
		return;

	struct QuadVertex { float x, y, z; float u, v; };
	if (primitiveQuad.Create(2, 4, D3DPT_TRIANGLESTRIP, sizeof(QuadVertex), D3DFVF_XYZ | D3DFVF_TEX1))
	{
		QuadVertex* verts = NULL;
		primitiveQuad.vb->Lock(0, 0, (void**)&verts, 0);
		verts[0] = { -1,  1, 0,  0, 0 };
		verts[1] = {  1,  1, 0,  1, 0 };
		verts[2] = { -1, -1, 0,  0, 1 };
		verts[3] = {  1, -1, 0,  1, 1 };
		primitiveQuad.vb->Unlock();
	}

	// the outline linestrip (BuildQuad, frankRender.cpp:501-534) - without real verts
	// the draw path falls back to the solid unit quad and outlines paint filled boxes
	if (primitiveQuadOutline.Create(4, 5, D3DPT_LINESTRIP, sizeof(QuadVertex), D3DFVF_XYZ | D3DFVF_TEX1))
	{
		QuadVertex* verts = NULL;
		primitiveQuadOutline.vb->Lock(0, 0, (void**)&verts, 0);
		verts[0] = { -1,  1, 0,  0, 0 };
		verts[1] = {  1,  1, 0,  0, 0 };
		verts[2] = {  1, -1, 0,  0, 0 };
		verts[3] = { -1, -1, 0,  0, 0 };
		verts[4] = { -1,  1, 0,  0, 0 };
		primitiveQuadOutline.vb->Unlock();
	}

	// mirrors the windows InitDeviceObjects: the normal pass is gated on
	// IsNormalMapShaderLoaded(), so this handle has to exist before it runs. Called
	// directly rather than through g_render, which is still being constructed here.
	if (DeferredRender::normalMappingEnable && !normalMapShader)
	{
		if (!LoadPixelShader(L"data/shaders/normalMapShader.psh", normalMapShader, normalMapConstantTable))
		{
			g_debugMessageSystem.AddError(L"Normal map shader failed to create!   Normal mapping disabled.");
			DeferredRender::normalMappingEnable = false;
		}
	}
}

void FrankRender::DestroyDeviceObjects()
{
	primitiveQuad.SafeRelease();
	primitiveQuadOutline.SafeRelease();
}

FrankRender::FrankRender()
{
	// windows does this in its own ctor (frankRender.cpp:44). without it
	// IsNormalMapShaderLoaded() reads uninitialised memory and the normal pass runs or
	// not at random.
	normalMapShader = NULL;
	normalMapConstantTable = NULL;
	InitDeviceObjects();
}

////////////////////////////////////////////////////////////////////////////////////////
// core draw paths (pass-aware, mirroring RenderInternal's deferred branching)
////////////////////////////////////////////////////////////////////////////////////////

// Port of the normal-map constant setup in RenderInternal (frankRender.cpp:1533-1574).
// Builds the local-to-world normal rotation out of the draw's world matrix and pushes it
// into the ported shader along with the diffuse colour and alpha percentage.
static void WebSetNormalMapConstants(const Matrix44& matrix, const Color& color)
{
	// get rid of position and fix z scale
	FrankMat44Base m = matrix.GetMatrixBase();
	m._33 = 1;
	m._41 = 0;
	m._42 = 0;

	// invert to get local transform
	Matrix44 inverted = Matrix44(m).Inverse();
	FrankMat44Base& m2 = inverted.GetMatrixBase();

	// get rid of scale component
	const Vector2 v1 = Vector2(m2._11, m2._12).Normalize();
	m2._11 = v1.x;
	m2._12 = v1.y;
	const Vector2 v2 = Vector2(m2._21, m2._22).Normalize();
	m2._21 = v2.x;
	m2._22 = v2.y;

	// check for flipped texture
	if (Vector2::IsClockwise(Vector2(matrix.GetRight()), Vector2(matrix.GetUp())))
	{
		m2._12 = -m2._12;
		m2._21 = -m2._21;
	}

	FrankWebSetUniformMatrix(webNormalMapProgram, "transformMatrix", &m2._11);

	// use all white with diffuse alpha
	const float diffuseColor[4] = { 1.0f, 1.0f, 1.0f, color.a };
	FrankWebSetUniformVec4(webNormalMapProgram, "diffuseColor", diffuseColor);

	float diffuseNormalAlphaPercent = 1;
	if (DeferredRender::DiffuseNormalAlphaRenderBlock::IsActive())
		diffuseNormalAlphaPercent = DeferredRender::DiffuseNormalAlphaRenderBlock::GetAlphaScale();
	FrankWebSetUniformFloat(webNormalMapProgram, "diffuseNormalAlphaPercent", diffuseNormalAlphaPercent);
}

// per-pass adjustments for game-object sprites (port of frankRender.cpp RenderInternal):
// returns false if the draw should be skipped for this pass
static bool WebAdjustForRenderPass(TextureID ti, const Color& colorIn, Color& colorOut, FrankWebTexture*& tex0, FrankWebTexture*& tex1, int& extraFlags)
{
	colorOut = colorIn;
	tex1 = NULL;
	extraFlags = 0;

	if (DeferredRender::GetRenderPassIsDirectionalShadow() && !DeferredRender::BackgroundRenderBlock::IsActive())
		return false;	// directional shadow pass only renders background-block objects

	if (DeferredRender::GetRenderPassIsNormalMap())
	{
		// port of RenderInternal's normal branch (frankRender.cpp:1493-1594)
		if (!tex0)
		{
			// no texture at all: flat z-facing normals
			colorOut = Color(0.5f, 0.5f, 1.0f, colorIn.a);
			return true;
		}
		FrankWebTexture* normalMap = g_render->GetTexture(ti, true);	// TT_Normal this pass
		if (!normalMap)
		{
			// no normal map: flat z-facing normals, silhouette from the diffuse alpha
			colorOut = Color(0.5f, 0.5f, 1.0f, colorIn.a);
			extraFlags |= WebFlag_IgnoreTexRGB;
			return true;
		}
		// real normal map: the ported shader rotates it into world space. tex0 stays the
		// diffuse map, which the shader samples for its alpha (same binding as windows).
		tex1 = normalMap;
		colorOut = Color::White();
		extraFlags |= WebFlag_UseNormalMapShader;
		return true;
	}

	if (DeferredRender::GetRenderPassIsSpecular())
	{
		// port of RenderInternal's specular branch (frankRender.cpp:1595-1639). windows
		// takes stage 1's colour with SELECTARG1, so the object colour never tints the
		// specular buffer - only its alpha carries through.
		const bool isSpecularBlock = DeferredRender::SpecularRenderBlock::IsActive();
		if (!tex0)
		{
			colorOut = isSpecularBlock ? Color::White(colorIn.a) : Color::Black(colorIn.a);
			return true;
		}
		FrankWebTexture* specularMap = g_render->GetTexture(ti, true);	// TT_Specular
		if (specularMap)
		{
			// its alpha already carries the diffuse silhouette (baked in LoadTexture)
			tex0 = specularMap;
			colorOut = Color(1.0f, 1.0f, 1.0f, colorIn.a);
			return true;
		}
		// no specular map: black (or white inside a specular block), silhouette from the
		// diffuse alpha
		colorOut = isSpecularBlock ? Color::White(colorIn.a) : Color(0.0f, 0.0f, 0.0f, colorIn.a);
		extraFlags |= WebFlag_IgnoreTexRGB;
		return true;
	}

	if (DeferredRender::GetRenderPassIsShadow() && !DeferredRender::TransparentRenderBlock::IsActive())
	{
		// solid shadow color, silhouette from the texture's alpha
		Color shadowColor = DeferredRender::defaultShadowColor;
		shadowColor.a *= colorIn.a;
		colorOut = shadowColor;
		extraFlags |= WebFlag_IgnoreTexRGB;
		return true;
	}

	if (DeferredRender::GetRenderPassIsEmissive() && tex0)
	{
		FrankWebTexture* emissive = g_render->GetTexture(ti, true);	// TT_Emissive during this pass
		const DWORD ambient = FrankWebGetDeviceState().renderStates[139];
		if (emissive && ambient != 0x00FFFFFF)
		{
			// no emissive override block: draw diffuse x emissive at full brightness
			// (frankRender.cpp:1640); ambient stays black for everything else
			tex1 = emissive;
			colorOut = colorIn;
			extraFlags |= WebFlag_ForceFullBright;
			return true;
		}
		// else: fall through; the ambient multiply blacks it out, or the emissive
		// render block has ambient at white and it draws normally
	}

	return true;
}

void FrankRender::Render(const Matrix44& matrix, const Color& color, TextureID ti, const RenderPrimitive& rp, const int tileRotation, const bool tileMirror)
{
	if (!webGLContext || color.a == 0)
		return;
	ASSERT(ti < MAX_TEXTURE_COUNT);

	const TextureWrapper& tw = textures[ti][TT_Diffuse];
	if (tw.tileSheetTi != Texture_Invalid)
	{
		RenderTile(tw.tilePos, tw.tileSize, matrix, color, tw.tileSheetTi, rp, tileRotation, tileMirror);
		return;
	}

	FrankWebTexture* tex0 = tw.texture;
	FrankWebTexture* tex1 = NULL;
	Color finalColor;
	int extraFlags = 0;
	if (!WebAdjustForRenderPass(ti, color, finalColor, tex0, tex1, extraFlags))
		return;

	const bool fullBright = (extraFlags & WebFlag_ForceFullBright) != 0;
	extraFlags &= ~WebFlag_ForceFullBright;
	const bool useNormalMapShader = (extraFlags & WebFlag_UseNormalMapShader) != 0;
	extraFlags &= ~WebFlag_UseNormalMapShader;
	if (useNormalMapShader)
	{
		WebSetNormalMapConstants(matrix, color);
		FrankWebGetDeviceState().pixelShader = normalMapShader;
	}

	if (rp.vb && rp.vb->data && (&rp != &primitiveQuad))
	{
		GLenum mode = GL_TRIANGLE_STRIP;
		const int vertCount = WebParseVerts(rp.vb, rp.primitiveCount, rp.primitiveType, rp.stride, rp.fvf, mode);
		if (vertCount >= 2)
			WebDraw(&matrix.GetMatrixBase()._11, finalColor, tex0, tex1, NULL, extraFlags, !fullBright, webParsedVerts, vertCount, mode);
	}
	else
		WebDraw(&matrix.GetMatrixBase()._11, finalColor, tex0, tex1, NULL, extraFlags, !fullBright, NULL, 0, GL_TRIANGLE_STRIP);

	if (useNormalMapShader)
		FrankWebGetDeviceState().pixelShader = NULL;
}

void FrankRender::RenderTile(const IntVector2& tilePos, const IntVector2& tileSize, const Matrix44& matrix, const Color& color, TextureID ti, const RenderPrimitive& rp, int tileRotation, bool tileMirror)
{
	if (!webGLContext || color.a == 0)
		return;

	FrankWebTexture* tex0 = textures[ti][TT_Diffuse].texture;
	FrankWebTexture* tex1 = NULL;
	Color finalColor;
	int extraFlags = 0;
	if (!WebAdjustForRenderPass(ti, color, finalColor, tex0, tex1, extraFlags))
		return;
	const bool fullBright = (extraFlags & WebFlag_ForceFullBright) != 0;
	extraFlags &= ~WebFlag_ForceFullBright;
	const bool useNormalMapShader = (extraFlags & WebFlag_UseNormalMapShader) != 0;
	extraFlags &= ~WebFlag_UseNormalMapShader;
	if (useNormalMapShader)
	{
		WebSetNormalMapConstants(matrix, color);
		FrankWebGetDeviceState().pixelShader = normalMapShader;
	}

	extern Matrix44 WebBuildTileUVMatrix(const IntVector2& tilePos, const IntVector2& tileSize, int tileRotation, bool tileMirror);
	Matrix44 uvMatrix = WebBuildTileUVMatrix(tilePos, tileSize, tileRotation, tileMirror);
	WebDraw(&matrix.GetMatrixBase()._11, finalColor, tex0, tex1, &uvMatrix.GetMatrixBase()._11, extraFlags, !fullBright, NULL, 0, GL_TRIANGLE_STRIP);

	if (useNormalMapShader)
		FrankWebGetDeviceState().pixelShader = NULL;
}

void FrankRender::RenderScreenSpace(const Matrix44& matrix, const Color& color, TextureID ti, const RenderPrimitive& rp)
{
	if (!webGLContext || color.a == 0)
		return;

	// screen space: matrix is in backbuffer pixels; map to clip directly
	FrankMat44Base pixelToClip;
	FrankMatrixIdentity(pixelToClip);
	pixelToClip._11 = 2.0f / g_backBufferWidth;
	pixelToClip._22 = -2.0f / g_backBufferHeight;
	pixelToClip._41 = -1;
	pixelToClip._42 = 1;

	float m[16];
	WebMatMul(m, &matrix.GetMatrixBase()._11, &pixelToClip._11);

	// draw with an identity view/proj by temporarily building the full matrix ourselves:
	// reuse WebDraw by pushing pixelToClip through the world slot with view/proj identity.
	// (screen-space draws happen outside the deferred passes, so no FBO flip here.)
	FrankWebDeviceState& s = FrankWebGetDeviceState();
	float savedView[16], savedProj[16];
	memcpy(savedView, s.view, sizeof(savedView));
	memcpy(savedProj, s.projection, sizeof(savedProj));
	memcpy(s.view, webIdentity, sizeof(webIdentity));
	memcpy(s.projection, webIdentity, sizeof(webIdentity));

	FrankWebTexture* tex0 = textures[ti][TT_Diffuse].texture;
	Matrix44 uvMatrix;
	const float* uvMatrixPtr = NULL;
	if (textures[ti][TT_Diffuse].tileSheetTi)
	{
		// tiled texture: window the sheet uvs like the d3d texture transform the
		// windows path sets here (frankRender.cpp:1199-1205 - plain scale/offset,
		// no uv-inset/rotation like the world-space tile path)
		const TextureWrapper& tw = textures[ti][TT_Diffuse];
		tex0 = textures[tw.tileSheetTi][TT_Diffuse].texture;
		const Vector2 scale = 1.0f / Vector2(tw.tileSize);
		uvMatrix = Matrix44::BuildScale(scale.x, scale.y, 0);
		uvMatrix.GetMatrixBase()._31 = tw.tilePos.x * scale.x;
		uvMatrix.GetMatrixBase()._32 = tw.tilePos.y * scale.y;
		uvMatrixPtr = &uvMatrix.GetMatrixBase()._11;
	}

	if (rp.vb && rp.vb->data && (&rp != &primitiveQuad))
	{
		GLenum mode = GL_TRIANGLE_STRIP;
		const int vertCount = WebParseVerts(rp.vb, rp.primitiveCount, rp.primitiveType, rp.stride, rp.fvf, mode);
		if (vertCount >= 2)
			WebDraw(m, color, tex0, NULL, uvMatrixPtr, 0, false, webParsedVerts, vertCount, mode);
	}
	else
		WebDraw(m, color, tex0, NULL, uvMatrixPtr, 0, false, NULL, 0, GL_TRIANGLE_STRIP);

	memcpy(s.view, savedView, sizeof(savedView));
	memcpy(s.projection, savedProj, sizeof(savedProj));
}

// raw-texture screen-space path (map textures, deferred buffers via the header inline)
void FrankRender::RenderScreenSpace(const Matrix44& matrix, const Color& color, LPDIRECT3DTEXTURE9 texture, LPDIRECT3DVERTEXBUFFER9 vb, int primitiveCount, D3DPRIMITIVETYPE primitiveType, UINT stride, DWORD fvf)
{
	if (!webGLContext || color.a == 0)
		return;

	// screen space: matrix is in backbuffer pixels; map to clip directly
	FrankMat44Base pixelToClip;
	FrankMatrixIdentity(pixelToClip);
	pixelToClip._11 = 2.0f / g_backBufferWidth;
	pixelToClip._22 = -2.0f / g_backBufferHeight;
	pixelToClip._41 = -1;
	pixelToClip._42 = 1;

	float m[16];
	WebMatMul(m, &matrix.GetMatrixBase()._11, &pixelToClip._11);

	FrankWebDeviceState& s = FrankWebGetDeviceState();
	float savedView[16], savedProj[16];
	memcpy(savedView, s.view, sizeof(savedView));
	memcpy(savedProj, s.projection, sizeof(savedProj));
	memcpy(s.view, webIdentity, sizeof(webIdentity));
	memcpy(s.projection, webIdentity, sizeof(webIdentity));

	if (vb && vb->data && vb != primitiveQuad.vb)
	{
		GLenum mode = GL_TRIANGLE_STRIP;
		const int vertCount = WebParseVerts(vb, primitiveCount, primitiveType, stride, fvf, mode);
		if (vertCount >= 2)
			WebDraw(m, color, texture, NULL, NULL, 0, false, webParsedVerts, vertCount, mode);
	}
	else
		WebDraw(m, color, texture, NULL, NULL, 0, false, NULL, 0, GL_TRIANGLE_STRIP);

	memcpy(s.view, savedView, sizeof(savedView));
	memcpy(s.projection, savedProj, sizeof(savedProj));
}

// texture transform replicated from the d3d path (frankRender.cpp:1362-1410);
// shared by the sprite tile path and the terrain renderer
Matrix44 WebBuildTileUVMatrix(const IntVector2& tilePos, const IntVector2& tileSize, int tileRotation, bool tileMirror)
{
	const Vector2 inverseTileSize = 1.0f / Vector2(tileSize);
	const Vector2 uvFix = (g_tileSetUVScale == 0) ? Vector2(0) : Vector2(1 - g_tileSetUVScale) * inverseTileSize;
	const Vector2 scale = inverseTileSize - uvFix;
	const float rotation = float(tileRotation) * PI / 2;

	Matrix44 matrixScale;
	if (tileMirror)
	{
		if (tileRotation % 2)
			matrixScale = Matrix44::BuildScale(scale.x, -scale.y, 0);
		else
			matrixScale = Matrix44::BuildScale(-scale.x, scale.y, 0);
	}
	else
		matrixScale = Matrix44::BuildScale(scale.x, scale.y, 0);
	Matrix44 matrixRotation = Matrix44::BuildRotateZ(rotation);
	Matrix44 uvMatrix = matrixRotation * matrixScale;

	Vector2 tilePosFloat = Vector2(tilePos);
	tilePosFloat += Vector2(0.5f);
	if (tileMirror)
	{
		if (tileRotation % 2)
			tilePosFloat -= g_tileSetUVScale * Vector2(1,-1) * Vector2(0.5f).Rotate(rotation);
		else
			tilePosFloat -= g_tileSetUVScale * Vector2(-1,1) * Vector2(0.5f).Rotate(rotation);
	}
	else
		tilePosFloat -= g_tileSetUVScale * Vector2(0.5f).Rotate(rotation);
	tilePosFloat *= inverseTileSize;

	uvMatrix.GetMatrixBase()._31 = tilePosFloat.x;
	uvMatrix.GetMatrixBase()._32 = tilePosFloat.y;
	return uvMatrix;
}

////////////////////////////////////////////////////////////////////////////////////////
// simple verts (lines/tris batches) - ported from the d3d path, gl streaming draw
////////////////////////////////////////////////////////////////////////////////////////

// packed stream vertex matching SimpleVertex (Vector3 + DWORD) but with RGBA byte order
struct WebStreamVertex { float x, y, z; unsigned char r, g, b, a; };

static void WebDrawStream(const void* verts, int count, GLenum mode, const Color& color)
{
	WebFlushSpriteBatch();	// streamed verts are an ordering barrier for pending sprites
	if (!webGLContext || count <= 1)
		return;
	if (webDrawSkipMask & 8)
		return;
	if (webShadowPass2Active)
		return;
	if ((webStripMask & 16) && !DeferredRender::GetRenderPassIsShadow())
		return;
	WebBindProgram(webProgram);
	float wvp[16];
	WebComputeWVP(wvp, NULL);
	glUniformMatrix4fv(webLocs.worldViewProj, 1, GL_FALSE, wvp);
	glUniformMatrix4fv(webLocs.uvMatrix, 1, GL_FALSE, webIdentity);
	glUniform4f(webLocs.color, color.r, color.g, color.b, color.a);
	glUniform1i(webLocs.flags, 0);
	WebBindTextureUnit(0, webWhiteTexture);
	glUniform1i(webLocs.texture, 0);
	// scrub unit 1 like WebDraw does: uTexture2 is statically declared, so once any
	// modulate2 draw points it at unit 1 the program samples unit 1 forever. a STALE
	// render-target texture left there while this batch draws into that target is a
	// feedback loop and the ENTIRE batch (hundreds of primitives, one draw call) is
	// silently skipped
	WebBindTextureUnit(1, webWhiteTexture);
	WebActiveUnit(0);	// leave unit 0 active, as the rest of the file assumes
	WebClearSamplers();	// this path never sets sampler state - use the textures' own
	WebApplyBlendState();
	const int first = webStreamPool.Upload(verts, count);
	for (int r = Max(1, webDrawRepeat); r--; )
		glDrawArrays(mode, first, count);
	WebCheckDrawError("simpleverts", NULL);
	if (webShadowFlush >= 4)
		glFlush();
	WebCountDrawForFlush();
	++webTotalDraws;
}

// D3DCOLOR is ARGB packed; convert to RGBA bytes for GL
static WebStreamVertex WebConvertVert(const Vector3& pos, DWORD c)
{
	WebStreamVertex v;
	v.x = pos.x; v.y = pos.y; v.z = pos.z;
	v.a = (unsigned char)((c >> 24) & 0xff);
	v.r = (unsigned char)((c >> 16) & 0xff);
	v.g = (unsigned char)((c >> 8) & 0xff);
	v.b = (unsigned char)(c & 0xff);
	return v;
}

// the windows flush path goes through RenderInternal, which drops everything during
// the directional-shadow pass unless a BackgroundRenderBlock is active
// (frankRender.cpp:1487); replicate that gate on the direct GL flush paths
static bool WebSimpleVertsGated()
{
	return DeferredRender::GetRenderPassIsDirectionalShadow() && !DeferredRender::BackgroundRenderBlock::IsActive();
}

void FrankRender::RenderSimpleLineVerts(const Color& color, bool makeBlack)
{
	if (WebSimpleVertsGated())
	{
		simpleVertLineCount = 0;
		return;
	}
	if (simpleVertLineCount <= 1)
	{
		simpleVertLineCount = 0;
		return;
	}
	totalSimpleVertsRendered += simpleVertLineCount;
	static WebStreamVertex converted[maxSimpleVerts];
	for (int i = 0; i < simpleVertLineCount; ++i)
	{
		DWORD c = simpleVertsLines[i].color;
		if (makeBlack) c &= 0xFF000000;
		converted[i] = WebConvertVert(simpleVertsLines[i].position, c);
	}
	WebDrawStream(converted, simpleVertLineCount, GL_LINE_STRIP, color);
	simpleVertLineCount = 0;
}

void FrankRender::RenderSimpleTriVerts(const Color& color, bool makeBlack, TextureID ti)
{
	if (WebSimpleVertsGated())
	{
		simpleVertTriCount = 0;
		return;
	}
	if (simpleVertTriCount <= 2)
	{
		simpleVertTriCount = 0;
		return;
	}
	totalSimpleVertsRendered += simpleVertTriCount;
	static WebStreamVertex converted[maxSimpleVerts];
	for (int i = 0; i < simpleVertTriCount; ++i)
	{
		DWORD c = simpleVertsTris[i].color;
		if (makeBlack) c &= 0xFF000000;
		converted[i] = WebConvertVert(simpleVertsTris[i].position, c);
	}
	WebDrawStream(converted, simpleVertTriCount, GL_TRIANGLE_STRIP, color);
	simpleVertTriCount = 0;
}

void FrankRender::RenderSimpleVerts()
{
	if (!webGLContext)
	{
		simpleVertLineCount = simpleVertTriCount = 0;
		return;
	}

	// deferred-pass rules ported from frankRender.cpp:674-736
	bool makeBlack = false;
	if (DeferredRender::GetRenderPassIsEmissive())
		makeBlack = (!simpleVertsAreAdditive && !DeferredRender::EmissiveRenderBlock::IsActive());
	else if (DeferredRender::GetRenderPassIsShadow() && !DeferredRender::TransparentRenderBlock::IsActive())
		makeBlack = true;

	// additive verts render with dest ONE like the d3d path
	FrankWebDeviceState& s = FrankWebGetDeviceState();
	const DWORD savedDst = s.renderStates[20];
	if (simpleVertsAreAdditive)
		s.renderStates[20] = 2;	// D3DBLEND_ONE

	RenderSimpleTriVerts(Color::White(), makeBlack);
	RenderSimpleLineVerts(Color::White(), makeBlack);

	s.renderStates[20] = savedDst;
}

void FrankRender::SetSimpleVertsAreAdditive(bool additive)
{
	if (additive != simpleVertsAreAdditive)
		RenderSimpleVerts();
	simpleVertsAreAdditive = additive;
}

void FrankRender::AddPointToLineVerts(const Vector2& position, DWORD color)
{
	if (simpleVertLineCount >= maxSimpleVerts - 8)
	{
		SimpleVertex vert1 = simpleVertsLines[simpleVertLineCount-1];
		simpleVertsLines[simpleVertLineCount++] = {vert1.position, 0};
		RenderSimpleVerts();
		simpleVertsLines[simpleVertLineCount++] = vert1;
	}
	SimpleVertex& vert = simpleVertsLines[simpleVertLineCount++];
	vert.position = position;
	vert.color = color;
}

void FrankRender::AddPointToTriVerts(const Vector2& position, DWORD color)
{
	if (simpleVertTriCount >= maxSimpleVerts - 8)
	{
		SimpleVertex vert1 = simpleVertsTris[simpleVertTriCount-2];
		SimpleVertex vert2 = simpleVertsTris[simpleVertTriCount-1];
		simpleVertsTris[simpleVertTriCount++] = {vert2.position, 0};
		simpleVertsTris[simpleVertTriCount++] = {vert2.position, 0};
		RenderSimpleVerts();
		simpleVertsTris[simpleVertTriCount++] = vert1;
		simpleVertsTris[simpleVertTriCount++] = vert2;
	}
	SimpleVertex& vert = simpleVertsTris[simpleVertTriCount++];
	vert.position = position;
	vert.color = color;
}

////////////////////////////////////////////////////////////////////////////////////////
// draw helpers - ported from the d3d bodies (pure math over the vert batches)
////////////////////////////////////////////////////////////////////////////////////////

void FrankRender::DrawLine(const Vector2& p1, const Vector2& p2, const DWORD color)
{
	CapLineVerts(p1);
	AddPointToLineVerts(p1, color);
	AddPointToLineVerts(p2, color);
	CapLineVerts(p2);
}

void FrankRender::DrawPolygon(const Vector2* vertices, const int vertexCount, const DWORD color)
{
	CapLineVerts(vertices[0]);
	for (int i = 0; i < vertexCount; ++i)
		AddPointToLineVerts(vertices[i], color);
	AddPointToLineVerts(vertices[0], color);
	CapLineVerts(vertices[0]);
}

void FrankRender::DrawSolidPolygon(const Vector2* vertices, const int vertexCount, const DWORD color)
{
	// convex polygon as a zigzag triangle strip with degenerate caps
	CapTriVerts(vertices[0]);
	int lo = 0, hi = vertexCount - 1;
	while (lo <= hi)
	{
		AddPointToTriVerts(vertices[lo++], color);
		if (lo <= hi)
			AddPointToTriVerts(vertices[hi--], color);
	}
	// re-cap with the last pushed vertex
	SimpleVertex& last = simpleVertsTris[simpleVertTriCount-1];
	const Vector2 lastPos(last.position.x, last.position.y);
	CapTriVerts(lastPos);
}

void FrankRender::DrawSolidBox(const XForm2& xf, const Vector2& size, const DWORD color)
{
	const Vector2 vertices[4] =
	{
		xf.TransformCoord(Vector2(size.x, size.y)),
		xf.TransformCoord(Vector2(size.x, -size.y)),
		xf.TransformCoord(Vector2(-size.x, -size.y)),
		xf.TransformCoord(Vector2(-size.x, size.y)),
	};
	DrawSolidPolygon(vertices, 4, color);
}

void FrankRender::DrawOutlinedBox(const XForm2& xf, const Vector2& size, const DWORD color, const DWORD outlineColor)
{
	const Vector2 vertices[4] =
	{
		xf.TransformCoord(Vector2(size.x, size.y)),
		xf.TransformCoord(Vector2(size.x, -size.y)),
		xf.TransformCoord(Vector2(-size.x, -size.y)),
		xf.TransformCoord(Vector2(-size.x, size.y)),
	};
	DrawSolidPolygon(vertices, 4, color);
	DrawPolygon(vertices, 4, outlineColor);
}

void FrankRender::DrawCircle(const XForm2& xf, const Vector2& size, const DWORD color, int sides)
{
	Vector2 prev = xf.TransformCoord(Vector2(0, size.y));
	CapLineVerts(prev);
	for (int i = 0; i <= sides; ++i)
	{
		const float angle = 2 * PI * i / sides;
		const Vector2 p = xf.TransformCoord(Vector2(size.x * sinf(angle), size.y * cosf(angle)));
		AddPointToLineVerts(p, color);
	}
	CapLineVerts(xf.TransformCoord(Vector2(0, size.y)));
}

void FrankRender::DrawSolidCircle(const XForm2& xf, const Vector2& size, const DWORD color, int sides)
{
	// fan as strip: center-out zigzag
	const Vector2 center = xf.TransformCoord(Vector2(0));
	CapTriVerts(center);
	for (int i = 0; i <= sides; ++i)
	{
		const float angle = 2 * PI * i / sides;
		const Vector2 p = xf.TransformCoord(Vector2(size.x * sinf(angle), size.y * cosf(angle)));
		AddPointToTriVerts(p, color);
		AddPointToTriVerts(center, color);
	}
	CapTriVerts(center);
}

void FrankRender::DrawOutlinedCircle(const XForm2& xf, const Vector2& size, const DWORD color, const DWORD outlineColor, int sides)
{
	DrawSolidCircle(xf, size, color, sides);
	DrawCircle(xf, size, outlineColor, sides);
}

void FrankRender::DrawCone(const XForm2& xf, float radius, float coneAngle, const DWORD color, int sides)
{
	const Vector2 tip = xf.TransformCoord(Vector2(0));
	CapLineVerts(tip);
	AddPointToLineVerts(tip, color);
	for (int i = 0; i <= sides; ++i)
	{
		const float angle = -coneAngle + 2 * coneAngle * i / sides;
		const Vector2 p = xf.TransformCoord(radius * Vector2::BuildFromAngle(angle));
		AddPointToLineVerts(p, color);
	}
	AddPointToLineVerts(tip, color);
	CapLineVerts(tip);
}

void FrankRender::DrawSolidCone(const XForm2& xf, float radius, float coneAngle, const DWORD color1, const DWORD color2, int sides)
{
	const Vector2 tip = xf.TransformCoord(Vector2(0));
	CapTriVerts(tip);
	for (int i = 0; i <= sides; ++i)
	{
		const float angle = -coneAngle + 2 * coneAngle * i / sides;
		const Vector2 p = xf.TransformCoord(radius * Vector2::BuildFromAngle(angle));
		AddPointToTriVerts(p, color2);
		AddPointToTriVerts(tip, color1);
	}
	CapTriVerts(tip);
}

void FrankRender::DrawThickLine(const Line2& l, float thickness, const DWORD color)
{
	DrawThickLineGradient(l, thickness, color, color);
}

void FrankRender::RenderQuadLine(const Line2& l, float thickness, const Color& color, TextureID ti)
{
	// textured quad along a segment (frankRender.cpp:1062)
	Vector2 center = 0.5f*l.p1 + 0.5f*l.p2;
	Vector2 direction = (l.p2 - l.p1);
	float length = direction.LengthAndNormalizeThis();
	XForm2 xf(center, direction.GetAngle());
	RenderQuad(xf, Vector2(thickness, length*0.5f), color, ti);
}

void FrankRender::RenderScreenSpaceTile(const IntVector2& tilePos, const IntVector2& tileSize, const Matrix44& matrix, const Color& color, TextureID ti, const int tileRotation, const bool tileMirror)
{
	// screen-space tile draw (HUD icons from a tile sheet)
	if (!webGLContext || color.a == 0)
		return;

	FrankMat44Base pixelToClip;
	FrankMatrixIdentity(pixelToClip);
	pixelToClip._11 = 2.0f / g_backBufferWidth;
	pixelToClip._22 = -2.0f / g_backBufferHeight;
	pixelToClip._41 = -1;
	pixelToClip._42 = 1;

	float m[16];
	WebMatMul(m, &matrix.GetMatrixBase()._11, &pixelToClip._11);

	FrankWebDeviceState& s = FrankWebGetDeviceState();
	float savedView[16], savedProj[16];
	memcpy(savedView, s.view, sizeof(savedView));
	memcpy(savedProj, s.projection, sizeof(savedProj));
	memcpy(s.view, webIdentity, sizeof(webIdentity));
	memcpy(s.projection, webIdentity, sizeof(webIdentity));

	Matrix44 uvMatrix = WebBuildTileUVMatrix(tilePos, tileSize, tileRotation, tileMirror);
	WebDraw(m, color, GetTexture(ti), NULL, &uvMatrix.GetMatrixBase()._11, 0, false, NULL, 0, GL_TRIANGLE_STRIP);

	memcpy(s.view, savedView, sizeof(savedView));
	memcpy(s.projection, savedProj, sizeof(savedProj));
}

void FrankRender::DrawThickLineGradient(const Line2& l, float thickness, const DWORD color1, const DWORD color2)
{
	const Vector2 direction = (l.p2 - l.p1).Normalize();
	const Vector2 normal = direction.RotateRightAngle() * (0.5f * thickness);
	const Vector2 verts[4] = { l.p1 + normal, l.p1 - normal, l.p2 + normal, l.p2 - normal };
	CapTriVerts(verts[0]);
	AddPointToTriVerts(verts[0], color1);
	AddPointToTriVerts(verts[1], color1);
	AddPointToTriVerts(verts[2], color2);
	AddPointToTriVerts(verts[3], color2);
	CapTriVerts(verts[3]);
}

////////////////////////////////////////////////////////////////////////////////////////
// terrain rendering - port of TerrainRender::RenderSlow using TerrainTile's cached
// edge polygons, batched into textured triangles per texture, pass-aware (phase 5)
////////////////////////////////////////////////////////////////////////////////////////

static const int webTerrainMaxVerts = 8192;
static WebTexVertex webTerrainVerts[webTerrainMaxVerts];
static int webTerrainVertCount = 0;
static FrankWebTexture* webTerrainTexture = NULL;

// expand each tile outward from its center so neighbors overlap a hair - gl
// rasterizes the independently-built tile polys with hairline cracks otherwise
// (fraction of a tile; ~0.02 is under a screen pixel at gameplay zoom)
// vertex expansion to hide hairline tile seams - off by default: the cracks turned
// out to be the game overriding tileSetUVScale above its 1-1/32 default (less uv
// inset than intended); kept as a console tunable in case a future game needs it
ConsoleCommandSimple(float, webTerrainTileExpand, 0.0f);

static void WebFlushTerrainVerts()
{
	if (!webTerrainVertCount)
		return;
	WebDraw(NULL, Color::White(), webTerrainTexture, NULL, &webIdentity[0], 0, false,
		webTerrainVerts, webTerrainVertCount, GL_TRIANGLES);
	if (webShadowFlush >= 3 && DeferredRender::GetRenderPassIsShadow())
		glFlush();
	if (webProbeCur && DeferredRender::GetRenderPassIsShadow())
	{
		// hash the exact bytes handed to the gpu (see webProbeShadowHash)
		const unsigned char* p = (const unsigned char*)webTerrainVerts;
		const int byteCount = webTerrainVertCount * (int)sizeof(WebTexVertex);
		unsigned h = webProbeShadowHash;
		for (int i = 0; i < byteCount; ++i)
			h = (h ^ p[i]) * 16777619u;
		webProbeShadowHash = h;
	}
	if (webProbeCur)
	{
		webProbeCur->flushedVerts += webTerrainVertCount;
		// gl error check only on shadow passes - that's the pass that goes missing.
		// gated: glGetError is a synchronous gpu-process round trip and this fired on
		// every terrain flush regardless of whether error checking was wanted
		if (webCheckErrors && DeferredRender::GetRenderPassIsShadow() && glGetError() != GL_NO_ERROR)
			++webProbeCur->glErrors;
	}
	webTerrainVertCount = 0;
}

// --- geometry helpers shared by the immediate and the cached path, so the two can
//     never drift apart (the cache is only correct if it bakes the exact same verts) ---

// uv transform for one tile surface. two texturing modes, matching
// TerrainRender::RenderTile (terrainRender.cpp:976): tile sets (uv window into the
// sheet) or per-surface textures wrapped in world space
static Matrix44 WebTerrainTileUVMatrix(const Vector2& tileOffset, BYTE surfaceID, BYTE tileSet,
	const GameSurfaceInfo& surfaceInfo, BYTE rotation, bool mirror)
{
	if (Terrain::tileSetCount > 0)
	{
		// tile set mode: uv window for this surface's cell in the sheet
		return WebBuildTileUVMatrix(IntVector2(surfaceID % 16, surfaceID / 16), IntVector2(16, 16), rotation, mirror);
	}

	// per-surface texture wrapped across world space (terrainRender.cpp:1013-1031)
	const Vector2 wrapScale = TerrainRender::textureWrapScale * surfaceInfo.textureWrapSize;
	const Vector2 scale = 1.0f / wrapScale;
	Matrix44 uvMatrix = Matrix44::BuildScale(scale.x, scale.y, 0);
	Vector2 texturePos = Vector2(1, -1) * tileOffset / (TerrainTile::GetSize() * wrapScale);
	texturePos.y -= 1 / wrapScale.y;	// matches the d3d path's alignment adjustment
	uvMatrix.GetMatrixBase()._31 = texturePos.x;
	uvMatrix.GetMatrixBase()._32 = texturePos.y;
	return uvMatrix;
}

// texture + color for a terrain render group (one surface id on one tile set) in the
// CURRENT pass. this is the only pass-dependent part of terrain rendering, which is what
// makes the geometry cacheable. (terrainRender.cpp RenderCached:490-517)
static void WebTerrainGroupPassState(BYTE surfaceID, BYTE tileSet, int layer,
	FrankWebTexture*& texOut, Color& colorOut)
{
	const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surfaceID);
	const TextureID ti = (Terrain::tileSetCount > 0) ? Terrain::GetTileSetTexture(tileSet) : surfaceInfo.ti;

	Color color = (layer == 0) ? surfaceInfo.color : surfaceInfo.backgroundColor;
	if (DeferredRender::GetRenderPassIsShadow())
	{
		// matches RenderCached:492 exactly (the override check is unconditional there)
		color = surfaceInfo.shadowColor;
		if (Terrain::GetTileSetInfo(tileSet).hasShadowOverride)
			color = Terrain::GetTileSetInfo(tileSet).shadowOverrideColor;
	}

	// pass-aware texture selection (GetTexture returns the emissive companion during
	// the emissive pass; NULL means render black there, like RenderCached:506)
	FrankWebTexture* tex = g_render->GetTexture(ti);
	if (DeferredRender::GetRenderPassIsEmissive())
		color = tex ? Color::White(color.a) : Color::Black(color.a);
	if (!tex)
		tex = g_render->GetTexture(ti, false);

	texOut = tex;
	colorOut = color;
}

// fan-triangulate one tile surface's cached edge polygon into `out`; returns verts written
static int WebTerrainEmitTileVerts(WebTexVertex* out, const Vector2& tileOffset, BYTE edgeIndex,
	const Matrix44& uvMatrix, const Color& color)
{
	const Vector2* polyVerts;
	int polyVertCount;
	TerrainTile::GetVertList(polyVerts, polyVertCount, edgeIndex);
	if (polyVertCount < 3)
		return 0;

	const FrankMat44Base& um = uvMatrix.GetMatrixBase();
	const float tileSize = TerrainTile::GetSize();
	const float half = 0.5f * tileSize;

	const DWORD c = (DWORD)color;
	const unsigned char cr = (unsigned char)((c >> 16) & 0xff);
	const unsigned char cg = (unsigned char)((c >> 8) & 0xff);
	const unsigned char cb = (unsigned char)(c & 0xff);
	const unsigned char ca = (unsigned char)((c >> 24) & 0xff);

	int count = 0;
	for (int i = 1; i < polyVertCount - 1; ++i)
	{
		const Vector2* tri[3] = { &polyVerts[0], &polyVerts[i], &polyVerts[i+1] };
		for (int k = 0; k < 3; ++k)
		{
			const Vector2& local = *tri[k];
			WebTexVertex& v = out[count++];
			// overlap neighbors slightly to cover rasterization cracks; uvs stay on
			// the unexpanded position so the texture window is unchanged
			v.x = tileOffset.x + half + (local.x - half) * (1 + webTerrainTileExpand);
			v.y = tileOffset.y + half + (local.y - half) * (1 + webTerrainTileExpand);
			v.z = 0;
			// local 0..tileSize -> quad-style uv (y down), then the tile-set window transform
			const float u0 = local.x / tileSize;
			const float v0 = 1.0f - local.y / tileSize;
			v.u = u0 * um._11 + v0 * um._21 + um._31;
			v.v = u0 * um._12 + v0 * um._22 + um._32;
			v.r = cr; v.g = cg; v.b = cb; v.a = ca;
		}
	}
	return count;
}

// immediate path: triangulate straight into the streaming batch. used for uncached
// patches (cache warm-up) and for the whole terrain in edit mode
static void WebRenderTerrainTileSurface(const Vector2& tileOffset, BYTE edgeIndex, BYTE surfaceID,
	BYTE tileSet, int layer, BYTE rotation, bool mirror)
{
	const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surfaceID);
	const Matrix44 uvMatrix = WebTerrainTileUVMatrix(tileOffset, surfaceID, tileSet, surfaceInfo, rotation, mirror);

	FrankWebTexture* tex;
	Color finalColor;
	WebTerrainGroupPassState(surfaceID, tileSet, layer, tex, finalColor);

	if (tex != webTerrainTexture)
	{
		WebFlushTerrainVerts();
		webTerrainTexture = tex;
	}
	if (webTerrainVertCount + (TERRAIN_TILE_MAX_VERTS - 2) * 3 > webTerrainMaxVerts)
		WebFlushTerrainVerts();

	const int count = WebTerrainEmitTileVerts(&webTerrainVerts[webTerrainVertCount],
		tileOffset, edgeIndex, uvMatrix, finalColor);
	webTerrainVertCount += count;
	if (webProbeCur)
		webProbeCur->vertsSubmitted += count;
}

////////////////////////////////////////////////////////////////////////////////////////
// static terrain vertex cache - port of TerrainRender::CacheTilesPrimitives/RenderCached
////////////////////////////////////////////////////////////////////////////////////////
// Without this the terrain is re-triangulated on the cpu and re-uploaded for EVERY pass
// of EVERY frame: diffuse, emissive, directional shadow, and one light-shadow pass per
// light. The windows path has always kept this geometry in a static vertex buffer per
// patch per layer, built on stream-in and refreshed only when the terrain is deformed.
//
// Nothing about the geometry is pass-dependent - only the surface COLOR is, and that
// moves out of the vertex stream into the uColor uniform. The shader computes
// uColor.rgb * vColor.rgb * tex.rgb, so white verts plus a colored uniform is exactly
// the old baked-in vertex color, not an approximation of it.
//
// Caveat inherited from the windows path: baked uvs assume g_tileSetUVScale and
// TerrainRender::textureWrapScale do not change at runtime. Both are startup config in
// every game here; if one is changed live, run terrainCacheClear.
ConsoleCommandSimple(bool, webTerrainCache, true);

// patch-layers built per frame. a full window is ~50 of them, so a teleport or a fresh
// load warms up over a few frames instead of hitching; uncached patches render through
// the immediate path meanwhile, so nothing is ever missing from the screen
ConsoleCommandSimple(int, webTerrainCacheBuildsPerFrame, 4);

struct WebTerrainCacheSlot
{
	static const int maxGroups = 192;
	struct Group { int first, count; BYTE surfaceID, tileSet; };

	const TerrainPatch* patch;	// NULL = slot is free
	int layer;
	GLuint vao, vbo;			// kept across evictions and reused
	int vertCount;
	int groupCount;
	bool overflowed;			// more groups than we can hold - always render immediately
	Group groups[maxGroups];
};

// 128 slots covers renderWindowSize 3 (7x7 patches x 2 layers); piroot uses 2 (5x5 = 50)
static const int webTerrainCacheSlotCount = 128;
static WebTerrainCacheSlot webTerrainCacheSlots[webTerrainCacheSlotCount];
static int webTerrainCacheBuilds = 0;		// builds spent this frame
static bool webTerrainCacheStale = false;	// tiles may have changed outside gameplay mode

// print a stat line once a second: how much of the terrain is coming from the cache and
// how much is still being rebuilt on the cpu every pass
ConsoleCommandSimple(bool, webTerrainCacheDebug, false);
static int webTerrainCachedPatchDraws = 0, webTerrainImmediatePatchDraws = 0;
static int webTerrainCacheBuildsTotal = 0, webTerrainCacheDebugFrames = 0;
static int webTerrainCacheGLDraws = 0;		// gl draws issued from the cache, after merging

// build scratch, sized from the actual patch size on first use
struct WebTerrainCacheTile
{
	Vector2 pos;
	BYTE surfaceData, edgeData, tileSet, rotation;
	bool mirror;
};
static WebTerrainCacheTile* webTerrainBuildTiles = NULL;
static WebTexVertex* webTerrainBuildVerts = NULL;
static int webTerrainBuildCapacity = 0;

// group by tile set then surface id. this is finer than the windows comparator
// (IsSameRenderGroup, which merges surfaces with identical textures and colors) but
// never coarser, so it can only cost a draw call, never correctness - and unlike a
// comparator built on GameSurfaceInfo::operator< it is a guaranteed strict weak ordering
static bool WebTerrainCacheTileSort(const WebTerrainCacheTile& a, const WebTerrainCacheTile& b)
{
	if (a.tileSet != b.tileSet)
		return a.tileSet < b.tileSet;
	return a.surfaceData < b.surfaceData;
}

static void WebTerrainCacheBuild(WebTerrainCacheSlot& slot, const TerrainPatch& patch, int layer)
{
	++webTerrainCacheBuilds;
	slot.patch = &patch;
	slot.layer = layer;
	slot.groupCount = 0;
	slot.vertCount = 0;
	slot.overflowed = false;

	const int tileCapacity = Terrain::patchSize * Terrain::patchSize * 2;
	if (tileCapacity > webTerrainBuildCapacity)
	{
		delete [] webTerrainBuildTiles;
		delete [] webTerrainBuildVerts;
		webTerrainBuildTiles = new WebTerrainCacheTile[tileCapacity];
		webTerrainBuildVerts = new WebTexVertex[tileCapacity * (TERRAIN_TILE_MAX_VERTS - 2) * 3];
		webTerrainBuildCapacity = tileCapacity;
	}

	// collect every tile surface in the patch (matches CacheTilesPrimitives:337-364)
	int tileCount = 0;
	for (int x = 0; x < Terrain::patchSize; ++x)
	for (int y = 0; y < Terrain::patchSize; ++y)
	{
		const TerrainTile& tile = patch.GetTileLocal(x, y, layer);
		if (tile.IsClear())
			continue;

		const Vector2 pos = patch.GetTilePos(x, y);
		for (int surface = 0; surface < 2; ++surface)
		{
			const BYTE surfaceIndex = tile.GetSurfaceData(surface);
			if (surfaceIndex == 0 || !tile.GetSurfaceHasArea(surface))
				continue;

			WebTerrainCacheTile& out = webTerrainBuildTiles[tileCount++];
			out.pos = pos;
			out.surfaceData = surfaceIndex;
			out.edgeData = (surface == 0) ? tile.GetEdgeData() : tile.GetInvertedEdgeData();
			out.tileSet = tile.GetTileSet();
			out.rotation = tile.GetRotation();
			out.mirror = tile.GetMirror();
		}
	}

	std::sort(webTerrainBuildTiles, webTerrainBuildTiles + tileCount, WebTerrainCacheTileSort);

	// emit verts, opening a new group whenever the tile set or surface changes
	for (int i = 0; i < tileCount; ++i)
	{
		const WebTerrainCacheTile& tile = webTerrainBuildTiles[i];
		if (!slot.groupCount ||
			slot.groups[slot.groupCount-1].surfaceID != tile.surfaceData ||
			slot.groups[slot.groupCount-1].tileSet != tile.tileSet)
		{
			if (slot.groupCount == WebTerrainCacheSlot::maxGroups)
			{
				// pathological patch: keep the slot claimed so we do not retry the
				// build every frame, and let it render through the immediate path
				slot.overflowed = true;
				slot.groupCount = 0;
				slot.vertCount = 0;
				return;
			}
			WebTerrainCacheSlot::Group& group = slot.groups[slot.groupCount++];
			group.first = slot.vertCount;
			group.count = 0;
			group.surfaceID = tile.surfaceData;
			group.tileSet = tile.tileSet;
		}

		const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(tile.surfaceData);
		const Matrix44 uvMatrix = WebTerrainTileUVMatrix(tile.pos, tile.surfaceData, tile.tileSet,
			surfaceInfo, tile.rotation, tile.mirror);
		// color is white here and supplied per-group by the uniform at draw time
		const int count = WebTerrainEmitTileVerts(&webTerrainBuildVerts[slot.vertCount],
			tile.pos, tile.edgeData, uvMatrix, Color::White());
		slot.vertCount += count;
		slot.groups[slot.groupCount-1].count += count;
	}

	if (!slot.vao)
	{
		glGenVertexArrays(1, &slot.vao);
		glGenBuffers(1, &slot.vbo);
	}
	WebBindVAO(slot.vao);
	WebBindArrayBuffer(slot.vbo);

	// one full respecify per build - never bufferSubData. STATIC_DRAW puts this buffer on
	// angle/d3d11's DIRECT path, which is the right path for data that is written once and
	// only read afterwards (unlike the streaming pool, which sub-updates ~1300x a frame and
	// must stay DYNAMIC - see the warning on webPoolStaticUsage).
	glBufferData(GL_ARRAY_BUFFER, slot.vertCount * (int)sizeof(WebTexVertex),
		slot.vertCount ? webTerrainBuildVerts : NULL, GL_STATIC_DRAW);

	// !! The attribute pointers are re-established on EVERY build, not just the first. !!
	// angle's VertexArray11 handles DIRTY_BIT_BUFFER_DATA only for VertexStorageType::STATIC
	// and lets DIRECT fall through doing nothing, so re-uploading a DIRECT buffer does not
	// on its own mark the attribs for re-translation - a patch rebuilt after a Deform could
	// keep drawing its OLD geometry. That is the same missed dirty bit that made
	// webPoolStaticUsage render stale content. Re-pointing the attributes sets
	// DIRTY_BIT_ATTRIB, which angle does honor, so the rebuild is always picked up.
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(WebTexVertex), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(WebTexVertex), (void*)(3*sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(WebTexVertex), (void*)(5*sizeof(float)));
}

static WebTerrainCacheSlot* WebTerrainCacheFind(const TerrainPatch* patch, int layer)
{
	for (int i = 0; i < webTerrainCacheSlotCount; ++i)
	{
		if (webTerrainCacheSlots[i].patch == patch && webTerrainCacheSlots[i].layer == layer)
			return &webTerrainCacheSlots[i];
	}
	return NULL;
}

// a free slot, else evict whichever cached patch is farthest from the stream center
static WebTerrainCacheSlot* WebTerrainCacheAcquire()
{
	for (int i = 0; i < webTerrainCacheSlotCount; ++i)
	{
		if (!webTerrainCacheSlots[i].patch)
			return &webTerrainCacheSlots[i];
	}

	const Vector2 center = g_gameControlBase->GetStreamCenter();
	WebTerrainCacheSlot* worst = &webTerrainCacheSlots[0];
	float worstDistance = -1;
	for (int i = 0; i < webTerrainCacheSlotCount; ++i)
	{
		const float distance = (webTerrainCacheSlots[i].patch->GetCenter() - center).LengthSquared();
		if (distance > worstDistance)
		{
			worstDistance = distance;
			worst = &webTerrainCacheSlots[i];
		}
	}
	worst->patch = NULL;
	return worst;
}

static void WebTerrainCacheDraw(const WebTerrainCacheSlot& slot)
{
	// keep submission order patch-by-patch while the cache is still warming up
	WebFlushTerrainVerts();

	// groups are laid out contiguously in build order, so any run of them that resolves
	// to the same texture and color in THIS pass collapses into a single draw. that is
	// most of them on a shadow pass, where every surface takes the same shadow color -
	// without this the cache would trade cpu rebuild cost for a pile of extra draw calls
	int i = 0;
	while (i < slot.groupCount)
	{
		FrankWebTexture* tex;
		Color color;
		WebTerrainGroupPassState(slot.groups[i].surfaceID, slot.groups[i].tileSet, slot.layer, tex, color);

		int first = slot.groups[i].first;
		int count = slot.groups[i].count;
		while (++i < slot.groupCount)
		{
			FrankWebTexture* nextTex;
			Color nextColor;
			WebTerrainGroupPassState(slot.groups[i].surfaceID, slot.groups[i].tileSet, slot.layer,
				nextTex, nextColor);
			if (nextTex != tex || slot.groups[i].first != first + count)
				break;
			if (nextColor.r != color.r || nextColor.g != color.g ||
				nextColor.b != color.b || nextColor.a != color.a)
				break;
			count += slot.groups[i].count;
		}

		if (count < 3)
			continue;
		WebDraw(NULL, color, tex, NULL, &webIdentity[0], 0, false, NULL, count,
			GL_TRIANGLES, slot.vao, first);
		++webTerrainCacheGLDraws;
		if (webProbeCur)
			webProbeCur->vertsSubmitted += count;
	}
	webTerrainTexture = NULL;	// the streaming batch no longer knows what is bound
}

// TerrainRender cache hooks - the real versions of what webStubs.cpp used to no-op.
// ClearCache comes from gameControlBase on a world reset; RefereshCached comes from
// Terrain::UpdatePost for every patch a Deform touched, which is the destructible
// terrain path. Slots keep their gl objects so a refresh is just a respecify.
void TerrainRender::ClearCache()
{
	for (int i = 0; i < webTerrainCacheSlotCount; ++i)
		webTerrainCacheSlots[i].patch = NULL;
}

void TerrainRender::RefereshCached(TerrainPatch& patch)
{
	for (int i = 0; i < webTerrainCacheSlotCount; ++i)
	{
		if (webTerrainCacheSlots[i].patch == &patch)
			webTerrainCacheSlots[i].patch = NULL;
	}
}

ConsoleFunction(terrainCacheClear)
{
	g_terrainRender.ClearCache();
	GetDebugConsole().AddLine(L"terrain vertex cache cleared");
}

// called once per frame from FrankWebRenderBegin
static void WebTerrainCacheBeginFrame()
{
	webTerrainCacheBuildsTotal += webTerrainCacheBuilds;
	webTerrainCacheBuilds = 0;

	if (!webTerrainCacheDebug)
		return;
	if (++webTerrainCacheDebugFrames < 60)
		return;

	int live = 0, verts = 0, groups = 0;
	for (int i = 0; i < webTerrainCacheSlotCount; ++i)
	{
		if (!webTerrainCacheSlots[i].patch)
			continue;
		++live;
		verts += webTerrainCacheSlots[i].vertCount;
		groups += webTerrainCacheSlots[i].groupCount;
	}
	// patch draws are counted across every pass of the last 60 frames, so "immediate 0"
	// is the goal state: no patch is being re-triangulated on the cpu anywhere
	printf("TERRAINCACHE slots=%d verts=%d groups=%d builds=%d patchDraws cached=%d immediate=%d"
		" terrainDraws/frame=%d TOTALDRAWS/frame=%d POOLKB/frame=%d"
		" spriteQuads/frame=%d inBatches=%d unbakeableColors=%d blendChanges/frame=%d"
		" pumps=%d fboPingPongs=%d texPingPongs=%d\n",
		live, verts, groups, webTerrainCacheBuildsTotal,
		webTerrainCachedPatchDraws, webTerrainImmediatePatchDraws,
		webTerrainCacheGLDraws / 60, webTotalDraws / 60, webPoolBytes / (60 * 1024),
		webBatchQuadsQueued / 60, webBatchFlushes / 60, webBatchUnbakeableColors,
		webBlendChanges / 60, webContextPumps, webFBOPingPongs / 60, webPingPongBinds / 60);
	webBlendChanges = 0;
	webContextPumps = 0;
	webFBOPingPongs = 0;
	webPingPongBinds = 0;
	webTotalDraws = 0;
	webPoolBytes = 0;
	webBatchQuadsQueued = 0;
	webBatchFlushes = 0;
	webTerrainCacheDebugFrames = 0;
	webTerrainCacheBuildsTotal = 0;
	webTerrainCachedPatchDraws = 0;
	webTerrainImmediatePatchDraws = 0;
	webTerrainCacheGLDraws = 0;
}

void TerrainLayerRender::Render()
{
	if (!webGLContext || !g_terrain)
		return;

	// per-pass layer gating (terrainRender.cpp:1118-1134)
	const bool emissivePass = DeferredRender::GetRenderPassIsEmissive();
	if (emissivePass)
	{
		if (!emissiveLighting)
			return;
	}
	else if (DeferredRender::GetRenderPassIsShadow())
	{
		if (DeferredRender::GetRenderPassIsLight() || DeferredRender::GetRenderPassIsVision())
		{
			if (!lightShadows)
				return;
		}
		else if (!directionalShadows)
			return;
	}
	// mark terrain batches for the pass-2 filter (independent of the probe)
	webTerrainBatchActive = true;

	// flicker probe: record what this pass/layer submits and the camera window used
	// to cull it (a wrong window here = patches silently missing from the shadow map).
	// frozen during pass-2/retry re-renders so stats and hash reflect pass 1 only
	webProbeCur = NULL;
	if (webShadowProbe && !webShadowRerenderActive && layer >= 0 && layer < 2)
	{
		const int pass = (int)DeferredRender::GetRenderPass();
		if (pass >= 0 && pass < webProbePassCount)
		{
			webProbeCur = &webProbeTerrain[pass][layer];
			webProbeCur->ran = true;
			const Box2AABB& bb = g_cameraBase->GetCameraBBox();
			webProbeCur->camL = bb.lowerBound.x; webProbeCur->camB = bb.lowerBound.y;
			webProbeCur->camR = bb.upperBound.x; webProbeCur->camT = bb.upperBound.y;
		}
	}


	// only render the window of patches around the stream center, matching the
	// windows path (terrainRender.cpp:141-153) - NOT the whole world. tile data for
	// every patch is always in memory, so without this a zoomed-out camera renders
	// the entire planet and the shadow pass pays for far-off patches every frame.
	const IntVector2 patchOffset = g_terrain->GetPatchIndex(g_gameControlBase->GetStreamCenter());
	const int window = Terrain::renderWindowSize;

	// the cache is gameplay-only, matching the windows path (terrainRender.cpp:268) - the
	// editor rewrites tiles without going through Deform, so anything cached across an
	// edit session would be stale. leaving edit mode invalidates everything once.
	const bool useCache = webTerrainCache && g_gameControlBase->IsGameplayMode();
	if (!useCache)
		webTerrainCacheStale = true;
	else if (webTerrainCacheStale)
	{
		g_terrainRender.ClearCache();
		webTerrainCacheStale = false;
	}

	for (int xp = patchOffset.x - window; xp <= patchOffset.x + window; ++xp)
	for (int yp = patchOffset.y - window; yp <= patchOffset.y + window; ++yp)
	{
		if (!g_terrain->IsPatchIndexValid(IntVector2(xp, yp)))
			continue;
		const TerrainPatch* patch = g_terrain->GetPatch(xp, yp);
		if (!patch || !g_cameraBase->CameraTest(patch->GetAABB()))
			continue;
		if (webProbeCur)
			++webProbeCur->patchesPassed;

		if (useCache)
		{
			WebTerrainCacheSlot* slot = WebTerrainCacheFind(patch, layer);
			if (!slot && webTerrainCacheBuilds < Max(1, webTerrainCacheBuildsPerFrame))
			{
				slot = WebTerrainCacheAcquire();
				WebTerrainCacheBuild(*slot, *patch, layer);
			}
			if (slot && !slot->overflowed)
			{
				++webTerrainCachedPatchDraws;
				WebTerrainCacheDraw(*slot);
				continue;
			}
		}
		++webTerrainImmediatePatchDraws;

		// immediate path: rebuild this patch's verts on the cpu. only reached while the
		// cache is warming up, for a patch with too many groups to cache, or in the editor.
		//
		// !! NO PER-TILE CAMERA CULL HERE !! It must submit exactly what the cached path
		// submits - the whole patch - or the two disagree and the geometry reaching the
		// shadow passes changes from frame to frame as patches finish caching. That shows
		// up as a directional shadow that pulses (most visible through water, which only
		// partly blocks light). The windows path has the same property: RenderCached does
		// no per-tile test either.
		for (int x = 0; x < Terrain::patchSize; ++x)
		for (int y = 0; y < Terrain::patchSize; ++y)
		{
			const TerrainTile& tile = patch->GetTileLocal(x, y, layer);
			if (tile.IsClear())
				continue;

			const Vector2 tileOffset = patch->GetTilePos(x, y);
			if (webProbeCur)
				++webProbeCur->tilesSubmitted;

			const BYTE rotation = tile.GetRotation();
			const bool mirror = tile.GetMirror();
			for (int surface = 0; surface < 2; ++surface)
			{
				const BYTE surfaceIndex = tile.GetSurfaceData(surface);
				if (surfaceIndex == 0 || !tile.GetSurfaceHasArea(surface))
					continue;

				const BYTE edge = (surface == 0) ? tile.GetEdgeData() : tile.GetInvertedEdgeData();
				WebRenderTerrainTileSurface(tileOffset, edge, surfaceIndex, tile.GetTileSet(),
					layer, rotation, mirror);
			}
		}
	}

	WebFlushTerrainVerts();
	webProbeCur = NULL;
	webTerrainBatchActive = false;
}

// called once per rendered frame (frankEngineWeb.cpp) - compare this frame's shadow-pass
// terrain submission against the previous frame and dump full state when it drops.
// each dump discriminates the failure stage:
//   ran=0            -> pass never reached terrain (RenderShadowMap early-out or render list)
//   ran=1 patches=0  -> camera-window culling rejected everything (see cam bounds vs prev)
//   patches ok, verts down -> per-tile cull or surface gating
//   all counts normal but glitch visible on screen -> gpu side (render target / blur / light sampling)
// EXPERIMENT: replace the game's entire frame with a self-verifying grid drawn through
// the SAME wasm -> emscripten -> pool -> drawArrays path the real rendering uses. this
// is the drawTest page, but inside the wasm app. flicker only reproduces on chrome's
// d3d11 backend (gl and vulkan are clean), and pure-js pages never reproduce it, so the
// question is whether a MINIMAL wasm frame still trips it.
//   0 = normal game, 1 = verified grid
// prints FLICKERGRID lines: a mismatch means a draw produced nothing (or stale pixels).
// (a real global, not ConsoleCommandSimple's file-static - frankEngineWeb.cpp reads it)
int webRenderMode = 0;
ConsoleCommand(webRenderMode, webRenderMode);
ConsoleCommandSimple(int, webGridCells, 288);		// draws per frame in grid mode

static int webGridFrame = 0;
static int webGridFails = 0;

static void WebGridReport(int failed, int cells)
{
	if (failed)
	{
		webGridFails += failed;
		printf("FLICKERGRID frame %d: %d/%d cells MISSED (total %d)\n",
			webGridFrame, failed, cells, webGridFails);
	}
	else if (webGridFrame % 300 == 0)
		printf("FLICKERGRID ok frame %d: %d cells verified, %d total misses\n",
			webGridFrame, cells, webGridFails);
	++webGridFrame;
}

// draws `cols*rows` squares filling the NDC rect (nx0,ny0)-(nx0+nw,ny0+nh) through the
// game's own pool+drawArrays path. verification is separate so mode 3 can put the whole
// game frame between the draws and the readback.
static int WebVerifyGridDrawOnly(int cols, int rows, float nx0, float ny0, float nw, float nh)
{
	glUseProgram(webProgram);
	glUniformMatrix4fv(webLocs.worldViewProj, 1, GL_FALSE, webIdentity);
	glUniformMatrix4fv(webLocs.uvMatrix, 1, GL_FALSE, webIdentity);
	glUniformMatrix4fv(webLocs.uvMatrix2, 1, GL_FALSE, webIdentity);
	glUniform4f(webLocs.color, 1, 1, 1, 1);
	glUniform1i(webLocs.flags, 1);	// ignore texture rgb, use vertex color
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, webWhiteTexture);
	glUniform1i(webLocs.texture, 0);

	const int cells = cols * rows;
	const float cw = nw / cols, ch = nh / rows;
	WebTexVertex verts[6];
	for (int i = 0; i < cells; ++i)
	{
		const int cx = i % cols, cy = i / cols;
		const float x0 = nx0 + cx * cw, y0 = ny0 + cy * ch;
		// per-frame color so STALE pixels are caught too, not just black ones
		const unsigned char r = (unsigned char)(40 + (i * 7 + webGridFrame) % 200);
		const unsigned char g = (unsigned char)(40 + (i * 13 + webGridFrame * 3) % 200);
		const unsigned char b = (unsigned char)(40 + (i * 29 + webGridFrame * 7) % 200);
		const float px[6] = { x0, x0 + cw, x0, x0 + cw, x0 + cw, x0 };
		const float py[6] = { y0, y0, y0 + ch, y0, y0 + ch, y0 + ch };
		for (int v = 0; v < 6; ++v)
		{
			verts[v].x = px[v]; verts[v].y = py[v]; verts[v].z = 0;
			verts[v].u = 0; verts[v].v = 0;
			verts[v].r = r; verts[v].g = g; verts[v].b = b; verts[v].a = 255;
		}
		const int first = webTexPool.Upload(verts, 6);
		for (int rep = Max(1, webDrawRepeat); rep--; )
			glDrawArrays(GL_TRIANGLES, first, 6);
	}
	return 0;
}

// read back a grid that fills the whole bound target (mode 3: drawn earlier this frame)
static int WebVerifyGridReadOnly(int cols, int rows, int targetW, int targetH)
{
	const int cells = cols * rows;
	int failed = 0, firstBad = -1;
	unsigned char pixel[4];
	for (int i = 0; i < cells; ++i)
	{
		const int cx = i % cols, cy = i / cols;
		const int px = (int)((cx + 0.5f) * targetW / cols);
		const int py = (int)((cy + 0.5f) * targetH / rows);
		glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		const int wantR = 40 + (i * 7 + webGridFrame) % 200;
		const int wantG = 40 + (i * 13 + webGridFrame * 3) % 200;
		const int wantB = 40 + (i * 29 + webGridFrame * 7) % 200;
		if (abs((int)pixel[0] - wantR) > 6 || abs((int)pixel[1] - wantG) > 6 || abs((int)pixel[2] - wantB) > 6)
		{
			if (firstBad < 0)
			{
				firstBad = i;
				printf("FLICKERGRID frame %d: cell %d wanted rgb(%d,%d,%d) got rgb(%d,%d,%d)\n",
					webGridFrame, i, wantR, wantG, wantB, pixel[0], pixel[1], pixel[2]);
			}
			++failed;
		}
	}
	return failed;
}

// draw + immediate readback against the default framebuffer (modes 1 and 2)
static int WebVerifyGrid(int cols, int rows, float nx0, float ny0, float nw, float nh)
{
	WebVerifyGridDrawOnly(cols, rows, nx0, ny0, nw, nh);
	const int cells = cols * rows;
	const float cw = nw / cols, ch = nh / rows;
	int failed = 0, firstBad = -1;
	unsigned char pixel[4];
	for (int i = 0; i < cells; ++i)
	{
		const int cx = i % cols, cy = i / cols;
		// ndc cell center -> window pixel
		const float ndcX = nx0 + (cx + 0.5f) * cw;
		const float ndcY = ny0 + (cy + 0.5f) * ch;
		const int px = (int)((ndcX * 0.5f + 0.5f) * g_backBufferWidth);
		const int py = (int)((ndcY * 0.5f + 0.5f) * g_backBufferHeight);
		glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
		const int wantR = 40 + (i * 7 + webGridFrame) % 200;
		const int wantG = 40 + (i * 13 + webGridFrame * 3) % 200;
		const int wantB = 40 + (i * 29 + webGridFrame * 7) % 200;
		if (abs((int)pixel[0] - wantR) > 6 || abs((int)pixel[1] - wantG) > 6 || abs((int)pixel[2] - wantB) > 6)
		{
			if (firstBad < 0)
			{
				firstBad = i;
				printf("FLICKERGRID frame %d: cell %d wanted rgb(%d,%d,%d) got rgb(%d,%d,%d)\n",
					webGridFrame, i, wantR, wantG, wantB, pixel[0], pixel[1], pixel[2]);
			}
			++failed;
		}
	}
	return failed;
}

// mode 1: the whole frame is the grid. mode 2: the game renders normally and the grid
// is overlaid last - an OBJECTIVE detector inside the real frame, so bisecting game
// systems (lightEnable, webDrawSkipMask, ...) gives hard miss counts, not eyeballing.
void WebMinimalRenderFrame()
{
	WebFlushSpriteBatch();
	if (!webGLContext)
		return;

	const int cols = 24;
	const int cells = Cap(webGridCells, 1, 1152);
	const int rows = (cells + cols - 1) / cols;

	WebBindFBO(0);
	glViewport(0, 0, g_backBufferWidth, g_backBufferHeight);
	WebSetBlendEnabled(false);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	const int failed = WebVerifyGrid(cols, rows, -1, -1, 2, 2);
	WebGridReport(failed, cols * rows);
}

// mode 3: the fair version of the strip test. the mode-2 overlay never missed, but it
// is drawn and read back in the same breath - readPixels forces those draws through, so
// it has no exposure window. the shadow map is rendered EARLY and read LATE, and that
// is what breaks. so: render the grid into our own off-screen target early in the
// frame, let the whole game frame happen, then verify at the end. same lifecycle as
// the shadow map, but with content we fully control.
static GLuint webGridFBO = 0, webGridTex = 0;
static const int webGridTexSize = 512;

void WebGridRenderEarly()
{
	WebFlushSpriteBatch();
	if (!webGLContext || webRenderMode != 3)
		return;

	if (!webGridFBO)
	{
		glGenTextures(1, &webGridTex);
		glBindTexture(GL_TEXTURE_2D, webGridTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, webGridTexSize, webGridTexSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glGenFramebuffers(1, &webGridFBO);
		WebBindFBO(webGridFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, webGridTex, 0);
	}

	const int cols = 24;
	const int rows = Cap(webGridCells / cols, 1, 24);
	WebBindFBO(webGridFBO);
	glViewport(0, 0, webGridTexSize, webGridTexSize);
	WebSetBlendEnabled(false);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	WebVerifyGridDrawOnly(cols, rows, -1, -1, 2, 2);
	WebBindFBO(0);
	glViewport(0, 0, g_backBufferWidth, g_backBufferHeight);
}

void WebGridVerifyLate()
{
	WebFlushSpriteBatch();
	if (!webGLContext || webRenderMode != 3 || !webGridFBO)
		return;

	const int cols = 24;
	const int rows = Cap(webGridCells / cols, 1, 24);
	WebBindFBO(webGridFBO);
	const int failed = WebVerifyGridReadOnly(cols, rows, webGridTexSize, webGridTexSize);
	WebBindFBO(0);
	WebGridReport(failed, cols * rows);
}

// overlay strip drawn after the real game frame (mode 2)
void WebGridOverlayFrame()
{
	WebFlushSpriteBatch();
	if (!webGLContext || webRenderMode != 2)
		return;

	const int cols = 24;
	const int rows = Cap(webGridCells / cols, 1, 12);

	WebBindFBO(0);
	glViewport(0, 0, g_backBufferWidth, g_backBufferHeight);
	WebSetBlendEnabled(false);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DEPTH_TEST);

	// bottom strip, tall enough for the requested rows
	const float h = 0.12f * rows;
	const int failed = WebVerifyGrid(cols, rows, -1, -1, 2, h);
	WebGridReport(failed, cols * rows);
}

void WebFlickerProbeEndFrame()
{
	++webProbeFrame;
	webMeterFBO = 0;	// async meter re-discovers the shadow target every frame
	if (!webShadowProbe)
		return;

	const int LS = (int)DeferredRender::RenderPass_lightShadow;
	const int DF = (int)DeferredRender::RenderPass_diffuse;
	const WebProbeTerrainStats* sNow = webProbeTerrain[LS];
	const WebProbeTerrainStats* sPrev = webProbeTerrainPrev[LS];
	const WebProbeTerrainStats* dNow = webProbeTerrain[DF];
	const WebProbeTerrainStats* dPrev = webProbeTerrainPrev[DF];
	const int shadowNow = sNow[0].vertsSubmitted + sNow[1].vertsSubmitted;
	const int shadowPrev = sPrev[0].vertsSubmitted + sPrev[1].vertsSubmitted;
	const int diffuseNow = dNow[0].vertsSubmitted + dNow[1].vertsSubmitted;
	const int diffusePrev = dPrev[0].vertsSubmitted + dPrev[1].vertsSubmitted;

	const char* why = NULL;
	if (webProbeFrame > 120 && diffuseNow * 3 > diffusePrev * 2)	// only when screen content is stable
	{
		if (shadowPrev > 300 && shadowNow * 2 < shadowPrev)
			why = "shadow-pass terrain dropped vs last frame";
		else if (sPrev[0].vertsSubmitted > 300 && sNow[0].vertsSubmitted * 2 < sPrev[0].vertsSubmitted
			&& sNow[1].vertsSubmitted * 4 > sPrev[1].vertsSubmitted * 3)
			why = "FOREGROUND layer only dropped from shadow pass";
		else if (!sNow[0].ran && !sNow[1].ran && (sPrev[0].ran || sPrev[1].ran))
			why = "shadow pass never reached terrain (early out / pass skipped)";
	}
	if (!why && (sNow[0].glErrors || sNow[1].glErrors))
		why = "GL error during shadow-pass terrain flush";

	if (why && webProbeDumpCount < 30)
	{
		++webProbeDumpCount;
		printf("FLICKERPROBE frame %d: %s\n", webProbeFrame, why);
		for (int layer = 0; layer < 2; ++layer)
		{
			printf("  shadow L%d: ran=%d patches=%d tiles=%d verts=%d flushed=%d glErr=%d cam=(%.0f,%.0f)-(%.0f,%.0f) | prev ran=%d patches=%d tiles=%d verts=%d cam=(%.0f,%.0f)-(%.0f,%.0f)\n",
				layer, (int)sNow[layer].ran, sNow[layer].patchesPassed, sNow[layer].tilesSubmitted, sNow[layer].vertsSubmitted, sNow[layer].flushedVerts, sNow[layer].glErrors,
				sNow[layer].camL, sNow[layer].camB, sNow[layer].camR, sNow[layer].camT,
				(int)sPrev[layer].ran, sPrev[layer].patchesPassed, sPrev[layer].tilesSubmitted, sPrev[layer].vertsSubmitted,
				sPrev[layer].camL, sPrev[layer].camB, sPrev[layer].camR, sPrev[layer].camT);
			printf("  diffuse L%d: ran=%d patches=%d tiles=%d verts=%d | prev verts=%d\n",
				layer, (int)dNow[layer].ran, dNow[layer].patchesPassed, dNow[layer].tilesSubmitted, dNow[layer].vertsSubmitted, dPrev[layer].vertsSubmitted);
		}
		printf("  draws: diffuse %d calls/%d verts (prev %d/%d), lightShadow %d/%d (prev %d/%d), dirShadow %d/%d, emissive %d/%d\n",
			webProbeDrawCalls[DF], webProbeDrawVerts[DF], webProbeDrawCallsPrev[DF], webProbeDrawVertsPrev[DF],
			webProbeDrawCalls[LS], webProbeDrawVerts[LS], webProbeDrawCallsPrev[LS], webProbeDrawVertsPrev[LS],
			webProbeDrawCalls[DeferredRender::RenderPass_directionalShadow], webProbeDrawVerts[DeferredRender::RenderPass_directionalShadow],
			webProbeDrawCalls[DeferredRender::RenderPass_emissive], webProbeDrawVerts[DeferredRender::RenderPass_emissive]);
		printf("  state: shadowEnable=%d lightEnable=%d interp=%.3f\n",
			(int)DeferredRender::shadowEnable, (int)DeferredRender::lightEnable, g_interpolatePercent);
	}

	// gpu-side detector: read sample strips of the ACTUAL shadow-map pixels. if the
	// content changes hard for one frame while every terrain vert was submitted, we
	// caught the glitch red-handed - download the whole map as a png for inspection.
	const bool vertsStable = shadowPrev > 300 && shadowNow * 4 > shadowPrev * 3;
	if (webProbeShadowFBO >= 0 && webProbeShadowW > 8 && webProbeShadowH > 8)
	{
		GLint prevFBO = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
		// read the post-swap shadow map when known (what the lights actually sampled)
		WebBindFBO((GLuint)((webProbeShadowFBOFinal >= 0) ? webProbeShadowFBOFinal : webProbeShadowFBO));
		WebProbeReadStrips(webProbeStripAlpha, webProbeStripLum);

		bool jumped = false, preJumped = false;
		for (int s = 0; s < 3; ++s)
		{
			if (webProbeStripAlphaPrev[s] >= 0)
			{
				const float dA = fabsf(webProbeStripAlpha[s] - webProbeStripAlphaPrev[s]);
				const float dL = fabsf(webProbeStripLum[s] - webProbeStripLumPrev[s]);
				if (dA > Max(0.08f, webProbeStripAlphaPrev[s] * 0.3f) || dL > Max(0.08f, webProbeStripLumPrev[s] * 0.3f))
					jumped = true;
			}
			if (webProbePreLumPrev[s] >= 0 && webProbePreLum[s] >= 0)
			{
				const float dP = fabsf(webProbePreLum[s] - webProbePreLumPrev[s]);
				if (dP > Max(0.08f, webProbePreLumPrev[s] * 0.3f))
					preJumped = true;
			}
		}
		if (webProbeForeignDraws > 0)
			jumped = true;	// something scribbled on the shadow map - always capture that

		if (jumped && vertsStable && webProbeFrame > 120
			&& webProbeCaptures < 6 && webProbeFrame - webProbeLastCaptureFrame > 30)
		{
			++webProbeCaptures;
			webProbeLastCaptureFrame = webProbeFrame;
			printf("FLICKERPROBE CAPTURE frame %d: shadow-map content jumped, verts stable (now %d prev %d)\n",
				webProbeFrame, shadowNow, shadowPrev);
			printf("  stage: preBlur %s, postBlur %s -> loss is %s\n",
				preJumped ? "JUMPED" : "stable", jumped ? "JUMPED" : "stable",
				preJumped ? "IN THE SHADOW PASS ITSELF" : "AFTER the shadow pass (blur or a foreign write)");
			for (int s = 0; s < 3; ++s)
				printf("  strip%d alpha %.3f->%.3f postLum %.3f->%.3f preLum %.3f->%.3f\n", s,
					webProbeStripAlphaPrev[s], webProbeStripAlpha[s],
					webProbeStripLumPrev[s], webProbeStripLum[s],
					webProbePreLumPrev[s], webProbePreLum[s]);
			printf("  fbo=%d mismatch=%d foreignDraws=%d (lastPass=%d) blurDraws=%d draws LS %d/%d verts (prev %d/%d)\n",
				webProbeShadowFBO, (int)webProbeShadowFBOMismatch,
				webProbeForeignDraws, webProbeForeignPassLast, webProbeBlurDraws,
				webProbeDrawCalls[LS], webProbeDrawVerts[LS], webProbeDrawCallsPrev[LS], webProbeDrawVertsPrev[LS]);
			printf("  terrainHash now=%08x prev=%08x -> %s\n",
				webProbeShadowHash, webProbeShadowHashPrev,
				(webProbeShadowHash == webProbeShadowHashPrev)
					? "IDENTICAL: cpu vertex data clean, loss is gpu-side"
					: "DIFFERENT: our vertex data changed this frame (cpu-side suspect, or camera moved)");

			// full-res grab of the glitched shadow map + the presented frame
			const int fw = webProbeShadowW, fh = webProbeShadowH;
			unsigned char* pixels = webShadowCapture ? (unsigned char*)malloc(fw * fh * 4) : NULL;
			if (pixels)
			{
				glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
				// flip rows: gl reads bottom-up, ImageData is top-down
				static unsigned char rowTemp[4096 * 4];
				for (int y = 0; y < fh / 2 && fw <= 4096; ++y)
				{
					memcpy(rowTemp, pixels + y * fw * 4, fw * 4);
					memcpy(pixels + y * fw * 4, pixels + (fh - 1 - y) * fw * 4, fw * 4);
					memcpy(pixels + (fh - 1 - y) * fw * 4, rowTemp, fw * 4);
				}
				EM_ASM({
					var w = $0;
					var h = $1;
					var ptr = $2;
					var frame = $3;
					var c = document.createElement('canvas');
					c.width = w;
					c.height = h;
					var img = new ImageData(new Uint8ClampedArray(HEAPU8.buffer, ptr, w*h*4).slice(), w, h);
					c.getContext('2d').putImageData(img, 0, 0);
					var a = document.createElement('a');
					a.download = 'flickerShadowMap_' + frame + '.png';
					a.href = c.toDataURL();
					a.click();
					var g = document.getElementById('canvas');
					if (g)
					{
						var a2 = document.createElement('a');
						a2.download = 'flickerFrame_' + frame + '.png';
						a2.href = g.toDataURL();
						a2.click();
					}
				}, fw, fh, pixels, webProbeFrame);
				free(pixels);
			}
		}

		WebBindFBO((GLuint)prevFBO);
		memcpy(webProbeStripAlphaPrev, webProbeStripAlpha, sizeof(webProbeStripAlpha));
		memcpy(webProbeStripLumPrev, webProbeStripLum, sizeof(webProbeStripLum));
		memcpy(webProbePreLumPrev, webProbePreLum, sizeof(webProbePreLum));
		if (webProbeChunksValid)
		{
			memcpy(webProbeChunksPrev, webProbeChunks, sizeof(webProbeChunks));
			webProbeChunksPrevValid = true;
			webProbeChunksValid = false;
		}
	}
	webProbeShadowFBO = -1;
	webProbeShadowFBOFinal = -1;
	webProbeShadowFBOMismatch = false;
	webProbeForeignDraws = 0;
	webProbeForeignPassLast = -1;
	webProbeBlurDraws = 0;

	// alive/baseline heartbeat every ~5s: if the glitch shows on screen with NO dump
	// near it, the terrain DID submit and the problem is gpu-side - that's a result too
	if (webProbeFrame % 300 == 0)
	{
		extern int webShadowRetryCount;
		printf("FLICKERPROBE ok frame %d: shadow verts %d (L0 %d, L1 %d), diffuse %d, lightShadow draws %d, strips a=%.2f/%.2f/%.2f, hash=%08x, RETRIES=%d\n",
			webProbeFrame, shadowNow, sNow[0].vertsSubmitted, sNow[1].vertsSubmitted, diffuseNow, webProbeDrawCalls[LS],
			webProbeStripAlpha[0], webProbeStripAlpha[1], webProbeStripAlpha[2], webProbeShadowHash, webShadowRetryCount);
	}
	webProbeShadowHashPrev = webProbeShadowHash;
	webProbeShadowHash = 2166136261u;

	memcpy(webProbeTerrainPrev, webProbeTerrain, sizeof(webProbeTerrain));
	memset(webProbeTerrain, 0, sizeof(webProbeTerrain));
	memcpy(webProbeDrawCallsPrev, webProbeDrawCalls, sizeof(webProbeDrawCalls));
	memcpy(webProbeDrawVertsPrev, webProbeDrawVerts, sizeof(webProbeDrawVerts));
	memset(webProbeDrawCalls, 0, sizeof(webProbeDrawCalls));
	memset(webProbeDrawVerts, 0, sizeof(webProbeDrawVerts));
}
