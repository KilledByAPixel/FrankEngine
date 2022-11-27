////////////////////////////////////////////////////////////////////////////////////////
/*
	Terrain Tile
	Copyright 2013 Frank Force - http://www.frankforce.com

	- code for a single compressed tile into 16 bit structure
	- edge data for each tile is compressed into a byte
	- surface and tile each use a byte
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#define TERRAIN_UNIQUE_TILE_COUNT	(256)	// how many different possible tiles there are
#define TERRAIN_TILE_MAX_VERTS		(6)		// only 6 verts max are needed to form each tile

#include "../terrain/terrain.h"

struct TerrainTile
{
	TerrainTile(BYTE _edgeByte = 0, BYTE _surface0 = 0, BYTE _surface1 = 0, BYTE _tileSetMirrorRotation = 0) :
		edgeByte(_edgeByte),
		surface0(_surface0),
		surface1(_surface1),
		tileSetMirrorRotation(_tileSetMirrorRotation)
	{}

	// builds cached vertex lists for each possible edge byte for rendering
	// this function MUST be called once init the cache!
	static void BuildCache();

	// a tile is clear if both surfaces are 0
	bool IsClear() const { return surface0 == 0 && surface1 == 0; }
	void MakeClear() { edgeByte = surface0 = surface1 = tileSetMirrorRotation = 0; }
	
	// check if any of the area is not clear
	bool IsAreaClear() const
	{
		return 
		(
			IsClear()
			|| !GetSurfaceHasArea(0) && GetSurfaceData(1) == 0 
			|| !GetSurfaceHasArea(1) && GetSurfaceData(0) == 0
		);
	}
	
	// check if any of the area is not clear
	bool IsAreaFull() const
	{
		return 
		(
			GetSurfaceData(0) != 0 && GetSurfaceData(1) != 0
			|| !GetSurfaceHasArea(0) && GetSurfaceData(1) != 0
			|| !GetSurfaceHasArea(1) && GetSurfaceData(0) != 0
		);
	}

	// a tile is full if both edges are the same
	// full tiles use surface0
	bool IsFull() const 
	{ 
		BYTE byteA, byteB;
		GetHalfByte(edgeByte, byteA, byteB);
		return (byteB == byteA);
	}
	void MakeFull() { edgeByte = 0; }

	// does this tile have an edge touching a certain side?
	// 0 == left, 1 == up, 2 == right, 3 == down, 
	bool TouchesSide(const int side, const bool clearsTouch = false, int surfaceSide = 1) const;

	// check if a surface is connected to another tile
	// this is necessary for flood fill operations
	void SurfaceTouches(const TerrainTile& tileNext, int xOffset, int yOffset, int direction, int surfaceSide, bool& surface0Touch, bool& surface1Touch) const;

	// does this tile have two positions that are the same corner?
	bool IsCorner() const;

	// get the points described by an edge byte
	static const Line2& GetEdgeLine(BYTE edgeByte) { return cachedEdgeLines[edgeByte]; }
	const Line2& GetEdgeLine() const { return cachedEdgeLines[edgeByte]; }
	const Vector2& GetPosA() const { return GetEdgeLine().p1; }
	const Vector2& GetPosB() const { return GetEdgeLine().p2; }

	// sets edge data for one side of edge based on passed in offset
	void SetLocalPosA(const Vector2& p);
	void SetLocalPosB(const Vector2& p);
	void SetHalfBytes(const BYTE halfByteA, const BYTE halfByteB);

	// returns the cached vert list for rendering
	void GetVertList(const Vector2*& vertices, int& vertexCount) const { GetVertList(vertices, vertexCount, edgeByte); }

	// returns edge byte for a tile with edge line in opposite direction
	// inother words tile shape representing the background of this tile
	BYTE GetInvertedEdgeData() const;

	TerrainTile Invert() const { return TerrainTile( GetInvertedEdgeData(), surface1, surface0, tileSetMirrorRotation ); }

	// does the surface have actual area or is it one dimensional?
	bool GetSurfaceHasArea(int side) const
	{ return side? (cachedPolyVertCount[GetInvertedEdgeData()] > 2) : (!IsFull() && cachedPolyVertCount[GetEdgeData()] > 2); }

	float GetSurfaceArea(int side) const;
	
	// returns suface side for surface with largest area
	int GetLargerSurfaceSide() const;

	// returns the cached vert list for rendering
	static void GetVertList(const Vector2*& vertices, int& vertexCount, BYTE edgeByte);

	// returns 0 if offset is on the suface1 side or 1 if it is on the suface2 side of the edge
	int GetSurfaceSide(const Vector2& offset) const;

	// resurface the tile if the local space line passes through it
	// returns true if the tile was changed
	bool Resurface(const Vector2& localPosA, const Vector2& localPosB);
	
	// resurface the tile if it is inside circle with local space pos
	// returns true if the tile was changed
	bool Resurface(const Vector2& pos, float radius, float randomness = 0);

	// shrink the block to be 1 smaller on the given side
	// returns true if the tile was changed
	bool MakeSmaller(int edgeSide);

	// shrink the block to be 1 smaller from the point given
	// returns true if the tile was changed
	bool MakeSmaller(const Vector2& pos, const Vector2& direction);

	// get normalized vector representing the direction of the edge
	const Vector2 GetEdgeDirection() const { return (GetPosB() - GetPosA()).Normalize(); }

	// does this connect to another tile with a given offset
	static bool IsConnectedAB(const TerrainTile& tileA, const TerrainTile& tileB, const int xOffset, const int yOffset);
	
	// check if tile should be a solid square of collision
	bool HasFullCollision() const;

public: // settings

	// check how large tiles are in world space
	static void SetSize(float _size);
	static float GetSize()		{ return size; }
	static float GetRadius()	{ return radius; }

private: // settings

	static float size;			// size in world space of each tile
	static float radius;		// radius of a tile based on it's size

public: // get and set functions for data

	// get/set raw edge data
	BYTE GetEdgeData() const { return edgeByte; }
	void SetEdgeData(const BYTE _edgeByte) { edgeByte = _edgeByte; }

	// get suface data based on passed in offset
	BYTE GetSurfaceData(const Vector2& offset) const
	{ return GetSurfaceData(GetSurfaceSide(offset)); }

	// set suface data based on passed in offset
	void SetSurfaceData(const Vector2& offset, const BYTE surface)
	{ SetSurfaceData(GetSurfaceSide(offset), surface); }

	// get suface data based on passed in side (false is side1, true is side2)
	BYTE GetSurfaceData(int side) const
	{ return (side? surface1 : surface0); }

	// set suface data based on passed in side (false is side1, true is side2)
	void SetSurfaceData(int side, const BYTE surface) 
	{ (side? surface1 : surface0) = surface; }

	// get XY position for edge endpoints
	void GetXYA(BYTE& x, BYTE& y) const;
	void GetXYB(BYTE& x, BYTE& y) const;
	void GetXYAB(BYTE& xa, BYTE& ya, BYTE& xb, BYTE& yb) const;

	// does this tile have sufaces matching the parameters?
	bool AreSurfacesEqual(const BYTE _surface0, const BYTE _surface1) const
	{ return (surface0 == _surface0) && (surface1 == _surface1); }

	// does this tile have two sufaces that are the same?
	bool AreBothSurfacesEqual() const
	{ return (surface0 == surface1); }

	// tile set functions
	void SetTileSetRotationMirror(BYTE t, BYTE r, bool m)
	{
		ASSERT(t < GetMaxTileSetCount() && r < 4);
		tileSetMirrorRotation = t | (r << 6) | (m ? (1 << 5) : 0);
	}

	void SetTileSet(BYTE t) { SetTileSetRotationMirror(t, GetRotation(), GetMirror()); }
	BYTE GetTileSet() const { return (31 & tileSetMirrorRotation); }

	Vector2 GetUp() const { return Vector2::BuildFromAngle(float((GetMirror() ? -1 : 1) * GetRotation()) * PI / 2); }
	void SetRotation(BYTE r) { SetTileSetRotationMirror(GetTileSet(), r, GetMirror()); }
	BYTE GetRotation() const { return (192 & tileSetMirrorRotation) >> 6;  }
	void RotateTexture(int r) { ASSERT(r >= 0 && r <= 4); SetRotation((GetRotation() + r) % 4); }

	void SetMirror(BYTE m) { SetTileSetRotationMirror(GetTileSet(), GetRotation(), m); }
	bool GetMirror() const { return (32 & tileSetMirrorRotation) >> 5; }

	// rotate/mirror edge data
	void RotateEdge(int rotate);
	void MirrorEdge();
	static void RotateXY(BYTE& x, BYTE& y, BYTE rotate);

	void Rotate(int rotate) { RotateTexture(GetMirror() ? ((rotate+2)%4) : rotate); RotateEdge(rotate); }
	void Mirror() { MirrorEdge(); SetMirror(!GetMirror()); }
	
	// clockwise rotation around the given offset
	// this is used by the code that builds physical edges
	static void RotateOffset(int& x, int &y, const int direction);
	
	static BYTE GetMaxTileSetCount() { return 32; }

private:

	BYTE edgeByte;				// the compressed edge 
	BYTE surface0;				// used to store texture/material properites of the tile
	BYTE surface1;				// used to store texture/material properites of the tile
	BYTE tileSetMirrorRotation;	// top 2 bits are used for rotation, bottom 6 are tile set

	///////////////////////////////////////////////////////////
	// internal functions
	///////////////////////////////////////////////////////////

	// we need to know if a half byte is on a coner when building the vert list
	static bool HalfByteIsCorner(const BYTE halfByte);

	// split an edge byte into two half edge bytes
	static void GetHalfByte(const BYTE edgeByte, BYTE& byteA, BYTE& byteB);

	// convert position to and from a half edge byte
	static BYTE GetHalfByteFromPosition(const Vector2& pos);
	static BYTE GetHalfByteFromPosition(const Vector2& pos, int side);
	static Vector2 GetHalfBytePosition(const BYTE halfEdgeByte);

	// functions to get positon from byte
	static void GetXYFromHalfByte(const BYTE halfByte, BYTE& x, BYTE& y);
	static BYTE GetHalfByteFromXY(BYTE x, BYTE y);
	static Vector2 GetOffsetFromHalfByte(const BYTE halfByte);
	static Line2 GetLineFromByte(const BYTE edgeByte);

	// keep cached positions for every possible tile
	static Line2 cachedEdgeLines[TERRAIN_UNIQUE_TILE_COUNT];

	// keep cached vert list to form poly for every possible tile
	static void BuildVertList(const BYTE edgeByte, Vector2* vertices, int& vertexCount);
	static Vector2 cachedPolyVerts[TERRAIN_UNIQUE_TILE_COUNT][TERRAIN_TILE_MAX_VERTS];
	static int cachedPolyVertCount[TERRAIN_UNIQUE_TILE_COUNT];
};

// split an edge byte into two half edge bytes
inline void TerrainTile::GetHalfByte(const BYTE byte, BYTE& byteA, BYTE& byteB)
{
	byteA = byte >> 4;
	byteB = byte & 0x0F;
}

// get XY for point A of edge
inline void TerrainTile::GetXYA(BYTE& x, BYTE& y) const
{
	BYTE byteA = edgeByte >> 4;
	GetXYFromHalfByte(byteA, x, y);
} 

// get XY for point B of edge
inline void TerrainTile::GetXYB(BYTE& x, BYTE& y) const
{
	BYTE byteB = edgeByte & 0x0F;
	GetXYFromHalfByte(byteB, x, y);
} 

// get XY for both sides of edge
inline void TerrainTile::GetXYAB(BYTE& xa, BYTE& ya, BYTE& xb, BYTE& yb) const
{
	BYTE byteA, byteB;
	GetHalfByte(edgeByte, byteA, byteB);
	GetXYFromHalfByte(byteA, xa, ya);
	GetXYFromHalfByte(byteB, xb, yb);
} 

// sets edge data for side A of edge based on passed in offset
inline void TerrainTile::SetLocalPosA(const Vector2& p)
{
	const BYTE newHalfByte = GetHalfByteFromPosition(p);
	edgeByte &= 0x0F;
	edgeByte += (newHalfByte << 4);
}

// sets edge data for side B of edge based on passed in offset
inline void TerrainTile::SetLocalPosB(const Vector2& p)
{
	const BYTE newHalfByte = GetHalfByteFromPosition(p);
	edgeByte &= 0xF0;
	edgeByte += newHalfByte;
}
// sets edge data for side B of edge based on passed in offset
inline void TerrainTile::SetHalfBytes(const BYTE halfByteA, const BYTE halfByteB)
{
	edgeByte = (halfByteA << 4) | halfByteB;
}

// we need to know if a half byte is on a coner when building the vert list
inline bool TerrainTile::HalfByteIsCorner(const BYTE halfByte) 
{ 
	ASSERT(halfByte < 16);
	return !(halfByte % 4); 
}

// returns edge byte for a tile with edge line in opposite direction
// inother words tile shape representing the background of this tile
inline BYTE TerrainTile::GetInvertedEdgeData() const
{ 
	BYTE byteA, byteB;
	GetHalfByte(edgeByte, byteA, byteB);
	return (byteB << 4) + byteA;
}

// returns true if offset is on inside surface (suface1) or false if it is on the outside (suface2) 
inline int TerrainTile::GetSurfaceSide(const Vector2& offset) const
{
	ASSERT(offset.x >= -FRANK_EPSILON && offset.y >= -FRANK_EPSILON );
	ASSERT(offset.x <= size + FRANK_EPSILON && offset.y <= size + FRANK_EPSILON);

	if (IsFull())
		return 1;

	// check if local pos is on which side of edge line
	// do a clockwise winding test to see which side we are on
	const Line2& line = GetEdgeLine();
	return Vector2::IsClockwise(line.p1, line.p2, offset)? 1 : 0;
}

// returns the cached vert list for rendering
inline void TerrainTile::GetVertList(const Vector2*& vertices, int& vertexCount, BYTE edgeByte)
{ 
	ASSERT(cachedPolyVertCount[0] != 0);	// vertex data not cached!
	vertices = &cachedPolyVerts[edgeByte][0];
	vertexCount = cachedPolyVertCount[edgeByte];
}

// clockwise rotation around the given offset
inline void TerrainTile::RotateOffset(int& x, int &y, const int direction)
{
	ASSERT(x==-1 || x==1 || y==-1 || y==1);
	ASSERT(x >= -1 && x <= 1 && y >= -1 && y <= 1);
	ASSERT(direction == 1 || direction == -1);

	if (y == 1 || y == -1)
	{
		x += y * direction;
		if (x == 2 || x == -2)
		{
			x /= 2;
			y = 0;
		}
	}
	else
		y -= direction * x;
}

// does this connect to another tile with a given offset
inline bool TerrainTile::IsConnectedAB(const TerrainTile& tileA, const TerrainTile& tileB, const int xOffset, const int yOffset)
{
	if (tileA.IsFull() || tileA.IsClear() || tileB.IsFull() || tileB.IsClear())
		return false;

	ASSERT(xOffset==-1 || xOffset==1 || yOffset==-1 || yOffset==1);
	ASSERT(xOffset >= -1 && xOffset <= 1 && yOffset >= -1 && yOffset <= 1);

	// get the endpoints
	BYTE xa, ya, xb, yb;
	tileA.GetXYA(xa, ya);
	tileB.GetXYB(xb, yb);

	// check if endpoints are touching
	xa += xOffset * 4;
	ya += yOffset * 4;
	xa = xa % 8;
	ya = ya % 8;
	
	return (xa == xb && ya == yb);
}

// does this tile have two positions that are the same corner?
// useful when editing to prevent against creation of solid corners
inline bool TerrainTile::IsCorner() const
{
	BYTE byteA, byteB;
	GetHalfByte(edgeByte, byteA, byteB);
	return (HalfByteIsCorner(byteA) && (byteA == byteB));
}

// convert from local position to half byte
inline BYTE TerrainTile::GetHalfByteFromPosition(const Vector2& pos)
{
	const BYTE x = (BYTE)(floorf(4*pos.x / size + 0.5f));
	const BYTE y = (BYTE)(floorf(4*pos.y / size + 0.5f));
	ASSERT(x >= 0 && x <= 4 && y >= 0 && y <= 4);

	return GetHalfByteFromXY(x, y);
}

inline BYTE TerrainTile::GetHalfByteFromXY(BYTE x, BYTE y)
{
	ASSERT(x >= 0 && y >= 0 && x <= 4 && y <= 4);

	if (x == 0)
		return y;
	else if (y == 4)
		return 4 + x;
	else if (x == 4)
		return 12 - y;
	else
		return 16 - x;
}

// convert from half byte to local position
inline Vector2 TerrainTile::GetHalfBytePosition(const BYTE halfEdgeByte)
{
	return size*GetOffsetFromHalfByte(halfEdgeByte);
}

// get x, y position offset for a half byte
inline void TerrainTile::GetXYFromHalfByte(const BYTE halfByte, BYTE& x, BYTE& y)
{
	ASSERT(halfByte < 16);

	if (halfByte <= 4)
	{
		x = 0;
		y = halfByte;
	}
	else if (halfByte <= 8)
	{
		x = halfByte - 4;
		y = 4;
	}
	else if (halfByte <= 12)
	{
		x = 4;
		y = 12 - halfByte;
	}
	else
	{
		x = 16 - halfByte;
		y = 0;
	}
}

// gets position offsets in 0-1 space for halfbyte
inline Vector2 TerrainTile::GetOffsetFromHalfByte(const BYTE halfByte)
{
	BYTE x, y;
	GetXYFromHalfByte(halfByte, x, y);
	return Vector2(x, y) / 4.0f;
}

// get the world space positions for a half byte
// line is in 0 - size space
inline Line2 TerrainTile::GetLineFromByte(const BYTE edgeByte)
{
	BYTE halfByte1, halfByte2;
	GetHalfByte(edgeByte, halfByte1, halfByte2);

	return Line2
	(
		size*GetOffsetFromHalfByte(halfByte1),
		size*GetOffsetFromHalfByte(halfByte2)
	);
}
