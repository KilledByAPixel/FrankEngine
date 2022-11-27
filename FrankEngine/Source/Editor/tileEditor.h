////////////////////////////////////////////////////////////////////////////////////////
/*
	Tile Editor
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../terrain/terrainSurface.h"
#include "../terrain/terrain.h"

class TileEditor
{
public:

	TileEditor();
	~TileEditor();

	void Update(bool isActive);
	void Render();
	void RenderSelectionBox();
	
	bool HasSelection() const					{ return selectedTiles != NULL; }
	int GetSelectedLayerCount() const			{ return selectedLayerCount; }
	bool IsOnSelected(const Vector2& pos) const;
	int GetLayer() const						{ return terrainLayer; }
	void SetLayer(int layer)					{ terrainLayer = layer; }
	void Select(Box2AABB& box, bool allLayers);
	void RotateSelection(bool rotateCW);
	void MirrorSelection();
	void ClearSelection(bool anchor = true);
	void ClearClipboard();
	bool IsClipboardEmpty() const				{ return copiedTiles == NULL; }
	Vector2 GetPasteOffset(const Vector2& pos);
	Vector2 GetRoundingOffset();
	Box2AABB GetSelectionBox() const;

	TerrainTile& GetSelectedTile(int x, int y, int layer)
	{
		ASSERT(selectedTiles && selectedTilesSize.x >= 0 && selectedTilesSize.y >= 0);
		ASSERT(x >= 0 && x < selectedTilesSize.x && y >= 0 && y < selectedTilesSize.y && layer >= 0 && layer < selectedLayerCount);
		return selectedTiles[selectedTilesSize.x*selectedTilesSize.y*layer + selectedTilesSize.y*x + y];
	}
	
	int GetDrawRotation() const { return drawRotation; }
	bool GetDrawMirror() const { return drawMirror; }
	int GetDisplayDrawSurfaceIndex() const;
	const GameSurfaceInfo& GetDisplayDrawSurfaceInfo() const  { return GameSurfaceInfo::Get(GetDisplayDrawSurfaceIndex()); }
	int GetDrawSurfaceIndex() const { return drawSurface; }
	const GameSurfaceInfo& GetDrawSurfaceInfo() const  { return GameSurfaceInfo::Get(GetDrawSurfaceIndex()); }
	int GetDrawTileSet() const { return drawTileSet; }
	void SetDrawTileSet(int layer, int tileSet) { drawTileSet = tileSet; }
	
	void Cut();
	void Copy();
	void Paste(const Vector2& offset);
	void ChangeDrawType(bool direction);
	void ChangeTileSet(bool direction);
	bool GetIsInQuickPick() const { return isInQuickPick; }
	void RemoveSelected();

	static bool enableCrossLayerPaste;
	static bool blockEdit;
	static bool pasteToCorner;

	void Resurface(const Vector2& posA, const Vector2& posB, BYTE surfaceData, BYTE tileRotation, bool tileMirror);
	void FloodFill(const Vector2& pos, BYTE surfaceData);
	void FloodErase(const Vector2& pos);

private:
	
	void Resurface(TerrainPatch& patch, const Vector2& posA, const Vector2& posB, BYTE surfaceData, BYTE tileRotation, bool tileMirror);

	void FloodFill(TerrainPatch& patch, const Vector2& testPos, BYTE surfaceData);
	void FloodFillDirection(TerrainPatch& patch, int x, int y, int direction, BYTE surfaceData, int surfaceSide, BYTE startSurfaceData);
	void FloodFillInternal(TerrainPatch& patch, int x, int y, BYTE surfaceData, int surfaceSide, BYTE startSurfaceData);
	
	Vector2 GetSnapLinePoint(const Vector2& pos, bool snapToGrid = false);

	int drawSurface = 0;
	int drawRotation = 0;
	bool drawMirror = false;
	int drawTileSet = 0;
	Vector2 mousePosLineStart = Vector2(0);
	Vector2 worldCenter = Vector2(0);

	int terrainLayer = 0;
	IntVector2 selectedTilesSize = IntVector2(0);
	int selectedLayerCount = 0;
	int firstSelectedLayer = 0;
	TerrainTile *selectedTiles = NULL;
	Vector2 selectedTilesPos = Vector2(0);

	TerrainTile *copiedTiles = NULL;
	IntVector2 copiedTileSize= IntVector2(0);
	Vector2 copiedTilesPos = Vector2(0);
	int copiedLayerCount = 0;
	bool isInQuickPick = false;
};
