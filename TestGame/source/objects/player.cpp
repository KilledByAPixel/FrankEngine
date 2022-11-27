////////////////////////////////////////////////////////////////////////////////////////
/*
	Player
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "gameGlobals.h"
#include "player.h"
#include "gameObjects.h"
#include "weapons.h"

ConsoleCommandSimple(bool,		godMode,					false);
ConsoleCommandSimple(bool,		playerKill,					false);
ConsoleCommandSimple(bool,		playerAutopilot,			false);
ConsoleCommandSimple(float,		playerMoveAccel,			100);
ConsoleCommandSimple(float,		playerSideDrag,				0.3f);
ConsoleCommandSimple(float,		playerMaxSpeed,				20);
ConsoleCommandSimple(float,		playerMaxAngularSpeed,		60);
ConsoleCommandSimple(float,		playerRefillHealthRate,		0.5f);
ConsoleCommandSimple(float,		playerRefillHealthWaitTime,	1);
ConsoleCommandSimple(float,		playerTurnAcceleration,		150);

void Player::Init()
{
	SetRenderGroup(RenderGroup_Player);
	equippedWeapon = NULL;
	g_player = this;

	// basic object settings
	SetTeam(GameTeam_Player);
	lifeTimer.Set();
	light = NULL;

	radius = 0.4f;
	ResetHealth();
	lifeTimer.Set();
	deadTimer.Unset();
	
	{
		// hook up physics
		CreatePhysicsBody();
		AddFixtureCircle(radius, Physics::defaultDensity, 1.0f, 0.2f);
		GetPhysicsBody()->SetLinearDamping(5.0f);
		GetPhysicsBody()->SetAngularDamping(10.0f);
		SetHasGravity(false);
	}
	{	
		// set up the player's weapon
		equippedWeapon = new SpecialWeapon(Vector2(0,radius), this);
	}
	if (!g_gameControl->IsEditMode())
	{
		// trail effect
		ParticleSystemDef trailEffect = ParticleSystemDef::BuildRibbonTrail
		(
			Color::White(0.7f),			// color1 start
			Color::White(0.7f),			// color2 start
			Color::White(0),			// color1 end
			Color::White(0),			// color2 end
			0.3f,	0.0f,				// start end size
			2.5f, 0, 10,	0,			// particle time, speed, emit rate, emit time
			0, 0						// gravity, randomness
		);
		new ParticleEmitter(trailEffect, Vector2(0), this);
	}
	{
		// hook up a spot light to the gun
		light = new Light(Vector2(0,radius), this, 15, Color::White(0), true);
		light->SetOverbrightRadius(.4f);
		light->SetHaloRadius(.5f);
		light->SetConeAngles(0.3f, 0.6f);
	}
	{
		// hook up a light for when player is damaged
		damageLight = new Light(Vector2(0), this, 20, Color::White(0), true);
		damageLight->SetOverbrightRadius(.6f);
	}

	// reset transforms so player pops to new respawn positon rather then interpolating
	ResetLastWorldTransforms();
}

Player::~Player()
{
	g_player = NULL;
}

void Player::Update()
{
	if (!g_gameControl->IsGameplayMode() || IsDead())
		return;

	{
		// control movement
		Vector2 inputControl(0);
		if (g_input->IsDown(GB_MoveRight))	inputControl.x += 1;
		if (g_input->IsDown(GB_MoveLeft))	inputControl.x -= 1;
		if (g_input->IsDown(GB_MoveUp))		inputControl.y += 1;
		if (g_input->IsDown(GB_MoveDown))	inputControl.y -= 1;
		if (g_gameControl->IsUsingGamepad())
		{
			// use left stick movement control
			inputControl += g_input->GetGamepadLeftStick();
		}
		Vector2 inputControlDirection = inputControl.Normalize();

		if (!inputControl.IsZero())
		{
			// apply movement acceleration
			ApplyAcceleration(playerMoveAccel*inputControlDirection);
		}
		{	
			// apply force to counter side velocity, makes turning corners more square
			const Vector2 right = inputControl.RotateRightAngle();
			const Vector2 velocity = GetPhysicsBody()->GetLinearVelocity();
			const float dp = velocity.Dot(right);
			ApplyAcceleration(-right * dp * playerSideDrag);
		}
		{
			// cap player's max speed
			float maxSpeed = playerMaxSpeed;
			CapSpeed(maxSpeed);
			CapAngularSpeed(playerMaxAngularSpeed);
		}
	}
	{
		// automatically regenerate health
		if (timeSinceDamaged > playerRefillHealthWaitTime)
			RefillHealth(playerRefillHealthRate);
	}
	
	if (equippedWeapon)
	{
		// use aim angle function to calculate best aim trajectory
		bool usingGamepad = g_gameControl->IsUsingGamepad();
		float angle = FrankUtil::GetPlayerAimAngle(equippedWeapon->GetPosWorld(), GetVelocity(), equippedWeapon->GetWeaponDef().projectileSpeed);

		if (!usingGamepad)
		{
			// fixup issue of aiming inside the player
			const Vector2 mousePos = g_input->GetMousePosWorldSpace();
			const Vector2 deltaMousePos = mousePos - GetPosWorld();
			if (deltaMousePos.Length() < radius)
				angle = deltaMousePos.GetAngle();
		}

		// turn to aim weapon
		const Vector2 gamepadRightStick = g_input->GetGamepadRightStick();
		float deltaAngle = CapAngle(angle - GetAngleWorld());
		float acceleration = playerTurnAcceleration*deltaAngle;
		if (!usingGamepad || gamepadRightStick.Length() > 0.15f)
			ApplyAngularAcceleration(acceleration);
	
		// fire weapon
		if (g_input->IsDown(GB_Trigger1))
			equippedWeapon->SetTriggerIsDown();
	}

	if (light)
	{
		bool isOn = g_input->IsDown(GB_Trigger2);
		// turn on light when player pushes trigger2
		Color c = light->GetColor();
		c.a = isOn? 1.0f : 0.0f;
		light->SetColor(c);
		if (isOn)
		{
			// melt terrain
			Vector2 startPos = light->GetPosWorld();
			Vector2 direction = light->GetUpWorld();
			direction = direction.Rotate(RAND_BETWEEN(-.4f, .4f));
			Vector2 endPos = startPos + 10 * direction;
			SimpleRaycastResult raycastResult;
			GameObject* hitObject = g_physics->RaycastSimple(Line2(startPos, endPos), &raycastResult, this);
			if (hitObject && hitObject->IsStatic())
				g_terrain->DeformTile(raycastResult.point, direction, this, GMI_Normal);
		}
	}

	if (godMode)
	{
		// godmode debug command
		SetMaxHealth(1000);
		SetHealth(1000);
	}

	if (playerKill)
	{
		// debug command to test killing the player
		ApplyDamage(10000);
		playerKill = false;
	}

	if (playerAutopilot)
	{
		if (path.empty() || pathTimer.GetTimeElapsed() > 20.0f)
		{
			g_debugRender.Clear();
			const Vector2 randomPos = GetPosWorld() + Vector2::BuildRandomInCircle(30.0f, 15.0f);
			if (g_gameControl->GetPathFinding()->GetPath(GetPosWorld(), randomPos, path, true))
				pathTimer.Set();
		}
		else
		{
			if (!PathFindingBase::pathFindingDebug)
			{
				Vector2 lastPos = GetPosWorld();
				for (const Vector2& pos : path)
				{
					Line2(pos, lastPos).RenderDebug(Color::Red());
					lastPos = pos;
				}
				lastPos.RenderDebug(Color::Green(0.5f), TerrainTile::GetSize() / 2);
			}

			const Vector2 nextPos = path.front();
			const Vector2 deltaPos = nextPos - GetPosWorld();
			if (deltaPos.Length() < 0.5f)
				path.pop_front();
			else
				SetVelocity(10*deltaPos.Normalize());
		}
	}
	else
		path.clear();
}

void Player::UpdateTransforms()
{
	const XForm2 xf = GetXFPhysics();

	if (light)
	{
		// update light aim
		if (g_gameControl->IsUsingGamepad())
		{
			// in gamepad mode keep light aimed straight ahead
			light->SetAngleLocal(0);
		}
		else
		{
			// aim light directly at the mouse
			const Vector2 mousePos = g_input->GetMousePosWorldSpace();
			const Vector2 deltaPos = mousePos - xf.position;
			const float angle = deltaPos.GetAngle();
			light->SetAngleLocal(angle - xf.angle);
		}
		
		light->UpdateTransforms();
	}

	Actor::UpdateTransforms();
}

void Player::Render()
{
	if (IsDead())
		return;
	
	const XForm2 xf = GetXFInterpolated();
	if (DeferredRender::GetRenderPassIsShadow())
	{
		g_render->RenderQuad(xf, Vector2(radius), Color::Black(0.5f), Texture_Circle);
		return;
	}

	// draw a circle for the player
	DeferredRender::SpecularRenderBlock specularRenderBlock;
	g_render->RenderQuad(xf, Vector2(radius), Color::Grey(), Texture_Circle);
	g_render->RenderQuad(xf, Vector2(radius*0.9f), Color::White(), Texture_Circle);

	{
		// draw a smaller nub where the light and gun are
		DeferredRender::EmissiveRenderBlock emissiveRenderBlock;
		Color c = Color::White();
		if (DeferredRender::GetRenderPassIsEmissive())
		{
			// make numb emissive
			c = Lerp(light->GetColor().a, Color::Red(0.5f), Color::White());
		}

		g_render->RenderQuad(xf.TransformCoord(Vector2(0, radius*0.7f)), Vector2(radius*0.3f),		c, Texture_Circle);
	}

	if (!DeferredRender::GetRenderPassIsDeferred())
	{
		// overlay a colored halo to show players health
		DeferredRender::EmissiveRenderBlock emissiveRenderBlock;
		DeferredRender::TransparentRenderBlock transparentRenderBlock;

		const float healthPercent = GetHealthPercent();
		Color color;
		if (healthPercent < 0.6f)
			color = PercentLerp(healthPercent, 0.1f, 0.6f, Color::Red(), Color::Yellow(0.6f));
		else
			color = PercentLerp(healthPercent, 0.6f, 1.0f, Color::Yellow(0.6f), Color::White(0.0f));
		g_render->RenderQuad(xf, Vector2(1.8f*radius),	color, Texture_Dot);
		
		// also update our damage light
		damageLight->SetColor(color);
	}
}

void Player::ApplyDamage(float damage, GameObject* attacker, GameDamageType damageType)
{
	if (godMode)
		return;
	
	g_input->ApplyRumble(1.0f, 0.5f);

	timeSinceDamaged.Set();
	Actor::ApplyDamage(damage, attacker, damageType);
}

void Player::Kill()
{
	deadTimer.Set();
	DetachEmitters();

	if (equippedWeapon)
	{
		// remove weapon on death
		equippedWeapon->Destroy();
		equippedWeapon = NULL;
	}
	if (light)
	{
		// get rid of our lights
		DetachChild(*light);
		light->SetFadeTimer(0.1f, true);
		light = NULL;
		
		DetachChild(*damageLight);
		damageLight->SetFadeTimer(0.1f, true);
		damageLight = NULL;
	}
	{
		// kick off an explosion
		new Explosion(GetXFWorld(), 5, 5, 100, this);
	}
	{
		// create some debris
		Debris::CreateDebris(*this, 20);
	}
	{
		// kick off a light flash
 		Light* light = new Light(Vector2(0), this, 15, Color(1.0f, 0.95f, 0.8f, 1.0f), true);
		light->SetOverbrightRadius(0.8f);
		light->SetFadeTimer(0.8f, true, true);
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
			1.5f,	2.5f,				// particle start & end size
			0.1f,	0.5f,				// particle start linear & angular speed
			17,	0.5f,					// emit rate & time
			0.5f,	0.5f,				// emit size & overall randomness
			PI, PI, 0.04f,				// cone angles, gravity
			ParticleFlags::CastShadows
		);
		smokeEffectDef.particleSpeedRandomness = 1.0f;
		ParticleEmitter* emitter = new ParticleEmitter(smokeEffectDef, GetXFWorld());
		emitter->SetRenderGroup(RenderGroup_ForegroundEffect);
	}
	{
		// kick off explosion effects
		ParticleSystemDef explosionEffectDef = ParticleSystemDef::Build
		(
			Texture_Smoke,		// particle texture
			Color::Red(0.5f),		// color1 start
			Color::Yellow(0.5f),	// color2 start
			Color::Red(0),			// color1 end
			Color::Yellow(0),		// color2 end
			0.7f,	0.1f,			// particle life time & percent fade in rate
			0.7f,	1.5f,			// particle start & end size
			.4f,	0.5f,			// particle start linear & angular speed
			100,	0.3f,			// emit rate & time
			.3f,	0.5f,			// emit size & overall randomness
			PI,		PI,		0,		// cone angle, gravity
			ParticleFlags::Additive
		);
		ParticleEmitter* emitter = new ParticleEmitter(explosionEffectDef, GetXFWorld());
		emitter->SetRenderGroup(RenderGroup_ForegroundAdditiveEffect);
	}
	{
		// play the test sound with a low frequency scale
		MakeSound(Sound_Test, 1, 0.2f);
	}
}

bool Player::ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const
{
	return !IsDead() && Actor::ShouldCollide(otherObject, myFixture, otherFixture);
}