////////////////////////////////////////////////////////////////////////////////////////
/*
	Tile Editor
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../terrain/terrain.h"
#include "../editor/tileEditor.h"

bool TileEditor::enableCrossLayerPaste = true;
ConsoleCommand(TileEditor::enableCrossLayerPaste, enableCrossLayerPaste);

bool TileEditor::blockEdit = false;
ConsoleCommand(TileEditor::blockEdit, blockEdit)

bool TileEditor::pasteToCorner = false;
ConsoleCommand(TileEditor::pasteToCorner, pasteToCorner)

TileEditor::TileEditor()
{
}

TileEditor::~TileEditor()
{
	SAFE_DELETE_ARRAY(selectedTiles);
	SAFE_DELETE_ARRAY(copiedTiles);
}

void TileEditor::Update(bool isActive)
{
	ASSERT(g_terrain);

	const Vector2 mousePos = g_input->GetMousePosWorldSpace();
	const Vector2 mousePosLast = g_input->GetLastMousePosWorldSpace();

	if (isActive)
	{
		if (!g_input->IsDown(GB_Control))
		{
			// allow moving surface around quick pick sheet
			if (g_input->IsDownUI(GB_Editor_Left))
				ChangeDrawType(false);
			else if (g_input->IsDownUI(GB_Editor_Right))
				ChangeDrawType(true);
			if (g_input->IsDownUI(GB_Editor_Up))
			{
				const int rows = g_editorGui.GetTileQuickPickRows();
				drawSurface -= EditorGui::quickPickTileColumns;
				if (drawSurface < 0)
					drawSurface += rows*EditorGui::quickPickTileColumns;
			}
			else if (g_input->IsDownUI(GB_Editor_Down))
			{
				const int rows = g_editorGui.GetTileQuickPickRows();
				drawSurface += EditorGui::quickPickTileColumns;
				if (drawSurface >= rows * EditorGui::quickPickTileColumns)
					drawSurface -= rows*EditorGui::quickPickTileColumns;
			}
		}
	
		if (g_input->IsDownUI(GB_Editor_TileSetUp))
			ChangeTileSet(true);
		else if (g_input->IsDownUI(GB_Editor_TileSetDown))
			ChangeTileSet(false);

		if (g_editor.HasSelection())
		{
			if (!g_input->IsDown(GB_Control) && g_input->WasJustPushed(GB_Editor_Remove))
			{
				g_editor.GetObjectEditor().RemoveSelected();
				RemoveSelected();
				g_editor.SaveState();
			}
		}
		else
		{
			if (!g_input->IsDown(GB_Control) && g_input->WasJustPushed(GB_Editor_Add))
			{
				FloodFill( g_input->GetMousePosWorldSpace(), drawSurface );
				g_editor.SaveState();
			}
			else if (!g_input->IsDown(GB_Control) && g_input->WasJustPushed(GB_Editor_Remove))
			{
				FloodErase( g_input->GetMousePosWorldSpace() );
				g_editor.SaveState();
			}
		}
		
		if ((!g_input->IsDown(GB_MouseLeft) || g_input->IsDown(GB_Editor_MouseDrag)) || g_input->IsDown(GB_MouseRight))
			isInQuickPick = false;

		if (!g_editor.IsRectangleSelecting() && g_input->IsDown(GB_MouseLeft) && !g_input->IsDown(GB_MouseRight) && !g_input->IsDown(GB_Editor_MouseDrag) && !g_input->IsDown(GB_Shift))
		{
			if (g_input->WasJustPushed(GB_MouseLeft) || isInQuickPick)
			{
				// quick pick
				const int quickPickSurface = g_editorGui.GetTileQuickPickId();
				if (quickPickSurface >= 0)
				{
					drawSurface = quickPickSurface;
					isInQuickPick = true;
				}
			}

			// eyedropper tool
			if (!isInQuickPick && g_input->IsDown(GB_Control) && !g_input->IsDown(GB_Alt))
			{
				if (IsOnSelected(mousePos))
				{
					// get draw surface from selected tiles
					IntVector2 tilePos = (mousePos - selectedTilesPos) / TerrainTile::GetSize();
					tilePos.x = Cap(tilePos.x, 0, selectedTilesSize.x-1);
					tilePos.y = Cap(tilePos.y, 0, selectedTilesSize.y-1);
					
					int layer = (selectedLayerCount - 1 >= terrainLayer) ? terrainLayer : 0;
					TerrainTile tile = GetSelectedTile(tilePos.x, tilePos.y, layer);
					drawSurface = tile.GetSurfaceData(0);
					drawRotation = tile.GetRotation();
					drawMirror = tile.GetMirror();
					drawTileSet = tile.GetTileSet();
				}
				else
				{
					TerrainTile* tile = g_terrain->GetTile(mousePos, terrainLayer);
					if (tile)
					{
						drawSurface = g_terrain->GetSurfaceIndex(mousePos, terrainLayer);
						drawRotation = tile->GetRotation();
						drawMirror = tile->GetMirror();
						drawTileSet = tile->GetTileSet();
					}
				}
			}
		}

		if (!g_editor.HasSelection() && !g_editor.IsRectangleSelecting() && !isInQuickPick)
		{
			// drawing tools
			if (g_input->WasJustPushed(GB_Editor_RotateCW))
			{
				drawRotation = (drawRotation + 3) % 4;
			}
			else if (g_input->WasJustPushed(GB_Editor_RotateCCW))
			{
				drawRotation = (drawRotation + 1) % 4;
			}
			else if (g_input->WasJustPushed(GB_Editor_Mirror))
			{
				drawMirror = !drawMirror;
			}
			if 
			(
				(g_input->IsDown(GB_MouseRight) || g_input->IsDown(GB_Shift) && (blockEdit || g_input->IsDown(GB_Alt))) 
				&& g_input->IsDown(GB_MouseLeft) && !g_input->IsDown(GB_Editor_MouseDrag)
			)
			{
				// erase tool
				TerrainTile* tile = g_terrain->GetTile(mousePos, terrainLayer);
				if (tile)
					tile->MakeClear();
				g_editor.SetStateChanged();
			}
			else if 
			(
				(blockEdit || g_input->IsDown(GB_Alt)) 
				&& g_input->IsDown(GB_MouseLeft) && !g_input->IsDown(GB_Editor_MouseDrag)
			)
			{
				// block draw tool
				TerrainTile* tile = g_terrain->GetTile(mousePos, terrainLayer);
				if (tile)
				{
					g_terrain->SetSurfaceIndex(mousePos, drawSurface, terrainLayer);
					tile->SetTileSet(drawTileSet);
					tile->SetRotation(drawRotation);
					tile->SetMirror(drawMirror);
				}
				g_editor.SetStateChanged();
			}
			else if (!blockEdit && g_input->IsDown(GB_Shift) && !g_input->IsDown(GB_Editor_MouseDrag))
			{
				// line drawing
				if (g_input->WasJustPushed(GB_Shift))
					mousePosLineStart = mousePos;
				if (g_input->WasJustPushed(GB_MouseLeft))
				{
					Resurface(mousePosLineStart, mousePos, drawSurface, drawRotation, drawMirror);
					mousePosLineStart = mousePos;
				}

				//Vector2 p1 = GetSnapLinePoint(mousePosLineStart, true);
				//Vector2 p2 = GetSnapLinePoint(g_input->GetMousePosWorldSpace());
				//
				//Vector2 delta = p2 - p1;
				//float distance = delta.Length();

				//float angle = delta.GetAngle();
				//angle = 16*floor(angle / 16);
				//
				//p2 = p1 + Vector2::BuildFromAngle(angle) * delta.Length();

				g_debugRender.RenderLine(mousePosLineStart, mousePos, Color::Red(), 0);
			}
			else if (g_input->IsDown(GB_MouseLeft) && !g_input->IsDown(GB_Alt) && !g_input->IsDown(GB_Control) && !g_input->IsDown(GB_Editor_MouseDrag))
			{
				// free form drawing
				Resurface(mousePosLast, mousePos, drawSurface, drawRotation, drawMirror);
			}

			TerrainTile *tile = g_terrain->GetTile(mousePos, terrainLayer);
			if (tile && tile->GetEdgeData() != 0)
			{
				// show edge points for active tile
				int x=0, y=0;
				g_terrain->GetTile(mousePos, x, y, terrainLayer);
				const Vector2 tilePos = g_terrain->GetTilePos(x, y);
				const Line2 edgeLine = tile->GetEdgeLine();
				g_debugRender.RenderPoint(tilePos + edgeLine.p1, Color::Red(0.5f));
				g_debugRender.RenderPoint(tilePos + edgeLine.p2, Color::Blue(0.5f));
			}
		}
	}

	if (selectedTiles)
	{
		bool isMoving = false;
		if (!g_editor.GetIsInQuickPick() && !g_input->IsDown(GB_Editor_MouseDrag) && (g_input->IsDown(GB_MouseLeft) || g_input->IsDown(GB_MouseRight)) && !g_input->IsDown(GB_Control) && !g_input->IsDown(GB_Shift))
		{
			// move selected
			isMoving = true;
			const Vector2 mouseDelta = g_input->GetMouseDeltaWorldSpace();
			selectedTilesPos += mouseDelta;
		}

		if (!isMoving)
		{
			// snap to grid when not moving
			Vector2 pos = (selectedTilesPos - g_terrain->GetPosWorld()) / TerrainTile::GetSize();
			pos = Vector2(floor(pos.x + 0.5f), floor(pos.y + 0.5f));
			const Vector2 newSelectedTilesPos = Vector2(pos) * TerrainTile::GetSize() + g_terrain->GetPosWorld();
			g_editor.GetObjectEditor().MoveSelectedStubs(newSelectedTilesPos - selectedTilesPos);
			selectedTilesPos = newSelectedTilesPos;
		}
	}
}

Vector2 TileEditor::GetSnapLinePoint(const Vector2& pos, bool snapToGrid)
{
	Vector2 roundedPos = pos;
	roundedPos -= g_terrain->GetPosWorld();
	roundedPos = 4*roundedPos / TerrainTile::GetSize();
	roundedPos = roundedPos.Round();
	roundedPos *= TerrainTile::GetSize()/4;
	roundedPos += g_terrain->GetPosWorld();

	if (!snapToGrid)
		return roundedPos;
	
	Vector2 p1 = roundedPos / TerrainTile::GetSize();

	float xGrid = p1.x - floor(p1.x);
	xGrid = Max(xGrid, 1 - xGrid);
	float yGrid = p1.y - floor(p1.y);
	yGrid = Max(yGrid, 1 - yGrid);

	if (xGrid > yGrid)
	{
		if (p1.x - floor(p1.x) < 1 - (p1.x - floor(p1.x)))
			p1.x = floor(p1.x);
		else
			p1.x = 1 + floor(p1.x);
	}
	else
	{
		if (p1.y - floor(p1.y) < 1 - (p1.y - floor(p1.y)))
			p1.y = floor(p1.y);
		else
			p1.y = 1 + floor(p1.y);
	}
					
	p1 *= TerrainTile::GetSize();

	return p1;
}

Vector2 TileEditor::GetRoundingOffset()
{
	if (!selectedTiles)
		return Vector2(0);

	// snap to grid
	Vector2 pos = selectedTilesPos;
	pos = (selectedTilesPos - g_terrain->GetPosWorld()) / TerrainTile::GetSize();
	pos = Vector2(floor(pos.x + 0.5f), floor(pos.y + 0.5f));
	pos = Vector2(pos) * TerrainTile::GetSize() + g_terrain->GetPosWorld();
	return pos - selectedTilesPos;
}

void TileEditor::Render()
{
	if (selectedTiles)
	{
		if (selectedTilesSize.x == INT_MAX || selectedTilesSize.y == INT_MAX)
		{
			// bug: sometimes tile size gets set wrong
			ASSERT(false);
			RemoveSelected();
			return;
		}

		const Vector2 pos = selectedTilesPos + GetRoundingOffset();
		g_terrainRender.RenderTiles(selectedTiles, pos, selectedTilesSize.x, selectedTilesSize.y, selectedLayerCount, firstSelectedLayer);
	}
}

void TileEditor::RenderSelectionBox()
{
	if (selectedTiles)
	{
		const Vector2 pos = selectedTilesPos + GetRoundingOffset();
		const Box2AABB box(pos, pos + Vector2(selectedTilesSize) * TerrainTile::GetSize());
		g_render->DrawAxisAlignedBox(box, selectedLayerCount > 1? Color::Yellow() : Color::Green());
	}
}

Box2AABB TileEditor::GetSelectionBox() const
{
	return Box2AABB(selectedTilesPos, selectedTilesPos + Vector2(selectedTilesSize) * TerrainTile::GetSize());
}

void TileEditor::Select(Box2AABB& box, bool allLayers)
{
	// paste the current tile buffer down
	ClearSelection();

	// get selection box
	box = box.SortBounds();

	if ((box.upperBound - box.lowerBound).LengthSquared() < 0.1f)
		return; // ignore really small selections

	IntVector2 pos0 = g_terrain->GetTileIndex(box.lowerBound);
	IntVector2 pos1 = g_terrain->GetTileIndex(box.upperBound) + IntVector2(1,1);

	// cap selection bounds
	pos0.x = Cap(pos0.x, 0, Terrain::fullSize.x*Terrain::patchSize);
	pos0.y = Cap(pos0.y, 0, Terrain::fullSize.y*Terrain::patchSize);
	pos1.x = Cap(pos1.x, 0, Terrain::fullSize.x*Terrain::patchSize);
	pos1.y = Cap(pos1.y, 0, Terrain::fullSize.y*Terrain::patchSize);

	// update the selection box to the snapped to grid version
	box.lowerBound = Vector2(pos0) * TerrainTile::GetSize() + g_terrain->GetPosWorld();
	box.upperBound = Vector2(pos1) * TerrainTile::GetSize() + g_terrain->GetPosWorld();

	// get selection dimensions
	selectedTilesSize = pos1 - pos0;
	selectedLayerCount = allLayers? Terrain::patchLayers : 1;
	if (selectedTilesSize.x == 0 && selectedTilesSize.y == 0)
		return;
	
	// create the selected tile buffer
	selectedTiles = new TerrainTile[selectedLayerCount * selectedTilesSize.x * selectedTilesSize.y];
	
	firstSelectedLayer = allLayers? 0 : terrainLayer;

	// copy over selected tiles
	for(int x=0; x<selectedTilesSize.x; ++x)
	for(int y=0; y<selectedTilesSize.y; ++y)
	for(int l=0; l<selectedLayerCount; ++l)
	{
		TerrainTile& selectedTile = GetSelectedTile(x, y, l);

		const IntVector2 terrainTilePos = IntVector2(x, y) + pos0;
		TerrainTile* terrainTile = g_terrain->GetTile(terrainTilePos.x, terrainTilePos.y, firstSelectedLayer + l);
		ASSERT(terrainTile);

		selectedTile = *terrainTile;
		terrainTile->MakeClear();
	}

	selectedTilesPos = Vector2(pos0) * TerrainTile::GetSize() + g_terrain->GetPosWorld();

	// create new tile buffer
	// clear that space in the terrain
	// set world space position of buffer
}

bool TileEditor::IsOnSelected(const Vector2& pos) const
{
	if (!selectedTiles)
		return false;

	const Box2AABB selectionBox(selectedTilesPos, selectedTilesPos + Vector2(selectedTilesSize) * TerrainTile::GetSize());
	return selectionBox.Contains(pos);
}

void TileEditor::RotateSelection(bool rotateCW)
{
	if (!selectedTiles)
		return;

	TerrainTile* tileBuffer = new TerrainTile[selectedLayerCount * selectedTilesSize.x * selectedTilesSize.y];
	
	for (int x = 0; x < selectedTilesSize.x; ++x)
	for (int y = 0; y < selectedTilesSize.y; ++y)
	for (int l = 0; l < selectedLayerCount; ++l)
	{
		TerrainTile& tb = tileBuffer[selectedTilesSize.x*selectedTilesSize.y*l + selectedTilesSize.y*x + y];
		tb = GetSelectedTile(x, y, l);
	}

	IntVector2 selectedTilesSizeOld = selectedTilesSize;
	swap(selectedTilesSize.x, selectedTilesSize.y);

	for (int x = 0; x < selectedTilesSizeOld.x; ++x)
	for (int y = 0; y < selectedTilesSizeOld.y; ++y)
	for (int l = 0; l < selectedLayerCount; ++l)
	{
		TerrainTile& tb = tileBuffer[selectedTilesSizeOld.x*selectedTilesSizeOld.y*l + selectedTilesSizeOld.y*x + y];
		
		int x2 = rotateCW ? y : selectedTilesSizeOld.y - 1 - y;
		int y2 = rotateCW ? selectedTilesSizeOld.x - 1 - x : x;
		tb.Rotate(rotateCW ? 3 : 1);
		GetSelectedTile(x2, y2, l) = tb;
	}

	SAFE_DELETE_ARRAY(tileBuffer);
}

void TileEditor::MirrorSelection()
{
	if (!selectedTiles)
		return;

	TerrainTile* tileBuffer = new TerrainTile[selectedLayerCount * selectedTilesSize.x * selectedTilesSize.y];
	
	for (int x = 0; x < selectedTilesSize.x; ++x)
	for (int y = 0; y < selectedTilesSize.y; ++y)
	for (int l = 0; l < selectedLayerCount; ++l)
	{
		TerrainTile& tb = tileBuffer[selectedTilesSize.x*selectedTilesSize.y*l + selectedTilesSize.y*x + y];
		tb = GetSelectedTile(x, y, l);
	}

	for (int x = 0; x < selectedTilesSize.x; ++x)
	for (int y = 0; y < selectedTilesSize.y; ++y)
	for (int l = 0; l < selectedLayerCount; ++l)
	{
		TerrainTile& tb = tileBuffer[selectedTilesSize.x*selectedTilesSize.y*l + selectedTilesSize.y*x + y];
		
		int x2 = selectedTilesSize.x - 1 - x;
		tb.Mirror();
		GetSelectedTile(x2, y, l) = tb;
	}

	SAFE_DELETE_ARRAY(tileBuffer);
}

void TileEditor::ClearSelection(bool anchor)
{
	if (!selectedTiles)
		return;

	if (anchor)
	{
		// round position
		const IntVector2 terrainTilePosOffset = (selectedTilesPos - g_terrain->GetPosWorld()) / TerrainTile::GetSize();
		
		// copy tiles to terrain
		for(int x=0; x<selectedTilesSize.x; ++x)
		for (int y = 0; y < selectedTilesSize.y; ++y)
		for (int l = 0; l < selectedLayerCount; ++l)
		{
			TerrainTile& selectedTile = GetSelectedTile(x, y, l);
			if (selectedTile.IsAreaClear())
				continue;

			const IntVector2 terrainTilePos = IntVector2(x, y) + terrainTilePosOffset;
			TerrainTile* terrainTile = g_terrain->GetTile(terrainTilePos.x, terrainTilePos.y, firstSelectedLayer + l);
			if (terrainTile)
				*terrainTile = selectedTile;
		}

		g_editor.SetStateChanged();
	}

	RemoveSelected();
}

void TileEditor::RemoveSelected()
{
	if (!selectedTiles)
		return;
	
	delete [] selectedTiles;
	selectedTiles = NULL;
	selectedTilesSize = IntVector2(0);
}

void TileEditor::ClearClipboard()
{
	SAFE_DELETE_ARRAY(copiedTiles);
}

void TileEditor::FloodFill(const Vector2& pos, BYTE surfaceData)
{
	TerrainPatch* patch = g_terrain->GetPatch(pos);
	if (!patch)
		return;
	
	FloodFill(*patch, pos, surfaceData);
}

void TileEditor::FloodErase(const Vector2& pos)
{
	TerrainPatch* patch = g_terrain->GetPatch(pos);
	if (!patch)
		return;

	FloodFill(*patch, pos, 0);
}

void TileEditor::Cut()
{
	Copy();
	RemoveSelected();
}

void TileEditor::Copy()
{
	ClearClipboard();

	if (!selectedTiles)
		return;
	
	copiedTileSize = selectedTilesSize;
	copiedLayerCount = selectedLayerCount;
	copiedTilesPos = selectedTilesPos;
	
	// create the selected tile buffer
	const int size = copiedLayerCount * copiedTileSize.x * copiedTileSize.y;
	copiedTiles = new TerrainTile[size];
	memcpy(copiedTiles, selectedTiles, size * sizeof(TerrainTile));
}

void TileEditor::Paste(const Vector2& offset)
{
	ClearSelection();

	if (!copiedTiles || copiedTileSize.x == 0 && copiedTileSize.y == 0)
		return;
	
	selectedTilesPos = copiedTilesPos + offset;
	selectedTilesSize = copiedTileSize;
	selectedLayerCount = copiedLayerCount;

	if (selectedLayerCount > 1 && firstSelectedLayer > 0)
		firstSelectedLayer = 0;
	
	if (enableCrossLayerPaste && selectedLayerCount == 1 && firstSelectedLayer != terrainLayer)
	{
		// allow pasting from one layer to another
		firstSelectedLayer = terrainLayer;
	}

	// create the selected tile buffer
	const int size = selectedLayerCount * selectedTilesSize.x * selectedTilesSize.y;
	selectedTiles = new TerrainTile[size];
	memcpy(selectedTiles, copiedTiles, size * sizeof(TerrainTile));
}

Vector2 TileEditor::GetPasteOffset(const Vector2& pos)
{
	if (copiedTiles == NULL)
		return pos;

	if (pasteToCorner)
		return pos - copiedTilesPos - Vector2(copiedTileSize) * TerrainTile::GetSize() * Vector2(0, 1) + 0.5f*Vector2(-TerrainTile::GetSize(), TerrainTile::GetSize());
	else
		return pos - copiedTilesPos - Vector2(copiedTileSize) * TerrainTile::GetSize() * 0.5f;
}

void TileEditor::ChangeDrawType(bool direction)
{
	const int rows = g_editorGui.GetTileQuickPickRows();

	if (direction)
	{
		++drawSurface;
		if (drawSurface >= rows * EditorGui::quickPickTileColumns)
			drawSurface -= rows * EditorGui::quickPickTileColumns;
	}
	else
	{
		--drawSurface;
		if (drawSurface < 0)
			drawSurface += rows * EditorGui::quickPickTileColumns;
	}
}
	
void TileEditor::ChangeTileSet(bool direction)
{
	if (Terrain::tileSetCount == 0)
	{
		drawTileSet = 0;
		return;
	}

	if (direction)
	{
		drawTileSet = (++drawTileSet % Terrain::tileSetCount);
	}
	else
	{
		if (drawTileSet == 0)
			drawTileSet = Terrain::tileSetCount - 1;
		else
			--drawTileSet;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	low level functions to modify terrain
*/
////////////////////////////////////////////////////////////////////////////////////////

void TileEditor::Resurface(const Vector2& posA, const Vector2& posB, BYTE surfaceData, BYTE tileRotation, bool tileMirror)
{
	for(int i = 0; i < Terrain::fullSize.x; ++i)
	for(int j = 0; j < Terrain::fullSize.y; ++j)
		Resurface(*g_terrain->GetPatch(i,j), posA, posB, surfaceData, tileRotation, tileMirror);
}

// test if line segment collided with this square tile
// if it is then set the positions on where it entered/exited
void TileEditor::Resurface(TerrainPatch& patch, const Vector2& posA, const Vector2& posB, BYTE surfaceData, BYTE tileRotation, bool tileMirror)
{
	// check if posA or posB 
	const Vector2 minPatchPos = patch.GetTilePos(0,0);
	const Vector2 maxPatchPos = patch.GetTilePos(Terrain::patchSize, Terrain::patchSize);

	Box2AABB box1(minPatchPos, maxPatchPos);
	Box2AABB box2(posA, posB);
	if (!box1.PartiallyContains(box2))
		return;

	bool stateChanged = false;
	for(int x=0; x<Terrain::patchSize; ++x)
	for(int y=0; y<Terrain::patchSize; ++y)
	{
		TerrainTile& tile = patch.GetTileLocal(x, y, terrainLayer);
		const Vector2 tileOffset = patch.GetTilePos(x, y);

		bool wasFull = tile.IsFull();
		const BYTE edgeData = tile.GetEdgeData();
		if (tile.Resurface(posA - tileOffset, posB - tileOffset))
		{
			if (surfaceData == 0)
			{
				if (wasFull && tile.GetSurfaceData(1) != 0)
					tile.SetSurfaceData(0, tile.GetSurfaceData(1));
				tile.SetSurfaceData(1, surfaceData);
			}
			else
			{
				if (wasFull && surfaceData == tile.GetSurfaceData(1))
					tile.SetSurfaceData(1, 0);
				tile.SetSurfaceData(0, surfaceData);
			}
			tile.SetTileSet(drawTileSet);
			tile.SetRotation(tileRotation);
			tile.SetMirror(tileMirror);
			stateChanged = true;
		}
	}

	if (stateChanged)
		g_editor.SetStateChanged();
}

// 0 == left, 1 == up, 2 == right, 3 == down, 
void TileEditor::FloodFill(TerrainPatch& patch, const Vector2& testPos, BYTE surfaceData)
{
	int x, y;
	TerrainTile* tile = patch.GetTile(testPos, x, y, terrainLayer);
	if (!tile)
		return;

	const int startSurfaceSide = g_terrain->GetSurfaceSide(testPos, terrainLayer);
	const BYTE startSurfaceData = tile->GetSurfaceData(startSurfaceSide);

	if (startSurfaceData == surfaceData && tile->GetTileSet() == drawTileSet && tile->GetRotation() == drawRotation && tile->GetMirror() == drawMirror)
		return;

	FloodFillInternal(patch, x, y, surfaceData, startSurfaceSide, startSurfaceData);
}

// 0 == left, 1 == up, 2 == right, 3 == down, 
void TileEditor::FloodFillDirection(TerrainPatch& patch, int x, int y, int direction, BYTE surfaceData, int surfaceSide, BYTE startSurfaceData)
{
	TerrainTile* tile = patch.GetTile(x, y, terrainLayer);
	if (!tile)
		return;

	// figure out where the next tile in that direction is is
	int xOffset = 0;
	int yOffset = 0;
	if (direction == 0)
		xOffset = -1;
	else if (direction == 1)
		yOffset = 1;
	else if (direction == 2)
		xOffset = 1;
	else if (direction == 3)
		yOffset = -1;

	int xNext = x + xOffset;
	int yNext = y + yOffset;

	TerrainTile* tileNext = patch.GetTile(xNext, yNext, terrainLayer);
	if (!tileNext)
		return;

	bool surface0Touch, surface1Touch;
	tile->SurfaceTouches(*tileNext, xOffset, yOffset, direction, surfaceSide, surface0Touch, surface1Touch);
	if (surface0Touch)
		FloodFillInternal(patch, xNext, yNext, surfaceData, 0, startSurfaceData);
	if (surface1Touch)
		FloodFillInternal(patch, xNext, yNext, surfaceData, 1, startSurfaceData);
}

void TileEditor::FloodFillInternal(TerrainPatch& patch, int x, int y, BYTE surfaceData, int surfaceSide, BYTE startSurfaceData)
{
	TerrainTile* tile = patch.GetTile(x, y, terrainLayer);
	if (!tile)
		return;

	// if we hit a tile with no surfaces in common with start then bail out
	if (tile->GetSurfaceData(1) != startSurfaceData && (tile->IsFull() || tile->GetSurfaceData(0) != startSurfaceData))
		return;

	// stop if we have a tile with the correct surface
	if (tile->GetSurfaceData(surfaceSide) == surfaceData && tile->GetTileSet() == drawTileSet && tile->GetRotation() == drawRotation && tile->GetMirror() == drawMirror)
		return;

	// stop if we are full and surface 1 is the correct surface
	if (tile->IsFull() && tile->GetSurfaceData(1) == surfaceData && tile->GetTileSet() == drawTileSet  && tile->GetRotation() == drawRotation && tile->GetMirror() == drawMirror)
		return;

	tile->SetSurfaceData(surfaceSide, surfaceData);
	tile->SetTileSet(drawTileSet);
	tile->SetRotation(drawRotation);
	tile->SetMirror(drawMirror);

	// if both sides of surface data are the same then wipe out edge data
	if (tile->AreBothSurfacesEqual())
	{
		tile->MakeFull();
		tile->SetSurfaceData(0, 0);
	}

	FloodFillDirection(patch, x, y, 0, surfaceData, surfaceSide, startSurfaceData);
	FloodFillDirection(patch, x, y, 1, surfaceData, surfaceSide, startSurfaceData);
	FloodFillDirection(patch, x, y, 2, surfaceData, surfaceSide, startSurfaceData);
	FloodFillDirection(patch, x, y, 3, surfaceData, surfaceSide, startSurfaceData);
}

int TileEditor::GetDisplayDrawSurfaceIndex() const
{
	const int quickPickSurface = g_editorGui.GetTileQuickPickId();
	return (quickPickSurface >= 0)? quickPickSurface : drawSurface; 
}