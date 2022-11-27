////////////////////////////////////////////////////////////////////////////////////////
/*
	Enemies
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class Enemy : public Actor
{
public:

	Enemy(const GameObjectStub& stub);
	static WCHAR* StubDescription() { return L"simple enemy that moves towards player"; }
	static WCHAR* StubAttributesDescription() { return L""; }

	void Init();
	void Update();
	void Render();
	void Kill();
	
	bool ShouldCollideSight() const { return false; }
	void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture);
	static bool IsSerializable() { return true; } 

private:

	Vector2 size;
	float effectSize;
};
