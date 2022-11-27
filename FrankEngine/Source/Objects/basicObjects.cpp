////////////////////////////////////////////////////////////////////////////////////////
/*
	Basic Objects
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../objects/basicObjects.h"

ConsoleCommandSimple(bool, triggerDebug,		false)

////////////////////////////////////////////////////////////////////////////////////////
/*
	SelfTrigger
*/
////////////////////////////////////////////////////////////////////////////////////////

SelfTrigger::SelfTrigger(const GameObjectStub& stub) : GameObject(stub) 
{ 
	CreatePhysicsBody(b2_staticBody); 

	// hack: make it a tiny bit smaller so triggers that are flush with collision can't be hit
	Vector2 size = stub.size;
	size.x = Max(0.0f, size.x - 0.1f);
	size.y = Max(0.0f, size.y - 0.1f);
	AddSensorBox(size); 
}

void SelfTrigger::Render()
{
	if (triggerDebug)
		g_render->RenderQuad(GetXFWorld(), GetStubSize(), Color::Red(0.5f));
}

bool SelfTrigger::ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const 
{ 
	return otherObject.IsPlayer() && otherFixture; 
}

void SelfTrigger::CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) 
{ 
	TriggerActivate(true, &otherObject, 0); 
}

Vector2 SelfTrigger::StubSelectSize(const GameObjectStub& stub)
{ 
	return stub.IsThisTypeSelected() ? stub.size : Vector2(0.5f);
}

void SelfTrigger::StubRender(const GameObjectStub& stub, float alpha)
{
	const ObjectTypeInfo& info = stub.GetObjectInfo();
	const Color c = info.GetColor().ScaleAlpha(alpha);
	g_render->RenderQuad(stub.xf, StubSelectSize(stub), c, info.GetTexture());
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	TriggerBox
*/
////////////////////////////////////////////////////////////////////////////////////////

TriggerBox::TriggerBox(const GameObjectStub& stub) : SelfTrigger(stub)
{
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(triggerData);
	parser.ParseValue(minTime);
	parser.ParseValue(reverseLogic);
	parser.ParseValue(delayTime);
	parser.ParseValue(continuous);
	if (parser.SkipToMarker('#'))
	{
		while (!parser.IsAtEnd())
		{
			int handle = 0;
			parser.ParseValue(handle);
			if (handle > 0)
				triggerHandles.push_back(handle);
		}
	}
}

void TriggerBox::Update()
{
	if (delayTimer.HasElapsed())
	{
		TriggerActivate(true, this, triggerData);
		if (!continuous)
			delayTimer.Unset();
	}

	if ((wantsDeactivate && (!touchTimer.IsSet() || touchTimer > 0)) && (minTime <= 0 || activateTimer > minTime))
	{
		delayTimer.Unset();
		TriggerActivate(false, this);
	}
}

void TriggerBox::TriggerActivate(bool activate, GameObject* activator, int data)
{
	wantsDeactivate = activate;

	if (activate)
	{
		if (delayTime > 0 && !delayTimer.IsSet())
		{
			// wait to activate
			if (!delayTimer.IsSet())
				delayTimer.SetTimeRemaining(delayTime);
			return;
		}
		if (!activateTimer.IsSet())
			activateTimer.Set();
	}
	else
	{
		if (minTime > 0 && activateTimer.IsSet() && activateTimer < minTime && activator != this)
			return; // wait to deactivate
		
		if (delayTimer.IsSet())
			return;

		touchTimer.Unset();
		activateTimer.Unset();
		wantsDeactivate = false;
	}

	static int triggerRecursionDepth = 0;
	++triggerRecursionDepth;
	if (triggerRecursionDepth > 10)
	{
		ASSERT(true); // infinite recursion detected
		return;
	}
	
	const bool sendActivate = reverseLogic ? !activate : activate;
	for (auto handle : triggerHandles)
	{
		GameObject* object = g_objectManager.GetObjectFromHandle(handle);
		if (object)
			object->TriggerActivate(sendActivate, activator, triggerData);
	}

	--triggerRecursionDepth;
}

void TriggerBox::CollisionPersist(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) 
{ 
	if (!continuous && touchTimer.IsSet())
	{
		touchTimer.Set(); 
		return;
	}

	touchTimer.Set(); 
	CollisionAdd(otherObject, contactEvent, myFixture, otherFixture);
}

void TriggerBox::StubRender(const GameObjectStub& stub, float alpha)
{
	if (g_editor.IsTileEdit())
		return;

	if (!stub.IsThisTypeSelected())
		return;
	
	// show all stubs trigger is connected to
	ObjectAttributesParser parser(stub.attributes);

	const ObjectTypeInfo& info = stub.GetObjectInfo();
	Color c = Color::Green(0.4f*alpha);
	g_render->DrawSolidBox(stub.xf, stub.size, c);

	if (parser.SkipToMarker('#'))
	{
		while (!parser.IsAtEnd())
		{
			int handle = 0;
			parser.ParseValue(handle);
			stub.DrawConnectingLine(handle, alpha);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	MusicGameObject
*/
////////////////////////////////////////////////////////////////////////////////////////

MusicGameObject::MusicGameObject(const GameObjectStub& stub) : SelfTrigger(stub)
{
	// get the filename
	strncpy_s(filename, GameObjectStub::attributesLength, stub.attributes, GameObjectStub::attributesLength);
}

void MusicGameObject::TriggerActivate(bool activate, GameObject* activator, int data)
{
	if (!activate)
		return;

	string line(filename);
	std::wstring wline;
	wline.assign(line.begin(), line.end());
	g_sound->GetMusicPlayer().Transition(wline.c_str());
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	SoundGameObject
*/
////////////////////////////////////////////////////////////////////////////////////////

SoundGameObject::SoundGameObject(const GameObjectStub& stub) : SelfTrigger(stub)
{
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue((int&)sound);
	parser.ParseValue(volume);
	parser.ParseValue(frequency);
	parser.ParseValue(selfTrigger);
	parser.ParseValue(onActivate);
	parser.ParseValue(onDeactivate);
}

void SoundGameObject::TriggerActivate(bool activate, GameObject* activator, int data)
{
	if (activate && !onActivate)
		return;
	else if (!activate && !onDeactivate)
		return;

	MakeSound(sound, volume, frequency, 0);
}

void SoundGameObject::CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) 
{ 
	if (!selfTrigger)
		return;

	TriggerActivate(true, &otherObject, 0); 
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	TextDecal
*/
////////////////////////////////////////////////////////////////////////////////////////

int TextDecal::DefaultRenderGroup = -500;

TextDecal::Attributes::Attributes(const GameObjectStub& stub)
{
	renderGroup = DefaultRenderGroup;
	// r g b a renderGroup emissive normals map visible #text
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(color);
	parser.ParseValue(renderGroup);
	parser.ParseValue(emissive);
	parser.ParseValue(normals);
	parser.ParseValue(visible);
	parser.ParseValue(map);
	parser.ParseValue(fontIndex);
	*text = 0;
	if (parser.SkipToMarker('#'))
		parser.GetStringPrintableCopy(text);
}

TextDecal::TextDecal(const GameObjectStub& stub) : GameObject(stub), attributes(stub)
{
	size = stub.size;
	SetRenderGroup(attributes.renderGroup);
	SetVisible(attributes.visible);
}

void TextDecal::RenderPost()
{
	if (GetRenderGroup() < 10000)
		return;
	
	FontFlags flags = FontFlags(FontFlag_CenterX | FontFlag_CenterY);
	DeferredRender::EmissiveRenderBlock emissiveRenderBlock(attributes.emissive > 0);
	
	Color color = attributes.color;
	if (DeferredRender::GetRenderPassIsEmissive() && emissiveRenderBlock.IsActive())
		color *= attributes.emissive;

	const XForm2 xf = GetXFInterpolated();
	FrankFont* font = g_gameControlBase->GetGameFont(attributes.fontIndex);
	if (font)
		font->Render(attributes.text, xf, 2*size.y, color, flags);
}

void TextDecal::Render()
{
	if (GetRenderGroup() >= 10000 || attributes.map)
		return;

	if (DeferredRender::GetRenderPassIsShadow())
		return;	// text never renders to lights

	FontFlags flags = FontFlags(FontFlag_CenterX | FontFlag_CenterY);
	if (!attributes.normals)
		flags = FontFlags(flags | FontFlag_NoNormals);
	DeferredRender::EmissiveRenderBlock emissiveRenderBlock(attributes.emissive > 0);

	Color color = attributes.color;
	if (DeferredRender::GetRenderPassIsEmissive() && emissiveRenderBlock.IsActive())
		color *= attributes.emissive;

	const XForm2 xf = GetXFInterpolated();
	FrankFont* font = g_gameControlBase->GetGameFont(attributes.fontIndex);
	if (font)
		font->Render(attributes.text, xf, 2*size.y, color, flags);
}

void TextDecal::StubRender(const GameObjectStub& stub, float alpha)
{
	Attributes attributes(stub);
	attributes.color.a *= alpha;
	if (attributes.map)
		attributes.color.a *= 0.2f;

	FrankFont* font = g_gameControlBase->GetGameFont(attributes.fontIndex);
	if (font)
		font->Render(attributes.text, stub.xf, 2*stub.size.y, attributes.color, FontFlags(FontFlag_CenterX | FontFlag_CenterY));
}

void TextDecal::StubRenderMap(const GameObjectStub& stub, float alpha)
{
	const XForm2 xf = stub.xf;
	Vector2 size = stub.size;
	Color color = Color::White();
	int renderGroup = 0;
	float emissive = 0;
	bool normals = false;
	bool visible = false;
	bool map = false;
	int fontIndex = 0;
	
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(color);
	parser.ParseValue(renderGroup);
	parser.ParseValue(emissive);
	parser.ParseValue(normals);
	parser.ParseValue(visible);
	parser.ParseValue(map);
	parser.ParseValue(fontIndex);
	char text[GameObjectStub::attributesLength] = "";
	if (parser.SkipToMarker('#'))
		parser.GetStringPrintableCopy(text);

	if (!map)
		return;
	
	color.a *= alpha;
	FrankFont* font = g_gameControlBase->GetGameFont(fontIndex);
	if (font)
		font->Render(text, xf, 2*size.y, color, FontFlags(FontFlag_CenterX | FontFlag_CenterY), false);
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	TileImageDecal
*/
////////////////////////////////////////////////////////////////////////////////////////

int TileImageDecal::DefaultRenderGroup = -500;

TileImageDecal::Attributes::Attributes(const GameObjectStub& stub)
{
	renderGroup = DefaultRenderGroup;
	// x y w h r g b a mirror renderGroup shadows emissive transparent visible collision map #texture
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(tilePos);
	parser.ParseValue(tileSize);
	parser.ParseValue(color);
	parser.ParseValue(mirror);
	parser.ParseValue(renderGroup);
	parser.ParseValue(castShadows);
	parser.ParseValue(emissive);
	parser.ParseValue(transparent);
	parser.ParseValue(visible);
	parser.ParseValue(collision);
	parser.ParseValue(map);
	if (parser.SkipToMarker('#'))
		parser.ParseTextureName(texture);
	
}
TileImageDecal::TileImageDecal(const GameObjectStub& stub) : GameObject(stub), attributes(stub)
{
	size = stub.size;
	SetRenderGroup(attributes.renderGroup);
	SetVisible(attributes.visible);

	if (attributes.collision)
	{
		CreatePhysicsBody(b2_staticBody);
		AddFixtureBox(stub.size);
	}

	if (attributes.mirror)
		size.x *= -1;
}

void TileImageDecal::RenderPost()
{
	if (GetRenderGroup() < 10000)
		return;

	TextureID tileSheet = g_render->GetTextureTileSheet(attributes.texture);
	if (tileSheet != Texture_Invalid)
		return;

	// render overlay
	const XForm2 xf = GetXFInterpolated();
	g_render->RenderTile(attributes.tilePos, attributes.tileSize, xf, size, attributes.color, attributes.texture);
}

void TileImageDecal::Render()
{
	if (GetRenderGroup() >= 10000)
		return;

	if (!attributes.castShadows && DeferredRender::GetRenderPassIsShadow() && !DeferredRender::GetRenderPassIsDirectionalShadow())
		return;
	
	TextureID tileSheet = g_render->GetTextureTileSheet(attributes.texture);
	if (tileSheet != Texture_Invalid)
		return;

	DeferredRender::EmissiveRenderBlock emissiveRenderBlock(attributes.emissive > 0);
	DeferredRender::TransparentRenderBlock transparentRenderBlock(attributes.transparent);
	DeferredRender::BackgroundRenderBlock backgroundRenderBlock(!attributes.castShadows);

	Color color = attributes.color;
	if (DeferredRender::GetRenderPassIsEmissive() && emissiveRenderBlock.IsActive())
		color *= attributes.emissive;

	const XForm2 xf = GetXFInterpolated();
	g_render->RenderTile(attributes.tilePos, attributes.tileSize, xf, size, color, attributes.texture);
}

void TileImageDecal::StubRender(const GameObjectStub& stub, float alpha)
{
	Attributes attributes(stub);
	Vector2 size = stub.size;
	if (attributes.mirror)
		size.x *= -1;
	attributes.color.a *= alpha;
	if (!stub.IsThisTypeSelected())
		attributes.color.a *= 0.5f;

	TextureID tileSheet = g_render->GetTextureTileSheet(attributes.texture);
	if (tileSheet != Texture_Invalid)
	{
		GameObject::StubRender(stub, alpha);
		return;
	}

	g_render->RenderTile(attributes.tilePos, attributes.tileSize, stub.xf, size, attributes.color, attributes.texture);
}

void TileImageDecal::StubRenderMap(const GameObjectStub& stub, float alpha)
{
	Attributes attributes(stub);
	
	if (!attributes.map)
		return;
	
	Vector2 size = stub.size;
	if (attributes.mirror)
		size.x *= -1;
	attributes.color.a *= alpha;

	TextureID tileSheet = g_render->GetTextureTileSheet(attributes.texture);
	if (tileSheet != Texture_Invalid)
	{
		GameObject::StubRender(stub, alpha);
		return;
	}

	g_render->RenderTile(attributes.tilePos, attributes.tileSize, stub.xf, size, attributes.color, attributes.texture, false);
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	ObjectSpawner
*/
////////////////////////////////////////////////////////////////////////////////////////

ObjectSpawner::ObjectSpawner(const GameObjectStub& stub) : GameObject(stub)
{
	SetVisible(false);

	// set size from stub
	spawnSize = stub.size;
	
	// spawnType, spawnSpeed, spawnMax, spawnRandomness, spawnRate, isActive
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(spawnType);
	parser.ParseValue(spawnSpeed);
	parser.ParseValue(spawnMax);
	parser.ParseValue(spawnRandomness);
	parser.ParseValue(spawnRate);
	parser.ParseValue(isActive);
	if (parser.SkipToMarker('#'))
		parser.GetStringPrintableCopy(attributes);

	// protect against bad input
	if (spawnType < 0)
		spawnType = GameObjectType(0);

	spawnTimer.Set(0);
}

void ObjectSpawner::Update()
{
	// remove any objects from our list that are gone
	for (list<GameObjectSmartPointer<>>::iterator it = spawnedObjects.begin(); it != spawnedObjects.end(); ) 
	{
		GameObject* gameObject = *it;
		if (!gameObject)
			it = spawnedObjects.erase(it);
		else
			++it;
	}
	
	if (spawnedObjects.size() >= spawnMax)
		spawnTimer.Set(spawnRate);

	if (isActive && spawnRate > 0)
	{
		if (!spawnTimer.IsSet())
			spawnTimer.Set(spawnRate);
		else if (spawnTimer.HasElapsed())
		{
			if (SpawnObject())
				spawnTimer.Set(spawnRate);
		}
	}
}

GameObject* ObjectSpawner::SpawnObject()
{
	if (spawnType == GameObjectType(0))
		return NULL;

	if (spawnedObjects.size() >= spawnMax)
	{
		// preventw spawning more then the max
		return NULL;
	}
	
	Vector2 size = spawnSize * RAND_BETWEEN(1 - spawnRandomness, 1 + spawnRandomness);
	GameObject* object = GameObjectStub(GetXFWorld(), size, spawnType, attributes).BuildObject();
	if (!object)
		return NULL;

	// keep track of our spawned objects by handle
	spawnedObjects.push_back(object);

	float speed = spawnSpeed * RAND_BETWEEN(1 - spawnRandomness, 1 + spawnRandomness);
	SetVelocity(speed * GetUpWorld() + GetVelocity());

	return object;
}

void ObjectSpawner::StubRender(const GameObjectStub& stub, float alpha)
{
	GameObject::StubRender(stub, alpha);

	GameObjectType spawnType = GameObjectType(0);
	
	// spawnType, spawnSize, spawnSpeed, spawnMax, spawnRandomness, spawnRate, isActive
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(spawnType);

	if (!GameObjectStub::HasObjectInfo(spawnType))
		return;
	
	// show a preview of where the spawned objects will be
	const float alphaScale = alpha*0.5f;
	const GameObjectStub spawnStub(stub.xf, Vector2(stub.size), spawnType);
	spawnStub.Render(alphaScale);
	g_render->DrawOutlinedBox(spawnStub.xf, stub.size, Color::White(0.1f), Color::Black(alphaScale));
	g_render->DrawLine(Line2(stub.xf.position, spawnStub.xf.position), Color::White(alphaScale));
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	ShootTrigger
*/
////////////////////////////////////////////////////////////////////////////////////////

ShootTrigger::ShootTrigger(const GameObjectStub& stub) : GameObject(stub)
{
	// make it appear below stuff
	SetRenderGroup(-500);
	
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(triggerData);
	parser.ParseValue(deactivateTime);
	parser.ParseValue(reverseLogic);
	parser.ParseValue(isToggle);
	if (parser.SkipToMarker('#'))
	{
		while (!parser.IsAtEnd())
		{
			int handle = -1;
			parser.ParseValue(handle);
			if (handle == -1)
				break;
			triggerHandles.push_back(handle);
		}
	}

	// make it square
	size = Vector2(stub.size.x);
	
	// hook up physics
	CreatePhysicsBody(b2_staticBody);
	AddFixtureCircle(size.x);
}

void ShootTrigger::Update()
{
	if (deactivateTime > 0 && activateTimer > deactivateTime)
		Activate(reverseLogic);
}

void ShootTrigger::Render()
{
	const XForm2 xf = GetXFInterpolated();
	
	DeferredRender::EmissiveRenderBlock emissiveRenderBlock;
	DeferredRender::SpecularRenderBlock specularRenderBlock;
	Color color = IsActive()? Color::Yellow() : Color::Green();
	g_render->RenderQuad(xf, size, color, Texture_Circle);
}

void ShootTrigger::CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture)
{
	GameObject* player = g_gameControlBase->GetPlayer();
	if (!player || otherObject.GetTeam() != player->GetTeam() || !otherObject.IsProjectile())
		return;

	if (isToggle)
		Activate(!IsActive());
	else
		Activate(!reverseLogic);
}

void ShootTrigger::Activate(bool activate)
{
	if (activate)
		activateTimer.Set();
	else
		activateTimer.Unset();

	for (auto handle : triggerHandles)
	{
		GameObject* object = g_objectManager.GetObjectFromHandle(handle);
		if (object)
			object->TriggerActivate(activate, this, triggerData);
	}
}

void ShootTrigger::StubRender(const GameObjectStub& stub, float alpha)
{
	GameObject::StubRender(stub, alpha);

	if (!stub.IsThisTypeSelected())
	{
		// only show preview when this object type is selected
		return;
	}

	bool isToggle = false;
	float deactivateTime = 0;

	// show all stubs trigger is connected to
	ObjectAttributesParser parser(stub.attributes);
	if (parser.SkipToMarker('#'))
	{
		while (!parser.IsAtEnd())
		{
			int handle = 0;
			parser.ParseValue(handle);
			stub.DrawConnectingLine(handle, alpha);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	MiniMapGameObject - draws the minimap to a place in the world
*/
////////////////////////////////////////////////////////////////////////////////////////

MiniMapGameObject::MiniMapGameObject(const GameObjectStub& stub) : GameObject(stub)
{
	int renderGroup = 0;
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(xfCenter.position);
	parser.ParseValue(xfCenter.angle);
	parser.ParseValue(zoom);
	parser.ParseValue(color);
	parser.ParseValue(renderGroup);
	parser.ParseValue(showFull);
	SetRenderGroup(renderGroup);
}

void MiniMapGameObject::StubRender(const GameObjectStub& stub, float alpha)
{
	MiniMap* miniMap = g_gameControlBase->GetMiniMap();
	if (!miniMap)
	{
		GameObject::StubRender(stub, alpha);
		return;
	}

	XForm2 xfCenter = XForm2::Identity();
	Color color = Color::White();
	float zoom = 100;
	int renderGroup = 0;
	bool showFull = true;
	ObjectAttributesParser parser(stub.attributes);
	parser.ParseValue(xfCenter.position);
	parser.ParseValue(xfCenter.angle);
	parser.ParseValue(zoom);
	parser.ParseValue(color);
	parser.ParseValue(renderGroup);
	parser.ParseValue(showFull);
	
	if (!g_editor.GetObjectEditor().IsSelected(stub))
		alpha *= 0.5;
	miniMap->RenderWorldSpace(stub.xf, stub.size, color.ScaleAlpha(alpha), xfCenter, zoom, showFull, true);
}

void MiniMapGameObject::Render()
{
	MiniMap* miniMap = g_gameControlBase->GetMiniMap();
	if (!miniMap)
		return;

	miniMap->RenderWorldSpace(GetXFInterpolated(), GetStubSize(), color, xfCenter, zoom, showFull);
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	ConsoleCommandObject - Executes console command when triggered
*/
////////////////////////////////////////////////////////////////////////////////////////

ConsoleCommandObject::ConsoleCommandObject(const GameObjectStub& stub) : GameObject(stub)
{
	ObjectAttributesParser parser(stub.attributes);
	parser.GetStringPrintableCopy(text);
}

void ConsoleCommandObject::TriggerActivate(bool activate, GameObject* activator, int data)
{
	if (activate)
	{
		wchar_t wtext[GameObjectStub::attributesLength];
		mbstowcs_s(NULL, wtext, GameObjectStub::attributesLength, text, _TRUNCATE);
		GetDebugConsole().ParseInputLine(wtext, true);
	}
}