////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Debug Renderer
	Copyright 2013 Frank Force - http://www.frankforce.com

	- simple render for debug purposes
	- calls do not need to be made within a render loop because they are cached
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../rendering/frankRender.h"

extern class DebugRender g_debugRender;

class DebugRender
{
public:

	DebugRender();
	~DebugRender();

	void RenderPoint(const Vector3& point, const Color& color = Color::White(0.5f), float radius = 0.1f, float time = 0.0f);
	void RenderLine(const Vector3& start, const Vector3& end, const Color& color = Color::White(0.5f), float time = 0.0f);
	void RenderLine(const Line2& line, const Color& color = Color::White(0.5f), float time = 0.0f);
	void RenderBox(const Box2AABB& box, const Color& color = Color::White(0.5f), float time = 0.0f);
	void RenderBox(const XForm2& xf, const Box2AABB& box, const Color& color = Color::White(0.5f), float time = 0.0f);
	void RenderBox(const XForm2& xf, const Vector2& size, const Color& color = Color::White(0.5f), float time = 0.0f);
	void RenderCircle(const Circle& circle, const Color& color = Color::White(0.5f), float time = 0.0f, int sides = 16);
	void RenderCone(const Cone& cone, const Color& color = Color::White(0.5f), float time = 0.0f, int sides = 16);
	void RenderText(const Vector2& position, const wstring& text, const Color& color = Color::White(0.5f), bool center = true, float time = 0.0f, bool screenSpace = false);
	void RenderTextFormatted(const Vector2& position, const Color& color, bool center, float time, const wstring messageFormat, ...);
	void RenderGraph(const Vector2& originPos, float(*function)(float), float min = 0, float max = 1, float delta = 0.01f);

	void Update(float delta);
	void Render();
	void Clear();

	void InitDeviceObjects();
	void DestroyDeviceObjects();
	
	void ImmediateRenderTextScreenSpace(const Vector2& point, const WCHAR* text, const Color& color = Color::White(0.5f), bool center = true,CDXUTTextHelper* textHelper = NULL);
	void ImmediateRenderText(const Vector2& point, const WCHAR* text, const Color& color = Color::White(0.5f), bool center = true,CDXUTTextHelper* textHelper = NULL);
	void ImmediateRenderTextFormatted(const Vector2& point, const Color& color, bool center, const wstring messageFormat, ...);

private:

	struct DebugRenderItemPoint
	{
		Matrix44 matrix;
		Color color;
		float time;
	};

	struct DebugRenderItemLine
	{
		Line3 line;
		Color color;
		float time;
	};
	
	struct DebugRenderItemText
	{
		Vector2 position;
		WCHAR text[256];
		Color color;
		float time;
		bool center;
		bool screenSpace;
	};

	static const int itemCountMax;
	static const int textCountMax;

	float debugRenderTimer = 0;

	DebugRenderItemPoint* itemPointArray = NULL;
	DebugRenderItemLine* itemLineArray = NULL;
	DebugRenderItemText* itemTextArray = NULL;

	list<DebugRenderItemPoint*> activeRenderItemPoints;
	list<DebugRenderItemPoint*> inactiveRenderItemPoints;

	list<DebugRenderItemLine*> activeRenderItemLines;
	list<DebugRenderItemLine*> inactiveRenderItemLines;

	list<DebugRenderItemText*> activeRenderItemTexts;
	list<DebugRenderItemText*> inactiveRenderItemTexts;

	void UpdatePrimitiveLineList();
	void CreatePrimitiveLineList();

	FrankRender::RenderPrimitive primitiveLineList;
};
