/*
* Copyright (c) 2006-2009 Erin Catto http://www.box2d.org
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
*/

#ifndef TEST_H
#define TEST_H

#include "Box2D/Box2D.h"
#include <stdlib.h>

class Test;
struct Settings;

typedef Test* TestCreateFcn();

#define	RAND_LIMIT	32767
#define DRAW_STRING_NEW_LINE 20

/// Random number in range [-1,1]
inline float32 RandomFloat()
{
	float32 r = (float32)(rand() & (RAND_LIMIT));
	r /= RAND_LIMIT;
	r = 2.0f * r - 1.0f;
	return r;
}

/// Random floating point number in range [lo, hi]
inline float32 RandomFloat(float32 lo, float32 hi)
{
	float32 r = (float32)(rand() & (RAND_LIMIT));
	r /= RAND_LIMIT;
	r = (hi - lo) * r + lo;
	return r;
}

/// Test settings. Some can be controlled in the GUI.
struct Settings
{
	Settings()
	{
		hz = 60.0f;
		velocityIterations = 8;
		positionIterations = 3;
		drawShapes = true;
		drawJoints = true;
		drawAABBs = false;
		drawContactPoints = false;
		drawContactNormals = false;
		drawContactImpulse = false;
		drawFrictionImpulse = false;
		drawCOMs = false;
		drawStats = false;
		drawProfile = false;
		enableWarmStarting = true;
		enableContinuous = true;
		enableSubStepping = false;
		enableSleep = true;
		pause = false;
		singleStep = false;
	}

	float32 hz;
	int32 velocityIterations;
	int32 positionIterations;
	bool drawShapes;
	bool drawJoints;
	bool drawAABBs;
	bool drawContactPoints;
	bool drawContactNormals;
	bool drawContactImpulse;
	bool drawFrictionImpulse;
	bool drawCOMs;
	bool drawStats;
	bool drawProfile;
	bool enableWarmStarting;
	bool enableContinuous;
	bool enableSubStepping;
	bool enableSleep;
	bool pause;
	bool singleStep;
};

struct TestEntry
{
	const char *name;
	TestCreateFcn *createFcn;
};

extern TestEntry g_testEntries[];
// This is called when a joint in the world is implicitly destroyed
// because an attached body is destroyed. This gives us a chance to
// nullify the mouse joint.
class TestDestructionListener : public b2DestructionListener
{
public:
	void SayGoodbye(b2Fixture* fixture) override { B2_NOT_USED(fixture); }
	void SayGoodbye(b2Joint* joint) override;

	Test* test;
};

const int32 k_maxContactPoints = 2048;

struct ContactPoint
{
	b2Fixture* fixtureA;
	b2Fixture* fixtureB;
	b2Vec2 normal;
	b2Vec2 position;
	b2PointState state;
	float32 normalImpulse;
	float32 tangentImpulse;
	float32 separation;
};

class Test : public b2ContactListener
{
public:

	Test();
	virtual ~Test();

	void DrawTitle(const char *string);
	virtual void Step(Settings* settings);
	virtual void Keyboard(int key) { B2_NOT_USED(key); }
	virtual void KeyboardUp(int key) { B2_NOT_USED(key); }
	void ShiftMouseDown(const b2Vec2& p);
	virtual void MouseDown(const b2Vec2& p);
	virtual void MouseUp(const b2Vec2& p);
	void MouseMove(const b2Vec2& p);
	void LaunchBomb();
	void LaunchBomb(const b2Vec2& position, const b2Vec2& velocity);
	
	void SpawnBomb(const b2Vec2& worldPt);
	void CompleteBombSpawn(const b2Vec2& p);

	// Let derived tests know that a joint was destroyed.
	virtual void JointDestroyed(b2Joint* joint) { B2_NOT_USED(joint); }

	// Callbacks for derived classes.
	virtual void BeginContact(b2Contact* contact)  override { B2_NOT_USED(contact); }
	virtual void EndContact(b2Contact* contact)  override { B2_NOT_USED(contact); }
	virtual void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override;
	virtual void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override
	{
		B2_NOT_USED(contact);
		B2_NOT_USED(impulse);
	}

	void ShiftOrigin(const b2Vec2& newOrigin);

protected:
	friend class TestDestructionListener;
	friend class BoundaryListener;
	friend class ContactListener;

	b2Body* m_groundBody;
	b2AABB m_worldAABB;
	ContactPoint m_points[k_maxContactPoints];
	int32 m_pointCount;
	TestDestructionListener m_destructionListener;
	int32 m_textLine;
	b2World* m_world;
	b2Body* m_bomb;
	b2MouseJoint* m_mouseJoint;
	b2Vec2 m_bombSpawnPoint;
	bool m_bombSpawning;
	b2Vec2 m_mouseWorld;
	int32 m_stepCount;

	b2Profile m_maxProfile;
	b2Profile m_totalProfile;
};

////////////////////////////////////////////////////////////////////////

#define GLFW_KEY_A ('A')
#define GLFW_KEY_B ('B')
#define GLFW_KEY_C ('C')
#define GLFW_KEY_D ('D')
#define GLFW_KEY_E ('E')
#define GLFW_KEY_F ('F')
#define GLFW_KEY_G ('G')
#define GLFW_KEY_H ('H')
#define GLFW_KEY_I ('I')
#define GLFW_KEY_J ('J')
#define GLFW_KEY_K ('K')
#define GLFW_KEY_L ('L')
#define GLFW_KEY_M ('M')
#define GLFW_KEY_N ('N')
#define GLFW_KEY_O ('O')
#define GLFW_KEY_P ('P')
#define GLFW_KEY_Q ('Q')
#define GLFW_KEY_R ('R')
#define GLFW_KEY_S ('S')
#define GLFW_KEY_T ('T')
#define GLFW_KEY_U ('U')
#define GLFW_KEY_V ('V')
#define GLFW_KEY_W ('W')
#define GLFW_KEY_X ('X')
#define GLFW_KEY_Y ('Y')
#define GLFW_KEY_Z ('Z')
#define GLFW_KEY_COMMA (VK_OEM_COMMA)

#define GLFW_KEY_0 ('0')
#define GLFW_KEY_1 ('1')
#define GLFW_KEY_2 ('2')
#define GLFW_KEY_3 ('3')
#define GLFW_KEY_4 ('4')
#define GLFW_KEY_5 ('5')
#define GLFW_KEY_6 ('6')
#define GLFW_KEY_7 ('7')
#define GLFW_KEY_8 ('8')
#define GLFW_KEY_9 ('9')

#endif
