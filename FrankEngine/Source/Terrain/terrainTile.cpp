////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Terrain
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../terrain/terrainTile.h"

///////////////////////////////////////////////////////////
// settings
///////////////////////////////////////////////////////////

float TerrainTile::size		= 1.0f;					// size in world space of each tile
float TerrainTile::radius	= 1.0f * ROOT_2 / 2;	// precalculated radius in world space

///////////////////////////////////////////////////////////
// cache data
///////////////////////////////////////////////////////////

Line2 TerrainTile::cachedEdgeLines[TERRAIN_UNIQUE_TILE_COUNT] = {Line2(Vector2(0), Vector2(0))};
Vector2 TerrainTile::cachedPolyVerts[TERRAIN_UNIQUE_TILE_COUNT][TERRAIN_TILE_MAX_VERTS] = {Vector2(0)};
int TerrainTile::cachedPolyVertCount[TERRAIN_UNIQUE_TILE_COUNT] = {0};

///////////////////////////////////////////////////////////
// member functions
///////////////////////////////////////////////////////////

void TerrainTile::SetSize(float _size)
{
	size = _size;
	radius = size * ROOT_2 / 2;

	BuildCache();
}

// builds cached vertex lists for each possible edge byte for rendering
// this function MUST be called once init the cache!
void TerrainTile::BuildCache()
{
	BYTE edgeByte = 0;
	do
	{
		cachedEdgeLines[edgeByte] = GetLineFromByte(edgeByte);
		BuildVertList(edgeByte, &cachedPolyVerts[edgeByte][0], cachedPolyVertCount[edgeByte]);
		++edgeByte;
	} 
	while (edgeByte);
}

// check if a surface is connected to another tile
// this is necessary for flood fill operations
void TerrainTile::SurfaceTouches
(
	const TerrainTile& tileNext, 
	int xOffset, int yOffset,
	int direction, int surfaceSide, 
	bool& surface0Touch, bool& surface1Touch
) const
{
	int directionFrom;
	if (direction == 0)
		directionFrom = 2;
	else if (direction == 2)
		directionFrom = 0;
	else if (direction == 1)
		directionFrom = 3;
	else if (direction == 3)
		directionFrom = 1;

	// check if this tile opens in that direction
	if (!TouchesSide(direction, true, surfaceSide))
	{
		surface0Touch = surface1Touch = false;
		return;
	}

	if (IsConnectedAB(tileNext, *this, xOffset, yOffset))
	{
		surface0Touch = surfaceSide == 0;
		surface1Touch = surfaceSide == 1;
		return;
	}

	if (IsConnectedAB(*this, tileNext, -xOffset, -yOffset))
	{
		surface0Touch = surfaceSide == 0;
		surface1Touch = surfaceSide == 1;
		return;
	}

	if (tileNext.IsFull())
	{
		surface0Touch = true;
		surface1Touch = true;
	}
	else
	{
		surface0Touch = false;
		surface1Touch = false;
	}

	surface0Touch = tileNext.TouchesSide(directionFrom, true, 0);
	surface1Touch = tileNext.TouchesSide(directionFrom, true, 1);
}


// does this tile have an edge touching a certain side?
// this function is very useful for flood erase code
// 0 == right, 1 == bottom, 2 == left, 3 == top
bool TerrainTile::TouchesSide(const int side, const bool clearsTouch, int surfaceSide) const
{
	ASSERT(side >= 0 && side <= 3);

	if (IsClear())
		return clearsTouch;
	if (IsFull())
		return true;

	BYTE xa, ya, xb, yb;
	GetXYAB(xa, ya, xb, yb);

	if (surfaceSide)
	{
		int temp = xa;
		xa = xb;
		xb = temp;
		temp = ya;
		ya = yb;
		yb = temp;
	}

	const Vector2 deltaPos((float)(xa - xb), (float)(ya - yb));
	switch (side)
	{
		case 0:
			return ((xa == 0 && ya != 4) || (xb == 0 && yb != 0) || ya < yb);

		case 1:
			return ((ya == 4 && xa != 4) || (yb == 4 && xb != 0) || xa < xb);

		case 2:
			return ((xa == 4 && ya != 0) || (xb == 4 && yb != 4) || ya > yb);

		case 3:
			return ((ya == 0 && xa != 0) || (yb == 0 && xb != 4) || xa > xb);
	}

	return false;
}


// resurface the tile if the local space line passes through it
// returns true if the tile was changed
bool TerrainTile::Resurface(const Vector2& posA, const Vector2& posB)
{
	int intersectionCount = 0;
	const TerrainTile tileOld = *this;

	// test against each segment that represents the bounding box for this tile
	// this could be optimized but it is only used in the editor
	Vector2 point;
	if (Vector2::LineSegmentIntersection(posA, posB, Vector2(0, 0), Vector2(0, size), point))
	{
		++intersectionCount;
		if (posA.x > posB.x)
			SetLocalPosA(point);
		else
			SetLocalPosB(point);
	}
	if (Vector2::LineSegmentIntersection(posA, posB, Vector2(size, 0), Vector2(size, size), point))
	{
		++intersectionCount;
		if (posA.x < posB.x)
			SetLocalPosA(point);
		else
			SetLocalPosB(point);
	}
	if (Vector2::LineSegmentIntersection(posA, posB, Vector2(size, 0), Vector2(0, 0), point))
	{
		++intersectionCount;
		if (posA.y > posB.y) 
			SetLocalPosA(point);
		else
			SetLocalPosB(point);
	}
	if (Vector2::LineSegmentIntersection(posA, posB, Vector2(0, size), Vector2(size, size), point))
	{
		++intersectionCount;
		if (posA.y < posB.y)
			SetLocalPosA(point);
		else
			SetLocalPosB(point);
	}

	// if we went in and out on a corner just set it back to what it was
	if (intersectionCount == 2 && IsCorner())
		*this = tileOld;
	else if (intersectionCount > 0)
	{
		if (intersectionCount == 1 && tileOld.GetEdgeData() == 0 && tileOld.GetSurfaceData(0) == tileOld.GetSurfaceData(1))
		{
			// check if it had 0 edge data (was untouched)
			// if so, make this a full block
			BYTE byteA, byteB;
			GetHalfByte(GetEdgeData(), byteA, byteB);
			if (byteA == 0)
				byteA = byteB;
			else if (byteB == 0)
				byteB = byteA;
			SetHalfBytes(byteA, byteB);
		}
		return true;
	}

	return false;
}


// resurface the tile if it is inside circle with local space pos
// returns true if the tile was changed
bool TerrainTile::Resurface(const Vector2& pos, float radius, float randomness)
{
	int intersectionCount = 0;
	const TerrainTile tileOld = *this;
	
	// calculate tangents
	const Vector2 deltaPos = (0.5f*Vector2(size) - pos);
	const Vector2 direction = deltaPos.Normalize();
	const Vector2 tanDirection = direction.RotateRightAngle();
	const Vector2 tanPos = pos + direction*radius;
	const Vector2 posA = tanPos + (tanDirection + Vector2::BuildRandomInCircle(randomness))*radius;
	const Vector2 posB = tanPos - (tanDirection - Vector2::BuildRandomInCircle(randomness))*radius;
					
	// test against each segment that represents the bounding box for this tile
	// this could be optimized but it is only used in the editor
	Vector2 point;
	if (Vector2::LineSegmentIntersection(posA, posB, Vector2(0, 0), Vector2(0, size), point))
	{
		++intersectionCount;
		if (posA.x > posB.x)
			SetLocalPosB(point);
		else
			SetLocalPosA(point);
	}
	if (Vector2::LineSegmentIntersection(posA, posB, Vector2(size, 0), Vector2(size, size), point))
	{
		++intersectionCount;
		if (posA.x < posB.x)
			SetLocalPosB(point);
		else
			SetLocalPosA(point);
	}
	if (Vector2::LineSegmentIntersection(posA, posB, Vector2(size, 0), Vector2(0, 0), point))
	{
		++intersectionCount;
		if (posA.y > posB.y) 
			SetLocalPosB(point);
		else
			SetLocalPosA(point);
	}
	if (Vector2::LineSegmentIntersection(posA, posB, Vector2(0, size), Vector2(size, size), point))
	{
		++intersectionCount;
		if (posA.y < posB.y)
			SetLocalPosB(point);
		else
			SetLocalPosA(point);
	}

	// if we went in and out on a corner just set it back to what it was
	if (intersectionCount == 2 && IsCorner())
		*this = tileOld;
	else if (intersectionCount > 0)
		return true;

	// check if it is fully inside
	if (deltaPos.LengthSquared() <= Square(radius))
	{
		MakeClear();
		return true;
	}

	return false;
}


// convert from local position to half byte
inline BYTE TerrainTile::GetHalfByteFromPosition(const Vector2& pos, int side)
{
	const float x = 4 * (pos.x / size);
	const float y = 4 * (pos.y / size);

	const float xc = x - 2;
	const float yc = y - 2;

	BYTE result;
	if (yc >= xc && yc > -xc)
		result = 4 + (BYTE)Cap(x, 0.0f, 3.0f);
	else if (xc > yc && xc >=-yc)
		result = 11 - (BYTE)Cap(y, 0.0f, 3.0f);
	else if (-yc > xc && -yc >=-xc)
		result = 15 - (BYTE)Cap(x, 0.0f, 3.0f);
	else
		result = (BYTE)Cap(y, 0.0f, 3.0f);

	if (!side)
		return result;
	else
		return (result + 1) % 16;
}

int TerrainTile::GetLargerSurfaceSide() const
{
	const int vertCount = cachedPolyVertCount[GetEdgeData()];

	if (vertCount <= 2)
		return 1;
	
	const Vector2* verts = cachedPolyVerts[GetEdgeData()];
	float area = 0;
	for (int i = 0; i < vertCount; ++i)
		area += verts[(i+1)%vertCount].Cross(verts[i]);

	return (area > TerrainTile::GetSize()*TerrainTile::GetSize())? 0 : 1;
}

float TerrainTile::GetSurfaceArea(int side) const
{
	const int vertCount = side? cachedPolyVertCount[GetInvertedEdgeData()] : cachedPolyVertCount[GetEdgeData()];

	if (vertCount <= 2)
		return 0;
	
	const Vector2* verts = side? cachedPolyVerts[GetInvertedEdgeData()] : cachedPolyVerts[GetEdgeData()];
	float area = 0;
	for (int i = 0; i < vertCount; ++i)
		area += verts[(i+1)%vertCount].Cross(verts[i]);

	return area/(2*TerrainTile::GetSize()*TerrainTile::GetSize());
}

// shrink the block to be 1 smaller on the given side
// returns true if the tile was changed
bool TerrainTile::MakeSmaller(int edgeSideA)
{
	BYTE halfByteA, halfByteB;
	GetHalfByte(edgeByte, halfByteA, halfByteB);

	if (edgeSideA)
	{
		// make A smaller
		BYTE newHalfByte = (halfByteA + 1) % 16;
		edgeByte &= 0x0F;
		edgeByte += (newHalfByte << 4);
	}
	else
	{
		// make B smaller
		BYTE newHalfByte = (halfByteB == 0)? 15 : (halfByteB - 1) % 16;
		edgeByte &= 0xF0;
		edgeByte += newHalfByte;
	}

	if (!GetSurfaceHasArea(false))
		MakeClear();

	return true;
}

static bool isBetweenHalfBytes(BYTE halfByteA, BYTE halfByteB, BYTE test)
{
	if (halfByteB < halfByteA)
	{
		halfByteB += 16;
	}

	return test < halfByteB;
}

static BYTE getGapSize(BYTE halfByteA, BYTE halfByteB)
{
	if (halfByteA < halfByteB)
		halfByteA += 16;

	return halfByteA - halfByteB;
}

/*float GetSurfaceArea() const
{
	float area = 0;
	BYTE i = 1;

	Vector2[] verts = cachedPolyVerts[edgeByte];
	Vector2 pivot = verts[0];
	for (int i = 0; i < cachedPolyVertCount[edgeByte]; ++i)
	{

		cachedPolyVerts[edgeByte][i] = GetLineFromByte(edgeByte);
		BuildVertList(edgeByte, &cachedPolyVerts[edgeByte][0], cachedPolyVertCount[edgeByte]);
		++i;
	} 
	while (i);

	return area;
}*/

// shrink the block to be 1 smaller on the given side
// returns true if the tile was changed
bool TerrainTile::MakeSmaller(const Vector2& pos, const Vector2& direction)
{
	if (IsClear())
		return false;
	
	const GameSurfaceInfo& tile0Info = GameSurfaceInfo::Get(surface0);
	const GameSurfaceInfo& tile1Info = GameSurfaceInfo::Get(surface1);
	if (tile0Info.HasCollision() && tile1Info.HasCollision() && GetSurfaceHasArea(0) && GetSurfaceHasArea(1))
	{
		SetSurfaceData(pos, 0);
		return true;
	}

	if (tile0Info.HasCollision() && tile1Info.HasCollision())
	{
		// if both surfaces have collision we need to reset the tile with a empty surface
		if (!GetSurfaceHasArea(0))
			surface0 = surface1;
		surface1 = 0;
		edgeByte = 0;
	}
	else
	{
		if (!tile0Info.HasCollision())
		{
			// flip it
			swap(surface0, surface1);
			if (!IsFull())
				edgeByte = GetInvertedEdgeData();
		}

		// needs one solid surface and one non solid surface
		ASSERT(tile0Info.HasCollision() && !tile1Info.HasCollision() || tile1Info.HasCollision() && !tile0Info.HasCollision());
	}
	
	// build a block with that side cut out
	TerrainTile desiredTile = *this;
	desiredTile.Resurface(pos - 2*size*direction, pos + 2*size*direction);
	
	BYTE halfByteAd, halfByteBd;
	GetHalfByte(desiredTile.edgeByte, halfByteAd, halfByteBd);

	if (!GetSurfaceHasArea(0) || !GetSurfaceHasArea(1))
	{
		// we are making the first break on a full tile
		if (desiredTile.IsFull() || getGapSize(halfByteAd, halfByteBd) > getGapSize(halfByteBd, halfByteAd))
		{
			// flip the desired tile to use the way that is larger
			BYTE temp = halfByteAd;
			halfByteAd = halfByteBd;
			halfByteBd = temp;
			halfByteBd += (halfByteBd >  0)? -1 :  15;
			halfByteAd += (halfByteAd < 15)?  1 : -15;
		}
		// set both points to be on either side of the nearest point
		const BYTE newHalfByte = GetHalfByteFromPosition(pos, 0);
		SetHalfBytes((newHalfByte + 1) % 16, newHalfByte);
		
		BYTE halfByteA, halfByteB;
		GetHalfByte(edgeByte, halfByteA, halfByteB);

		while (true)
		{
			if (halfByteA != halfByteAd)
			{
				MakeSmaller(1);
				if (GetSurfaceHasArea(1))
					break;
			}
			else
			{
				MakeSmaller(0);
				if (GetSurfaceHasArea(1))
					break;
			}
		}

		return true;
	}

	BYTE halfByteA, halfByteB;
	GetHalfByte(edgeByte, halfByteA, halfByteB);

	if (getGapSize(halfByteAd, halfByteBd) > getGapSize(halfByteBd, halfByteAd))
	{
		// try to decide which side of the split tile we want to look at
		// check each ab distances on each side
		BYTE temp = halfByteAd;
		halfByteAd = halfByteBd;
		halfByteBd = temp;
		halfByteBd += (halfByteBd >  0)? -1 :  15;
		halfByteAd += (halfByteAd < 15)?  1 : -15;
	}

	// find which edge is closer
	const float distanceA = (pos - GetPosA()).LengthSquared();
	const float distanceB = (pos - GetPosB()).LengthSquared();
	int edgeSideA = (distanceA < distanceB);

	bool aSafe = isBetweenHalfBytes(halfByteA, halfByteB, halfByteAd);
	bool bSafe = isBetweenHalfBytes(halfByteA, halfByteB, halfByteBd);
	
	MakeSmaller(edgeSideA && aSafe || !bSafe);
	//if (!GetSurfaceHasArea(0))
	//	MakeClear();

	return true;
}

void TerrainTile::RotateXY(BYTE& x, BYTE& y, BYTE rotate)
{
	ASSERT(rotate >= 0 && rotate <= 3);

	const BYTE xo = x, yo = y;
	switch(rotate)
	{
		default:
		case 0: break;
		case 1: x = 4-yo; y = xo; break;
		case 2: x = 4-xo; y = 4-yo; break;
		case 3: x = yo; y = 4-xo; break;
	};
}

void TerrainTile::RotateEdge(int rotate)
{
	ASSERT(rotate >= 0 && rotate < 4);

	if (!rotate || IsClear() || IsFull())
		return;

	BYTE xa, ya, xb, yb;
	GetXYAB(xa, ya, xb, yb);
	
	RotateXY(xa, ya, rotate);
	RotateXY(xb, yb, rotate);

	const BYTE halfByteA = TerrainTile::GetHalfByteFromXY(xa, ya);
	const BYTE halfByteB = TerrainTile::GetHalfByteFromXY(xb, yb);
	SetHalfBytes(halfByteA, halfByteB);
}

void TerrainTile::MirrorEdge()
{
	if (IsClear() || IsFull())
		return;

	BYTE xa, ya, xb, yb;
	GetXYAB(xa, ya, xb, yb);

	const BYTE halfByteA = TerrainTile::GetHalfByteFromXY(4 - xb, yb);
	const BYTE halfByteB = TerrainTile::GetHalfByteFromXY(4 - xa, ya);

	SetHalfBytes(halfByteA, halfByteB);
}

///////////////////////////////////////////////////////////
// internal functions
///////////////////////////////////////////////////////////

// builds a list of verts that a tile with a given edge byte will need for rendering
void TerrainTile::BuildVertList(BYTE edgeByte, Vector2* vertices, int& vertexCount)
{
	vertexCount = 0;

	// get angles
	BYTE byteA, byteB;
	GetHalfByte(edgeByte, byteA, byteB);
	ASSERT(byteA >= 0 && byteB >= 0);

	// if we are a solid tile start in corner to use less verts
	// check if points are on the same side
	if (byteA <= 3 && byteB <= 3 && byteA >= byteB)
		byteA = byteB = 0;
	else if (byteA >= 4 && byteA <= 7 && byteB >= 4 && byteB <= 7 && byteA >= byteB)
		byteA = byteB = 0;
	else if (byteA >= 8 && byteA <= 11 && byteB >= 8 && byteB <= 11 && byteA >= byteB)
		byteA = byteB = 0;
	else if (byteA >= 12 && byteA <= 15 && byteB >= 12 && byteB <= 15 && byteA >= byteB)
		byteA = byteB = 0;

	*vertices = GetHalfBytePosition(byteA);
	++vertices;
	++vertexCount;

	// just circle around until we are at byteB's location
	BYTE place = byteA;
	while (true)
	{
		++place;
		if (place == 16)
			place = 0;
		if (place == byteB)
			break;

		if (HalfByteIsCorner(place))
		{
			// create internal points at each corner
			*vertices = GetHalfBytePosition(place);
			++vertices;
			++vertexCount;
		}
	}

	if (byteA != byteB)
	{
		*vertices = GetHalfBytePosition(byteB);
		++vertices;
		++vertexCount;	
	}

	ASSERT(vertexCount <= TERRAIN_TILE_MAX_VERTS);
}

bool TerrainTile::HasFullCollision() const 
{
	const GameSurfaceInfo& tile0Info = GameSurfaceInfo::Get(GetSurfaceData(0));
	if (tile0Info.HasCollision() && !GetSurfaceHasArea(1))
		return true; // all surface 0
	
	const GameSurfaceInfo& tile1Info = GameSurfaceInfo::Get(GetSurfaceData(1));
	if (tile1Info.HasCollision() && !GetSurfaceHasArea(0))
		return true; // all surface 1

	// both surfaces must have collision and the same material to consider it full
	return (tile0Info.HasCollision() && tile1Info.HasCollision() && tile0Info.materialIndex == tile1Info.materialIndex);
}
