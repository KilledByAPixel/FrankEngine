////////////////////////////////////////////////////////////////////////////////////////
/*
	Physics Render
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../physics/physics.h"
#include "../physics/physicsRender.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Globals
*/
////////////////////////////////////////////////////////////////////////////////////////

// the one and only physics renderer
PhysicsRender g_physicsRender;

#define D3DFVF_PhysicsRenderVertex (D3DFVF_XYZ|D3DFVF_DIFFUSE)

float PhysicsRender::alpha = 0.5f;
ConsoleCommand(PhysicsRender::alpha, physicsDebugAlpha);

float PhysicsRender::pointSizeScale = 0.05f;

////////////////////////////////////////////////////////////////////////////////////////
/*
	Physics Rendering
*/
////////////////////////////////////////////////////////////////////////////////////////

PhysicsRender::PhysicsRender()
{
	// used for physics debug rendering
	SetDebugRender(false);
}

void PhysicsRender::SetDebugRender(bool _debugRender)
{
	debugRender = _debugRender;
	// set the flags for what should be rendered
	uint32 flags = 0;
	if (debugRender)
	{
		flags += b2Draw::e_shapeBit;
		flags += b2Draw::e_jointBit;
		flags += b2Draw::e_aabbBit;
		flags += b2Draw::e_pairBit;
		flags += b2Draw::e_centerOfMassBit;
	} 
	else
		flags = 0;
	SetFlags(flags);
}

void PhysicsRender::Update()
{
	if (!debugRender)
		return;
	
	// print info for object under mouse
	Vector2 mousePos = g_input->GetMousePosWorldSpace();
 	SimpleRaycastResult result;
	GameObject* hitObject = g_physics->PointcastSimple(mousePos, &result);
	b2Fixture* fixture = result.hitFixture;
	if (hitObject && fixture)
	{
		const WCHAR* name = L"";
		const ObjectTypeInfo* objectInfo = hitObject->GetObjectInfo();
		if (objectInfo)
			name = objectInfo->GetName();

		XForm2 xf = hitObject->GetPosWorld();
		Vector2 velocity = hitObject->GetVelocity();
		float angularVelocity = hitObject->GetAngularVelocity();
		float friction = fixture->GetFriction();
		float density = fixture->GetDensity();
		float restitution = fixture->GetRestitution();
		int renderGroup = hitObject->GetRenderGroup();
		int physicsGroup = hitObject->GetPhysicsGroup();

		g_debugRender.RenderTextFormatted(mousePos + g_cameraBase->GetXFWorld().TransformVector(Vector2(0.5f, -0.5f)), Color::White(), false, 0, 
			L"%s #%d\n"
			"xf: (%.2f, %.2f) %.2f\n"
			"vel: (%.2f, %.2f) %.2f\n"
			"fric: %.2f dens: %.2f rest: %.2f\n"
			"rendGroup: %d physGroup: %d",
			name, hitObject->GetHandle(),
			xf.position.x, xf.position.y, xf.angle,
			velocity.x, velocity.y, angularVelocity,
			friction, density, restitution,
			renderGroup, physicsGroup
		);
	}
}

void PhysicsRender::Render()
{
	if (debugRender)
		g_physics->GetPhysicsWorld()->DrawDebugData();
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	Primitive Rendering
*/
////////////////////////////////////////////////////////////////////////////////////////

void PhysicsRender::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
	g_render->DrawPolygon((Vector2*)vertices, vertexCount, Color(color).ScaleAlpha(alpha));
}

void PhysicsRender::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
	g_render->DrawSolidPolygon((Vector2*)vertices, vertexCount, Color(color).ScaleAlpha(alpha));
	g_render->DrawPolygon((Vector2*)vertices, vertexCount, Color(color).ScaleAlpha(alpha));
}

void PhysicsRender::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
{
	g_render->DrawCircle(XForm2(center), Vector2(radius), Color(color).ScaleAlpha(alpha));
}

void PhysicsRender::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
{
	const float startAngle = atan2f(axis.y, axis.x);
	g_render->DrawOutlinedCircle(XForm2(center, startAngle), Vector2(radius), Color(color).ScaleAlpha(alpha), Color(color).ScaleAlpha(alpha));
}

void PhysicsRender::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
	g_render->DrawLine(p1, p2, Color(color).ScaleAlpha(alpha));
}

void PhysicsRender::DrawTransform(const b2Transform& xf)
{
	g_render->DrawLine(xf.p, xf.p + xf.q.GetXAxis(), Color::Red().ScaleAlpha(alpha));
	g_render->DrawLine(xf.p, xf.p + xf.q.GetYAxis(), Color::Blue().ScaleAlpha(alpha));
}

void PhysicsRender::DrawPoint(const b2Vec2& p, float32 size, const b2Color& color)
{
	DrawCircle(p, size * pointSizeScale, color);
}

void PhysicsRender::DrawFixture(const XForm2& xf, const b2Fixture& fixture, const Color& color, const Color& outlineColor)
{
	switch (fixture.GetType())
	{
		case b2Shape::e_circle:
		{
			b2CircleShape* circle = (b2CircleShape*)fixture.GetShape();
			g_render->DrawOutlinedCircle(XForm2(circle->m_p)*xf, Vector2(circle->m_radius), color, outlineColor);
			break;
		}

		case b2Shape::e_edge:
		{
			b2EdgeShape* edge = (b2EdgeShape*)fixture.GetShape();
			g_render->DrawLine(xf.TransformCoord(edge->m_vertex1), xf.TransformCoord(edge->m_vertex2), color);
			break;
		}

		case b2Shape::e_chain:
		{
			b2ChainShape* chain = (b2ChainShape*)fixture.GetShape();
			int32 count = chain->m_count;
			const b2Vec2* vertices = chain->m_vertices;

			Vector2 v1 = xf.TransformCoord(vertices[0]);
			for (int32 i = 1; i < count; ++i)
			{
				Vector2 v2 = xf.TransformCoord(vertices[i]);
				g_render->DrawLine(v1, v2, color);
				g_render->DrawOutlinedCircle(XForm2(v1)*xf, 0.05f, color, outlineColor, 12);
				v1 = v2;
			}
			break;
		}

		case b2Shape::e_polygon:
		{
			b2PolygonShape* poly = (b2PolygonShape*)fixture.GetShape();
			int32 vertexCount = poly->m_count;
			b2Assert(vertexCount <= b2_maxPolygonVertices);
			b2Vec2 vertices[b2_maxPolygonVertices];

			for (int32 i = 0; i < vertexCount; ++i)
				vertices[i] = xf.TransformCoord(poly->m_vertices[i]);

			g_render->DrawSolidPolygon((Vector2*)vertices, vertexCount, color);
			g_render->DrawPolygon((Vector2*)vertices, vertexCount, outlineColor);
			break;
		}
	}
}

void PhysicsRender::DrawBody(b2Body& physicsBody, const Color& _color, const Color& outlineColor)
{
	const Color color = _color.ScaleAlpha(alpha);
	const b2Transform xf = physicsBody.GetTransform();
	for (const b2Fixture* f = physicsBody.GetFixtureList(); f; f = f->GetNext())
		DrawFixture(xf, *f, color, outlineColor);
	
	for (const b2JointEdge* j = physicsBody.GetJointList(); j; j = j->next)
	{
		b2Joint* joint = j->joint;
		g_render->DrawOutlinedCircle(Vector2(joint->GetAnchorA()), Vector2(0.1f), color, outlineColor, 12);
		g_render->DrawOutlinedCircle(Vector2(joint->GetAnchorB()), Vector2(0.1f), color, outlineColor, 12);
		g_render->DrawLine(joint->GetAnchorA(), joint->GetAnchorB(), outlineColor);
	}
}

void PhysicsRender::DrawString(int x, int y, const char *string, ...)
{
	char output[256];
	va_list args;
	va_start(args, string);
	StringCchVPrintfA( output, 256, string, args );
	va_end(args);
	output[255] = L'\0';

	wchar_t text[256];
	mbstowcs_s(NULL, text, 256, output, _TRUNCATE);
	
	Vector2 pos = Vector2(float(x), float(y));
	g_debugRender.ImmediateRenderTextScreenSpace(pos, text, Color::White(alpha), false);
}

void PhysicsRender::DrawString(const b2Vec2& pw, const char *string, ...)
{
	char output[256];
	va_list args;
	va_start(args, string);
	StringCchVPrintfA( output, 256, string, args );
	va_end(args);
	output[255] = L'\0';

	wchar_t text[256];
	mbstowcs_s(NULL, text, 256, output, _TRUNCATE);
	
	const Vector2 pos = g_render->WorldSpaceToScreenSpace(pw);
	g_debugRender.ImmediateRenderTextScreenSpace(pos, text, Color::White(alpha), false);
}

void PhysicsRender::DrawAABB(b2AABB* aabb, const b2Color& c)
{
    b2Vec2 p1 = aabb->lowerBound;
    b2Vec2 p2 = b2Vec2(aabb->upperBound.x, aabb->lowerBound.y);
    b2Vec2 p3 = aabb->upperBound;
    b2Vec2 p4 = b2Vec2(aabb->lowerBound.x, aabb->upperBound.y);
    
	g_render->DrawLine(p1, p2);
	g_render->DrawLine(p2, p3);
	g_render->DrawLine(p3, p4);
	g_render->DrawLine(p4, p1);
}