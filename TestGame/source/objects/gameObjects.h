////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Objects
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////////////////////////////
/*
	Crate - explodable crate that takes damage from weapons or collisions
*/
////////////////////////////////////////////////////////////////////////////////////////

class Crate : public Actor
{
public:

	Crate(const GameObjectStub& stub);

	void Update() override;
	void Render() override;
	void Kill() override;
	void Explode();
	void CollisionResult(GameObject& otherObject, const ContactResult& contactResult, b2Fixture* myFixture, b2Fixture* otherFixture) override;

	static WCHAR* StubDescription() { return L"A physical crate."; }
	static WCHAR* StubAttributesDescription() { return L""; }

private:

	GameTimerPercent explosionTimer;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	StressTest - track how long player is in the test area and displays messages
*/
////////////////////////////////////////////////////////////////////////////////////////

class StressTest : public GameObject
{
public:

	static WCHAR* StubDescription() { return L"Turn on a bunch of spawners and track how long player survives."; }
	
	StressTest(const GameObjectStub& stub);	
	void Update() override;
	void Render() override;
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0) override;

private:

	static float highTime;
	GameTimer activeTimer;
};
