////////////////////////////////////////////////////////////////////////////////////////
/*
	Terrain
	Copyright 2013 Frank Force - http://www.frankforce.com
	
	- static terrain to form the world
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../objects/gameObject.h"
#include "../terrain/terrainTile.h"
#include "../terrain/terrainSurface.h"

////////////////////////////////////////////////////////////////////////////////////////
// terrain defines

class TerrainLayerRender;

class TerrainPatch : public GameObject
{
public:

	TerrainPatch(const Vector2& pos);
	~TerrainPatch();

	TerrainTile& GetTileLocal(int x, int y, int layer = 0) const;
	static bool IsTileIndexValid(int x, int y, int layer = 0);
	bool GetTileLocalIsSolid(int x, int y) const;
	Vector2 GetTilePos(int x, int y) const { return GetPosWorld() + TerrainTile::GetSize() * Vector2((float)x, (float)y); }
	void RebuildPhysics() { needsPhysicsRebuild = true; }
	Vector2 GetCenter() const;
	Box2AABB GetAABB() const;

	GameObjectStub* GetStub(GameObjectHandle handle);
	bool RemoveStub(GameObjectHandle handle);
	GameObjectStub* GetStub(const Vector2& pos);
	list<GameObjectStub>& GetStubs() { return objectStubs; }
	
	void Clear();
	void ClearTileData();
	void ClearTileData(int layer);
	void ClearObjectStubs();

	TerrainTile* GetTile(const Vector2& testPos, int layer = 0);
	TerrainTile* GetTile(const Vector2& testPos, int& x, int& y, int layer = 0);
	TerrainTile* GetTile(int x, int y, int layer = 0);

	bool IsTerrain() const override { return true; }

public: // internal engine functions

	bool HasActivePhysics() const { return activePhysics; }
	bool HasActiveObjects() const { return activeObjects; }

	void Deactivate()
	{
		SetActivePhysics(false);
		SetActiveObjects(false);
	}

	void SetActivePhysics(bool _activePhysics);
	void SetActiveObjects(bool _activeObjects, bool windowMoved = false);

	void CreatePolyPhysicsBody();
	void CreateEdgePhysicsBody();

	GameObjectStub* AddStub(const GameObjectStub& stub) 
	{ 
		objectStubs.push_back(stub); 
		return &objectStubs.back();
	}

	bool RemoveStub(GameObjectStub* stub) 
	{ 
		for (list<GameObjectStub>::iterator it = objectStubs.begin(); it != objectStubs.end(); ++it) 
		{       
			if (stub == &(*it))
			{
				objectStubs.erase(it);
				return true;
			}
		}

		return false;
	}
	
	// do not use block allocator for terrain patches
	void* operator new (size_t size)	{  return (BYTE*)_aligned_malloc(sizeof(TerrainPatch), 16); }
	void operator delete (void* block)	{ _aligned_free(block); }
	
	static b2PolygonShape BuiltTileShape(BYTE edgeByte, const Vector2& offset);

public: // data members

	TerrainTile *tiles;
	list<GameObjectStub> objectStubs;
	
	bool activePhysics;
	bool activeObjects;
	bool needsPhysicsRebuild;
};

class Terrain : public GameObject
{
public:

	friend class TileEditor;
	friend class ObjectEditor;

	Terrain(const Vector2& pos);
	~Terrain();
	
	void Deactivate();

	TerrainLayerRender* GetLayerRender(int i) { ASSERT(i >= 0 && i < patchLayers); return layerRenderArray[i]; }
	
	XForm2 GetXFormCenter() const { return XForm2(Vector2(0.5f*Vector2(fullSize*patchSize)*TerrainTile::GetSize())) * GetXFWorld(); }
	
	static bool IsPatchIndexValid(const IntVector2& p) { return (p.x>=0 && p.x<fullSize.x && p.y>=0 && p.y<fullSize.y); }
	IntVector2 GetPatchIndex(const Vector2& pos) const
	{
		const Vector2 index = GetPatchIndexFloat(pos);
		return IntVector2((int)floorf(index.x), (int)floorf(index.y));
	}
	Vector2 GetPatchIndexFloat(const Vector2& pos) const { return (pos - GetPosWorld()) / (patchSize*TerrainTile::GetSize()); }
	Vector2 GetPatchPos(const IntVector2& pos) const { return Vector2(pos*patchSize)*TerrainTile::GetSize() + GetPosWorld(); }
	
	GameObjectStub* FindNextStubByType(GameObjectType type, GameObjectStub* lastStub = NULL);
	GameObjectStub* GetStub(GameObjectHandle handle, TerrainPatch** returnPatch = NULL);
	GameObjectStub* GetStub(const Vector2& pos, TerrainPatch** returnPatch = NULL);
	bool RemoveStub(GameObjectHandle handle, TerrainPatch* patch = NULL);

	TerrainPatch* GetPatch(const Vector2& pos) const
	{
		const IntVector2 index = GetPatchIndex(pos);
		return (IsPatchIndexValid(index) ? GetPatch(index.x, index.y) :  NULL);
	}

	TerrainPatch* GetPatch(int x, int y) const
	{
		if (IsPatchIndexValid(IntVector2(x,y)))
			return patches[x + fullSize.x * y];
		else
			return NULL;
	}

	Box2AABB GetStreamWindow() const { return streamWindow; }
	void UpdateActiveWindow();
	void UpdatePost();
	
	IntVector2 GetTileIndex(const Vector2& pos) const;
	Vector2 GetTilePos(int x, int y) const { return GetPosWorld() + TerrainTile::GetSize() * Vector2((float)x, (float)y); }
	Vector2 GetTileCenter(const Vector2& pos) const;
	Vector2 GetTileCenter(int x, int y) const;
	TerrainTile* GetTile(const Vector2& pos, int& x, int& y, int layer = 0) const;
	TerrainTile* GetTile(const Vector2& pos, int layer = 0) const;
	TerrainTile* GetTile(int x, int y, int layer = 0) const;
	int GetSurfaceSide(const Vector2& pos, int layer = 0) const;
	BYTE GetSurfaceIndex(const Vector2& pos, int layer = 0) const;
	int SetSurfaceIndex(const Vector2& pos, BYTE surface, int layer = 0);
	
	bool Deform(const Vector2& pos, float radius, GameMaterialIndex gmi, float randomness = 0.1f);
	bool Deform(const Vector2& pos, float radius, const list<GameMaterialIndex>& gmiList, float randomness = 0.1f);
	GameMaterialIndex DeformTile(const Vector2& startPos, const Vector2& direction, const GameObject* ignoreObject = NULL, GameMaterialIndex gmi = GMI_Invalid, bool clear = false);

	// quick test if there is a given area is totally clear or not
	bool IsClear(const Vector2& pos, int layer = 0)
	{
		TerrainTile* tile = GetTile(pos);
		return (!tile || tile->IsClear());
	}

	bool Save(const WCHAR* filename);
	bool Load(const WCHAR* filename);
	void Clear();

	TerrainTile* GetConnectedTileA(int x, int y, int &x2, int &y2, int layer = 0);
	TerrainTile* GetConnectedTileB(int x, int y, int &x2, int &y2, int layer = 0);
	TerrainTile* GetConnectedTileA(int x, int y) { int x2, y2; return GetConnectedTileA(x, y, x2, y2); }
	TerrainTile* GetConnectedTileB(int x, int y) { int x2, y2; return GetConnectedTileB(x, y, x2, y2); }
	
	Vector2 GetPlayerEditorStartPos() const { return playerEditorStartPos; }
	void SetPlayerEditorStartPos(const Vector2& pos) { playerEditorStartPos = pos; }

	// what handle to use for new objects
	GameObjectHandle GetStartHandle() const { return startHandle; }
	void ResetStartHandle(GameObjectHandle handle);
	
	struct TileSetInfo
	{
		wstring name;
		TextureID texture = Texture_Invalid;
		bool enableCollision = true;
		bool hasShadowOverride = false;
		Color shadowOverrideColor = Color::Black();
	};
	
	void GiveStubNewHandle(GameObjectStub& stub, bool fromEditor = false);
	void CheckForErrors();
	void CleanUpTiles();

	static TileSetInfo& GetTileSetInfo(int i) { ASSERT(i >= 0 && i < TerrainTile::GetMaxTileSetCount()); return tileSets[i]; }
	static void SetTileSetInfo(int i, wstring name, TextureID texture, bool collision = true, bool hasShadowOverride = false, const Color& shadowOverrideColor = Color::Black())
	{
		tileSetCount = Max(tileSetCount, i+1);
		GetTileSetInfo(i).name = name; 
		GetTileSetInfo(i).texture = texture;
		GetTileSetInfo(i).enableCollision = collision; 
		GetTileSetInfo(i).hasShadowOverride = hasShadowOverride; 
		GetTileSetInfo(i).shadowOverrideColor = shadowOverrideColor; 
	}
	static const wstring& GetTileSetName(int i)				{ return GetTileSetInfo(i).name; }
	static TextureID GetTileSetTexture(int i)				{ return GetTileSetInfo(i).texture; }
	static bool GetTileSetHasCollision(int i)				{ return GetTileSetInfo(i).enableCollision; }
	
	bool ShouldStreamOut() const override		{ return false; }
	bool DestroyOnWorldReset() const override	{ return false; }
	bool IsTerrain() const override				{ return true; }

public: // settings

	static int dataVersion;					// used to prevent old version from getting loaded
	static IntVector2 fullSize;				// how many patches per terrain
	static int patchSize;					// how many tiles per patch
	static int patchLayers;					// how many layers patch
	static int physicsLayer;				// the layer to create physics for
	static int windowSize;					// size of the terrain stream window
	static int renderWindowSize;			// size of the terrain render window
	static Color editorTileEdgeColor1;		// color used for lines around tiles when editing
	static Color editorTileEdgeColor2;		// color used for lines around tiles when editing
	static Vector2 gravity;					// acceleartion due to gravity
	static WCHAR terrainFilename[256];		// filename used for save/load terrain operations
	static float restitution;				// restitution for terrain physics
	static float friction;					// friction for terrain physics
	static bool usePolyPhysics;				// should polygons be used instead of edge shapes for collision
	static bool combineTileShapes;			// optimization to combine physics shapes for tiles
	static bool enableStreaming;			// streaming of objects and physics for the window around the player
	static bool streamDebug;				// show streaming debug overlay
	static int maxProxies;					// limit on how many terrain proxies can be made
	static bool isCircularPlanet;			// should terrain be treated like a circular planet?
	static bool terrainAlwaysDestructible;	// allow any kind of terrain to be destroyed
	static bool deformUpdateMap;			// should terrain deform operations update the minimap

	// tile sheets
	static int tileSetCount;						// how many tile sets there are
	static const int maxTileSets = 256;				// how many tile sets max
	static TileSetInfo tileSets[maxTileSets];		// map of which tile set references which texture

private:
	
	void UpdateStreaming();
	bool LoadFromResource(const WCHAR* filename);
	
	IntVector2 streamWindowPatch;
	IntVector2 streamWindowPatchLast;
	int streamWindowSizeLast;
	Box2AABB streamWindow;
	Vector2 playerEditorStartPos;
	TerrainPatch **patches;
	GameObjectHandle startHandle;
	TerrainLayerRender** layerRenderArray;
	bool wasReset = false;

	friend class TerrainRender;
};

inline Box2AABB TerrainPatch::GetAABB() const 
{ 
	return Box2AABB(GetTilePos(0,0), GetTilePos(Terrain::patchSize,Terrain::patchSize)); 
}

inline Vector2 TerrainPatch::GetCenter() const
{
	const Vector2 minPatchPos = GetTilePos(0, 0);
	const Vector2 maxPatchPos = GetTilePos(Terrain::patchSize, Terrain::patchSize);
	return minPatchPos + 0.5f * (maxPatchPos - minPatchPos);
}

inline bool TerrainPatch::IsTileIndexValid(int x, int y, int l)
{ 
	return (x >= 0 && x < Terrain::patchSize && y >= 0 && y < Terrain::patchSize && l >= 0 && l < Terrain::patchLayers); 
}

inline TerrainTile& TerrainPatch::GetTileLocal(int x, int y, int layer) const
{
	ASSERT(IsTileIndexValid(x,y,layer));
	return tiles[Terrain::patchSize*Terrain::patchSize*layer + Terrain::patchSize * x + y];
}
