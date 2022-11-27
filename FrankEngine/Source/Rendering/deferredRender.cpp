////////////////////////////////////////////////////////////////////////////////////////
/*
	Deferred Rendering System
	Copyright 2013 Frank Force - http://www.frankforce.com
	
	- multi pass building of final light map
	- point lights
	- spot lights
	- dynamic soft shadows
	- additive light colors 
	- directional lights
	- light gels
	- colored transparent objects and terrain
	- emissive objects and terrain

*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../rendering/frankRender.h"
#include "deferredRender.h"

LPDIRECT3DTEXTURE9 DeferredRender::textureSwapArray[textureSwapArraySize] = {NULL};
LPDIRECT3DTEXTURE9 DeferredRender::texture = NULL;
LPDIRECT3DTEXTURE9 DeferredRender::textureNormalMap = NULL;
LPDIRECT3DTEXTURE9 DeferredRender::textureSpecularMap = NULL;
LPDIRECT3DTEXTURE9 DeferredRender::textureShadowMap = NULL;
LPDIRECT3DTEXTURE9 DeferredRender::textureShadowMapDirectional = NULL;
LPDIRECT3DTEXTURE9 DeferredRender::textureVision = NULL;
LPDIRECT3DTEXTURE9 DeferredRender::textureFinal = NULL;
LPDIRECT3DTEXTURE9 DeferredRender::textureEmissive = NULL;
LPDIRECT3DPIXELSHADER9 DeferredRender::deferredLightShader = NULL;
LPD3DXCONSTANTTABLE DeferredRender::shadowLightConstantTable = NULL;
LPDIRECT3DPIXELSHADER9 DeferredRender::visionShader = NULL;
LPD3DXCONSTANTTABLE DeferredRender::visionConstantTable = NULL;
LPDIRECT3DPIXELSHADER9 DeferredRender::directionalLightShader = NULL;
LPD3DXCONSTANTTABLE DeferredRender::directionalLightConstantTable = NULL;
LPDIRECT3DPIXELSHADER9 DeferredRender::blurShader = NULL;
LPD3DXCONSTANTTABLE DeferredRender::blurConstantTable = NULL;
FrankRender::RenderPrimitive DeferredRender::primitiveLightMask;

TextureID DeferredRender::overbrightTexture = Texture_Circle;
DeferredRender::RenderPass DeferredRender::renderPass = DeferredRender::RenderPass_diffuse;
int DeferredRender::dynamicLightCount = 0;
int DeferredRender::simpleLightCount = 0;
static const Vector2 halfPixel(-1, 1);
bool DeferredRender::SpecularRenderBlock::active = false;
bool DeferredRender::BackgroundRenderBlock::active = false;
bool DeferredRender::TransparentRenderBlock::active = false;
bool DeferredRender::EmissiveRenderBlock::active = false;
bool DeferredRender::NoTextureRenderBlock::active = false;
float DeferredRender::DiffuseNormalAlphaRenderBlock::alphaScale = 1;
bool DeferredRender::DiffuseNormalAlphaRenderBlock::active = false;

////////////////////////////////////////////////////////////////////////////////////////

// show light debug overlay
bool DeferredRender::lightDebug = false;
ConsoleCommand(DeferredRender::lightDebug, lightDebug);

// show overlay of final texture for debugging
int DeferredRender::showTexture = 0; 
ConsoleFunction(lightShowTexture)
{
	swscanf_s(text.c_str(), L"%d", &DeferredRender::showTexture);
	GetDebugConsole().AddFormatted(L"Show Texture: %s", DeferredRender::GetShowTextureModeString());
}

// save shadows to disk for debugging
ConsoleCommandSimple(bool, shadowSaveTextures, false);

// prevent flicker between stationary lights and objects by rounding to nearest pixel
ConsoleCommandSimple(bool, lightPixelRoundingEnable, true);

// enable lighting
bool DeferredRender::lightEnable = true;
ConsoleCommand(DeferredRender::lightEnable, lightEnable);

// if any more dynamic lights then this are needed per frame they will be skipped
int DeferredRender::maxDynamicLights = 24;
ConsoleCommand(DeferredRender::maxDynamicLights, maxDynamicLights);

// if any more simple lights then this are needed per frame they will be skipped
int DeferredRender::maxSimpleLights = 128;
ConsoleCommand(DeferredRender::maxSimpleLights, maxSimpleLights);

// globaly scales alpha of all light colors
float DeferredRender::alphaScale = 1.0f;
ConsoleCommand(DeferredRender::alphaScale, lightAlphaScale);

// color of ambient light
Color DeferredRender::ambientLightColor = Color::Black();
ConsoleCommand(DeferredRender::ambientLightColor, ambientLightColor);

// texture size used by lights
float DeferredRender::textureSize = 256;
ConsoleCommand(DeferredRender::textureSize, lightTextureSize);

// texture size used by lights
float DeferredRender::shadowMapTextureSize = 1024;
ConsoleCommand(DeferredRender::shadowMapTextureSize, shadowMapTextureSize);

// how much bigger to make the shadow map
float DeferredRender::shadowMapScale = 3.0f;
ConsoleCommand(DeferredRender::shadowMapScale, shadowMapScale);

// toggle normal mapping
bool DeferredRender::normalMappingEnable = true;
ConsoleCommand(DeferredRender::normalMappingEnable, normalMappingEnable);

// how much to scale radius before camera check
float DeferredRender::lightCullScale = 1;
ConsoleCommand(DeferredRender::lightCullScale, lightCullScale);

// radius for point lights
float DeferredRender::defaultLightRadius = 10;
ConsoleCommand(DeferredRender::defaultLightRadius, defaultLightRadius);

////////////////////////////////////////////////////////////////////////////////////////

// final pass that creates a mask for the player's vision
bool DeferredRender::visionEnable = false;
ConsoleCommand(DeferredRender::visionEnable, visionEnable);

float DeferredRender::visionRadius = 15;
ConsoleCommand(DeferredRender::visionRadius, visionRadius);

float DeferredRender::visionOverbrightRadius = 0.5f;
ConsoleCommand(DeferredRender::visionOverbrightRadius, visionOverbrightRadius);

float DeferredRender::visionSoftening = 0.0f;
ConsoleCommand(DeferredRender::visionSoftening, visionSoftening);

float DeferredRender::visionCastScale = 1.5f;
ConsoleCommand(DeferredRender::visionCastScale, visionCastScale);

int DeferredRender::visionPassCount = 18;
ConsoleCommand(DeferredRender::visionPassCount, visionPassCount);

float DeferredRender::visionTextureSize = 256;
ConsoleCommand(DeferredRender::visionTextureSize, visionTextureSize);

int DeferredRender::visionPreBlurPassCount = 2;
ConsoleCommand(DeferredRender::visionPreBlurPassCount, visionPreBlurPassCount);

int DeferredRender::visionPostBlurPassCount = 7;
ConsoleCommand(DeferredRender::visionPostBlurPassCount, visionPostBlurPassCount);

////////////////////////////////////////////////////////////////////////////////////////

// enable lights casting shadows
bool DeferredRender::shadowEnable = true;
ConsoleCommand(DeferredRender::shadowEnable, shadowEnable);

// default shadow color to use for objects
Color DeferredRender::defaultShadowColor = Color::Black(1);
ConsoleCommand(DeferredRender::defaultShadowColor, defaultShadowColor);

// how much do shadows bleed through edges
float DeferredRender::shadowSoftening = 0.5f;
ConsoleCommand(DeferredRender::shadowSoftening, shadowSoftening);

// how much the shadow generation is scaled each pass, setting it lower can smooth out some artifacts
float DeferredRender::shadowCastScale = 1.8f;
ConsoleCommand(DeferredRender::shadowCastScale, shadowCastScale);

// how many passes for the shadow casting lights
int DeferredRender::shadowPassCount = 11;
ConsoleCommand(DeferredRender::shadowPassCount, shadowPassCount);

// how big to start stretching shadow by in pixels, 1 for best results 
float DeferredRender::shadowPassStartSize = 2;
ConsoleCommand(DeferredRender::shadowPassStartSize, shadowPassStartSize);

// height used for normal mapping
float DeferredRender::shadowLightHeightDefault = 4.0f;
ConsoleCommand(DeferredRender::shadowLightHeightDefault, shadowLightHeightDefault);

////////////////////////////////////////////////////////////////////////////////////////

// enable emsisive lighting pass
bool DeferredRender::emissiveLightEnable = true;
ConsoleCommand(DeferredRender::emissiveLightEnable, emissiveLightEnable);

// how many emissive blurs to do
int DeferredRender::emissiveBlurPassCount = 4;
ConsoleCommand(DeferredRender::emissiveBlurPassCount, emissiveBlurPassCount);

// how bright is the emissive blur, can be brighter then 1 for bleeding
float DeferredRender::emissiveBlurAlpha = 1.2f;
ConsoleCommand(DeferredRender::emissiveBlurAlpha, emissiveBlurAlpha);

// how much to blur the emssive pass
float DeferredRender::emissiveBlurSize = 1;
ConsoleCommand(DeferredRender::emissiveBlurSize, emissiveBlurSize);

// background color for emissive pass
Color DeferredRender::emissiveBackgroundColor = Color::White();
ConsoleCommand(DeferredRender::emissiveBackgroundColor, emissiveBackgroundColor);

// size of emissive texture
float DeferredRender::emissiveTextureSize = 1024;
ConsoleCommand(DeferredRender::emissiveTextureSize, emissiveTextureSize);

// alpha for normal emissive blend
float DeferredRender::emissiveAlpha = 1;
ConsoleCommand(DeferredRender::emissiveAlpha, emissiveAlpha);

// alpha for normal emissive bloom blend
float DeferredRender::emissiveBloomAlpha = 1;
ConsoleCommand(DeferredRender::emissiveBloomAlpha, emissiveBloomAlpha);

////////////////////////////////////////////////////////////////////////////////////////

// enables directional lighting pass
bool DeferredRender::directionalLightEnable = true;
ConsoleCommand(DeferredRender::directionalLightEnable, directionalLightEnable);

// color of directional light
Color DeferredRender::directionalLightColor = Color::White(0.8f);
ConsoleCommand(DeferredRender::directionalLightColor, directionalLightColor);

// direction of directional light
Vector2 DeferredRender::directionalLightDirection(-1, -1);
ConsoleCommand(DeferredRender::directionalLightDirection, directionalLightDirection);

// how much to soften directional shadows
float DeferredRender::directionalLightSoftening = 0.1f;
ConsoleCommand(DeferredRender::directionalLightSoftening, directionalLightSoftening);

// how much the directional generation is moved each pass, the lower the slower
float DeferredRender::directionalLightCastOffset = 1.3f;
ConsoleCommand(DeferredRender::directionalLightCastOffset, directionalLightCastOffset);

// how much the directional generation is scaled each pass, 1 is no scale
float DeferredRender::directionalLightCastScale = 1.0f;
ConsoleCommand(DeferredRender::directionalLightCastScale, directionalLightCastScale);

// height used for normal mapping directional
float DeferredRender::directionalLightHeight = 0.5f;
ConsoleCommand(DeferredRender::directionalLightHeight, directionalLightHeight);

// number of passes for directional light
int DeferredRender::directionalLightPassCount = 10;
ConsoleCommand(DeferredRender::directionalLightPassCount, directionalLightPassCount);

// alpha factor for redrawing to prevent light from going through solid areas
int DeferredRender::directionalLightForgroundPassCount = 7;
ConsoleCommand(DeferredRender::directionalLightForgroundPassCount, directionalLightForgroundPassCount);

// how much to erode dark areas in directional pass
float DeferredRender::directionalLightBlurBrightness = 1.3f;
ConsoleCommand(DeferredRender::directionalLightBlurBrightness, directionalLightBlurBrightness);

int DeferredRender::directionalLightBlurPassCount = 4;
ConsoleCommand(DeferredRender::directionalLightBlurPassCount, directionalLightBlurPassCount);

////////////////////////////////////////////////////////////////////////////////////////

float DeferredRender::specularPower = 12;
ConsoleCommand(DeferredRender::specularPower, specularPower);

float DeferredRender::specularHeight = 2;
ConsoleCommand(DeferredRender::specularHeight, specularHeight);

float DeferredRender::specularAmount = 1;
ConsoleCommand(DeferredRender::specularAmount, specularAmount);

////////////////////////////////////////////////////////////////////////////////////////

// texture size used for final overlay
Vector2 DeferredRender::finalTextureSize(0);
float DeferredRender::finalTextureSizeScale(1);
ConsoleCommand(DeferredRender::finalTextureSizeScale, finalTextureScale);

// how much extra to scale the final texture around the camera to fix artifacts
float DeferredRender::finalTextureCameraScale = 1.2f;
ConsoleCommand(DeferredRender::finalTextureCameraScale, finalLightTextureCameraScale);

// determines format used for lighting texture
bool DeferredRender::use32BitTextures = true;
ConsoleCommand(DeferredRender::use32BitTextures, lightsUse32BitTextures);

////////////////////////////////////////////////////////////////////////////////////////

void DeferredRender::UpdateSimpleLights(list<Light*> simpleLights)
{
	FrankProfilerEntryDefine(L"DeferredRender::UpdateSimpleLights()", Color::White(), 10);

	// simple lights (without shadows or cones) are rendered in the same batch to the final shadow texture
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

	// render to final shadow texture
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	textureFinal->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender();
	
	// set up camera transforms
	Vector2 cameraSize;
	const XForm2 xfFinal = GetFinalTransform(finalTextureSize, &cameraSize);
	g_cameraBase->PrepareForRender(xfFinal, finalTextureCameraScale*GetShadowMapZoom());
		
	// set up additive blending
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

	// batch render for simple lights
	for (Light* light : simpleLights)
		UpdateSimpleLight(*light, xfFinal, cameraSize);
	
	g_render->EndRender();
	SAFE_RELEASE(renderSurface);
	
	// set stuf back the way it was
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
}

void DeferredRender::UpdateSimpleLight(Light& light, const XForm2& xfFinal, const Vector2& cameraSize)
{
	ASSERT(light.IsSimpleLight());
	
	light.wasRendered = false;

	if (!lightEnable || simpleLightCount >= maxSimpleLights || light.radius == 0 || light.color.a == 0)
		return;

	const XForm2 xf = light.GetXFInterpolated();
	
	// check if light is on screen
	if (!g_cameraBase->CameraTest(xf.position, light.radius*lightCullScale))
		return;

	light.wasRendered = true;
	++simpleLightCount;
	
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const Vector2 halfPixelOffset = halfPixel / textureSize;

	// render this light onto the final map
	Color finalColor = light.color;
	finalColor.a *= light.GetFadeAlpha() * alphaScale;
		
	if (!normalMappingEnable || !deferredLightShader)
	{
		g_render->RenderQuad(XForm2(halfPixelOffset * light.radius)*xf, Vector2(light.radius), finalColor, g_render->GetTexture(light.gelTexture? light.gelTexture : Texture_LightMask, false));
	}
	else
	{
		{	
			// create a transform to convert texture space to normal map space
			const Vector2 size = Vector2(light.radius)/cameraSize;
			Vector2 offset = 0.5f*(xf.position - xfFinal.position)/cameraSize;
			offset = 0.5f*(Vector2(1) - size) + offset * Vector2(1,-1);

			// adjust for half pixel offset
			offset -= 0.5f * size / textureSize;

			D3DXMATRIX m4
			(
				size.x,		0,			0,		0,
				0,			size.y,		0,		0,
				offset.x,	offset.y,	1,		0,
				0,			0,			0,		1
			);

			D3DXMATRIX m1; 
			D3DXMatrixTranslation(&m1, -0.5f, -0.5f, 0);
			D3DXMATRIX m2; 
			D3DXMatrixRotationZ(&m2, -xf.angle);
			D3DXMATRIX m3; 
			D3DXMatrixTranslation(&m3, 0.5f, 0.5f, 0);
			D3DXMATRIX m5;
			D3DXMatrixScaling(&m5, -1, -1, 1);

			D3DXMATRIX lightMatrix = m1*m5*m2;
			D3DXMATRIX normalMatrix = m1*m2*m3*m4;
			
			shadowLightConstantTable->SetMatrix(pd3dDevice, "lightMatrix", &lightMatrix);
			shadowLightConstantTable->SetMatrix(pd3dDevice, "normalMatrix", &normalMatrix);
		}

		pd3dDevice->SetTexture(0, textureNormalMap);
		pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		pd3dDevice->SetTexture(1, textureSpecularMap);
		pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 );
		pd3dDevice->SetTexture(2, g_render->GetTexture(light.gelTexture? light.gelTexture : Texture_LightMask, false));
		pd3dDevice->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, 0 );
			
		D3DXVECTOR4 finalColorVector = finalColor;
		shadowLightConstantTable->SetVector(pd3dDevice, "lightColor", &finalColorVector);
		shadowLightConstantTable->SetFloat(pd3dDevice, "lightHeight", light.height/light.radius);

		pd3dDevice->SetPixelShader(deferredLightShader);
		g_render->RenderQuadSimple(XForm2(halfPixelOffset * light.radius)*xf, Vector2(light.radius));

		pd3dDevice->SetPixelShader(NULL);
		pd3dDevice->SetTexture(1, NULL);
		pd3dDevice->SetTexture(2, NULL);
	}
}

void DeferredRender::UpdateDynamicLight(Light& light)
{
	FrankProfilerEntryDefine(L"DeferredRender::UpdateDynamicLight()", Color::White(), 10);
	ASSERT(!light.IsSimpleLight());

	light.wasRendered = false;

	if (!lightEnable || dynamicLightCount >= maxDynamicLights || light.radius == 0 || (light.coneAngle == 0 && light.coneFadeAngle == 0) || light.color.a == 0)
		return;

	const XForm2 xfInterpolated = light.GetXFInterpolated();
	const XForm2 xf(xfInterpolated.position);	// wipe out angle from light calculations
	
	// check if light is on screen
	if (light.coneAngle < 2*PI)
	{
		if (!g_cameraBase->CameraConeTest(xfInterpolated, light.radius*lightCullScale, light.coneAngle + light.coneFadeAngle))
			return;
	}
	else if (!g_cameraBase->CameraTest(xf.position, light.radius*lightCullScale))
		return;

	{
		// hack: make sure the patch is cached
		// fixes issue with shadow casting lights flashing when streamed in because their patch isn't being rendered yet
		TerrainPatch* patch = g_terrain->GetPatch(xfInterpolated.position);
		if (patch && !g_terrainRender.IsPatchCached(*patch))
			return;
	}

	light.wasRendered = true;
	++dynamicLightCount;
	
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const Vector2 halfPixelOffset = halfPixel / textureSize;
	
	// create the light texture with shadows and cone rendering
	RenderLightTexture(light);

	// render to final shadow texture
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	textureFinal->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender();
	{
		// set up camera transforms
		Vector2 cameraSize;
		const XForm2 xfFinal = GetFinalTransform(finalTextureSize, &cameraSize);
		g_cameraBase->PrepareForRender(xfFinal, finalTextureCameraScale*GetShadowMapZoom());
		
		// set up additive blending
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		
		// render this light onto the final map
		Color finalColor = light.color;
		finalColor.a *= light.GetFadeAlpha() * alphaScale;
		
		if (!normalMappingEnable || !deferredLightShader || !shadowLightConstantTable)
		{
			g_render->RenderQuad(XForm2(halfPixelOffset * light.radius)*xf, Vector2(light.radius), finalColor, texture);
		}
		else
		{
			// defrered rendering
			{	
				// create a transform to convert texture space to normal map space
				const Vector2 size = Vector2(light.radius)/cameraSize;
				Vector2 offset = 0.5f*(xf.position - xfFinal.position)/cameraSize;
				offset = 0.5f*(Vector2(1) - size) + offset * Vector2(1,-1);

				// adjust for half pixel offset
				offset -= 0.5f * size / textureSize;

				D3DXMATRIX m4
				(
					size.x,		0,			0,		0,
					0,			size.y,		0,		0,
					offset.x,	offset.y,	1,		0,
					0,			0,			0,		1
				);
				
				D3DXMATRIX m1; 
				D3DXMatrixTranslation(&m1, -0.5f, -0.5f, 0);
				D3DXMATRIX m2; 
				D3DXMatrixRotationZ(&m2, -xf.angle);
				D3DXMATRIX m3; 
				D3DXMatrixTranslation(&m3, 0.5f, 0.5f, 0);
				D3DXMATRIX m5;
				D3DXMatrixScaling(&m5, -1, -1, 1);

				D3DXMATRIX lightMatrix = m1*m5*m2;
				D3DXMATRIX normalMatrix = m1*m2*m3*m4;
			
				shadowLightConstantTable->SetMatrix(pd3dDevice, "lightMatrix", &lightMatrix);
				shadowLightConstantTable->SetMatrix(pd3dDevice, "normalMatrix", &normalMatrix);
			}

			pd3dDevice->SetTexture(0, textureNormalMap);
			pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			pd3dDevice->SetTexture(1, textureSpecularMap);
			pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 );
			pd3dDevice->SetTexture(2, texture);
			pd3dDevice->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, 0 );
			
			D3DXVECTOR4 finalColorVector = finalColor;
			shadowLightConstantTable->SetVector(pd3dDevice, "lightColor", &finalColorVector);
			shadowLightConstantTable->SetFloat(pd3dDevice, "lightHeight", light.height/light.radius);
			
			shadowLightConstantTable->SetFloat(pd3dDevice, "specularPower", specularPower);
			shadowLightConstantTable->SetFloat(pd3dDevice, "specularHeight", specularHeight);
			shadowLightConstantTable->SetFloat(pd3dDevice, "specularAmount", specularAmount);

			pd3dDevice->SetPixelShader(deferredLightShader);
			g_render->RenderQuadSimple(XForm2(halfPixelOffset * light.radius)*xf, Vector2(light.radius));

			pd3dDevice->SetPixelShader(NULL);
			pd3dDevice->SetTexture(1, NULL);
			pd3dDevice->SetTexture(2, NULL);
		}
	}
	g_render->EndRender();
	SAFE_RELEASE(renderSurface);
	
	// set stuf back the way it was
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
	
	// hack: return camera to normal
	// this is so the camera extents are set when checking if next light is on screen
	g_cameraBase->PrepareForRender();
}

void DeferredRender::RenderLightTexture(Light& light)
{
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const Vector2 halfPixelOffset = halfPixel / textureSize;
		
	const XForm2 xfInterpolated = light.GetXFInterpolated();
	const XForm2 xf(xfInterpolated.position);	// wipe out angle from light calculations
	const float& radius = light.radius;
	const float& overbrightRadius = light.overbrightRadius;
	const bool& castShadows = light.castShadows;
	const TextureID& gelTexture = light.gelTexture;

	// set up default render states
	SetFiltering(true);
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

	// prepare render to shadow texture
	LPDIRECT3DTEXTURE9& textureSwap = GetSwapTexture(texture);
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	texture->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender(true);
	{
		// setup camera to render shadow objects in the space of the light
		const XForm2 xfViewInterp = xf.Inverse();
		const Matrix44 matrixView = Matrix44::BuildXFormZ(xfViewInterp.position, xfViewInterp.angle, 10.0f);
		pd3dDevice->SetTransform( D3DTS_VIEW, &matrixView.GetD3DXMatrix() );
		D3DXMATRIX d3dMatrixProjection;
		D3DXMatrixOrthoLH( &d3dMatrixProjection, radius*2, radius*2, 1, 1000);
		pd3dDevice->SetTransform( D3DTS_PROJECTION, &d3dMatrixProjection );
		
		if (castShadows && shadowEnable)
		{
			// render the shadow objects
			Vector2 cameraSize;
			const float shadowMapZoom = GetShadowMapZoom();
			XForm2 xf = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
			cameraSize *= shadowMapScale;
			const Vector2 halfPixelOffset = halfPixel / shadowMapTextureSize;
			Vector2 offset = cameraSize * halfPixelOffset;

			// disable alpha to prevent blending
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
			g_render->RenderQuad(offset + xf.position, cameraSize, Color::White(), textureShadowMap);
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
		}
		
		if (overbrightRadius > 0)
		{
			// use the light mask for the overbright
			pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
			pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
			g_render->RenderQuad(xf, Vector2(overbrightRadius), Color::White(), overbrightTexture, false);
		}

		// setup camera for the shadow casting
		D3DXMATRIX matrixIdentity;
		D3DXMatrixIdentity(&matrixIdentity);
		pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixIdentity);
		pd3dDevice->SetTransform(D3DTS_VIEW, &matrixIdentity);

		if (castShadows && shadowEnable)
		{
			{
				// end the render and copy the texture to the swap texture
				g_render->EndRender();

				// copy texture
				LPDIRECT3DSURFACE9 renderSurfaceSwap;
				textureSwap->GetSurfaceLevel(0, &renderSurfaceSwap);
				pd3dDevice->StretchRect(renderSurface, NULL, renderSurfaceSwap, NULL, D3DTEXF_NONE);
				SAFE_RELEASE(renderSurfaceSwap);

				// start up a render on the new texture
				pd3dDevice->SetRenderTarget(0, renderSurface);
				g_render->BeginRender();
			}
			/*{
				// experiment with shadows for solar system project
				// clear texture to white
				g_render->RenderQuad(halfPixelOffset, Vector2(2), Color::White());
			
				// use subtractive blending
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);

				for (int i=0; i < shadowPassCount; ++i)
				{
					float scale = 1 + shadowCastScale * i;
					float a = 1 - float(i) / float(shadowPassCount);
					g_render->RenderQuad(halfPixelOffset, Vector2(scale), Color::White(a), textureSwap);
				}
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			*/

			// render out many passes of the shadow stretching it each time
			const float inverseTextureSize = 1.0f / textureSize;
			float scale = shadowPassStartSize; // start size
			int passCount = 0;
			while (1)
			{
				// soften the shadow
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

				float brightness = 1;
				if (shadowSoftening <= 1)
				{
					brightness = Percent((float)passCount, 7.0f, 2.0f);
					brightness = brightness*brightness;
					brightness *= shadowSoftening;
				}
				else if (shadowSoftening <= 2)
				{
					brightness = shadowSoftening - 1;
				}

				// soften
				g_render->RenderQuad(halfPixelOffset, Vector2(1), Color::Grey(1, brightness), Texture_LightMask, false);

				// stretch out the shadow
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
				const Vector2 actualScale = Vector2((textureSize + scale) * inverseTextureSize);
				g_render->RenderQuad(halfPixelOffset, actualScale, Color::White(), textureSwap);

				if (++passCount >= shadowPassCount)
					 break;

				// scale up the shadow a little bit each time
				// reducing this towards 1 will increase smoothness but take more passes
				// the largest we can scale without gaps is 2
				scale *= shadowCastScale;

				{
					// ping pong the texture to prep for another pass
					// end the render and copy the texture to the swap texture
					g_render->EndRender();

					// copy texture
					LPDIRECT3DSURFACE9 renderSurfaceSwap;
					textureSwap->GetSurfaceLevel(0, &renderSurfaceSwap);
					pd3dDevice->StretchRect(renderSurface, NULL, renderSurfaceSwap, NULL, D3DTEXF_NONE);
					SAFE_RELEASE(renderSurfaceSwap);

					// start up a render on the new texture
					pd3dDevice->SetRenderTarget(0, renderSurface);
					g_render->BeginRender();
				}
			}
		}
		{
			// render a light mask around the outside so it won't have square corners
			pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
			pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);

			XForm2 xfGel(Vector2(halfPixelOffset), xfInterpolated.angle);

			// use a quad with uvs set between -1 and 2 with black border color to cover the whole texture with the mask
			pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
			pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
			pd3dDevice->SetSamplerState(0, D3DSAMP_BORDERCOLOR, 0xFF000000);

			TextureID ti = gelTexture? gelTexture : Texture_LightMask;
			//g_render->RenderQuad(xfGel, Vector2(1), Color::White(), ti, false);
			g_render->Render(Matrix44::BuildScale(3) * Matrix44(xfGel), Color::White(1), ti, primitiveLightMask); 

			pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
			pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

			// render the cone overlay if it has one
			RenderCone(light, xfInterpolated.angle);
		}

		g_render->EndRender();

		if (shadowSaveTextures)
			D3DXSaveSurfaceToFile( L"shadowMap.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
		SAFE_RELEASE(renderSurface);
	}
}

void DeferredRender::RenderCone(Light& light, float lightAngle)
{
	const float coneAngle = light.coneAngle;
	const float coneFadeAngle = light.coneFadeAngle;
	const float coneFadeColor = light.coneFadeColor;

	if (coneAngle >= 2*PI || coneAngle < 0 || coneFadeAngle >= 2*PI || coneFadeAngle < 0)
		return;
	
	// render a cone for spot lights
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

	const float insideBlurStep = Max(coneFadeAngle/32, 0.01f);
	const float outsideBlurStep = PI/4;
	float angle = coneAngle;
			
	int mode = 0;
			
	g_render->CapLineVerts(Vector2(0));
	while(1)
	{
		Color c1 = Color::Grey(1, coneFadeColor);
		Color c2 = Color::Grey(1, coneFadeColor);

		float angle2 = 0;
		if (mode == 0)
		{
			angle2 = angle + insideBlurStep;
			if (angle2 > coneAngle + coneFadeAngle || coneFadeAngle == 0)
			{
				mode = 1;
				angle2 = coneAngle + coneFadeAngle;
			}
			c1 = Color::Grey(1, PercentLerp(angle, coneAngle + coneFadeAngle, coneAngle, coneFadeColor, 1.0f));
			c2 = Color::Grey(1, PercentLerp(angle2, coneAngle + coneFadeAngle, coneAngle, coneFadeColor, 1.0f));
		}
		else if (mode == 1)
		{
			angle2 = angle + outsideBlurStep;
			if (angle2 > 2*PI - coneAngle - coneFadeAngle)
			{
				mode = 2;
				angle2 = 2*PI - coneAngle - coneFadeAngle;
			}
		}
		else if (mode == 2)
		{
			angle2 = angle + insideBlurStep;
			if (angle2 > 2*PI - coneAngle || coneFadeAngle == 0)
			{
				mode = 3;
				angle2 = 2*PI - coneAngle;
			}
			c1 = Color::Grey(1, PercentLerp(angle, 2*PI - coneAngle - coneFadeAngle, 2*PI - coneAngle, coneFadeColor, 1.0f));
			c2 = Color::Grey(1, PercentLerp(angle2, 2*PI - coneAngle - coneFadeAngle, 2*PI - coneAngle, coneFadeColor, 1.0f));
		}
				
		const Vector2 pos = 2*Vector2::BuildFromAngle(lightAngle + angle);
		const Vector2 pos2 = 2*Vector2::BuildFromAngle(lightAngle + angle2);
				
		g_render->AddPointToTriVerts(Vector2(0), c1);
		g_render->AddPointToTriVerts(pos, c1);
		g_render->AddPointToTriVerts(pos2, c2);

		if (mode == 3)
			break;

		angle = angle2;
	}
	g_render->CapLineVerts(Vector2(0));
	g_render->RenderSimpleVerts();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool DeferredRender::CreateTexture(const IntVector2& size, LPDIRECT3DTEXTURE9& texture, D3DFORMAT format)
{
	return
	(
		SUCCEEDED(DXUTGetD3D9Device()->CreateTexture(
		size.x, size.y, 1,
		D3DUSAGE_RENDERTARGET, format,
		D3DPOOL_DEFAULT, &texture, NULL))
	);
}

void DeferredRender::InitDeviceObjects()
{
	{
		FrankRender::RenderPrimitive& rp = primitiveLightMask;
		
		#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_TEX1)
		struct CUSTOMVERTEX
		{
			D3DXVECTOR3 position;		// vertex position
			FLOAT tu, tv;				// texture coordinates
		};

		if 
		(
			rp.Create
			(
				2,							// primitiveCount
				4,							// vertexCount
				D3DPT_TRIANGLESTRIP,		// primitiveType
				sizeof(CUSTOMVERTEX),		// stride
				D3DFVF_CUSTOMVERTEX			// fvf
			)
		)
		{
			CUSTOMVERTEX* vertices;
			if
			(
				SUCCEEDED
				(
					rp.vb->Lock(0, 0, (VOID**)&vertices, 0)
				)
			)
			{
				vertices[0].position = D3DXVECTOR3( -1.0f,  1.0f, 0.0f );
				vertices[0].tu = -1.0f;
				vertices[0].tv = -1.0f;
				vertices[1].position = D3DXVECTOR3(  1.0f,  1.0f, 0.0f );
				vertices[1].tu = 2.0f;
				vertices[1].tv = -1.0f;
				vertices[2].position = D3DXVECTOR3( -1.0f, -1.0f, 0.0f );
				vertices[2].tu = -1.0f;
				vertices[2].tv = 2.0f;
				vertices[3].position = D3DXVECTOR3(  1.0f, -1.0f, 0.0f );
				vertices[3].tu = 2.0f;
				vertices[3].tv = 2.0f;

				rp.vb->Unlock();
			}
		}
	}

	// use smaller texture format to make lighting faster
	D3DFORMAT lightTextureFormat = use32BitTextures? D3DFMT_X8R8G8B8 : D3DFMT_A1R5G5B5;

	if (!CreateTexture(IntVector2(int(textureSize)), texture, lightTextureFormat))
	{
		// fall back to more common format
		lightTextureFormat = D3DFMT_X8R8G8B8;

		if (!CreateTexture(IntVector2(int(textureSize)), texture, lightTextureFormat))
		{
			g_debugMessageSystem.AddError(L"Light map texture failed to create! Lights disabled.");
			lightEnable = false;
			return;
		}
	}
	
	if (!CreateTexture(IntVector2(int(shadowMapTextureSize)), textureShadowMap, lightTextureFormat))
	{
		g_debugMessageSystem.AddError(L"Shadow map texture failed to create! Lights disabled.");
		lightEnable = false;
		return;
	}
	
	if (!CreateTexture(IntVector2(int(emissiveTextureSize)), textureEmissive, lightTextureFormat))
	{
		g_debugMessageSystem.AddError(L"Emissive map texture failed to create! Emissive lights disabled.");
		emissiveLightEnable = false;
		return;
	}
	
	if (!CreateTexture(IntVector2(int(shadowMapTextureSize)), textureShadowMapDirectional, lightTextureFormat))
	{
		g_debugMessageSystem.AddError(L"Shadow map directional texture failed to create! Lights disabled.");
		lightEnable = false;
		return;
	}

	if (!CreateTexture(IntVector2(int(visionTextureSize)), textureVision, lightTextureFormat))
	{
		g_debugMessageSystem.AddError(L"Vision map texture failed to create! Vision disabled.");
		visionEnable = false;
	}

	{
		finalTextureSize.x = float(g_backBufferWidth);
		finalTextureSize.y = float(g_backBufferHeight);
		finalTextureSize *= finalTextureSizeScale * finalTextureCameraScale;

		// round to nearest multiple of 4
		finalTextureSize.x = 4.0f * (int)(finalTextureSize.x / 4);
		finalTextureSize.y = 4.0f * (int)(finalTextureSize.y / 4);
	}
	
	if (!CreateTexture(finalTextureSize, textureFinal))
	{
		// try a safer texture size
		finalTextureSize = Vector2(512, 512);
		if (!CreateTexture(finalTextureSize, textureFinal))
		{
			g_debugMessageSystem.AddError(L"Final light map texture failed to create. Lights disabled!");
			lightEnable = false;
			return;
		}
	}

	{
		int maxSize = 0;
		maxSize = Max(maxSize, int(textureSize));
		maxSize = Max(maxSize, int(shadowMapTextureSize));
		maxSize = Max(maxSize, int(emissiveTextureSize));
		maxSize = Max(maxSize, int(visionTextureSize));

		// create a list of different texture sizes to swap with
		int size = textureSwapStartSize;
		for(int i = 0; i < textureSwapArraySize; ++i)
		{
			if (size > maxSize)
			{
				textureSwapArray[i] = NULL;
			}
			else
			{
				CreateTexture(IntVector2(size), textureSwapArray[i], lightTextureFormat);
				size *= 2;
			}
		}
	}
	
	{
		if (!g_render->LoadPixelShader(L"blurShader.psh", blurShader, blurConstantTable, false))
		if (!g_render->LoadPixelShader(L"data/shaders/blurShader.psh", blurShader, blurConstantTable, false))
		if (!g_render->LoadPixelShader(L"blurShader.psh", blurShader, blurConstantTable))
		{
			if (lightEnable)
				g_debugMessageSystem.AddError(L"Blur shader failed to create. Blur disabled.");
		}
	}
	
	{
		if (!g_render->LoadPixelShader(L"visionShader.psh", visionShader, visionConstantTable, false))
		if (!g_render->LoadPixelShader(L"data/shaders/visionShader.psh", visionShader, visionConstantTable, false))
		if (!g_render->LoadPixelShader(L"visionShader.psh", visionShader, visionConstantTable))
		{
			//g_debugMessageSystem.AddError(L"Vision shader failed to create.");
		}
	}
		
	if (!g_render->LoadPixelShader(L"directionalLightShader.psh", directionalLightShader, directionalLightConstantTable, false))
	if (!g_render->LoadPixelShader(L"data/shaders/directionalLightShader.psh", directionalLightShader, directionalLightConstantTable, false))
	if (!g_render->LoadPixelShader(L"directionalLightShader.psh", directionalLightShader, directionalLightConstantTable))
	{
		if (lightEnable && directionalLightEnable)
			g_debugMessageSystem.AddError(L"Directional light shader failed to create. Directional light disabled!");
		directionalLightEnable = false;
	}

	if (normalMappingEnable)
	{
		if (!CreateTexture(finalTextureSize, textureNormalMap))
		{
			g_debugMessageSystem.AddError(L"Normal map texture failed to create. Normal mapping disabled!");
			normalMappingEnable = false;
			return;
		}
		if (!CreateTexture(finalTextureSize, textureSpecularMap))
		{
			g_debugMessageSystem.AddError(L"Normal map texture failed to create. Normal mapping disabled!");
			normalMappingEnable = false;
			return;
		}
		
		if (!g_render->LoadPixelShader(L"deferredLightShader.psh", deferredLightShader, shadowLightConstantTable, false))
		if (!g_render->LoadPixelShader(L"data/shaders/deferredLightShader.psh", deferredLightShader, shadowLightConstantTable, false))
		if (!g_render->LoadPixelShader(L"deferredLightShader.psh", deferredLightShader, shadowLightConstantTable))
		{
			g_debugMessageSystem.AddError(L"Deferred light shader failed to create. Normal mapping disabled!");
			normalMappingEnable = false;
			return;
		}
	}
}

void DeferredRender::DestroyDeviceObjects()
{
	for(int i = 0; i < textureSwapArraySize; ++i)
		SAFE_RELEASE(textureSwapArray[i]);
	SAFE_RELEASE(texture);
	SAFE_RELEASE(textureNormalMap);
	SAFE_RELEASE(textureSpecularMap);
	SAFE_RELEASE(textureShadowMap);
	SAFE_RELEASE(textureShadowMapDirectional);
	SAFE_RELEASE(textureVision);
	SAFE_RELEASE(textureFinal);
	SAFE_RELEASE(shadowLightConstantTable);
	SAFE_RELEASE(deferredLightShader);
	SAFE_RELEASE(textureEmissive);
	SAFE_RELEASE(visionConstantTable);
	SAFE_RELEASE(visionShader);
	SAFE_RELEASE(blurShader);
	SAFE_RELEASE(blurConstantTable);
	SAFE_RELEASE(directionalLightConstantTable);
	SAFE_RELEASE(directionalLightShader);
	primitiveLightMask.SafeRelease();
}

void DeferredRender::GlobalUpdate()
{
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"DeferredRender::GlobalUpdate()" );
	{
		FrankProfilerEntryDefine(L"DeferredRender::GlobalPreUpdate()", Color::White(), 10);

		dynamicLightCount = 0;
		simpleLightCount = 0;

		if (g_gameControlBase->IsEditMode() && !g_gameControlBase->IsEditPreviewMode() || !lightEnable)
			return;

		if (!g_render->GetTexture(Texture_LightMask))
		{
			g_debugMessageSystem.AddError( L"Light mask texture not found. Lighting disabled!" );
			lightEnable = false;
			return;
		}

		if (!g_render->GetTexture(overbrightTexture))
		{
			g_debugMessageSystem.AddError( L"Overbright texture not found. Lighting disabled!" );
			lightEnable = false;
			return;
		}

		// pre render a shadow map and normal map for the entire scene
		RenderShadowMap();
		RenderNormalMap();
		RenderSpecularMap();

		// clear the final texture
		IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
		LPDIRECT3DSURFACE9 renderSurface = NULL;
		textureFinal->GetSurfaceLevel(0, &renderSurface);
		if (shadowSaveTextures)
			D3DXSaveSurfaceToFile( L"shadowMapFinal.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
		pd3dDevice->SetRenderTarget(0, renderSurface);
		pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, ambientLightColor, 1.0f, 0);
		SAFE_RELEASE(renderSurface);

		// leave camera in the normal state
		g_cameraBase->PrepareForRender();
	}
	{
		FrankProfilerEntryDefine(L"DeferredRender::GlobalPostUpdate()", Color::White(), 10);
		
		{
			// update all the lights
			list<Light*> simpleLights;
			list<Light*> dynamicLights;
			GameObjectHashTable& objects = g_objectManager.GetObjects();
			for (GameObjectHashTable::iterator it = objects.begin(); it != objects.end(); ++it)
			{
				GameObject& object = *((*it).second);
				if (!object.IsLight() || object.IsDestroyed())
					continue;
		
				Light& light = static_cast<Light&>(object);
				if (!light.GetIsActive())
					continue;

				if (light.IsSimpleLight())
					simpleLights.push_back(&light);
				else
					dynamicLights.push_back(&light);
			}
		
			// sort dynamic lights so higher priority lights are first
			dynamicLights.sort(Light::SortCompare);
			for (Light* dynamicLight : dynamicLights)
					UpdateDynamicLight(*dynamicLight);
			UpdateSimpleLights(simpleLights);
		}

		if (g_gameControlBase->IsEditMode() && !g_gameControlBase->IsEditPreviewMode() || !lightEnable)
			return;
	
		 if (showTexture == 3)
			 return; // test display of shadow map

		RenderDirectionalPass();
		RenderEmissivePass();

		if (lightEnable && visionEnable)
			RenderVisionShadowMap();

		RenderVisionPass(g_gameControlBase->GetUserPosition(), textureVision);
	}
}

LPDIRECT3DTEXTURE9& DeferredRender::GetSwapTexture(LPDIRECT3DTEXTURE9& texture)
{
	const IntVector2 textureSize = g_render->GetTextureSize(texture);
	const int i = Log2Int(textureSize.x)-Log2Int(textureSwapStartSize);

	ASSERT(i > 0 && i <= textureSwapArraySize);
	ASSERT(textureSwapArray[i]);
	ASSERT(g_render->GetTextureSize(texture) == g_render->GetTextureSize(textureSwapArray[i]));
	ASSERT(g_render->GetTextureFormat(texture) == g_render->GetTextureFormat(textureSwapArray[i]));

	return textureSwapArray[i];
}

void DeferredRender::SwapTextures(LPDIRECT3DTEXTURE9& texture1, LPDIRECT3DTEXTURE9& texture2)
{
	ASSERT(g_render->GetTextureSize(texture1) == g_render->GetTextureSize(texture2));
	ASSERT(g_render->GetTextureFormat(texture) == g_render->GetTextureFormat(texture2));
	swap(texture1, texture2);
}

void DeferredRender::GlobalRender()
{
	FrankProfilerEntryDefine(L"DeferredRender::GlobalRender()", Color::White(), 10);
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"DeferredRender::GlobalRender()" );

	if (g_gameControlBase->IsEditMode() && !g_gameControlBase->IsEditPreviewMode() || !lightEnable)
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
	// save the camera transforms
	D3DXMATRIX viewMatrixOld, projectionMatrixOld;
	pd3dDevice->GetTransform(D3DTS_VIEW, &viewMatrixOld);
	pd3dDevice->GetTransform(D3DTS_PROJECTION, &projectionMatrixOld);

	// setup camera for the final shadow render
	D3DXMATRIX matrixIdentity;
	D3DXMatrixIdentity(&matrixIdentity);
	pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixIdentity);
	pd3dDevice->SetTransform(D3DTS_VIEW, &matrixIdentity);
	SetFiltering(true);
	
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	
	LPDIRECT3DTEXTURE9 textureRender = textureFinal;
	Color color = Color::White();

	Vector2 cameraSize;
	XForm2 xfFinal = GetFinalTransform(finalTextureSize, &cameraSize);
	Camera& camera = *g_cameraBase;
	XForm2 xf = xfFinal;

	if (showTexture > 0)
	{
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );

		if (showTexture == 7)
		{
			// final light texture
		}
		else if (showTexture == 2)
		{
			// normal map
			textureRender = textureNormalMap;
		}
		else if (showTexture == 3)
		{
			// specular map
			textureRender = textureSpecularMap;
		}
		else if (showTexture == 4)
		{
			// emissive
			// use square aspect ratio
			const float oldAspect = camera.GetAspectRatio();
			const float oldLockedAspect = camera.GetLockedAspectRatio();
			camera.SetAspectRatio(1);
			camera.SetLockedAspectRatio(1);
			const float zoom = GetShadowMapZoom() * oldLockedAspect;
			xf = GetFinalTransform(Vector2(emissiveTextureSize), &cameraSize, &zoom);
			camera.SetAspectRatio(oldAspect);
			camera.SetLockedAspectRatio(oldLockedAspect);
			color = Color::White();
			textureRender = textureEmissive;
		}
		else if (showTexture == 5)
		{
			// shadow map
			color = Color::White();
			textureRender = textureShadowMap;
			const float shadowMapZoom = GetShadowMapZoom();
			xf = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
			cameraSize *= shadowMapScale;
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		}
		else if (showTexture == 6)
		{
			// shadow map background
			color = Color::White();
			textureRender = textureShadowMapDirectional;
			const float shadowMapZoom = GetShadowMapZoom();
			xf = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
			cameraSize *= shadowMapScale;
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		}
		else if (showTexture == 8)
		{
			// vision shadow map
			color = Color::White();
			textureRender = textureShadowMap;
			const float shadowMapZoom = GetShadowMapZoom();
			xf = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
			cameraSize *= shadowMapScale;
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		}
	}

	//g_cameraBase->PrepareForRender(xfFinal, finalTextureCameraScale*GetShadowMapZoom());

	// set up camera transforms
	g_cameraBase->PrepareForRender();

	// render the final shadow overlay
	// hack: half pixel offset doesn't seem right, fudged to make it better
	const Vector2 halfPixelOffset = 2 * halfPixel * cameraSize / finalTextureSize;
	if (showTexture != 1)
		g_render->RenderQuad(XForm2(halfPixelOffset) * xf.position, cameraSize, color, textureRender);

	if (emissiveLightEnable && showTexture == 0 && emissiveBloomAlpha > 0)
	{
		// emissive bloom pass
		// use square aspect ratio
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
		const float oldAspect = camera.GetAspectRatio();
		const float oldLockedAspect = camera.GetLockedAspectRatio();
		camera.SetAspectRatio(1);
		camera.SetLockedAspectRatio(1);
		const float zoom = GetShadowMapZoom() *oldLockedAspect;
		xf = GetFinalTransform(Vector2(emissiveTextureSize), &cameraSize, &zoom);
		camera.SetAspectRatio(oldAspect);
		camera.SetLockedAspectRatio(oldLockedAspect);
		g_cameraBase->PrepareForRender();
		g_render->RenderQuad(XForm2(halfPixelOffset) * xf.position, cameraSize, Color::White(emissiveBloomAlpha), textureEmissive);
	}

	if (visionEnable && showTexture == 0 && !g_gameControlBase->IsEditPreviewMode())
	{
		const XForm2 xfPlayer = g_gameControlBase->GetUserPosition();

		/*if (visionShader)
		{
			pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );

			pd3dDevice->SetPixelShader(visionShader);
			g_render->RenderQuad(xf.TransformCoord(halfPixelOffset), Vector2(visionRadius), Color::White(), texture);
			pd3dDevice->SetPixelShader(NULL);
			
			pd3dDevice->SetTexture(1, NULL);
		}
		else*/
		{
			pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
			pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
			pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
		
			//const Vector2 halfPixelOffset = cameraSize * halfPixel / visionTextureSize;
			g_render->RenderQuad(XForm2(halfPixel * visionRadius / finalTextureSize) * xfPlayer.position, Vector2(visionRadius), Color::White(), textureVision);
			g_gameControlBase->RenderVision();
		}
	}

	// set stuff back to normal
	pd3dDevice->SetTransform(D3DTS_VIEW, &viewMatrixOld);
	pd3dDevice->SetTransform(D3DTS_PROJECTION, &projectionMatrixOld);
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
	shadowSaveTextures = false;
}

void DeferredRender::Invert(LPDIRECT3DTEXTURE9& texture, const Vector2& invertTextureSize)
{
	LPDIRECT3DTEXTURE9& texture2 = GetSwapTexture(texture);

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const Vector2 halfPixelOffset = halfPixel / invertTextureSize;
	
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT);

	// set up camera transforms
	D3DXMATRIX matrixIdentity;
	D3DXMatrixIdentity(&matrixIdentity);
	pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixIdentity);
	pd3dDevice->SetTransform(D3DTS_VIEW, &matrixIdentity);
	{
		LPDIRECT3DSURFACE9 renderSurface = NULL;
		(texture2)->GetSurfaceLevel(0, &renderSurface);
		pd3dDevice->SetRenderTarget(0, renderSurface);
		g_render->BeginRender(true);

		// render the quad
		g_render->RenderQuad(halfPixelOffset, Vector2(1), Color::White(), texture);
		
		g_render->EndRender();
		//D3DXSaveSurfaceToFile( L"Invert.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
		SAFE_RELEASE(renderSurface);

		// switch it with the swap texture
		SwapTextures(texture, texture2);
	}
	pd3dDevice->SetPixelShader(NULL);
	
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
}

void DeferredRender::ApplyBlur(LPDIRECT3DTEXTURE9& texture, const Vector2& blurTextureSize, float brightness, float blurSize, int passCount)
{
	if (!blurShader)
		return;

	LPDIRECT3DTEXTURE9& texture2 = GetSwapTexture(texture);

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const Vector2 halfPixelOffset = halfPixel / blurTextureSize;
	const float size = 0.5f*blurSize / blurTextureSize.x;
	
	blurConstantTable->SetFloat(pd3dDevice, "blurSize",			size);
	blurConstantTable->SetFloat(pd3dDevice, "blurBrightness",	brightness);
	
	// set up camera transforms
	D3DXMATRIX matrixIdentity;
	D3DXMatrixIdentity(&matrixIdentity);
	pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixIdentity);
	pd3dDevice->SetTransform(D3DTS_VIEW, &matrixIdentity);

	// set render states
	pd3dDevice->SetPixelShader(blurShader);

	for(int i = 0; i < passCount; ++i)
	{
		// apply a gausian blur
		LPDIRECT3DSURFACE9 renderSurface = NULL;
		(texture2)->GetSurfaceLevel(0, &renderSurface);
		pd3dDevice->SetRenderTarget(0, renderSurface);
		g_render->BeginRender(true, Color::Black());
		{
			// do a simple gausian blur
			g_render->RenderQuad(halfPixelOffset, Vector2(1), Color::White(), texture);
		}

		g_render->EndRender();
		SAFE_RELEASE(renderSurface);

		// switch it with the swap texture
		SwapTextures(texture, texture2);
	}
	pd3dDevice->SetPixelShader(NULL);
}

XForm2 DeferredRender::GetFinalTransform(const Vector2& textureRoundSize, Vector2* cameraSize, const float* zoomOverride)
{
	// correct for flicker by rounding to nearest pixel
	// round camera xf to nearest pixel pos
	Camera& camera = *g_cameraBase;
	XForm2 xf = camera.GetXFInterpolated();
	xf.angle = 0;
	
	const float aspectFix = camera.GetAspectFix();
	const Vector2 aspectRatioScale = camera.GetAspectRatioScale();
	float zoomScale = GetShadowMapZoom();
	if (zoomOverride)
		zoomScale = *zoomOverride;
	Vector2 zoom = zoomScale * finalTextureCameraScale * aspectFix * aspectRatioScale;

	if (lightPixelRoundingEnable)
	{
		const Vector2 pixelWorldSize = ((shadowMapScale) * zoom) / textureRoundSize;
		xf.position.x = pixelWorldSize.x * (float)(int)(xf.position.x / pixelWorldSize.x);
		xf.position.y = pixelWorldSize.y * (float)(int)(xf.position.y / pixelWorldSize.y);
	}

	if (cameraSize)
		*cameraSize = 0.5f * zoom;

	return xf;
}

float DeferredRender::GetShadowMapZoom()
{
	// always use max zoom for shadow map zoom to prevent aliasing and flicker
	const float zoomMax = g_cameraBase->GetMaxGameplayZoom();
	if (!g_gameControlBase->showDebugInfo && !g_gameControlBase->IsEditPreviewMode() && !Camera::cameraManualOverride)
		return zoomMax;
	
	// for dev modes the zoom cam go larger
	const float zoomInterp = g_cameraBase->GetZoomInterpolated();
	return (zoomInterp > zoomMax) ? zoomInterp : zoomMax;
}

void DeferredRender::SetFiltering(bool forceLinear)
{
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	if (!g_usePointFiltering || forceLinear)
	{
		// do linear filtering
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
	}
	else
	{
		// do point filtering
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
		pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
	}
}

bool DeferredRender::GetLightValue(const Vector2& pos, float& value, float sampleRadius)
{
	// warning: this function is very slow
    HRESULT hr;

	if (!textureFinal)
		return false;
	
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

	LPDIRECT3DSURFACE9 renderSurface = NULL;
	hr = textureFinal->GetSurfaceLevel(0, &renderSurface);
    if (FAILED(hr))
        return false;

    D3DSURFACE_DESC rtDesc;
    renderSurface->GetDesc( &rtDesc );

	// todo: pre-create the surface in init device objects and only copy once per frame
    LPDIRECT3DSURFACE9 plainSurface;
    hr = pd3dDevice->CreateOffscreenPlainSurface( rtDesc.Width, rtDesc.Height, rtDesc.Format, D3DPOOL_SYSTEMMEM, &plainSurface, NULL );
    if (FAILED(hr))
        return false;

	hr = pd3dDevice->GetRenderTargetData( renderSurface, plainSurface );
    if (FAILED(hr))
	{
		SAFE_RELEASE(plainSurface);
		return false;
	}

    D3DLOCKED_RECT lockedRect;
    RECT rect;
    rect.left = 0;
    rect.right = rtDesc.Width;
    rect.top = 0;
    rect.bottom = rtDesc.Height;

    hr = plainSurface->LockRect( &lockedRect, &rect, D3DLOCK_READONLY );
    if (FAILED(hr))
	{
		SAFE_RELEASE(plainSurface);
		return false;
	}

	LPDIRECT3DTEXTURE9 texture = textureFinal;
	Vector2 cameraSize;
	XForm2 xfFinal = GetFinalTransform(finalTextureSize, &cameraSize);
	XForm2 xfInverse = xfFinal.Inverse();
	
	bool isValid = false;
	value = 0;
	int valueCount = 0;
	for(int i = 0; i < 20; ++i)
	{
		Vector2 samplePos = pos + Vector2::BuildRandomUnitVector() * sampleRadius;
		Vector2 localPos = xfInverse.TransformCoord(pos) + cameraSize;
		localPos *= finalTextureSize/(2*cameraSize);
		IntVector2 texturePos = localPos;
		if (texturePos.x >= 0 && texturePos.y >= 0 && texturePos.x < finalTextureSize.x && texturePos.y < finalTextureSize.y)
		{
			const DWORD valueDword = *(reinterpret_cast<LPDWORD>(lockedRect.pBits) + (rtDesc.Height - texturePos.y - 1) * (lockedRect.Pitch / 4) + texturePos.x);
			const D3DXCOLOR pixelColor = D3DXCOLOR(valueDword);
			value += (pixelColor.r + pixelColor.g + pixelColor.b) / 3;
			++valueCount;
		}
	}

	if (valueCount > 0)
	{
		value = value / float(valueCount);
		isValid = true;
	}

	SAFE_RELEASE(renderSurface);
	plainSurface->UnlockRect();
	SAFE_RELEASE(plainSurface);

	return isValid;
}

void DeferredRender::CycleShowTexture()
{
	++showTexture;

	if (!normalMappingEnable && showTexture == 2)
		++showTexture;
	if (!normalMappingEnable && showTexture == 3)
		++showTexture;
	if (!visionEnable && showTexture == 8)
		++showTexture;

	showTexture = showTexture % 9;
}

WCHAR* DeferredRender::GetShowTextureModeString()
{
	switch (showTexture)
	{
		case 0: return L"0 None";
		case 1: return L"1 Diffuse";
		case 2: return L"2 Normals";
		case 3: return L"3 Specular";
		case 4: return L"4 Emissive";
		case 5: return L"5 ShadowMap";
		case 6: return L"6 BackgroundShadowMap";
		case 7: return L"7 Final";
		case 8: return L"8 Vision";
	}

	return L"Unknown";
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
// Deferred render passes

void DeferredRender::RenderNormalMap()
{
	if (!lightEnable || !normalMappingEnable || !g_render->IsNormalMapShaderLoaded())
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
	// set up default render states
	SetFiltering();
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	
	// setup camera to render normal map
	XForm2 xf = GetFinalTransform(finalTextureSize);
	g_cameraBase->PrepareForRender(xf, finalTextureCameraScale*GetShadowMapZoom());
	
	// prepare render to normal map
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	textureNormalMap->GetSurfaceLevel(0, &renderSurface);
 	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender(true, Color(0.5f, 0.5f, 1.0f));
	{
		// render the normal maps
		renderPass = RenderPass_normals;
		g_gameControlBase->RenderInterpolatedObjects();
		renderPass = RenderPass_diffuse;
	}
	g_render->EndRender();
	if (shadowSaveTextures)
		D3DXSaveSurfaceToFile( L"normalMapBuffer.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
	SAFE_RELEASE(renderSurface);
	
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
}

void DeferredRender::RenderSpecularMap()
{
	if (!lightEnable || !normalMappingEnable || !g_render->IsNormalMapShaderLoaded())
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
	// set up default render states
	SetFiltering();
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	
	// setup camera to render specular map
	XForm2 xf = GetFinalTransform(finalTextureSize);
	g_cameraBase->PrepareForRender(xf, finalTextureCameraScale*GetShadowMapZoom());
	
	// prepare render to specular map
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	textureSpecularMap->GetSurfaceLevel(0, &renderSurface);
 	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender(true, Color::Black());
	{
		// render the specular maps
		renderPass = RenderPass_specular;
		g_gameControlBase->RenderInterpolatedObjects();
		renderPass = RenderPass_diffuse;
	}
	g_render->EndRender();
	if (shadowSaveTextures)
		D3DXSaveSurfaceToFile( L"specularMapBuffer.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
	SAFE_RELEASE(renderSurface);
	
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
}

void DeferredRender::RenderShadowMap()
{
	if (!lightEnable || !shadowEnable && !visionEnable && !directionalLightEnable)
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
	// set up default render states
	SetFiltering();
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );

	// setup camera to render shadow objects
	Vector2 cameraSize;
	const float shadowMapZoom = GetShadowMapZoom();
	XForm2 xf = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
	cameraSize *= shadowMapScale;
	g_cameraBase->PrepareForRender(xf, shadowMapScale*finalTextureCameraScale*shadowMapZoom);
	
	// prepare render to shadow texture
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	textureShadowMap->GetSurfaceLevel(0, &renderSurface);
 	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender(true);
	{
		// render the shadow objects
		renderPass = RenderPass_lightShadow;
		g_gameControlBase->RenderInterpolatedObjects();
		renderPass = RenderPass_diffuse;
	}
	g_render->EndRender();
	if (shadowSaveTextures)
		D3DXSaveSurfaceToFile( L"shadowMapBuffer.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
	SAFE_RELEASE(renderSurface);

	if (directionalLightEnable)
	{
		// render only the objects that didnt get rendered the first time
		LPDIRECT3DSURFACE9 renderSurfaceDirectional;
		textureShadowMapDirectional->GetSurfaceLevel(0, &renderSurfaceDirectional);
 		pd3dDevice->SetRenderTarget(0, renderSurfaceDirectional);
		g_render->BeginRender(true);
		{
			// setup camera to render shadow objects
			g_cameraBase->PrepareForRender(xf, shadowMapScale*finalTextureCameraScale*shadowMapZoom);

			renderPass = RenderPass_directionalShadow;
			g_gameControlBase->RenderInterpolatedObjects();
			renderPass = RenderPass_diffuse;
		}
		g_render->EndRender();
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
		if (shadowSaveTextures)
			D3DXSaveSurfaceToFile( L"shadowMapBufferDirectional.jpg", D3DXIFF_JPG, renderSurfaceDirectional, NULL, NULL );
		SAFE_RELEASE(renderSurfaceDirectional);
	}

	// apply a slight blur to the shadow map to help reduce artifacts
	SetFiltering(true);
	ApplyBlur(textureShadowMap, Vector2(shadowMapTextureSize), 1, 1, 1);

	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x00FFFFFF );
}

void DeferredRender::RenderEmissivePass()
{
	if (!emissiveLightEnable)
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const Vector2 halfPixelOffset = halfPixel / emissiveTextureSize;
	
	// set up default render states
	SetFiltering();
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	
	// correct for flicker by rounding to nearest pixel
	// round camera xf to nearest pixel pos
	Camera& camera = *g_cameraBase;
	Vector2 cameraSize;
	// use square aspect ratio
	const float oldAspect = camera.GetAspectRatio();
	const float oldLockedAspect = camera.GetLockedAspectRatio();
	camera.SetAspectRatio(1);
	camera.SetLockedAspectRatio(1);
	const float zoom = GetShadowMapZoom() * oldLockedAspect;
	XForm2 xf = GetFinalTransform(Vector2(emissiveTextureSize), &cameraSize, &zoom);
	
	// setup camera to render shadow objects
	g_cameraBase->PrepareForRender(xf, finalTextureCameraScale*zoom);

	// prepare render to shadow texture
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	textureEmissive->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender(true, emissiveBackgroundColor);
	{
		// render the shadow objects
		pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x00000000 );	// force everything to black
		renderPass = RenderPass_emissive;
		g_gameControlBase->RenderInterpolatedObjects();
		pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x00FFFFFF );
		renderPass = RenderPass_diffuse;
	}
	g_render->EndRender();
	if (shadowSaveTextures)
		D3DXSaveSurfaceToFile( L"shadowMapEmissive.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
	SAFE_RELEASE(renderSurface);
	camera.SetAspectRatio(oldAspect);
	camera.SetLockedAspectRatio(oldLockedAspect);
	
	// force linear filtering
	SetFiltering(true);

	ApplyBlur(textureEmissive, Vector2(emissiveTextureSize), emissiveBlurAlpha, emissiveBlurSize, emissiveBlurPassCount);
	
	if (emissiveAlpha > 0)
	{
		// render to final shadow texture
		textureFinal->GetSurfaceLevel(0, &renderSurface);
		pd3dDevice->SetRenderTarget(0, renderSurface);
		g_render->BeginRender();
		{
			// setup camera to render the emsisive texture
			XForm2 xfFinal = GetFinalTransform(finalTextureSize);
			g_cameraBase->PrepareForRender(xfFinal, finalTextureCameraScale*GetShadowMapZoom());

			pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
			pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
			pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		
			Vector2 offset = cameraSize * halfPixelOffset / finalTextureSize;
			Vector2 scale = cameraSize;
			g_render->RenderQuad(offset + xf.position, scale, Color::White(emissiveAlpha), textureEmissive);
		}
		g_render->EndRender();
		SAFE_RELEASE(renderSurface);
	}
	
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
}

void DeferredRender::RenderDirectionalPass()
{
	if (!directionalLightEnable)
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
	// set up default render states
	SetFiltering(true);
	pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x00FFFFFF );
	
	Vector2 cameraSize;
	const float shadowMapZoom = GetShadowMapZoom();
	XForm2 xf = GetFinalTransform(Vector2(textureSize), &cameraSize, &shadowMapZoom);
	cameraSize *= finalTextureCameraScale;

	// render to final shadow texture
	LPDIRECT3DTEXTURE9& textureSwap = GetSwapTexture(texture);
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	texture->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender(true);
	{
		g_cameraBase->PrepareForRender(xf, finalTextureCameraScale*shadowMapZoom);
		{
			// render the shadow objects
			const Vector2 halfPixelOffset = halfPixel / shadowMapTextureSize;
			Vector2 cameraSize;
			XForm2 xf = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
			cameraSize *= shadowMapScale;
			Vector2 offset = cameraSize * halfPixelOffset;

			// disable alpha to prevent blending
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
			g_render->RenderQuad(offset + xf.position, cameraSize, Color::White(), textureShadowMapDirectional);

			// multiply by foreground shadow map
			pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
			pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
			pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			g_render->RenderQuad(offset + xf.position, cameraSize, Color::White(), textureShadowMap);
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
		}
		
		// setup camera for the shadow casting
		D3DXMATRIX matrixIdentity;
		D3DXMatrixIdentity(&matrixIdentity);
		pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixIdentity);
		pd3dDevice->SetTransform(D3DTS_VIEW, &matrixIdentity);
		
		// set up additive blending
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

		{
			// end the render and copy the texture to the swap texture
			g_render->EndRender();

			// copy texture
			LPDIRECT3DSURFACE9 renderSurfaceSwap;
			textureSwap->GetSurfaceLevel(0, &renderSurfaceSwap);
			pd3dDevice->StretchRect(renderSurface, NULL, renderSurfaceSwap, NULL, D3DTEXF_NONE);
			SAFE_RELEASE(renderSurfaceSwap);

			// start up a render on the new texture
			pd3dDevice->SetRenderTarget(0, renderSurface);
			g_render->BeginRender();
		}

		// fix up light direction to correct for background aspect
		Vector2 lightDirection = directionalLightDirection / g_cameraBase->GetAspectRatioScale();

		// render out many passes of the shadow sliding it in a direction each time
		const Vector2 halfPixelOffset = halfPixel / textureSize;
		float offset = directionalLightCastOffset;
		float scale = 1.0f;
		int passCount = 0;
		while (1)
		{
			// stretch out the shadow
			const Vector2 actualOffset = Vector2(offset) / 256.0f;
			directionalLightSoftening = CapPercent(directionalLightSoftening);
			g_render->RenderQuad(halfPixelOffset + actualOffset*lightDirection, Vector2(scale), Color::Grey(1, directionalLightSoftening), textureSwap);

			if (directionalLightForgroundPassCount > 0 && passCount <= directionalLightForgroundPassCount)
			{
				if (passCount == directionalLightForgroundPassCount)
				{
					offset = directionalLightCastOffset;
				}

				// redraw the shadow map every time to keep stuff dark
				g_cameraBase->PrepareForRender(xf, finalTextureCameraScale*shadowMapZoom);

				// set up blending
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
				pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

				// render the shadow objects
				const Vector2 halfPixelOffset = halfPixel / shadowMapTextureSize;
				Vector2 cameraSize;
				XForm2 xf = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
				cameraSize *= shadowMapScale;
				Vector2 offset = cameraSize * halfPixelOffset;
				g_render->RenderQuad(offset + xf.position, cameraSize, Color::White(), textureShadowMap);
				
				// set up additive blending
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
			
				// setup camera for the shadow casting
				D3DXMATRIX matrixIdentity;
				D3DXMatrixIdentity(&matrixIdentity);
				pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixIdentity);
				pd3dDevice->SetTransform(D3DTS_VIEW, &matrixIdentity);
			}

			scale *= directionalLightCastScale;
			offset += directionalLightCastOffset;

			if (++passCount >= directionalLightPassCount)
				 break;
			
			{
				// ping pong the texture to prep for another pass
				// end the render and copy the texture to the swap texture
				g_render->EndRender();

				// copy texture
				LPDIRECT3DSURFACE9 renderSurfaceSwap;
				textureSwap->GetSurfaceLevel(0, &renderSurfaceSwap);
				pd3dDevice->StretchRect(renderSurface, NULL, renderSurfaceSwap, NULL, D3DTEXF_NONE);
				SAFE_RELEASE(renderSurfaceSwap);

				// start up a render on the new texture
				pd3dDevice->SetRenderTarget(0, renderSurface);
				g_render->BeginRender();
			}
		}
	}
	g_render->EndRender();
	if (shadowSaveTextures)
		D3DXSaveSurfaceToFile( L"shadowMapBackground.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
	SAFE_RELEASE(renderSurface);

	{
		// apply an additive blur to erode the light texture
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		ApplyBlur(texture, Vector2(textureSize), directionalLightBlurBrightness, 1.0, directionalLightBlurPassCount);
	}

	// render to final shadow texture
	textureFinal->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender();
	{
		// set up camera transforms
		XForm2 xfFinal = GetFinalTransform(finalTextureSize);
		g_cameraBase->PrepareForRender(xfFinal, finalTextureCameraScale*GetShadowMapZoom());

		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		
		const Vector2 halfPixelOffset = halfPixel / textureSize;
		Color color = directionalLightColor * (directionalLightColor.a);
		
		// get the texture size and xf
		xf = GetFinalTransform(Vector2(textureSize), &cameraSize, &shadowMapZoom);
		Vector2 offset = cameraSize * halfPixelOffset;

		if (!normalMappingEnable || !directionalLightShader || !shadowLightConstantTable)
		{
			g_render->RenderQuad(offset + xf.position, cameraSize, color, texture);
		}
		else
		{
			{	
				// create a transform to convert texture space to normal map space
				const Vector2 size((shadowMapZoom)/shadowMapZoom);
				Vector2 offset = 0.5f*size*(xf.position - xfFinal.position)/cameraSize;
				offset = 0.5f*(Vector2(1) - size) + offset * Vector2(1,-1);

				// correct for half pixel offset in normal map
				offset -= Vector2(shadowMapZoom)/(2*textureSize*shadowMapZoom);

				D3DXMATRIX matrix
				(
					size.x,		0,			0,		0,
					0,			size.y,		0,		0,
					offset.x,	offset.y,	1,		0,
					0,			0,			0,		1
				);

				shadowLightConstantTable->SetMatrix(pd3dDevice, "normalMatrix", &matrix);
			}

			pd3dDevice->SetTexture(0, textureNormalMap);
			pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
			pd3dDevice->SetTexture(1, textureSpecularMap);
			pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 );
			pd3dDevice->SetTexture(2, texture);
			pd3dDevice->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, 0 );
			
			D3DXVECTOR4 lightDirection(-directionalLightDirection.x, directionalLightDirection.y, directionalLightHeight, 1);
			D3DXVec4Normalize(&lightDirection, &lightDirection);
			directionalLightConstantTable->SetVector(pd3dDevice, "lightDirection", &lightDirection);
			D3DXVECTOR4 finalColorVector = color;
			directionalLightConstantTable->SetVector(pd3dDevice, "lightColor", &finalColorVector);
			directionalLightConstantTable->SetFloat(pd3dDevice, "specularPower", specularPower);
			directionalLightConstantTable->SetFloat(pd3dDevice, "specularHeight", specularHeight);
			directionalLightConstantTable->SetFloat(pd3dDevice, "specularAmount", specularAmount);

			pd3dDevice->SetPixelShader(directionalLightShader);
			g_render->RenderQuadSimple(offset + xf.position, Vector2(cameraSize));

			pd3dDevice->SetPixelShader(NULL);
			pd3dDevice->SetTexture(1, NULL);
			pd3dDevice->SetTexture(2, NULL);
		}
	}
	g_render->EndRender();
	SAFE_RELEASE(renderSurface);
	
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
}

void DeferredRender::RenderVisionShadowMap()
{
	if (!lightEnable)
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
	// set up default render states
	SetFiltering();
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	
	// setup camera to render shadow objects
	Vector2 cameraSize;
	const float shadowMapZoom = GetShadowMapZoom();
	XForm2 xf = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
	cameraSize *= shadowMapScale;
	g_cameraBase->PrepareForRender(xf, shadowMapScale*finalTextureCameraScale*shadowMapZoom);
	
	// prepare render to shadow texture
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	textureShadowMap->GetSurfaceLevel(0, &renderSurface);
 	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender(true);
	{
		// render the shadow objects
		renderPass = RenderPass_vision;
		g_gameControlBase->RenderInterpolatedObjects();
		renderPass = RenderPass_diffuse;
	}
	g_render->EndRender();
	if (shadowSaveTextures)
		D3DXSaveSurfaceToFile( L"visionShadowMapBuffer.jpg", D3DXIFF_JPG, renderSurface, NULL, NULL );
	SAFE_RELEASE(renderSurface);

	// apply a slight blur to the vision map to help reduce artifacts
	SetFiltering(true);
	ApplyBlur(textureShadowMap, Vector2(shadowMapTextureSize), 1, 1, 2);

	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x00FFFFFF );
}

void DeferredRender::RenderVisionPass(const XForm2& xfInterpolated, LPDIRECT3DTEXTURE9& texture)
{
	FrankProfilerEntryDefine(L"DeferredRender::RenderVision()", Color::White(), 10);

	if (!lightEnable || !visionEnable || showTexture > 0 && showTexture != 1 || !texture)
		return;

	Color color = Color::White();
	const XForm2 xf(xfInterpolated.position);	// wipe out angle from light calculations
	
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const Vector2 halfPixelOffset = halfPixel / visionTextureSize;
	visionSoftening = CapPercent(visionSoftening);
	
	// set up default render states
	SetFiltering(true);
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );

	// prepare render to shadow texture
	LPDIRECT3DTEXTURE9& textureSwap = GetSwapTexture(texture);
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	texture->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);
	g_render->BeginRender(true);
	{
		// setup camera to render shadow objects in the space of the light
		const XForm2 xfViewInterp = xf.Inverse();
		const Matrix44 matrixView = Matrix44::BuildXFormZ(xfViewInterp.position, xfViewInterp.angle, 10.0f);
		pd3dDevice->SetTransform( D3DTS_VIEW, &matrixView.GetD3DXMatrix() );
		D3DXMATRIX d3dMatrixProjection;
		D3DXMatrixOrthoLH( &d3dMatrixProjection, visionRadius*2, visionRadius*2, 1, 1000);
		pd3dDevice->SetTransform( D3DTS_PROJECTION, &d3dMatrixProjection );
		
		{
			// render the shadow objects
			Vector2 cameraSize;
			const float shadowMapZoom = GetShadowMapZoom();
			XForm2 xfFinal = GetFinalTransform(Vector2(shadowMapTextureSize), &cameraSize, &shadowMapZoom);
			cameraSize *= shadowMapScale;
			Vector2 offset = cameraSize * halfPixelOffset;
			g_render->RenderQuad(offset + xfFinal.position, cameraSize, Color::White(0.5f), textureShadowMap);

			if (visionOverbrightRadius > 0)
			{
				// use the light mask as a gel for the overbright
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
				g_render->RenderQuad(XForm2(0.5f*visionOverbrightRadius*cameraSize * halfPixelOffset)*xf, Vector2(visionOverbrightRadius), Color::White(1.0f), overbrightTexture, false);
			}
		}

		{
			{
				// end the render and copy the texture to the swap texture
				g_render->EndRender();
				SAFE_RELEASE(renderSurface);

				{
					// apply an additive blur to erode the vision texture
					pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
					pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
					ApplyBlur(texture, Vector2(visionTextureSize), 1.2f, 2, visionPreBlurPassCount);
				}

				texture->GetSurfaceLevel(0, &renderSurface);

				// copy texture
				LPDIRECT3DSURFACE9 renderSurfaceSwap;
				textureSwap->GetSurfaceLevel(0, &renderSurfaceSwap);
				pd3dDevice->StretchRect(renderSurface, NULL, renderSurfaceSwap, NULL, D3DTEXF_NONE);
				SAFE_RELEASE(renderSurface);
			
				// swap swap texture with our texture
				SwapTextures(texture, textureSwap);
				renderSurface = renderSurfaceSwap;

				// start up a render on the new texture
				pd3dDevice->SetRenderTarget(0, renderSurface);
				g_render->BeginRender();
			}

			// setup camera for the shadow casting
			D3DXMATRIX matrixIdentity;
			D3DXMatrixIdentity(&matrixIdentity);
			pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixIdentity);
			pd3dDevice->SetTransform(D3DTS_VIEW, &matrixIdentity);
		
			// render out many passes of the shadow stretching it each time
			float scale = 1;
			int passCount = 0;
			while (1)
			{
				// soften the shadow
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
				float passPercent = Percent((float)passCount, visionPassCount-6.0f, 4.0f);
				passPercent = passPercent*passPercent;
				g_render->RenderQuad(halfPixelOffset, Vector2(1.0f), Color::Grey(1, passPercent*visionSoftening), Texture_Invalid, false);

				// stretch out the shadow
				pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
				pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
				const Vector2 actualScale = (Vector2(visionTextureSize) + Vector2(scale)) / visionTextureSize;
				g_render->RenderQuad(halfPixelOffset, actualScale, Color::Grey(1, 1), textureSwap);

				// scale up the shadow a little bit each time
				// reducing this towards 1 will increase smoothness but take more passes
				// the largest we can scale without gaps is 2
				scale *= visionCastScale;

				if (++passCount >= visionPassCount)
					 break;

				{
					// ping pong the texture to prep for another pass
					// end the render and copy the texture to the swap texture
					g_render->EndRender();

					// copy texture
					LPDIRECT3DSURFACE9 renderSurfaceSwap;
					textureSwap->GetSurfaceLevel(0, &renderSurfaceSwap);
					pd3dDevice->StretchRect(renderSurface, NULL, renderSurfaceSwap, NULL, D3DTEXF_NONE);
					SAFE_RELEASE(renderSurface);

					// swap swap texture with our texture
					SwapTextures(texture, textureSwap);
					renderSurface = renderSurfaceSwap;

					// start up a render on the new texture
					pd3dDevice->SetRenderTarget(0, renderSurface);
					g_render->BeginRender();
				}
			}
		}
		//{
		//	// render a light mask around the outside so it won't have square corners
		//	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
		//	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
		//	g_render->RenderQuad(halfPixelOffset, Vector2(1), Color::White(), Texture_LightMask, false);
		//}
		g_render->EndRender();
		
		if (shadowSaveTextures)
			D3DXSaveSurfaceToFile( L"visionMap.png", D3DXIFF_PNG, renderSurface, NULL, NULL );
		SAFE_RELEASE(renderSurface);
	}
	
	{
		// apply an additive blur to erode the vision texture
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		ApplyBlur(texture, Vector2(visionTextureSize), 1.8f, 2, visionPostBlurPassCount);
	}

	// set stuf back the way it was
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
}

DeferredRender::ScrollingTextureRenderBlock::ScrollingTextureRenderBlock(const Vector2& offset, const Vector2& scale, bool enable)
{
	if (!enable)
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	pd3dDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

	// set up the texture transform
	Matrix44 matrix = Matrix44::BuildScale(scale.x, scale.y, 0);
	D3DMATRIX& d3dmatrix = matrix.GetD3DXMatrix();
	d3dmatrix._31 = offset.x;
	d3dmatrix._32 = offset.y;
	pd3dDevice->SetTransform(D3DTS_TEXTURE0, &d3dmatrix);
	pd3dDevice->SetTransform(D3DTS_TEXTURE1, &d3dmatrix);
}

DeferredRender::ScrollingTextureRenderBlock::~ScrollingTextureRenderBlock()
{
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pd3dDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
}

DeferredRender::AlphaOnlyRenderBlock::AlphaOnlyRenderBlock(bool enable)
{
	if (!enable)
		return;

	DXUTGetD3D9Device()->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
}

DeferredRender::AlphaOnlyRenderBlock::~AlphaOnlyRenderBlock()
{
	DXUTGetD3D9Device()->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
}

DeferredRender::PointFilterRenderBlock::PointFilterRenderBlock(bool enable)
{
	g_render->SetPointFilter(enable);
}

DeferredRender::PointFilterRenderBlock::~PointFilterRenderBlock()
{
	g_render->SetFiltering();
}