////////////////////////////////////////////////////////////////////////////////////////
/*
	Object Editor
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../terrain/terrain.h"
#include "../editor/tileEditor.h"
#include "../editor/objectEditor.h"

// prevent scaling stubs below this minimum size
float ObjectEditor::minStubSize = 0.1f;
ConsoleCommand(ObjectEditor::minStubSize, minStubSize);

// should editor always snap to grid without user holding a modifier
bool ObjectEditor::snapToGrid = true;
ConsoleCommand(ObjectEditor::snapToGrid, editorSnapToGrid);

// setting to snap to grid in editor, defined as percentage of tile size
float ObjectEditor::snapToGridSize = 0.25f;
ConsoleCommand(ObjectEditor::snapToGridSize, editorSnapToGridSize);

// allow dragging objects out of quick pick
bool ObjectEditor::enableQuickPickDrag = true;
ConsoleCommand(ObjectEditor::enableQuickPickDrag, enableQuickPickDrag);

// setting to snap to grid angle in editor, defined by how many divisions there are
int ObjectEditor::snapToGridAngle = 16;
ConsoleCommand(ObjectEditor::snapToGridAngle, editorSnapToGridAngle);

// small radius so 0 sized stubs can be selected
float ObjectEditor::zeroStubRadius = 0.25f;
ConsoleCommand(ObjectEditor::zeroStubRadius, editorZeroStubRadius);

ObjectEditor::ObjectEditor()
{
}
	
void ObjectEditor::ChangeDrawType(bool direction)
{
	const int rows = g_editorGui.GetObjectQuickPickRows();

	if (direction)
	{
		newStubType = (GameObjectType)(newStubType + 1);
		if (newStubType >= rows * EditorGui::quickPickObjectColumns)
			newStubType = GameObjectType(newStubType - rows * EditorGui::quickPickObjectColumns);
	}
	else
	{
		newStubType = (GameObjectType)(newStubType - 1);
		if (newStubType < 0)
			newStubType = GameObjectType(newStubType + rows * EditorGui::quickPickObjectColumns);
	}
}

void ObjectEditor::Update(bool isActive)
{
	ASSERT(g_terrain);
	const Vector2 mousePos = g_input->GetMousePosWorldSpace();

	if (isActive)
	{
		if (!g_editorGui.IsEditBoxFocused())
		{
			if (!g_input->IsDown(GB_Control))
			{
				if (g_input->IsDownUI(GB_Editor_Left))
					ChangeDrawType(false);
				else if (g_input->IsDownUI(GB_Editor_Right))
					ChangeDrawType(true);
				if (g_input->IsDownUI(GB_Editor_Up))
				{
					const int rows = g_editorGui.GetObjectQuickPickRows();
					newStubType = GameObjectType(newStubType - EditorGui::quickPickObjectColumns);
					if (newStubType < 0)
						newStubType = GameObjectType(newStubType + rows * EditorGui::quickPickObjectColumns);
				}
				else if (g_input->IsDownUI(GB_Editor_Down))
				{
					const int rows = g_editorGui.GetObjectQuickPickRows();
					newStubType = GameObjectType(newStubType + EditorGui::quickPickObjectColumns);
					if (newStubType >= rows * EditorGui::quickPickObjectColumns)
						newStubType = GameObjectType(newStubType - rows * EditorGui::quickPickObjectColumns);
				}
		
				if (g_input->WasJustPushed(GB_Editor_Add))
					AddStub();
				else if ( g_input->WasJustPushed(GB_Editor_Remove))
				{
					if (g_editor.HasSelection())
					{
						RemoveSelected();
						g_editor.GetTileEditor().RemoveSelected();
						g_editor.SaveState();
					}
				}
			}
		}
		
		if (isInQuickPick)
		{
			if (g_editorGui.GetObjectQuickPickId() == -1)
			{
				if (enableQuickPickDrag && g_input->IsDown(GB_MouseLeft) && !g_input->IsDown(GB_Editor_MouseDrag) && !g_input->IsDown(GB_MouseRight))
				{
					// create object when dragging out of quick pick
					AddStub();
				}
				isInQuickPick = false;
			}
		}

		if (!g_editor.IsRectangleSelecting() && g_input->IsDown(GB_MouseLeft) && !g_input->IsDown(GB_Editor_MouseDrag) && !g_input->IsDown(GB_MouseRight))
		{
			if (g_input->WasJustPushed(GB_MouseLeft))
			{
				// quick pick
				const int quickPickType = g_editorGui.GetObjectQuickPickId();
				if (quickPickType >= 0)
				{
					if (quickPickType >= 0 && quickPickType <= ObjectTypeInfo::GetMaxType())
						newStubType = GameObjectType(quickPickType);
					else
						newStubType = GameObjectType(0);
					isInQuickPick = true;
				}
			}
		}

		if (!isInQuickPick && g_input->WasJustPushed(GB_MouseLeft) && !g_input->IsDown(GB_Editor_MouseDrag))
		{
			// object selection
			GameObjectStub* newSelectedObjectStub = g_terrain->GetStub(mousePos);

			if (g_editor.HasSelection() && !g_editor.IsOnSelected(mousePos) && !g_input->IsDown(GB_Shift))
			{
				g_editor.ClearSelection();
				g_editor.SaveState(true);
			}	

			if (newSelectedObjectStub)
			{
				if (!IsSelected(*newSelectedObjectStub))
				{
					// select stub
					if (!g_input->IsDown(GB_Shift))
						selectedStubs.clear();
					selectedStubs.push_back(newSelectedObjectStub);
				}
				else if (g_input->IsDown(GB_Shift) && !HasOnlyOneSelected())
					selectedStubs.remove(newSelectedObjectStub);
				g_editorGui.NewObjectSelected();
			}

			if (HasOnlyOneSelected())
			{
				newStubType = GetOnlyOneSelectedStub()->type;
				g_editorGui.NewObjectSelected();
			}
		}
	}

	if (!selectedStubs.empty())
	{
		if (HasOnlyOneSelected())
		{
			GameObjectStub& stub = *GetOnlyOneSelectedStub();

			// allow user to change type and attributes when only one stub is selected
			//stub.type = newStubType;

			// update attributes box in real time
			LPCWSTR attributes = g_editorGui.GetEditBoxText();
			wcstombs_s(NULL, stub.attributes, GameObjectStub::attributesLength, attributes, GameObjectStub::attributesLength-1);	
		}

		for (list<GameObjectStub*>::iterator it = selectedStubs.begin(); it != selectedStubs.end(); ) 
		{
			// protect against stubs being removed from the list
			GameObjectStub* stub = *it;
			++it;

			UpdateSelected(stub);
		}
	}
}

void ObjectEditor::Render()
{
	Vector2 offset(0);
	if (g_editor.GetTileEditor().HasSelection())
	{
		offset = g_editor.GetTileEditor().GetRoundingOffset();
	}

	for (list<GameObjectStub*>::iterator it = selectedStubs.begin(); it != selectedStubs.end(); ++it) 
	{
		// draw selected objects
		GameObjectStub stub = **it;
		
		if (g_editor.GetTileEditor().HasSelection())
		{
			stub.xf.position += offset;
		}
		else if (!g_editor.GetIsInQuickPick() && !g_input->IsDown(GB_Editor_MouseDrag) && (g_input->IsDown(GB_MouseRight) || g_input->IsDown(GB_MouseLeft)) && snapToGrid)
		{
			if (snapToGridSize > 0)
			{
				// round position
				const float gridSize = snapToGridSize * TerrainTile::GetSize();
				stub.xf.position.x = gridSize*floor(0.5f + stub.xf.position.x/gridSize);
				stub.xf.position.y = gridSize*floor(0.5f + stub.xf.position.y/gridSize);
			}

			if (snapToGridSize > 0)
			{
				// round scale
				const float gridSize = snapToGridSize * TerrainTile::GetSize();
				stub.size.x = Max(gridSize*floor(0.5f + stub.size.x/gridSize), minStubSize);
				stub.size.y = Max(gridSize*floor(0.5f + stub.size.y/gridSize), minStubSize);
			}

			if (snapToGridAngle > 0)
			{
				// round rotation
				stub.xf.angle = (PI / snapToGridAngle)*floor(0.5f + stub.xf.angle / (PI / snapToGridAngle));
			}
		}
		
		stub.Render();

		const Color c = Color::White();
		g_render->DrawBox(stub.xf, stub.size, Color::Green());
		g_render->DrawSolidCircle(stub.xf, 0.1f*TerrainTile::GetSize(), Color::Black(0.5f));
		g_render->DrawSolidCircle(stub.xf, 0.05f*TerrainTile::GetSize(), c);

		// draw a little arrow
		Line2 line(stub.xf.position, stub.xf.position + 2*stub.size.y*stub.xf.GetUp());
		g_render->DrawLine(line, c);
		line.p1 = line.p2 - 0.5f*stub.size.y*stub.xf.GetUp() - 0.5f*stub.size.y*stub.xf.GetRight();
		g_render->DrawLine(line, c);
		line.p1 = line.p2 - 0.5f*stub.size.y*stub.xf.GetUp() + 0.5f*stub.size.y*stub.xf.GetRight();
		g_render->DrawLine(line, c);
	}
}

bool ObjectEditor::HasOnlyOneSelected() const 
{ 
	return selectedStubs.size() == 1 && !g_editor.GetTileEditor().HasSelection(); 
}

void ObjectEditor::UpdateSelected(GameObjectStub* stub)
{
	GameObjectStub stubCopy = *stub;

	const Vector2 mousePos = g_input->GetMousePosWorldSpace();
	const Vector2 mouseDelta = g_input->GetMouseDeltaWorldSpace();

	bool isScaling = false;
	bool isRotating = false;
	bool isMoving = false;

	if (!g_editor.GetIsInQuickPick() && !g_input->IsDown(GB_Editor_MouseDrag) && (g_input->IsDown(GB_MouseLeft) || g_input->IsDown(GB_MouseRight)))
	{
		if (HasOnlyOneSelected() && g_input->IsDown(GB_Control))
		{
			// scale object on only one axis
			isScaling = true;
			Vector2 localOffset = stub->xf.Inverse().TransformCoord(mousePos);
			localOffset.x = fabs(localOffset.x);
			localOffset.y = fabs(localOffset.y);

			Vector2 localOffsetLast = stub->xf.Inverse().TransformCoord(mousePos - mouseDelta);
			localOffsetLast.x = fabs(localOffsetLast.x);
			localOffsetLast.y = fabs(localOffsetLast.y);

			const Vector2 localOffsetDelta = localOffset - localOffsetLast;
			stub->size += localOffsetDelta;
			stub->size.x = Max(stub->size.x, minStubSize);
			stub->size.y = Max(stub->size.y, minStubSize);
		}
		else if (HasOnlyOneSelected() && g_input->IsDown(GB_Shift))
		{
			// rotate object
			isRotating = true;
			const float localAngle = (stub->xf.position - mousePos).GetAngle();
			const float localAngleLast = (stub->xf.position - (mousePos - mouseDelta)).GetAngle();
			const float localAngleDelta = localAngle - localAngleLast;

			stub->xf.angle += localAngleDelta;
			if (stub->xf.angle < 0)
				stub->xf.angle = 2*PI + stub->xf.angle;
			else if (stub->xf.angle > 2*PI)
				stub->xf.angle = stub->xf.angle - 2*PI;
		}
		
		if (!g_input->IsDown(GB_Control) && !g_input->IsDown(GB_Shift))
		{
			// move object
			isMoving = true;
			stub = &MoveStub(*stub, mouseDelta);
		}
	}
	
	if (snapToGrid)
	{
		if (!g_editor.GetTileEditor().HasSelection() && !isMoving && snapToGridSize > 0)
		{
			// round position
			// note: must be done via MoveStub so that stubs stay in the correct patch
			const float gridSize = snapToGridSize * TerrainTile::GetSize();
			Vector2 newPos(gridSize*floor(0.5f + stub->xf.position.x / gridSize), gridSize*floor(0.5f + stub->xf.position.y / gridSize));
			Vector2 deltaPos = newPos - stub->xf.position;
			MoveStub(*stub, deltaPos);
		}

		if (!isScaling && snapToGridSize > 0)
		{
			// round scale
			const float gridSize = snapToGridSize * TerrainTile::GetSize();
			stub->size.x = Max(gridSize*floor(0.5f + stub->size.x/gridSize), minStubSize);
			stub->size.y = Max(gridSize*floor(0.5f + stub->size.y/gridSize), minStubSize);
		}

		if (!isRotating && snapToGridAngle > 0)
		{
			// round rotation
			stub->xf.angle = (PI / snapToGridAngle)*floor(0.5f + stub->xf.angle / (PI / snapToGridAngle));
		}
	}
		
	if (stubCopy.xf != stub->xf || stub->size != stub->size)
		g_editor.SetStateChanged();
}

void ObjectEditor::MoveSelectedStubs(const Vector2& offset)
{
	for (list<GameObjectStub*>::iterator it = selectedStubs.begin(); it != selectedStubs.end(); ) 
	{
		// protect against stubs being removed from the list
		GameObjectStub& stub = **it;
		++it;

		MoveStub(stub, offset);
	}
}

GameObjectStub& ObjectEditor::MoveStub(GameObjectStub& stub, const Vector2& offset)
{
	TerrainPatch* patchOld = g_terrain->GetPatch(stub.xf.position);
	
	ASSERT(patchOld->GetStub(stub.handle));
	if (!patchOld->GetStub(stub.handle))
	{
		// hack: there was a bug where stubs were getting put int the wrong patch, should be fixed now
		// incase patch doesn't match pos, search terrain for matching stub
		g_terrain->GetStub(stub.handle, &patchOld);
	}

	const Vector2 newPos = stub.xf.position + offset;
	TerrainPatch* patchNew = g_terrain->GetPatch(newPos);
	if (!patchNew)
	{
		// todo: decide how to handle stubs moved off any patches
		return stub;
	}

	stub.xf.position = newPos;
	if (patchOld == patchNew)
		return stub;

	const bool isSelected = IsSelected(stub);
	const GameObjectStub copyStub = stub;
	patchOld->RemoveStub(&stub);
	if (isSelected)
		selectedStubs.remove(&stub);
	GameObjectStub* newStub = patchNew->AddStub(copyStub);
	if (isSelected)
		selectedStubs.push_front(newStub);
	
	ASSERT(g_terrain->GetPatch(newStub->xf.position)->GetStub(newStub->handle));
	g_editor.SetStateChanged();
	return *newStub;
}

void ObjectEditor::AddStub()
{
	// create new stub that is copy of selected stub
	// make selected be the new stub
	// add the selected stub to the list for this patch
	const Vector2 mousePos = g_input->GetMousePosWorldSpace();
	TerrainPatch* patch = g_terrain->GetPatch(mousePos);
	if (!patch)
		return;

	GameObjectStub newStubCopy(XForm2(mousePos), Vector2(TerrainTile::GetSize()/2), newStubType);
	
	// set default attrubutes
	strncpy_s(newStubCopy.attributes, sizeof(newStubCopy.attributes), newStubCopy.GetObjectInfo().GetAttributesDefault(), sizeof(newStubCopy.attributes));
	
	newStubCopy.xf.position = mousePos;
	g_terrain->GiveStubNewHandle(newStubCopy, true);

	selectedStubs.clear();
	GameObjectStub* newStub = patch->AddStub(newStubCopy);
	g_editor.SaveState();
	selectedStubs.push_back(newStub);
	g_editorGui.NewObjectSelected();

	/* // Hack: load test
	GameControlBase::autoSaveTerrain = false;
	for(int x=0; x<Terrain::fullSize.x; ++x)
	for(int y=0; y<Terrain::fullSize.y; ++y)
	{
		int patchForegroundTileCount = 0;
		int patchPickupCount = 0;
		TerrainPatch& patch = *(g_terrain->GetPatch(x,Terrain::fullSize.y-1-y));
		//for(int i=0; i<Terrain::patchSize; ++i)
		//for(int j=0; j<Terrain::patchSize; ++j)
		{
			GameObjectStub stub(patch.GetXFWorld(), Vector2(4,4), newStubCopy.type, "123423456778990ddsg");
			g_terrain->GiveStubNewHandle(stub, true);
			patch.AddStub(stub);
		}
	}*/
}

void ObjectEditor::DeleteUnselected(const Box2AABB& _box)
{
	// delete stubs that arent selected within a box (used to stamp down terrain)
	const Box2AABB box = _box.SortBounds();

	for(int x=0; x<Terrain::fullSize.x; ++x)
	for(int y=0; y<Terrain::fullSize.y; ++y)
	{
		TerrainPatch& patch = *(g_terrain->GetPatch(x,Terrain::fullSize.y-1-y));
		for (list<GameObjectStub>::iterator it = patch.objectStubs.begin(); it != patch.objectStubs.end(); ) 
		{
			GameObjectStub& stub = *it;
			++it;
			
			if (!box.Contains(stub.xf.position))
				continue;

			if (IsSelected(stub))
				continue;

			patch.RemoveStub(&stub);
		}
	}
}

void ObjectEditor::Select(const Box2AABB& _box)
{
	// select all stubs within a bbox
	const Box2AABB box = _box.SortBounds();
	selectedStubs.clear();

	for(int x=0; x<Terrain::fullSize.x; ++x)
	for(int y=0; y<Terrain::fullSize.y; ++y)
	{
		TerrainPatch& patch = *(g_terrain->GetPatch(x,Terrain::fullSize.y-1-y));
		for (list<GameObjectStub>::iterator it = patch.objectStubs.begin(); it != patch.objectStubs.end(); ++it) 
		{
			GameObjectStub& stub = *it;
			const Vector2 stubSelectSize = stub.GetStubSelectSize();

			if (stubSelectSize.x < 0 || stubSelectSize.y < 0)
				continue;

			if (box.Contains(stub.xf.position))
				selectedStubs.push_back(&stub);
		}
	}

	if (HasOnlyOneSelected())
		newStubType = GetOnlyOneSelectedStub()->type;
	
	g_editorGui.NewObjectSelected();
}

void ObjectEditor::RotateSelection(bool rotateCW)
{
	if (!HasSelection())
		return;

	Box2AABB box;
	
	if (g_editor.GetTileEditor().HasSelection())
		box = g_editor.GetTileEditor().GetSelectionBox();
	else
	{
		box.lowerBound = Vector2(100000);
		box.upperBound = Vector2(-100000);
		for (GameObjectStub* stub : selectedStubs) 
		{
			box.lowerBound.x = Min(stub->xf.position.x, box.lowerBound.x);
			box.lowerBound.y = Min(stub->xf.position.y, box.lowerBound.y);
			box.upperBound.x = Max(stub->xf.position.x, box.upperBound.x);
			box.upperBound.y = Max(stub->xf.position.y, box.upperBound.y);
		}
	}

	const float angle = rotateCW ? -PI / 2 : PI / 2;
	
	for (list<GameObjectStub*>::iterator it = selectedStubs.begin(); it != selectedStubs.end(); ) 
	{
		// protect against stubs being removed from the list
		GameObjectStub* stub = *it;
		++it;

		Vector2 pos = stub->xf.position;
		Vector2 newPos = pos;
		newPos -= Vector2(box.lowerBound);
		if (rotateCW)
			newPos.x = box.GetSize().x - newPos.x;
		else
			newPos.y = box.GetSize().y - newPos.y;
		swap(newPos.x, newPos.y);
		newPos += Vector2(box.lowerBound);
		
		Vector2 offset = newPos - stub->xf.position;
		stub->xf.angle = CapAngle(stub->xf.angle + angle);
		MoveStub(*stub, offset);
	}
}

void ObjectEditor::MirrorSelection()
{
	if (!HasSelection())
		return;

	Box2AABB box;
	
	if (g_editor.GetTileEditor().HasSelection())
		box = g_editor.GetTileEditor().GetSelectionBox();
	else
	{
		box.lowerBound = Vector2(100000);
		box.upperBound = Vector2(-100000);
		for (GameObjectStub* stub : selectedStubs) 
		{
			box.lowerBound.x = Min(stub->xf.position.x, box.lowerBound.x);
			box.lowerBound.y = Min(stub->xf.position.y, box.lowerBound.y);
			box.upperBound.x = Max(stub->xf.position.x, box.upperBound.x);
			box.upperBound.y = Max(stub->xf.position.y, box.upperBound.y);
		}
	}
	
	for (list<GameObjectStub*>::iterator it = selectedStubs.begin(); it != selectedStubs.end(); ) 
	{
		// protect against stubs being removed from the list
		GameObjectStub* stub = *it;
		++it;

		Vector2 pos = stub->xf.position;
		Vector2 newPos = pos;
		newPos.x -= box.GetCenter().x;
		newPos.x = -newPos.x;
		newPos.x += box.GetCenter().x;

		Vector2 offset = newPos - stub->xf.position;
		stub->xf.angle = CapAngle(2*PI - stub->xf.angle);
		MoveStub(*stub, offset);
	}
}

void ObjectEditor::ClearSelection()
{
	selectedStubs.clear();
	g_editorGui.NewObjectSelected();
}

void ObjectEditor::ClearClipboard()
{
	copiedStubs.clear();
}

void ObjectEditor::RemoveSelected()
{
	if (!HasSelection())
		return;

	for(int x=0; x<Terrain::fullSize.x; ++x)
	for(int y=0; y<Terrain::fullSize.y; ++y)
	{
		TerrainPatch& patch = *(g_terrain->GetPatch(x,y));
		for (list<GameObjectStub>::iterator it = patch.objectStubs.begin(); it != patch.objectStubs.end(); ) 
		{
			GameObjectStub& stub = *it;
			++it;

			if (IsSelected(stub))
				patch.RemoveStub(&stub);
		}
	}
	
	ClearSelection();
}

bool ObjectEditor::IsOnSelected(const Vector2& pos) const
{
	GameObjectStub* stub = g_terrain->GetStub(pos);
	for (list<GameObjectStub*>::const_iterator it = selectedStubs.begin(); it != selectedStubs.end(); ++it) 
	{
		if (*it == stub)
			return true;
	}

	return false;
}

void ObjectEditor::SelectStub(const Vector2& pos)
{
	GameObjectStub* newSelectedObjectStub = g_terrain->GetStub(pos);
	if (newSelectedObjectStub && !IsSelected(*newSelectedObjectStub))
	{
		selectedStubs.push_back(newSelectedObjectStub);
		g_editorGui.NewObjectSelected();
	}

	if (HasOnlyOneSelected())
	{
		newStubType = GetOnlyOneSelectedStub()->type;
		g_editorGui.NewObjectSelected();
	}
}

bool ObjectEditor::IsSelected(const GameObjectStub& stub) const
{
	for (list<GameObjectStub*>::const_iterator it = selectedStubs.begin(); it != selectedStubs.end(); ++it) 
	{
		if ((**it).handle == stub.handle)
			 return true;
	}

	return false;
}

void ObjectEditor::RemoveFromSelection(GameObjectStub& stub) 
{ 
	selectedStubs.remove(&stub); 
	if (HasOnlyOneSelected())
	{
		newStubType = GetOnlyOneSelectedStub()->type;
		g_editorGui.NewObjectSelected();
	}
}

void ObjectEditor::AddToSelection(GameObjectStub& stub) 
{ 
	if (IsSelected(stub))
		return;

	selectedStubs.push_back(&stub); 
	if (HasOnlyOneSelected())
	{
		newStubType = GetOnlyOneSelectedStub()->type;
		g_editorGui.NewObjectSelected();
	}
}

void ObjectEditor::Cut()
{
	Copy();
	RemoveSelected();
}

void ObjectEditor::Copy()
{
	ClearClipboard();

	if (!HasSelection())
		return;
	
	for (list<GameObjectStub*>::iterator it = selectedStubs.begin(); it != selectedStubs.end(); ++it) 
	{
		GameObjectStub stub = **it;
		stub.handle = GameObject::invalidHandle;
		copiedStubs.push_back(stub);
	}

	if (selectedStubs.size() == 1)
	{
		// if there is only 1 stub, copy handle to clipboard
		WCHAR text[256];
		GameObjectHandle handle = selectedStubs.front()->handle;
		swprintf_s(text, 256, L"%d", handle);
		FrankUtil::CopyToClipboard(text);
	}
}

void ObjectEditor::Paste(const Vector2& offset)
{
	ClearSelection();
	
	if (copiedStubs.size() == 1)
	{
		// set new stub type to be the same as the selected
		newStubType = copiedStubs.front().type;
	}

	for (list<GameObjectStub>::iterator it = copiedStubs.begin(); it != copiedStubs.end(); ++it) 
	{
		GameObjectStub stub = *it;
		
		// paste stub into a patch
		stub.xf.position += offset;
		
		g_terrain->GiveStubNewHandle(stub, true);

		TerrainPatch* patch = g_terrain->GetPatch(stub.xf.position);
		if (!patch)
			continue;

		GameObjectStub* newStub = patch->AddStub(stub);
		selectedStubs.push_front(newStub);
	}

	g_editorGui.NewObjectSelected();
}

Vector2 ObjectEditor::GetPasteOffset(const Vector2& pos)
{
	if (copiedStubs.empty())
		return Vector2(0);

	Vector2 totalPos(0);
	for (list<GameObjectStub>::iterator it = copiedStubs.begin(); it != copiedStubs.end(); ++it) 
		totalPos += (*it).xf.position;

	const Vector2 avePos = totalPos / float(copiedStubs.size());
	return pos - avePos;
}

GameObjectType ObjectEditor::GetDisplayNewStubType() const 
{ 
	const int quickPickType = g_editorGui.GetObjectQuickPickId();
	return (quickPickType >= 0)? GameObjectType(quickPickType) : newStubType; 
}