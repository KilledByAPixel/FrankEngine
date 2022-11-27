////////////////////////////////////////////////////////////////////////////////////////
/*
	Enemies
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"
#include "enemies.h"
#include "weapons.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Enemy
*/
////////////////////////////////////////////////////////////////////////////////////////

GAME_OBJECT_DEFINITION(Enemy, Texture_Arrow, Color::Yellow());

Enemy::Enemy(const GameObjectStub& stub) :
	Actor(stub)
{
	size = stub.size;
	Init();
}

void Enemy::Init()
{
	// basic object settings
	SetTeam(GameTeam_Enemy);
	
	// set health based on size of object
	const float health = 200*size.x*size.y;
	ResetHealth(health);

	{
		// create the physics body
		CreatePhysicsBody();
		AddFixtureBox(size);
		GetPhysicsBody()->SetLinearDamping(1.0f);
		GetPhysicsBody()->SetAngularDamping(0.5f);
		SetHasGravity(false);
	}
	{
		// scale effects based on size
		effectSize = Min(size.x, size.y);

		// simple trail effect
		const ParticleSystemDef defaultTrailEffect = ParticleSystemDef::BuildRibbonTrail
		(
			Color(1.0f, 0.0f, 0.0f, 0.3f),	// color1 start
			Color(1.0f, 0.5f, 0.0f, 0.3f),	// color2 start
			Color(1.0f, 0.0f, 0.0f, 0.0f),	// color1 start
			Color(1.0f, 0.5f, 0.0f, 0.0f),	// color2 start
			1,	0,							// start end size
			2.0f, .3f, 10,	0,				// particle time, speed, emit rate, emit time
			0, 0,							// gravity, randomness
			ParticleFlags::Additive
		);
		new ParticleEmitter(defaultTrailEffect, Vector2::Zero(), this, effectSize);
	}
}

void Enemy::Update()
{
	const Player* player = g_player;
	if (!player || player->IsDead())
		return;
	
	// check if player is in view
	Line2 playerSightLine(GetPosWorld(), g_player->GetPosWorld());
	GameObject* hitObject = g_physics->RaycastSimple(playerSightLine, NULL, this, true);
	if (hitObject && !hitObject->IsOwnedByPlayer())
		return;

	// fly straight at nearest player
	const Vector2 desiredPos = player->GetPosWorld();
	const Vector2 deltaPosDesired = desiredPos - GetPosWorld();
	const Vector2 direction = deltaPosDesired.Normalize();

	// move towards target
	static float accel = 10.0f;
	ConsoleCommand(accel, seekerAccel);
	ApplyAcceleration(direction * accel);

	// spin around
	static float angularAccel = 3.0f;
	ConsoleCommand(angularAccel, seekerAngularAccel);
	ApplyAngularAcceleration(angularAccel);
	
	// apply force to counter side velocity
	static float sideDragForce = 2;
	ConsoleCommand(sideDragForce, seekerSideDragForce);
	const Vector2 right = direction.RotateRightAngle();
	const Vector2 velocity = GetPhysicsBody()->GetLinearVelocity();
	const float dp = velocity.Dot(right);
	ApplyAcceleration(-right * dp * sideDragForce);

	// cap max speed
	static float maxSpeed = 7.0f;
	ConsoleCommand(maxSpeed, seekerSpeed);
	CapSpeed(maxSpeed);

	Actor::Update();
}

void Enemy::Render()
{
	const XForm2 xf = GetXFInterpolated();
	{
		// give it a transparent yellow glow
		DeferredRender::TransparentRenderBlock transparentRenderBlock;
		DeferredRender::EmissiveRenderBlock emissiveRenderBlock;
		g_render->RenderQuad(xf, size,	Color::Yellow(0.5f), Texture_Invalid);
	}

	g_render->RenderQuad(xf, size*0.6f,	Color::Orange(), Texture_Invalid);
}

void Enemy::Kill()
{
	if (IsDestroyed())
		return;
	
	{
		// kick off an explosion
		new Explosion(GetXFWorld(), effectSize, 6*effectSize, GetMaxHealth(), this);
	}
	{
		// kick off a light flash
		Light* light = new Light(GetPosWorld(), NULL, effectSize*30, Color(1.0f, 0.95f, 0.8f, 1.0f), true);
		light->SetOverbrightRadius(effectSize*2);
		light->SetFadeTimer(0.6f, true, true);
	}
	{
		// create some debris
		Debris::CreateDebris(*this, (int)(0.2f*GetMaxHealth()));
	}
	{
		// kick off smoke
		ParticleSystemDef smokeEffectDef = ParticleSystemDef::Build
		(
			Texture_Smoke,			// particle texture
			Color::Grey(0.5f, 0.6f),	// color1 start
			Color::Grey(0.5f, 0.3f),	// color2 start
			Color::Grey(0, 0.6f),		// color1 end
			Color::Grey(0, 0.3f),		// color2 end
			3.0f,	0.2f,				// particle life time & percent fade in rate
			3, 5,						// particle start & end size
			.1f,		0.5f,			// particle start linear & angular speed
			12,	0.6f,					// emit rate & time
			.5f,	0.5f,				// emit size & overall randomness
			PI, PI, 0.04f,				// cone angles, gravity
			ParticleFlags::CastShadows
		);
		smokeEffectDef.particleSpeedRandomness = 1.0f;
		ParticleEmitter* emitter = new ParticleEmitter(smokeEffectDef, GetXFWorld(), NULL, effectSize);
		emitter->SetRenderGroup(RenderGroup_ForegroundEffect);
	}
	{
		// kick off explosion effects
		ParticleSystemDef explosionEffectDef = ParticleSystemDef::Build
		(
			Texture_Smoke,			// particle texture
			Color::Red(0.5f),			// color1 start
			Color::Yellow(0.5f),		// color2 start
			Color::Red(0),				// color1 end
			Color::Yellow(0),			// color2 end
			0.7f,		0.1f,			// particle life time & percent fade in rate
			3, 4,						// particle start & end size
			.4f,1,						// particle start linear & angular speed
			50,	0.2f,					// emit rate & time
			.4f,	0.5f,				// emit size & overall randomness
			PI, PI,	0,					// cone angles, gravity
			ParticleFlags::Additive
		);
		ParticleEmitter* emitter = new ParticleEmitter(explosionEffectDef, GetXFWorld(), NULL, effectSize);
		emitter->SetRenderGroup(RenderGroup_ForegroundAdditiveEffect);
	}
	{
		// play the test sound with a low frequency scale
		MakeSound(Sound_Test, 1, 0.4f);
	}

	Actor::Kill();
}

void Enemy::CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture)
{
	if (otherObject.IsPlayer())
	{
		// damage player and kill ourselves if we hit them
		// increase damage with size of enemy
		//const float damage = 5*size.x*size.y;
		//g_gameControlBase->GetPlayer()->ApplyDamage(damage);
		Kill();
	}
}