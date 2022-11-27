////////////////////////////////////////////////////////////////////////////////////////
/*
	Player
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

class Player : public Actor
{
public:	// basic functionality

	Player(const XForm2& xf = XForm2::Identity()) : 
		Actor(GameObjectStub(xf)), 
		light(NULL)
	{ Init(); }
	
	// create 
	Player(const GameObjectStub& stub) : 
		Actor(stub), 
		light(NULL)
	{ Init(); }

	~Player();

	void Init();
	void Update() override;
	void Render() override;
	void Kill() override;
	void UpdateTransforms() override;
	bool IsPlayer() const override			{ return true; }
	bool IsOwnedByPlayer() const override	{ return true; }
	bool ShouldStreamOut() const override	{ return false; }
	void ApplyDamage(float damage, GameObject* attacker = NULL, GameDamageType damageType = GameDamageType_Default) override;
	float GetLifeTime() const				{ return lifeTimer; }
	float GetDeadTime() const				{ return deadTimer; }

private:
	
	bool ShouldCollideSight() const override { return false; }
	bool ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const override;

	float radius;
	GameTimer lifeTimer;
	GameTimer deadTimer;
	GameTimer timeSinceDamaged;
	Light* light;
	Light* damageLight;
	Weapon* equippedWeapon;

	GameTimer pathTimer;
	list<Vector2> path;
};
