////////////////////////////////////////////////////////////////////////////////////////
/*
	Terrain Tile Surface
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../terrain/terrainTile.h"
#include "../terrain/terrainSurface.h"

// count will get autoamtically filled in with the amount of textures on init
int GameSurfaceInfo::surfaceInfoCount = 0;
GameSurfaceInfo  GameSurfaceInfo::gameSurfaceInfoArray[TERRAIN_UNIQUE_TILE_COUNT] = { GameSurfaceInfo() };

bool GameSurfaceInfo::NeedsEdgeCollision(const TerrainTile& tile)
{
	// this only applies to edge loop type terrain physics
	const GameSurfaceInfo& info1 = Get(tile.GetSurfaceData(0));
	const GameSurfaceInfo& info2 = Get(tile.GetSurfaceData(1));

	// we only want collision on tiles where they dont both have collison
	// this will prevent edge data from being genereated on internal areas where
	// nothing physical should be able to get to anyway
	if (info1.flags & GSI_Collision)
		return !(info2.flags & GSI_Collision);
	else
		return info2.flags & GSI_Collision;
}

void GameSurfaceInfo::Init()
{
	{
		// force 0 to always be a clear tile
		GameSurfaceInfo& info = gameSurfaceInfoArray[0];
		info.name = L"Clear Tile";
		info.color = Color::White(0);
		info.backgroundColor = Color::Black();
		info.shadowColor = Color::White(0);
		info.emissiveColor = Color::White(0);
		info.flags = GSI_None;
	}

	// fill in ids
	for(int i=0; i < TERRAIN_UNIQUE_TILE_COUNT; ++i)
		gameSurfaceInfoArray[i].SetID(i);

	for(surfaceInfoCount=0; surfaceInfoCount < TERRAIN_UNIQUE_TILE_COUNT; ++surfaceInfoCount)
	{
		if (gameSurfaceInfoArray[surfaceInfoCount].name == NULL)
			break;
	}

	// fill in defaults for tiles we dont have set up so we can see them in case a loaded terrain file uses them
	for(int i=surfaceInfoCount; i < TERRAIN_UNIQUE_TILE_COUNT; ++i)
		gameSurfaceInfoArray[i] = GameSurfaceInfo();
}
