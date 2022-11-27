////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../terrain/terrain.h"
#include "../objects/particleSystem.h"
#include "../objects/gameObject.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object Class Globals
*/
////////////////////////////////////////////////////////////////////////////////////////

GameObjectHandle GameObject::nextUniqueHandleValue = 1;	// starting unique handle
const GameObjectHandle GameObject::invalidHandle = 0;	// invalid handle value

float GameObject::defaultSoundDistanceMin = 10;
ConsoleCommand(GameObject::defaultSoundDistanceMin, soundDefaultDistanceMin);

float GameObject::defaultSoundDistanceMax = 1000;
ConsoleCommand(GameObject::defaultSoundDistanceMax, soundDefaultDistanceMax);

float GameObject::defaultFrequencyRandomness = 0.05f;
ConsoleCommand(GameObject::defaultFrequencyRandomness, soundDefaultFrequencyRandomness);

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object Member Functions
*/
////////////////////////////////////////////////////////////////////////////////////////

GameObject::GameObject(const GameObjectStub& stub, GameObject* _parent, bool addToWorld) :
	gameObjectType(stub.type),
	stubSize(stub.size),
	stubXF(stub.xf),
	xfLocal(stub.xf),
	xfWorld(stub.xf),
	xfWorldLast(stub.xf),
	parent(NULL),
	flags(ObjectFlag_JustAdded|ObjectFlag_Visible|ObjectFlag_Gravity),
	physicsBody(NULL),
	renderGroup(1),
	team(GameTeam(0))
{
	// give it the stubs handle
	if (stub.handle != invalidHandle)
		handle = stub.handle;
	else
		handle = nextUniqueHandleValue++;

	// automatically add the object to the world
	if (addToWorld)
		g_objectManager.Add(*this);

	if (_parent)
	{
		_parent->AttachChild(*this);

		// refresh my world transform since I am in parent space
		UpdateTransforms();

		// reset world delta so we dont get a huge offset from 0 to where the parent was
		xfWorldLast = xfWorld;
	}
}

GameObject::GameObject(const XForm2& xf, GameObject* _parent, GameObjectType _gameObjectType, bool addToWorld) :
	gameObjectType(_gameObjectType),
	stubSize(0),
	stubXF(xf),
	xfLocal(xf),
	xfWorld(xf),
	xfWorldLast(xf),
	parent(NULL),
	handle(nextUniqueHandleValue++),
	flags(ObjectFlag_JustAdded|ObjectFlag_Visible|ObjectFlag_Gravity),
	physicsBody(NULL),
	renderGroup(1),
	team(GameTeam(0))
{
	// automatically add the object to the world
	if (addToWorld)
		g_objectManager.Add(*this);

	if (_parent)
	{
		_parent->AttachChild(*this);

		// refresh my world transform since I am in parent space
		UpdateTransforms();

		// reset world delta so we dont get a huge offset from 0 to where the parent was
		xfWorldLast = xfWorld;
	}
}

GameObject::~GameObject()
{
	// never delete game objects directly, instead call Destroy()
	// this also prevents objects from being accidentally created on the stack
	ASSERT(!GameObjectManager::GetLockDeleteObjects()); 
	DestroyPhysicsBody();
}

GameObjectStub GameObject::Serialize() const
{ 
	return GameObjectStub(GetXFWorld(), stubSize, gameObjectType, NULL, handle); 
}

void GameObject::CreatePhysicsBody(b2BodyType type, bool fixedRotation, bool allowSleeping)
{
	b2BodyDef bodyDef;
	bodyDef.type = type;
	bodyDef.position = xfWorld.position;
	bodyDef.angle = xfWorld.angle;
	bodyDef.linearDamping = Physics::defaultLinearDamping;
	bodyDef.angularDamping = Physics::defaultAngularDamping;
	bodyDef.userData = this;
	bodyDef.fixedRotation = fixedRotation;
	bodyDef.allowSleep = allowSleeping;
	CreatePhysicsBody(bodyDef);
}

b2Fixture* GameObject::AddFixture(const b2FixtureDef& fixtureDef)
{
	ASSERT(physicsBody);

	// only static bodies can have 0 density
	ASSERT(fixtureDef.density != 0 || fixtureDef.isSensor || physicsBody->GetType() == b2_staticBody);

	// attach the shape
	return physicsBody->CreateFixture(&fixtureDef);
}
	
b2Fixture* GameObject::AddFixture(const b2Shape& shape, float density, float friction, float restitution, void* userData)
{
	ASSERT(physicsBody);

	// create and add the fixture
	b2FixtureDef fixtureDef;
	fixtureDef.shape = &shape;
	fixtureDef.density = density;
	fixtureDef.friction = friction;
	fixtureDef.restitution = restitution;
	fixtureDef.userData = userData;
	return AddFixture(fixtureDef);
}
	
b2Fixture* GameObject::AddSensor(const b2Shape& shape, void* userData)
{
	ASSERT(physicsBody);

	// create and add the fixture
	b2FixtureDef fixtureDef;
	fixtureDef.shape = &shape;
	fixtureDef.userData = userData;
	fixtureDef.isSensor = true;
	return AddFixture(fixtureDef);
}

b2Fixture* GameObject::AddFixtureBox(const Vector2& size, float density, float friction, float restitution)
{
	ASSERT(size.x > 0 && size.y > 0);
	b2PolygonShape shape;
	shape.SetAsBox(size.x, size.y);
	return AddFixture(shape, density, friction, restitution);
}

b2Fixture* GameObject::AddFixtureBox(const Vector2& size, const XForm2& offset, float density, float friction, float restitution)
{
	ASSERT(size.x > 0 && size.y > 0);
	b2PolygonShape shape;
	shape.SetAsBox(size.x, size.y, offset.position, offset.angle);
	return AddFixture(shape, density, friction, restitution);
}

b2Fixture* GameObject::AddFixtureCircle(const float radius, float density, float friction, float restitution)
{
	ASSERT(radius > 0);
	b2CircleShape shape;
	shape.m_radius = radius;
	return AddFixture(shape, density, friction, restitution);
}

b2Fixture* GameObject::AddFixturePolygon(const Vector2* points, int count, float density, float friction, float restitution)
{
	b2PolygonShape shape;
	shape.Set(reinterpret_cast<const b2Vec2*>(points), count);
	return AddFixture(shape, density, friction, restitution);
}

b2Fixture* GameObject::AddFixtureEdge(const Vector2& p1, const Vector2& p2, float density, float friction, float restitution)
{
	b2EdgeShape shape;
	shape.Set(p1, p2);
	return AddFixture(shape, density, friction, restitution);
}

b2Fixture* GameObject::AddFixtureEdge(const Vector2& p1, const Vector2& p2, const Vector2& p0, const Vector2& p3, float density, float friction, float restitution)
{
	b2EdgeShape shape;
	shape.Set(p1, p2);
	shape.m_vertex0 = p0;
	shape.m_vertex3 = p3;
	shape.m_hasVertex0 = true;
	shape.m_hasVertex3 = true;
	return AddFixture(shape, density, friction, restitution);
}

b2Fixture* GameObject::AddSensorBox(const Vector2& size, const XForm2& offset)
{
	ASSERT(size.x > 0 && size.y > 0);
	b2PolygonShape shape;
	shape.SetAsBox(size.x, size.y, offset.position, offset.angle);
	return AddSensor(shape);
}

b2Fixture* GameObject::AddSensorBox(const Vector2& size)
{
	ASSERT(size.x > 0 && size.y > 0);
	b2PolygonShape shape;
	shape.SetAsBox(size.x, size.y);
	return AddSensor(shape);
}

b2Fixture* GameObject::AddSensorCircle(const float radius)
{
	ASSERT(radius > 0);
	b2CircleShape shape;
	shape.m_radius = radius;
	return AddSensor(shape);
}

b2Fixture* GameObject::AddSensorPolygon(const Vector2* points, int count)
{
	b2PolygonShape shape;
	shape.Set(reinterpret_cast<const b2Vec2*>(points), count);
	return AddSensor(shape);
}

void GameObject::RemoveFixture(b2Fixture& fixture)
{
	ASSERT(physicsBody);

	// destroy the fixture
	physicsBody->DestroyFixture(&fixture);
}

void GameObject::RemoveAllFixtures()
{
	ASSERT(physicsBody);

	for (b2Fixture* f = physicsBody->GetFixtureList(); f; )
	{
		b2Fixture* fLast = f;
		f = f->GetNext();
		physicsBody->DestroyFixture(fLast);
	}
}

bool GameObject::ShouldStreamOut() const
{
	const Box2AABB& streamWindow = g_terrain->GetStreamWindow();
	const ObjectTypeInfo* objectTypeInfo = GetObjectInfo();
	if (objectTypeInfo && objectTypeInfo->IsSerializable())
	{
		// serializable objects stream out when their aabb goes out of the stream window
		const Box2AABB stubAABB(GetXFWorld(), GetStubSize());
		if (Terrain::streamDebug)
			stubAABB.RenderDebug();
		return !streamWindow.FullyContains(stubAABB);
	}
	else if (IsStatic())
	{
		// static objects stream when center is out of the window
		return !streamWindow.Contains(GetPosWorld());
	}

	// dynamic objects must be fully contained in the window
	return !FullyContainedBy(streamWindow);
}

Box2AABB GameObject::GetPhysicsAABB(bool includeSensors) const
{
	if (!physicsBody)
		return Box2AABB(GetPosWorld());

	Box2AABB aabb;
	bool isFirst = true;
	for (const b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
	{
		if (f->IsSensor())
			continue;	// we only care about solid shapes

		for(int i = 0; i < f->GetProxyCount(); ++i)
		{
			if (!includeSensors && f->IsSensor())
				continue;

			const Box2AABB shapeAABB = f->GetAABB(i);
			if (isFirst)
			{
				isFirst = false;
				aabb = shapeAABB;
			}
			else
				aabb.Combine(shapeAABB);
		}
	}

	if (isFirst) // no fixtures found
		return Box2AABB(GetPosWorld());

	return aabb;
}

bool GameObject::FullyContainedBy(const Box2AABB& bbox) const
{
	if (physicsBody)
	{
		// test all my shapes
		for (const b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
		{
			if (f->IsSensor())
				continue;	// we only care about solid shapes

			for(int i = 0; i < f->GetProxyCount(); ++i)
			{
				const b2AABB shapeAABB = f->GetAABB(i);
				//g_debugRender.RenderBox(shapeAABB);
				if (!bbox.FullyContains(shapeAABB))
					return false;
			}
		}
	}

	// test all children shapes
	for (list<GameObject*>::const_iterator it = GetChildren().begin(); it != GetChildren().end(); ++it) 
	{
		const GameObject& obj = **it;
		if (!obj.FullyContainedBy(bbox))
			return false;
	}

	return bbox.Contains(xfWorld.position);
}

bool GameObject::PartiallyContainedBy(const Box2AABB& bbox) const
{
	if (physicsBody)
	{
		// test all my shapes
		for (const b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
		{
			if (f->IsSensor())
				continue;	// we only care about solid shapes

			for(int i = 0; i < f->GetProxyCount(); ++i)
			{
				const b2AABB shapeAABB = f->GetAABB(i);
				//g_debugRender.RenderBox(shapeAABB);
				if (!bbox.PartiallyContains(shapeAABB))
					return false;
			}
		}
	}

	// test all children shapes
	for (list<GameObject*>::const_iterator it = GetChildren().begin(); it != GetChildren().end(); ++it) 
	{
		const GameObject& obj = **it;
		if (obj.PartiallyContainedBy(bbox))
			return true;
	}

	return bbox.Contains(xfWorld.position);
}

void GameObject::DetachEmitters() 
{ 
	// detach and kill all particle emitters
	for (list<GameObject*>::const_iterator it = GetChildren().begin(); it != GetChildren().end();) 
	{
		GameObject& obj = **it;
		if (!obj.IsParticleEmitter())
		{
			++it;
			continue;
		}

		ParticleEmitter& emitter = static_cast<ParticleEmitter&>(obj);
		it = GetChildren().erase(it);
		emitter.parent = NULL;
		// update this child's xform since it is no longer childed
		emitter.SetXFLocal(emitter.GetXFWorld());
		emitter.Kill();
	}
}

void GameObject::Destroy()
{
	if (IsDestroyed())
		return;

	// TODO: handle joints
	// go through all joints to this body
	// callback to JointDestroyed function
	// JointDestroyed will by default tell that object to be destroyed
	// so all connected objects in a joint chain will be destroyed
	// objects can override JointDestroyed if they dont want to be destroyed

	DetachParent();

	// recursivly destroy all children
	for (list<GameObject*>::iterator it = children.begin(); it != children.end(); ++it)
	{
		(**it).parent = NULL;
		(**it).Destroy();
	}
	children.clear();
	SetFlag(ObjectFlag_Destroyed, true);
}

SoundObjectSmartPointer GameObject::MakeSound(SoundID si, float volume, float frequency, float frequencyRandomness, float distanceMin, float distanceMax)
{
	return g_sound->PlayInternal(si, *this, volume, frequency, frequencyRandomness, distanceMin, distanceMax);
}

const ObjectTypeInfo* GameObject::GetObjectInfo() const
{
	if (GameObjectStub::HasObjectInfo(gameObjectType))
		return &GameObjectStub::GetObjectInfo(gameObjectType);
	return NULL; 
}

void GameObject::AttachChild(GameObject& child) 
{ 
	ASSERT(!IsDestroyed());
	ASSERT(!child.HasParent());
	children.push_back(&child); 
	child.parent = this;
}

void GameObject::DetachChild(GameObject& child)
{
	ASSERT(!IsDestroyed());
	// find this in the parent's list of children
	for (list<GameObject*>::iterator it = children.begin(); it != children.end(); ++it) 
	{
		if (*it == &child) 
		{
			child.parent = NULL;
			// update this child's xform since it is no longer childed
			child.SetXFLocal(child.GetXFWorld());
			children.erase(it);
			return;
		}
	}
	// child not found
	ASSERT(false);
}

void GameObject::UpdateTransforms()
{
	// update local transform if necessary
	if (parent && !physicsBody)
	{
		xfWorld = xfLocal * parent->xfWorld;
	}
	else
	{
		if (physicsBody)
			xfLocal = physicsBody->GetTransform();
		xfWorld = xfLocal;
	}

	// update children transforms
	for (list<GameObject*>::iterator it = children.begin(); it != children.end(); ++it) 
	{
		GameObject& obj = **it;
		obj.UpdateTransforms();
	}
}

void GameObject::Render()
{
	ASSERT(IsVisible());

	if (!HasPhysics())
		return;
	
	const b2Transform xf = physicsBody->GetTransform();
	for (const b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
	{
		float a = 1;
		if (f->IsSensor())
		{
			if (DeferredRender::GetRenderPassIsShadow())
				continue;
			a = 0.5f;
		}
		
		g_physicsRender.DrawFixture(xf, *f, Color::Black(a), Color::White(a));
	}
}

Vector2 GameObject::GetGravity() const 
{ 
	return g_gameControlBase->GetGravity( GetPosWorld() );
}
	
// called to render a stub in the editor
// can be overriden by child classes
void GameObject::StubRender(const GameObjectStub& stub, float alpha)
{
	const ObjectTypeInfo& info = stub.GetObjectInfo();
	const Color c = info.GetColor().ScaleAlpha(alpha);
	g_render->RenderQuad(stub.xf, stub.size, c, info.GetTexture());
}

// return size of stub for selection in the editor
// this can be used to shrink stubs to reduce clutter
Vector2 GameObject::StubSelectSize(const GameObjectStub& stub) 
{ 
	return stub.size; 
}

void GameObject::ResetLastWorldTransforms()
{
	if (parent)
	{
		ASSERT(!physicsBody);
		xfWorld = xfLocal * parent->xfWorld;
	}
	else
	{
		if (physicsBody)
			xfLocal = physicsBody->GetTransform();
		xfWorld = xfLocal;
	}

	xfWorldLast = xfWorld;

	// update children transforms
	for (list<GameObject*>::iterator it = children.begin(); it != children.end(); ++it) 
	{
		GameObject& obj = **it;
		obj.ResetLastWorldTransforms();
	}
}

void GameObject::SetMass(float mass)
{
	if (!physicsBody)
		return;

	b2MassData massData;
	physicsBody->GetMassData(&massData);
	massData.mass = mass;
	physicsBody->SetMassData(&massData);
}

void GameObject::SetMassCenter(const Vector2& center)
{
	if (!physicsBody)
		return;

	b2MassData massData;
	physicsBody->GetMassData(&massData);
	massData.center = center;
	physicsBody->SetMassData(&massData);
}

void GameObject::SetRotationalInertia(float I)
{
	if (!physicsBody)
		return;

	b2MassData massData;
	physicsBody->GetMassData(&massData);
	massData.I = I;
	physicsBody->SetMassData(&massData);
}

void GameObject::SetRestitution(float restitution)
{
	if (!HasPhysics())
		return;
	
	for (b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
		f->SetRestitution(restitution);
}

void GameObject::SetFriction(float friction)
{
	if (!HasPhysics())
		return;

	for (b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
		f->SetFriction(friction);
}

bool GameObject::TestPoint(const Vector2& point) const
{
	for (b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
	{
		if (f->TestPoint(point))
			return true;
	}
	return false;
}

void GameObject::SetPhysicsFilter(int16 groupIndex, uint16 categoryBits, uint16 maskBits)
{
	ASSERT(HasPhysics());

	b2Filter filter;
	filter.groupIndex = groupIndex;
	filter.categoryBits = categoryBits;
	filter.maskBits = maskBits;
	for (b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
		f->SetFilterData(filter);
}

int16 GameObject::GetPhysicsGroup() const
{
	if (!HasPhysics())
		return 0;

	// return the group of the first fixture it finds
	for (b2Fixture* f = physicsBody->GetFixtureList(); f; f = f->GetNext())
		return f->GetFilterData().groupIndex;

	return 0;
}