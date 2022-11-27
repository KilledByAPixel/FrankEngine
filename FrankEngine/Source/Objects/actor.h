////////////////////////////////////////////////////////////////////////////////////////
/*
	Actor
	Copyright 2013 Frank Force - http://www.frankforce.com

	- has health, can die if it recieves too much damage
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../objects/gameObject.h"

class Actor : public GameObject
{
public:

	explicit Actor(const GameObjectStub& stub, float _health = 100) :
		GameObject(stub, NULL),
		health(_health),
		maxHealth(_health)
	{}

	explicit Actor(const XForm2& xf, float _health = 100, bool addToWorld = true) :
		GameObject(xf, NULL, addToWorld),
		health(_health),
		maxHealth(_health)
	{}
		
	bool IsActor() const override { return true; }
	virtual float GetHealth() const { return health; }
	virtual float GetMaxHealth() const { return maxHealth; }
	virtual float GetHealthPercent() const { return health / maxHealth; }
	virtual void SetHealth(float _health) { health = _health; }
	virtual void SetMaxHealth(float _maxHealth) { maxHealth = _maxHealth; }
	virtual void ResetHealth(float _maxHealth) { health = maxHealth = _maxHealth; }
	virtual void ResetHealth() { health = maxHealth; } 
	virtual bool IsDead() const override { return health <= 0; }
	virtual void ApplyDamage(float damage, GameObject* attacker = NULL, GameDamageType damageType = GameDamageType_Default) override;
	virtual void RefillHealth(float deltaHealth) 
	{
		ASSERT(deltaHealth > 0);
		if (!IsDead())
			health = Min(health + deltaHealth, maxHealth);
	}

protected:

	float health;
	float maxHealth;
};
