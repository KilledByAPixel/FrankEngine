////////////////////////////////////////////////////////////////////////////////////////
/*
	Terrain Render
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../terrain/terrain.h"
#include "../terrain/terrainRender.h"
#include "../editor/tileEditor.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Renderer Globals
*/
////////////////////////////////////////////////////////////////////////////////////////

#define D3DFVF_TERRAIN_VERTEX (D3DFVF_XYZ|D3DFVF_TEX1)

// the one and only terrain renderer
TerrainRender g_terrainRender;

bool TerrainRender::enableRender = true;
ConsoleCommand(TerrainRender::enableRender, terrainRender)
	
bool TerrainRender::showEdgePoints = false;
ConsoleCommand(TerrainRender::showEdgePoints, showEdgePoints)
	
float TerrainRender::textureWrapScale = 8;
ConsoleCommand(TerrainRender::textureWrapScale, terrainTextureWrapScale)

bool TerrainRender::foregroundLayerOcculsion = false;
ConsoleCommand(TerrainRender::foregroundLayerOcculsion, foregroundLayerOcculsion)
	
// only update one cache per frame to prevent frame spike
bool TerrainRender::limitCacheUpdate = true;
ConsoleCommand(TerrainRender::limitCacheUpdate, limitCacheUpdate)

bool TerrainRender::cacheEnable = true;
ConsoleCommand(TerrainRender::cacheEnable, terrainCacheEnable)

bool TerrainRender::enableDiffuseLighting = false;
ConsoleCommand(TerrainRender::enableDiffuseLighting, terrainEnableDiffuseLighting)

float TerrainRender::editorBackgroundAlpha = 0.3f;
ConsoleCommand(TerrainRender::editorBackgroundAlpha, terrainEditorBackgroundAlpha)

float TerrainRender::editorForegroundAlpha = 0.05f;
ConsoleCommand(TerrainRender::editorForegroundAlpha, terrainEditorForegroundAlpha)

ConsoleCommandSimple(int, terrainRenderBatchDebug, 0)

////////////////////////////////////////////////////////////////////////////////////////
/*
	Member functions
*/
////////////////////////////////////////////////////////////////////////////////////////

TerrainRender::TerrainRender()
{
	renderTexture = NULL;
	tileBuffers = NULL;
	tileBufferCount = 0;
	renderedPrimitiveCount = 0;
	renderedBatchCount = 0;
	cacheNeedsFullRefresh = true;
}

TerrainRender::~TerrainRender()
{
	SAFE_RELEASE(renderTexture);
}

void TerrainRender::Update()
{
	if (!enableRender || !cacheEnable || !g_gameControlBase->IsGameplayMode())
	{
		for (int k = 0; k < tileBufferCount; ++k)
		for (int l = 0; l < Terrain::patchLayers; ++l)
			UncacheTilesPrimitives(k, l);

		return;
	}

	// get current position
	const Vector2& pos = g_gameControlBase->GetStreamCenter();
	const IntVector2 patchIndex = g_terrain->GetPatchIndex(pos);

	// get the render widnow size
	int window = Terrain::renderWindowSize;
	int i1 =patchIndex.x-window;
	int i2 =patchIndex.x+window;
	int j1 =patchIndex.y-window;
	int j2 =patchIndex.y+window;

	// check if window size changed and trigger device reset
	static int lastWindow = window;
	if (lastWindow != window)
	{
		DXUTResetDevice();
		lastWindow = window;
	}

	for (int k = 0; k < tileBufferCount; ++k)
	{
		// uncache tiles no longer in the window
		const TerrainPatch* patch = tileBuffers[k].patch;
		if (!patch)
			continue;
		
		const Vector2 centerPatchPos = patch->GetCenter();
		const IntVector2 checkPatchIndex = g_terrain->GetPatchIndex(centerPatchPos);
		if (checkPatchIndex.x < i1 || checkPatchIndex.x > i2 || checkPatchIndex.y < j1 || checkPatchIndex.y > j2)
		{
			for (int l = 0; l < Terrain::patchLayers; ++l)
				UncacheTilesPrimitives(k, l);
		}
	}

	UpdateCache();
}
	
void TerrainRender::ClearCache()
{
	// clear cache
	for (int k = 0; k < tileBufferCount; ++k)
	for (int l = 0; l < Terrain::patchLayers; ++l)
		UncacheTilesPrimitives(k, l);

	cacheNeedsFullRefresh = true;
}

void TerrainRender::UpdateCache()
{
	// must have a player to refresh the cache
	if (!g_gameControlBase->GetPlayer())
		return;

	// get current position
	const Vector2& pos = g_gameControlBase->GetStreamCenter();
	const IntVector2 patchOffset = g_terrain->GetPatchIndex(pos);

	// get the render widnow size
	int window = Terrain::renderWindowSize;
	int i1 = patchOffset.x-window;
	int i2 = patchOffset.x+window;
	int j1 = patchOffset.y-window;
	int j2 = patchOffset.y+window;

	// loop over all the patches and tiles in the window
	for(int i=i1; i<=i2; ++i)
	for(int j=j1; j<=j2; ++j)
	{
		if (!g_terrain->IsPatchIndexValid(IntVector2(i,j)))
			continue;
		
		const TerrainPatch& patch = *g_terrain->GetPatch(i, j);

		// check if patch is cached
		bool cached = false;
		for (int k = 0; k < tileBufferCount; ++k)
		{
			if (&patch == tileBuffers[k].patch)
			{
				cached = true;
				break;
			}
		}
		if (!cached)
		{
			// cache the patch in an open slot
			for (int k = 0; k < tileBufferCount; ++k)
			{
				if (!tileBuffers[k].patch)
				{
					for (int l = 0; l < Terrain::patchLayers; ++l)
						CacheTilesPrimitives(k, patch, l);
					break;
				}
			}

			// opt: only rebuild one patch per update
			if (limitCacheUpdate && !cacheNeedsFullRefresh)
				return;
		}
	}

	cacheNeedsFullRefresh = false;
}

bool TerrainRender::IsPatchCached(TerrainPatch& patch)
{
	if (!cacheEnable)
		return true;

	// check if patch is cached
	for (int k = 0; k < tileBufferCount; ++k)
	{
		if (&patch == tileBuffers[k].patch)
			return true;
	}

	return false;
}

void TerrainRender::RefereshCached(TerrainPatch& patch)
{
	// check if patch is cached
	for (int k = 0; k < tileBufferCount; ++k)
	{
		if (&patch == tileBuffers[k].patch)
		{
			for (int l = 0; l < Terrain::patchLayers; ++l)
			{
				UncacheTilesPrimitives(k, l);
				CacheTilesPrimitives(k, patch, l);
			}
			break;
		}
	}
}

void TerrainRender::Render(const Terrain& terrain, const Vector2 &pos, int layer, float alpha)
{
	FrankProfilerEntryDefine(L"TerrainRender::Render()", Color::White(), 4);
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"TerrainRender::Render()" );
	
	if (!enableRender)
		return;
	
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); 
	//pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	//pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

	// hack: this seems to fix weird flicker issue with diffuse render
	const DWORD d3dLighting = (!enableDiffuseLighting && !DeferredRender::GetRenderPass() && g_gameControlBase->IsGameplayMode()) ? FALSE : TRUE;
	pd3dDevice->SetRenderState(D3DRS_LIGHTING, d3dLighting);
	
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

	static DWORD savedAmbient = 0;
	pd3dDevice->GetRenderState( D3DRS_AMBIENT, &savedAmbient );
	if (DeferredRender::GetRenderPassIsShadow() || DeferredRender::GetRenderPassIsEmissive())
	{
		// terrain handles it's own render pass ambient stuff
		pd3dDevice->SetRenderState(D3DRS_AMBIENT, Color::White());
	}
	
	pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 );

	if (cacheEnable && g_gameControlBase->IsGameplayMode())
	{
		for (int k = 0; k < tileBufferCount; ++k)
			RenderCached(k, layer, alpha);
	}
	else
	{
		// get render window extents
		const IntVector2 patchIndex = g_terrain->GetPatchIndex(pos);
		int i1 = patchIndex.x-Terrain::renderWindowSize;
		int i2 = patchIndex.x+Terrain::renderWindowSize;
		int j1 = patchIndex.y-Terrain::renderWindowSize;
		int j2 = patchIndex.y+Terrain::renderWindowSize;
		if (!g_gameControlBase->IsGameplayMode())
			g_editor.GetTerrainRenderWindow(patchIndex.x, patchIndex.y, i1, i2, j1, j2);

		// loop over all the patches and tiles in the window
		for(int i = i1; i <= i2; ++i)
		for(int j = j1; j <= j2; ++j)
		{
			if (!terrain.IsPatchIndexValid(IntVector2(i,j)))
				continue;

			const TerrainPatch& patch = *terrain.GetPatch(i, j);
			RenderSlow(patch, layer, alpha);
		}
	}

	pd3dDevice->SetRenderState(D3DRS_AMBIENT, savedAmbient);
	pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
	pd3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);
}

void TerrainRender::UncacheTilesPrimitives(int tileBufferIndex, int layer)
{
	CachedTileBuffer& tileBuffer = tileBuffers[tileBufferIndex + layer*tileBufferCount];
	tileBuffer.patch = NULL;
}

void TerrainRender::CacheTilesPrimitives(int tileBufferIndex, const TerrainPatch& patch, int layer)
{
	CachedTileBuffer& tileBuffer = tileBuffers[tileBufferIndex + layer*tileBufferCount];
	tileBuffer.patch = &patch;

	struct SurfaceEdgeTile
	{
		SurfaceEdgeTile(Vector2 _pos, BYTE surface, BYTE edge, BYTE _tileSet, BYTE _rotation, bool _mirror) : pos(_pos), surfaceData(surface), edgeData(edge), tileSet(_tileSet), rotation(_rotation), mirror(_mirror) {}

		static bool SortCompare(const SurfaceEdgeTile& first, const SurfaceEdgeTile& second) 
		{ 
			if (first.tileSet != second.tileSet)
				return (first.tileSet < second.tileSet);
			
			const GameSurfaceInfo& s1 = GameSurfaceInfo::Get(first.surfaceData);
			const GameSurfaceInfo& s2 = GameSurfaceInfo::Get(second.surfaceData);
			return s1 < s2;
		}

		Vector2 pos;
		BYTE surfaceData;
		BYTE edgeData;
		BYTE tileSet;
		BYTE rotation;
		bool mirror;
	};
		
	// create a list of all the tile surfaces in this patch
	list<SurfaceEdgeTile> tileList;
	for(int xPatch=0; xPatch<Terrain::patchSize; ++xPatch)
	for(int yPatch=0; yPatch<Terrain::patchSize; ++yPatch)
	{
		const TerrainTile& tile = patch.GetTileLocal(xPatch, yPatch, layer);
		if (tile.IsClear())
			continue; // skip clear tiles

		if (Terrain::tileSetCount > 0 && tile.GetTileSet() >= Terrain::tileSetCount)
			continue;	// tileset out of range

		if (foregroundLayerOcculsion && layer == 1)
		{
			// check if background tiles are completely blocked by foreground tile
			const TerrainTile& tile0 = patch.GetTileLocal(xPatch, yPatch, 0);
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(tile0.GetSurfaceData(0));
			if (tile0.IsFull() && !tile0.IsClear() && (surfaceInfo.flags & GSI_ForegroundOcclude))
				continue;
		}
			
		const Vector2 pos = patch.GetTilePos(xPatch, yPatch);
		const BYTE surface0 = tile.GetSurfaceData(0);
		if (surface0 && tile.GetSurfaceHasArea(0))
			tileList.push_back(SurfaceEdgeTile(pos, surface0, tile.GetEdgeData(), tile.GetTileSet(), tile.GetRotation(), tile.GetMirror())); 
			
		const BYTE surface1 = tile.GetSurfaceData(1);
		if (surface1 && tile.GetSurfaceHasArea(1))
			tileList.push_back(SurfaceEdgeTile(pos, surface1, tile.GetInvertedEdgeData(), tile.GetTileSet(), tile.GetRotation(), tile.GetMirror())); 
	}
		
	// sort list by surface info type
	tileList.sort(SurfaceEdgeTile::SortCompare);

	// clear out the tile buffer
	for (int i = 0; i < CachedTileBuffer::maxGroups; ++i)
		tileBuffer.renderGroups[i].vertexCount = 0;

	// iterate through list and build vertex buffer
	TERRAIN_VERTEX* lockedVerts;
	if (!SUCCEEDED(tileBuffer.rp.vb->Lock(0, 0, (VOID**)&lockedVerts, D3DLOCK_DISCARD)))
		return;

	int vertexIndex = 0;
	tileBuffer.groupCount = 0;
	BYTE lastSurfaceData = 0;
	BYTE lastTileSet = 0;
	bool first = true;

	for (list<SurfaceEdgeTile>::iterator it = tileList.begin(); it != tileList.end(); ++it) 
	{
		SurfaceEdgeTile& tile = *it;

		// get vert list for the tile
		const Vector2 *edgeVerts = NULL;
		int vertexCount;
		TerrainTile::GetVertList(edgeVerts, vertexCount, tile.edgeData);
		const BYTE surfaceData = tile.surfaceData;
		const BYTE rotation = tile.rotation;
		const bool mirror = tile.mirror;
		const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(tile.surfaceData);
		const GameSurfaceInfo& lastSurfaceInfo = GameSurfaceInfo::Get(lastSurfaceData);

		// check if it's a new group
		if (first || !surfaceInfo.IsSameRenderGroup(lastSurfaceInfo) || tile.tileSet != lastTileSet )
		{
			// init the new group
			first = false;
			++tileBuffer.groupCount;
			CachedTileBuffer::TileRenderGroup& tileRenderGroup = tileBuffer.renderGroups[tileBuffer.groupCount-1];
			tileRenderGroup.startVertex = vertexIndex;
			tileRenderGroup.tileSet = tile.tileSet;
			tileRenderGroup.surfaceID = surfaceData;
			lastSurfaceData = surfaceData;
			lastTileSet = tile.tileSet;
		}
			
		CachedTileBuffer::TileRenderGroup& tileRenderGroup = tileBuffer.renderGroups[tileBuffer.groupCount-1];
		
		// degenerate tri on start
		SetupVert(&lockedVerts[vertexIndex++], tile.pos + edgeVerts[0], edgeVerts[0], surfaceInfo, surfaceData, rotation, mirror);
		SetupVert(&lockedVerts[vertexIndex++], tile.pos + edgeVerts[0], edgeVerts[0], surfaceInfo, surfaceData, rotation, mirror);
			
		int leftPos = vertexCount-1;
		int rightPos = 1;
		while (1)
		{
			Vector2 edgeVert = edgeVerts[rightPos++];
			Vector2 pos = tile.pos + edgeVert;
			SetupVert(&lockedVerts[vertexIndex++], pos, edgeVert, surfaceInfo, surfaceData, rotation, mirror);
			if (rightPos > leftPos)
			{
				// degerate tri on ends
				SetupVert(&lockedVerts[vertexIndex++], pos, edgeVert, surfaceInfo, surfaceData, rotation, mirror);
				break;
			}
				
			edgeVert = edgeVerts[leftPos--];
			pos = tile.pos + edgeVert;
			SetupVert(&lockedVerts[vertexIndex++], pos, edgeVert, surfaceInfo, surfaceData, rotation, mirror);
			if (leftPos < rightPos)
			{
				// degerate tri on ends
				SetupVert(&lockedVerts[vertexIndex++], pos, edgeVert, surfaceInfo, surfaceData, rotation, mirror);
				break;
			}
		}

		tileRenderGroup.vertexCount += vertexCount + 2;
	}
	tileBuffer.rp.vb->Unlock();
}

void TerrainRender::RenderCached(int tileBufferIndex, int layer, float alpha)
{
	const CachedTileBuffer& tileBuffer = tileBuffers[tileBufferIndex + layer*tileBufferCount];

	if (!tileBuffer.patch)
		return;

	// do a camera test on the full patch
	if (!g_cameraBase->CameraTest(tileBuffer.patch->GetAABB()))
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const DeferredRender::RenderPass renderPass = DeferredRender::GetRenderPass();

	// set the world transform
	D3DXMATRIX matrixIdentity;
	D3DXMatrixIdentity(&matrixIdentity);
	pd3dDevice->SetTransform(D3DTS_WORLD, &matrixIdentity);

	// set the primitive
	pd3dDevice->SetStreamSource(0, tileBuffer.rp.vb, 0, tileBuffer.rp.stride);
	pd3dDevice->SetFVF(tileBuffer.rp.fvf);

	for (int i = 0; i < CachedTileBuffer::maxGroups; ++i)
	{
		// push out a render for each tile group
		const CachedTileBuffer::TileRenderGroup& tileRenderGroup = tileBuffer.renderGroups[i];
		const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(tileRenderGroup.surfaceID);

		if (tileRenderGroup.vertexCount <= 2)
			continue;

		// set the texture
		LPDIRECT3DTEXTURE9 texture = NULL;
		if (Terrain::tileSetCount > 0)
		{
			const TextureID ti = Terrain::GetTileSetTexture(tileRenderGroup.tileSet);
			texture = g_render->GetTexture(ti);
		}
		else
			texture = g_render->GetTexture(surfaceInfo.ti);

		// set the color
		Color color = layer == 0? surfaceInfo.color : surfaceInfo.backgroundColor;
		if (DeferredRender::GetRenderPassIsShadow())
		{
			color = surfaceInfo.shadowColor;
			
			// tile set override to not cast shadows or have transparent shadows
			if (Terrain::GetTileSetInfo(tileRenderGroup.tileSet).hasShadowOverride)
				color = Terrain::GetTileSetInfo(tileRenderGroup.tileSet).shadowOverrideColor;
		}
		else if (renderPass == DeferredRender::RenderPass_emissive)
			color = surfaceInfo.emissiveColor;
		color.a *= alpha;

		if (DeferredRender::GetRenderPassIsDeferred())
		{
			if (!texture)
			{
				color = DeferredRender::GetRenderPassIsNormalMap()? Color(0.5f, 0.5f, 1.0f, color.a) : Color(0.0f, 0.0f, 0.0f, color.a);
				
				// use the alpha channel from the diffuse map
				if (Terrain::tileSetCount > 0)
				{
					const TextureID ti = Terrain::GetTileSetTexture(tileRenderGroup.tileSet);
					texture = g_render->GetTexture(ti, false);
				}
				else
					texture = g_render->GetTexture(surfaceInfo.ti, false);
				
				pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
				pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
				pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
				pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			}
			else if (DeferredRender::GetRenderPassIsEmissive() || DeferredRender::GetRenderPassIsSpecular() || DeferredRender::GetRenderPassIsNormalMap())
			{
				color = Color::White();

				// use the alpha channel from the diffuse map for emissive, specular, and normals
				LPDIRECT3DTEXTURE9 textureDiffuse = NULL;
				if (Terrain::tileSetCount > 0)
				{
					const TextureID ti = Terrain::GetTileSetTexture(tileRenderGroup.tileSet);
					textureDiffuse = g_render->GetTexture(ti, false);
				}
				else
					textureDiffuse = g_render->GetTexture(surfaceInfo.ti, false);
					
				pd3dDevice->SetTexture( 1, textureDiffuse );
				pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
				pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
				pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
				pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
				pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG2, D3DTA_CURRENT );
			}
			else
				color = Color::White();
		}

		pd3dDevice->SetTexture(0, texture);

		const D3DMATERIAL9 material =
		{
			{0, 0, 0, color.a}, 
			{color.r, color.g, color.b, 1}, 
			{0, 0, 0, 0},
			{0, 0, 0, 0}, 0
		};
		pd3dDevice->SetMaterial(&material);
		
		if (terrainRenderBatchDebug == 0 || (terrainRenderBatchDebug == renderedBatchCount && DeferredRender::GetRenderPass() == 0))
		{
			UINT primitiveCount = tileRenderGroup.vertexCount - 2;
			pd3dDevice->DrawPrimitive(tileBuffer.rp.primitiveType, tileRenderGroup.startVertex, primitiveCount);
			renderedPrimitiveCount += primitiveCount;
		}
		if (DeferredRender::GetRenderPass() == 0)
			++renderedBatchCount; // only count the diffuse for rendered batch debug display

		{
			// set stuff back to normal
			pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
			pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
			pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
			pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
			pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
			pd3dDevice->SetTexture( 1, NULL );
		}
	}
}

void TerrainRender::RenderTiles(TerrainTile* tiles, const Vector2& pos, int width, int height, int layers, int firstLayer, float alpha)
{
	// special rendering system for the selected tiles while editing
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	
	// set up texture transforming
	pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	pd3dDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
	pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
	const Vector2 halfOffset = Vector2(0.5f * TerrainTile::GetSize());
	const TerrainTile* mouseTile = g_terrain->GetTile(g_input->GetMousePosWorldSpace());

	for(int x=0; x<width; ++x)
	for(int y=0; y<height; ++y)
	for(int l=layers-1; l>=0; --l)
	{
		const int actualLayer = l + firstLayer;

		TerrainTile& tile = tiles[width*height*l + height*x + y];
		if (tile.IsClear())
			continue; // skip clear tiles

		// check if it is on screen
		const Vector2 tileOffset = pos + TerrainTile::GetSize()*Vector2(float(x), float(y));
		if (!g_cameraBase->CameraTest(tileOffset + halfOffset, TerrainTile::GetRadius()))
			continue;

		const BYTE rotation = tile.GetRotation();
		const bool mirror = tile.GetMirror();
		const BYTE surface0Index = tile.GetSurfaceData(0);
		if (surface0Index != 0 && tile.GetSurfaceHasArea(0))
		{
			// draw side 1
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface0Index);
			const BYTE edgeIndex = tile.GetEdgeData();
			Color c = actualLayer == 0? surfaceInfo.color : surfaceInfo.backgroundColor;
			c.a *= alpha;
			RenderTile(tileOffset, edgeIndex, surface0Index, tile.GetTileSet(), surfaceInfo, c, rotation, mirror);
		}
		const BYTE surface1Index = tile.GetSurfaceData(1);
		if (surface1Index != 0 && tile.GetSurfaceHasArea(1))
		{
			// draw side 2
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface1Index);
			const BYTE edgeIndex = tile.GetInvertedEdgeData();
			Color c = actualLayer == 0? surfaceInfo.color : surfaceInfo.backgroundColor;
			c.a *= alpha;
			RenderTile(tileOffset, edgeIndex, surface1Index, tile.GetTileSet(), surfaceInfo, c, rotation, mirror);
		}
		{
			// draw edge lines
			static const Vector2 halfOffset = Vector2(TerrainTile::GetSize()/2, TerrainTile::GetSize()/2);
			{
				if (g_cameraBase->CameraTest(tileOffset + halfOffset, TerrainTile::GetRadius()))
				{
					// draw a dashed line to show the edge
					const Vector2 posStart = tileOffset + tile.GetPosA();
					const Vector2 posEnd = tileOffset + tile.GetPosB();
					const Vector2 deltaPos = posEnd - posStart;
					g_render->DrawLine(posStart, posEnd, Terrain::editorTileEdgeColor1);
					int dashCount = (int)(8*deltaPos.Length() / TerrainTile::GetSize());
					for (int i = 0; i < dashCount; ++i)
					{
						if (i % 2)
							continue;

						const Vector2 pos1 = posStart + deltaPos * (float)i / (float)dashCount;
						const Vector2 pos2 = posStart + deltaPos * (float)(i+1) / (float)dashCount;
						g_render->DrawLine(pos1, pos2, Terrain::editorTileEdgeColor2);
					}
				}
			}
		}
	}

	pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pd3dDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	g_render->RenderSimpleVerts();
}

void TerrainRender::RenderSlow(const TerrainPatch& patch, int layer, float alpha, const Vector2& offset, bool cameraTest)
{
	// do a camera test on the full patch
	if (cameraTest && !g_cameraBase->CameraTest(patch.GetAABB()))
		return;

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	const DeferredRender::RenderPass renderPass = DeferredRender::GetRenderPass();

	// set up texture transforming
	pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	pd3dDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	const Vector2 halfOffset = Vector2(0.5f * TerrainTile::GetSize());

	for(int xPatch=0; xPatch<Terrain::patchSize; ++xPatch)
	for(int yPatch=0; yPatch<Terrain::patchSize; ++yPatch)
	{
		const TerrainTile& tile = patch.GetTileLocal(xPatch, yPatch, layer);
		if (tile.IsClear())
			continue; // skip clear tiles

		// check if it is on screen
		const Vector2 tileOffset = offset + patch.GetTilePos(xPatch, yPatch);
		if (cameraTest && !g_cameraBase->CameraTest(tileOffset + halfOffset, TerrainTile::GetRadius()))
			continue;

		const BYTE rotation = tile.GetRotation();
		const bool mirror = tile.GetMirror();
		const BYTE surface0Index = tile.GetSurfaceData(0);
		if (surface0Index != 0 && tile.GetSurfaceHasArea(0))
		{
			// draw side 1
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface0Index);
			const BYTE edgeIndex = tile.GetEdgeData();
			Color c = layer == 0? surfaceInfo.color : surfaceInfo.backgroundColor;
			if (DeferredRender::GetRenderPassIsShadow())
				c = surfaceInfo.shadowColor;
			else if (renderPass == DeferredRender::RenderPass_emissive)
				c = surfaceInfo.emissiveColor;
			c.a *= alpha;
			RenderTile(tileOffset, edgeIndex, surface0Index, tile.GetTileSet(), surfaceInfo, c, rotation, mirror);
		}
		const BYTE surface1Index = tile.GetSurfaceData(1);
		if (surface1Index != 0 && tile.GetSurfaceHasArea(1))
		{
			// draw side 2
			const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface1Index);
			const BYTE edgeIndex = tile.GetInvertedEdgeData();
			Color c = layer == 0? surfaceInfo.color : surfaceInfo.backgroundColor;
			if (DeferredRender::GetRenderPassIsShadow())
				c = surfaceInfo.shadowColor;
			else if (renderPass == DeferredRender::RenderPass_emissive)
				c = surfaceInfo.emissiveColor;
			c.a *= alpha;
			RenderTile(tileOffset, edgeIndex, surface1Index, tile.GetTileSet(), surfaceInfo, c, rotation, mirror);
		}
		
		if (showEdgePoints && !renderPass && tile.GetEdgeData() != 0)
		{
			// draw edge points for debugging
			const Vector2 posA = tile.GetPosA() + tileOffset;
			const Vector2 posB = tile.GetPosB() + tileOffset;
			posA.RenderDebug(Color::Red(0.5f));
			posB.RenderDebug(Color::Blue(0.5f));
		}
	}

	pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	pd3dDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
}

void TerrainRender::RenderToTexture(const Terrain& terrain, RenderToTileCallback preCallback, RenderToTileCallback postCallback)
{
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

	if (!renderTexture)
	{
		const int textureSize = 1024;
		if (!SUCCEEDED(pd3dDevice->CreateTexture(
				textureSize, textureSize, 1,  // width, height, mip levels
				D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8,
				D3DPOOL_DEFAULT, &renderTexture, NULL)))
			return;
	}
	
	LPDIRECT3DSURFACE9 backBuffer = NULL;
	pd3dDevice->GetRenderTarget(0,&backBuffer);

	// render to texture
	LPDIRECT3DSURFACE9 renderSurface = NULL;
	renderTexture->GetSurfaceLevel(0, &renderSurface);
	pd3dDevice->SetRenderTarget(0, renderSurface);

	g_render->BeginRender(true, Color::Black());
	{
		// set display transforms
		D3DXMATRIX matrixProjection;
		Vector2 size = Vector2(Terrain::fullSize) * float(Terrain::patchSize) * TerrainTile::GetSize();
		D3DXMatrixOrthoLH(&matrixProjection, size.x, size.y, -1000, 1000);
		pd3dDevice->SetTransform(D3DTS_PROJECTION, &matrixProjection);

		D3DXMATRIX viewMatrix;
		D3DXMatrixIdentity(&viewMatrix);
		pd3dDevice->SetTransform(D3DTS_VIEW, &viewMatrix);
		
		pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); 
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		// Set up the textures
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
		pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		// Set miscellaneous render states
		pd3dDevice->SetRenderState( D3DRS_DITHERENABLE,   FALSE );
		pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
		pd3dDevice->SetRenderState( D3DRS_AMBIENT,        0x00FFFFFF );
		pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE);
		pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE);
		pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE);
		pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE);
		pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE);
		pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE);

		if (preCallback)
			preCallback();

		// set up texture transforming
		pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

		// render all the tiles
		for(int i=0; i<Terrain::fullSize.x; ++i)
		for(int j=0; j<Terrain::fullSize.y; ++j)
		{
			const TerrainPatch& patch = *terrain.GetPatch(i, j);

			for(int xPatch=0; xPatch<Terrain::patchSize; ++xPatch)
			for(int yPatch=0; yPatch<Terrain::patchSize; ++yPatch)
			{
				const TerrainTile& tile = patch.GetTileLocal(xPatch, yPatch);
				if (tile.IsClear())
					continue; // skip clear tiles
				
				const Vector2 tileOffset = patch.GetTilePos(xPatch, yPatch);
				{
					// draw both sides of the tile
					const BYTE rotation = tile.GetRotation();
					const bool mirror = tile.GetMirror();
					const BYTE surface0Index = tile.GetSurfaceData(0);
					if (surface0Index != 0 && tile.GetSurfaceHasArea(0))
					{
						const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface0Index);
						const BYTE edgeIndex = tile.GetEdgeData();
						RenderTile(tileOffset, edgeIndex, surface0Index, tile.GetTileSet(), surfaceInfo, surfaceInfo.color, rotation, mirror);
					}
					const BYTE surface1Index = tile.GetSurfaceData(1);
					if (surface1Index != 0 && tile.GetSurfaceHasArea(1))
					{
						const GameSurfaceInfo& surfaceInfo = GameSurfaceInfo::Get(surface1Index);
						const BYTE edgeIndex = tile.GetInvertedEdgeData();
						RenderTile(tileOffset, edgeIndex, surface1Index, tile.GetTileSet(), surfaceInfo, surfaceInfo.color, rotation, mirror);
					}
				}
			}
		}

		pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		if (postCallback)
			postCallback();
	}
	g_render->EndRender();
	
	renderTexture->GenerateMipSubLevels();

    //D3DXSaveSurfaceToFile( L"terrain.png", D3DXIFF_PNG, renderSurface, NULL, NULL );
	
	pd3dDevice->SetRenderTarget(0, backBuffer);

	SAFE_RELEASE(renderSurface);
	SAFE_RELEASE(backBuffer);
}

void TerrainRender::InitDeviceObjects()
{
	// create a render primitive for each tile
	for (int tileIndex = 0; tileIndex < TERRAIN_UNIQUE_TILE_COUNT; ++tileIndex)
	{
		FrankRender::RenderPrimitive& rp = tilePrimitives[tileIndex];

		if 
		(
			!rp.Create
			(
				0,							// primitiveCount
				TERRAIN_TILE_MAX_VERTS,		// vertexCount
				D3DPT_TRIANGLESTRIP,		// primitiveType
				sizeof(TERRAIN_VERTEX),		// stride
				D3DFVF_TERRAIN_VERTEX		// fvf
			)
		)
			return;

		TERRAIN_VERTEX* vertices;
		if (!SUCCEEDED(rp.vb->Lock(0, 0, (VOID**)&vertices, D3DLOCK_DISCARD)))
			return;

		// fill in the tri strip
		BuildTriStrip(vertices, rp, tileIndex);
		
		rp.vb->Unlock();
	} 

	const int patchVertCount = Terrain::patchSize*Terrain::patchSize*(TERRAIN_TILE_MAX_VERTS+2)*2;
	const int tileBufferWidth = Terrain::renderWindowSize * 2 + 1;
	tileBufferCount = tileBufferWidth*tileBufferWidth;
	tileBuffers = new CachedTileBuffer[tileBufferCount*Terrain::patchLayers];
	for (int i = 0; i < tileBufferCount*Terrain::patchLayers; ++i)
	{
		tileBuffers[i].rp.Create
		(
			0,							// primitiveCount
			patchVertCount,				// vertexCount
			D3DPT_TRIANGLESTRIP,		// primitiveType
			sizeof(TERRAIN_VERTEX),		// stride
			D3DFVF_TERRAIN_VERTEX,		// fvf
			true						// dynamic
		);

		tileBuffers[i].patch = NULL;

		// clear render groups
		for (int j = 0; j < CachedTileBuffer::maxGroups; ++j)
		{
			CachedTileBuffer::TileRenderGroup& trg = tileBuffers[i].renderGroups[j];
			trg.startVertex = 0;
			trg.vertexCount = 0;
			trg.surfaceID = 0;
			trg.tileSet = 0;
		}
	}

	ClearCache();
}

void TerrainRender::DestroyDeviceObjects()
{
	for (int tileIndex = 0; tileIndex < TERRAIN_UNIQUE_TILE_COUNT; ++tileIndex)
		tilePrimitives[tileIndex].SafeRelease();

	SAFE_RELEASE(renderTexture);
	
	for (int i = 0; i < tileBufferCount*Terrain::patchLayers; ++i)
		tileBuffers[i].rp.SafeRelease();
	tileBufferCount = 0;

	delete [] tileBuffers;
	tileBuffers = NULL;
}

void TerrainRender::BuildTriStrip(TERRAIN_VERTEX* vertices, FrankRender::RenderPrimitive& rp, int tileIndex)
{
	const Vector2 *edgeVerts = NULL;
	int vertexCount;
	TerrainTile::GetVertList(edgeVerts, vertexCount, tileIndex);

	int i = 0;
	const Vector2 startPos = edgeVerts[0];
	SetupVertUnwrapped(&vertices[i++], startPos);

	int leftPos = vertexCount-1;
	int rightPos = 1;

	while (1)
	{
		SetupVertUnwrapped(&vertices[i++], edgeVerts[rightPos++]);
		if (rightPos > leftPos)
			break;

		SetupVertUnwrapped(&vertices[i++], edgeVerts[leftPos--]);
		if (leftPos < rightPos)
			break;
	}

	rp.vertexCount = vertexCount;
	rp.primitiveCount = vertexCount - 2;
	ASSERT(rp.primitiveCount >= 0);
}

inline void TerrainRender::RenderTile(const Vector2& position, const BYTE edgeIndex, const BYTE surfaceID, const BYTE tileSet, const GameSurfaceInfo& surfaceInfo, const Color& color, const BYTE rotation, bool mirror)
{
	if (Terrain::tileSetCount > 0)
	{
		const FrankRender::RenderPrimitive& rp = tilePrimitives[edgeIndex];
		ASSERT(rp.primitiveCount > 0);

		// tile set mode
		const ByteVector2 tileSize(16, 16);
		const ByteVector2 tilePos(surfaceID % 16, surfaceID / 16);
		const TextureID ti = Terrain::GetTileSetTexture(tileSet);

		g_render->RenderTile
		(
			tilePos,
			tileSize,
			Matrix44(position),
			color,
			ti,
			rp,
			rotation,
			mirror
		);
	}
	else if (g_render->GetTextureTileSheet(surfaceInfo.ti))
	{
		const FrankRender::RenderPrimitive& rp = tilePrimitives[edgeIndex];
		ASSERT(rp.primitiveCount > 0);

		g_render->Render
		(
			Matrix44(position),
			color,
			surfaceInfo.ti,
			rp
		);
	}
	else
	{
		IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
		{
			// set up the texture transform
			const Vector2 wrapScale = textureWrapScale*surfaceInfo.textureWrapSize;
			const Vector2 scale = 1.0f/wrapScale;
			Matrix44 matrix = Matrix44::BuildScale(scale.x, scale.y, 0);
			D3DMATRIX& d3dmatrix = matrix.GetD3DXMatrix();
			Vector2 texturePos = Vector2(1, -1) * position / (TerrainTile::GetSize()*wrapScale);

			// hack: not sure why this must be done so teture lines up the samne in immediate and cached modes
			texturePos.y -= 1/wrapScale.y;

			d3dmatrix._31 = texturePos.x;
			d3dmatrix._32 = texturePos.y;
			pd3dDevice->SetTransform(D3DTS_TEXTURE0, &d3dmatrix);
			pd3dDevice->SetTransform(D3DTS_TEXTURE1, &d3dmatrix);
		}

		const FrankRender::RenderPrimitive& rp = tilePrimitives[edgeIndex];
		ASSERT(rp.primitiveCount > 0);

		g_render->Render
		(
			Matrix44(position),
			color,
			surfaceInfo.ti,
			rp
		);
	}
}

inline void TerrainRender::SetupVert(TERRAIN_VERTEX* vertex, const Vector2& worldPos, const Vector2& localPos, const GameSurfaceInfo& surfaceInfo, const BYTE surfaceID, const BYTE tileRotation, bool tileMirror)
{
	if (Terrain::tileSetCount > 0 || g_render->GetTextureTileSheet(surfaceInfo.ti))
	{
		// tile set mode
		float rotation = float(tileRotation) * PI / 2;

		Vector2 localPos2 = Vector2(localPos.x, TerrainTile::GetSize() - localPos.y);
		localPos2 -= Vector2(0.5f);
		if (tileMirror)
			localPos2.x *= -1;
		localPos2 = localPos2.Rotate(rotation);
		localPos2 += Vector2(0.5f);

		if (g_tileSetUVScale > 0)
		{
			// hack: scale uvs when non point filtered
			const Vector2 offset(TerrainTile::GetSize()/2);
			localPos2 -= offset;
			localPos2 *= g_tileSetUVScale;
			localPos2 += offset;
		}
			
		const ByteVector2 tileSize(16, 16);
		const ByteVector2 tilePos(surfaceID % 16, surfaceID / 16);
		const Vector2 scale = 1.0f / (TerrainTile::GetSize() * Vector2(tileSize));
		const Vector2 tilePosFloat = localPos2 * scale + Vector2(tilePos) / Vector2(tileSize);

		vertex->position = Vector3(worldPos.x, worldPos.y, 0);
		vertex->textureCoords = tilePosFloat;
	}
	else
	{
		vertex->position = Vector3(worldPos.x, worldPos.y, 0);
		const Vector2 texturePos = worldPos / (TerrainTile::GetSize()*surfaceInfo.textureWrapSize*textureWrapScale);
		vertex->textureCoords = texturePos * Vector2(1, -1);
	}
}

inline void TerrainRender::SetupVertUnwrapped(TERRAIN_VERTEX* vertex, const Vector2& pos)
{
	vertex->position = Vector3(pos.x, pos.y, 0);
	const Vector2 texturePos = pos / (TerrainTile::GetSize());
	vertex->textureCoords = Vector2(texturePos.x, 1 - texturePos.y);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// TerrainLayerRender - a game object that is used to render layers of terrain
//

TerrainLayerRender::TerrainLayerRender(const Vector2& pos, int _layer, int _renderGroup) :
	GameObject(XForm2(pos)),
	layer(_layer),
	lightShadows(false),
	directionalShadows(false),
	emissiveLighting(true)
{
	SetRenderGroup(_renderGroup);

	if (layer == 0)
	{
		// layer 0 is set to cast light shadows by default
		lightShadows = true;
	}
	if (layer == 1)
	{
		// layer 0 is set to cast light shadows by default
		directionalShadows = true;
	}
}

void TerrainLayerRender::Render()
{
	if (DeferredRender::GetRenderPassIsEmissive()) 
	{
		if (!emissiveLighting)
			return;
	}
	else if (DeferredRender::GetRenderPassIsShadow())
	{
		if (DeferredRender::GetRenderPassIsLight() || DeferredRender::GetRenderPassIsVision())
		{
			if (!lightShadows) 
				return;
		}
		else if (!directionalShadows)
			return;
	}

	// terrain rendering handled by terain render class
	const Vector2 pos = g_gameControlBase->GetStreamCenter();
	float alpha = 1.0f;

	if (g_gameControlBase->IsTileEditMode() && !g_gameControlBase->IsEditPreviewMode())
	{
		const bool isSelctedLayer = g_editor.GetTileEditor().GetLayer() == layer;
		if (g_editor.IsRectangleSelecting() && g_editor.IsMultiLayerSelecting() || g_editor.GetTileEditor().HasSelection() && g_editor.GetTileEditor().GetSelectedLayerCount() > 1)
		{
			float alpha2 = alpha * (isSelctedLayer? 1.0f : TerrainRender::editorBackgroundAlpha);
			g_terrainRender.Render(*g_terrain, pos, layer, alpha2);
		}
		else if (isSelctedLayer)
		{
			if (TerrainRender::editorBackgroundAlpha > 0)
			{
				// render unselected layers behind
				float alpha2 = alpha * TerrainRender::editorBackgroundAlpha;
				for (int l = 0; l < Terrain::patchLayers; ++l)
				{
					if (layer != l)
						g_terrainRender.Render(*g_terrain, pos, l, alpha2);
				}
			}

			g_terrainRender.Render(*g_terrain, pos, layer, alpha);
			
			if (TerrainRender::editorForegroundAlpha > 0)
			{
				// render unselected layers in front with super low alpha
				float alpha3 = alpha * TerrainRender::editorForegroundAlpha;
				for (int l = 0; l < Terrain::patchLayers; ++l)
				{
					if (layer != l)
						g_terrainRender.Render(*g_terrain, pos, l, alpha3);
				}
			}
		}
	}
	else
		g_terrainRender.Render(*g_terrain, pos, layer, alpha);
}