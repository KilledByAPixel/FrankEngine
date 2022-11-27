////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object Builder
	Copyright 2013 Frank Force - http://www.frankforce.com

	- use this instead of new to build game objects
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../objects/gameObject.h"

enum GameObjectType;
const GameObjectType GAME_OBJECT_TYPE_MAX_COUNT = (GameObjectType)256;
const GameObjectType GAME_OBJECT_TYPE_INVALID = (GameObjectType)0;

struct ObjectTypeInfo;
struct GameObjectStub;
extern ObjectTypeInfo* g_gameObjectInfoArray[GAME_OBJECT_TYPE_MAX_COUNT];

// holds information about an object type / class
// constructor automatically adds this to the global array
struct ObjectTypeInfo
{
public:
	
	typedef GameObject* (*BuildObjectFunction)(const GameObjectStub& stub);
	typedef bool (*IsSerializableFunction)();
	typedef void (*StubRenderFunction)(const GameObjectStub& stub, float alpha);
	typedef WCHAR* (*StubDescriptionFunction)();
	typedef char* (*StubAttributesFunction)();
	typedef Vector2 (*StubSelectSizeFunction)(const GameObjectStub& stub);
	
	ObjectTypeInfo
	(
		const WCHAR* _name, 
		GameObjectType _type, 
		BuildObjectFunction _buildObjectFunction, 
		IsSerializableFunction _isSerializableFunction,
		StubRenderFunction _stubRenderFunction, 
		StubRenderFunction _stubRenderMapFunction, 
		StubDescriptionFunction _stubAttributesDescriptionFunction, 
		StubDescriptionFunction _stubDescriptionFunction,
		StubAttributesFunction _stubAttributesDefaultFunction,
		StubSelectSizeFunction _stubSelectSizeFunction,
		TextureID _ti = TextureID(0), 
		const Color& _stubColor = Color::White()
	) :
		name(_name),
		type(_type),
		ti(_ti),
		stubColor(_stubColor),
		buildObjectFunction(_buildObjectFunction),
		isSerializableFunction(_isSerializableFunction),
		stubRenderFunction(_stubRenderFunction),
		stubRenderMapFunction(_stubRenderMapFunction),
		stubAttributesDescriptionFunction(_stubAttributesDescriptionFunction),
		stubDescriptionFunction(_stubDescriptionFunction),
		stubAttributesDefaultFunction(_stubAttributesDefaultFunction),
		stubSelectSizeFunction(_stubSelectSizeFunction)
	{
		// init my entry in the global object info index array
		ASSERT(!g_gameObjectInfoArray[type]);
		g_gameObjectInfoArray[type] = this;

		if (type > maxType)
			maxType = type;
	}

	const WCHAR* GetName() const											{ return name; }
	GameObjectType GetType() const											{ return type; }
	TextureID GetTexture() const										{ return ti; }
	const Color& GetColor() const											{ return stubColor; }
	GameObject* BuildObject(const GameObjectStub& stub) const				{ return buildObjectFunction(stub); }
	void StubRender(const GameObjectStub& stub, float alpha = 1) const		{ stubRenderFunction(stub, alpha); }
	void StubRenderMap(const GameObjectStub& stub, float alpha = 1) const	{ stubRenderMapFunction(stub, alpha); }
	const WCHAR* GetAttributesDescription() const							{ return stubAttributesDescriptionFunction(); }
	const WCHAR* GetDescription() const										{ return stubDescriptionFunction(); }
	const char* GetAttributesDefault() const								{ return stubAttributesDefaultFunction(); }
	bool IsSerializable() const												{ return isSerializableFunction(); }
	Vector2 GetStubSelectSize(const GameObjectStub& stub) const				{ return stubSelectSizeFunction(stub); }

	void SetTexture(TextureID _ti)		{ ti = _ti; }
	void SetColor(const Color& _stubColor)	{ stubColor = _stubColor; }

	// get maximum registered object type, used by editors
	static GameObjectType GetMaxType() { return maxType; }

	static const ObjectTypeInfo invalidTypeInfo;

private:

	const WCHAR* name;
	GameObjectType type;
	TextureID ti;
	Color stubColor;
	BuildObjectFunction buildObjectFunction;
	IsSerializableFunction isSerializableFunction;
	StubRenderFunction stubRenderFunction;
	StubRenderFunction stubRenderMapFunction;
	StubDescriptionFunction stubAttributesDescriptionFunction;
	StubDescriptionFunction stubDescriptionFunction;
	StubAttributesFunction stubAttributesDefaultFunction;
	StubSelectSizeFunction stubSelectSizeFunction;
	static GameObjectType maxType;
};

// holds information about how to create a specific object
struct GameObjectStub
{
public:
	GameObjectStub
	(
		const XForm2& _xf = XForm2::Identity(),
		const Vector2& _size = Vector2(1),
		GameObjectType _type = GAME_OBJECT_TYPE_INVALID,
		const char* _attributes = NULL,
		GameObjectHandle _handle = GameObject::invalidHandle
	);

	const ObjectTypeInfo& GetObjectInfo() const	{ return GetObjectInfo(type); }
	GameObject* BuildObject() const				{ return GetObjectInfo().BuildObject(*this); }
	bool HasObjectInfo() const					{ return g_gameObjectInfoArray[type] != NULL; }
	Vector2 GetStubSelectSize() const			{ return GetObjectInfo().GetStubSelectSize(*this); }
	Box2AABB GetAABB() const					{ return Box2AABB(xf, size); }
	bool IsThisTypeSelected() const;
	void Render(float alpha = 1) const			{ if (HasObjectInfo()) GetObjectInfo().StubRender(*this, alpha); }

	// test if a point is inside the box defined by the stub
	bool TestPosition(const Vector2& pos) const { return Vector2::InsideBox(xf, size, pos); }

	// quick way to get a single attribute
	int GetAttributesInt() const { int attributesInt = 0; sscanf_s(attributes, "%d", &attributesInt); return attributesInt; }
	float GetAttributesFloat() const { float attributesFloat = 0; sscanf_s(attributes, "%f", &attributesFloat); return attributesFloat; }

	// quick way to draw line between 2 stubs in the editor
	void DrawConnectingLine(GameObjectHandle otherHandle, float alpha) const;
	void DrawConnectingLine(const GameObjectStub* otherStub, float alpha) const;
	
	// global functions to get object info
	static bool HasObjectInfo(GameObjectType type);
	static const ObjectTypeInfo& GetObjectInfo(GameObjectType type);

public: // stub data

	static const int attributesLength = 256;	// length of attributes string
	GameObjectType type;						// type of object
	XForm2 xf;									// pos and angle of object
	Vector2 size;								// width and height (may not apply to some)
	char attributes[attributesLength];			// this value can be different depending on the object
	GameObjectHandle handle;					// static handle for this stub / object
};

// use this macro to expose a class
#define	GAME_OBJECT_DEFINITION(className, stubTexture, stubColor) \
static GameObject* _##className##Build(const GameObjectStub& stub) \
{ return new className(stub); }				\
static ObjectTypeInfo _##className##Info	\
(											\
	L#className, 							\
	GOT_##className, 						\
	_##className##Build, 					\
	className##::IsSerializable, 			\
	className##::StubRender, 				\
	className##::StubRenderMap, 			\
	className##::StubAttributesDescription, \
	className##::StubDescription, 			\
	className##::StubAttributesDefault, 	\
	className##::StubSelectSize,			\
	stubTexture, 							\
	stubColor								\
);
