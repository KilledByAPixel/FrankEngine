////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Objects
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"
#include "gameObjects.h"
#include "weapons.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Basic objects from engine
*/
////////////////////////////////////////////////////////////////////////////////////////

GAME_OBJECT_DEFINITION(Light, Texture_Arrow, Color::White(0.5f));
GAME_OBJECT_DEFINITION(ParticleEmitter, Texture_Smoke, Color::White(0.5f));
GAME_OBJECT_DEFINITION(ObjectSpawner, Texture_Arrow, Color::Purple());
GAME_OBJECT_DEFINITION(MusicGameObject, Texture_Arrow, Color::Red(0.5f));
GAME_OBJECT_DEFINITION(TextDecal, Texture_Font1, Color::White());
GAME_OBJECT_DEFINITION(TileImageDecal, Texture_Arrow, Color::Cyan());
GAME_OBJECT_DEFINITION(TriggerBox, Texture_Invalid, Color::Green(0.1f));
GAME_OBJECT_DEFINITION(ShootTrigger, Texture_Arrow, Color::Green());
GAME_OBJECT_DEFINITION(MiniMapGameObject, Texture_Arrow, Color::Magenta());
GAME_OBJECT_DEFINITION(ConsoleCommandObject, Texture_Circle, Color::Yellow());
GAME_OBJECT_DEFINITION(SoundGameObject, Texture_Circle, Color::Cyan());

////////////////////////////////////////////////////////////////////////////////////////
/*
	Crate
*/
////////////////////////////////////////////////////////////////////////////////////////

GAME_OBJECT_DEFINITION(Crate, Texture_Crate, Color::White());

Crate::Crate(const GameObjectStub& stub) :
	Actor(stub)
{
	// create the physics
	CreatePhysicsBody();
	AddFixtureBox(stub.size);

	// set health based on size of object
	const Vector2 size = GetStubSize();
	const float health = 50*size.x*size.y;
	ResetHealth(health);
}

void Crate::Update()
{
	if (explosionTimer.HasElapsed())
		Explode();
}

void Crate::Render()
{
	const XForm2 xf = GetXFInterpolated();
	const Vector2 size = GetStubSize();
	g_render->RenderQuad(xf, size, Color::Grey(1, 0.7f), Texture_Crate);

	DeferredRender::EmissiveRenderBlock emissiveRenderBlock;
	DeferredRender::SpecularRenderBlock specularRenderBlock;
	const float healthPercent = GetHealthPercent();
	Color color = Lerp(healthPercent, Color::Cyan(), Color(0.8f, 1, 1));
	if (explosionTimer.IsSet())
		color = Lerp((float)explosionTimer, Color::Cyan(), Color::White());

	if (DeferredRender::GetRenderPassIsEmissive())
	{
		// make it more emissive when it is hit
		color.a = Lerp(healthPercent, 0.6f, 0.2f);
		if (explosionTimer.IsSet())
			color.a = Lerp((float)explosionTimer, 0.6f, 1.0f);
	}
	g_render->RenderQuad(xf, size*0.65f, color, Texture_Circle);
}

void Crate::Kill()
{
	if (!explosionTimer.IsSet())
	{
		// set an explosion timer instead of being immediatly destroyed
		explosionTimer.Set(RAND_BETWEEN(0.3f, 0.6f));
	}
}

void Crate::CollisionResult(GameObject& otherObject, const ContactResult& contactResult, b2Fixture* myFixture, b2Fixture* otherFixture)
{
	// check for collision damage
	const float impulse = contactResult.normalImpulse + contactResult.tangentImpulse;
	float damage = 0.7f*impulse / GetMass();
	float minDamage = 0.2f*GetMaxHealth();
	if (damage > minDamage)
		ApplyDamage(damage - minDamage);

	Actor::CollisionResult(otherObject, contactResult, myFixture, otherFixture);
}

void Crate::Explode()
{
	if (IsDestroyed())
		return;
	
	// scale effects based on size
	const Vector2 size = GetStubSize();
	const float effectSize = Min(size.x, size.y);

	{
		// do an explosion
		new Explosion(GetXFWorld(), effectSize, 6*effectSize, 0.5f*GetMaxHealth(), this);
	}
	{
		// create some debris
		Debris::CreateDebris(*this, (int)(15*Min(size.x, size.y)));
	}
	{
		// kick off a light flash
		Light* light = new Light(GetPosWorld(), NULL, effectSize*20, Color(0.0f, 1.0f, 1.0f, 0.9f), true);
		light->SetOverbrightRadius(effectSize*1.5f);
		light->SetFadeTimer(0.6f, true, true);
	}
	{
		// kick off smoke
		ParticleSystemDef smokeEffectDef = ParticleSystemDef::Build
		(
			Texture_Smoke,			// particle texture
			Color::Grey(0.5f, 0.8f),	// color1 start
			Color::Grey(0.5f, 0.4f),	// color2 start
			Color::Grey(0, 0.8f),		// color1 end
			Color::Grey(0, 0.4f),		// color2 end
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
			Color(0.2f, 1.0f, 1, 0.5f),	// color1 start
			Color(0.2f, 0.2f, 1, 0.5f),	// color2 start
			Color(0.2f, 1.0f, 1, 0),	// color1 end
			Color(0.2f, 0.2f, 1, 0),	// color2 end
			0.6f,		0.1f,			// particle life time & percent fade in rate
			2, 2,						// particle start & end size
			.4f,1,						// particle start linear & angular speed
			33,	0.2f,					// emit rate & time
			.4f,	0.5f,				// emit size & overall randomness
			PI, PI,	0,					// cone angles, gravity
			ParticleFlags::Additive		// gravity, flags
		);
		ParticleEmitter* emitter = new ParticleEmitter(explosionEffectDef, GetXFWorld(), NULL, effectSize);
		emitter->SetRenderGroup(RenderGroup_ForegroundAdditiveEffect);
	}
	{
		// play the test sound with a frequency scale based on volume
		const float frequencyPercent = Percent(size.x*size.y, 10.0f, 70.0f);
		const float frequency = Lerp(frequencyPercent, 0.7f, 0.4f);
		MakeSound(Sound_Test, 1, frequency);
	}

	Actor::Destroy();
}

////////////////////////////////////////////////////////////////////////////////////////
/*
	StressTest
*/
////////////////////////////////////////////////////////////////////////////////////////

GAME_OBJECT_DEFINITION(StressTest, Texture_Invalid, Color::Red(0.2f));

float StressTest::highTime = 0;

StressTest::StressTest(const GameObjectStub& stub) : GameObject(stub)
{
	// make it appear below stuff
	SetRenderGroup(RenderGroup_Low);
}
	
void StressTest::Update()
{
	const float time = activeTimer;
	if (time > highTime)
		highTime = time;
}

void StressTest::TriggerActivate(bool activate, GameObject* activator, int data)
{
	if (activeTimer.IsSet() == activate)
		return;

	if (activate)
		activeTimer.Set();
	else
		activeTimer.Unset();
}

void StressTest::Render()
{
	WCHAR buffer[256];
	if (activeTimer.IsSet() && !g_player->IsDead())
	{
		const float time = activeTimer;
		if (time > highTime)
			swprintf_s(buffer, 256, L"Stress Test Time\n%.2f\nNew High!", time);
		else
			swprintf_s(buffer, 256, L" Stress TestTime\n%.2f\nHigh Time\n%0.2f", time, highTime);
	}
	else
		swprintf_s(buffer, 256, L"Stress Test Time\n0\nHigh Time\n%0.2f", highTime);
	
	DeferredRender::EmissiveRenderBlock erb;
	FontFlags flags = FontFlags(FontFlag_CenterX | FontFlag_CenterY | FontFlag_NoNormals);
	g_gameFont->Render(buffer, GetPosWorld(), 2.2f, Color::White(), flags);
}