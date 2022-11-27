////////////////////////////////////////////////////////////////////////////////////////
/*
	Weapon
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../objects/weapon.h"

WeaponDef::WeaponDef
(
	float _fireRate,							// amount of time between shots
	float _damage,								// amount of damage applied when bullets hit
	float _projectileSpeed,						// speed the projectiles when fired
	float _projectileRadius,					// size of the projectiles
	float _fireAngle,							// randomness in angle applied to fire direction
	bool _fireAutomatic,						// does weapon continue firing when trigger is held?
	bool _projectileBullet,						// are projectiles bullets or physical?
	SoundID _fireSound,						// sound effect when weapon is fired
	const ParticleSystemDef* _trailEffect,		// trail applied to projectiles
	const ParticleSystemDef* _fireFlashEffect,	// flash effect when weapon is fired
	const ParticleSystemDef* _fireSmokeEffect,	// smoke effect when weapon is fired
	const ParticleSystemDef* _hitEffect,		// effect when projectile hits
	SoundID _hitSound,							// sound effect when projectile hits
	int _projectileCount						// how many projectiles to launch (for shotgun type weapons)
) :
	fireRate(_fireRate),
	damage(_damage),
	projectileSpeed(_projectileSpeed),
	projectileRadius(_projectileRadius),
	fireAngle(_fireAngle),
	projectileCount(_projectileCount),
	fireSound(_fireSound),
	hitSound(_hitSound),
	fireAutomatic(_fireAutomatic),
	projectileBullet(_projectileBullet)
{
	if (_trailEffect)
		trailEffect = *_trailEffect;
	if (_fireFlashEffect)
		fireFlashEffect = *_fireFlashEffect;
	if (_fireSmokeEffect)
		fireSmokeEffect = *_fireSmokeEffect;
	if (_hitEffect)
		hitEffect = *_hitEffect;
}

Weapon::Weapon(const WeaponDef &_weaponDef, const XForm2& xf, GameObject* _parent, const XForm2& _xfFire) :
	GameObject(xf, _parent),
	weaponDef(_weaponDef),
	xfFire(_xfFire),
	owner(_parent)
{
}

void Weapon::Update()
{
	fireTimer += GAME_TIME_STEP;
	fireTimer = Min(fireTimer, GAME_TIME_STEP);

	if (triggerIsDown)
		while (Fire());

	if (triggerWasDown && !triggerIsDown)
		TriggerReleased();

	triggerWasDown = triggerIsDown;
	triggerIsDown = false;
	
	ParticleEmitter* smokeEmitter = smokeEmitterHandle;
	if (smokeEmitter && lastFiredTimer > weaponDef.fireRate)
		smokeEmitter->SetPaused(true, false);
}

// will be called automatically in Update() depending if TiggerIsDown() was called and the weapon can fire
// this can be called directly to try firing a single shot, but may not fire if unable to
// retuns true if weapon was fired
bool Weapon::Fire()
{
	if (!CanFire())
		return false;

	// update the fire timer
	lastFiredTimer.Set();
	if (GetWeaponDef().fireAutomatic)
		fireTimer -= weaponDef.fireRate;
	else
		fireTimer = -weaponDef.fireRate;

	PlayFireEffect();
	
	const float maxFireAngle = GetWeaponDef().fireAngle;
	for (int i = 0; i < weaponDef.projectileCount; ++i)
	{
		const float fireAngle = GetAngleWorld() + RAND_BETWEEN(-maxFireAngle, maxFireAngle);
		LaunchProjectile(xfFire * XForm2(GetPosWorld(), fireAngle));
	}

	return true;
}

// weapons automatically launch a projectile when they fire
// this may be a physical or a bullet type projectile
GameObject* Weapon::LaunchProjectile(const XForm2& xf)
{
	if (GetWeaponDef().projectileBullet)
		return new BulletProjectile(xf, *this);
	else
		return new SolidProjectile(xf, *this);
}

// play particle and sound effect when the weapon fires
void Weapon::PlayFireEffect()
{
	// play sound effect
	if (weaponDef.fireSound)
		MakeSound(weaponDef.fireSound);

	// start flash effect
	if (weaponDef.fireFlashEffect.IsValid())
		new ParticleEmitter(weaponDef.fireFlashEffect, xfFire, this);

	// startfire smoke effect
	ParticleEmitter* smokeEmitter = smokeEmitterHandle;
	if (smokeEmitter && !smokeEmitter->IsDestroyed())
	{
		smokeEmitter->SetPaused(false, false);
		smokeEmitter->ResetLifeTime();
	}
	else if (weaponDef.fireSmokeEffect.IsValid())
		smokeEmitterHandle = new ParticleEmitter(weaponDef.fireSmokeEffect, xfFire, this);
}