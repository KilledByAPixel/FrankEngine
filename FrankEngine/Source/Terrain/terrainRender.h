////////////////////////////////////////////////////////////////////////////////////////
/*
	Terrain Render
	Copyright 2013 Frank Force - http://www.frankforce.com
	
	- renders tile based terrain
	- caches verts to optimize rendering
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../terrain/terrainTile.h"

extern class TerrainRender g_terrainRender;

class TerrainRender
{
public:

	TerrainRender();
	~TerrainRender();

	void Update();
	void ClearCache();
	void UpdateCache();
	void Render(const Terrain& terrain, const Vector2& pos, int layer = 0, float alpha = 1);
	void RenderTiles(TerrainTile* tiles, const Vector2& pos, int width, int height, int layers, int firstLayer, float alpha = 1);

	void InitDeviceObjects();
	void DestroyDeviceObjects();

	typedef void (*RenderToTileCallback)();
	void RenderToTexture(const Terrain& terrain, RenderToTileCallback preCallback = NULL, RenderToTileCallback postCallback = NULL);
	LPDIRECT3DTEXTURE9 GetRenderTexture() { return renderTexture; }
	void RefereshCached(TerrainPatch& patch);
	bool IsPatchCached(TerrainPatch& patch);

	void RenderTile(const Vector2& position, const BYTE edgeIndex, const BYTE surfaceID, const BYTE tileSet, const GameSurfaceInfo& surfaceInfo, const Color& color, const BYTE tileRotation, bool tileMirror);
	void RenderSlow(const TerrainPatch& patch, int layer = 0, float alpha = 1, const Vector2& offset = Vector2(0), bool cameraTest = true);

	struct CachedTileBuffer
	{
		FrankRender::RenderPrimitive rp;
		
		static const int maxGroups = 512;
		struct TileRenderGroup
		{
			UINT startVertex;
			UINT vertexCount;
			BYTE surfaceID;
			BYTE tileSet;
		};
		TileRenderGroup renderGroups[maxGroups];
		int groupCount;
		const TerrainPatch* patch;
	};
	
	int renderedPrimitiveCount;
	int renderedBatchCount;
	
	static bool enableRender;
	static bool showEdgePoints;
	static bool foregroundLayerOcculsion;
	static bool limitCacheUpdate;
	static bool useAlpha;
	static bool cacheEnable;
	static bool enableDiffuseLighting;
	static float textureWrapScale;
	static float editorBackgroundAlpha;
	static float editorForegroundAlpha;

private:

	struct TERRAIN_VERTEX
	{
		Vector3 position;
		Vector2 textureCoords;
	};

	void BuildTriStrip(TERRAIN_VERTEX* vertices, FrankRender::RenderPrimitive& rp, int tileIndex);
	void CacheTilesPrimitives(int tileBufferIndex, const TerrainPatch& patch, int layer);
	void UncacheTilesPrimitives(int tileBufferIndex, int layer);
	void RenderCached(int tileBufferIndex, int layer, float alpha);

	FrankRender::RenderPrimitive tilePrimitives[TERRAIN_UNIQUE_TILE_COUNT];
	CachedTileBuffer* tileBuffers;
	int tileBufferCount;
	bool cacheNeedsFullRefresh;
	LPDIRECT3DTEXTURE9 renderTexture;

	void SetupVert(TERRAIN_VERTEX* vertex, const Vector2& worldPos, const Vector2& localPos, const GameSurfaceInfo& surfaceInfo, const BYTE surfaceID, const BYTE tileRotation, bool tileMirror);
	void SetupVertUnwrapped(TERRAIN_VERTEX* vertex, const Vector2& pos);
};

// terrain layer, just a way to render out a layer easily
class TerrainLayerRender : public GameObject
{
public:

	TerrainLayerRender(const Vector2& pos, int layer = 0, int renderGroup = -500);
	void Render() override;

	void SetLightShadows(bool _lightShadows)				{ lightShadows = _lightShadows; }
	bool GetLightShadows()									{ return lightShadows; }

	void SetDirectionalShadows(bool _directionalShadows)	{ directionalShadows = _directionalShadows; }
	bool GetDirectionalShadows()							{ return directionalShadows; }

	void SetEmissiveLighting(bool _emissiveLighting)		{ emissiveLighting = _emissiveLighting; }
	bool GetEmissiveLighting()								{ return emissiveLighting; }

	bool ShouldStreamOut() const override					{ return false; }
	bool DestroyOnWorldReset() const override				{ return false; }

protected:

	int layer;
	bool lightShadows;
	bool directionalShadows;
	bool emissiveLighting;
};
