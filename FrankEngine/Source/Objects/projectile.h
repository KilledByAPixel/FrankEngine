////////////////////////////////////////////////////////////////////////////////////////
/*
	Projectile
	Copyright 2013 Frank Force - http://www.frankforce.com

	- a simple flying object, 
	- can fired by a weapon or created from scratch
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../objects/gameObject.h"
#include "../objects/particleSystem.h"
#include "../sound/soundControl.h"

class Weapon;
struct WeaponDef;

class Projectile : public GameObject
{
public:

	GameObject* GetAttacker() const					{ return attackerHandle; }
	void SetAttacker(GameObject* attacker)			{ attackerHandle = attacker; }
	bool IsAttacker(const GameObject &obj) const	{ return (attackerHandle == obj); }

	float GetRadius() const							{ return radius; }
	void SetRadius(float _radius)					{ radius = _radius; }
	float GetDamage() const							{ return damage; }
	void SetDamage(float _damage)					{ damage = _damage; }
	int GetDamageType() const						{ return damageType; }
	void SetDamageType(GameDamageType _damageType)	{ damageType = _damageType; }
	bool HasTrailEmitter() const					{ return trailEmitter != NULL; }
	ParticleEmitter& GetTrailEmitter() const		{ return *trailEmitter; }
	bool IsProjectile() const override				{ return true; }
	virtual void HitObject(GameObject& object)		{ Kill(); }
	void Kill();

	bool ShouldCollide(const GameObject& otherObject, const b2Fixture* myShape, const b2Fixture* otherFixture) const;
	void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture);
	bool ShouldCollideSight() const override { return false; }

	static float defaultDensity;
	static float projectileSpeedScale;
	static bool projectilesCollide;
	static bool applyAttackerVelocity;

protected:

	Projectile
	(
		const XForm2& xf, 
		float speed, 
		GameObject* attacker = NULL, 
		const ParticleSystemDef* particleDef = NULL, 
		float damage = 100, 
		float radius = 0.0f, 
		const ParticleSystemDef* _hitEffect = NULL, 
		SoundID _hitSound = Sound_Invalid
	);
	Projectile(const XForm2& xf, Weapon& weapon);
	Projectile(const XForm2& xf, const WeaponDef& weaponDef, GameObject* attacker = NULL);

	void Init(GameObject* attacker, const ParticleSystemDef* particleDef);
	void Render() override;

	float radius;
	float damage;
	GameDamageType damageType;
	ParticleSystemDef hitEffect;
	SoundID hitSound;
	GameObjectSmartPointer<> attackerHandle;
	class ParticleEmitter* trailEmitter;
};


class SolidProjectile : public Projectile
{
public:

	SolidProjectile
	(
		const XForm2& xf,
		float speed, 
		GameObject* attacker = NULL, 
		const ParticleSystemDef* particleDef = NULL, 
		float damage = 100, 
		float radius = 0.0f,
		const ParticleSystemDef* _hitEffectDef = NULL, 
		SoundID _hitSound = Sound_Invalid,
		float density = Projectile::defaultDensity, 
		float friction = Physics::defaultFriction,
		float restitution = Physics::defaultRestitution

	);
	SolidProjectile(const XForm2& xf, Weapon& weapon, float density = Projectile::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution);
	SolidProjectile(const XForm2& xf, const WeaponDef& weaponDef, GameObject* attacker = NULL, float density = Projectile::defaultDensity, float friction = Physics::defaultFriction, float restitution = Physics::defaultRestitution);
	
	void Update() override;

private:

	void Init(GameObject* attacker, float speed, float density, float friction, float restitutionn);
};


class BulletProjectile : public Projectile
{
public:

	BulletProjectile
	(
		const XForm2 &xf, 
		float speed, 
		GameObject* attacker = NULL, 
		const ParticleSystemDef* particleDef = NULL, 
		float damage = 100, 
		float radius = 0.2f,
		const ParticleSystemDef* _hitEffectDef = NULL, 
		SoundID _hitSound = Sound_Invalid
	);
	BulletProjectile(const XForm2& xf, Weapon& weapon);
	BulletProjectile(const XForm2& xf, const WeaponDef& weaponDef, GameObject* attacker = NULL);

	void SetLinearDamping(float _damping) override		{ damping = _damping; }
	void SetMass(float _mass)							{ mass = _mass; }
	float GetMass() const override						{ return mass; }
	Vector2 GetVelocity() const override				{ return velocity; }
	void SetVelocity(const Vector2& v) override			{ velocity = v; }
	void ApplyAcceleration(const Vector2& a) override;

	static float defaultDamping;

protected:
	
	virtual void CollisionTest();
	void Update() override;
	void UpdateTransforms() override;
	bool IsStatic() const override { return false; }

protected:

	void Init(GameObject* attacker, float speed);

	Vector2 velocity;
	float mass;
	float damping;
};
