////////////////////////////////////////////////////////////////////////////////////////
/*
	Physics Render
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

// this is the global physics render
extern class PhysicsRender g_physicsRender;

// This class implements debug drawing callbacks that are invoked
// inside b2World::Step.
class PhysicsRender : public b2Draw
{
public:

	PhysicsRender();
	void Update();
	void Render();

	bool GetDebugRender() const { return debugRender; }
	void SetDebugRender(bool debugRender);

	///////////////////////////////////////
	// rendering
	///////////////////////////////////////

	void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
	void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
	void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override;
	void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color) override;
	void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
	void DrawTransform(const b2Transform& xf) override;
	void DrawPoint(const b2Vec2& p, float32 size, const b2Color& color) override;

	void DrawBody(b2Body& physicsBody, const Color& color = Color::Black(), const Color& outlineColor = Color::White());
	void DrawFixture(const XForm2& xf, const b2Fixture& fixture, const Color& color = Color::Black(), const Color& outlineColor = Color::White());
	
	void DrawString(int x, int y, const char *string, ...);
	void DrawString(const b2Vec2& pw, const char *string, ...);
	void DrawAABB(b2AABB* aabb, const b2Color& color);

	static float alpha;
	static float pointSizeScale;
	bool debugRender;
};
