////////////////////////////////////////////////////////////////////////////////////////
/*
	Actor
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../objects/actor.h"

void Actor::ApplyDamage(float damage, GameObject* attacker, GameDamageType damageType)
{
	if (damage <= 0)
		return;
	
	if (IsDead())
		return;

	if (damage > health)
		damage = health;

	health -= damage;

	if (IsDead())
		Kill();
}

