////////////////////////////////////////////////////////////////////////////////////////
/*
	Terrain Tile Surface
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../terrain/terrainTile.h"

enum GameMaterialIndex;
const GameMaterialIndex GMI_Invalid = GameMaterialIndex(0);
const GameMaterialIndex GMI_Start = GameMaterialIndex(1);
struct GameSurfaceInfo;

enum GameSurfaceInfoFlags
{
	GSI_None				= 0x00,		// no flags
	GSI_Collision			= 0x01,		// build collision for tile
	GSI_Destructible		= 0x02,		// tile can be destroyed
	GSI_ForegroundOcclude	= 0x04,		// forground tile is solid and occlude background
};

typedef void (*TileCreateCallback)(const GameSurfaceInfo& tileInfo, const Vector2& pos, int layer);

struct GameSurfaceInfo
{
	GameSurfaceInfo
	(
		WCHAR *_name = NULL,
		TextureID _ti = Texture_Invalid,
		GameMaterialIndex _materialIndex = GMI_Invalid,
		const Color& _color = Color::White(),
		const Color& _backgroundColor = Color::White(),
		UINT _flags = GSI_Collision,
		const Color& _shadowColor = Color::Black(),
		const Color& _emissiveColor = Color::Black(),
		TileCreateCallback _tileCreateCallback = NULL,
		float _friction = -1,
		float _restitution = -1
	) :
		name(_name),
		ti(_ti),
		materialIndex(_materialIndex),
		color(_color),
		backgroundColor(_backgroundColor),
		flags(_flags),
		shadowColor(_shadowColor),
		emissiveColor(_emissiveColor),
		tileCreateCallback(_tileCreateCallback),
		textureWrapSize(1),
		id(-1),
		friction(_friction),
		restitution(_restitution)
	{}

	static void Init();
	static bool NeedsEdgeCollision(const TerrainTile& tile);

	static int Count() { return surfaceInfoCount; }
	static GameSurfaceInfo& Get(BYTE surfaceIndex) 
	{ 
		ASSERT(gameSurfaceInfoArray);
		return gameSurfaceInfoArray[surfaceIndex]; 
	}

	bool HasCollision() const						{ return (flags & GSI_Collision) != 0; }
	bool IsDestructible() const						{ return (flags & GSI_Destructible) != 0; }
	void SetID(int _id)								{ id = _id; }
	const int GetID() const							{ return id; }
	bool HasSamePhysics(const GameSurfaceInfo& si) const  { return (friction == si.friction) && (restitution == si.restitution); }

	bool IsSameRenderGroup(const GameSurfaceInfo& other) const 
	{ 
		return
		(
			ti == other.ti && 
			color == other.color && 
			backgroundColor == other.backgroundColor && 
			shadowColor == other.shadowColor && 
			emissiveColor == other.emissiveColor && 
			textureWrapSize == other.textureWrapSize
		);
	}
	
	bool operator < (const GameSurfaceInfo& other) const
	{ 
		const GameSurfaceInfo& s1 = *this;
		const GameSurfaceInfo& s2 = other;
			
		if (s1.ti != s2.ti)
			return (s1.ti < s2.ti);
		else if (s1.color != s2.color)
			return (s1.color < s2.color);
		else if (s1.backgroundColor != s2.backgroundColor)
			return (s1.backgroundColor < s2.backgroundColor);
		else if (s1.shadowColor != s2.shadowColor)
			return (s1.shadowColor < s2.shadowColor);
		else if (s1.emissiveColor != s2.emissiveColor)
			return (s1.emissiveColor < s2.emissiveColor);
		else if (s1.textureWrapSize.x != s2.textureWrapSize.x)
			return (s1.textureWrapSize.x < s2.textureWrapSize.x);
		else if (s1.textureWrapSize.y != s2.textureWrapSize.y)
			return (s1.textureWrapSize.y < s2.textureWrapSize.y);
		else
			return false;
	}

public: // data

	WCHAR *name;
	TextureID ti;
	GameMaterialIndex materialIndex;
	Color color;
	Color backgroundColor;
	UINT flags;
	Color shadowColor;
	Color emissiveColor;
	TileCreateCallback tileCreateCallback;
	Vector2 textureWrapSize;
	int id;
	float friction;
	float restitution;

private:
	
	static int surfaceInfoCount;
	static GameSurfaceInfo gameSurfaceInfoArray[TERRAIN_UNIQUE_TILE_COUNT];
};
