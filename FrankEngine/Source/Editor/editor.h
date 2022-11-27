////////////////////////////////////////////////////////////////////////////////////////
/*
	Editor
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../terrain/terrain.h"
#include "../objects/gameObjectBuilder.h"
#include "../editor/tileEditor.h"
#include "../editor/objectEditor.h"

// this is the global object editor
extern class Editor g_editor;

class Editor
{
public:

	Editor();

	void Update();
	void Render();
	void RenderGrid();

	void ResetEditor();
	
	bool IsObjectEdit() const				{ return !isTileEdit; }
	bool IsTileEdit() const					{ return isTileEdit; } 
	void SetIsTileEdit(bool _isTileEdit)	{ isTileEdit = _isTileEdit; } 
	bool IsRectangleSelecting() const		{ return isRectangleSelecting; }
	bool IsMultiLayerSelecting() const		{ return isMultiLayerSelecting; }
	bool IsOnSelected(const Vector2& pos) const;
	bool HasSelection() const;
	void ClearSelection();
	void RotateSelection(bool rotateCW = true);
	void MirrorSelection();
	void ClearClipboard();
	void SetEditLayer(int editLayer);
	void ChangeDrawType(bool direction);
	bool GetIsInQuickPick() const { return (isTileEdit && tileEditor.GetIsInQuickPick()) || (!isTileEdit && objectEditor.GetIsInQuickPick()); }

	TileEditor& GetTileEditor()			{ return tileEditor; }
	ObjectEditor& GetObjectEditor()		{ return objectEditor; }

	void Undo();
	void Redo();
	void SaveState(bool onlyIfStateChanged=false);
	void SetStateChanged()					{ stateChanged = true; }
	void GetTerrainRenderWindow(int xPos, int yPos, int& xStart, int& xEnd, int& yStart, int& yEnd) const;

	static void ClearTempFolder();

	static int terrainRenderWindowSize;
	static bool showGrid;
	static Color gridColorTile;
	static Color gridColorObject;
	static Color gridPatchColor;
	static float stubCullRange;

private:

	WCHAR* GetUndoFilename(int i);
	void RenderPatch(const TerrainPatch& patch) const;
	void RenderStubs(const TerrainPatch& patch) const;

	TileEditor tileEditor;
	ObjectEditor objectEditor;

	Box2AABB rectangleSelectBox;
	bool isTileEdit;
	bool isRectangleSelecting;
	bool isMultiLayerSelecting;
	bool stateChanged;
	
	int saveStateStart;
	int saveStateEnd;
	int saveStatePosition;
	static const int maxSaveStates = 100;
};
