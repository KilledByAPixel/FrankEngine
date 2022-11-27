////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Debug Renderer
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../rendering/frankDebugRender.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Debug Render Globals
*/
////////////////////////////////////////////////////////////////////////////////////////

const int DebugRender::itemCountMax = 5000;
const int DebugRender::textCountMax = 200;

ConsoleCommandSimple(bool, debugRenderEnable, true);

DebugRender g_debugRender;

#define D3DFVF_LineListVertex (D3DFVF_XYZ|D3DFVF_DIFFUSE)
struct LineListVertex
{
    D3DXVECTOR3 position;		// vertex position
	DWORD color;				// color
};

// console command to clear all primitves in the buffer
static void ConsoleCommandCallback_clearDebugPrimitives(const wstring& text) { g_debugRender.Clear(); }
ConsoleCommand(ConsoleCommandCallback_clearDebugPrimitives, clearDebugPrimitives);

////////////////////////////////////////////////////////////////////////////////////////
/*
	Debug Render Member Functions
*/
////////////////////////////////////////////////////////////////////////////////////////

DebugRender::DebugRender()
{
	itemPointArray = new DebugRenderItemPoint[itemCountMax];
	itemLineArray = new DebugRenderItemLine[itemCountMax];
	itemTextArray = new DebugRenderItemText[textCountMax];

	Clear();
}

DebugRender::~DebugRender()
{
	SAFE_DELETE_ARRAY(itemPointArray);
	SAFE_DELETE_ARRAY(itemLineArray);
	SAFE_DELETE_ARRAY(itemTextArray);
}

void DebugRender::Clear()
{
	activeRenderItemPoints.clear();
	activeRenderItemLines.clear();
	activeRenderItemTexts.clear();
	inactiveRenderItemPoints.clear();
	inactiveRenderItemLines.clear();
	inactiveRenderItemTexts.clear();

	for (int i = 0; i < itemCountMax; ++i)
	{
		inactiveRenderItemPoints.push_back(&itemPointArray[i]);
		inactiveRenderItemLines.push_back(&itemLineArray[i]);
	}
	for (int i = 0; i < textCountMax; ++i)
		inactiveRenderItemTexts.push_back(&itemTextArray[i]);
}

void DebugRender::Update(float delta)
{
	debugRenderTimer += delta;

	for (list<DebugRenderItemPoint*>::iterator it = activeRenderItemPoints.begin(); it != activeRenderItemPoints.end();)
	{
		DebugRenderItemPoint *renderItem = (*it);

		if (renderItem->time < debugRenderTimer)
		{
			it = activeRenderItemPoints.erase(it);
			inactiveRenderItemPoints.push_back(renderItem);
		}
		else
			++it;
	}

	for (list<DebugRenderItemLine*>::iterator it = activeRenderItemLines.begin(); it != activeRenderItemLines.end();)
	{
		DebugRenderItemLine *renderItem = (*it);

		if (renderItem->time < debugRenderTimer)
		{
			it = activeRenderItemLines.erase(it);
			inactiveRenderItemLines.push_back(renderItem);
		}
		else
			++it;
	}

	for (list<DebugRenderItemText*>::iterator it = activeRenderItemTexts.begin(); it != activeRenderItemTexts.end();)
	{
		DebugRenderItemText *renderItem = (*it);

		if (renderItem->time < debugRenderTimer)
		{
			it = activeRenderItemTexts.erase(it);
			inactiveRenderItemTexts.push_back(renderItem);
		}
		else
			++it;
	}
}

void DebugRender::Render()
{
	if (!debugRenderEnable)
		return;
	
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"DebugRender::Render()" );

	IDirect3DDevice9* pd3dDevice = DXUTGetD3D9Device();

	pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); 
	pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
	pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	pd3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE);

	for (DebugRenderItemPoint* renderItem : activeRenderItemPoints)
		g_render->RenderCircle(renderItem->matrix, renderItem->color);

	UpdatePrimitiveLineList();

	pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

	if (primitiveLineList.primitiveCount > 0)
		g_render->Render(Matrix44::Identity(), Color::White(), Texture_Invalid, primitiveLineList);
	
	for (DebugRenderItemText* renderItem : activeRenderItemTexts)
	{
		if (renderItem->screenSpace)
			ImmediateRenderTextScreenSpace(renderItem->position, renderItem->text, renderItem->color, renderItem->center);
		else
			ImmediateRenderText(renderItem->position, renderItem->text, renderItem->color, renderItem->center);
	}

	pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE);
}

void DebugRender::InitDeviceObjects()
{
	CreatePrimitiveLineList();
}

void DebugRender::DestroyDeviceObjects()
{
	primitiveLineList.SafeRelease();
}

void DebugRender::UpdatePrimitiveLineList()
{
	if (activeRenderItemLines.empty())
	{
		primitiveLineList.primitiveCount = 0;
		return;
	}

	// lock the vb while we update it, use d3dlock_discard since we will overwrite everything
	LineListVertex* vertices;
	if (FAILED(primitiveLineList.vb->Lock(0, 0, (VOID**)&vertices, D3DLOCK_DISCARD)))
		return;

	int i = -1;
	for (DebugRenderItemLine* renderItem : activeRenderItemLines)
	{
		const Color& c = renderItem->color;

		LineListVertex& vertexStart = vertices[++i];
		vertexStart.position = renderItem->line.p1;
		vertexStart.color = c;

		LineListVertex& vertexEnd = vertices[++i];
		vertexEnd.position = renderItem->line.p2;
		vertexEnd.color = c;
			
		if (i + 1 >= (int)primitiveLineList.vertexCount)
			break;
	}

	primitiveLineList.vb->Unlock();
	primitiveLineList.primitiveCount = (i+1) / 2;
}

void DebugRender::CreatePrimitiveLineList()
{
	static const int lineCountMax = itemCountMax;
	primitiveLineList.Create
	(
		lineCountMax,				// primitiveCount
		lineCountMax * 2,			// vertexCount
		D3DPT_LINELIST,				// primitiveType
		sizeof(LineListVertex),		// stride
		D3DFVF_LineListVertex,		// fvf
		true						// dynamic
	);

	// start with no lines
	primitiveLineList.primitiveCount = 0;
}

void DebugRender::RenderPoint(const Vector3 &position, const Color &color, float scale, float time)
{
	ASSERT(time >= 0 || scale > 0);

	if (inactiveRenderItemPoints.empty() || color.a <= 0)
		return;

	DebugRenderItemPoint *newItem = inactiveRenderItemPoints.back();
	inactiveRenderItemPoints.pop_back();
	activeRenderItemPoints.push_back(newItem);

	const Matrix44 matrixTranslate = Matrix44::BuildTranslate(position);
	const Matrix44 matrixScale = Matrix44::BuildScale(scale);
	const Matrix44 matrixRender = matrixScale * matrixTranslate;

	newItem->matrix = matrixRender;
	newItem->color = color;
	newItem->time = debugRenderTimer + time;
}

void DebugRender::RenderTextFormatted(const Vector2& position, const Color& color, bool center, float time, const wstring messageFormat, ...)
{
	WCHAR text[256];
	
    va_list args;
    va_start(args, messageFormat);
	StringCchVPrintf( text, 256, messageFormat.c_str(), args );
    va_end(args);
	text[255] = L'\0';

	RenderText(position, text, color, center, time);
}

void DebugRender::RenderText(const Vector2& position, const wstring& text, const Color& color, bool center, float time, bool screenSpace)
{
	if (time < 0 || inactiveRenderItemTexts.empty() || color.a <= 0)
		return;
	
	DebugRenderItemText *newItem = inactiveRenderItemTexts.back();
	inactiveRenderItemTexts.pop_back();
	activeRenderItemTexts.push_back(newItem);

	newItem->position = position;
	newItem->color = color;
	newItem->center = center;
	newItem->time = debugRenderTimer + time;
	newItem->screenSpace = screenSpace;
	wcsncpy_s(newItem->text, 256, text.c_str(), 256);
}

void DebugRender::RenderLine(const Vector3& start, const Vector3& end, const Color& color, float time)
{
	if (time < 0 || inactiveRenderItemLines.empty() || color.a <= 0)
		return;

	DebugRenderItemLine *newItem = inactiveRenderItemLines.back();
	inactiveRenderItemLines.pop_back();
	activeRenderItemLines.push_back(newItem);

	newItem->line.p1 = start;
	newItem->line.p2 = end;
	newItem->color = color;
	newItem->time = debugRenderTimer + time;
}

void DebugRender::RenderLine(const Line2& line, const Color& color, float time)
{
	RenderLine(line.p1, line.p2, color), time;
}

void DebugRender::RenderBox(const Box2AABB& box, const Color& color, float time)
{
	RenderBox(XForm2::Identity(), box, color, time);
}

void DebugRender::RenderBox(const XForm2& xf, const Box2AABB& box, const Color& color, float time)
{	
	const Vector2 lu = xf.TransformCoord(Vector2(box.lowerBound.x, box.upperBound.y));
	const Vector2 uu = xf.TransformCoord(Vector2(box.upperBound.x, box.upperBound.y));
	const Vector2 ll = xf.TransformCoord(Vector2(box.lowerBound.x, box.lowerBound.y));
	const Vector2 ul = xf.TransformCoord(Vector2(box.upperBound.x, box.lowerBound.y));
	RenderLine(ll, ul, color, time);
	RenderLine(lu, uu, color, time);
	RenderLine(ll, lu, color, time);
	RenderLine(ul, uu, color, time);
}

void DebugRender::RenderBox(const XForm2& xf, const Vector2& size, const Color& color, float time)
{
	RenderBox(xf, Box2AABB(-size, size), color, time);
}

void DebugRender::RenderCircle(const Circle& circle, const Color& color, float time, int sides)
{	
	const float coef = 2*PI/sides;
	Vector2 lastPos = circle.radius * Vector2::BuildFromAngle(0);
	for (int i = 1; i < sides; ++i)
	{
		const Vector2 pos = circle.radius * Vector2::BuildFromAngle((i+1)*coef);
		RenderLine(lastPos + circle.position, pos + circle.position, color, time);
		lastPos = pos;
	}
}

void DebugRender::RenderCone(const Cone& cone, const Color& color, float time, int sides)
{	
	Vector2 lastPos = cone.xf.position;

	for(int i = 0; i <= sides; ++i)
	{
		float p = float(i) / float(sides);
		float angle = Lerp(p, -cone.angle , cone.angle);
		const Vector2 pos = cone.xf.position + cone.radius*Vector2::BuildFromAngle(cone.xf.angle + angle);	
		RenderLine(lastPos, pos, color, time);
		lastPos = pos;
	}
	
	RenderLine(lastPos, cone.xf.position, color, time);
}

// draws a graph between min and max on the x axis and function on the y axis
void DebugRender::RenderGraph(const Vector2& originPos, float(*function)(float), float min, float max, float delta)
{
	Vector2 posLast = originPos;
	for (float p = min; p <= max; p += delta)
	{
		Vector2 pos = originPos;
		pos += Vector2(p, function(p)) * 10;
		pos.RenderDebug();
		Line2(pos, posLast).RenderDebug();
		posLast = pos;
	}
	Line2(originPos, originPos + Vector2(10, 0)).RenderDebug(Color::Red());
	Line2(originPos, originPos + Vector2(0, 10)).RenderDebug(Color::Red());
	Line2(originPos + Vector2(10, 0), originPos + Vector2(10, 10)).RenderDebug(Color::Red());
	Line2(originPos + Vector2(0, 10), originPos + Vector2(10, 10)).RenderDebug(Color::Red());
}

void DebugRender::ImmediateRenderTextFormatted
(
	const Vector2& position, 
	const Color& color, 
	bool center,
	const wstring messageFormat, ...
)
{
	if (DeferredRender::GetRenderPass())
		return;
	
	// fill in the message
    va_list args;
    va_start(args, messageFormat);
	WCHAR text[1024];
	StringCchVPrintf( text, 1024, messageFormat.c_str(), args );
    va_end(args);
	text[1023] = L'\0';

	const Vector2 screenSpacePos = g_render->WorldSpaceToScreenSpace(position);
	ImmediateRenderTextScreenSpace(screenSpacePos, text, color, center);
}

void DebugRender::ImmediateRenderText
(
	const Vector2& position, 
	const WCHAR* text, 
	const Color& color, 
	bool center,
	CDXUTTextHelper* textHelper
)
{
	const Vector2 screenSpacePos = g_render->WorldSpaceToScreenSpace(position);
	ImmediateRenderTextScreenSpace(screenSpacePos, text, color, center, textHelper);
}

void DebugRender::ImmediateRenderTextScreenSpace
(
	const Vector2& position, 
	const WCHAR* text, 
	const Color& color, 
	bool center,
	CDXUTTextHelper* textHelper
)
{
	if (DeferredRender::GetRenderPass() || color.a == 0)
		return;
	
	if (!textHelper)
		textHelper = g_textHelper;
	
	textHelper->Begin();
	textHelper->DrawTextLine((int)position.x + 1, (int)position.y + 0, text, Color::Black(color.a), center, center);
	textHelper->DrawTextLine((int)position.x - 1, (int)position.y + 0, text, Color::Black(color.a), center, center);
	textHelper->DrawTextLine((int)position.x + 0, (int)position.y + 1, text, Color::Black(color.a), center, center);
	textHelper->DrawTextLine((int)position.x + 0, (int)position.y - 1, text, Color::Black(color.a), center, center);
	textHelper->DrawTextLine((int)position.x, (int)position.y, text, color, center, center);
	textHelper->End();
}