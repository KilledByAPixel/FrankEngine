////////////////////////////////////////////////////////////////////////////////////////
/*
	Basic Objects
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../objects/gameObject.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	SelfTrigger - base class for objects that get triggered when touched by player
*/
////////////////////////////////////////////////////////////////////////////////////////

class SelfTrigger : public GameObject
{
public:

	SelfTrigger(const GameObjectStub& stub);
	void Render() override;
	bool ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const override;
	void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) override;
	static Vector2 StubSelectSize(const GameObjectStub& stub);
	static void StubRender(const GameObjectStub& stub, float alpha);
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Trigger box - calls activate on all object handles in attributes when triggered
*/
////////////////////////////////////////////////////////////////////////////////////////

class TriggerBox : public SelfTrigger
{
public:

	TriggerBox(const GameObjectStub& stub);	
	void Update() override;
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override;

	void CollisionPersist(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) override;
	bool HasTriggerHandle(GameObjectHandle handle) const { return find(triggerHandles.begin(), triggerHandles.end(), handle) != triggerHandles.end(); }

	static WCHAR* StubDescription() { return L"Triggers all objects matching list of handles in the attributes."; }
	static WCHAR* StubAttributesDescription() { return L"triggerData time reverseLogic delay continuous #handles"; }
	static char* StubAttributesDefault() { return "#"; }
	static void StubRender(const GameObjectStub& stub, float alpha);
	static Vector2 StubSelectSize(const GameObjectStub& stub) { return stub.IsThisTypeSelected() ? stub.size : Vector2(0); }

protected:

	list<GameObjectHandle> triggerHandles;
	GameTimer touchTimer;
	GameTimer activateTimer;
	GameTimer delayTimer;
	float minTime = 0;
	float delayTime = 0;
	int triggerData = 0;
	bool reverseLogic = false;
	bool continuous = true;
	bool wantsDeactivate = false;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	MusicGameObject - plays ogg file when triggered
*/
////////////////////////////////////////////////////////////////////////////////////////

class MusicGameObject : public SelfTrigger
{
public:

	MusicGameObject(const GameObjectStub& stub);
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override;

	static WCHAR* StubDescription() { return L"Plays ogg music file when triggered."; }
	static WCHAR* StubAttributesDescription() { return L"filename"; }

private:

	char filename[GameObjectStub::attributesLength] = "";
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	SoundGameObject - plays a sound when triggered
*/
////////////////////////////////////////////////////////////////////////////////////////

class SoundGameObject : public SelfTrigger
{
public:

	SoundGameObject(const GameObjectStub& stub);
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override;
	void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) override;

	static WCHAR* StubDescription() { return L"Plays sound when triggered."; }
	static WCHAR* StubAttributesDescription() { return L"soundID volume frequency selfTrigger onActivate onDeactivate"; }

private:

	SoundID sound = SoundID(0);
	float volume = 1;
	float frequency = 1;
	bool selfTrigger = true;
	bool onActivate = true;
	bool onDeactivate = false;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	TextDecal - displays text in world space
*/
////////////////////////////////////////////////////////////////////////////////////////

class TextDecal : public GameObject
{
public:

	TextDecal(const GameObjectStub& stub);

	void Render() override;
	void RenderPost() override;
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override { SetVisible(activate); }
	
	static WCHAR* StubDescription() { return L"Draws a text decal"; }
	static WCHAR* StubAttributesDescription() { return L"r g b a renderGroup emissive normals visible map font #text"; }
	static char* StubAttributesDefault() { return "1 1 1 1 #"; }
	static void StubRender(const GameObjectStub& stub, float alpha);
	static void StubRenderMap(const GameObjectStub& stub, float alpha);

	static int DefaultRenderGroup;

private:

	struct Attributes
	{
		Attributes(const GameObjectStub& stub);
		
		Color color = Color::White();
		int renderGroup = 0;
		float emissive = 0;
		int fontIndex = 0;
		bool visible = true;
		bool map = false;
		bool normals = false;
		char text[GameObjectStub::attributesLength];
	};

	Attributes attributes;
	Vector2 size = Vector2(1);
};

////////////////////////////////////////////////////////////////////////////////////////
/*
TileImageDecal - displays tile from an image in world space
*/
////////////////////////////////////////////////////////////////////////////////////////

class TileImageDecal : public GameObject
{
public:

	TileImageDecal(const GameObjectStub& stub);

	void Render() override;
	void RenderPost() override;
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override { SetVisible(activate); }
	bool IsPathable(bool canFly) const override { return !HasPhysics(); }

	static WCHAR* StubDescription() { return L"Draws an image from a tile sheet"; }
	static WCHAR* StubAttributesDescription() { return L"x y w h r g b a mirror renderGroup shadows emissive transparent visible collision map #texture"; }
	static char* StubAttributesDefault() { return "0 0 16 16 1 1 1 1 #"; }
	static void StubRender(const GameObjectStub& stub, float alpha) ;
	static void StubRenderMap(const GameObjectStub& stub, float alpha);

	static int DefaultRenderGroup;

private:

	struct Attributes
	{
		Attributes(const GameObjectStub& stub);
		
		ByteVector2 tilePos = ByteVector2(0);
		ByteVector2 tileSize = ByteVector2(16);
		TextureID texture = Texture_Invalid;
		Color color = Color::White();
		int renderGroup = 0;
		bool castShadows = false;
		float emissive = 0;
		bool transparent = false;
		bool visible = true;
		bool map = false;
		bool collision = false;
		bool mirror = false;
	};

	Attributes attributes;
	Vector2 size = Vector2(1);
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	ObjectSpawner - complex object that can spawn other objects via it's attributes
*/
////////////////////////////////////////////////////////////////////////////////////////

class ObjectSpawner : public GameObject
{
public:

	ObjectSpawner(const GameObjectStub& stub);

	void Update() override;
	virtual GameObject* SpawnObject();
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override { isActive = activate; }
	
	static WCHAR* StubDescription() { return L"Spawns objects of a given type"; }
	static WCHAR* StubAttributesDescription() { return L"type speed max randomness rate isActive #attributes"; }
	static char* StubAttributesDefault() { return "0 0 1 0 1 1"; }
	static void StubRender(const GameObjectStub& stub, float alpha);

private:

	GameTimerPercent spawnTimer;
	list<GameObjectSmartPointer<>> spawnedObjects;
	UINT spawnMax = 1;
	Vector2 spawnSize = Vector2(1);
	GameObjectType spawnType = GameObjectType(0);
	float spawnSpeed = 0;
	float spawnRandomness = 0;
	float spawnRate = 0.2f;
	bool isActive = false;
	char attributes[GameObjectStub::attributesLength] = "";
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	ShootTrigger - activates objects when shot by player
*/
////////////////////////////////////////////////////////////////////////////////////////

class ShootTrigger : public GameObject
{
public:

	ShootTrigger(const GameObjectStub& stub);

	void Update() override;
	void Render() override;
	void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) override;
	void Activate(bool activate);
	bool IsActive() { return activateTimer.IsSet(); }
	
	static WCHAR* StubDescription() { return L"activates objects when shot by player"; }
	static WCHAR* StubAttributesDescription() { return L"triggerData time reverseLogic isToggle #handles"; }
	static char* StubAttributesDefault() { return "#"; }
	static void StubRender(const GameObjectStub& stub, float alpha);

private:
	
	list<GameObjectHandle> triggerHandles;
	GameTimer activateTimer;
	float deactivateTime = 0;
	int triggerData = 0;
	Vector2 size = Vector2(1);
	bool reverseLogic = false;
	bool isToggle = false;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	MiniMapGameObject - draws the minimap to a place in the world
*/
////////////////////////////////////////////////////////////////////////////////////////

class MiniMapGameObject : public GameObject
{
public:

	MiniMapGameObject(const GameObjectStub& stub);

	void Render() override;
	
	static WCHAR* StubDescription() { return L"Draw world space map"; }
	static WCHAR* StubAttributesDescription() { return L"X Y angle zoom r g b a renderGroup showFull"; }
	static char* StubAttributesDefault() { return "0 0 0 100 1 1 1 1"; }
	static void StubRender(const GameObjectStub& stub, float alpha);

	XForm2 xfCenter = XForm2::Identity();
	float zoom = 100;
	bool showFull = true;
	Color color = Color::White();
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	ConsoleCommandObject - Executes console command when triggered
*/
////////////////////////////////////////////////////////////////////////////////////////

class ConsoleCommandObject : public GameObject
{
public:

	ConsoleCommandObject(const GameObjectStub& stub);
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override;
	
	static WCHAR* StubDescription() { return L"Executes console command when triggered"; }
	static WCHAR* StubAttributesDescription() { return L"consoleCommand"; }

private:

	char text[GameObjectStub::attributesLength];
};
