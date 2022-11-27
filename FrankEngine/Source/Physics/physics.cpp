////////////////////////////////////////////////////////////////////////////////////////
/*
	Physics Controller
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../objects/gameObject.h"
#include "../physics/physicsRender.h"
#include "../physics/physics.h"

////////////////////////////////////////////////////////////////////////////////////////
// default physics settings

float Physics::defaultFriction				= 0.3f;	// how much friction an object has
ConsoleCommand(Physics::defaultFriction, defaultFriction);

float Physics::defaultRestitution			= 0.2f;	// how bouncy objects are
ConsoleCommand(Physics::defaultRestitution, defaultRestitution);

float Physics::defaultDensity				= 1.0f;	// how heavy objects are
ConsoleCommand(Physics::defaultDensity, defaultDensity);

float Physics::defaultLinearDamping			= 0.2f;	// how quickly objects come to rest
ConsoleCommand(Physics::defaultLinearDamping, defaultLinearDamping);

float Physics::defaultAngularDamping		= 0.1f;	// how quickly objects stop rotating
ConsoleCommand(Physics::defaultAngularDamping, defaultAngularDamping);

Vector2 Physics::worldSize					= Vector2(2000);	// maximum extents of physics world

int32 Physics::velocityIterations = 8;
ConsoleCommand(Physics::velocityIterations, physicsVelocityIterations);

int32 Physics::positionIterations = 3;
ConsoleCommand(Physics::positionIterations, physicsPositionIterations);

ConsoleCommandSimple(bool, showRaycasts,				false);
ConsoleCommandSimple(bool, showContacts,				false);
ConsoleCommandSimple(bool, physicsShowWorldBoundary,	false);
ConsoleCommandSimple(bool, physicsEnable,				true);

////////////////////////////////////////////////////////////////////////////////////////
/*
	Boundary Wall Object
*/
////////////////////////////////////////////////////////////////////////////////////////

class BoundaryObject : public GameObject
{
public:

	BoundaryObject(const Vector2& pos, const Vector2& size) :
		GameObject(pos)
	{
		CreatePhysicsBody(b2_staticBody);
		AddFixtureBox(size);
	}

	void Update() override { SetVisible(physicsShowWorldBoundary); }

private:

	bool ShouldStreamOut() const override		{ return false; }
	bool DestroyOnWorldReset() const override	{ return false; }
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Main Physics Functions
*/
////////////////////////////////////////////////////////////////////////////////////////

Physics::Physics()
{
	ASSERT(!g_physics);
	worldAABB.lowerBound.Set(-worldSize.x, -worldSize.y);
	worldAABB.upperBound.Set(worldSize.x, worldSize.y);
	b2Vec2 gravity;
	gravity.Set(0, 0);
	world = new b2World(gravity);

	world->SetDestructionListener(&destructionListener);
	world->SetContactListener(&contactListener);
	world->SetContactFilter(&contactFilter);
	world->SetDebugDraw(&g_physicsRender);
}

Physics::~Physics()
{
	SAFE_DELETE(world);
}

void Physics::Init()
{
	// call physics init only on startup!
	static bool initialized = false;
	ASSERT(!initialized);
	initialized = true;

	{
		// create boundry objects to keep everything inside the world
		const float size = 10;
		new BoundaryObject(Vector2(0, -worldSize.x + 2*size), Vector2(worldSize.y - size, size));
		new BoundaryObject(Vector2(0, worldSize.x - 2*size),  Vector2(worldSize.y - size, size));
		new BoundaryObject(Vector2(-worldSize.x + 2*size, 0), Vector2(size, worldSize.y - size));
		new BoundaryObject(Vector2(worldSize.x - 2*size, 0),  Vector2(size, worldSize.y - size));
	}
}

void Physics::Render()
{
	if (g_gameControlBase->IsGameplayMode())
		g_physicsRender.Render();
}

void Physics::Update(float delta)
{
	FrankProfilerEntryDefine(L"Physics::Update()", Color::White(), 5);
	
	g_physicsRender.Update();

	raycastCount = 0;
	collideCheckCount = 0;
	world->SetGravity(Terrain::gravity);

	if (physicsEnable)
		world->Step(delta, velocityIterations, positionIterations);

	ProcessEvents();
}

void Physics::ProcessEvents()
{
	// process buffered collision events after physics update
	// this is to fix problems with changing the physics state during an update

	// process contact add events
	for (list<ContactEvent>::iterator it = contactAddEvents.begin(); it != contactAddEvents.end(); ++it) 
	{
		ContactEvent& ce = *it;
		GameObject* obj1 = GameObject::GetFromPhysicsBody(*ce.fixtureA->GetBody());
		GameObject* obj2 = GameObject::GetFromPhysicsBody(*ce.fixtureB->GetBody());

		ASSERT(obj1 && obj2);
		ce.normal *= -1;
		if (!obj1->IsDestroyed())
			obj1->CollisionAdd(*obj2, ce, ce.fixtureA, ce.fixtureB);

		// switch direction of contact event
		swap(ce.fixtureA, ce.fixtureB);
		ce.normal *= -1;

		if (!obj2->IsDestroyed())
			obj2->CollisionAdd(*obj1, ce, ce.fixtureA, ce.fixtureB);
	}
	contactAddEvents.clear();
	
	// process contact persist events
	for (list<b2Contact*>::iterator it = contacts.begin(); it != contacts.end(); ++it) 
	{
		b2Contact* contact = *it;
		b2Fixture* fixtureA = contact->GetFixtureA();
		b2Fixture* fixtureB = contact->GetFixtureB();
		GameObject* obj1 = GameObject::GetFromPhysicsBody(*fixtureA->GetBody());
		GameObject* obj2 = GameObject::GetFromPhysicsBody(*fixtureB->GetBody());

		ContactEvent ce;
		ce.fixtureA = fixtureA;
		ce.fixtureB = fixtureB;
		ce.contact = contact;
		
		if (fixtureA->IsSensor() || fixtureB->IsSensor())
		{
			// sensors don't have manifolds, just use midpoint as center
			const Vector2 p1 = fixtureA->GetAABB(0).GetCenter();
			const Vector2 p2 = fixtureB->GetAABB(0).GetCenter();
			ce.point = 0.5f*p1 + 0.5f*p2;
			ce.normal = (p1 - p2).Normalize();
		}
		else
		{
			b2WorldManifold worldManifold;
			contact->GetWorldManifold(&worldManifold);
			ce.point = worldManifold.points[0];
			ce.normal = worldManifold.normal;
		}

		if (showContacts)
		{
			Line2(ce.point, ce.fixtureA->GetAABB(0).GetCenter()).RenderDebug();
			Line2(ce.point, ce.fixtureB->GetAABB(0).GetCenter()).RenderDebug();
			ce.point.RenderDebug(Color::Yellow(0.5f));
		}

		ASSERT(obj1 && obj2);
		ce.normal *= -1;
		if (!obj1->IsDestroyed())
			obj1->CollisionPersist(*obj2, ce, fixtureA, fixtureB);

		// switch direction of contact event
		swap(ce.fixtureA, ce.fixtureB);
		ce.normal *= -1;

		if (!obj2->IsDestroyed())
			obj2->CollisionPersist(*obj1, ce, fixtureB, fixtureA);
	}

	// process contact remove events
	for (list<ContactRemoveEvent>::iterator it = contactRemoveEvents.begin(); it != contactRemoveEvents.end(); ++it) 
	{
		ContactRemoveEvent& cre = *it;
		ContactEvent& ce = cre.event;

		GameObject* obj1 = g_objectManager.GetObjectFromHandle(cre.handle1);
		GameObject* obj2 = g_objectManager.GetObjectFromHandle(cre.handle2);
		if (!obj1 || !obj2)
			continue; // don't process remove events if objects no longer exit

		if (!obj1->IsDestroyed())
			obj1->CollisionRemove(*obj2, ce, ce.fixtureA, ce.fixtureB);

		// switch direction of contact event
		swap(ce.fixtureA, ce.fixtureB);

		if (!obj2->IsDestroyed())
			obj2->CollisionRemove(*obj1, ce, ce.fixtureA, ce.fixtureB);
	}
	contactRemoveEvents.clear();

	// process contact result events
	for (list<ContactResult>::iterator it = contactResults.begin(); it != contactResults.end(); ++it) 
	{
		ContactResult& cr = *it;
		GameObject* obj1 = GameObject::GetFromPhysicsBody(*cr.ce.fixtureA->GetBody());
		GameObject* obj2 = GameObject::GetFromPhysicsBody(*cr.ce.fixtureB->GetBody());

		ASSERT(obj1 && obj2);
		if (!obj1->IsDestroyed())
			obj1->CollisionResult(*obj2, cr, cr.ce.fixtureA, cr.ce.fixtureB);
		if (!obj2->IsDestroyed())
			obj2->CollisionResult(*obj1, cr, cr.ce.fixtureB, cr.ce.fixtureA);
	}
	contactResults.clear();
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	Physics Helper Functions
*/
////////////////////////////////////////////////////////////////////////////////////////

int Physics::GetBodyCount() const
{
	ASSERT(world);
	return world->GetBodyCount();
}

int Physics::GetProxyCount() const
{
	ASSERT(world);
	return world->GetProxyCount();
}

b2Body* Physics::CreatePhysicsBody(const b2BodyDef& bodyDef)
{
	ASSERT(worldAABB.Contains(Box2AABB(bodyDef.position)));	// physics body out of world
	return world->CreateBody(&bodyDef);
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	Listeners
*/
////////////////////////////////////////////////////////////////////////////////////////

bool ContactFilter::ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB)
{
	++g_physics->collideCheckCount;
	if (!b2ContactFilter::ShouldCollide(fixtureA, fixtureB))
		return false;

	const GameObject* obj1 = GameObject::GetFromPhysicsBody(*fixtureA->GetBody());
	const GameObject* obj2 = GameObject::GetFromPhysicsBody(*fixtureB->GetBody());
	ASSERT(obj1 && obj2);

	if (!obj1->ShouldCollide(*obj2, fixtureA, fixtureB))
		return false;
	else if (!obj2->ShouldCollide(*obj1, fixtureB, fixtureA))
		return false;
	else
		return true;
}

void ContactListener::BeginContact(b2Contact* contact)
{
	ASSERT(find(g_physics->contacts.begin(), g_physics->contacts.end(), contact) == g_physics->contacts.end());

	ContactEvent ce;
	ce.fixtureA = contact->GetFixtureA();
	ce.fixtureB = contact->GetFixtureB();
	ce.contact = contact;

	if (ce.fixtureA->IsSensor() || ce.fixtureB->IsSensor())
	{
		// sensors don't have manifolds, just use midpoint as center
		const Vector2 p1 = ce.fixtureA->GetAABB(0).GetCenter();
		const Vector2 p2 = ce.fixtureB->GetAABB(0).GetCenter();
		ce.point = 0.5f*p1 + 0.5f*p2;
		ce.normal = (p1 - p2).Normalize();
	}
	else
	{
		b2WorldManifold worldManifold;
		contact->GetWorldManifold(&worldManifold);
		ce.point = worldManifold.points[0];
		ce.normal = worldManifold.normal;
	}
	
	// add to contacts list
	g_physics->contactAddEvents.push_back(ce);
	g_physics->contacts.push_back(contact);
}

void ContactListener::EndContact(b2Contact* contact)
{
	// remove from the contacts list
	list<b2Contact*>::iterator itContact = find(g_physics->contacts.begin(), g_physics->contacts.end(), contact);
	ASSERT(itContact != g_physics->contacts.end());
	g_physics->contacts.erase(itContact);

	// only do remove event if object stills exist (this may cause remove events to not occur if objet is destroyed while contacting)
	// todo: if this causes a problem, maybe destroy physics objects before object is destroyed
	GameObject* obj1 = GameObject::GetFromPhysicsBody(*contact->GetFixtureA()->GetBody());
	GameObject* obj2 = GameObject::GetFromPhysicsBody(*contact->GetFixtureB()->GetBody());
	if (!obj1 || !obj1->GetPhysicsBody() || obj1->IsDestroyed())
		return;
	if (!obj2 || !obj2->GetPhysicsBody() || obj2->IsDestroyed())
		return;

	// there is a problem when a body is destroyed it is also sending a contact remove event
	ContactRemoveEvent cre;
	ContactEvent& ce = cre.event;
	ce.fixtureA = contact->GetFixtureA();
	ce.fixtureB = contact->GetFixtureB();
	ce.contact = contact;
	
	if (ce.fixtureA->IsSensor() || ce.fixtureB->IsSensor())
	{
		// sensors don't have manifolds, just use midpoint as center
		const Vector2 p1 = ce.fixtureA->GetAABB(0).GetCenter();
		const Vector2 p2 = ce.fixtureB->GetAABB(0).GetCenter();
		ce.point = 0.5f*p1 + 0.5f*p2;
		ce.normal = (p1 - p2).Normalize();
	}
	else
	{
		b2WorldManifold worldManifold;
		contact->GetWorldManifold(&worldManifold);
		ce.point = worldManifold.points[0];
		ce.normal = worldManifold.normal;
	}
	
	cre.handle1 = obj1->GetHandle();
	cre.handle2 = obj2->GetHandle();
	g_physics->contactRemoveEvents.push_back(cre);
}

void ContactListener::PreSolve(b2Contact* contact, const b2Manifold* oldManifold)
{
	ContactEvent ce;
	ce.fixtureA = contact->GetFixtureA();
	ce.fixtureB = contact->GetFixtureB();
	ce.contact = contact;
	
	if (ce.fixtureA->IsSensor() || ce.fixtureB->IsSensor())
	{
		// sensors don't have manifolds, just use midpoint as center
		const Vector2 p1 = ce.fixtureA->GetAABB(0).GetCenter();
		const Vector2 p2 = ce.fixtureB->GetAABB(0).GetCenter();
		ce.point = 0.5f*p1 + 0.5f*p2;
		ce.normal = (p1 - p2).Normalize();
	}
	else
	{
		b2WorldManifold worldManifold;
		contact->GetWorldManifold(&worldManifold);
		ce.point = worldManifold.points[0];
		ce.normal = worldManifold.normal;
	}

	GameObject* obj1 = GameObject::GetFromPhysicsBody(*ce.fixtureA->GetBody());
	GameObject* obj2 = GameObject::GetFromPhysicsBody(*ce.fixtureB->GetBody());
	ASSERT(obj1 && obj2);

	obj1->CollisionPreSolve(*obj2, ce, ce.fixtureA, ce.fixtureB, oldManifold);
	obj2->CollisionPreSolve(*obj1, ce, ce.fixtureB, ce.fixtureA, oldManifold);
}

void ContactListener::PostSolve(b2Contact* contact, const b2ContactImpulse* impulse)
{
	ContactResult cr;
	cr.ce.fixtureA = contact->GetFixtureA();
	cr.ce.fixtureB = contact->GetFixtureB();
	cr.ce.contact = contact;
	
	if (cr.ce.fixtureA->IsSensor() || cr.ce.fixtureB->IsSensor())
	{
		// sensors don't have manifolds, just use midpoint as center
		const Vector2 p1 = cr.ce.fixtureA->GetAABB(0).GetCenter();
		const Vector2 p2 = cr.ce.fixtureB->GetAABB(0).GetCenter();
		cr.ce.point = 0.5f*p1 + 0.5f*p2;
		cr.ce.normal = (p1 - p2).Normalize();
	}
	else
	{
		b2WorldManifold worldManifold;
		contact->GetWorldManifold(&worldManifold);
		cr.ce.point = worldManifold.points[0];
		cr.ce.normal = worldManifold.normal;
	}
	
    for (int32 i = 0; i < impulse->count; ++i)
        cr.normalImpulse = Max(cr.normalImpulse, impulse->normalImpulses[i]);
    for (int32 i = 0; i < impulse->count; ++i)
        cr.tangentImpulse = Max(cr.tangentImpulse, impulse->tangentImpulses[i]);

	g_physics->contactResults.push_back(cr);
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	Raycasts
*/
////////////////////////////////////////////////////////////////////////////////////////

static FrankProfilerEntry rayCastProfilerEntry(L"Physics::Raycast()", Color::White(), 5);

const GameSurfaceInfo& SimpleRaycastResult::GetSurfaceInfo() const 
{
	return GameSurfaceInfo::Get(hitSurface);
}

void SimpleRaycastResult::RenderDebug(const Color& color, float radius, float time) const
{
	point.RenderDebug(color, radius, time);
	Line2(point, point + 2*radius*normal).RenderDebug(color, time);
}

class RaycastQueryCallback : public b2QueryCallback
{
public:

	RaycastQueryCallback(const GameObject* _ignoreObject, const b2Vec2& _point, bool _sightCheck = false, bool _ignoreSensors = true) :
		ignoreObject(_ignoreObject),
		hitFixture(NULL),
		point(_point),
		sightCheck(_sightCheck),
		ignoreSensors(_ignoreSensors)
	{}

	/// Called for each fixture found in the query AABB.
	/// @return false to terminate the query.
	bool ReportFixture(b2Fixture* fixture) override
	{
		b2Body* body = fixture->GetBody();
		GameObject* object = GameObject::GetFromPhysicsBody(*body);
		ASSERT(object); // all bodies should have objects
		if (object == ignoreObject)
			return true;

		// don't collide with sensors
		if (ignoreSensors && fixture->IsSensor())
			return true;

		// sight raycast check
		if (sightCheck && !object->ShouldCollideSight())
			return true;
		
		// check if it inside the fixture
		if (!fixture->TestPoint(point))
			return true;

		// do object should collide check
		if (ignoreObject)
		{
			++g_physics->collideCheckCount;
			if (!object->ShouldCollide(*ignoreObject, fixture, NULL))
				return true;

			if (!ignoreObject->ShouldCollide(*object, NULL, fixture))
				return true;
		}
		
		hitFixture = fixture;
		return false;
	}

	const GameObject* ignoreObject;
	b2Fixture* hitFixture;
	b2Vec2 point;
	bool sightCheck;
	bool ignoreSensors;
};

class RayCastClosestCallback : public b2RayCastCallback
{
public:
	
	RayCastClosestCallback(const GameObject* _ignoreObject = NULL, bool _sightCheck = false, bool _ignoreSensors = true) :
		ignoreObject(_ignoreObject),
		hitObject(NULL),
		hitFixture(NULL),
		lambda(0),
		sightCheck(_sightCheck),
		ignoreSensors(_ignoreSensors)
	{}
	
	float32 ReportFixture(b2Fixture* fixture, const b2Vec2& _point, const b2Vec2& _normal, float32 fraction)
	{
		b2Body* body = fixture->GetBody();
		GameObject* object = static_cast<GameObject*>(body->GetUserData());
		ASSERT(object); // all bodies should have objects
		if (object == ignoreObject)
			return -1.0f;

		// don't collide with sensors
		if (ignoreSensors && fixture->IsSensor())
			return -1.0f;

		// sight raycast check
		if (sightCheck && !object->ShouldCollideSight())
			return -1.0f;
	
		// do object should collide check
		if (ignoreObject)
		{
			++g_physics->collideCheckCount;
			if (!object->ShouldCollide(*ignoreObject, fixture, NULL))
				return -1.0f;

			if (!ignoreObject->ShouldCollide(*object, NULL, fixture))
				return -1.0f;
		}

		hitObject = object;
		point = _point;
		normal = _normal;
		hitFixture = fixture;
		lambda = fraction;
		return fraction;
	}

	const GameObject* ignoreObject;
	GameObject* hitObject;
	b2Fixture* hitFixture;
	b2Vec2 point;
	b2Vec2 normal;
	float lambda;
	bool sightCheck;
	bool ignoreSensors;
};

GameObject* Physics::PointcastSimple(const Vector2& point, SimpleRaycastResult* result, const GameObject* ignoreObject, bool sightCheck, bool ignoreSensors)
{
	FrankProfilerBlockTimer profilerBlock(rayCastProfilerEntry);

	if (!ignoreObject)
		ignoreObject = g_cameraBase;	// use camera as ignore object if there isn't one

	RaycastQueryCallback queryCallback(ignoreObject, point, sightCheck, ignoreSensors);
		
	if (showRaycasts)
		point.RenderDebug(Color(1.0f, 0.8f, 0.5f, 0.5f));

	// make a small box
	b2Vec2 d(0.001f, 0.001f);
	b2AABB aabb = {b2Vec2(point) - d, b2Vec2(point) + d};
	world->QueryAABB(&queryCallback, aabb);

	if (result)
	{
		result->point = point;
		result->normal = Vector2(0);
		result->lambda = 0;
		result->hitFixture = queryCallback.hitFixture;
		if (queryCallback.hitFixture)
			result->hitSurface = (int)queryCallback.hitFixture->GetUserData();
	}

	if (!queryCallback.hitFixture)
		return NULL;

	return GameObject::GetFromPhysicsBody(*queryCallback.hitFixture->GetBody());
}

GameObject* Physics::RaycastSimple(const Line2& line, SimpleRaycastResult* result, const GameObject* ignoreObject, bool sightCheck, bool ignoreSensors)
{
	FrankProfilerBlockTimer profilerBlock(rayCastProfilerEntry);

	if (!ignoreObject)
		ignoreObject = g_cameraBase;	// use camera as ignore object if there isn't one

	{
		// check inside solid fixtures
		// First check if we are starting inside an object
		RaycastQueryCallback queryCallback(ignoreObject, line.p1, sightCheck, ignoreSensors);

		// make a small box
		b2Vec2 d(0.001f, 0.001f);
		b2AABB aabb = {b2Vec2(line.p1) - d, b2Vec2(line.p1) + d};
		world->QueryAABB(&queryCallback, aabb);

		if (queryCallback.hitFixture)
		{
			GameObject* hitObject = GameObject::GetFromPhysicsBody(*queryCallback.hitFixture->GetBody());
			if (result)
			{
				// raycast is starting inside a fixture
				result->point = line.p1;
				result->normal = (line.p1 - line.p2).Normalize();
				result->hitFixture = queryCallback.hitFixture;
				if (queryCallback.hitFixture)
					result->hitSurface = (int)queryCallback.hitFixture->GetUserData();
			}
		
			if (showRaycasts)
				line.p1.RenderDebug(Color(1.0f, 0.8f, 0.5f, 0.5f));

			return GameObject::GetFromPhysicsBody(*queryCallback.hitFixture->GetBody());
		}
	}

	RayCastClosestCallback raycastResult(ignoreObject, sightCheck, ignoreSensors);
	Raycast(line, raycastResult);

	if (result)
	{
		if (raycastResult.hitObject)
		{
			result->point = raycastResult.point;
			result->normal = raycastResult.normal;
			result->lambda = raycastResult.lambda;
			result->hitFixture = raycastResult.hitFixture;
			if (raycastResult.hitFixture)
				result->hitSurface = (int)raycastResult.hitFixture->GetUserData();
		}
		else
		{
			// if there was no hit then put raycast at the end
			result->point = line.p2;
			result->normal = (line.p1 - line.p2).Normalize();
			result->lambda = 1;
		}
	}

	return raycastResult.hitObject;
}

void Physics::Raycast(const Line2& line, b2RayCastCallback& raycastResult)
{
	FrankProfilerBlockTimer profilerBlock(rayCastProfilerEntry);

	if (line.p1.x == line.p2.x && line.p1.y == line.p2.y)
		return;

	if (showRaycasts)
		line.RenderDebug(Color(1.0f, 0.8f, 0.5f, 0.5f));
	
	++raycastCount;
	world->RayCast(&raycastResult, line.p1, line.p2);
}

class QueryAABBCallback : public b2QueryCallback
{
public:

	QueryAABBCallback(Box2AABB _box, GameObject** _hitObjects, unsigned _maxHitObjectCount, const GameObject* _ignoreObject, bool _ignoreSensors) :
	  box(_box),
	  hitObjects(_hitObjects),
	  ignoreObject(_ignoreObject),
	  maxHitObjectCount(_maxHitObjectCount),
	  resultCount(0),
	  ignoreSensors(_ignoreSensors)
	{}

	/// Called for each fixture found in the query AABB.
	/// @return false to terminate the query.
	bool ReportFixture(b2Fixture* fixture) override
	{
		if (ignoreSensors && fixture->IsSensor())
			return true;

		b2Body* body = fixture->GetBody();
		GameObject* object = GameObject::GetFromPhysicsBody(*body);
		ASSERT(object); // all bodies should have objects
		if (object == ignoreObject)
			return true;
		
		// do object should collide check
		if (ignoreObject)
		{
			++g_physics->collideCheckCount;
			if (!object->ShouldCollide(*ignoreObject, fixture, NULL))
				return true;

			if (!ignoreObject->ShouldCollide(*object, NULL, fixture))
				return true;
		}
		
		hitObjects[resultCount] = object;
		++resultCount;

		return (resultCount < maxHitObjectCount);
	}

	Box2AABB box;
	GameObject** hitObjects;
	const GameObject* ignoreObject;
	const unsigned maxHitObjectCount;
	unsigned resultCount;
	bool ignoreSensors;
};

unsigned Physics::QueryAABB(const Box2AABB& box, GameObject** hitObjects, unsigned maxHitObjectCount, const GameObject* ignoreObject, bool ignoreSensors)
{
	ASSERT(hitObjects && maxHitObjectCount > 0);
	FrankProfilerBlockTimer profilerBlock(rayCastProfilerEntry);
	if (showRaycasts)
		box.RenderDebug(Color(1.0f, 0.8f, 0.5f, 0.5f));

	if (!ignoreObject)
		ignoreObject = g_cameraBase;	// use camera as ignore object if there isn't one
	
	QueryAABBCallback queryCallback(box, hitObjects, maxHitObjectCount, ignoreObject, ignoreSensors);
	world->QueryAABB(&queryCallback, box);
	return queryCallback.resultCount;
}	

GameObject* Physics::QueryAABBSimple(const Box2AABB& box, const GameObject* ignoreObject, bool ignoreSensors)
{
	FrankProfilerBlockTimer profilerBlock(rayCastProfilerEntry);
	if (showRaycasts)
		box.RenderDebug(Color(1.0f, 0.8f, 0.5f, 0.5f));

	if (!ignoreObject)
		ignoreObject = g_cameraBase;	// use camera as ignore object if there isn't one

	GameObject* hitObject[1];
	QueryAABBCallback queryCallback(box, hitObject, 1, ignoreObject, ignoreSensors);
	world->QueryAABB(&queryCallback, box);
	return (queryCallback.resultCount > 0) ? hitObject[0] : NULL;
}

float Physics::ComputeArea(const b2Shape& shape)
{
	switch (shape.GetType())
	{
		case b2Shape::e_circle: 
		{
			return PI * Square(shape.m_radius);
		}
		case b2Shape::e_polygon:
		{
			const b2PolygonShape& polygonShape = static_cast<const b2PolygonShape&>(shape);

			b2Assert(polygonShape.m_count >= 3);

			// s is the reference point for forming triangles.
			// It's location doesn't change the result (except for rounding error).
			b2Vec2 s(0.0f, 0.0f);

			// This code would put the reference point inside the polygon.
			for (int32 i = 0; i < polygonShape.m_count; ++i)
			{
				s += polygonShape.m_vertices[i];
			}
			s *= 1.0f / polygonShape.m_count;
			
			float32 area = 0.0f;
			for (int32 i = 0; i < polygonShape.m_count; ++i)
			{
				// Triangle vertices.
				b2Vec2 e1 = polygonShape.m_vertices[i] - s;
				b2Vec2 e2 = i + 1 < polygonShape.m_count ? polygonShape.m_vertices[i+1] - s : polygonShape.m_vertices[0] - s;

				float32 D = b2Cross(e1, e2);

				float32 triangleArea = 0.5f * D;
				area += triangleArea;
			}

			return area;
		}
		default:
			return 0;
	}
}

// make gravitational forces are sub steped with physics
b2Vec2 FrankEngineGetGravity(b2Body* b)
{
	// calculate gravity based on object position
	const GameObject* obj = GameObject::GetFromPhysicsBody(*b);
	if (!obj)
		return g_gameControlBase->GetGravity( b->GetWorldCenter() );
	return obj->HasGravity() ? obj->GetGravity() : Vector2(0);
}

// hook to override linear dampening
float FrankEngineGetLinearDamping(b2Body* b)
{
	 return b->GetLinearDamping();
}

// hook to override angular dampening
float FrankEngineGetAngularDamping(b2Body* b)
{
	 return b->GetAngularDamping();
}

