////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Font System
	Copyright 2013 Frank Force - http://www.frankforce.com
	
	- uses BMFont to generate font textures - http://www.angelcode.com/products/bmfont/
	- some code is from Promit's tutorial - http://www.gamedev.net/topic/330742-quick-tutorial-variable-width-bitmap-fonts/
	- fonts are loaded from texture
	- fonts can be rendered in world or screen space and scaled
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

enum FontFlags
{
	FontFlag_None		= 0x00,
	FontFlag_CenterX	= 0x01,
	FontFlag_CenterY	= 0x02,
	FontFlag_Center		= 0x03,
	FontFlag_CastShadows= 0x04,			// allows font to cast shadows as if it were a foreground object
	FontFlag_NoNormals	= 0x08,			// skip normals pass, allows normal map below it to show through
	FontFlag_AlignRight	= 0x10,
};

inline FontFlags operator | (FontFlags a, FontFlags b) { return static_cast<FontFlags>(static_cast<int>(a) | static_cast<int>(b)); }
inline FontFlags operator & (FontFlags a, FontFlags b) { return static_cast<FontFlags>(static_cast<int>(a) & static_cast<int>(b)); }

struct FrankFont
{
	FrankFont(TextureID texture, WCHAR* dataFilename);

	static void InitDeviceObjects();
	static void DestroyDeviceObjects();
	
	void Render(const WCHAR* text, const XForm2& xf, float size, const Color& color = Color::White(), FontFlags flags = FontFlag_None, bool cameraCheck = true);
	void Render(const char* text, const XForm2& xf, float size, const Color& color = Color::White(), FontFlags flags = FontFlag_None, bool cameraCheck = true);
	void Render(const char* text, const Matrix44& matrix, const Color& color = Color::White(), FontFlags flags = FontFlag_None, bool screenSpace = false);
	
	void RenderScreenSpace(const WCHAR* text, const XForm2& xf, float size, const Color& color = Color::White(), FontFlags flags = FontFlag_None);
	void RenderScreenSpace(const char* text, const XForm2& xf, float size, const Color& color = Color::White(), FontFlags flags = FontFlag_None);
	
	Box2AABB GetLocalBBox(const char* text, float size, FontFlags flags = FontFlag_None) const;
	Box2AABB GetAABBox(const char* text, const XForm2& xf, float size, FontFlags flags = FontFlag_None) const;

	void SetUsePointFilter(bool _usePointFilter) { usePointFilter = _usePointFilter; }
	void SetExtraRightBounds(float _extraRightBounds) { extraRightBounds = _extraRightBounds; }

	static void SetReplaceToken(const char* token, const char* text) { tokenReplaceMap[token] = text; }
	
	static const char* PreProcess(const char* text);

	static const char tokenStart;
	static const char tokenEnd;

private:
	
	struct CharDescriptor
	{
		Vector2 position;
		Vector2 size;
		Vector2 offset;
		float advanceX;
		float page;

		CharDescriptor() : position(0), size(0), offset(0), advanceX(0), page(0) {}
	};

	struct CharSet
	{
		float lineHeight;
		float base;
		float pages;
		Vector2 scale;
		CharDescriptor Chars[256];
	};
	
	struct FontVertex
	{
		Vector3 position;
		DWORD color;
		Vector2 textureCoords;
	};

	void GetLineBounds(const char* text, float& minX, float& maxX) const;	
	void GetBounds(const char* text, Vector2& minPos, Vector2& maxPos) const;
	void BuildTriStrip(const char* text, const Color& color = Color::White(), FontFlags flags = FontFlag_None);

	static bool ParseFont(istream& Stream, CharSet& CharSetDesc);

	CharSet charSet;
	TextureID texture;

	static FrankRender::RenderPrimitive primitiveTris;
	static const int maxTextLength = 1024;
	bool usePointFilter = false;
	float extraRightBounds = 0;

	static unordered_map<string, string> tokenReplaceMap;
};
