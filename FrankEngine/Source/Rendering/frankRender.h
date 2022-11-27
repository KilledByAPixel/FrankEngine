////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Rendering
	Copyright 2013 Frank Force - http://www.frankforce.com

	- low level rendering for Frank engine
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#define MAX_TEXTURE_COUNT	(256)

enum TextureID;
const TextureID Texture_Invalid				= TextureID(0);
const TextureID Texture_Smoke				= TextureID(1);
const TextureID Texture_Circle				= TextureID(2);
const TextureID Texture_Dot					= TextureID(3);
const TextureID Texture_Arrow				= TextureID(4);
const TextureID Texture_LightMask			= TextureID(5);
const TextureID Texture_UNUSED				= TextureID(6);
const TextureID Texture_StartGameTextures	= TextureID(7);	// this is where gameside textures ids start

class FrankRender
{
public:

	FrankRender();

	void InitDeviceObjects();
	void DestroyDeviceObjects();

	bool LoadTexture(const WCHAR* textureName, TextureID ti);
	void ReleaseTextures();
	void ReloadModifiedTextures();
	
	// support for using texture tile sheets
	void SetTextureTile(const WCHAR* textureName, TextureID tileSheetTi, TextureID tileTi, const ByteVector2& tilePos, const ByteVector2& tileSize);
	void SetTextureTile(TextureID tileSheetTi, TextureID tileTi, const ByteVector2& tilePos, const ByteVector2& tileSize) { SetTextureTile(NULL, tileSheetTi, tileTi, tilePos, tileSize);}
	
	TextureID GetTextureTileSheet(TextureID ti) const { ASSERT(ti < MAX_TEXTURE_COUNT); return textures[ti][TT_Diffuse].tileSheetTi; }
	void GetTextureTileInfo(TextureID ti, ByteVector2& tilePos, ByteVector2& tileSize) const { ASSERT(ti < MAX_TEXTURE_COUNT); tilePos = textures[ti][TT_Diffuse].tilePos; tileSize = textures[ti][TT_Diffuse].tileSize; }

	LPDIRECT3DTEXTURE9 GetTexture(TextureID ti, bool deferredCheck = true) const;
	const WCHAR* GetTextureName(TextureID ti) const { return textures[ti][TT_Diffuse].name; }
	IntVector2 GetTextureSize(TextureID ti) const;
	IntVector2 GetTextureSize(LPDIRECT3DTEXTURE9 texture) const;
	D3DFORMAT GetTextureFormat(LPDIRECT3DTEXTURE9 texture) const;

	LPDIRECT3DTEXTURE9 FindTexture(const wstring& textureName) const;
	TextureID FindTextureID(const wstring& textureName) const;
	
	static Vector2 WorldSpaceToScreenSpace(const Vector2& p);
	bool LoadPixelShader(const WCHAR* name, LPDIRECT3DPIXELSHADER9& shader, LPD3DXCONSTANTTABLE& constants, bool loadFromResource = true);
	bool IsNormalMapShaderLoaded() const { return normalMapShader != NULL; }

	void SetFiltering() { SetPointFilter(g_usePointFiltering); }
	void SetPointFilter(bool pointFilter);

	// used by engine to begin and end rendering
	bool BeginRender(bool needsClear = false, const Color& clearColor = Color::White(), DWORD clearFlags = D3DCLEAR_TARGET);
	void EndRender();
	
	bool GetIsInRenderBlock() const { return isInRenderBlock; }

public:	// fast rendering (no textures)

	void DrawPolygon(const Vector2* vertices, int vertexCount, const DWORD color = Color::White());
	void DrawPolygon(const XForm2& xf, const Vector2* vertices, int vertexCount, const DWORD color = Color::White());
	void DrawSolidPolygon(const Vector2* vertices, int vertexCount, const DWORD color = Color::White());
	void DrawSolidPolygon(const Vector2* vertices, int vertexCount, const DWORD* colors);
	void DrawSolidPolygon(const XForm2& xf, const Vector2* vertices, int vertexCount, const DWORD color = Color::White());
	void DrawOutlinedPolygon(const XForm2& xf, const Vector2* vertices, int vertexCount, const DWORD color = Color::Black(), const DWORD outlineColor = Color::White());

	void DrawCircle(const XForm2& xf, const Vector2& size, const DWORD color = Color::White(), int sides = 24);
	void DrawSolidCircle(const XForm2& xf, const Vector2& size, const DWORD color = Color::White(), int sides = 24);
	void DrawOutlinedCircle(const XForm2& xf, const Vector2& size, const DWORD color = Color::Black(), const DWORD outlineColor = Color::White(), int sides = 24);

	void DrawCircle(const XForm2& xf, float radius, const DWORD color = Color::White(), int sides = 24) { DrawCircle(xf, Vector2(radius), color, sides); }
	void DrawSolidCircle(const XForm2& xf, float radius, const DWORD color = Color::White(), int sides = 24)  { DrawSolidCircle(xf, Vector2(radius), color, sides); }
	void DrawOutlinedCircle(const XForm2& xf, float radius, const DWORD color = Color::Black(), const DWORD outlineColor = Color::White(), int sides = 24) { DrawOutlinedCircle(xf, Vector2(radius), color, outlineColor, sides); }

	void DrawCone(const XForm2& xf, float radius, float coneAngle, const DWORD color = Color::White(), int divisions = 24);
	void DrawSolidCone(const XForm2& xf, float radius, float coneAngle, const DWORD color1 = Color::White(), const DWORD color2 = Color::White(), int divisions = 24);

	void DrawLine(const Vector2& p1, const Vector2& p2, const DWORD color = Color::White());
	void DrawLine(const XForm2& xf, const Vector2& p1, const Vector2& p2, const DWORD color = Color::White());

	void DrawLine(const Line2& l, const DWORD color = Color::White()) { DrawLine(l.p1, l.p2, color); }
	void DrawLine(const XForm2& xf, const Line2& l, const DWORD color = Color::White()) { DrawLine(xf, l.p1, l.p2, color); }
	void DrawThickLine(const Line2& l, float thickness = 0.1f, const DWORD color = Color::White());
	void DrawThickLineGradient(const Line2& l, float thickness = 0.1f, const DWORD color1 = Color::White(), const DWORD color2 = Color::White());

	void DrawBox(const XForm2& xf, const Vector2& size, const DWORD color = Color::White());
	void DrawSolidBox(const XForm2& xf, const Vector2& size, const DWORD color = Color::White());
	void DrawOutlinedBox(const XForm2& xf, const Vector2& size, const DWORD color = Color::White(), const DWORD outlineColor = Color::Black());
	void DrawAxisAlignedBox(const Box2AABB& box, const DWORD color = Color::White());

	void DrawXForm(const XForm2& xf, float radius = 1, const DWORD color = Color::White());

public:	// lower level functions for fast rendering

	// call these at beginning and end of simper vert rendering
	void RenderSimpleVerts();

	void AddPointToLineVerts(const Vector2& v, DWORD color);
	void AddPointToTriVerts(const Vector2& v, DWORD color);

	// call these before and after you start a shape to cap it off
	void CapLineVerts(const Vector2& v) { AddPointToLineVerts(v, 0); }
	void CapTriVerts(const Vector2& v) { AddPointToTriVerts(v, 0); AddPointToTriVerts(v, 0); }

	// call thse functions to set the current render group simple verts to be additive
	// note: this will affect all simple verts that are rendered out for the current group!
	void SetSimpleVertsAreAdditive(bool additive);
	bool GetSimpleVertsAreAdditive() const			{ return simpleVertsAreAdditive; }

public:	// rendering functions

	static Matrix44 GetScreenSpaceMatrix(float x, float y, float sx, float sy, float r = 0) 
	{ 
		Matrix44 matrixS = Matrix44::BuildScale(sx, -sy, 0);
		Matrix44 matrixR = Matrix44::BuildRotateZ(r);
		Matrix44 matrixP = Matrix44::BuildTranslate(Vector3(x, y, 0));
		return matrixS * matrixR * matrixP;
	}

	void RenderQuad
	(
		const XForm2& xf,
		const Vector2& size,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid,
		bool cameraCheck = true
	)
	{ 
		if (!cameraCheck || g_cameraBase->CameraTest(xf.position, fabs(size.x) + fabs(size.y)))
			Render(Matrix44::BuildScale(size) * Matrix44(xf), color, ti, primitiveQuad); 
	}

	void RenderQuad
	(
		const XForm2& xf,
		const Box2AABB& localBoundingBox,
		const Color& color,
		TextureID ti = Texture_Invalid,
		bool cameraCheck = true
	)
	{
		const Vector2 size = Vector2(localBoundingBox.upperBound - localBoundingBox.lowerBound) / 2;
		const Vector2 offset = localBoundingBox.lowerBound + size;
		
		if (!cameraCheck || g_cameraBase->CameraTest(xf.position, sqrtf(Max(localBoundingBox.upperBound.LengthSquared(), localBoundingBox.lowerBound.LengthSquared()))))
			Render(Matrix44::BuildScale(size) * Matrix44::BuildTranslate(offset.x, offset.y, 0) * Matrix44(xf), color, ti, primitiveQuad); 
	}
	
	void RenderQuadLine
	(
		const Line2& segment,
		float thickness = 0.1f,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid
	);

	void RenderQuad
	(
		const XForm2& xf,
		const Vector2& size,
		const Color& color,
		LPDIRECT3DTEXTURE9 texture
	)
	{ 
		Render
		(
			Matrix44::BuildScale(size) * Matrix44(xf),
			color,
			texture,
			primitiveQuad.vb,
			primitiveQuad.primitiveCount,
			primitiveQuad.primitiveType,
			primitiveQuad.stride,
			primitiveQuad.fvf
		);
	}
	
	void RenderQuadSimple
	(
		const XForm2& xf,
		const Vector2& size
	)
	{ 
		Render
		(
			Matrix44::BuildScale(size) * Matrix44(xf),
			primitiveQuad.vb,
			primitiveQuad.primitiveCount,
			primitiveQuad.primitiveType,
			primitiveQuad.stride,
			primitiveQuad.fvf
		);
	}

	void RenderQuad
	(
		const Matrix44& matrix,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid
	)
	{ Render(matrix, color, ti, primitiveQuad); }
	
	void RenderScreenSpaceQuadOutline
	(
		const Vector2& position,
		const Vector2& size,
		const Color& color = Color::White()
	)
	{
		const Matrix44 matrix = GetScreenSpaceMatrix(position.x, position.y, size.x, size.y);
		RenderScreenSpaceQuadOutline(matrix, color);
	}
	
	void RenderScreenSpaceQuad
	(
		const Box2AABB& box2,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid,
		const Color& outlineColor = Color::Black(0)
	)
	{
		const Vector2 size = Vector2(box2.upperBound - box2.lowerBound)/2.0f;
		RenderScreenSpaceQuad
		(
			Vector2(box2.lowerBound + size),
			size,
			color,
			ti,
			outlineColor
		);
	}

	void RenderScreenSpaceQuad
	(
		const XForm2& xf,
		const Vector2& size,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid,
		const Color& outlineColor = Color::Black(0)
	)
	{
		const Matrix44 matrix = GetScreenSpaceMatrix(xf.position.x, xf.position.y, size.x, size.y, xf.angle);
		g_render->RenderScreenSpaceQuad(matrix, color, ti);

		if (outlineColor.a)
			RenderScreenSpace(matrix, outlineColor, Texture_Invalid, primitiveQuadOutline);
	}

	void RenderScreenSpaceQuad
	(
		const XForm2& xf,
		const Vector2& size,
		const Color& color,
		LPDIRECT3DTEXTURE9 texture,
		const Color& outlineColor = Color::Black(0)
	)
	{
		const Matrix44 matrix = GetScreenSpaceMatrix(xf.position.x, xf.position.y, size.x, size.y, xf.angle);
		g_render->RenderScreenSpaceQuad(matrix, color, texture);

		if (outlineColor.a)
			RenderScreenSpace(matrix, outlineColor, Texture_Invalid, primitiveQuadOutline);
	}
	
	void RenderScreenSpaceQuadOutline
	(
		const Matrix44& matrix,
		const Color& color = Color::White()
	)
	{ RenderScreenSpace(matrix, color, Texture_Invalid, primitiveQuadOutline); }

	void RenderScreenSpaceQuad
	(
		const Matrix44& matrix,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid
	)
	{ RenderScreenSpace(matrix, color, ti, primitiveQuad); }
	
	void RenderScreenSpaceQuad
	(
		const Matrix44& matrix,
		const Color& color,
		LPDIRECT3DTEXTURE9 texture
	)
	{ 
		RenderScreenSpace
		(
			matrix,
			color,
			texture,
			primitiveQuad.vb,
			primitiveQuad.primitiveCount,
			primitiveQuad.primitiveType,
			primitiveQuad.stride,
			primitiveQuad.fvf
		);
	}
		
	void RenderCircle
	(
		const Matrix44& matrix,
		const Color& color = Color::White()
	)
	{ Render(matrix, color, Texture_Invalid, primitiveCircle); }

	void RenderCylinder
	(
		const Matrix44& matrix,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid
	)
	{ Render(matrix, color, ti, primitiveCylinder); }

	void RenderCube
	(
		const Matrix44& matrix,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid
	)
	{ Render(matrix, color, ti, primitiveCube); }

	struct RenderPrimitive
	{
		LPDIRECT3DVERTEXBUFFER9 vb;
		UINT primitiveCount;
		UINT vertexCount;
		D3DPRIMITIVETYPE primitiveType;
		UINT stride;
		DWORD fvf;

		RenderPrimitive() :
			vb(NULL),
			primitiveType(D3DPT_LINESTRIP),
			primitiveCount(0),
			vertexCount(0),
			stride(0),
			fvf(0)
		{}

		bool Create
		(
			UINT _primitiveCount,
			UINT _vertexCount,
			D3DPRIMITIVETYPE _primitiveType,
			UINT _stride,
			DWORD _fvf,
			bool dynamic = false
		)
		{
			ASSERT(!vb);

			primitiveCount = _primitiveCount;
			vertexCount = _vertexCount;
			primitiveType = _primitiveType;
			stride = _stride;
			fvf = _fvf;

			const D3DPOOL pool = D3DPOOL_DEFAULT;
			const DWORD usage = (dynamic? (D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC) : D3DUSAGE_WRITEONLY);

			return SUCCEEDED(DXUTGetD3D9Device()->CreateVertexBuffer(vertexCount*stride, usage, fvf, pool, &vb, NULL));
		}

		void SafeRelease() { SAFE_RELEASE(vb); }
	};

	void Render
	(
		const Matrix44& matrix,
		const Color& color,
		TextureID ti,
		const RenderPrimitive& rp,
		const int tileRotation = 0,
		const bool tileMirror = false
	);

	void RenderScreenSpace
	(
		const Matrix44& matrix,
		const Color& color,
		TextureID ti,
		const RenderPrimitive& rp
	);

	void ResetTotalSimpleVertsRendered() { totalSimpleVertsRendered = 0; }
	int GetTotalSimpleVertsRendered() const { return totalSimpleVertsRendered; }

public: // tile rendering

	void RenderTile
	(
		const IntVector2& tilePos,
		const IntVector2& tileSize,
		const XForm2& xf,
		const Vector2& size,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid,
		bool cameraCheck = true,
		int tileRotation = 0,
		const bool tileMirror = false
	)
	{
		if (!cameraCheck || g_cameraBase->CameraTest(xf.position, fabs(size.x) + fabs(size.y)))
			RenderTile(tilePos, tileSize, Matrix44::BuildScale(size) * Matrix44(xf), color, ti, primitiveQuad, tileRotation, tileMirror); 
	}
	
	void RenderScreenSpaceTile
	(
		const IntVector2& tilePos,
		const IntVector2& tileSize,
		const Vector2& position,
		const Vector2& size,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid
	)
	{
		const Matrix44 matrix = GetScreenSpaceMatrix(position.x, position.y, size.x, size.y);
		RenderScreenSpaceTile(tilePos, tileSize, matrix, color, ti);
	}
	
	void RenderScreenSpaceTile
	(
		const IntVector2& tilePos,
		const IntVector2& tileSize,
		const Matrix44& matrix,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid,
		const int tileRotation = 0,
		const bool tileMirror = false
	);
	
	void RenderTile
	(
		const IntVector2& tilePos,
		const IntVector2& tileSize,
		const Matrix44& matrix,
		const Color& color,
		TextureID ti,
		const RenderPrimitive& rp,
		const int tileRotation,
		const bool tileMirror = false
	);
	
	void RenderScreenSpaceTile
	(
		const IntVector2& tilePos,
		const IntVector2& tileSize,
		const Box2AABB& box2,
		const Color& color = Color::White(),
		TextureID ti = Texture_Invalid
	)
	{
		const Vector2 size = Vector2(box2.upperBound - box2.lowerBound)/2.0f;
		RenderScreenSpaceTile
		(
			tilePos,
			tileSize,
			box2.lowerBound + size,
			size,
			color,
			ti
		);
	}
private:

	struct TextureWrapper
	{
		TextureWrapper() {}

		WCHAR name[FILENAME_STRING_LENGTH] = L"\0";
		LPDIRECT3DTEXTURE9 texture = NULL;
		TextureID tileSheetTi = Texture_Invalid;
		ByteVector2 tilePos = ByteVector2(0);
		ByteVector2 tileSize = ByteVector2(0);
		FILETIME fileTime = { 0, 0 };

		bool CheckIfNeedReload() const;
		FILETIME GetDiskFiletime() const;
	};
	
	bool LoadTexture(const WCHAR* textureName, TextureID ti, TextureWrapper& t);
	bool LoadTextureFromFile(const WCHAR* textureName, TextureID ti, TextureWrapper& t);
	void ClearTextureList();
	
	void RenderInternal
	(
		const Matrix44& matrix,
		const Color& color,
		TextureID ti,
		const RenderPrimitive& rp
	);

	void Render
	(
		const Matrix44& matrix,
		const Color& color,
		LPDIRECT3DTEXTURE9 texture,
		LPDIRECT3DVERTEXBUFFER9 vb,
		int primitiveCount,
		D3DPRIMITIVETYPE primitiveType,
		UINT stride,
		DWORD fvf
	);

	void Render
	(
		const Matrix44& matrix,
		LPDIRECT3DVERTEXBUFFER9 vb,
		int primitiveCount,
		D3DPRIMITIVETYPE primitiveType,
		UINT stride,
		DWORD fvf
	);

	void RenderScreenSpace
	(
		const Matrix44& matrix,
		const Color& color,
		LPDIRECT3DTEXTURE9 texture,
		LPDIRECT3DVERTEXBUFFER9 vb,
		int primitiveCount,
		D3DPRIMITIVETYPE primitiveType,
		UINT stride,
		DWORD fvf
	);

	void BuildQuad();
	void BuildCircle();
	void BuildCylinder();
	void BuildCube();

    RenderPrimitive primitiveQuad;
    RenderPrimitive primitiveQuadOutline;
    RenderPrimitive primitiveCircle;
	RenderPrimitive primitiveCylinder;
	RenderPrimitive primitiveCube;
	RenderPrimitive primitiveLines;
	RenderPrimitive primitiveTris;
	
	LPDIRECT3DPIXELSHADER9 normalMapShader;
	LPD3DXCONSTANTTABLE normalMapConstantTable;

	enum TextureType
	{
		TT_Diffuse = 0,
		TT_Normal,
		TT_Specular,
		TT_Emissive,
		TT_Count
	};

	TextureWrapper textures[MAX_TEXTURE_COUNT][TT_Count];
	int highestLoadedTextureID = 0;

	typedef pair<wstring, int> TextureNameHashPair;
	unordered_map<wstring, int> textureHashMap;

	bool wasInitialized = false;
	bool isInRenderBlock = false;

private: // simple vert stuff

	struct SimpleVertex
	{
		Vector3 position;
		DWORD color;
	};
	
	void RenderSimpleLineVerts(const Color& color, bool makeBlack);
	void RenderSimpleTriVerts(const Color& color, bool makeBlack, TextureID ti = Texture_Invalid);

	static const int maxSimpleVerts = 2000;
	int simpleVertLineCount = 0;
	int simpleVertTriCount = 0;
	SimpleVertex simpleVertsLines[maxSimpleVerts];
	SimpleVertex simpleVertsTris[maxSimpleVerts];
	int totalSimpleVertsRendered = 0;
	bool simpleVertsAreAdditive = false;
};

inline void FrankRender::Render
(
	const Matrix44& matrix,
	const Color& color,
	LPDIRECT3DTEXTURE9 texture,
	LPDIRECT3DVERTEXBUFFER9 vb,
	int primitiveCount,
	D3DPRIMITIVETYPE primitiveType,
	UINT stride,
	DWORD fvf
)
{
	ASSERT(isInRenderBlock);

	if (primitiveCount <= 0)
		return;

	if (color.a == 0)
		return;	// skip if there is no alpha

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

	// set the world transform
	pd3dDevice->SetTransform(D3DTS_WORLD, &matrix.GetD3DXMatrix());

	// set up the color
	const D3DMATERIAL9 material =
	{
		{1, 1, 1, color.a}, 
		{color.r, color.g, color.b, 1}, 
		{0, 0, 0, 0},
		{0, 0, 0, 0}, 0
	};
	pd3dDevice->SetMaterial(&material);

	// set up the texture
	pd3dDevice->SetTexture(0, texture);

	// render the primitive
    pd3dDevice->SetStreamSource(0, vb, 0, stride);
    pd3dDevice->SetFVF(fvf);
	pd3dDevice->DrawPrimitive(primitiveType, 0, primitiveCount);
}
