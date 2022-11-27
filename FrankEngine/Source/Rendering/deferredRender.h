///////////////////////////////////////////////////////////////////////////////////////////////
/*
	Deferred Rendering System
	Copyright 2013 - Frank Force
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class Light;

class DeferredRender
{
public:
	static void InitDeviceObjects();
	static void DestroyDeviceObjects();
	static void GlobalRender();
	static void GlobalUpdate();
	static int GetDynamicLightCount()				{ return dynamicLightCount; }
	static int GetSimpleLightCount()				{ return simpleLightCount; }
	static bool GetLightValue(const Vector2& pos, float& value, float sampleRadius = 1.0f);

	static void CycleShowTexture();
	static WCHAR* GetShowTextureModeString();

	enum RenderPass
	{
		RenderPass_diffuse,
		RenderPass_emissive,
		RenderPass_lightShadow,
		RenderPass_directionalShadow,
		RenderPass_vision,
		RenderPass_normals,
		RenderPass_specular,
	};

	static RenderPass GetRenderPass()				{ return renderPass; }
	static bool GetRenderPassIsDiffuse()			{ return renderPass == RenderPass_diffuse; }
	static bool GetRenderPassIsEmissive()			{ return renderPass == RenderPass_emissive; }
	static bool GetRenderPassIsLight()				{ return renderPass == RenderPass_lightShadow; }
	static bool GetRenderPassIsDirectionalShadow()	{ return renderPass == RenderPass_directionalShadow; }
	static bool GetRenderPassIsVision()				{ return renderPass == RenderPass_vision; }
	static bool GetRenderPassIsNormalMap()			{ return renderPass == RenderPass_normals; }
	static bool GetRenderPassIsSpecular()			{ return renderPass == RenderPass_specular; }
	static bool GetRenderPassIsShadow()				{ return GetRenderPassIsLight() || GetRenderPassIsDirectionalShadow() || GetRenderPassIsVision(); }
	static bool GetRenderPassIsDeferred()			{ return GetRenderPassIsNormalMap() || GetRenderPassIsSpecular() || GetRenderPassIsEmissive(); }

	struct EmissiveRenderBlock
	{
		EmissiveRenderBlock(bool enable = true) { active = enable; if (enable && GetRenderPassIsEmissive()) DXUTGetD3D9Device()->SetRenderState( D3DRS_AMBIENT, 0x00FFFFFF ); }
		~EmissiveRenderBlock() { active = false; if (GetRenderPassIsEmissive()) DXUTGetD3D9Device()->SetRenderState( D3DRS_AMBIENT, 0x00000000 ); }
		static bool IsActive() { return active; }

		private:
		static bool active;
	};
	
	struct NoTextureRenderBlock
	{
		NoTextureRenderBlock(bool enable = true) { active = enable; if (enable) DXUTGetD3D9Device()->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG2 ); }
		~NoTextureRenderBlock() { active = false; DXUTGetD3D9Device()->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE ); }
		static bool IsActive() { return active; }

		private:
		static bool active;
	};

	struct TransparentRenderBlock
	{
		TransparentRenderBlock(bool enable = true)	{ active = enable; }
		~TransparentRenderBlock()					{ active = false; }
		static bool IsActive()						{ return active; }

		private:
		static bool active;
	};
	
	struct SpecularRenderBlock
	{
		SpecularRenderBlock(bool enable = true)	{ active = enable; }
		~SpecularRenderBlock()					{ active = false; }
		static bool IsActive()					{ return active; }

		private:
		static bool active;
	};

	struct BackgroundRenderBlock
	{
		BackgroundRenderBlock(bool enable = true) { active = enable; }
		~BackgroundRenderBlock() { active = false; }
		static bool IsActive() { return active; }

	private:
		static bool active;
	};
	
	struct DiffuseNormalAlphaRenderBlock
	{
		DiffuseNormalAlphaRenderBlock(float _alphaScale, bool enable = true)	{ {alphaScale = _alphaScale; active = enable;} }
		~DiffuseNormalAlphaRenderBlock()										{ active = false; }
		static bool IsActive()													{ return active; }
		static float GetAlphaScale()											{ return alphaScale; }

		private:
		static float alphaScale;
		static bool active;
	};
	
	struct AdditiveRenderBlock
	{
		AdditiveRenderBlock(bool enable = true) { if (enable && !(GetRenderPassIsNormalMap() || GetRenderPassIsSpecular())) DXUTGetD3D9Device()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE); }
		~AdditiveRenderBlock() { DXUTGetD3D9Device()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA); }
	};
	
	struct ScrollingTextureRenderBlock
	{
		ScrollingTextureRenderBlock(const Vector2& offset, const Vector2& scale = Vector2(1), bool enable = true);
		~ScrollingTextureRenderBlock();
	};
	
	struct AlphaOnlyRenderBlock
	{
		AlphaOnlyRenderBlock(bool enable = true);
		~AlphaOnlyRenderBlock();
	};
	
	struct PointFilterRenderBlock
	{
		PointFilterRenderBlock(bool enable = true);
		~PointFilterRenderBlock();
	};

	// global light settings
	static bool lightEnable;				// enable lighting
	static int maxDynamicLights;			// max limit on how many shadow and cone lights lights per frame
	static int maxSimpleLights;				// max limit on how many simple lights per frame
	static float alphaScale;				// globaly scales alpha of all light colors
	static float textureSize;				// size of texture used for lightmapping
	static float shadowMapTextureSize;		// size of texture used for texture maps
	static Color ambientLightColor;			// color of ambient light
	static float shadowMapScale;			// how much bigger to make the shadow map
	static bool normalMappingEnable;		// toggles normal mapping
	static float lightNormalBias;			// how much to bias the normal dot product
	static float lightCullScale;			// how much to scale radius before camera 
	static bool visionEnable;				// final pass that creates a mask for the player's vision
	static float visionRadius;				// radius of visiom
	static float visionOverbrightRadius;	// overbright for vision
	static float visionSoftening;			// softening for vision
	static float visionCastScale;			// shadow cast scale for visio
	static int visionPassCount;				// how many vision passes to do
	static float visionTextureSize;			// size of texture used for vision
	static int visionPreBlurPassCount;		// blur settings for vision
	static int visionPostBlurPassCount;		// blur settings for vision
	static float defaultLightRadius;		// radius for point lights
	static bool use32BitTextures;			// determines format used for lighting texture
	static TextureID overbrightTexture;		// circle texture needed for light rendering

	// shadow casting pass settings
	static bool shadowEnable;				// enable lights casting shadows
	static float shadowSoftening;			// how soft are shadows
	static float shadowCastScale;			// how much to scale shadow cast each pass
	static int shadowPassCount;				// how many shadow passes to do
	static float shadowPassStartSize;		// how big to start stretching shadow by, 1 for best results 
	static float shadowLightHeightDefault;	// default height used for normal mapping
	static Color defaultShadowColor;		// default shadow color to use for objects

	// emissive pass settings
	static bool emissiveLightEnable;		// enable emsisive lighting pass
	static int emissiveBlurPassCount;		// how many emissive blurs to do
	static float emissiveBlurAlpha;			// how bright is the emissive blur
	static float emissiveBlurSize;			// how much to blur the emssive pass
	static Color emissiveBackgroundColor;	// background color for emissive pass
	static float emissiveTextureSize;		// size of emissive texturets
	static float emissiveAlpha;				// alpha for normal emissive blend
	static float emissiveBloomAlpha;		// alpha for emissive bloom blend

	// directional light pass settings
	static bool directionalLightEnable;				// enables directional lighting pass
	static Color directionalLightColor;				// color of directional light
	static Vector2 directionalLightDirection;		// direction of directional light
	static float directionalLightSoftening;			// how much to soften directional shadows
	static float directionalLightCastOffset;		// how much to move directional cast each pass
	static float directionalLightCastScale;			// how much the directional generation is scaled each pass, 1 is no scale
	static float directionalLightHeight;			// height used for normal mapping
	static int directionalLightPassCount;			// number of passes for directional light
	static int directionalLightForgroundPassCount;	// how many passes of forground to draw for directional lights
	static float directionalLightBlurBrightness;	// how much to erode dark areas in directional pass
	static int directionalLightBlurPassCount;		// how many passes for directional blur

	// specular settings
	static float specularPower;
	static float specularHeight;
	static float specularAmount;

	// final pass settings
	static Vector2 finalTextureSize;		// size of final light overlay texture
	static float finalTextureSizeScale;		// how much to scale final texture based on back buffer size
	static float finalTextureCameraScale;	// how much extra to scale the final texture

	static RenderPass renderPass;
	static int dynamicLightCount;
	static int simpleLightCount;
	static bool lightDebug;
	static int showTexture;
	
	// hack: to handle rendering vision for multiple players
	static void RenderVisionPass(const XForm2& xfInterpolated, LPDIRECT3DTEXTURE9& texture);

	// hack: to allow custom textures
	static bool CreateTexture(const IntVector2& size, LPDIRECT3DTEXTURE9& texture, D3DFORMAT format = D3DFMT_X8R8G8B8);

private:

	static void RenderLightTexture(Light& light);
	static void RenderCone(Light& light, float angle);
	static void UpdateSimpleLight(Light& light, const XForm2& xfFinal, const Vector2& cameraSize);
	static void UpdateDynamicLight(Light& light);

	static XForm2 GetFinalTransform(const Vector2& textureRoundSize, Vector2* cameraSize = NULL, const float* zoomOverride = NULL);
	static void SetFiltering(bool forceLinear = false);
	static LPDIRECT3DTEXTURE9& GetSwapTexture(LPDIRECT3DTEXTURE9& texture);
	static void SwapTextures(LPDIRECT3DTEXTURE9& texture1, LPDIRECT3DTEXTURE9& texture2);
	
	static float GetShadowMapZoom();
	static void UpdateSimpleLights(list<Light*> simpleLights);
	static void Invert(LPDIRECT3DTEXTURE9& texture, const Vector2& invertTextureSize);
	static void ApplyBlur(LPDIRECT3DTEXTURE9& texture, const Vector2& blurTextureSize, float brightness, float blurSize, int passCount = 1);
	static void RenderShadowMap();
	static void RenderNormalMap();
	static void RenderSpecularMap();
	static void RenderEmissivePass();
	static void RenderDirectionalPass();
	static void RenderVisionShadowMap();

	static const int textureSwapStartSize = 32;
	static const int textureSwapArraySize = 10;
	static LPDIRECT3DTEXTURE9 textureSwapArray[textureSwapArraySize];
	static LPDIRECT3DTEXTURE9 texture;
	static LPDIRECT3DTEXTURE9 textureVision;
	static LPDIRECT3DTEXTURE9 textureShadowMap;
	static LPDIRECT3DTEXTURE9 textureShadowMapDirectional;
	static LPDIRECT3DTEXTURE9 textureNormalMap;
	static LPDIRECT3DTEXTURE9 textureSpecularMap;
	static LPDIRECT3DTEXTURE9 textureEmissive;
	static LPDIRECT3DTEXTURE9 textureFinal;
	static LPDIRECT3DPIXELSHADER9 deferredLightShader;
	static LPD3DXCONSTANTTABLE shadowLightConstantTable;
	static LPDIRECT3DPIXELSHADER9 visionShader;
	static LPD3DXCONSTANTTABLE visionConstantTable;
	static LPDIRECT3DPIXELSHADER9 blurShader;
	static LPD3DXCONSTANTTABLE blurConstantTable;
	static LPDIRECT3DPIXELSHADER9 directionalLightShader;
	static LPD3DXCONSTANTTABLE directionalLightConstantTable;
    static FrankRender::RenderPrimitive primitiveLightMask;
};
