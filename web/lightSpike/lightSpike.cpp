////////////////////////////////////////////////////////////////////////////////////////
/*
	Phase 1.5 lighting spike - proves Frank Engine's deferred lighting works in WebGL2.

	Ports deferredLightShader.psh and blurShader.psh verbatim to GLSL ES 3.00 and
	replicates the light-pass recipe from deferredRender.cpp (matrix construction at
	lines 361-391, constants at 401-403/521-526, additive accumulation, blurred
	per-light shadow maps). Scene buffers (normals/specular) are synthesized
	procedurally - the real pipeline generates them as screen-space passes, so no
	game assets are involved either way.

	Debug keys: 1 diffuse, 2 normals, 3 specular, 4 shadow map (light 0), 5 final.
	See docs/plans/2026-07-22-piroot-web-phase15-lighting-spike.md
*/
////////////////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include "frankMathBase.h"

using namespace FrankMath;

static const int screenW = 800, screenH = 600;
static const int shadowMapSize = 256;
static const float TEST_PI = 3.14159265358979f;

// engine defaults from deferredRender.cpp:260-266
static const float specularPower = 12;
static const float specularHeight = 2;
static const float specularAmount = 1;

////////////////////////////////////////////////////////////////////////////////////////
// GL helpers
////////////////////////////////////////////////////////////////////////////////////////

static GLuint CompileProgram(const char* vs, const char* fs, const char* name)
{
	const GLuint p = glCreateProgram();
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
			char log[2048];
			glGetShaderInfoLog(s, sizeof(log), NULL, log);
			printf("FAILED compiling %s (%s):\n%s\n", name, i ? "fs" : "vs", log);
		}
		glAttachShader(p, s);
	}
	glLinkProgram(p);
	GLint ok = 0;
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok)
		printf("FAILED linking %s\n", name);
	return p;
}

static GLuint MakeTexture(int w, int h, const unsigned char* data)
{
	GLuint t;
	glGenTextures(1, &t);
	glBindTexture(GL_TEXTURE_2D, t);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return t;
}

struct RenderTarget { GLuint fbo, tex; };

static RenderTarget MakeRenderTarget(int w, int h)
{
	RenderTarget rt;
	rt.tex = MakeTexture(w, h, NULL);
	glGenFramebuffers(1, &rt.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt.tex, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("FAILED: framebuffer incomplete\n");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return rt;
}

////////////////////////////////////////////////////////////////////////////////////////
// Shaders
////////////////////////////////////////////////////////////////////////////////////////

// shared quad vertex shader: unit quad -> rotated/scaled/positioned in [0,1] world,
// texcoord passed as vec4 like the engine's fixed-function TEXCOORD0
static const char* vsQuad = R"GLSL(#version 300 es
layout(location = 0) in vec2 aPos;            // unit quad 0..1
uniform vec2 uPos;                            // center, world [0,1]
uniform vec2 uSize;                           // half size
uniform float uAngle;
out vec4 vTex;
void main()
{
	vec2 local = (aPos - 0.5) * 2.0 * uSize;
	float c = cos(uAngle), s = sin(uAngle);
	vec2 world = uPos + vec2(local.x*c - local.y*s, local.x*s + local.y*c);
	gl_Position = vec4(world * 2.0 - 1.0, 0.0, 1.0);
	vTex = vec4(aPos.x, 1.0 - aPos.y, 0.0, 0.0);  // engine quads are y-down in texture space
}
)GLSL";

// raw clip-space polygon vertex shader (shadow geometry)
static const char* vsClip = R"GLSL(#version 300 es
layout(location = 0) in vec2 aPos;            // already clip space
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

static const char* fsSolid = R"GLSL(#version 300 es
precision highp float;
uniform vec4 uColor;
out vec4 outColor;
void main() { outColor = uColor; }
)GLSL";

static const char* fsTex = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D uTex;
uniform vec4 uColor;
in vec4 vTex;
out vec4 outColor;
void main() { outColor = uColor * texture(uTex, vTex.xy); }
)GLSL";

// radial falloff standing in for the engine's Texture_LightMask gel
static const char* fsFalloff = R"GLSL(#version 300 es
precision highp float;
in vec4 vTex;
out vec4 outColor;
void main()
{
	float d = length((vTex.xy - 0.5) * 2.0);
	float v = clamp(1.0 - d, 0.0, 1.0);
	v = v * v * (3.0 - 2.0 * v);              // smoothstep-shaped falloff
	outColor = vec4(v, v, v, 1.0);
}
)GLSL";

////////////////////////////////////////////////////////////////////////////////////////
// deferredLightShader.psh ported line-for-line to GLSL ES
// HLSL mul(v, M) row-vector == GLSL (M * v) when the row-major bytes are uploaded
// with transpose=false (see phase 1.5 plan). shaderTextures[0..2] -> 3 samplers.
////////////////////////////////////////////////////////////////////////////////////////

static const char* fsDeferredLight = R"GLSL(#version 300 es
precision highp float;

uniform float lightHeight;                    // light z position used for normal mapping
uniform vec4 lightColor;                      // diffuse light color
uniform mat4 normalMatrix;                    // matrix to map normal map coords
uniform mat4 lightMatrix;
uniform sampler2D shaderTexture0;             // normal map
uniform sampler2D shaderTexture1;             // specular map
uniform sampler2D shaderTexture2;             // shadow map

uniform float specularPower;
uniform float specularHeight;
uniform float specularAmount;

in vec4 vTex;
out vec4 outColor;

void main()
{
	// get the light direction
	vec4 lightDirection = vTex;
	lightDirection.w = 1.0;
	lightDirection = lightMatrix * lightDirection;
	lightDirection.z = lightHeight;
	lightDirection.w = 0.0;
	lightDirection = normalize(lightDirection);

	// get the normal map coordinates
	vec4 normalCoords = vTex;
	normalCoords.z = 1.0;
	normalCoords.w = 1.0;
	normalCoords = normalMatrix * normalCoords;

	// get the normal
	vec3 normal = texture(shaderTexture0, normalCoords.xy).xyz;
	normal = 2.0 * normal - 1.0;
	normal = normalize(normal);

	// calculate lighting using a dot product
	float brightness = clamp(dot(normal, lightDirection.xyz), 0.0, 1.0);

	// calculate specular contribution
	vec3 viewDirection = vec3(0.5, 0.5, 0.0) - normalCoords.xyz;
	viewDirection.z = specularHeight;
	viewDirection = normalize(viewDirection);
	vec4 specularColor = texture(shaderTexture1, normalCoords.xy);
	vec3 reflection = normalize(2.0 * brightness * normal - lightDirection.xyz);
	vec4 specular = specularColor * pow(clamp(dot(reflection, viewDirection), 0.0, 1.0), specularPower);

	// get the shadow map color
	vec4 shadowMapColor = texture(shaderTexture2, vTex.xy);

	// modulate with shadow map and light color
	outColor = (brightness + specularAmount * specular) * shadowMapColor * lightColor;
}
)GLSL";

////////////////////////////////////////////////////////////////////////////////////////
// blurShader.psh ported line-for-line: 4 sample gaussian-ish cross blur
////////////////////////////////////////////////////////////////////////////////////////

static const char* fsBlur = R"GLSL(#version 300 es
precision highp float;

uniform float blurSize;
uniform float blurBrightness;
uniform sampler2D shaderTexture0;

in vec4 vTex;
out vec4 outColor;

void main()
{
	float colorScale = blurBrightness * 0.25;
	outColor = vec4(0.0);
	outColor += colorScale * texture(shaderTexture0, vTex.xy + vec2(0.0, blurSize));
	outColor += colorScale * texture(shaderTexture0, vTex.xy + vec2(0.0, -blurSize));
	outColor += colorScale * texture(shaderTexture0, vTex.xy + vec2(blurSize, 0.0));
	outColor += colorScale * texture(shaderTexture0, vTex.xy + vec2(-blurSize, 0.0));
}
)GLSL";

// final composite: diffuse modulated by accumulated lighting
static const char* fsFinal = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D uTex;                       // diffuse
uniform sampler2D uLight;                     // accumulated light buffer
in vec4 vTex;
out vec4 outColor;
void main()
{
	vec4 diffuse = texture(uTex, vTex.xy);
	// uLight is FBO-rendered: GL render targets are bottom-up (v=0 = screen bottom),
	// opposite of uploaded textures - sample with v flipped. D3D9 has no such split.
	vec4 light = texture(uLight, vec2(vTex.x, 1.0 - vTex.y));
	outColor = vec4(diffuse.rgb * light.rgb, 1.0);
}
)GLSL";

////////////////////////////////////////////////////////////////////////////////////////
// procedural test scene - rounded bricks (diffuse + screen-space normals + specular)
////////////////////////////////////////////////////////////////////////////////////////

static float BrickHeight(int x, int y)
{
	const int brickW = 100, brickH = 50, mortar = 10;
	const int row = y / brickH;
	const int xo = (row & 1) ? brickW / 2 : 0;   // offset alternate rows
	const int bx = ((x + xo) % brickW);
	const int by = (y % brickH);
	// distance from brick interior edge, 0 in mortar
	const float dx = (bx < brickW/2 ? bx : brickW - bx) - mortar * 0.5f;
	const float dy = (by < brickH/2 ? by : brickH - by) - mortar * 0.5f;
	float d = dx < dy ? dx : dy;
	if (d < 0) d = 0;
	const float edge = 6.0f;                     // rounded edge width in pixels
	float h = d / edge;
	if (h > 1) h = 1;
	return h * h * (3 - 2 * h);                  // smooth rounded profile
}

static unsigned Hash2(int x, int y) { unsigned h = (unsigned)(x * 374761393 + y * 668265263); h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }

// occluder boxes baked into the scene as raised stone blocks so they are visible,
// lit, and visually attached to the shadows they cast.
// forward declaration - the box list lives with the spike state below
static float OccluderHeightAt(int x, int y);

// combined scene height: bricks with the occluder blocks standing on top
static float SceneHeight(int x, int y)
{
	const float boxH = OccluderHeightAt(x, y);
	const float brick = BrickHeight(x, y);
	return boxH > 0 ? 1.0f + boxH : brick;       // blocks sit above brick level
}

static void BuildSceneTextures(GLuint& diffuse, GLuint& normals, GLuint& specular)
{
	const int w = screenW, h = screenH;
	unsigned char* dif = (unsigned char*)malloc(w * h * 4);
	unsigned char* nrm = (unsigned char*)malloc(w * h * 4);
	unsigned char* spc = (unsigned char*)malloc(w * h * 4);

	for (int y = 0; y < h; ++y)
	for (int x = 0; x < w; ++x)
	{
		const int i = (y * w + x) * 4;
		const float hc = BrickHeight(x, y);
		const float boxH = OccluderHeightAt(x, y);

		if (boxH > 0)
		{
			// occluder block: unmistakable slate-blue, brighter toward its flat top
			const float v = 0.30f + 0.45f * boxH;
			float blue = v * 1.15f;
			if (blue > 1) blue = 1;
			dif[i+0] = (unsigned char)(255 * v * 0.55f);
			dif[i+1] = (unsigned char)(255 * v * 0.75f);
			dif[i+2] = (unsigned char)(255 * blue);
		}
		else
		{
			// diffuse: per-brick tinted red-brown bricks, gray mortar
			const int row = y / 50, col = (x + ((row & 1) ? 50 : 0)) / 100;
			const unsigned hash = Hash2(col, row);
			const float tint = 0.85f + 0.15f * ((hash & 255) / 255.0f);
			const float mr = 0.45f, mg = 0.42f, mb = 0.40f;             // mortar
			const float br = 0.62f * tint, bg = 0.28f * tint, bb = 0.22f * tint;	// brick
			dif[i+0] = (unsigned char)(255 * (mr + (br - mr) * hc));
			dif[i+1] = (unsigned char)(255 * (mg + (bg - mg) * hc));
			dif[i+2] = (unsigned char)(255 * (mb + (bb - mb) * hc));
		}
		dif[i+3] = 255;

		// screen-space normal from the combined height field, y-up world convention
		const float scale = 2.5f;
		const float dhdx = (SceneHeight(x+1, y) - SceneHeight(x-1, y)) * scale;
		const float dhdy = (SceneHeight(x, y+1) - SceneHeight(x, y-1)) * scale;
		float nx = -dhdx, ny = dhdy, nz = 1;     // ny positive: screen y down -> world y up
		const float len = sqrtf(nx*nx + ny*ny + nz*nz);
		nrm[i+0] = (unsigned char)(255 * (0.5f + 0.5f * nx / len));
		nrm[i+1] = (unsigned char)(255 * (0.5f + 0.5f * ny / len));
		nrm[i+2] = (unsigned char)(255 * (0.5f + 0.5f * nz / len));
		nrm[i+3] = 255;

		// specular: brick faces shiny-ish, mortar dull, blocks shiniest, plus speckle
		const float speckle = 0.8f + 0.2f * ((Hash2(x, y) & 255) / 255.0f);
		const float sv = (boxH > 0 ? 0.8f * boxH : 0.15f + 0.55f * hc) * speckle;
		spc[i+0] = spc[i+1] = spc[i+2] = (unsigned char)(255 * sv);
		spc[i+3] = 255;
	}

	// diagnostic: prove the bake happened and what a block texel actually holds
	int boxPixels = 0;
	for (int y = 0; y < h; ++y)
	for (int x = 0; x < w; ++x)
		if (OccluderHeightAt(x, y) > 0) ++boxPixels;
	const int cx = (int)(0.36f * w), cy = (int)((1.0f - 0.56f) * h);
	const int ci = (cy * w + cx) * 4;
	printf("bake diag: %d occluder pixels; block1 center texel rgb = %d %d %d\n",
		boxPixels, dif[ci+0], dif[ci+1], dif[ci+2]);

	diffuse = MakeTexture(w, h, dif);
	normals = MakeTexture(w, h, nrm);
	specular = MakeTexture(w, h, spc);
	free(dif); free(nrm); free(spc);
}

////////////////////////////////////////////////////////////////////////////////////////
// spike state
////////////////////////////////////////////////////////////////////////////////////////

struct SpikeLight
{
	FrankVec3Base color;
	float radius;
	float height;		// world units, lightHeight uniform = height/radius
	FrankVec3Base pos;	// z unused
	float angle;
};

static GLuint progTex, progSolid, progFalloff, progLight, progBlur, progFinal;
static GLuint quadVBO, quadVAO, polyVBO, polyVAO;
static GLuint texDiffuse, texNormals, texSpecular;
static RenderTarget rtLight;                     // accumulated lighting, screen sized
static RenderTarget rtShadowA, rtShadowB;        // per-light shadow map ping-pong
static SpikeLight lights[2];
static int debugView = 5;
static double startTime = 0;

// occluder boxes in world space [0,1]: center + half size
static const float occluders[][4] = { {0.36f, 0.56f, 0.09f, 0.04f}, {0.64f, 0.42f, 0.04f, 0.10f}, {0.5f, 0.74f, 0.06f, 0.06f} };
static const int numOccluders = 3;

// height of the occluder blocks at a scene-texture pixel (0 outside, 0..1 rounded plateau).
// texture row 0 displays at the top of the screen, so world y = 1 - row/screenH.
static float OccluderHeightAt(int x, int y)
{
	const float wx = (float)x / screenW;
	const float wy = 1.0f - (float)y / screenH;
	float best = 0;
	for (int o = 0; o < numOccluders; ++o)
	{
		const float dx = occluders[o][2] - fabsf(wx - occluders[o][0]);
		const float dy = occluders[o][3] - fabsf(wy - occluders[o][1]);
		if (dx <= 0 || dy <= 0)
			continue;
		const float dPix = (dx * screenW < dy * screenH) ? dx * screenW : dy * screenH;
		float v = dPix / 6.0f;                   // rounded edge, ~6px
		if (v > 1) v = 1;
		v = v * v * (3 - 2 * v);
		if (v > best) best = v;
	}
	return best;
}

static void DrawUnitQuad(GLuint prog, float px, float py, float sx, float sy, float angle)
{
	glUseProgram(prog);
	glUniform2f(glGetUniformLocation(prog, "uPos"), px, py);
	glUniform2f(glGetUniformLocation(prog, "uSize"), sx, sy);
	glUniform1f(glGetUniformLocation(prog, "uAngle"), angle);
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void DrawClipPoly(const float* pts, int count, float r, float g, float b, float a)
{
	glUseProgram(progSolid);
	glUniform4f(glGetUniformLocation(progSolid, "uColor"), r, g, b, a);
	glBindVertexArray(polyVAO);
	glBindBuffer(GL_ARRAY_BUFFER, polyVBO);
	glBufferData(GL_ARRAY_BUFFER, count * 2 * sizeof(float), pts, GL_STREAM_DRAW);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, count);
}

// world position -> this light's quad texture space (0..1), inverse of the quad mapping.
// vTex is y-down (vsQuad flips y to match the engine's D3D quad convention), so world
// +y should map to smaller v — but one mirror in the stack is still unaccounted for,
// so the sign is runtime-toggleable (F key) until visually calibrated.
static float shadowFlipY = -1.0f;

static void WorldToLightUV(const SpikeLight& light, float wx, float wy, float& u, float& v)
{
	const float dx = wx - light.pos.x, dy = wy - light.pos.y;
	const float c = cosf(-light.angle), s = sinf(-light.angle);
	const float lx = dx*c - dy*s, ly = dx*s + dy*c;
	u = lx / (2 * light.radius) + 0.5f;
	v = 0.5f + shadowFlipY * ly / (2 * light.radius);
}

// build this light's shadow map: radial falloff gel, minus extruded occluder shadows, blurred
static void RenderShadowMap(const SpikeLight& light)
{
	glViewport(0, 0, shadowMapSize, shadowMapSize);
	glDisable(GL_BLEND);

	// falloff gel
	glBindFramebuffer(GL_FRAMEBUFFER, rtShadowA.fbo);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	DrawUnitQuad(progFalloff, 0.5f, 0.5f, 0.5f, 0.5f, 0);

	// occluder shadows: extrude each box corner away from the light center
	for (int o = 0; o < numOccluders; ++o)
	{
		float cx, cy;
		WorldToLightUV(light, occluders[o][0], occluders[o][1], cx, cy);
		const float hx = occluders[o][2] / (2 * light.radius);
		const float hy = occluders[o][3] / (2 * light.radius);
		// corners in light uv space (box is world-axis aligned; light rotation applied)
		const float c = cosf(-light.angle), s = sinf(-light.angle);
		float corner[4][2];
		const float offs[4][2] = { {-1,-1}, {1,-1}, {1,1}, {-1,1} };
		for (int k = 0; k < 4; ++k)
		{
			const float ox = offs[k][0] * occluders[o][2], oy = offs[k][1] * occluders[o][3];
			corner[k][0] = cx + (ox*c - oy*s) / (2 * light.radius);
			corner[k][1] = cy + shadowFlipY * (ox*s + oy*c) / (2 * light.radius);
		}
		// shadow quad per edge: edge verts + verts extruded from light center (0.5, 0.5)
		for (int e = 0; e < 4; ++e)
		{
			const float* p0 = corner[e];
			const float* p1 = corner[(e + 1) & 3];
			const float extrude = 4.0f;
			float pts[8];
			// clip space: uv*2-1
			pts[0] = p0[0]*2-1;										pts[1] = p0[1]*2-1;
			pts[2] = p1[0]*2-1;										pts[3] = p1[1]*2-1;
			pts[4] = (p0[0] + (p0[0]-0.5f)*extrude)*2-1;			pts[5] = (p0[1] + (p0[1]-0.5f)*extrude)*2-1;
			pts[6] = (p1[0] + (p1[0]-0.5f)*extrude)*2-1;			pts[7] = (p1[1] + (p1[1]-0.5f)*extrude)*2-1;
			DrawClipPoly(pts, 4, 0, 0, 0, 1);
		}
	}

	// blur ping-pong with the ported blurShader, 2 passes like the engine's shadow soften
	for (int pass = 0; pass < 2; ++pass)
	{
		const RenderTarget& src = pass ? rtShadowB : rtShadowA;
		const RenderTarget& dst = pass ? rtShadowA : rtShadowB;
		glBindFramebuffer(GL_FRAMEBUFFER, dst.fbo);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(progBlur);
		glUniform1f(glGetUniformLocation(progBlur, "blurSize"), 2.0f / shadowMapSize);
		glUniform1f(glGetUniformLocation(progBlur, "blurBrightness"), 1.0f);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, src.tex);
		glUniform1i(glGetUniformLocation(progBlur, "shaderTexture0"), 0);
		DrawUnitQuad(progBlur, 0.5f, 0.5f, 0.5f, 0.5f, 0);
	}
	// result ends in rtShadowA
}

// engine-exact matrix construction, deferredRender.cpp:361-391, using phase 1 portable math
static void BuildLightMatrices(const SpikeLight& light, FrankMat44Base& lightMatrix, FrankMat44Base& normalMatrix)
{
	const float cameraSizeX = 0.5f, cameraSizeY = 0.5f;	// world [0,1] -> camera half size 0.5
	const float camPosX = 0.5f, camPosY = 0.5f;			// camera centered

	const float sizeX = light.radius / cameraSizeX;
	const float sizeY = light.radius / cameraSizeY;
	float offsetX = 0.5f * (light.pos.x - camPosX) / cameraSizeX;
	float offsetY = 0.5f * (light.pos.y - camPosY) / cameraSizeY;
	offsetX = 0.5f * (1 - sizeX) + offsetX;
	offsetY = 0.5f * (1 - sizeY) - offsetY;				// engine: offset * Vector2(1,-1)
	offsetX -= 0.5f * sizeX / screenW;					// half pixel offset vs buffer size
	offsetY -= 0.5f * sizeY / screenH;

	FrankMat44Base m4;
	FrankMatrixIdentity(m4);
	m4._11 = sizeX;	m4._22 = sizeY;
	m4._31 = offsetX; m4._32 = offsetY; m4._33 = 1;

	FrankMat44Base m1, m2, m3, m5;
	FrankMatrixTranslation(m1, -0.5f, -0.5f, 0);
	FrankMatrixRotationZ(m2, -light.angle);
	FrankMatrixTranslation(m3, 0.5f, 0.5f, 0);
	FrankMatrixScaling(m5, -1, -1, 1);

	lightMatrix = m1 * m5 * m2;
	normalMatrix = m1 * m2 * m3 * m4;
}

static void RenderLight(const SpikeLight& light)
{
	FrankMat44Base lightMatrix, normalMatrix;
	BuildLightMatrices(light, lightMatrix, normalMatrix);

	glUseProgram(progLight);
	glUniformMatrix4fv(glGetUniformLocation(progLight, "lightMatrix"), 1, GL_FALSE, &lightMatrix._11);
	glUniformMatrix4fv(glGetUniformLocation(progLight, "normalMatrix"), 1, GL_FALSE, &normalMatrix._11);
	glUniform4f(glGetUniformLocation(progLight, "lightColor"), light.color.x, light.color.y, light.color.z, 1.0f);
	glUniform1f(glGetUniformLocation(progLight, "lightHeight"), light.height / light.radius);
	glUniform1f(glGetUniformLocation(progLight, "specularPower"), specularPower);
	glUniform1f(glGetUniformLocation(progLight, "specularHeight"), specularHeight);
	glUniform1f(glGetUniformLocation(progLight, "specularAmount"), specularAmount);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texNormals);
	glUniform1i(glGetUniformLocation(progLight, "shaderTexture0"), 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, texSpecular);
	glUniform1i(glGetUniformLocation(progLight, "shaderTexture1"), 1);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, rtShadowA.tex);
	glUniform1i(glGetUniformLocation(progLight, "shaderTexture2"), 2);

	DrawUnitQuad(progLight, light.pos.x, light.pos.y, light.radius, light.radius, light.angle);
	glActiveTexture(GL_TEXTURE0);
}

static void ShowBuffer(GLuint tex)
{
	glUseProgram(progTex);
	glUniform4f(glGetUniformLocation(progTex, "uColor"), 1, 1, 1, 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);
	glUniform1i(glGetUniformLocation(progTex, "uTex"), 0);
	DrawUnitQuad(progTex, 0.5f, 0.5f, 0.5f, 0.5f, 0);
}

static void MainLoop()
{
	const double t = emscripten_get_now() / 1000.0 - startTime;

	// animate: light 0 orbits, light 1 sweeps a figure 8; both slowly rotate their quads
	lights[0].pos = FrankVec3Base(0.5f + 0.28f * (float)cos(t * 0.6), 0.5f + 0.22f * (float)sin(t * 0.6), 0);
	lights[0].angle = (float)t * 0.3f;
	lights[1].pos = FrankVec3Base(0.5f + 0.3f * (float)sin(t * 0.4), 0.5f + 0.25f * (float)sin(t * 0.8), 0);
	lights[1].angle = -(float)t * 0.2f;

	// pass 1: accumulate lights additively into the light buffer
	glBindFramebuffer(GL_FRAMEBUFFER, rtLight.fbo);
	glViewport(0, 0, screenW, screenH);
	glClearColor(0.13f, 0.13f, 0.16f, 1);        // ambient
	glClear(GL_COLOR_BUFFER_BIT);
	for (int i = 0; i < 2; ++i)
	{
		RenderShadowMap(lights[i]);              // rebuilds rtShadowA for this light
		glBindFramebuffer(GL_FRAMEBUFFER, rtLight.fbo);
		glViewport(0, 0, screenW, screenH);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);             // additive, like the engine's light pass
		RenderLight(lights[i]);
		glDisable(GL_BLEND);
	}

	// pass 2: composite to screen (or show a debug buffer)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenW, screenH);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	switch (debugView)
	{
	case 1: ShowBuffer(texDiffuse); break;
	case 2: ShowBuffer(texNormals); break;
	case 3: ShowBuffer(texSpecular); break;
	case 4: ShowBuffer(rtShadowA.tex); break;    // last light's shadow map
	default:
	{
		glUseProgram(progFinal);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texDiffuse);
		glUniform1i(glGetUniformLocation(progFinal, "uTex"), 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, rtLight.tex);
		glUniform1i(glGetUniformLocation(progFinal, "uLight"), 1);
		DrawUnitQuad(progFinal, 0.5f, 0.5f, 0.5f, 0.5f, 0);
		glActiveTexture(GL_TEXTURE0);
		break;
	}
	}
}

static EM_BOOL OnKey(int type, const EmscriptenKeyboardEvent* e, void* userData)
{
	if (e->key[0] >= '1' && e->key[0] <= '5' && e->key[1] == 0)
	{
		debugView = e->key[0] - '0';
		printf("view %d (%s)\n", debugView, debugView == 1 ? "diffuse" : debugView == 2 ? "normals" : debugView == 3 ? "specular" : debugView == 4 ? "shadow map" : "final");
		return EM_TRUE;
	}
	if ((e->key[0] == 'f' || e->key[0] == 'F') && e->key[1] == 0)
	{
		shadowFlipY = -shadowFlipY;
		printf("shadow flip y = %+.0f\n", shadowFlipY);
		return EM_TRUE;
	}
	return EM_FALSE;
}

int main()
{
	EmscriptenWebGLContextAttributes attrs;
	emscripten_webgl_init_context_attributes(&attrs);
	attrs.majorVersion = 2;
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attrs);
	if (ctx <= 0)
	{
		printf("FAILED: WebGL2 context\n");
		return 1;
	}
	emscripten_webgl_make_context_current(ctx);

	progTex = CompileProgram(vsQuad, fsTex, "tex");
	progSolid = CompileProgram(vsClip, fsSolid, "solid");
	progFalloff = CompileProgram(vsQuad, fsFalloff, "falloff");
	progLight = CompileProgram(vsQuad, fsDeferredLight, "deferredLight");
	progBlur = CompileProgram(vsQuad, fsBlur, "blur");
	progFinal = CompileProgram(vsQuad, fsFinal, "final");

	const float quad[8] = { 0,0, 1,0, 0,1, 1,1 };
	glGenVertexArrays(1, &quadVAO);
	glBindVertexArray(quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glGenVertexArrays(1, &polyVAO);
	glBindVertexArray(polyVAO);
	glGenBuffers(1, &polyVBO);
	glBindBuffer(GL_ARRAY_BUFFER, polyVBO);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

	BuildSceneTextures(texDiffuse, texNormals, texSpecular);
	rtLight = MakeRenderTarget(screenW, screenH);
	rtShadowA = MakeRenderTarget(shadowMapSize, shadowMapSize);
	rtShadowB = MakeRenderTarget(shadowMapSize, shadowMapSize);

	lights[0].color = FrankVec3Base(1.0f, 0.9f, 0.7f);   // warm white
	lights[0].radius = 0.38f;
	lights[0].height = 0.11f;
	lights[1].color = FrankVec3Base(0.4f, 0.7f, 1.0f);   // cool blue
	lights[1].radius = 0.45f;
	lights[1].height = 0.14f;

	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_TRUE, OnKey);

	printf("lighting spike v7: keys 1-5 views, F flips shadow projection (default should be correct)\n");
	startTime = emscripten_get_now() / 1000.0;
	emscripten_set_main_loop(MainLoop, 0, 1);
	return 0;
}
