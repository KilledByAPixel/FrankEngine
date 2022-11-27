////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Object Builder
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	ObjectTypeInfo
*/
////////////////////////////////////////////////////////////////////////////////////////

static GameObject* InvalidObjectBuild(const GameObjectStub& stub) { return NULL; }
static bool InvalidObjectIsSerializable() { return false; }
const ObjectTypeInfo ObjectTypeInfo::invalidTypeInfo
(
	L"Invalid Type", 
	GameObjectType(0), 
	InvalidObjectBuild, 
	InvalidObjectIsSerializable, 
	GameObject::StubRender, 
	GameObject::StubRenderMap, 
	GameObject::StubAttributesDescription, 
	GameObject::StubDescription,
	GameObject::StubAttributesDefault,
	GameObject::StubSelectSize
);

ObjectTypeInfo* g_gameObjectInfoArray[GAME_OBJECT_TYPE_MAX_COUNT] = {0};

// what is the maximum registered object type
GameObjectType ObjectTypeInfo::maxType = GameObjectType(0);

////////////////////////////////////////////////////////////////////////////////////////
/*
	GameObjectStub
*/
////////////////////////////////////////////////////////////////////////////////////////

GameObjectStub::GameObjectStub
(
	const XForm2& _xf, 
	const Vector2& _size, 
	GameObjectType _type, 
	const char* _attributes, 
	GameObjectHandle _handle
) :
	xf(_xf),
	size(_size),
	type(_type),
	handle(_handle)
{
	if (_attributes)
		strncpy_s(attributes, sizeof(attributes), _attributes, sizeof(attributes));	
	else
		*attributes = 0;
}

bool GameObjectStub::HasObjectInfo(GameObjectType type) 
{ 
	return (type > GAME_OBJECT_TYPE_INVALID && type <= ObjectTypeInfo::GetMaxType()) && g_gameObjectInfoArray[g_gameObjectInfoArray[type]? type : 0] != NULL;
}

const ObjectTypeInfo& GameObjectStub::GetObjectInfo(GameObjectType type) 
{ 
	if (HasObjectInfo(type))
		return *g_gameObjectInfoArray[g_gameObjectInfoArray[type]? type : 0]; 
	else
		return ObjectTypeInfo::invalidTypeInfo;
}

bool GameObjectStub::IsThisTypeSelected() const
{
	return g_gameControlBase->IsObjectEditMode() && type == g_editor.GetObjectEditor().GetNewStubType(); 
}

void GameObjectStub::DrawConnectingLine(GameObjectHandle otherHandle, float alpha) const
{
	GameObjectStub* otherStub = g_terrain->GetStub(otherHandle);
	DrawConnectingLine(otherStub, alpha);
}

void GameObjectStub::DrawConnectingLine(const GameObjectStub* otherStub, float alpha) const
{
	if (!otherStub)
		return;

	g_render->DrawThickLineGradient(Line2(xf.position, otherStub->xf.position), 0.1f, Color::Red(alpha), Color::Cyan(alpha));
}