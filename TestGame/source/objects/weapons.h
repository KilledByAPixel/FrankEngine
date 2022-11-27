////////////////////////////////////////////////////////////////////////////////////////
/*
	Weapons
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

////////////////////////////////////////////////////////////////////////////////////////
/*
	Special Weapon
*/
////////////////////////////////////////////////////////////////////////////////////////

class SpecialWeapon : public Weapon
{
public:

	SpecialWeapon(const XForm2& xf = XForm2::Identity(), GameObject* _parent = NULL);

private:

	Projectile* LaunchProjectile(const XForm2& xf) override;

	static const WeaponDef weaponStaticDef;
	static const ParticleSystemDef fireEffect;
	static const ParticleSystemDef smokeEffect;
	static const ParticleSystemDef hitEffect;
	static const ParticleSystemDef trailEffect;
};

class SpecialWeaponProjectile : public SolidProjectile
{
public:

	SpecialWeaponProjectile(const XForm2& xf, Weapon& weapon);
	void Render() override;
	void HitObject(GameObject& object) override;
	void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) override;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Explosion
*/
////////////////////////////////////////////////////////////////////////////////////////

class Explosion : public GameObject
{
public:

	Explosion(const XForm2& xf, float _force, float _radius, float _damage = 0, GameObject* _attacker = NULL, float _time = 0, bool damageSelf = false);

private:
	
	void Update() override;
	void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) override;
	void CollisionPersist(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture) override;
	bool ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const override;

	float force;
	float radius;
	float damage;
	GameObjectSmartPointer<> attacker;
	GameTimer lifeTime;
	list<GameObjectSmartPointer<>> hitObjects;
	bool damageSelf;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Debris
*/
////////////////////////////////////////////////////////////////////////////////////////

class Debris : public SolidProjectile
{
public:

	Debris(const XForm2& xf, float speed, GameObject* attacker, float radius, const ParticleSystemDef *trailEffect = &theTrailEffect, float _brightness = 1);
	static void CreateDebris(GameObject& object, int count, float speedScale = 1);

private:

	void Render() override;
	void Update() override;
	void HitObject(GameObject& object) override;
	bool ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const override;

	float brightness;
	GameTimerPercent lifeTimer;
	static ParticleSystemDef theTrailEffect;
};
