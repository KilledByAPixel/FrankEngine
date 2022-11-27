////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Font System
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include <fstream>
#include <string>
#include "../rendering/frankFont.h"

static bool fontDebug = false;
ConsoleCommand(fontDebug, fontDebug);

FrankRender::RenderPrimitive FrankFont::primitiveTris;
unordered_map<string, string> FrankFont::tokenReplaceMap;

const char FrankFont::tokenStart = '{';
const char FrankFont::tokenEnd   = '}';

FrankFont::FrankFont(TextureID _texture, WCHAR* dataFilenameWide) :
	texture(_texture)
{
	{
		ifstream inFile(dataFilenameWide, ios::in);
		if (inFile.is_open())
		{
			// read data from file
			ParseFont( inFile, charSet );
			inFile.close();
			return;
		}
	}
	
	{
		// hack: check data/fonts
		wstring filename(L"data/fonts/");
		filename.append(dataFilenameWide);
		ifstream inFile(filename, ios::in);
		if (inFile.is_open())
		{
			// read data from file
			ParseFont( inFile, charSet );
			inFile.close();
			return;
		}
	}

	// Get pointer and size to resource
	HRSRC hRes = FindResource(0, dataFilenameWide, RT_RCDATA);
	HGLOBAL hMem = LoadResource(0, hRes);
	void* dataPointer = LockResource(hMem);
	DWORD size = SizeofResource(0, hRes);

	if (!dataPointer || size == 0)
	{
		g_debugMessageSystem.AddError(L"Count not find font data file '%s'.", dataFilenameWide);
		return;
	}

	// copy the resource into a string stream
	std::stringstream dataStream((char*)dataPointer);

	// free the resource
	UnlockResource(hMem);
	FreeResource(hRes);
	
	ParseFont( dataStream, charSet );
}

Box2AABB FrankFont::GetLocalBBox(const char* _text, float size, FontFlags flags) const
{
	const char* text = PreProcess(_text);

	const float fontScale = size/charSet.lineHeight;
	Vector2 minPos, maxPos;
	GetBounds(text, minPos, maxPos);

	if (flags & FontFlag_CenterY)
	{
		const float offset = (maxPos.y - minPos.y) / 2;
		minPos.y += offset;
		maxPos.y += offset;
	}
	
	if (flags & FontFlag_CenterX)
	{
		const float offset = (maxPos.x - minPos.x) / 2;
		minPos.x -= offset;
		maxPos.x -= offset;
	}

	return Box2AABB(fontScale*minPos, fontScale*maxPos);
}
	
Box2AABB FrankFont::GetAABBox(const char* text, const XForm2& xf, float size, FontFlags flags) const
{
	return Box2AABB(xf, GetLocalBBox(text, size, flags));
}

void FrankFont::RenderScreenSpace(const WCHAR* wstring, const XForm2& xf, float size, const Color& color, FontFlags flags)
{
	float s = size / charSet.lineHeight;
	const Matrix44 matrix = FrankRender::GetScreenSpaceMatrix(xf.position.x, xf.position.y, s, s, xf.angle);
	char string[maxTextLength];
	wcstombs_s(NULL, string, maxTextLength, wstring, maxTextLength);
	return Render(string, matrix, color, flags, true);
}

void FrankFont::RenderScreenSpace(const char* string, const XForm2& xf, float size, const Color& color, FontFlags flags)
{
	float s = size / charSet.lineHeight;
	const Matrix44 matrix = FrankRender::GetScreenSpaceMatrix(xf.position.x, xf.position.y, s, s, xf.angle);
	return Render(string, matrix, color, flags, true);
}

void FrankFont::Render(const WCHAR* wstring, const XForm2& xf, float size, const Color& color, FontFlags flags, bool cameraCheck)
{
	char string[maxTextLength];
	wcstombs_s(NULL, string, maxTextLength, wstring, maxTextLength);

	if (cameraCheck)
	{
		// check if on screen
		const Box2AABB box = GetAABBox(string, xf, size, flags);
		if (!g_cameraBase->CameraTest(box))
			return;
	}
	
	const Matrix44 matrix = Matrix44::BuildScale(size/charSet.lineHeight) * Matrix44(xf);
	return Render(string, matrix, color, flags);
}

void FrankFont::Render(const char* text, const XForm2& xf, float size, const Color& color, FontFlags flags, bool cameraCheck)
{
	if (cameraCheck)
	{
		// check if on screen
		const Box2AABB box = GetAABBox(text, xf, size, flags);
		if (!g_cameraBase->CameraTest(box))
			return;
	}

	if (fontDebug)
		GetLocalBBox(text, size, flags).RenderDebug(xf, Color::Red());

	const Matrix44 matrix = Matrix44::BuildScale(size/charSet.lineHeight) * Matrix44(xf);
	Render(text, matrix, color, flags);
}

void FrankFont::Render(const char* _text, const Matrix44& matrix, const Color& color, FontFlags flags, bool screenSpace)
{
	FrankProfilerEntryDefine(L"FrankFont::Render()", Color::White(), 5);

	if (!(flags & FontFlag_CastShadows))
	{
		if (DeferredRender::GetRenderPassIsShadow())
			return;
	}
	
	if (flags & FontFlag_NoNormals)
	{
		if (DeferredRender::GetRenderPassIsNormalMap())
			return;
	}
	
	const char* text = PreProcess(_text);

	Vector2 offset(0);

	if (flags & FontFlag_CenterY)
	{
		Vector2 minPos, maxPos;
		GetBounds(text, minPos, maxPos);
		offset.y = (maxPos.y - minPos.y) / 2;
	}
	
	// disable lighting for screen space fonts, so custom colors can be used
	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();
	pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
	DeferredRender::PointFilterRenderBlock pointFilterDisable(usePointFilter);

	BuildTriStrip(text, color, flags);

	const Matrix44 finalMatrix = Matrix44(offset) * matrix;
	if (screenSpace)
		g_render->RenderScreenSpace(finalMatrix, Color::White(), texture, primitiveTris);
	else
		g_render->Render(finalMatrix, Color::White(), texture, primitiveTris);

	pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
}

void FrankFont::InitDeviceObjects()
{
	const int vertCount = maxTextLength * 6;
	primitiveTris.Create
	(
		0,											// primitiveCount
		vertCount,									// vertexCount
		D3DPT_TRIANGLESTRIP,						// primitiveType
		sizeof(FontVertex),							// stride
		(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1),	// fvf
		true										// dynamic
	);
}

void FrankFont::DestroyDeviceObjects()
{
	primitiveTris.SafeRelease();
}

void FrankFont::BuildTriStrip(const char* text, const Color& _color, FontFlags flags)
{
	Color color = _color;

	bool allowColorChange = true;
	if (DeferredRender::GetRenderPassIsEmissive() && !DeferredRender::EmissiveRenderBlock::IsActive())
	{
		// always use black when rendering non emissive
		allowColorChange = false;
		color = Color::Black();
	}

	float sizeScale = 1;
	bool modeWave = false;
	bool modeShake = false;
	const float time = GamePauseTimer::GetTimeGlobal();

	FontVertex* verts;
	primitiveTris.vb->Lock(0, 0, (VOID**)&verts, D3DLOCK_DISCARD);

	int vertCount = 0;
	int textLength = strlen(text);
	Vector2 position(0);
	FontVertex* vert = &verts[0];

	if (flags & FontFlag_CenterX)
	{
		float minX = 0, maxX = 0;
		GetLineBounds(text, minX, maxX);
		position.x = -(minX + (maxX - minX)/2);
	}
	else if (flags & FontFlag_AlignRight)
	{
		float minX = 0, maxX = 0;
		GetLineBounds(text, minX, maxX);
		position.x = -(minX + (maxX - minX));
	}

	for(int i = 0; i < textLength; ++i)
	{
		const unsigned char& c = text[i];

		if (!c)
			break;

		if (c == '\n')
		{
			if (flags & FontFlag_CenterX)
			{
				float minX = 0, maxX = 0;
				GetLineBounds(&text[i+1], minX, maxX);
				position.x = -(minX + (maxX - minX)/2);
			}
			else if (flags & FontFlag_AlignRight)
			{
				float minX = 0, maxX = 0;
				GetLineBounds(&text[i+1], minX, maxX);
				position.x = -(minX + (maxX - minX));
			}
			else
				position.x = 0;
			position.y -= charSet.lineHeight;
			continue;
		}
		else if (c == tokenStart)
		{
			// parse token
			char token[256] = "";
			int iToken = 0;
			for(++i; i < textLength; ++i)
			{
				const unsigned char& c2 = text[i];
				if (!c2 || c2 == tokenEnd)
					break;

				token[iToken] = c2;
				++iToken;
				if (iToken >= 255)
					break;
			}

			token[iToken] = 0;
			if (!strcmp(token, "wave"))
				modeWave = !modeWave;
			else if (!strcmp(token, "shake"))
				modeShake = !modeShake;
			else if (!strcmp(token, "big"))
				sizeScale += 0.25f;
			else if (!strcmp(token, "small"))
				sizeScale = Max(sizeScale - 0.25f, 0.25f);
			else if (allowColorChange)
			{
				if (!strcmp(token, "white"))
					color = Color::White();
				else if (!strcmp(token, "black"))
					color = Color::Black();
				else if (!strcmp(token, "red"))
					color = Color::Red();
				else if (!strcmp(token, "orange"))
					color = Color::Orange();
				else if (!strcmp(token, "yellow"))
					color = Color::Yellow();
				else if (!strcmp(token, "green"))
					color = Color::Green();
				else if (!strcmp(token, "blue"))
					color = Color::Blue();
				else if (!strcmp(token, "purple"))
					color = Color::Purple();
				else if (!strcmp(token, "magenta"))
					color = Color::Magenta();
				else if (!strcmp(token, "cyan"))
					color = Color::Cyan();
				color.a = _color.a;
			}

			continue;
		}

		const CharDescriptor& charDescriptor = charSet.Chars[c];
		
		if (c == ' ')
		{
			position.x += charDescriptor.advanceX;
			continue;
		}

		const Vector2 charUL = charDescriptor.position;
		const Vector2 charUR = charDescriptor.position + Vector2(charDescriptor.size.x, 0);
		const Vector2 charLL = charDescriptor.position + Vector2(0, charDescriptor.size.y);
		const Vector2 charLR = charDescriptor.position + charDescriptor.size;
		
		Vector2 offsetPosition(0);
		if (modeWave)
		{
			offsetPosition.y += 10*sinf(10*time + i);
		}
		if (modeShake)
		{
			Vector2 noise(0);
			noise.y = PerlineNoise::noise(10*time + 3*i);
			noise.x = PerlineNoise::noise(10*time + 5*i + 11);
			offsetPosition += 20*noise;
		}

		// keep it centered if size changes
		offsetPosition.y += 0.5f*charDescriptor.size.y * (sizeScale - 1);

		//offsetPosition *= sizeScale;
		offsetPosition += position;

		// lower left
		Vector3 vertPosition(0);
		vertPosition.x = offsetPosition.x + sizeScale*(charDescriptor.offset.x);
		vertPosition.y = offsetPosition.y + sizeScale*(-charDescriptor.size.y - charDescriptor.offset.y);
		vert->position = vertPosition;
		vert->textureCoords = charLL / charSet.scale;
		vert->color = color;
		++vert;
		++vertCount;
		
		// degenerate tri
		vert->position = vertPosition;
		vert->textureCoords = charLL / charSet.scale;
		vert->color = color;
		++vert;
		++vertCount;

		// lower right
		vertPosition.x = offsetPosition.x + sizeScale*(charDescriptor.size.x + charDescriptor.offset.x);
		vertPosition.y = offsetPosition.y + sizeScale*(-charDescriptor.size.y - charDescriptor.offset.y);
		vert->position = vertPosition;
		vert->textureCoords = charLR / charSet.scale;
		vert->color = color;
		++vert;
		++vertCount;
		
		// upper left
		vertPosition.x = offsetPosition.x + sizeScale*(charDescriptor.offset.x);
		vertPosition.y = offsetPosition.y + sizeScale*(-charDescriptor.offset.y);
		vert->position = vertPosition;
		vert->textureCoords = charUL / charSet.scale;
		vert->color = color;
		++vert;
		++vertCount;

		// upper right
		vertPosition.x = offsetPosition.x + sizeScale*(charDescriptor.size.x + charDescriptor.offset.x);
		vertPosition.y = offsetPosition.y + sizeScale*(-charDescriptor.offset.y);
		vert->position = vertPosition;
		vert->textureCoords = charUR / charSet.scale;
		vert->color = color;
		++vert;
		++vertCount;

		// degenerate tri
		vert->position = vertPosition;
		vert->textureCoords = charUR / charSet.scale;
		vert->color = color;
		++vert;
		++vertCount;

		position.x += sizeScale*(charDescriptor.advanceX);
	}

	primitiveTris.vb->Unlock();

	primitiveTris.primitiveCount = vertCount - 2;
}

void FrankFont::GetBounds(const char* text, Vector2& minPos, Vector2& maxPos) const
{
	minPos = Vector2(FLT_MAX);
	maxPos = Vector2(0);
	
	float sizeScale = 1;
	const int textLength = strlen(text);
	Vector2 position(0);
	for(int i = 0; i < textLength; ++i)
	{
		const unsigned char& c = text[i];

		if (c == '\n')
		{
			position.x = 0;
			position.y -= charSet.lineHeight;
			continue;
		}
		else if (c == tokenStart)
		{
			// parse token
			char token[256] = "";
			int iToken = 0;
			for(++i; i < textLength; ++i)
			{
				const unsigned char& c2 = text[i];
				if (!c2 || c2 == tokenEnd)
					break;

				token[iToken] = c2;
				++iToken;
				if (iToken >= 255)
					break;
			}

			token[iToken] = 0;
			if (!strcmp(token, "big"))
				sizeScale += 0.25f;
			else if (!strcmp(token, "small"))
				sizeScale = Max(sizeScale - 0.25f, 0.25f);

			continue;
		}

		const CharDescriptor& charDescriptor = charSet.Chars[c];
		
		if (c == ' ')
		{
			position.x += charDescriptor.advanceX;
			continue;
		}
		
		//lower left
		Vector3 vertPosition(0);
		vertPosition.x = position.x + charDescriptor.offset.x;
		vertPosition.y = position.y - charDescriptor.size.y - charDescriptor.offset.y;
		minPos.x = Min(vertPosition.x, minPos.x);
		minPos.y = Min(vertPosition.y, minPos.y);
		maxPos.x = Max(vertPosition.x, maxPos.x);
		maxPos.y = Max(vertPosition.y, maxPos.y);

		//lower right
		vertPosition.x = position.x + charDescriptor.size.x + charDescriptor.offset.x;
		vertPosition.y = position.y - charDescriptor.size.y - charDescriptor.offset.y;
		minPos.x = Min(vertPosition.x, minPos.x);
		minPos.y = Min(vertPosition.y, minPos.y);
		maxPos.x = Max(vertPosition.x, maxPos.x);
		maxPos.y = Max(vertPosition.y, maxPos.y);

		//upper left
		vertPosition.x = position.x + charDescriptor.offset.x;
		vertPosition.y = position.y - charDescriptor.offset.y;
		minPos.x = Min(vertPosition.x, minPos.x);
		minPos.y = Min(vertPosition.y, minPos.y);
		maxPos.x = Max(vertPosition.x, maxPos.x);
		maxPos.y = Max(vertPosition.y, maxPos.y);

		//upper right
		vertPosition.x = position.x + charDescriptor.size.x + charDescriptor.offset.x;
		vertPosition.y = position.y - charDescriptor.offset.y;
		minPos.x = Min(vertPosition.x, minPos.x);
		minPos.y = Min(vertPosition.y, minPos.y);
		maxPos.x = Max(vertPosition.x, maxPos.x);
		maxPos.y = Max(vertPosition.y, maxPos.y);

		position.x += sizeScale*charDescriptor.advanceX;
	}

	minPos.x = Min(minPos.x, maxPos.x);
	minPos.y = Min(minPos.y, maxPos.y);
	maxPos.x += extraRightBounds;
}

void FrankFont::GetLineBounds(const char* text, float& minX, float& maxX) const
{
	minX = FLT_MAX;
	maxX = 0;
	
	float sizeScale = 1;
	int textLength = strlen(text);
	Vector2 position(0);
	for(int i = 0; i < textLength; ++i)
	{
		const unsigned char& c = text[i];

		if (c == '\n' || !c)
			break;
		else if (c == tokenStart)
		{
			// parse token
			char token[256] = "";
			int iToken = 0;
			for(++i; i < textLength; ++i)
			{
				const unsigned char& c2 = text[i];
				if (!c2 || c2 == tokenEnd)
					break;

				token[iToken] = c2;
				++iToken;
				if (iToken >= 255)
					break;
			}

			token[iToken] = 0;
			if (!strcmp(token, "big"))
				sizeScale += 0.25f;
			else if (!strcmp(token, "small"))
				sizeScale = Max(sizeScale - 0.25f, 0.25f);

			continue;
		}

		const CharDescriptor& charDescriptor = charSet.Chars[c];
		
		if (c == ' ')
		{
			position.x += charDescriptor.advanceX;
			continue;
		}
		
		//lower left
		Vector3 vertPosition(0);
		vertPosition.x = position.x + charDescriptor.offset.x;
		minX = Min(vertPosition.x, minX);
		maxX = Max(vertPosition.x, maxX);

		//lower right
		vertPosition.x = position.x + charDescriptor.size.x + charDescriptor.offset.x;
		minX = Min(vertPosition.x, minX);
		maxX = Max(vertPosition.x, maxX);

		//upper left
		vertPosition.x = position.x + charDescriptor.offset.x;
		minX = Min(vertPosition.x, minX);
		maxX = Max(vertPosition.x, maxX);

		//upper right
		vertPosition.x = position.x + charDescriptor.size.x + charDescriptor.offset.x;
		minX = Min(vertPosition.x, minX);
		maxX = Max(vertPosition.x, maxX);

		position.x += sizeScale*charDescriptor.advanceX;
	}

	minX = Min(minX, maxX);
	minX += extraRightBounds;
}

bool FrankFont::ParseFont(istream& Stream, CharSet& CharSetDesc)
{
	string Line;
	string Read, Key, Value;
	std::size_t i;
	while( !Stream.eof() )
	{
		std::stringstream LineStream;
		std::getline( Stream, Line );
		LineStream << Line;

		//read the line's type
		LineStream >> Read;
		if( Read == "common" )
		{
			//this holds common data
			while( !LineStream.eof() )
			{
				std::stringstream Converter;
				LineStream >> Read;
				i = Read.find( '=' );
				Key = Read.substr( 0, i );
				Value = Read.substr( i + 1 );

				//assign the correct value
				Converter << Value;
				if( Key == "lineHeight" )
					Converter >> CharSetDesc.lineHeight;
				else if( Key == "base" )
					Converter >> CharSetDesc.base;
				else if( Key == "scaleW" )
					Converter >> CharSetDesc.scale.x;
				else if( Key == "scaleH" )
					Converter >> CharSetDesc.scale.y;
				else if( Key == "pages" )
					Converter >> CharSetDesc.pages;
			}
		}
		else if( Read == "char" )
		{
			//this is data for a specific char
			unsigned short CharID = 0;

			while( !LineStream.eof() )
			{
				std::stringstream Converter;
				LineStream >> Read;
				i = Read.find( '=' );
				Key = Read.substr( 0, i );
				Value = Read.substr( i + 1 );

				//assign the correct value
				Converter << Value;
				if( Key == "id" )
				{
					Converter >> CharID;
					ASSERT(CharID < 256); // only basic ascii set of 256 chars is supported
				}
				else if( Key == "x" )
					Converter >> CharSetDesc.Chars[CharID].position.x;
				else if( Key == "y" )
					Converter >> CharSetDesc.Chars[CharID].position.y;
				else if( Key == "width" )
					Converter >> CharSetDesc.Chars[CharID].size.x;
				else if( Key == "height" )
					Converter >> CharSetDesc.Chars[CharID].size.y;
				else if( Key == "xoffset" )
					Converter >> CharSetDesc.Chars[CharID].offset.x;
				else if( Key == "yoffset" )
					Converter >> CharSetDesc.Chars[CharID].offset.y;
				else if( Key == "xadvance" )
					Converter >> CharSetDesc.Chars[CharID].advanceX;
				else if( Key == "page" )
					Converter >> CharSetDesc.Chars[CharID].page;
			}
		}
	}

	return true;
}

const char* FrankFont::PreProcess(const char* text)
{
	static string textOut;
	textOut = string("");

	float sizeScale = 1;
	int textLength = strlen(text);
	for(int i = 0; i < textLength; ++i)
	{
		const unsigned char& c = text[i];
		
		if (c == '_')
			continue;
		else if (c == tokenStart)
		{
			// parse token
			char token[256] = "";
			int iToken = 0;
			for(++i; i < textLength; ++i)
			{
				const unsigned char& c2 = text[i];
				if (!c2 || c2 == tokenEnd)
					break;

				token[iToken] = c2;
				++iToken;
				if (iToken >= 255)
					break;
			}

			token[iToken] = 0;

			// replace token if in dictionary
			unordered_map<string, string>::iterator it = tokenReplaceMap.find(token);
			if (it != tokenReplaceMap.end())
			{
				textOut.append(((*it).second));
				continue;
			}
			
			textOut.append(1, tokenStart);
			textOut.append(token);
			textOut.append(1, tokenEnd);
			continue;
		}

		textOut.append(1, c);
	}
	
	textOut.append(1, 0);
	return textOut.c_str();
}
