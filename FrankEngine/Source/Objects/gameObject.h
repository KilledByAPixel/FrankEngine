////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object
	Copyright 2013 Frank Force - http://www.frankforce.com

	- 2d object in the x/y plain
	- automatically added to world group by default when constructed
	- automatically deleted during garbage collection phase after destroy is called
	- may have a physics object
	- may have a parents or multiple children
	- has a unique handle
	- can't be copied
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../objects/gameObjectManager.h"
#include "../physics/physics.h"

#define gameobject_cast(OBJECT, TYPE) (((OBJECT)->GetObjectType() == GOT_##TYPE)? static_cast<TYPE*>(OBJECT) : NULL)

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object Smart Pointer
*/
////////////////////////////////////////////////////////////////////////////////////////

template <class T = GameObject>
struct GameObjectSmartPointer
{
	GameObjectSmartPointer() : handle(0)			{}
	GameObjectSmartPointer(const T* object) : 
		handle(object?object->GetHandle() : 0)		{}

	operator T* () const							{ return GetPointer(); }
	T* GetPointer() const							{ return static_cast<T*>(g_objectManager.GetObjectFromHandle(handle)); }
	bool operator == (const T& o) const				{ return (handle == o.GetHandle()); }
	bool operator != (const T& o) const				{ return (handle != o.GetHandle()); }
	GameObjectHandle GetHandle() const				{ return handle; }
	void SetHandle(GameObjectHandle h)				{ handle = h; }

private:

	GameObjectHandle handle = 0;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object
*/
////////////////////////////////////////////////////////////////////////////////////////

enum GameObjectType;

class GameObject : private Uncopyable
{
public: // basic functionality

	explicit GameObject(const struct GameObjectStub& stub, GameObject* _parent = NULL, bool addToWorld = true);
	explicit GameObject(const XForm2& xf, GameObject* _parent = NULL, GameObjectType _gameObjectType = GameObjectType(0), bool addToWorld = true);
	virtual ~GameObject() = 0;

	// every object has a unique handle, use object handles instead of pointers to keep track of objects
	GameObjectHandle GetHandle() const { return handle; }
	bool operator == (const GameObject& other) const								{ return (handle == other.GetHandle()); }
	bool operator != (const GameObject& other) const								{ return (handle != other.GetHandle()); }
	template <class T> bool operator == (const GameObjectSmartPointer<T> o) const	{ return (handle == o.GetHandle()); }
	template <class T> bool operator != (const GameObjectSmartPointer<T> o) const	{ return (handle != o.GetHandle()); }

	// call this function mark an object for destruction instead of calling delete
	// all of the object's children will also be removed and marked to be destroyed
	virtual void Destroy();
	bool IsDestroyed() const { return GetFlag(ObjectFlag_Destroyed); } 
	
	// warning: this should probably never be called
	virtual void UnDestroy() { SetFlag(ObjectFlag_Destroyed, false); }

	// detach and kill particle emitters
	void DetachEmitters();

	// Kill() is how objects die during normal gameplay
	// override this to make special things happen when an object dies
	virtual void Kill() { DetachEmitters(); Destroy(); }
	virtual void ApplyDamage(float damage, GameObject* attacker = NULL, GameDamageType damageType = GameDamageType_Default) {}
	virtual bool CanBeDamagedBy(GameDamageType damageType) const { return true; }
	
public: // sound
	
	static float defaultFrequencyRandomness;
	static float defaultSoundDistanceMin;
	static float defaultSoundDistanceMax;

	virtual struct SoundObjectSmartPointer MakeSound(enum SoundID si, float volume = 1, float frequency = 1, float frequencyRandomness = defaultFrequencyRandomness, float distanceMin = defaultSoundDistanceMin, float distanceMax = defaultSoundDistanceMax);

public: // game object information

	GameObjectType GetObjectType() const { return gameObjectType; }
	void SetObjectType(GameObjectType type) { gameObjectType = type; }
	const struct ObjectTypeInfo* GetObjectInfo() const;

	Vector2 GetStubSize() const { return stubSize; }
	void SetStubSize(const Vector2& _stubSize) { stubSize = _stubSize; }

	XForm2 GetStubXF() const { return stubXF; }
	void SetStubXF(const XForm2& _stubXF) { stubXF = _stubXF; }

	const GameTeam GetTeam() const { return team; }
	void SetTeam(GameTeam _team) { team = _team; }
	
public: // generic gameplay functions
	
	// dynamic type info (should only be used for objects that can be cast to these types)
	virtual bool IsPlayer() const { return false; }
	virtual bool IsOwnedByPlayer() const { return false; }
	virtual bool IsActor() const { return false; }
	virtual bool IsCharacter() const { return false; }
	virtual bool IsParticleEmitter() const { return false; }
	virtual bool IsProjectile() const { return false; }
	virtual bool IsTerrain() const { return false; }
	virtual bool IsLight() const { return false; }
	virtual bool Bounce(const Vector2& direction) { return false; }
	virtual bool IsFlying() const { return false; }
	virtual bool CanBeTargetedBy(const GameObject& otherObject, GameDamageType damageType = GameDamageType_Default) { return false; }
	virtual GameObject* GetOwner() { return NULL; }

	// generic gameside functions
	virtual bool IsDead() const { return false; }
	virtual void SetIsStunned() {}
	virtual bool CanPickUp(GameObject* pickerUpper = NULL) const { return false; }
	virtual void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) {}
	virtual bool IsPathable(bool canFly) const { return true; }
	
	// called for each object when terrain is deformed 
	virtual void TerrainDeform(const Vector2& pos, float radius) {}

	// get position that should be targeted if something aimed at this
	virtual const Vector2 GetPosTarget() const { return GetPosWorld(); }
	
	// can be used to prevent objects from rapidly playing sounds
	void SetSoundTimer() { soundTimer.Set(); }
	bool HasPlayedSound(float time) const { return soundTimer.IsSet() && soundTimer < time; }

public: // transforms

	const XForm2& GetXFLocal() const { return xfLocal; }
	const XForm2& GetXFWorld() const { return xfWorld; }
	const XForm2& GetXFWorldLast() const { return xfWorldLast; }

	void SetPosLocal(const Vector2& pos) { xfLocal.position = pos; }
	void SetAngleLocal(float angle) { xfLocal.angle = angle; }
	void SetXFLocal(const XForm2& xf) { xfLocal = xf; }

	void SetXFWorld(const XForm2& xf)
	{
		ASSERT(xf.IsFinite());
		if (HasPhysics())
			SetXFPhysics(xf);
		if (HasParent())
		{
			xfWorld = xf; 
			xfLocal = xfWorld * parent->xfWorld.Inverse();
			return;
		}

		xfWorld = xf; 
		SetXFLocal(xf);
	}
	void SetPosWorld(const Vector2& pos)
	{
		ASSERT(pos.IsFinite());
		if (HasPhysics())
			SetPosPhysics(pos);
		if (HasParent())
		{
			xfWorld.position = pos; 
			xfLocal.position = (xfWorld * parent->xfWorld.Inverse()).position;
			return;
		}

		xfWorld.position = pos; 
		SetPosLocal(pos);
	}
	void SetAngleWorld(float angle)
	{
		ASSERT(isfinite(angle));
		if (HasPhysics())
			SetAnglePhysics(angle);
		if (HasParent())
		{
			xfWorld.angle = angle; 
			xfLocal.angle = (xfWorld * parent->xfWorld.Inverse()).angle;
			return;
		}

		xfWorld.angle = angle; 
		SetAngleLocal(angle);
	}

	// shortcuts to quickly get at parts of the xform
	const Vector2&	GetPosLocal()	const { return xfLocal.position; }
	const Vector2&	GetPosWorld()	const { return xfWorld.position; }
	const float		GetAngleLocal()	const { return xfLocal.angle; }
	const float		GetAngleWorld()	const { return xfWorld.angle; }
	const Vector2	GetRightLocal()	const { return xfLocal.GetRight(); }
	const Vector2	GetRightWorld()	const { return xfWorld.GetRight(); }
	const Vector2	GetUpLocal()	const { return xfLocal.GetUp(); }
	const Vector2	GetUpWorld()	const { return xfWorld.GetUp(); }
	
	// resurcive update of object and child transform
	virtual void UpdateTransforms();

	// call to wipe out interpolation data for this object and it's children
	// so an object can instantly transport to a new position rather then interpolating
	void ResetLastWorldTransforms();

	// get the delta xform used for interpolation
	XForm2 GetXFDelta() const { return xfWorld - xfWorldLast; }

protected: // lower level object functions

	// automatically called for all object during update phase
	virtual void Update() {}

	// automatically called for every visible object during render phase
	virtual void Render();

	// automatically called for every visible object during render post phase (after main render is complete)
	virtual void RenderPost() {}

public: // children and parents

	void AttachChild(GameObject& child);
	void DetachChild(GameObject& child);
	bool HasChildren() const { return !(children.empty()); }
	void DetachParent() { if (parent) parent->DetachChild(*this); }
	void DetachChildren();
	void DestroyChildren();
	virtual void WasDetachedFromParent() {}

	bool HasParent() const { return parent != NULL; }
	GameObject* GetParent() { return parent; }
	const GameObject* GetParent() const { return parent; }

	GameObject* GetFirstChild() { return children.front(); }
	const GameObject* GetFirstChild() const { return children.front(); }

	list<GameObject*>& GetChildren() { return children; }
	const list<GameObject*>& GetChildren() const { return children; }

public: // interpolation (used by rendering)

	// use this transform when rendering to get the interpolated xform
	XForm2 GetXFInterpolated() const;
	XForm2 GetXFInterpolated(float percent) const;
	Matrix44 GetMatrixInterpolated() const;

public: // rendering

	void SetVisible(bool visible) { SetFlag(ObjectFlag_Visible, visible); }
	bool IsVisible() const { return GetFlag(ObjectFlag_Visible); }

	// order of rendering for objects within a given render pass
	// lower numbers draw earlier
	int GetRenderGroup() const { return renderGroup; }
	void SetRenderGroup(int _renderGroup) { renderGroup = _renderGroup; }
	
	// called to render a stub in the editor
	static void StubRender(const GameObjectStub& stub, float alpha);
	
	// called to render a stub on the map (by default stubs don't appear on the map)
	static void StubRenderMap(const GameObjectStub& stub, float alpha) {}

	// called to show a description of the object in editor
	static WCHAR* StubDescription() { return L"Unknown object type."; }
	
	// called to show a description of the object in editor
	static WCHAR* StubAttributesDescription() { return L"N/A"; }

	// fill in default attributes when stub is created
	static char* StubAttributesDefault() { return ""; }

	// return size of stub for selection in the editor
	static Vector2 StubSelectSize(const GameObjectStub& stub);

public: // physics

	bool HasPhysics() const					{ return physicsBody != NULL; }
	b2Body* GetPhysicsBody()				{ return physicsBody; }
	const b2Body* GetPhysicsBody() const	{ return physicsBody; }
	
	// set box2d collision filter
	// Set group of objects to never collide (negative) or always collide (positive). Zero means no collision group.
	void SetPhysicsFilter(int16 groupIndex = 0, uint16 categoryBits = 0x0001, uint16 maskBits = 0xFFFF);
	int16 GetPhysicsGroup() const;

	// check if objects should collide. if a fixture is null that indicates it is a raycast collide test
	virtual bool ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const { return true; }

	// special check for line of sight collision
	virtual bool ShouldCollideSight() const { return true; }

	// check if this collides with physical particle systems
	virtual bool ShouldCollideParticles() const { return IsStatic(); }

	// collision callbacks (these are batched up and called after physics)
	// getting a null fixture for the other object indicates that it is a raycast collision
	virtual void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) {}
	virtual void CollisionPersist(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) {}
	virtual void CollisionRemove(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) {}
	virtual void CollisionResult(GameObject& otherObject, const ContactResult& contactResult, b2Fixture* myFixture, b2Fixture* otherFixture) {}

	// collision pre-solve callback, called immediatly during physics update
	virtual void CollisionPreSolve(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture, const b2Manifold* oldManifold) {}

	// create/destroy functions for physics
	void CreatePhysicsBody(b2BodyType type = b2_dynamicBody, bool fixedRotation = false, bool allowSleeping = true);
	void CreatePhysicsBody(const b2BodyDef& bodyDef);
	void DestroyPhysicsBody();
	b2Fixture* AddFixture(const b2FixtureDef& fixtureDef);
	b2Fixture* AddFixture(const b2Shape& shape, float density = Physics::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution, void* userData = NULL);
	b2Fixture* AddSensor(const b2Shape& shape, void* userData = NULL);
	b2Fixture* AddFixtureBox(const Vector2& size, float density = Physics::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution);
	b2Fixture* AddFixtureBox(const Vector2& size, const XForm2& offset, float density = Physics::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution);
	b2Fixture* AddFixtureCircle(const float radius, float density = Physics::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution);
	b2Fixture* AddFixturePolygon(const Vector2* points, int count, float density = Physics::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution);
	b2Fixture* AddFixtureEdge(const Vector2& p1, const Vector2& p2, float density = Physics::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution);
	b2Fixture* AddFixtureEdge(const Vector2& p1, const Vector2& p2, const Vector2& p0, const Vector2& p3,float density = Physics::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution);
	b2Fixture* AddSensorBox(const Vector2& size);
	b2Fixture* AddSensorBox(const Vector2& size, const XForm2& offset);
	b2Fixture* AddSensorCircle(const float radius);
	b2Fixture* AddSensorPolygon(const Vector2* points, int count);
	void RemoveFixture(b2Fixture& fixture);
	void RemoveAllFixtures();
	static GameObject* GetFromPhysicsBody(b2Body& body) { return static_cast<GameObject*>(body.GetUserData()); }

	// get info about the attached physics body
	virtual Vector2 GetVelocity() const;
	virtual void SetVelocity(const Vector2& v);
	virtual void SetAngularVelocity(const float v);
	virtual float GetSpeed() const				{ return GetVelocity().Length(); }
	virtual float GetSpeedSquared() const		{ return GetVelocity().LengthSquared(); }
	virtual float GetAngularVelocity() const;
	virtual float GetMass() const;
	virtual float GetKineticEnergy() const;
	virtual XForm2 GetXFPhysics() const;
	virtual bool IsStatic() const;
	virtual void SetXFPhysics(const Vector2& position, float angle = 0) { SetXFPhysics(XForm2(position, angle)); }
	virtual void SetXFPhysics(const XForm2& xf);
	virtual void SetPosPhysics(const Vector2& pos);
	virtual void SetAnglePhysics(float angle);
	virtual void SetLinearDamping(float damp)	{ if (GetPhysicsBody()) GetPhysicsBody()->SetLinearDamping(damp); }
	virtual void SetAngularDamping(float damp)	{ if (GetPhysicsBody()) GetPhysicsBody()->SetAngularDamping(damp); }
	virtual void SetMass(float mass);
	virtual void SetMassCenter(const Vector2& center);
	virtual void SetRotationalInertia(float I);
	virtual void SetRestitution(float restitution);
	virtual void SetFriction(float friction);
	virtual bool TestPoint(const Vector2& point) const;

	// apply forces to the attached physics body
	virtual void ApplyTorque(float torque);
	virtual void ApplyAngularAcceleration(float angularAcceleration);
	virtual void ApplyForce(const Vector2& force, const Vector2& pos);
	virtual void ApplyForce(const Vector2& force);
	virtual void ApplyImpulse(const Vector2& impulse, const Vector2& pos);
	virtual void ApplyImpulse(const Vector2& impulse);
	virtual void ApplyAcceleration(const Vector2& a, const Vector2& pos);
	virtual void ApplyAcceleration(const Vector2& a);
	virtual void ApplyRotation(float angle);
	virtual void CapSpeed(float speed);
	virtual void CapAngularSpeed(float speed);

	// stuff that has to do with gravity
	bool HasGravity() const {return GetFlag(ObjectFlag_Gravity);}
	void SetHasGravity(bool hasGravity);
	virtual Vector2 GetGravity() const;
	Vector2 GetGravityUpDirection() const { return -GetGravity().Normalize(); }
	void SetGravityScale(float scale) { ASSERT(HasPhysics()) GetPhysicsBody()->SetGravityScale(scale); }
	float GetGravityScale() const { ASSERT(HasPhysics()) return GetPhysicsBody()->GetGravityScale(); }

	// move up heiarachy until we find a physical object if there is one
	const GameObject* GetAttachedPhysics() const;
	GameObject* GetAttachedPhysics();
	
	void SetIgnoreExplosions(bool ignoreExplosions) { SetFlag(ObjectFlag_IgnoreExplosions, ignoreExplosions); }
	bool GetIgnoreExplosions() const { return (flags & ObjectFlag_IgnoreExplosions) != 0; }

public: // streaming

	// check if object should be streamed out
	virtual bool ShouldStreamOut() const;

	// automatically called when an object gets streamed out
	virtual void StreamOut() { Destroy(); }

	// automatically called if an object still exists when trying to stream in
	virtual void StreamIn() {}

	// true if the object should do seralized setreaming
	static bool IsSerializable() { return false; }

	// saves this object out to a stub for seralized streaming
	virtual GameObjectStub Serialize() const;

	// get combined aabb for all the fixtures in this physics body
	Box2AABB GetPhysicsAABB(bool includeSensors = false) const;

	// is this object (and it's children) fully contained inside the aabb?
	bool FullyContainedBy(const Box2AABB& bbox) const;

	// is this object (or it's children) partially contained inside the aabb?
	bool PartiallyContainedBy(const Box2AABB& bbox) const;
	
	// was the object added this frame?
	bool WasJustAdded() const { return GetFlag(ObjectFlag_JustAdded); }
	
public: // allocation
	
	void* operator new (size_t size)	{ return g_objectManager.AllocateBlock(size); }
	void operator delete (void* block)	{ g_objectManager.FreeBlock(block); }

private: // private stuff

	GameObjectHandle handle;			// the unique handle of this object
	GameObjectType gameObjectType;		// the type of game object if it has one
	b2Body* physicsBody;				// the physics body if it has one
	int renderGroup;					// order objects are rendered in with their render pass
	Vector2 stubSize;					// size from the stub, used for streaming
	XForm2 stubXF;						// xf when spawned
	GameTimer soundTimer;				// can be used to limit how often objects play sounds

	XForm2 xfLocal;						// transform in local space
	XForm2 xfWorld;						// transform from local to world space (only updated once per frame)
	XForm2 xfWorldLast;					// world space transform from last frame, used for interpolation

	list<GameObject*> children;			// list of children	
	GameObject* parent;					// parent if it has one
	GameTeam team;						// team object is on

	enum ObjectFlags
	{
		ObjectFlag_Destroyed			= 0x01,		// is marked for deletion
		ObjectFlag_Visible				= 0x02,		// should be rendered
		ObjectFlag_JustAdded			= 0x04,		// was just created this frame
		ObjectFlag_Gravity				= 0x08,		// should gravity be applied
		ObjectFlag_IgnoreExplosions		= 0x10,		// should not be effected by explosions
	};
	UINT flags;								// bit field of flags for the object
	
	void SetFlag(ObjectFlags flag, bool enable);
	bool GetFlag(ObjectFlags flag) const { return (flags & flag) != 0; }

	static GameObjectHandle nextUniqueHandleValue;	// used only internaly to give out unique handles

	// should object be destroyed when world is reset? 
	// only a few special objects like the camera and terrain will need to override this
	virtual bool DestroyOnWorldReset() const { return true; }

public:	// public data members

	static const GameObjectHandle invalidHandle;	// represents an invalid object handle

private: // special functions to get and reset the next unique handle

	// these friends are really only necessary so they can call to the special get and set handle functions
	friend class ObjectEditor;
	friend class Terrain;
	friend class GameObjectManager;

	// these functions should only be used by editors and startup code
	static GameObjectHandle GetNextUniqueHandleValue() { return nextUniqueHandleValue; }
	static void SetNextUniqueHandleValue(GameObjectHandle _nextUniqueHandleValue) { nextUniqueHandleValue = _nextUniqueHandleValue; }
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Inline functions
*/
////////////////////////////////////////////////////////////////////////////////////////

inline void GameObject::DetachChildren()
{
	for (list<GameObject*>::iterator it = children.begin(); it != children.end(); ++it)
	{
		(**it).WasDetachedFromParent();
		(**it).parent = NULL;
	}
	children.clear();
}

inline void GameObject::DestroyChildren()
{
	for (list<GameObject*>::iterator it = children.begin(); it != children.end(); ++it)
		(**it).Destroy();
}

inline XForm2 GameObject::GetXFInterpolated(float percent) const	{ return xfWorld.Interpolate(GetXFDelta(), percent); }
inline XForm2 GameObject::GetXFInterpolated() const					{ return GetXFInterpolated(g_interpolatePercent); }
inline Matrix44 GameObject::GetMatrixInterpolated() const			{ return Matrix44(GetXFInterpolated()); }
inline bool GameObject::IsStatic() const							{ return (!physicsBody || physicsBody->GetType() == b2_staticBody); }
inline Vector2 GameObject::GetVelocity() const						{ return physicsBody? physicsBody->GetLinearVelocity() : Vector2::Zero(); }

inline void GameObject::SetVelocity(const Vector2& v)
{ 
	ASSERT(v.IsFinite());
	if (physicsBody)
		physicsBody->SetLinearVelocity(v); 
}

inline void GameObject::SetAngularVelocity(const float v)
{ 
	ASSERT(isfinite(v));
	if (physicsBody)
		physicsBody->SetAngularVelocity(v); 
}

inline float GameObject::GetAngularVelocity() const { return physicsBody? physicsBody->GetAngularVelocity() : 0; }
inline float GameObject::GetMass() const { return physicsBody? physicsBody->GetMass() : 0; }
inline float GameObject::GetKineticEnergy() const { return physicsBody? physicsBody->GetMass() * physicsBody->GetLinearVelocity().LengthSquared() : 0; }

inline void GameObject::ApplyTorque(float torque)
{ 
	ASSERT(isfinite(torque));
	if (physicsBody)
		physicsBody->ApplyTorque(torque, true); 
}

inline void GameObject::ApplyAngularAcceleration(float angularAcceleration)
{ 
	ApplyTorque(angularAcceleration * physicsBody->GetInertia());
}

inline void GameObject::ApplyImpulse(const Vector2& impulse, const Vector2& pos)
{ 
	ASSERT(impulse.IsFinite() && pos.IsFinite());
	if (physicsBody)
		physicsBody->ApplyLinearImpulse(impulse, pos, true); 
}

inline void GameObject::ApplyImpulse(const Vector2& impulse)
{ 
	ApplyImpulse(impulse, physicsBody->GetWorldCenter());
}

inline void GameObject::ApplyForce(const Vector2& force, const Vector2& pos)
{ 
	ASSERT(force.IsFinite() && pos.IsFinite());
	if (physicsBody)
		physicsBody->ApplyForce(force, pos, true); 
}

inline void GameObject::ApplyForce(const Vector2& force)
{ 
	ApplyForce(force, physicsBody->GetWorldCenter());
}

inline void GameObject::ApplyAcceleration(const Vector2& accel, const Vector2& pos)
{ 
	ApplyForce(accel*physicsBody->GetMass(), pos);
}

inline void GameObject::ApplyAcceleration(const Vector2& accel)
{ 
	ApplyAcceleration(accel, physicsBody->GetWorldCenter());
}

inline void GameObject::ApplyRotation(float angle)
{ 
	ASSERT(isfinite(angle));
	if (physicsBody)
	{
		const XForm2 xf = GetXFPhysics();
		SetXFPhysics(xf.position, xf.angle + angle); 
	}
	else
		xfLocal.angle += angle;
}

inline void GameObject::SetXFPhysics(const XForm2& xf)
{
	ASSERT(physicsBody && xf.IsFinite());
	physicsBody->SetTransform(xf.position, xf.angle);
}

inline void GameObject::SetPosPhysics(const Vector2& pos)
{
	ASSERT(physicsBody && pos.IsFinite());
	physicsBody->SetTransform(pos, GetXFPhysics().angle);
}

inline void GameObject::SetAnglePhysics(float angle)
{
	ASSERT(physicsBody && isfinite(angle));
	physicsBody->SetTransform(physicsBody->GetTransform().p, angle);
}

inline XForm2 GameObject::GetXFPhysics() const
{
	return physicsBody? physicsBody->GetTransform() : xfWorld;
}

inline void GameObject::CapSpeed(float maxSpeed)
{
	ASSERT(isfinite(maxSpeed));
	if (!physicsBody)
		return;

	const Vector2 velocity = physicsBody->GetLinearVelocity();
	const float currentSpeed = velocity.Length();
	if (currentSpeed > maxSpeed)
		physicsBody->SetLinearVelocity(velocity * (maxSpeed/currentSpeed));
}

inline void GameObject::CapAngularSpeed(float maxSpeed)
{
	ASSERT(isfinite(maxSpeed));
	if (!physicsBody)
		return;

	const float currentSpeed = physicsBody->GetAngularVelocity();
	if (fabs(currentSpeed) > maxSpeed)
		physicsBody->SetAngularVelocity(maxSpeed * (currentSpeed > 0? 1 : -1));
}

inline void GameObject::CreatePhysicsBody(const b2BodyDef& bodyDef)
{
	ASSERT(!physicsBody);
	ASSERT(!HasParent());
	ASSERT(bodyDef.userData); // must be connected to an object

	physicsBody = g_physics->CreatePhysicsBody(bodyDef);

	// update xform so it appears in the correct spot
	xfLocal = GetPhysicsBody()->GetTransform();
	xfWorld = xfLocal;
}

inline void GameObject::DestroyPhysicsBody()
{
	if (physicsBody)
	{
		b2Body* oldPhysicsBody = physicsBody;
		physicsBody = NULL;
		g_physics->DestroyPhysicsBody(oldPhysicsBody);
	}
}

inline GameObject* GameObject::GetAttachedPhysics() { return const_cast<GameObject*>(static_cast<const GameObject*>(this)->GetAttachedPhysics()); }
inline const GameObject* GameObject::GetAttachedPhysics() const
{
	// search for the physical object in the heiarachy
	const GameObject* physicsObject = this;
	while (physicsObject && !physicsObject->HasPhysics())
	{ physicsObject = physicsObject->GetParent(); }
	return physicsObject;
}

inline void GameObject::SetHasGravity(bool hasGravity) 
{ 
	if (physicsBody)
		SetGravityScale(hasGravity ? 1.0f : 0.0f);
	SetFlag(ObjectFlag_Gravity, hasGravity);
}

inline void GameObject::SetFlag(ObjectFlags flag, bool enable) 
{ 
	if (enable)
		flags |= flag;
	else
		flags &= ~flag;
}