////////////////////////////////////////////////////////////////////////////////////////
/*
	Terrain
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../terrain/terrainSurface.h"
#include "../terrain/terrain.h"
#include "../editor/objectEditor.h"
#include <fstream>

////////////////////////////////////////////////////////////////////////////////////////

// terrain settings
int Terrain::dataVersion				= 12;
IntVector2 Terrain::fullSize			= IntVector2(20);	// how many patches per terrain
int Terrain::patchSize					= 16;				// how many tiles per patch
int Terrain::patchLayers				= 2;				// how many layers per patch
int Terrain::physicsLayer				= 0;				// the layer to create physics for
int Terrain::windowSize					= 1;				// size of the tetrain stream window
int Terrain::renderWindowSize			= 1;				// size of the tetrain render window
Color Terrain::editorTileEdgeColor1		= Color::White();	// color used for lines around tiles when editing
Color Terrain::editorTileEdgeColor2		= Color::Black();	// color used for lines around tiles when editing
Vector2 Terrain::gravity				= Vector2::Zero();	// acceleartion due to gravity
WCHAR Terrain::terrainFilename[256]		= defaultTerrainFilename;
float Terrain::restitution				= 0;				// how bouncy is the terrain
float Terrain::friction					= 0.3f;				// how much friction for terrain
int Terrain::maxProxies					= 10000;			// limit on how many terrain proxies can be made
bool Terrain::terrainAlwaysDestructible	= false;			// allow any kind of terrain to be destroyed

// planet terrain config
bool Terrain::isCircularPlanet			= false;	// should terrain be treated like a circular planet?

// tile sets
int Terrain::tileSetCount				= 0;				// how many tile sets there are
Terrain::TileSetInfo Terrain::tileSets[maxTileSets] = { TileSetInfo() };

// if it should use polys or edge loops for terrain physics
bool Terrain::usePolyPhysics = true;

// combine physics of tiles to reduce proxies
bool Terrain::combineTileShapes = true;

// stream a window around the player
bool Terrain::enableStreaming = true;
bool Terrain::streamDebug = false;
ConsoleCommandSimple(bool, terrainDebug, false);

// set the handle to a large enough value to cover any objects we might create on startup
static const GameObjectHandle firstStartHandle = 10000; 

// should terrain deform operations update the minimap
bool Terrain::deformUpdateMap = true;

ConsoleCommand(Terrain::streamDebug, streamDebug);
ConsoleCommand(Terrain::gravity, gravity);
ConsoleCommand(Terrain::terrainAlwaysDestructible, terrainAlwaysDestructible);
ConsoleCommand(Terrain::isCircularPlanet, isCircularPlanet);
ConsoleCommand(Terrain::terrainFilename, terrainFilename);
ConsoleCommand(Terrain::restitution, terrainRestitution);
ConsoleCommand(Terrain::friction, terrainFriction);
ConsoleCommand(Terrain::usePolyPhysics, terrainPolyPhysics);
ConsoleCommand(Terrain::combineTileShapes, combineTileShapes);
ConsoleCommand(Terrain::enableStreaming, enableStreaming);
ConsoleCommand(Terrain::maxProxies, maxTerrainProxies);
ConsoleCommand(Terrain::windowSize, streamWindowSize);
ConsoleCommand(Terrain::renderWindowSize, terrainRenderWindowSize);
ConsoleCommand(Terrain::editorTileEdgeColor1, editorTileEdgeColor1);
ConsoleCommand(Terrain::editorTileEdgeColor2, editorTileEdgeColor2);
ConsoleCommand(Terrain::deformUpdateMap, deformUpdateMap);

////////////////////////////////////////////////////////////////////////////////////////

Terrain::Terrain(const Vector2& pos) :
	GameObject(XForm2(pos)),
	streamWindowPatch(0, 0),
	streamWindowPatchLast(0, 0),
	streamWindowSizeLast(0),
	streamWindow(Vector2::Zero(), Vector2::Zero())
{
	CreatePhysicsBody(b2_staticBody);

	playerEditorStartPos = Vector2(0);
	SetRenderGroup(0); // terrain is on render 0

	patches = static_cast<TerrainPatch**>(malloc(sizeof(void*) * fullSize.x * fullSize.y));

	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
	{
		const Vector2 patchPos = pos + patchSize * TerrainTile::GetSize() * Vector2((float)x, (float)y);
		patches[x + fullSize.x * y] = new TerrainPatch(patchPos);
	}

	// create terrain layers
	layerRenderArray = new TerrainLayerRender *[patchLayers];
	for (int i = 0; i < Terrain::patchLayers; ++i)
		layerRenderArray[i] = new TerrainLayerRender(pos, i);

	// assign first layer to our render group
	layerRenderArray[0]->SetRenderGroup(GetRenderGroup());

	startHandle = firstStartHandle;
	ResetStartHandle(startHandle);

	// terrain layer render handles rendering
	SetVisible(false);
}

Terrain::~Terrain()
{
	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
		delete GetPatch(x,y);

	free(patches);
	delete [] layerRenderArray;
}

void Terrain::GiveStubNewHandle(GameObjectStub& stub, bool fromEditor)
{
	if (fromEditor)
		stub.handle = startHandle++;
	else
	{
		stub.handle = GameObject::GetNextUniqueHandleValue();
		GameObject::SetNextUniqueHandleValue(stub.handle + 1);
		startHandle = GameObject::GetNextUniqueHandleValue();
	}
}

void Terrain::ResetStartHandle(GameObjectHandle handle)
{
	// reset the starting handle
	startHandle = handle;
	GameObject::SetNextUniqueHandleValue(startHandle);
}

void Terrain::Deactivate()
{
	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
		GetPatch(x,y)->Deactivate();
}

void Terrain::UpdateActiveWindow()
{
	const Vector2& pos = g_gameControlBase->GetStreamCenter();
	streamWindowPatch = GetPatchIndex(pos);
	int x2 = streamWindowPatchLast.x;
	int y2 = streamWindowPatchLast.y;
	int w2 = streamWindowSizeLast;

	const bool windowMoved = (streamWindowPatchLast != streamWindowPatch) || (streamWindowSizeLast != windowSize);

	// update stream window
	streamWindow = Box2AABB
	(
		GetPosWorld() + patchSize*TerrainTile::GetSize()*Vector2(streamWindowPatch - IntVector2(windowSize)),
		GetPosWorld() + patchSize*TerrainTile::GetSize()*Vector2(streamWindowPatch + IntVector2(1+windowSize))
	);
	
	streamWindowPatchLast = streamWindowPatch;
	streamWindowSizeLast = windowSize;

	//if (!init && (x == x2 && y == y2))
	//	return; // window has not moved

	if (!wasReset && windowMoved && enableStreaming)
	{
		// stream out patches
		// go through patches that were in the window
		for(int i=x2-w2; i<=x2+w2; ++i)
		for(int j=y2-w2; j<=y2+w2; ++j)
		{
			TerrainPatch* patch = GetPatch(i,j);
			if (!patch)
				continue;

			// check if patch is no longer in the window
			if (i < streamWindowPatch.x-windowSize || i > streamWindowPatch.x+windowSize || 
					j < streamWindowPatch.y-windowSize || j > streamWindowPatch.y+windowSize)
			{
				patch->SetActivePhysics(false);
				patch->SetActiveObjects(false);
			}
		}
	}

	if (enableStreaming)
	{
		// make physics in the current window active
		for(int i=streamWindowPatch.x-windowSize; i<=streamWindowPatch.x+windowSize; ++i)
		for(int j=streamWindowPatch.y-windowSize; j<=streamWindowPatch.y+windowSize; ++j)
		{
			TerrainPatch* patch = GetPatch(i,j);
			if (!patch)
				continue;

			patch->SetActivePhysics(true);
			patch->SetActiveObjects(true, windowMoved);
		}
	}
	else if (wasReset)
	{
		// load everything on startup if streaming is disabled
		for(int i=0; i<fullSize.x; ++i)
		for(int j=0; j<fullSize.y; ++j)
		{
			TerrainPatch* patch = GetPatch(i,j);
			if (!patch)
				continue;

			patch->SetActivePhysics(true);
			patch->SetActiveObjects(true);
		}
	}

	UpdateStreaming();
	wasReset = false;
}

void Terrain::UpdatePost()
{
	// update physics or terrain that needs rebuild
	const IntVector2 start = enableStreaming? streamWindowPatch - IntVector2(windowSize) : IntVector2(0);
	const IntVector2 end = enableStreaming? streamWindowPatch + IntVector2(windowSize) : fullSize - IntVector2(1);
	for(int i=start.x; i<=end.x; ++i)
	for(int j=start.y; j<=end.y; ++j)
	{
		TerrainPatch* patch = GetPatch(i,j);
		if (!patch)
			continue;

		if (patch->needsPhysicsRebuild)
		{
			g_terrainRender.RefereshCached(*patch);
			patch->SetActivePhysics(false);
			patch->SetActivePhysics(true);
		}
	}
}

void Terrain::UpdateStreaming()
{
	if (!enableStreaming)
		return;

	if (streamDebug)
		streamWindow.RenderDebug();

	// for all world objects
	const GameObjectHashTable& objects = g_objectManager.GetObjects();
	for (GameObjectHashTable::const_iterator it = objects.begin(); it != objects.end(); ++it)
	{
		GameObject* gameObject = it->second;

		if (gameObject->HasParent())
			continue;	// only stream out top level objects

		if (gameObject->IsDestroyed())
			continue;

		if (!gameObject->ShouldStreamOut())
			continue;

		const ObjectTypeInfo* type = gameObject->GetObjectInfo();

		if (type && type->IsSerializable())
		{
			// seriialize objects when they stream out
			TerrainPatch* patch = GetPatch(gameObject->GetPosWorld());
			if (patch)
			{
				const GameObjectStub stub = gameObject->Serialize();
				patch->AddStub(stub);
			}
		}

		gameObject->StreamOut();
	}
}

/*
void Terrain::Convert()
{
	IntVector2 newfullSize(25);
	int newpatchSize = 32;

	ofstream outTerrainFile("terrain.2dt", ios::out | ios::binary);
	if (outTerrainFile.fail())
		return;

	// write out the data version
	const char version = (char)(dataVersion);
	outTerrainFile.write(&version, 1);

	// save the next handle
	outTerrainFile.write((const char *)&startHandle, sizeof(startHandle));

	for(int x=0; x<newfullSize.x; ++x)
	for(int y=0; y<newfullSize.y; ++y)
	{
		//const TerrainPatch& patch = *patches[x][y];

		// write out the tile data
		for(int x2=0; x2<newpatchSize; ++x2)
		for(int y2=0; y2<newpatchSize; ++y2)
		{
			{
				int x3 = x * newpatchSize + x2;
				int y3 = y * newpatchSize + y2;

				int xo = x3 / patchSize;
				int xo2 = x3 % patchSize;

				int yo = y3 / patchSize;
				int yo2 = y3 % patchSize;

				ASSERT(xo < fullSize.x && yo < fullSize.y)
				ASSERT(xo2 < patchSize && yo2 < patchSize)

				TerrainTile& tile = patches[xo][yo]->tiles[xo2][yo2];
				outTerrainFile.write((const char *)&tile, sizeof(TerrainTile));
			}

			//outTerrainFile.write((const char *)&patch.tiles[x2][y2], sizeof(TerrainTile));
		}

		// save out the object stubs
		unsigned int stubCount = 0;
		outTerrainFile.write((const char *)&stubCount, sizeof(stubCount));
	}

	outTerrainFile.close();
}*/

bool Terrain::Save(const WCHAR* filename)
{
	ofstream outTerrainFile(filename, ios::out | ios::binary);
	if (outTerrainFile.fail())
		return false;

	// write out the data version
	const char version = (char)(dataVersion);
	outTerrainFile.write(&version, 1);

	// save player position
	playerEditorStartPos = g_gameControlBase->GetPlayer()? g_gameControlBase->GetPlayer()->GetPosWorld() : Vector2(0);
	outTerrainFile.write((const char *)&playerEditorStartPos.x,	sizeof(float));
	outTerrainFile.write((const char *)&playerEditorStartPos.y,	sizeof(float));
	
	outTerrainFile.write((const char *)&fullSize.x, sizeof(fullSize.x));
	outTerrainFile.write((const char *)&fullSize.y, sizeof(fullSize.y));
	outTerrainFile.write((const char *)&patchSize, sizeof(patchSize));
	outTerrainFile.write((const char *)&patchLayers, sizeof(patchLayers));

	// save the next handle
	outTerrainFile.write((const char *)&startHandle, sizeof(startHandle));

	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
	{
		const TerrainPatch& patch = *GetPatch(x,y);
		
		// fast write out the tile data
		outTerrainFile.write((const char *)patch.tiles, sizeof(TerrainTile) * patchSize * patchSize * patchLayers);

		// separate write out the tile data
		/*for(int l=0; l<patchLayers; ++l)
		for(int j=0; j<patchSize; ++j)
		for(int k=0; k<patchSize; ++k)
		{
			const TerrainTile& tile = patch.GetTileLocal(j, k, l);

			outTerrainFile << tile.GetEdgeData();
			outTerrainFile << tile.GetSurfaceData(0);
			outTerrainFile << tile.GetSurfaceData(1);
			outTerrainFile << tile.GetTileSet();
		}*/

		// save out the object stubs
		unsigned int stubCount = patch.objectStubs.size();
		outTerrainFile.write((const char *)&stubCount, sizeof(stubCount));
		for (list<GameObjectStub>::const_iterator it = patch.objectStubs.begin(); it != patch.objectStubs.end(); ++it) 
		{       
			const GameObjectStub& stub = *it;

			outTerrainFile.write((const char *)&stub.type,		sizeof(stub.type));
			outTerrainFile.write((const char *)&stub.xf,		sizeof(stub.xf));
			outTerrainFile.write((const char *)&stub.size,		sizeof(stub.size));
			outTerrainFile.write((const char *)&stub.handle,	sizeof(stub.handle));

			int attributesLength = strlen(stub.attributes) + 1;
			outTerrainFile.write((const char *)&attributesLength, sizeof(attributesLength));
			outTerrainFile.write((const char *)&stub.attributes, attributesLength);
		}
	}

	outTerrainFile.close();
	return true;
}

bool Terrain::Load(const WCHAR* filename)
{
	wasReset = true;
	g_editor.ResetEditor();

	ifstream inTerrainFile(filename, ios::in | ios::binary);

	if (inTerrainFile.fail())
		return LoadFromResource(filename);

	// check the data version
	char version;
	inTerrainFile.read(&version, 1);

	bool separateXYsize = true;
	if (version == 11)
	{
		version = 12; // handle converting to separate xy size
		separateXYsize = false;
	}

	if (version != dataVersion)
	{
		g_debugMessageSystem.AddError(L"Local terrain file version mismatch.  Using built in terrain.");
		inTerrainFile.close();
		return LoadFromResource(filename);
	}
	
	Vector2 playerPos(0);
	inTerrainFile.read((char *)&playerEditorStartPos.x, sizeof(float));
	inTerrainFile.read((char *)&playerEditorStartPos.y, sizeof(float));
	
	IntVector2 fullSizeIn;
	int patchSizeIn, patchLayersIn;
	inTerrainFile.read((char *)&fullSizeIn.x, sizeof(fullSizeIn.x));
	if (separateXYsize)
		inTerrainFile.read((char *)&fullSizeIn.y, sizeof(fullSizeIn.y));
	else
		fullSizeIn.y = fullSizeIn.x;
	inTerrainFile.read((char *)&patchSizeIn, sizeof(patchSizeIn));
	inTerrainFile.read((char *)&patchLayersIn, sizeof(patchLayersIn));
	if (fullSizeIn.x != fullSize.x || fullSizeIn.y != fullSize.y || patchSizeIn != patchSize || patchLayersIn != patchLayers)
	{
		g_debugMessageSystem.AddError(L"Local terrain file size mismatch.  Using built in terrain.");
		inTerrainFile.close();
		return LoadFromResource(filename);
	}
	
	// clear out terrain
	Clear();

	// read in next handle
	inTerrainFile.read((char *)&startHandle, sizeof(startHandle));

	for(int x=0; x<fullSizeIn.x; ++x)
	for(int y=0; y<fullSizeIn.y; ++y)
	{
		TerrainPatch& patch = *GetPatch(x,y);
		
		// fast read in the tile data
		inTerrainFile.read((char *)patch.tiles, sizeof(TerrainTile) * patchSizeIn * patchSizeIn * patchLayersIn);

		// separate read in the tile data
		/*for(int l=0; l<patchLayers; ++l)
		for(int j=0; j<patchSize; ++j)
		for(int k=0; k<patchSize; ++k)
		{
			TerrainTile& tile = patch.GetTileLocal(j, k, l);

			BYTE edgeData = 0;
			BYTE surfaceData0 = 0;
			BYTE surfaceData1 = 0;
			BYTE tileSet = 0;
			
			inTerrainFile.read((char *)&edgeData,		1);
			inTerrainFile.read((char *)&surfaceData0,	1);
			inTerrainFile.read((char *)&surfaceData1,	1);
			inTerrainFile.read((char *)&tileSet,		1);

			tile.SetEdgeData(edgeData);
			tile.SetSurfaceData(0, surfaceData0);
			tile.SetSurfaceData(1, surfaceData1);
			tile.SetTileSet(tileSet);
		}*/

		if (inTerrainFile.eof())
			break; // error

		// read in the object stubs
		unsigned int stubCount;
		inTerrainFile.read((char*)&stubCount, sizeof(stubCount));
		for (unsigned int i = 0; i < stubCount; ++i) 
		{       
			GameObjectStub stub;

			inTerrainFile.read((char *)&stub.type,		sizeof(stub.type));
			inTerrainFile.read((char *)&stub.xf,		sizeof(stub.xf));
			inTerrainFile.read((char *)&stub.size,		sizeof(stub.size));
			inTerrainFile.read((char *)&stub.handle,	sizeof(stub.handle));

			// hack: cap small stub sizes
			if (fabs(stub.size.x) < 0.01f)
				stub.size.x = 0.01f;
			if (fabs(stub.size.y) < 0.01f)
				stub.size.y = 0.01f;

			int attributesLength = 0;
			inTerrainFile.read((char *)&attributesLength, sizeof(attributesLength));
			inTerrainFile.read((char *)&stub.attributes, attributesLength);
			
			patch.AddStub(stub);
			if (inTerrainFile.eof())
				break; // error
		}
	}

	inTerrainFile.close();
	ResetStartHandle(startHandle);
	return true;
}

bool Terrain::LoadFromResource(const WCHAR* filename)
{
	// Get pointer and size to resource
	HRSRC hRes = FindResource(0, filename, RT_RCDATA);
	HGLOBAL hMem = LoadResource(0, hRes);
	void* pMem = LockResource(hMem);
	DWORD size = SizeofResource(0, hRes);

	if (!pMem || size == 0)
		return false;
	
	// clear out terrain
	Clear();

	BYTE* dataPointer = (BYTE*)pMem;	

	// check the data version
	BYTE version = *(dataPointer++);

	bool separateXYsize = true;
	if (version == 11)
	{
		version = 12; // handle converting to separate xy size
		separateXYsize = false;
	}

	if (version != dataVersion)
	{
		g_debugMessageSystem.AddError(L"Built in terrain version mismatch.  Using clear terrain.");
		return false;
	}
	
	playerEditorStartPos.x = *(float*)(dataPointer); dataPointer += sizeof(float);
	playerEditorStartPos.y = *(float*)(dataPointer); dataPointer += sizeof(float);
	
	IntVector2 fullSizeIn;
	int patchSizeIn, patchLayersIn;
	fullSizeIn.x = *(int*)(dataPointer);
	dataPointer += sizeof(fullSizeIn.x);
	if (separateXYsize)
	{
		fullSizeIn.y = *(int*)(dataPointer);
		dataPointer += sizeof(fullSizeIn.y);
	}
	else
		fullSizeIn.y = fullSizeIn.x;
	patchSizeIn = *(int*)(dataPointer);
	dataPointer += sizeof(patchSizeIn);
	patchLayersIn = *(int*)(dataPointer);
	dataPointer += sizeof(patchLayersIn);
	if (fullSizeIn.x != fullSize.x || fullSizeIn.y != fullSize.y ||patchSizeIn != patchSize || patchLayersIn != patchLayers)
	{
		g_debugMessageSystem.AddError(L"Built in terrain file size mismatch.  Using clear terrain.");
		return false;
	}

	// read in next handle
	startHandle = *(GameObjectHandle*)(dataPointer);
	dataPointer += sizeof(startHandle);

	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
	{
		TerrainPatch& patch = *GetPatch(x,y);

		// read in the tile data
		const int dataSize = sizeof(TerrainTile) * patchSize * patchSize * patchLayers;
		memcpy(patch.tiles, dataPointer, dataSize);
		dataPointer += dataSize;

		// read in the object stubs
		unsigned int stubCount = *(unsigned int*)(dataPointer);
		dataPointer += sizeof(unsigned int);
		for (unsigned int i = 0; i < stubCount; ++i) 
		{
			GameObjectStub stub;
			stub.type = *(GameObjectType*)(dataPointer); dataPointer += sizeof(GameObjectType);
			stub.xf = *(XForm2*)(dataPointer); dataPointer += sizeof(XForm2);
			stub.size = *(Vector2*)(dataPointer); dataPointer += sizeof(Vector2);
			stub.handle = *(GameObjectHandle*)(dataPointer); dataPointer += sizeof(GameObjectHandle);

			// hack: cap small stub sizes
			if (fabs(stub.size.x) < 0.1f)
				stub.size.x = 0.1f;
			if (fabs(stub.size.y) < 0.1f)
				stub.size.y = 0.1f;
			
			char* attributes = NULL;
			int attributesLength = 0;
			attributesLength = *(int*)(dataPointer); dataPointer += sizeof(int);
			attributes = (char*)(dataPointer); dataPointer += attributesLength;
			strncpy_s(stub.attributes, attributes, sizeof(stub.attributes));
			
			patch.AddStub(stub);
		}
	}
	UnlockResource(hMem);
	FreeResource(hRes);
	ResetStartHandle(startHandle);
	return true;
}

void Terrain::Clear()
{
	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
	{
		TerrainPatch& patch = *GetPatch(x,y);
		patch.Deactivate();
		patch.Clear();
	}

	ResetStartHandle(firstStartHandle);
}

IntVector2 Terrain::GetTileIndex(const Vector2& pos) const
{
	Vector2 index = (pos - GetPosWorld()) / (TerrainTile::GetSize());
	return IntVector2((int)floorf(index.x), (int)floorf(index.y));
}

Vector2 Terrain::GetTileCenter(const Vector2& pos) const
{
	IntVector2 tileIndex = GetTileIndex(pos);
	return GetTileCenter(tileIndex.x, tileIndex.y);
}

Vector2 Terrain::GetTileCenter(int x, int y) const
{
	Vector2 tilePos = GetTilePos(x, y);
	return tilePos + Vector2(TerrainTile::GetSize()/2);
}

TerrainTile* Terrain::GetTile(const Vector2& pos, int& x, int& y, int layer) const
{
	const IntVector2 tileIndex = GetTileIndex(pos);

	if (tileIndex.x < 0 || tileIndex.y < 0 || tileIndex.x >= patchSize*fullSize.x || tileIndex.y >= patchSize*fullSize.y)
		return NULL;

	x = tileIndex.x;
	y = tileIndex.y;
	return GetTile(x, y, layer);
}

TerrainTile* Terrain::GetTile(int x, int y, int layer) const
{
	// seperate patch and tile index
	const int patchX = x / patchSize;
	const int patchY = y / patchSize;

	const int tileX = x - patchX*patchSize;
	const int tileY = y - patchY*patchSize;

	if (patchX < 0 || patchX >= fullSize.x || patchY < 0 || patchY >= fullSize.y)
		return NULL;
	if (tileX < 0 || tileX >= patchSize || tileY < 0 || tileY >= patchSize)
		return NULL;

	return &GetPatch(patchX, patchY)->GetTileLocal(tileX, tileY, layer);
}

TerrainTile* Terrain::GetTile(const Vector2& pos, int layer) const
{
	int x, y;
	return GetTile(pos, x, y, layer);
}

int Terrain::GetSurfaceSide(const Vector2& pos, int layer) const
{
	int x, y;
	TerrainTile* tile = GetTile(pos, x, y, layer);
	if (tile)
	{
		const Vector2 offset = pos - GetTilePos(x, y);
		return tile->GetSurfaceSide(offset);
	}
	return false;
}

BYTE Terrain::GetSurfaceIndex(const Vector2& pos, int layer) const
{
	int x, y;
	TerrainTile* tile = GetTile(pos, x, y, layer);
	if (!tile)
	return 0;

	const Vector2 offset = pos - GetTilePos(x, y);
	return tile->GetSurfaceData(offset);
}

int Terrain::SetSurfaceIndex(const Vector2& pos, BYTE surface, int layer)
{
	int x, y;
	TerrainTile* tile = GetTile(pos, x, y, layer);
	if (!tile)
		return false;

	const Vector2 offset = pos - GetTilePos(x, y);
	int side = tile->GetSurfaceSide(offset);
	tile->SetSurfaceData(side, surface);
	return side;
}

TerrainTile* Terrain::GetConnectedTileA(int x, int y, int &x2, int &y2, int layer)
{
	const TerrainTile* tile = GetTile(x, y, layer);
	if (!tile || tile->IsClear() || tile->IsFull())
		return NULL;

	BYTE xa, ya;
	tile->GetXYA(xa, ya);

	TerrainTile* tileNeighbor = NULL;

	if (xa == 0 && ya == 0 || xa == 0 && ya == 4 || xa == 4 && ya == 0 || xa == 4 && ya == 4)
	{
		int xOffset = 0;
		int yOffset = 0;
		if (xa == 0 && ya == 0)
			yOffset = -1;
		else if (xa == 4 && ya == 4)
			yOffset = 1;
		else if (xa == 0 && ya == 4)
			xOffset = -1;
		else if (xa == 4 && ya == 0)
			xOffset = 1;
		
		// check around the corner until we find a connection
		for (int i = 0; i < 3; ++i)
		{
			x2 = x + xOffset;
			y2 = y + yOffset;
			tileNeighbor = GetTile(x2, y2);
			if (tileNeighbor && GameSurfaceInfo::NeedsEdgeCollision(*tileNeighbor))
			{
				if (TerrainTile::IsConnectedAB(*tile, *tileNeighbor, xOffset, yOffset))
					return tileNeighbor;
			}
			TerrainTile::RotateOffset(xOffset, yOffset, 1);

		}
		return NULL;
	}

	int xOffset = 0;
	int yOffset = 0;
	if (xa == 0)
		xOffset = -1;
	else if (xa == 4)
		xOffset = 1;
	else if (ya == 0)
		yOffset = -1;
	else if (ya == 4)
		yOffset = 1;

	x2 = x + xOffset;
	y2 = y + yOffset;
	tileNeighbor = GetTile(x2, y2);
	if (tileNeighbor && GameSurfaceInfo::NeedsEdgeCollision(*tileNeighbor))
	{
		if (!TerrainTile::IsConnectedAB(*tile, *tileNeighbor, xOffset, yOffset))
			tileNeighbor = NULL;
	}

	return tileNeighbor;

}

TerrainTile* Terrain::GetConnectedTileB(int x, int y, int &x2, int &y2, int layer)
{
	const TerrainTile* tile = GetTile(x, y, layer);
	if (!tile || tile->IsClear() || tile->IsFull())
		return NULL;

	BYTE xb, yb;
	tile->GetXYB(xb, yb);

	TerrainTile* tileNeighbor = NULL;

	if (xb == 0 && yb == 0 || xb == 0 && yb == 4 || xb == 4 && yb == 0 || xb == 4 && yb == 4)
	{
		int xOffset = 0;
		int yOffset = 0;
		if (xb == 0 && yb == 0)
			xOffset = -1;
		else if (xb == 4 && yb == 4)
			xOffset = 1;
		else if (xb == 0 && yb == 4)
			yOffset = 1;
		else if (xb == 4 && yb == 0)
			yOffset = -1;
		
		// check around the corner until we find a connection
		for (int i = 0; i < 3; ++i)
		{
			x2 = x + xOffset;
			y2 = y + yOffset;
			tileNeighbor = GetTile(x2, y2);
			if (tileNeighbor && GameSurfaceInfo::NeedsEdgeCollision(*tileNeighbor))
			{
				if (TerrainTile::IsConnectedAB(*tileNeighbor, *tile, xOffset, yOffset))
					return tileNeighbor;
			}
			TerrainTile::RotateOffset(xOffset, yOffset, -1);

		}
		return NULL;
	}

	int xOffset = 0;
	int yOffset = 0;
	if (xb == 0)
		xOffset = -1;
	else if (xb == 4)
		xOffset = 1;
	else if (yb == 0)
		yOffset = -1;
	else if (yb == 4)
		yOffset = 1;

	x2 = x + xOffset;
	y2 = y + yOffset;
	tileNeighbor = GetTile(x2, y2);
	if (tileNeighbor && GameSurfaceInfo::NeedsEdgeCollision(*tileNeighbor))
	{
		if (!TerrainTile::IsConnectedAB(*tileNeighbor, *tile, xOffset, yOffset))
			tileNeighbor = NULL;
	}

	return tileNeighbor;
}

bool Terrain::Deform(const Vector2& pos, float radius, GameMaterialIndex gmi, float randomness)
{
	list<GameMaterialIndex> gmiList;
	gmiList.push_back(gmi);
	return Deform(pos, radius, gmiList, randomness);
}
	
bool Terrain::Deform(const Vector2& pos, float radius, const list<GameMaterialIndex>& gmiList, float randomness)
{
	int centerX, centerY;
	TerrainTile* tile = GetTile(pos, centerX, centerY, 1);

	const float tileSize = TerrainTile::GetSize();
	if (terrainDebug)
		Circle(pos, radius).RenderDebug(Color::Red(0.5f), 2);

	const int tileRadius = int(1 + radius / tileSize);
	
	// update destroyed tiles in the map
	MiniMap* gameMap = deformUpdateMap ? g_gameControlBase->GetMiniMap() : NULL;
	MiniMap::MapRenderBlock mapRenderBlock(gameMap ? gameMap->GetMiniMapTexture() : NULL, false);

	bool destroyedAnything = false;
	for(int i=-tileRadius; i <= tileRadius; ++i)
	for(int j=-tileRadius; j <= tileRadius; ++j)
	{
		const int x = centerX + i;
		const int y = centerY + j;

		Vector2 tilePos = GetTilePos(x, y);
		TerrainPatch* patch = GetPatch(tilePos);
		TerrainTile* tile = GetTile(tilePos, 0);
		if (!patch || !tile)
			continue;
		
		bool needsRebuild = false;

		if (tile->IsAreaClear())
			continue;
		
		const Vector2 localCenterPos = pos - tilePos;

		// try to resurface
		const BYTE surfaceIndex0 = tile->GetSurfaceData(0);
		const BYTE surfaceIndex1 = tile->GetSurfaceData(1);
		const GameSurfaceInfo& surfaceInfo0 = GameSurfaceInfo::Get(surfaceIndex0);
		const GameSurfaceInfo& surfaceInfo1 = GameSurfaceInfo::Get(surfaceIndex1);
		const GameMaterialIndex materialIndex0 = surfaceInfo0.materialIndex;
		const GameMaterialIndex materialIndex1 = surfaceInfo1.materialIndex;
		bool surface0IsDestrutible = terrainAlwaysDestructible || surfaceInfo0.IsDestructible();
		bool surface1IsDestrutible = terrainAlwaysDestructible || surfaceInfo1.IsDestructible();

		if (!terrainAlwaysDestructible)
		{
			// check if destrutible surface is in the gmi list
			if (surface0IsDestrutible)
			{
				if (std::find(gmiList.begin(), gmiList.end(), materialIndex0) == gmiList.end())
					surface0IsDestrutible = false;
			}
			if (surface1IsDestrutible)
			{
				if (std::find(gmiList.begin(), gmiList.end(), materialIndex1) == gmiList.end())
					surface1IsDestrutible = false;
			}
		}

		bool wasDestroyed = false;
		if (!surface0IsDestrutible && tile->GetSurfaceHasArea(0) || !surface1IsDestrutible && tile->GetSurfaceHasArea(1))
		{
			// one of the two surfaces is not destructable
			// just get rid of whatevers destructable
			if (surface0IsDestrutible && tile->GetSurfaceHasArea(0))
			{
				tile->SetSurfaceData(0,0);
				wasDestroyed = true;
			}
			else if (surface1IsDestrutible && tile->GetSurfaceHasArea(1))
			{
				tile->SetSurfaceData(1,0);
				wasDestroyed = true;
			}
		}
		else if (!tile->HasFullCollision())
		{
			// both materials are destructible but tile isn't full, just clear it
			tile->MakeClear();
			wasDestroyed = true;	
		}
		else if (tile->Resurface(localCenterPos, radius, randomness))
		{
			// prevent tiles that are too small
			const float minAreaPercent = 0.4f;
			const float area = tile->GetSurfaceArea(1);
			if (area < minAreaPercent)
				tile->MakeClear();
			else
				tile->SetSurfaceData(0,0);
			wasDestroyed = true;
		}

		if (!wasDestroyed)
			continue;
				
		const GameSurfaceInfo& destroyedSurfaceInfo = surfaceInfo0.IsDestructible() ? surfaceInfo0 : surfaceInfo1;
		const Vector2 tileCenter = tilePos + Vector2(tileSize / 2);
		g_gameControlBase->TerrainDeformTileCallback(tileCenter, destroyedSurfaceInfo);

		destroyedAnything = true;
		if (terrainDebug)
			Box2AABB(tilePos, tilePos + Vector2(tileSize)).RenderDebug(Color::Red(0.8f), 2);

		// only render tiles that have changed
		if (gameMap)
		{
			TerrainTile* tile1 = GetTile(tilePos, 1);
			if (tile1)
				gameMap->RenderTileToTexture(*tile1, tilePos, 1);
			gameMap->RenderTileToTexture(*tile, tilePos, 0);
		}
		patch->RebuildPhysics();
	}

	if (!destroyedAnything)
		return false;

	if (g_gameControlBase->IsEditMode())
		g_editor.SaveState();

	if (gameMap)
		gameMap->SetMapDirty();

	return true;
}

GameMaterialIndex Terrain::DeformTile(const Vector2& startPos, const Vector2& direction, const GameObject* ignoreObject, GameMaterialIndex gmi, bool clear)
{
	const float distance = 0.1f;

	ASSERT(direction.IsNormalized());
	const Line2 testLine(startPos - distance*direction, startPos + distance*direction);
	if (terrainDebug)
	{
		(startPos - distance*direction).RenderDebug(Color::Green(0.5f), 0.1f, 2);
		testLine.RenderDebug(Color::Red(0.5f), 2);
		startPos.RenderDebug(Color::Red(0.5f), 0.1f, 2);
	}

	SimpleRaycastResult raycastResult;
	GameObject* hitObject = g_physics->RaycastSimple(testLine, &raycastResult, ignoreObject);
	if (!hitObject || !hitObject->IsTerrain() || !raycastResult.hitFixture)
		return GMI_Invalid;
	
	// shift result inside thet tile a little bit
	Vector2 pos = raycastResult.point + 0.01f * (testLine.p2 - testLine.p1).Normalize();
	if (terrainDebug)
		pos.RenderDebug(Color::Blue(0.5f), 0.1f, 2);

	int x, y;
	TerrainTile* tile = GetTile(pos, x, y);
	if (!tile || tile->IsAreaClear())
		return GMI_Invalid;
	
	const int surfaceData = (int)(raycastResult.hitFixture->GetUserData());
	const GameSurfaceInfo& gsi = GameSurfaceInfo::Get(surfaceData);
	const GameMaterialIndex gmiHit = gsi.materialIndex;
	if (!terrainAlwaysDestructible && (gsi.materialIndex != gmi || !gsi.IsDestructible()))
		return gmiHit;

	// update destroyed tiles in the map
	MiniMap* gameMap = deformUpdateMap? g_gameControlBase->GetMiniMap() : NULL;
	MiniMap::MapRenderBlock mapRenderBlock(gameMap ? gameMap->GetMiniMapTexture() : NULL, false);

	const Vector2 tilePos = GetTilePos(x, y);
	const Vector2 tileCenter = tilePos + Vector2(TerrainTile::GetSize() / 2);
	TerrainPatch* patch = GetPatch(pos);
	if (!patch)
		return gmiHit;
	
	bool destroyedAnything = false;
	if (clear)
	{
		const int side = GetSurfaceSide(pos);
		const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(tile->GetSurfaceData(side));
		if (terrainAlwaysDestructible || surfaceInfo.materialIndex == gmi && surfaceInfo.IsDestructible())
		{
			g_gameControlBase->TerrainDeformTileCallback(tileCenter, surfaceInfo);
			tile->SetSurfaceData(side, 0);
			destroyedAnything = true;
		}
	}
	else
	{
		destroyedAnything = true;
		tile->MakeSmaller(pos - tilePos, direction);
			
		// clear the tile if there is only a tiny bit left
		const float minAreaPercent = 0.4f;
		const GameSurfaceInfo& tile0Info = GameSurfaceInfo::Get(tile->GetSurfaceData(0));
		const GameSurfaceInfo& tile1Info = GameSurfaceInfo::Get(tile->GetSurfaceData(1));
		if ((terrainAlwaysDestructible || tile0Info.materialIndex == gmi && tile0Info.IsDestructible()) && !tile1Info.HasCollision() && tile->GetSurfaceArea(0) < minAreaPercent)
		{
			g_gameControlBase->TerrainDeformTileCallback(tileCenter, tile0Info);
			tile->MakeClear();
		}
		else if ((terrainAlwaysDestructible || tile1Info.materialIndex == gmi && tile1Info.IsDestructible()) && !tile0Info.HasCollision() && tile->GetSurfaceArea(1) < minAreaPercent)
		{
			g_gameControlBase->TerrainDeformTileCallback(tileCenter, tile1Info);
			tile->MakeClear();
		}
		else
			g_gameControlBase->TerrainDeformTileCallback(tileCenter, gsi);
	}

	if (!destroyedAnything)
		return gmiHit;

	if (gameMap)
	{
		// only render tiles that have changed
		const Vector2 tileOffset = GetTilePos(x, y);
		TerrainTile* tile1 = GetTile(tileOffset, 1);
		if (tile1)
			gameMap->RenderTileToTexture(*tile1, tileOffset, 1);
		gameMap->RenderTileToTexture(*tile, tileOffset, 0);
		gameMap->SetMapDirty();
	}

	patch->RebuildPhysics();
	return gmiHit;
}

GameObjectStub* Terrain::FindNextStubByType(GameObjectType type, GameObjectStub* lastStub)
{
	// search for matching stub
	bool foundLastStub = (lastStub == NULL);
	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
	{
		TerrainPatch* patch = patches[x + fullSize.x * y];
		for (list<GameObjectStub>::iterator it = patch->objectStubs.begin(); it != patch->objectStubs.end(); ++it) 
		{       
			GameObjectStub& stub = *it;
			if (stub.type != type)
				continue;

			if (&stub == lastStub)
				foundLastStub = true;
			else if (foundLastStub)
			{
				return &stub;
			}
		}
	}
	return NULL;
}

GameObjectStub* Terrain::GetStub(GameObjectHandle handle, TerrainPatch** returnPatch)
{
	if (handle <= 0)
		return NULL;

	// search for matching stub
	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
	{
		TerrainPatch* patch = patches[x + fullSize.x * y];
		GameObjectStub* stub = patch->GetStub(handle);
		if (!stub)
			continue;
		
		if (returnPatch)
			*returnPatch = patch;
		return stub;
	}
	return NULL;
}

GameObjectStub* Terrain::GetStub(const Vector2& pos, TerrainPatch** returnPatch)
{
	GameObjectStub* bestStub = NULL;
	float bestDistance = FLT_MAX;

	// pick out the stub that we are nearest to the edge of, so we can pick out overlapping stubs
	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
	{
		TerrainPatch* patch = patches[x + fullSize.x * y];
		list<GameObjectStub>& objectStubs = patch->GetStubs();
		for (list<GameObjectStub>::iterator it = objectStubs.begin(); it != objectStubs.end(); ++it) 
		{       
			GameObjectStub& stub = *it;

			const Vector2 stubSelectSize = stub.GetStubSelectSize();
			if (stubSelectSize.x == 0 && stubSelectSize.y == 0)
			{
				if ((stub.xf.position - pos).LengthSquared() > Square(ObjectEditor::zeroStubRadius))
					continue;
			}
			else if (stubSelectSize.x < 0 || stubSelectSize.y < 0)
				continue;
			else if (!Vector2::InsideBox(stub.xf, stub.GetStubSelectSize(), pos))
				continue;

			float distance = FLT_MAX;
			if (stubSelectSize.x == 0 && stubSelectSize.y == 0)
			{
				Vector2 deltaPos = stub.xf.position - pos;
				distance = ObjectEditor::zeroStubRadius - deltaPos.Length();
			}
			else
			{
				const Vector2 posLocal = stub.xf.Inverse().TransformCoord(pos);
				distance = Min(stub.size.x - fabs(posLocal.x), stub.size.y - fabs(posLocal.y));
			}
			if (distance > bestDistance)
				continue;

			bestDistance = distance;
			bestStub = &stub;
			if (returnPatch)
				*returnPatch = patch;
		}
	}
	return bestStub;
}

bool Terrain::RemoveStub(GameObjectHandle handle, TerrainPatch* patch)
{
	if (patch && patch->RemoveStub(handle))
		return true;

	// check every patch
	for(int x=0; x<fullSize.x; ++x)
	for(int y=0; y<fullSize.y; ++y)
	{
		if (patches[x + fullSize.x * y]->RemoveStub(handle))
			return true;
	}
	return false;
}

void Terrain::CheckForErrors()
{
	// check for duplicate handles
	for(int x=0; x<Terrain::fullSize.x; ++x)
	for(int y=0; y<Terrain::fullSize.y; ++y)
	{
		TerrainPatch& patch = *GetPatch(x,y);
		for (GameObjectStub& stub : patch.objectStubs) 
		{
			ASSERT(stub.handle != 0);
			for(int i=0; i<Terrain::fullSize.x; ++i)
			for(int j=0; j<Terrain::fullSize.y; ++j)
			{
				TerrainPatch& patch2 = *(GetPatch(i,j));

				// save out the object stubs
				for (GameObjectStub& stub2 : patch2.objectStubs) 
				{
					if (stub.handle > GameObject::GetNextUniqueHandleValue())
					{
						ASSERT(false); // stub has handle greater it should
						stub.handle = GameObject::GetNextUniqueHandleValue();
						GameObject::SetNextUniqueHandleValue(stub.handle + 1);
					}
					if (stub.handle == stub2.handle && &stub != &stub2)
					{
						ASSERT(false); // duplicate handles should not happen
						stub.handle = GameObject::GetNextUniqueHandleValue();
						GameObject::SetNextUniqueHandleValue(stub.handle + 1);
					}
				}
			}
		}
	}
}

void Terrain::CleanUpTiles()
{
	// check for duplicate handles
	for(int l=0; l<Terrain::patchLayers; ++l)
	for(int x=0; x<Terrain::fullSize.x * Terrain::patchSize; ++x)
	for(int y=0; y<Terrain::fullSize.y * Terrain::patchSize; ++y)
	{
		TerrainTile* tile = GetTile(x, y, l);
		if (!tile)
			continue;
		
		if (tile->IsAreaClear())
			tile->MakeClear();
		else if (!tile->IsFull())
		{
			if (!tile->GetSurfaceHasArea(1))
			{
				tile->MakeFull();
				tile->SetSurfaceData(1, tile->GetSurfaceData(0));
			}
			else if (!tile->GetSurfaceHasArea(0))
			{
				tile->MakeFull();
			}
		}
		if (tile->IsFull())
			tile->SetSurfaceData(0, 0);
	}
	
	g_editor.SaveState();
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Terrain Patch
*/
////////////////////////////////////////////////////////////////////////////////////////

// Terrain patch constructor
// note: terrain patches are not be added to the world!
TerrainPatch::TerrainPatch(const Vector2& pos) :
	GameObject(pos, NULL, GameObjectType(0), false),
	activePhysics(false),
	activeObjects(false),
	needsPhysicsRebuild(false)
{
	tiles = new TerrainTile[Terrain::patchLayers * Terrain::patchSize * Terrain::patchSize];

	Clear();

	// terrain layer render handles rendering
	SetVisible(false);
}

TerrainPatch::~TerrainPatch()
{
	delete [] tiles;
}

void TerrainPatch::Clear()
{
	Deactivate();
	ClearTileData();
	ClearObjectStubs();
}

void TerrainPatch::ClearTileData(int layer)
{
	ASSERT(layer < Terrain::patchLayers);

	// wipe out all the tile data
	for(int x=0; x<Terrain::patchSize; ++x)
	for(int y=0; y<Terrain::patchSize; ++y)
	{
		// reset tiles
		GetTileLocal(x, y, layer).MakeClear();
	}
}
	

void TerrainPatch::ClearTileData()
{
	// wipe out all the tile data
	for(int x=0; x<Terrain::patchSize; ++x)
	for(int y=0; y<Terrain::patchSize; ++y)
	for(int l=0; l<Terrain::patchLayers; ++l)
	{
		// reset tiles
		GetTileLocal(x, y, l).MakeClear();
	}
}

void TerrainPatch::ClearObjectStubs()
{
	// clear the object stub list
	objectStubs.clear();
}

void TerrainPatch::SetActivePhysics(bool _activePhysics)
{
	if (activePhysics == _activePhysics)
		return;
	activePhysics = _activePhysics;

	if (activePhysics)
	{
		ASSERT(!GetPhysicsBody());
		if (Terrain::usePolyPhysics)
			CreatePolyPhysicsBody();
		else
			CreateEdgePhysicsBody();
	}
	else
	{
		ASSERT(GetPhysicsBody());
		DestroyPhysicsBody();
	}
}

void TerrainPatch::SetActiveObjects(bool _activeObjects, bool windowMoved)
{
	if (_activeObjects && !activeObjects || windowMoved)
	{
		// update serialize objects when window moves or patch first becomes active
		const Box2AABB streamWindowAABB = g_terrain->GetStreamWindow();
		for (list<GameObjectStub>::iterator it = objectStubs.begin(); it != objectStubs.end(); ) 
		{       
			const GameObjectStub& stub = *it;
			list<GameObjectStub>::iterator itLast = it;
			++it;

			// check if it already exists
			GameObject* object = g_objectManager.GetObjectFromHandle(stub.handle);
			if (object)
			{
				object->StreamIn();
				continue;
			}

			const ObjectTypeInfo& objectInfo = stub.GetObjectInfo();
			if (!objectInfo.IsSerializable())
				continue;

			// only load seralizable if fully in the stream window
			if (Terrain::enableStreaming && !streamWindowAABB.FullyContains(stub.GetAABB()))
				continue;

			// create the object from the stub
			stub.BuildObject();
			
			// seralizeable objects are removed as they are spawned
			objectStubs.erase(itLast);
		}
	}

	if (activeObjects == _activeObjects)
		return;
	activeObjects = _activeObjects;

	if (activeObjects)
	{
		const Box2AABB streamWindowAABB = g_terrain->GetStreamWindow();

		// create all the objects in the stub list
		for (list<GameObjectStub>::iterator it = objectStubs.begin(); it != objectStubs.end(); ++it) 
		{       
			const GameObjectStub& stub = *it;

			// check if it already exists
			if (g_objectManager.GetObjectFromHandle(stub.handle))
				continue;

			// serializible objecs update every time stream window moves
			const ObjectTypeInfo& objectInfo = stub.GetObjectInfo();
			if (objectInfo.IsSerializable())
				continue;

			// create the object from the stub
			stub.BuildObject();
		}

		// do tile create callbacks
		for(int x=0; x<Terrain::patchSize; ++x)
		for(int y=0; y<Terrain::patchSize; ++y)
		{
			for(int l=0; l<Terrain::patchLayers; ++l)
			{
				const TerrainTile& tile = GetTileLocal(x, y, l);
				const GameSurfaceInfo& tile0Info = GameSurfaceInfo::Get(tile.GetSurfaceData(0));
				const GameSurfaceInfo& tile1Info = GameSurfaceInfo::Get(tile.GetSurfaceData(1));
				const Vector2 tileOffset = TerrainTile::GetSize() * Vector2((float)x, (float)y);

				// call the tile create callbacks if it has any
				if (tile0Info.tileCreateCallback && tile.GetSurfaceHasArea(0))
					(tile0Info.tileCreateCallback)(tile0Info, GetPosWorld() + tileOffset + 0.5f*Vector2(TerrainTile::GetSize()), l);
				if (tile1Info.tileCreateCallback && tile.GetSurfaceHasArea(1))
					(tile1Info.tileCreateCallback)(tile1Info, GetPosWorld() + tileOffset + 0.5f*Vector2(TerrainTile::GetSize()), l);
			}
		}
	}
}

bool TerrainPatch::GetTileLocalIsSolid(int x, int y) const
{
	const TerrainTile& tile = GetTileLocal(x, y, Terrain::physicsLayer);

	if (tile.GetSurfaceHasArea(false) && GameSurfaceInfo::Get(tile.GetSurfaceData(false)).HasCollision())
		return true;
	
	if (tile.GetSurfaceHasArea(true) && GameSurfaceInfo::Get(tile.GetSurfaceData(true)).HasCollision())
		return true;
		
	return false;
}

void TerrainPatch::CreateEdgePhysicsBody()
{
	ASSERT(!GetPhysicsBody());
	ASSERT(!HasParent());
	ASSERT(!HasPhysics());
	ASSERT(g_terrain);
	
	needsPhysicsRebuild = false;
	GameObject::CreatePhysicsBody(b2_staticBody);

	for(int x=0; x<Terrain::patchSize; ++x)
	for(int y=0; y<Terrain::patchSize; ++y)
	{
		const TerrainTile& tile = GetTileLocal(x, y, Terrain::physicsLayer);
		if (tile.IsClear() || tile.IsFull() || !GameSurfaceInfo::NeedsEdgeCollision(tile))
			continue;
		
		// check if tile set disabled collision
		if (!Terrain::GetTileSetHasCollision(tile.GetTileSet()))
			continue;

		// protect against creating way too many proxies
		const int proxyCount = g_physics->GetPhysicsWorld()->GetProxyCount();
		if (proxyCount > Terrain::maxProxies)
			break;

		const GameSurfaceInfo& tile0Info = GameSurfaceInfo::Get(tile.GetSurfaceData(0));
		const GameSurfaceInfo& tile1Info = GameSurfaceInfo::Get(tile.GetSurfaceData(1));
		const Vector2 tileOffset = TerrainTile::GetSize() * Vector2((float)x, (float)y);

		{
			// create edge shape terrain
			b2EdgeShape shapeDef;
			shapeDef.Set(tile.GetPosA() + tileOffset, tile.GetPosB() + tileOffset);
		
			const BYTE surfaceData = (tile0Info.HasCollision() && tile.GetSurfaceHasArea(0))? tile.GetSurfaceData(0) : tile.GetSurfaceData(1);
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surfaceData);

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &shapeDef;
			fixtureDef.userData = (void*)(surfaceData);
			fixtureDef.friction = surfaceInfo.friction >= 0 ? surfaceInfo.friction : Terrain::friction;
			fixtureDef.restitution = surfaceInfo.restitution >= 0 ? surfaceInfo.restitution : Terrain::restitution;
			AddFixture(fixtureDef);
		}
	}
}

b2PolygonShape TerrainPatch::BuiltTileShape(BYTE edgeByte, const Vector2& offset)
{
	const Vector2 *edgeVerts = NULL;
	int vertexCount = 0;
	TerrainTile::GetVertList(edgeVerts, vertexCount, edgeByte);

	// add offset
	b2Vec2 verts[TERRAIN_TILE_MAX_VERTS];
	ASSERT(vertexCount <= TERRAIN_TILE_MAX_VERTS);
	for (int i = 0; i < vertexCount; ++i)
		verts[i] = edgeVerts[i] + offset;

	b2PolygonShape shapeDef;
	shapeDef.Set(verts, vertexCount);
	return shapeDef;
}

void TerrainPatch::CreatePolyPhysicsBody()
{
	ASSERT(!GetPhysicsBody());
	ASSERT(!HasParent());
	ASSERT(!HasPhysics());
	ASSERT(g_terrain);
	
	needsPhysicsRebuild = false;
	GameObject::CreatePhysicsBody(b2_staticBody);
	
	bool* solidTileArray = static_cast<bool*>(malloc(sizeof(bool) * Terrain::patchSize * Terrain::patchSize));
	for(int x=0; x<Terrain::patchSize; ++x)
	for(int y=0; y<Terrain::patchSize; ++y)
		solidTileArray[x + y*Terrain::patchSize] = false;
	
	for(int y=0; y<Terrain::patchSize; ++y)
	for(int x=0; x<Terrain::patchSize; ++x)
	{
		TerrainTile tile = GetTileLocal(x, y, Terrain::physicsLayer);
		if (tile.IsClear())
			continue;
		
		// check if tile set disabled collision
		if (!Terrain::GetTileSetHasCollision(tile.GetTileSet()))
			continue;

		bool& solidTileCheck = solidTileArray[x + y*Terrain::patchSize];
		if (solidTileCheck)
			continue;

		// protect against creating way too many proxies
		const int proxyCount = g_physics->GetPhysicsWorld()->GetProxyCount();
		if (proxyCount > Terrain::maxProxies)
			break;

		if (tile.HasFullCollision() && Terrain::combineTileShapes)
		{
			const int fullCollisionSurface = tile.GetSurfaceHasArea(0) ? 0 : 1;
			const GameSurfaceInfo& tileInfo = GameSurfaceInfo::Get(tile.GetSurfaceData(fullCollisionSurface));
			const GameMaterialIndex materialIndex = tileInfo.materialIndex;

			// try to create large block for connected tiles
			int right = Terrain::patchSize;
			int bottom = Terrain::patchSize;

			// get the width first
			for(int x2=x+1; x2<right; ++x2)
			{
				const TerrainTile& tile2 = GetTileLocal(x2, y, Terrain::physicsLayer);
				bool hasFullCollision = tile2.HasFullCollision() && Terrain::GetTileSetHasCollision(tile2.GetTileSet());
				const int fullCollisionSurface2 = tile2.GetSurfaceHasArea(0) ? 0 : 1;
				const GameSurfaceInfo& tile2Info = GameSurfaceInfo::Get(tile2.GetSurfaceData(fullCollisionSurface2));
				bool samePhysics = tile2Info.HasSamePhysics(tileInfo);
				if (!hasFullCollision || !samePhysics || tile2Info.materialIndex != materialIndex || solidTileArray[x2 + y*Terrain::patchSize])
				{
					right = x2;
					break;
				}
			}

			// get the height
			bool foundBottom = false;
			for(int y2=y+1; y2<bottom && !foundBottom; ++y2)
			for(int x2=x; x2<right; ++x2)
			{
				const TerrainTile& tile2 = GetTileLocal(x2, y2, Terrain::physicsLayer);
				bool hasFullCollision = tile2.HasFullCollision() && Terrain::GetTileSetHasCollision(tile2.GetTileSet());
				const int fullCollisionSurface2 = tile2.GetSurfaceHasArea(0) ? 0 : 1;
				const GameSurfaceInfo& tile2Info = GameSurfaceInfo::Get(tile2.GetSurfaceData(fullCollisionSurface2));
				bool samePhysics = tile2Info.HasSamePhysics(tileInfo);
				if (!hasFullCollision || !samePhysics || tile2Info.materialIndex != materialIndex)
				{
					foundBottom = true;
					bottom = y2;
					break;
				}
			}
			
			// mark off solid areas
			for(int x2=x; x2<right; ++x2)	
			for(int y2=y; y2<bottom; ++y2)
				solidTileArray[x2 + y2*Terrain::patchSize] = true;

			// create a box to fit that size
			b2PolygonShape shapeDef;
			const float width  = (right  - x)*0.5f*TerrainTile::GetSize();
			const float height = (bottom - y)*0.5f*TerrainTile::GetSize();
			const Vector2 center = TerrainTile::GetSize() * (Vector2(float(x), float(y)) + Vector2(width, height));
			shapeDef.SetAsBox(width, height, center, 0);
			const GameSurfaceInfo& tile0Info = GameSurfaceInfo::Get(tile.GetSurfaceData(0));
			const GameSurfaceInfo& tile1Info = GameSurfaceInfo::Get(tile.GetSurfaceData(1));
			const BYTE surfaceData = (tile0Info.HasCollision() && tile.GetSurfaceHasArea(0)) ? tile.GetSurfaceData(0) : tile.GetSurfaceData(1);
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surfaceData);

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &shapeDef;
			fixtureDef.userData = (void*)(surfaceData);
			fixtureDef.friction = surfaceInfo.friction >= 0 ? surfaceInfo.friction : Terrain::friction;
			fixtureDef.restitution = surfaceInfo.restitution >= 0 ? surfaceInfo.restitution : Terrain::restitution;
			AddFixture(fixtureDef);
			continue;
		}

		const GameSurfaceInfo& tile0Info = GameSurfaceInfo::Get(tile.GetSurfaceData(0));
		const GameSurfaceInfo& tile1Info = GameSurfaceInfo::Get(tile.GetSurfaceData(1));
		const Vector2 tileOffset = TerrainTile::GetSize() * Vector2((float)x, (float)y);

		if (tile.HasFullCollision())
		{
			// use full box
			b2PolygonShape shapeDef = BuiltTileShape(0, tileOffset);
			const BYTE surfaceData = (tile0Info.HasCollision() && tile.GetSurfaceHasArea(0)) ? tile.GetSurfaceData(0) : tile.GetSurfaceData(1);
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surfaceData);

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &shapeDef;
			fixtureDef.userData = (void*)(surfaceData);
			fixtureDef.friction = surfaceInfo.friction >= 0 ? surfaceInfo.friction : Terrain::friction;
			fixtureDef.restitution = surfaceInfo.restitution >= 0 ? surfaceInfo.restitution : Terrain::restitution;
			AddFixture(fixtureDef);
			continue;
		}

		if (tile.GetSurfaceHasArea(0) && tile0Info.HasCollision())
		{
			// use tile vert list to make the collision
			const BYTE edgeData = tile.GetEdgeData();
			const BYTE surfaceData = tile.GetSurfaceData(0);
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surfaceData);
			b2PolygonShape shapeDef = BuiltTileShape(edgeData, tileOffset);

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &shapeDef;
			fixtureDef.userData = (void*)(surfaceData);
			fixtureDef.friction = surfaceInfo.friction >= 0 ? surfaceInfo.friction : Terrain::friction;
			fixtureDef.restitution = surfaceInfo.restitution >= 0 ? surfaceInfo.restitution : Terrain::restitution;
			AddFixture(fixtureDef);
		}
		
		if (tile.GetSurfaceHasArea(1) && tile1Info.HasCollision())
		{
			// use tile vert list to make the collision
			const BYTE edgeData = tile.GetInvertedEdgeData();
			const BYTE surfaceData = tile.GetSurfaceData(1);
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surfaceData);
			b2PolygonShape shapeDef = BuiltTileShape(edgeData, tileOffset);

			b2FixtureDef fixtureDef;
			fixtureDef.shape = &shapeDef;
			fixtureDef.userData = (void*)(surfaceData);
			fixtureDef.friction = surfaceInfo.friction >= 0 ? surfaceInfo.friction : Terrain::friction;
			fixtureDef.restitution = surfaceInfo.restitution >= 0 ? surfaceInfo.restitution : Terrain::restitution;
			AddFixture(fixtureDef);
		}
	}
	free(solidTileArray);
}

// returns true if position is in this terrain patch, false if it is not
// sets x and y to the tile array location
TerrainTile* TerrainPatch::GetTile(int x, int y, int layer)
{
	if (x < 0 || y < 0 || x >= Terrain::patchSize || y >= Terrain::patchSize)
		return NULL;
	
	return &GetTileLocal(x, y, layer);
}

// returns true if position is in this terrain patch, false if it is not
// sets x and y to the tile array location
TerrainTile* TerrainPatch::GetTile(const Vector2& testPos, int& x, int& y, int layer)
{
	const Vector2 pos = testPos - GetPosWorld();
	x = (int)(floorf(pos.x / TerrainTile::GetSize()));
	y = (int)(floorf(pos.y / TerrainTile::GetSize()));

	return GetTile(x, y, layer);
}

TerrainTile* TerrainPatch::GetTile(const Vector2& testPos, int layer)
{
	int x, y;
	return GetTile(testPos, x, y, layer);
}

GameObjectStub* TerrainPatch::GetStub(const Vector2& pos)
{
	GameObjectStub* bestStub = NULL;
	float bestDistance = FLT_MAX;

	// pick out the stub that we are nearest to the edge of, so we can pick out overlapping stubs
	for (list<GameObjectStub>::iterator it = objectStubs.begin(); it != objectStubs.end(); ++it) 
	{       
		GameObjectStub& stub = *it;
		if (stub.TestPosition(pos))
		{
			const Vector2 posLocal = stub.xf.Inverse().TransformCoord(pos);
			const float distance = Min(stub.size.x - fabs(posLocal.x), stub.size.y - fabs(posLocal.y));
			if (distance > bestDistance)
				continue;

			bestDistance = distance;
			bestStub = &stub;
		}
	}
	return bestStub;
}

GameObjectStub* TerrainPatch::GetStub(GameObjectHandle handle)
{
	for (list<GameObjectStub>::iterator it = objectStubs.begin(); it != objectStubs.end(); ++it) 
	{       
		GameObjectStub& stub = *it;
		if (stub.handle == handle)
			return &stub;
	}
	return NULL;
}

bool TerrainPatch::RemoveStub(GameObjectHandle handle)
{
	for (list<GameObjectStub>::iterator it = objectStubs.begin(); it != objectStubs.end(); ++it) 
	{       
		GameObjectStub& stub = *it;
		if (stub.handle != handle)
			continue;

		objectStubs.erase(it);
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////
//
// Helpful console commands
//

ConsoleFunction(saveTerrain)
{
	if (!g_terrain)
		return;

	if (g_terrain->Save(text.c_str()))
		GetDebugConsole().AddFormatted(L"Terrain saved to '%s'", text.c_str());
	else
		GetDebugConsole().AddFormatted(L"Terrain save error");
}

ConsoleFunction(loadTerrain)
{
	if (!g_terrain)
		return;

	if (g_terrain->Load(text.c_str()))
		GetDebugConsole().AddFormatted(L"Terrain loaded from '%s'", text.c_str());
	else
		GetDebugConsole().AddFormatted(L"Terrain load error");
}

ConsoleFunction(clearTerrain)
{
	if (!g_terrain)
		return;

	g_editor.ClearSelection();
	g_terrain->Clear();
	g_editor.SaveState();

	GetDebugConsole().AddLine(L"Terrain cleared.");
}

ConsoleFunction(replaceTile)
{
	if (!g_terrain)
		return;
	
	int oldTileIDStart = -1;
	int oldTileIDEnd = -1;
	int newTileIDStart = -1;
	int oldTileSet = -1;
	int newTileSet = -1;
	swscanf_s(text.c_str(), L"%d %d %d %d %d", &oldTileIDStart, &oldTileIDEnd, &newTileIDStart, &oldTileSet, &newTileSet);

	if (oldTileIDStart == -1 || newTileIDStart == -1)
	{
		GetDebugConsole().AddLine(L"syntax: replaceTile oldTileStart oldTileEnd newTileStart oldTileSet newTileSet");
		return;
	}

	if (oldTileIDEnd == 0)
		oldTileIDEnd = oldTileIDStart;

	int replaceCount = 0;
	for(int x=0; x<Terrain::fullSize.x*Terrain::patchSize; ++x)
	for(int y=0; y<Terrain::fullSize.y*Terrain::patchSize; ++y)
	for(int l=0; l<Terrain::patchLayers; ++l)
	{
		TerrainTile* tile = g_terrain->GetTile(x, y, l);
		const int s1 = tile->GetSurfaceData(0);
		if ((tile->GetTileSet() == oldTileSet || oldTileSet == -1) && s1 >= oldTileIDStart && s1 <= oldTileIDEnd)
		{
			++replaceCount;
			tile->SetSurfaceData(0, s1 + (newTileIDStart - oldTileIDStart));
			if (newTileSet != -1)
				tile->SetTileSet(newTileSet);
		}
		const int s2 = tile->GetSurfaceData(1);
		if ((tile->GetTileSet() == oldTileSet || oldTileSet == -1) && s2 >= oldTileIDStart && s2 <= oldTileIDEnd)
		{
			++replaceCount;
			tile->SetSurfaceData(1, s2 + (newTileIDStart - oldTileIDStart));
			if (newTileSet != -1)
				tile->SetTileSet(newTileSet);
		}
	}

	g_editor.SaveState();
	GetDebugConsole().AddFormatted(L"Replaced %d tiles.", replaceCount);
}

ConsoleFunction(replaceObjectTypes)
{
	if (!g_terrain)
		return;
	
	int oldObjectType = -1;
	int newObjectType = -1;
	swscanf_s(text.c_str(), L"%d %d", &oldObjectType, &newObjectType);

	if (oldObjectType == -1 || newObjectType == -1)
	{
		GetDebugConsole().AddLine(L"syntax: replaceObjectTypes oldObjectType newObjectType");
		return;
	}

	g_editor.GetObjectEditor().ClearSelection();

	int replaceCount = 0;
	for(int x=0; x<Terrain::fullSize.x; ++x)
	for(int y=0; y<Terrain::fullSize.y; ++y)
	{
		TerrainPatch* patch = g_terrain->GetPatch(x, y);
		list<GameObjectStub>& objectStubs = patch->GetStubs();
		for (list<GameObjectStub>::iterator it = objectStubs.begin(); it != objectStubs.end(); ) 
		{
			GameObjectStub& stub = *it;
			++it;

			if (stub.GetObjectInfo().GetType() == oldObjectType)
			{
				++replaceCount;
				if (newObjectType == 0)
					patch->RemoveStub(&stub);
				else
					stub.type = GameObjectType(newObjectType);
			}
			else if (stub.GetObjectInfo().GetType() == newObjectType && newObjectType > 0)
			{
				++replaceCount;
				stub.type = GameObjectType(oldObjectType);
			}
		}
	}

	g_editor.SaveState();
	if (newObjectType == 0)
		GetDebugConsole().AddFormatted(L"Removed %d object types.", replaceCount);
	else
		GetDebugConsole().AddFormatted(L"Replaced %d object types.", replaceCount);
}

ConsoleFunction(terrainCleanUp)
{
	g_terrain->CleanUpTiles();
}

ConsoleFunction(terrainCheckForErrors)
{
	g_terrain->CheckForErrors();
}

ConsoleFunction(terrainExportStubs)
{
	ofstream outFile("stubs.txt", ios::out);
	if (outFile.fail())
		return;

	for(int x=0; x<Terrain::fullSize.x; ++x)
	for(int y=0; y<Terrain::fullSize.y; ++y)
	{
		TerrainPatch* patch = g_terrain->GetPatch(x, y);
		list<GameObjectStub>& objectStubs = patch->GetStubs();
		for (GameObjectStub& stub : objectStubs)
		{
			outFile << stub.handle << " ";
			outFile << stub.type << " ";
			outFile << stub.xf.position.x << " ";
			outFile << stub.xf.position.y << " ";
			outFile << stub.xf.angle << " ";
			outFile << stub.size.x << " ";
			outFile << stub.size.y << " ";
			outFile << stub.attributes << " ";
			outFile << "\n";
		}
	}

	outFile.close();
}

ConsoleFunction(terrainImportStubs)
{
	ifstream inFile("stubs.txt", ios::in);
	if (inFile.fail())
		return;

	for(int x=0; x<Terrain::fullSize.x; ++x)
	for(int y=0; y<Terrain::fullSize.y; ++y)
		g_terrain->GetPatch(x, y)->GetStubs().clear();
	
	int highestHandle = 0;
	while (!inFile.eof())
	{
		char line[1024];
		inFile.getline(line, 1024);
		GameObjectStub stub;
		istringstream ss(line);
		ss >> stub.handle;
		ss >> (int&)stub.type;
		ss >> stub.xf.position.x;
		ss >> stub.xf.position.y;
		ss >> stub.xf.angle;
		ss >> stub.size.x;
		ss >> stub.size.y;
		ss.getline(stub.attributes, stub.attributesLength);
		strncpy_s(stub.attributes, TrimString(stub.attributes).c_str(), stub.attributesLength);
		
		if (stub.handle <= 0)
		{
			if (!inFile.eof())
				GetDebugConsole().AddFormatted(Color::Red(), L"Error: object with 0 handle found\n");
			continue;
		}
		if (stub.GetObjectInfo().GetType() != stub.type)
		{ 
			GetDebugConsole().AddFormatted(Color::Red(), L"Error: object type not recognized for stub with handle %d\n", stub.handle);
			continue;
		}

		TerrainPatch* patch = g_terrain->GetPatch(stub.xf.position);
		if (!patch)
		{ 
			GetDebugConsole().AddFormatted(Color::Red(), L"Error: no patch found for stub with handle %d\n", stub.handle);
			continue;
		}
		patch->AddStub(stub);
		if (int(stub.handle) > highestHandle)
			highestHandle = stub.handle;
	}

	inFile.close();
	g_terrain->ResetStartHandle(highestHandle+1);
}

/*ConsoleFunction(fixOldTerrain)
{
	if (!g_terrain)
		return;
	
	for(int x=0; x<Terrain::fullSize.x*Terrain::patchSize; ++x)
	for(int y=0; y<Terrain::fullSize.y*Terrain::patchSize; ++y)
	for(int l=0; l<Terrain::patchLayers; ++l)
	{
		TerrainTile* tile = g_terrain->GetTile(x, y, l);
		if (!tile->IsFull())
			continue;

		const int s1 = tile->GetSurfaceData(0);
		const int s2 = tile->GetSurfaceData(1);
		tile->SetSurfaceData(0, s2);
		tile->SetSurfaceData(1, s1);
	}

	g_editor.SaveState();
}*/