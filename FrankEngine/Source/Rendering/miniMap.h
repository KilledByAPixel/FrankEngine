////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Mini Map
	Copyright 2018 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class TerrainPatch;
struct TerrainTile;
struct GameSurfaceInfo;

class MiniMap
{
public:

	virtual void Init();
	virtual void Update();
	virtual void SetupRender();
	virtual void Render();
	virtual void RedrawPatch(const TerrainPatch& patch);
	virtual void SetMapData(const Vector2& pos) { SetMapDirty(); }
	virtual int GetMapData(const IntVector2& pos) { return 1; }
	virtual TextureID GetMinimapTexture(int layer) const;
	virtual void RenderWorldSpace(const XForm2& xf, const Vector2& size, const Color& color, const XForm2& xfCenter, float zoom, bool showFull, bool isStub = false);
	
	virtual void InitDeviceObjects();
	virtual void DestroyDeviceObjects();
	
	void SetMapDirty() { mapDataDirty = true; }
	void RenderTileToTexture(const TerrainTile& tile, const Vector2& tileOffset, int layer);
	
	struct MapRenderBlock
	{
		MapRenderBlock(LPDIRECT3DTEXTURE9 texture, bool needsClear = true);
		~MapRenderBlock();

		LPDIRECT3DSURFACE9 backBuffer = NULL;
		LPDIRECT3DSURFACE9 renderSurface = NULL;
	};
	
	static bool mapEnable;
	static bool mapShowFull;
	static float mapSizeSmall;
	static float mapSizeLarge;
	static float mapZoomScale;
	static float mapGapSize;
	static float mapAlpha;

	LPDIRECT3DTEXTURE9 GetMiniMapTexture() const { return mapFullTexture; }
	LPDIRECT3DTEXTURE9 GetMiniMapHiddenTexture() const { return mapHiddenTexture; }

protected:
	
	virtual void SetRenderSettings(bool isLarge);
	virtual void RenderMiniMap();
	virtual void RenderLargeMap();
	
	void RenderMapQuad(const XForm2& xf, const Vector2& size, const Color& color, LPDIRECT3DTEXTURE9 texture, const Color& outlineColor = Color::Black(0)) const;
	void RenderMapQuad(const XForm2& xf, const Vector2& size, const Color& color, const TextureID texture, const Color& outlineColor = Color::Black(0)) const;
	void RenderMapIcon(const XForm2& xf, const Vector2& size, const Color& color, const TextureID texture, const TextureID arrowTexture = Texture_Invalid) const;
	void RenderMapText(const XForm2& xf, float size, const Color& color, const char* text, FontFlags flags = FontFlags::FontFlag_Center) const;
	void RenderMapText(const XForm2& xf, float size, const Color& color, const WCHAR* text, FontFlags flags = FontFlags::FontFlag_Center) const;
	
	XForm2 GetMapSpaceXF(const XForm2& xf) const;

	virtual void RenderMiniMapBackground() const;
	virtual void RenderMiniMapForeground() const;
	virtual TextureID GetCircleTexture() const { return Texture_Circle; }
	virtual TextureID GetDotTexture() const { return Texture_Dot; }
	virtual TextureID GetMaskTexture() const { return GetCircleTexture(); }
	virtual bool IsLargeMapVisible() const { return g_gameControlBase->IsPaused(); };
	virtual bool IsMapSquare() const { return false; };
	
	void RenderMapHiddenToTexture();
	void RenderToTexture();
	
	void RenderTilesToTexture();
	void RenderObjectsToTexture();
	virtual void RenderTilesToTexture(const TerrainPatch& patch);
	virtual void RenderObjectsToTexture(const TerrainPatch& patch);
	void RenderTileToTexture(const TerrainTile& tile, const Vector2& tileOffset, const TextureID tileTexture);
	void RenderTileToTexture(const TerrainTile& tile, const Vector2& tileOffset, const GameSurfaceInfo& surfaceInfo, const TextureID tileTexture);

	int mapRevealWindow = 1;
	bool mapDataDirty = false;
	LPDIRECT3DTEXTURE9 mapHiddenTexture = NULL;
	LPDIRECT3DTEXTURE9 mapFullTexture = NULL;

	struct MapRenderSettings
	{
		bool isLarge = false;
		float alpha = 1;
		float worldZoom = 0;
		Vector2 worldCenter = Vector2(0);
		Vector2 screenSize = Vector2(0);
		XForm2 screenXF = XForm2(0);
	};

	MapRenderSettings mapRenderSettings;
};
