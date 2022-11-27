////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Mini Map
	Copyright 2018 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "miniMap.h"

bool MiniMap::mapEnable = true;
ConsoleCommand(MiniMap::mapEnable, mapEnable)

float MiniMap::mapAlpha = 0.5f;
ConsoleCommand(MiniMap::mapAlpha, mapAlpha)

bool MiniMap::mapShowFull = false;
ConsoleCommand(MiniMap::mapShowFull, mapShowFull)

float MiniMap::mapSizeSmall = 0.14f;
ConsoleCommand(MiniMap::mapSizeSmall, mapSizeSmall)

float MiniMap::mapSizeLarge = 0.35f;
ConsoleCommand(MiniMap::mapSizeLarge, mapSizeLarge)

// zoom in world units
float MiniMap::mapZoomScale = 80;
ConsoleCommand(MiniMap::mapZoomScale, mapZoomScale)

float MiniMap::mapGapSize = 10.0f;
ConsoleCommand(MiniMap::mapGapSize, mapGapSize)

ConsoleCommandSimple(bool, mapSaveTexture,			false)
ConsoleCommandSimple(bool, mapAlwaysUpdateTexture,	false)
ConsoleCommandSimple(bool, mapUpdateTexture,		false)

void MiniMap::Init()
{
	static bool wasEditMode = false;
	if (GameControlBase::devMode)
	{
		if (mapSaveTexture || !g_gameControlBase->IsEditMode() && (mapAlwaysUpdateTexture || !wasEditMode))
			RenderToTexture();
	}
	else
		RenderToTexture();

	wasEditMode = g_gameControlBase->IsEditMode();
}

void MiniMap::InitDeviceObjects()
{
	if (!mapHiddenTexture)
	{
		IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
		const IntVector2 textureSize = Terrain::fullSize*Terrain::patchSize;
		if (!SUCCEEDED(pd3dDevice->CreateTexture(
				textureSize.x, textureSize.y, 1,  // width, height, mip levels
				D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8,
				D3DPOOL_DEFAULT, &mapHiddenTexture, NULL)))
		{
			g_debugMessageSystem.AddError(L"Could not create map texture!");
			mapHiddenTexture = NULL;
			return;
		}
	}
	if (!mapFullTexture)
	{
		IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
		const IntVector2 textureSize = Terrain::fullSize*Terrain::patchSize;
		if (!SUCCEEDED(pd3dDevice->CreateTexture(
				textureSize.x, textureSize.y, 1,  // width, height, mip levels
				D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8,
				D3DPOOL_DEFAULT, &mapFullTexture, NULL)))
		{
			g_debugMessageSystem.AddError(L"Could not create map texture!");
			mapFullTexture = NULL;
			return;
		}
	}

	RenderToTexture();
}

void MiniMap::DestroyDeviceObjects()
{
	SAFE_RELEASE(mapHiddenTexture);
	SAFE_RELEASE(mapFullTexture);
}

void MiniMap::Update()
{
	GameObject*	player = g_gameControlBase->GetPlayer();
	if (!mapEnable || !g_terrain || !player || player->IsDead() || g_gameControlBase->IsEditMode())
		return;

	// update reveal window
	Vector2 patchScale(TerrainTile::GetSize()*Terrain::patchSize);
	for (int x = -mapRevealWindow; x <= mapRevealWindow; ++x)
	for (int y = -mapRevealWindow; y <= mapRevealWindow; ++y)
	{
		SetMapData(player->GetPosWorld() + Vector2(float(x),float(y))*patchScale);
	}

	if (mapUpdateTexture)
	{
		mapUpdateTexture = 0;
		RenderToTexture();
	}
}

void MiniMap::SetupRender()
{
	if (mapDataDirty)
	{
		mapDataDirty = false;
		RenderMapHiddenToTexture();
	}
}

void MiniMap::SetRenderSettings(bool isLarge)
{
	const XForm2 xfCamera = g_cameraBase->GetXFInterpolated();
	mapRenderSettings.isLarge = isLarge;
	mapRenderSettings.alpha = mapAlpha;
	if (isLarge)
	{
		mapRenderSettings.worldZoom = float(Terrain::fullSize.x * Terrain::patchSize) * TerrainTile::GetSize();
		mapRenderSettings.worldCenter = Vector2(0);
		mapRenderSettings.screenSize = Vector2(g_backBufferHeight * mapSizeLarge);
		mapRenderSettings.screenXF =  XForm2(Vector2(g_backBufferWidth / 2.0f, g_backBufferHeight / 2.0f), xfCamera.angle);
	}
	else
	{
		mapRenderSettings.worldZoom = mapZoomScale;
		GameObject*	player = g_gameControlBase->GetPlayer();
		mapRenderSettings.worldCenter = player? player->GetXFInterpolated().position : g_gameControlBase->GetUserPosition();

		const Vector2 cornerOffset = g_cameraBase->GetBackBufferLockedCornerOffset() * Vector2(1,-1);
		const Vector2 corner = g_cameraBase->GetBackBufferCenter() + cornerOffset;
		mapRenderSettings.screenSize = Vector2(fabs(cornerOffset.y * 2) * mapSizeSmall);
		mapRenderSettings.screenXF = XForm2(corner + Vector2(-mapRenderSettings.screenSize.x - mapGapSize, mapRenderSettings.screenSize.y + mapGapSize), xfCamera.angle);	
	}
}

void MiniMap::Render()
{
	GameObject*	player = g_gameControlBase->GetPlayer();
	if (!mapEnable || !g_terrain || !player || g_gameControlBase->IsEditMode())
		return;
	
	SetRenderSettings(false);
	if (!mapRenderSettings.screenSize.IsZero())
		RenderMiniMap();
	
	if (IsLargeMapVisible())
	{
		SetRenderSettings(true);
		if (!mapRenderSettings.screenSize.IsZero())
			RenderLargeMap();
	}
}

void MiniMap::RenderMiniMapBackground() const
{
	RenderMapQuad(mapRenderSettings.worldCenter, Vector2(mapRenderSettings.worldZoom), Color::Black(mapRenderSettings.alpha), GetCircleTexture());
}

void MiniMap::RenderMiniMapForeground() const
{
	RenderMapQuad(mapRenderSettings.worldCenter, Vector2(4), Color::Red(), GetCircleTexture());
	RenderMapQuad(mapRenderSettings.worldCenter, Vector2(2), Color::White(), GetCircleTexture());
}

void MiniMap::RenderMiniMap()
{
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

	RenderMiniMapBackground();

	{
		// set up transform so map is centered on player
		Vector2 quadWorldPos = g_terrain->GetPosWorld();
		Vector2 quadWorldSize = Vector2(Terrain::fullSize*Terrain::patchSize)*TerrainTile::GetSize();
		const Vector2 scale ( mapRenderSettings.worldZoom / quadWorldSize);
		Matrix44 textureMatrix = Matrix44::BuildScale(scale.x, scale.y, 1);
		D3DMATRIX& textureMatrixD3D = textureMatrix.GetD3DXMatrix();

		Vector2 mPos(-.5f*scale + Vector2(0.5f) + (Vector2(1,-1)*mapRenderSettings.worldCenter + quadWorldPos + quadWorldSize * 0.5f) / quadWorldSize);
		textureMatrixD3D._31 = mPos.x;
		textureMatrixD3D._32 = mPos.y;
		pd3dDevice->SetTransform(D3DTS_TEXTURE1, &textureMatrixD3D);
		pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,	D3DTOP_MODULATE );
		pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1,	D3DTA_TEXTURE );
		pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2,	D3DTA_CURRENT );
		pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,	D3DTOP_MODULATE );
		pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1,	D3DTA_TEXTURE );
		pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG2,	D3DTA_CURRENT );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU,		D3DTADDRESS_BORDER );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV,		D3DTADDRESS_BORDER );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSW,		D3DTADDRESS_BORDER );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER,		D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER,		D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER,		D3DTEXF_NONE );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER,		D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER,		D3DTEXF_POINT );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER,		D3DTEXF_NONE );
		pd3dDevice->SetTexture(1, mapShowFull ? mapFullTexture : mapHiddenTexture);
	}

	pd3dDevice->SetRenderState( D3DRS_SRCBLEND,				D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState( D3DRS_DESTBLEND,			D3DBLEND_ONE);
	
	RenderMapQuad(mapRenderSettings.worldCenter, Vector2(mapRenderSettings.worldZoom), Color::White(mapRenderSettings.alpha), GetMaskTexture());

	// set stuff back to normal
	pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,	D3DTOP_DISABLE );
	pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,	D3DTOP_DISABLE );
	pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pd3dDevice->SetTexture(1, NULL);
	pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSW,		D3DTADDRESS_WRAP );
	
	RenderMiniMapForeground();
}

void MiniMap::RenderLargeMap()
{
	{
		IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
		// draw a border around the outside 
		RenderMapQuad(XForm2(0), Vector2(mapRenderSettings.worldZoom * 1.05f), Color::Grey(mapRenderSettings.alpha*0.5f), Texture_Invalid, Color::Black(mapRenderSettings.alpha*0.5f));

		// draw the map
		RenderMapQuad(XForm2(0), Vector2(mapRenderSettings.worldZoom), Color::White(mapRenderSettings.alpha), mapShowFull ? mapFullTexture : mapHiddenTexture, Color::Black(mapRenderSettings.alpha*0.5f));
	
		// draw squares around each patch
		{
			// vertical
			Vector2 offset(g_terrain->patchSize*TerrainTile::GetSize() / 2.0f, 0);
			Vector2 patchSize = Vector2(g_terrain->patchSize*TerrainTile::GetSize(), g_terrain->fullSize.y*g_terrain->patchSize*TerrainTile::GetSize());
			for(int i=0; i<Terrain::fullSize.x; i += 2)
			{
				XForm2 patchXF = g_terrain->GetPatchPos(IntVector2(i, Terrain::fullSize.y/2)) + offset;
				RenderMapQuad(patchXF, patchSize, Color::White(0), Texture_Invalid, Color::Grey(mapRenderSettings.alpha*0.2f));
			}
		}
		{
			// horizontal
			Vector2 offset(0, g_terrain->patchSize*TerrainTile::GetSize() / 2.0f);
			Vector2 patchSize = Vector2(g_terrain->fullSize.x*g_terrain->patchSize*TerrainTile::GetSize(), g_terrain->patchSize*TerrainTile::GetSize());
			for(int j=0; j<Terrain::fullSize.y; j += 2)
			{
				XForm2 patchXF = g_terrain->GetPatchPos(IntVector2(Terrain::fullSize.x/2, j)) + offset;
				RenderMapQuad(patchXF, patchSize, Color::White(0), Texture_Invalid, Color::Grey(mapRenderSettings.alpha*0.2f));
			}
		}
	}
	
	GameObject*	player = g_gameControlBase->GetPlayer();
	const XForm2 xfPlayer = player->GetXFInterpolated();
}

XForm2 MiniMap::GetMapSpaceXF(const XForm2& _xf) const
{
	XForm2 xf = _xf;
	xf.position -= mapRenderSettings.worldCenter;
	xf = xf.ScalePos(2 * mapRenderSettings.screenSize / mapRenderSettings.worldZoom);
	xf = xf.ScalePos(Vector2(1, -1));
	xf *= mapRenderSettings.screenXF;
	return xf;
}

void MiniMap::RenderMapQuad(const XForm2& _xf, const Vector2& size, const Color& color, LPDIRECT3DTEXTURE9 texture, const Color& outlineColor) const
{
	// convert world space xf and size to map space for rendering
	XForm2 xf = GetMapSpaceXF(_xf);
	g_render->RenderScreenSpaceQuad(xf, mapRenderSettings.screenSize*size/mapRenderSettings.worldZoom, color, texture, outlineColor);
}

void MiniMap::RenderMapQuad(const XForm2& _xf, const Vector2& size, const Color& color, const TextureID texture, const Color& outlineColor) const
{
	// convert world space xf and size to map space for rendering
	XForm2 xf = GetMapSpaceXF(_xf);
	g_render->RenderScreenSpaceQuad(xf, mapRenderSettings.screenSize*size/mapRenderSettings.worldZoom, color, texture, outlineColor);
}

void MiniMap::RenderMapText(const XForm2& _xf, float size, const Color& color, const WCHAR* text, FontFlags flags) const
{
	// convert world space xf and size to map space for rendering
	XForm2 xf = GetMapSpaceXF(_xf);
	FrankFont* font = g_gameControlBase->GetGameFont();
	ASSERT(font);
	font->RenderScreenSpace(text, xf, mapRenderSettings.screenSize.x*size/mapRenderSettings.worldZoom, color, flags);
}

void MiniMap::RenderMapText(const XForm2& _xf, float size, const Color& color, const char* text, FontFlags flags) const
{
	// convert world space xf and size to map space for rendering
	XForm2 xf = GetMapSpaceXF(_xf);
	FrankFont* font = g_gameControlBase->GetGameFont();
	ASSERT(font);
	font->RenderScreenSpace(text, xf, mapRenderSettings.screenSize.x*size/mapRenderSettings.worldZoom, color, flags);
}

void MiniMap::RenderMapIcon(const XForm2& _xf, const Vector2& size, const Color& color, const TextureID texture, const TextureID arrowTexture) const
{
	XForm2 xf = _xf;
	xf.angle = 0;

	bool inRange = true;
	if (!mapRenderSettings.isLarge)
	{
		// check if in range
		Vector2 deltaPos = xf.position - mapRenderSettings.worldCenter;
		float mapRange = mapRenderSettings.worldZoom * 0.48f;
		if (IsMapSquare())
		{
			inRange = (fabs(deltaPos.x) < mapRange && fabs(deltaPos.y) < mapRange);
			if (!inRange)
			{
				if (arrowTexture == Texture_Invalid)
					return;
				deltaPos.x = Cap(deltaPos.x, -mapRange, mapRange);
				deltaPos.y = Cap(deltaPos.y, -mapRange, mapRange);
				xf.position = deltaPos + mapRenderSettings.worldCenter;
				xf.angle = -deltaPos.GetAngle();
			}
		}
		else
		{
			inRange = deltaPos.LengthSquared() < Square(mapRange);
			if (!inRange)
			{
				if (arrowTexture == Texture_Invalid)
					return;
				xf.position = mapRenderSettings.worldCenter + deltaPos.Normalize() * mapRange;
				xf.angle = -deltaPos.GetAngle();
			}
		}
	}
	
	// hack: dont use point filter when drawing arrow
	DeferredRender::PointFilterRenderBlock pfrb(inRange);
	RenderMapQuad(xf, size, color, inRange ? texture : arrowTexture);
}

void MiniMap::RenderWorldSpace(const XForm2& xf, const Vector2& size, const Color& color, const XForm2& xfCenter, float zoom, bool showFull, bool isStub)
{
	if (DeferredRender::GetRenderPassIsNormalMap() || DeferredRender::GetRenderPassIsSpecular())
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
	{
		// set up transform so map is centered and rotated
		Vector2 quadWorldPos = g_terrain->GetPosWorld();
		Vector2 quadWorldSize = Vector2(Terrain::fullSize*Terrain::patchSize)*TerrainTile::GetSize();
		Vector2 scale( zoom / quadWorldSize);
		scale.y *= size.y / size.x;

		Matrix44 textureMatrix = Matrix44::BuildScale(scale.x, scale.y, 1);
		Vector2 mPos = Vector2(1, -1)*xfCenter.position.Rotate(-xfCenter.angle);
		mPos += quadWorldPos + quadWorldSize * 0.5f;
		mPos /= quadWorldSize;
		mPos += -.5f*scale;
		mPos = mPos.Rotate(-xfCenter.angle);
		mPos += Vector2(0.5f);

		textureMatrix *= Matrix44::BuildRotateZ(-xfCenter.angle);
		textureMatrix.GetD3DXMatrix()._31 = mPos.x;
		textureMatrix.GetD3DXMatrix()._32 = mPos.y;

		pd3dDevice->SetTransform(D3DTS_TEXTURE0, &textureMatrix.GetD3DXMatrix());
		pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU,		D3DTADDRESS_BORDER );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV,		D3DTADDRESS_BORDER );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSW,		D3DTADDRESS_BORDER );
	}

	g_render->RenderQuad(xf, size, color, showFull ? mapFullTexture : mapHiddenTexture);

	// set stuff back to normal
	pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSW,		D3DTADDRESS_WRAP );
}

//////////////////////////////////////////////////////////////////////////////////////
// rendering map to a texture

void MiniMap::RenderMapHiddenToTexture()
{
	MapRenderBlock mapRenderBlock(mapHiddenTexture);
	
	DXUTGetD3D9Device()->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	
	// render the full texture down first
	g_render->RenderQuad(XForm2(Vector2(-0.5f, 0.5f)), 0.5f*Vector2(Terrain::fullSize*Terrain::patchSize), Color::White(), mapFullTexture);

	// cover up undiscovered areas
	for(int i=0; i<Terrain::fullSize.x; ++i)
	for(int j=0; j<Terrain::fullSize.y; ++j)
	{
		const int data = GetMapData(IntVector2(i,j));
		if (data == 1)
			continue;

		Vector2 pos = g_terrain->GetPatchPos(IntVector2(i, j)) + Vector2(0.5f*float(Terrain::patchSize));
		g_render->RenderQuad(pos, Vector2(0.5f*float(Terrain::patchSize)), Color::Black(), Texture_Invalid, false);
		g_render->RenderQuad(pos, Vector2(0.3f*float(Terrain::patchSize)), Color::Grey(0.5f), Texture_Invalid, false);
	}
}

void MiniMap::RenderToTexture()
{
	if (!g_terrain)
		return;
	
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"MiniMap::RenderToTexture()" );
	{
		MapRenderBlock mapRenderBlock(mapFullTexture);
		RenderTilesToTexture();
		RenderObjectsToTexture();
	}

	RenderMapHiddenToTexture();
}

void MiniMap::RenderTilesToTexture()
{
	for(int i=0; i<Terrain::fullSize.x; ++i)
	for(int j=0; j<Terrain::fullSize.y; ++j)
		RenderTilesToTexture(*(g_terrain->GetPatch(i, j)));
}

void MiniMap::RenderObjectsToTexture()
{
	for(int i=0; i<Terrain::fullSize.x; ++i)
	for(int j=0; j<Terrain::fullSize.y; ++j)
		RenderObjectsToTexture(*(g_terrain->GetPatch(i, j)));
}

TextureID MiniMap::GetMinimapTexture(int layer) const
{
	return g_terrain->GetTileSetTexture(layer);
}

void MiniMap::RenderTileToTexture(const TerrainTile& tile, const Vector2& tileOffset, int layer)
{
	RenderTileToTexture(tile, tileOffset, GetMinimapTexture(layer));
}

void MiniMap::RenderTilesToTexture(const TerrainPatch& patch)
{
	for (int layer = Terrain::patchLayers-1; layer >= 0; --layer)
	for (int xPatch = 0; xPatch < Terrain::patchSize; ++xPatch)
	for (int yPatch = 0; yPatch < Terrain::patchSize; ++yPatch)
	{
		const TextureID tileTexture = GetMinimapTexture(layer);
		const TerrainTile& tile = patch.GetTileLocal(xPatch, yPatch, layer);
		const Vector2 tileOffset = patch.GetTilePos(xPatch, yPatch);
		RenderTileToTexture(tile, tileOffset, tileTexture);
	}
}

void MiniMap::RenderObjectsToTexture(const TerrainPatch& patch)
{
	for (list<GameObjectStub>::const_iterator it = patch.objectStubs.begin(); it != patch.objectStubs.end(); ++it) 
	{       
		const GameObjectStub& stub = *it;
		stub.GetObjectInfo().StubRenderMap(stub);
	}
}

void MiniMap::RedrawPatch(const TerrainPatch& patch)
{
	{
		MapRenderBlock mapRenderBlock(mapFullTexture, false);
		RenderTilesToTexture(patch);
		RenderObjectsToTexture(patch);
	}

	RenderMapHiddenToTexture();
}

void MiniMap::RenderTileToTexture(const TerrainTile& tile, const Vector2& tileOffset, const TextureID tileTexture)
{
	if (!Terrain::GetTileSetHasCollision(tile.GetTileSet()))
		return;	// skip if tile set has no collision

	if (tile.IsFull())
	{
		const BYTE surface1Index = tile.GetSurfaceData(1);
		const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface1Index);
		RenderTileToTexture(tile, tileOffset, surfaceInfo, tileTexture);
		return;
	}
	
	if (tile.IsAreaClear())
	{
		const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(0);
		RenderTileToTexture(tile, tileOffset, surfaceInfo, tileTexture);
	}

	// draw both sides of the tile
	const BYTE surface0Index = tile.GetSurfaceData(0);
	if (surface0Index != 0 && tile.GetSurfaceHasArea(0))
	{
		const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface0Index);
		RenderTileToTexture(tile, tileOffset, surfaceInfo, tileTexture);
	}
	const BYTE surface1Index = tile.GetSurfaceData(1);
	if (surface1Index != 0 && tile.GetSurfaceHasArea(1))
	{
		const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface1Index);
		RenderTileToTexture(tile, tileOffset, surfaceInfo, tileTexture);
	}
}

void MiniMap::RenderTileToTexture(const TerrainTile& tile, const Vector2& tileOffset, const GameSurfaceInfo& surfaceInfo, const TextureID tileTexture)
{
	// tile set mode
	const int surfaceID = surfaceInfo.GetID();
	const ByteVector2 tileSize(16, 16);
	const ByteVector2 tilePos(surfaceID % 16, surfaceID / 16);
	const Color color = (tile.IsClear() || tile.IsAreaClear()) ? Color::Black() : Color::White();

	g_render->RenderTile
	(
		tilePos,
		tileSize,
		XForm2(tileOffset + Vector2(0.5f)),
		Vector2(0.5),
		color,
		tileTexture,
		false
	);
}

MiniMap::MapRenderBlock::MapRenderBlock(LPDIRECT3DTEXTURE9 texture, bool needsClear)
{
	if (!texture)
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	pd3dDevice->GetRenderTarget(0, &backBuffer);

	// render to texture
	texture->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);

	g_render->BeginRender(needsClear, Color::Black());
	
	// hack: make camera not cull anything
	g_cameraBase->PrepareForRender(XForm2(0), 100000);

	// set display transforms
	D3DXMATRIX matrixProjection;
	Vector2 size = Vector2(Terrain::fullSize) * float(Terrain::patchSize) * TerrainTile::GetSize();
	D3DXMatrixOrthoLH(&matrixProjection, size.x, size.y, -1000, 1000);
	pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixProjection);

	D3DXMATRIX viewMatrix;
	D3DXMatrixIdentity(&viewMatrix);
	pd3dDevice->SetTransform(D3DTS_VIEW, &viewMatrix);

	// Set up the textures
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,	D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1,	D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2,	D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,	D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1,	D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2,	D3DTA_DIFFUSE );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSW,		D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER,		D3DTEXF_POINT );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER,		D3DTEXF_POINT );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER,		D3DTEXF_NONE );
	pd3dDevice->SetRenderState( D3DRS_SRCBLEND,				D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState( D3DRS_DESTBLEND,			D3DBLEND_INVSRCALPHA);

	// Set miscellaneous render states
	pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE,		TRUE);
	pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,		FALSE); 
	pd3dDevice->SetRenderState( D3DRS_DITHERENABLE,			FALSE );
	pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE,		FALSE );
	pd3dDevice->SetRenderState( D3DRS_ZENABLE,				FALSE);
	pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,			FALSE);
	pd3dDevice->SetRenderState( D3DRS_LIGHTING,				TRUE);
	pd3dDevice->SetRenderState( D3DRS_AMBIENT,				0x00FFFFFF);
	pd3dDevice->SetRenderState( D3DRS_CULLMODE,				D3DCULL_NONE);

	// set up texture transforming
	pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
}

MiniMap::MapRenderBlock::~MapRenderBlock()
{
	if (!renderSurface)
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	g_render->EndRender();

	if (mapSaveTexture)
	{
		mapSaveTexture = false;
		D3DXSaveSurfaceToFile( L"map.png", D3DXIFF_PNG, renderSurface, NULL, NULL );
	}
	
	pd3dDevice->SetRenderTarget(0, backBuffer);

	SAFE_RELEASE(renderSurface);
	SAFE_RELEASE(backBuffer);
}