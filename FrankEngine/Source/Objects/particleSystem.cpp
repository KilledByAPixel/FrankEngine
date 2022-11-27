////////////////////////////////////////////////////////////////////////////////////////
/*
	Particle Emitter
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../terrain/terrain.h"
#include "../objects/particleSystem.h"

int ParticleEmitter::totalEmitterCount = 0;
int ParticleEmitter::defaultRenderGroup = -20;
int ParticleEmitter::defaultAdditiveRenderGroup = -10;
DWORD Particle::simpleVertsLastColor = 0;
bool Particle::simpleVertsNeedsCap = false;

bool ParticleEmitter::enableParticles = true;
ConsoleCommand(ParticleEmitter::enableParticles, particleEnable);

bool ParticleEmitter::particleDebug = false;
ConsoleCommand(ParticleEmitter::particleDebug, particleDebug);

bool ParticleEmitter::particleAlphaEffectsEnable = true;
ConsoleCommand(ParticleEmitter::particleAlphaEffectsEnable, particleAlphaEffectsEnable);

float ParticleEmitter::shadowRenderAlpha = 0.6f;
ConsoleCommand(ParticleEmitter::shadowRenderAlpha, particleShadowRenderAlpha);

///////////////////////////////////////////////////////////////////////////////////////////////////////////

ParticleEmitter::ParticleEmitter(const ParticleSystemDef& _systemDef, const XForm2& xf, GameObject* _parent, float scale) :
	GameObject(xf, _parent),
	systemDef(_systemDef)
{
	ASSERT(systemDef.IsValid());
	++totalEmitterCount;
	lifeTimer.Set();
	systemDef.Scale(scale);
	SetPaused(systemDef.HasFlags(ParticleFlags::StartPaused), false);

	if (systemDef.HasFlags(ParticleFlags::TrailLine|ParticleFlags::TrailRibbon) && !paused && systemDef.emitRate > 0)
	{
		// create 2 particles so there is something we can draw and it connects to the start position
		const XForm2 xfStart = systemDef.HasFlags(ParticleFlags::LocalSpace)? XForm2::Identity() : GetXFWorld();
		AddParticle(xfStart);
		AddParticle(xfStart);
		emitRateTimer -= 1 / systemDef.emitRate;
	}

	// render additive particles on top of normal particles and both under the terrain 
	SetRenderGroup(systemDef.HasFlags(ParticleFlags::Additive)? defaultAdditiveRenderGroup : defaultRenderGroup);
}

ParticleEmitter::~ParticleEmitter()
{
	for (list<Particle*>::iterator it = particles.begin(); it != particles.end(); ++it)
		DeleteParticle(*it);
	--totalEmitterCount;
}

void ParticleEmitter::WarmUp(float time)
{
	while (time >= GAME_TIME_STEP) 
	{
		time -= GAME_TIME_STEP;
		Update();
	}
}

void ParticleEmitter::SpawnParticle()
{
	if (systemDef.HasFlags(ParticleFlags::LocalSpace))
		AddParticle(XForm2::Identity());
	else
		AddParticle(GetXFWorld());
}

 void ParticleEmitter::StreamOut() 
{ 
	// make it die out
	if (createdFromStub)
		systemDef.emitLifeTime = lifeTimer;
		 
	Kill(); 
}

 void ParticleEmitter::StreamIn() 
{ 
	// hack: bring it back to life
	if (createdFromStub)
	{
		UnDestroy();
		systemDef.emitLifeTime = 0;
	}
}
 
void ParticleEmitter::SetPaused(bool _paused,  bool fade)
{
	if (paused == _paused)
		return;

	paused = _paused;
	if (!fade)
		pauseFade = paused? 0.0f : 1.0f;
	if (!paused && pauseFade == 0)
		SpawnParticle();
}

void ParticleEmitter::Update()
{
	if (!enableParticles)
	{
		for (list<Particle*>::iterator it = particles.begin(); it != particles.end(); ++it)
			DeleteParticle(*it);
		particles.clear();
		if (IsDead())
		{
			Destroy();
			return;
		}
	}

	// check if we are past our life time
	if (IsDead())
	{
		// self destruct if we are past life time and have no particles
		if (particles.empty())
			Destroy();
	}
	else if (systemDef.emitRate > 0 && (pauseFade > 0 || !paused))
	{
		float pauseFadeNext = CapPercent(pauseFade + (paused? -0.05f : 0.05f));

		// emit new particles
		emitRateTimer += GAME_TIME_STEP;
		while (emitRateTimer >= 0)
		{
			emitRateTimer -= 1 / systemDef.emitRate;
			if (pauseFadeNext == 0)
				pauseFade = 0;	// must spawn alpha particle to complete fade

			// update sub step interpolation
			const float interpolation = CapPercent(emitRateTimer / GAME_TIME_STEP);

			if (systemDef.HasFlags(ParticleFlags::LocalSpace))
				AddParticle(XForm2::Identity(), interpolation * GAME_TIME_STEP);
			else
				AddParticle(GetXFInterpolated(interpolation), interpolation * GAME_TIME_STEP);
		}

		if (pauseFadeNext != 0)
			pauseFade = pauseFadeNext;
	}
	
	Vector2 lastParticlePos;
	bool firstParticle = true;

	// update particles
	for (list<Particle*>::iterator it = particles.begin(); it != particles.end();) 
	{
		Particle& particle = **it;

		if (particle.IsDead())
		{ 
			bool canRemove = true;
			if (systemDef.HasFlags(ParticleFlags::TrailLine|ParticleFlags::TrailRibbon) && it != --particles.end())
			{
				// hack: peek at the next particle
				// we must wait for the next one to die to avoid a glich when we are destroyed
				const Particle& nextParticle = **next(it);
				if (!nextParticle.IsDead())
					canRemove = false;
			}

			if (canRemove)
			{
				// remove dead particle
				it = particles.erase(it);
				DeleteParticle(&particle);
				continue;
			}
		}

		particle.Update();
		
		if (particleDebug)
		{
			const XForm2 xfEmitter = systemDef.HasFlags(ParticleFlags::LocalSpace)? GetXFWorld() : XForm2::Identity();
			const Vector2 particlePos = (particle.GetXForm()*xfEmitter).position;
			if (!firstParticle)
				Line2(particlePos, lastParticlePos).RenderDebug(Color::White(1 - particle.GetLifetimePercent()));
			lastParticlePos = particlePos;
			firstParticle = false;
		}

		++it;
	}
	
	if (systemDef.HasFlags(ParticleFlags::TrailLine|ParticleFlags::TrailRibbon))
	{
		if ((!IsDead() || setTrailEnd) && particles.size() > 1 && pauseFade > 0)
		{
			setTrailEnd = false;
			Particle& particle = *particles.back();
			if (systemDef.HasFlags(ParticleFlags::LocalSpace))
			{
				particle.xf.position = Vector2::Zero();
				particle.xfDelta.position = Vector2::Zero();
			}
			else
			{
				particle.xf.position = GetXFWorld().position;
				particle.xfDelta.position = GetXFDelta().position;
			}

			if (IsDead() && !systemDef.HasFlags(ParticleFlags::LocalSpace))
			{
				particle.xf.position += g_cameraBase->GetXFDelta().position;
				particle.xfDelta.position += g_cameraBase->GetXFDelta().position;
			}
		}
	}
	
	if (particleDebug && particles.size() > 1)
	{
		Particle* particle = particles.back();
		Line2(GetPosWorld(), lastParticlePos).RenderDebug(Color::White(1 - particle->GetLifetimePercent()));
		
		const float percentTimeLeft = 1 - (systemDef.emitLifeTime > 0 ? lifeTimer / systemDef.emitLifeTime : 0);
		Circle(GetPosWorld(), systemDef.emitSize).RenderDebug(Color::Red(percentTimeLeft), 0, 12);
		GetXFWorld().RenderDebug(Color::White(percentTimeLeft), 0.1f*systemDef.emitSize);
		if (emitBox.GetSize().LengthManhattan() > 0)
			emitBox.RenderDebug(GetXFWorld(), Color::Red(percentTimeLeft));
			
		wstringstream s;
		s << particles.size();
		g_debugRender.RenderText(GetPosWorld(), s.str(), Color::White());
	}

	CapSpeed(maxSpeed);
}

// some effects may need this for when there is no parent or the parent dies
void ParticleEmitter::SetTrailEnd(const Vector2& trailEndPos)
{
	DetachParent();
	Kill();
	SetPosLocal(trailEndPos);
	setTrailEnd = true;

	if (particles.size() > 1)
	{
		// set the first particle to be emiter position
		Particle& particle = *particles.back();
		particle.xfDelta.position += trailEndPos - particle.xf.position;
		particle.xf.position = trailEndPos;// - g_cameraBase->GetXFDelta().position;
	}
	if (particles.size() > 1)
	{
		// set the first particle to be emiter position
		Particle& particle = *particles.back();
		particle.xf.position = trailEndPos;
	}
}
	
void ParticleEmitter::RenderInternal(bool allowAdditive)
{
	if (!enableParticles)
		return;

	FrankProfilerEntryDefine(L"ParticleEmitter::Render()", Color::White(), 6);
	CDXUTPerfEventGenerator( DXUT_PERFEVENTCOLOR, L"ParticleEmitter::Render()" );
	const XForm2 xf = GetXFInterpolated();
	const XForm2 xfEmitter = systemDef.HasFlags(ParticleFlags::LocalSpace) ? xf : XForm2::Identity();
	//xfEmitter.RenderDebug(Color::White(), 1, 1);

	if (systemDef.HasFlags(ParticleFlags::TrailLine))
	{
		if (particles.size() > 1)
		{
			// set additive rendering for simple verts
			g_render->SetSimpleVertsAreAdditive(allowAdditive && systemDef.HasFlags(ParticleFlags::Additive));

			Particle::simpleVertsNeedsCap = true;
			for (list<Particle*>::iterator it = particles.begin(); it != particles.end(); ++it) 
				(**it).RenderTrail(xfEmitter);
   
			// connect to the end and cap it off
			const Vector2 endPos = xf.position;
			g_render->AddPointToLineVerts(endPos, Particle::simpleVertsLastColor);
			g_render->CapLineVerts(endPos);
		}
	}
	else if (systemDef.HasFlags(ParticleFlags::TrailRibbon))
	{
		if (particles.size() > 1)
		{
			// set additive rendering for simple verts
			g_render->SetSimpleVertsAreAdditive(allowAdditive && systemDef.HasFlags(ParticleFlags::Additive));

			Vector2 previousPos = (**particles.begin()).GetXFInterpolated(xfEmitter).position;
			if (particles.size() > 1)
			{
				// if ribbion has more then 1 particle, create a fake previous pos based on where the next particle is
				// this used to orient the ribbon properly
				const Vector2 nextPos = (**(++particles.begin())).GetXFInterpolated(xfEmitter).position;
				previousPos = previousPos - (nextPos - previousPos);
			}

			Particle::simpleVertsNeedsCap = true;
			for (list<Particle*>::iterator it = particles.begin(); it != particles.end(); ++it) 
				(**it).RenderRibbon(xfEmitter, previousPos);
		
			if (IsDead() && systemDef.HasFlags(ParticleFlags::CameraSpace))
			{
				// for camera space systems that are no longer spawning particles, cap on the last particle instead of the emitter
				// this fixes a tiny glitch where the end point will see to stick in world space since only particles update their camera space offsets
				g_render->CapTriVerts(previousPos);
			}
			else
			{
				// connect to the end and cap it off
				const Vector2 endPos = xf.position;
				g_render->AddPointToTriVerts(endPos, Particle::simpleVertsLastColor);
				g_render->CapTriVerts(endPos);
				//endPos.RenderDebug();
			}
		}
	}
	else
	{
		const bool transparent = systemDef.HasFlags(ParticleFlags::Transparent);
		const bool additive = systemDef.HasFlags(ParticleFlags::Additive);
		DeferredRender::AdditiveRenderBlock additiveRenderBlock(allowAdditive && additive);
		DeferredRender::EmissiveRenderBlock emissiveRenderBlock(allowAdditive && additive);
		DeferredRender::TransparentRenderBlock transparentRenderBlock(transparent);

		for (list<Particle*>::iterator it = particles.begin(); it != particles.end(); ++it) 
			(**it).Render(xfEmitter);
	}

	{
		const bool transparent = systemDef.HasFlags(ParticleFlags::Transparent);
		const bool additive = systemDef.HasFlags(ParticleFlags::Additive);
		DeferredRender::AdditiveRenderBlock additiveRenderBlock(allowAdditive && additive);
		DeferredRender::EmissiveRenderBlock emissiveRenderBlock(allowAdditive && additive);
		DeferredRender::TransparentRenderBlock transparentRenderBlock(transparent);

		if (HasPhysics())
		{
			Color color = systemDef.colorStart1;
			float a = PercentLerp(lifeTimer, systemDef.emitLifeTime - 1, systemDef.emitLifeTime, 1.0f, 0.0f);
			color = color.ScaleAlpha(a);

			b2Fixture& fixture = *GetPhysicsBody()->GetFixtureList();
			switch (fixture.GetType())
			{
				case b2Shape::e_circle:
				{
					b2CircleShape* circle = (b2CircleShape*)fixture.GetShape();
					g_render->DrawSolidCircle(XForm2(circle->m_p)*xf, Vector2(circle->m_radius), color);
					break;
				}

				case b2Shape::e_polygon:
				{
					b2PolygonShape* poly = (b2PolygonShape*)fixture.GetShape();
					int32 vertexCount = poly->m_count;
					b2Assert(vertexCount <= b2_maxPolygonVertices);
					b2Vec2 vertices[b2_maxPolygonVertices];

					for (int32 i = 0; i < vertexCount; ++i)
						vertices[i] = xf.TransformCoord(poly->m_vertices[i]);

					g_render->DrawSolidPolygon((Vector2*)vertices, vertexCount, color);
					break;
				}
			}
		}
	}
}

void ParticleEmitter::ManualRender()
{
	if (DeferredRender::GetRenderPassIsShadow())
	{
		// todo: add flag for particles blocking vision
		if (DeferredRender::GetRenderPassIsVision())
			return; 

		if (DeferredRender::GetRenderPassIsLight())
		{
			if (systemDef.HasFlags(ParticleFlags::CastShadows|ParticleFlags::Transparent))
			{
				if (shadowRenderAlpha <= 0)
					return;

				RenderInternal(false);
			}
		}
		else
		{
			if (systemDef.HasFlags(ParticleFlags::CastShadowsDirectional))
			{
				if (shadowRenderAlpha <= 0)
					return;

				DeferredRender::BackgroundRenderBlock backgroundRenderBlock;
				RenderInternal(false);
			}
		}
	}
	else
		RenderInternal();
}

void ParticleEmitter::Render()
{
	if (systemDef.HasFlags(ParticleFlags::ManualRender))
		return;

	ManualRender();
}

void ParticleEmitter::MakePhysical(float radius, float _maxSpeed, bool box)
{
	CreatePhysicsBody();
	if (box)
		AddFixtureBox(Vector2(radius));
	else
		AddFixtureCircle(radius);
	SetPhysicsFilter(-111);
	maxSpeed = _maxSpeed;
}

bool ParticleEmitter::ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const
{ 
	return otherFixture && !otherFixture->IsSensor() && otherObject.ShouldCollideParticles(); 
}

void ParticleEmitter::CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture)
{
	const float maxLife = RAND_BETWEEN(0.5f, 1.0f);
	if (systemDef.emitLifeTime == 0 || systemDef.emitLifeTime > lifeTimer + maxLife)
	{
		// make it die out quicker
		systemDef.emitLifeTime = lifeTimer + RAND_BETWEEN(0.0f, maxLife);
		//SetPaused(true);
	}
}

inline void ParticleEmitter::AddParticle(const XForm2& xf, float startTime)
{
	if (!enableParticles)
		return;

	// random position within the emitter radius
	const Vector2 particlePosition = xf.TransformCoord(Vector2::BuildRandomInBox(emitBox)) + Vector2::BuildRandomInCircle(systemDef.emitSize);

	ASSERT(systemDef.particleSpeedRandomness >= 0 && systemDef.particleSpeedRandomness <= 1);
	ASSERT(systemDef.particleAngularSpeedRandomness >= 0 && systemDef.particleAngularSpeedRandomness <= 1);

	// world space direction is randomized in cone
	float angle = xf.angle + RAND_BETWEEN(-systemDef.emitConeAngle, systemDef.emitConeAngle);
	const Vector2 direction = Vector2::BuildFromAngle(angle);

	// randomize speed of particle
	const float speed = systemDef.particleSpeed * (1 + systemDef.particleSpeedRandomness*RAND_BETWEEN(-1.0f,1.0f));
	float angularSpeed = systemDef.particleAngularSpeed * (1 + systemDef.particleAngularSpeedRandomness*RAND_BETWEEN(-1.0f,1.0f));
	if (!systemDef.HasFlags(ParticleFlags::DontFlipAngular))
		angularSpeed *= RAND_SIGN;

	Particle *particle = NewParticle();
	if (!particle)
		return;
	
	const float particleAngle = systemDef.HasFlags(ParticleFlags::DontUseEmitAngle) ? xf.angle : angle;
	particle->Init(this, XForm2(particlePosition, particleAngle), direction*speed, angularSpeed, startTime);
	if (pauseFade < 1)
	{
		particle->colorStart.a *= pauseFade;
		particle->colorDelta.a *= pauseFade;
	}

	particles.push_back(particle);
}

///////////////////////////////////////////////////////////
// particle member functions
///////////////////////////////////////////////////////////

void Particle::Init(const ParticleEmitter* _emitter, const XForm2& _xf, const Vector2& _velocity, float _angularSpeed, float _startTime)
{
	cachedFrame = 0;
	systemDef = &_emitter->GetDef();
	emitter = _emitter;
	xf = _xf;
	xfDelta = XForm2::Identity();
	velocity = _velocity;
	angularSpeed = _angularSpeed;
	time = _startTime;

	// randomize particle values
	lifeTime = systemDef->particleLifeTime * (1 + systemDef->particleLifeTimeRandomness*RAND_BETWEEN(-1.0f,1.0f));

	// randomly flip the texture for more randomness
	const float randomFlip = systemDef->HasFlags(ParticleFlags::TrailLine|ParticleFlags::TrailRibbon|ParticleFlags::DontFlip)? 1 : (float)RAND_SIGN;
	const float sizeRandomness = (1 + systemDef->particleSizeRandomness*RAND_BETWEEN(-1, 1));
	sizeStart = randomFlip * systemDef->particleSizeStart * sizeRandomness;
	sizeEnd = randomFlip * systemDef->particleSizeEnd * sizeRandomness;
	xf.angle += RAND_BETWEEN(-systemDef->particleConeAngle, systemDef->particleConeAngle);

	colorStart = Color::RandBetween(systemDef->colorStart1, systemDef->colorStart2);
	const Color colorEnd = Color::RandBetween(systemDef->colorEnd1, systemDef->colorEnd2);
	colorDelta = (colorEnd - colorStart);
}

inline void Particle::Update()
{
	// update time
	time += GAME_TIME_STEP;

	const XForm2 xfLast = xf;

	// update gravity
	if (systemDef->particleGravity)
		velocity -= GAME_TIME_STEP * systemDef->particleGravity * g_gameControlBase->GetGravity(xf.position);

	// update transform
	xf.position += velocity * GAME_TIME_STEP;
	xf.angle += angularSpeed * GAME_TIME_STEP;

	// camera space particles move along with the camera
	if (systemDef->HasFlags(ParticleFlags::CameraSpace))
		xf *= g_cameraBase->GetXFDelta();
	
	xfDelta = xf - xfLast;

	if (ParticleEmitter::particleDebug)
	{
		const XForm2 xfEmitter = systemDef->HasFlags(ParticleFlags::LocalSpace)? emitter->GetXFWorld() : XForm2::Identity();
		const XForm2 xfWorld = GetXForm() * xfEmitter;

		const float percent = GetLifetimePercent();
		const float size = Max(0.01f, Lerp(percent, fabs(sizeStart), fabs(sizeEnd)));
		g_debugRender.RenderBox(xfWorld, Vector2(size), Color::White(1 - percent));
	}
}

inline void Particle::Render(const XForm2& xfParent)
{
	if (cachedFrame != g_gameControlBase->GetRenderFrameCount())
	{
		// cache data for faster rendering
		cachedFrame = g_gameControlBase->GetRenderFrameCount();

		// caluculate percent of particle life time
		const float percent = GetLifetimePercent();

		// cache the particle info for this render frame
		const XForm2 xfWorld = GetXFInterpolated(xfParent);
		cachedSize = sizeStart + percent * (sizeEnd - sizeStart);

		cachedColor = colorStart + percent * colorDelta;
		if (percent < systemDef->particleFadeInTime)
			cachedColor.a *= (percent / systemDef->particleFadeInTime);

		cachedPos1 = xfWorld.position;
		cachedAngle = xfWorld.angle;
	}

	if (!g_cameraBase->CameraTest(cachedPos1, fabs(cachedSize) * ROOT_2))
		return;

	const XForm2 xfWorld(cachedPos1, cachedAngle);
	const Vector2 size(cachedSize);
	Color color = cachedColor;
	color.a *= emitter->alphaScale;
	if (DeferredRender::GetRenderPassIsShadow())
		color.a *= ParticleEmitter::shadowRenderAlpha;

	if (systemDef->HasFlags(ParticleFlags::DisableAlphaBlend) && ParticleEmitter::particleAlphaEffectsEnable)
		DXUTGetD3D9Device()->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	if (systemDef->HasFlags(ParticleFlags::FakeAlpha) && ParticleEmitter::particleAlphaEffectsEnable)
	{
		// todo: cache this caps for performance?
		const D3DCAPS9* caps = DXUTGetD3D9DeviceCaps();
		if (caps->AlphaCmpCaps & D3DPCMPCAPS_GREATEREQUAL)
		{
			// simulate alpha by rendering with full alpha but using greater test to fade
			// can make some effects like blood look better
			const DWORD a = (DWORD)(255 * CapPercent(1 - color.a));
			color.a = 1;
			DXUTGetD3D9Device()->SetRenderState(D3DRS_ALPHAREF, a);
			DXUTGetD3D9Device()->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
			DXUTGetD3D9Device()->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
		}
	}

	g_render->RenderQuad(xfWorld, size, color, systemDef->texture);
	
	if (systemDef->HasFlags(ParticleFlags::FakeAlpha) && ParticleEmitter::particleAlphaEffectsEnable)
		DXUTGetD3D9Device()->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

	if (systemDef->HasFlags(ParticleFlags::DisableAlphaBlend) && ParticleEmitter::particleAlphaEffectsEnable)
		DXUTGetD3D9Device()->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
}

inline void Particle::RenderTrail(const XForm2& xfParent)
{
	if (cachedFrame != g_gameControlBase->GetRenderFrameCount())
	{
		// cache data for faster rendering
		cachedFrame = g_gameControlBase->GetRenderFrameCount();

		// caluculate percent of particle life time
		const float percent = GetLifetimePercent();

		cachedColor = colorStart + percent * colorDelta;
		if (percent < systemDef->particleFadeInTime)
			cachedColor.a *= (percent / systemDef->particleFadeInTime);

		cachedPos1 = GetXFInterpolated(xfParent).position;
	}

	Color color = cachedColor;
	color.a *= emitter->alphaScale;
	if (DeferredRender::GetRenderPassIsShadow())
		color.a *= ParticleEmitter::shadowRenderAlpha;

	if (Particle::simpleVertsNeedsCap)
	{
		g_render->CapLineVerts(cachedPos1);
		Particle::simpleVertsNeedsCap = false;
	}

	const DWORD colorDword = color;
	g_render->AddPointToLineVerts(cachedPos1, colorDword);
	simpleVertsLastColor = colorDword;
}

inline void Particle::RenderRibbon(const XForm2& xfParent, Vector2& previousPos)
{
	if (cachedFrame != g_gameControlBase->GetRenderFrameCount())
	{
		// cache data for faster rendering
		cachedFrame = g_gameControlBase->GetRenderFrameCount();

		// caluculate percent of particle life time
		const float percent = GetLifetimePercent();

		cachedColor = colorStart + percent * colorDelta;
		if (percent < systemDef->particleFadeInTime)
			cachedColor.a *= (percent / systemDef->particleFadeInTime);

		const XForm2 xfWorld = GetXFInterpolated(xfParent);
		const float size = sizeStart + percent * (sizeEnd - sizeStart);

		const Vector2 offset = size*(xfWorld.position - previousPos).Normalize().RotateRightAngle();

		cachedPos1 = xfWorld.position - offset;
		cachedPos2 = xfWorld.position + offset;
		//cachedPos1.RenderDebug();
	}
	
	// pass out the position of the particle
	previousPos = 0.5f*(cachedPos1 + cachedPos2);
	
	Color color = cachedColor;
	color.a *= emitter->alphaScale;
	if (DeferredRender::GetRenderPassIsShadow())
		color.a *= ParticleEmitter::shadowRenderAlpha;

	if (Particle::simpleVertsNeedsCap)
	{
		g_render->CapTriVerts(cachedPos1);
		Particle::simpleVertsNeedsCap = false;
	}

	const DWORD colorDword = color;
	g_render->AddPointToTriVerts(cachedPos1, colorDword);
	g_render->AddPointToTriVerts(cachedPos2, colorDword);
	simpleVertsLastColor = colorDword;
}

///////////////////////////////////////////////////////////
// particle memory management
///////////////////////////////////////////////////////////

// global particle pools
list<Particle*> ParticleEmitter::freeParticleList;
Particle ParticleEmitter::particlePool[maxParticles];

void ParticleEmitter::InitParticleSystem()
{
	// prep the free particle list
	freeParticleList.clear();
	for(int i = 0; i < maxParticles; ++i)
		freeParticleList.push_back(&particlePool[i]);
}

Particle* ParticleEmitter::NewParticle()
{
	if (freeParticleList.empty())
		return NULL; // ran out of particles

	Particle* particle = freeParticleList.back();
	freeParticleList.pop_back();
	return particle;
}

void ParticleEmitter::DeleteParticle(Particle* particle)
{
	freeParticleList.push_back(particle);
}

///////////////////////////////////////////////////////////
// ParticleSystemDef functions
///////////////////////////////////////////////////////////

// full constructor for particle systems
ParticleSystemDef::ParticleSystemDef
(
	TextureID _texture,				// texture of particle (you must set this)
	const Color& _colorStart1,				// color at start of life
	const Color& _colorStart2,				// color at start of life
	const Color& _colorEnd1,				// color at end of life
	const Color& _colorEnd2,				// color at end of life

	float _particleLifeTime,				// amount of time particle can live for, (0 = render exactaly 1 frame)
	float _particleFadeInTime,				// how quickly to fade in particle (0 = instant, 1 = lifeTime)
	float _particleSizeStart,				// size at start of life
	float _particleSizeEnd,					// size at end of life
	float _particleSpeed,					// start speed for particles
	float _particleAngularSpeed,			// start angular speed for particles
	float _emitRate,						// rate in seconds per particle (higher = slower)
	float _emitLifeTime,					// how long to emit for before dying (0 = forever)
	float _emitSize,						// size of the emit radius (0 = point emitter)

	float _particleSpeedRandomness,			// randomness for particle speed
	float _particleAngularSpeedRandomness,	// randomness for particle angular speed
	float _particleSizeRandomness,			// randomness for particle size
	float _particleLifeTimeRandomness,		// randomness for particle life time

	float _emitConeAngle,					// angle in radians of the emit cone in local space
	float _particleConeAngle,				// angle in radians of the particle's initial rotation cone
	float _particleGravity,					// how much does gravity effect particles
	ParticleFlags _particleFlags			// list of flags for the system
) :
	texture(_texture),
	colorStart1(_colorStart1),
	colorStart2(_colorStart2),
	colorEnd1(_colorEnd1),
	colorEnd2(_colorEnd2),
	particleLifeTime(_particleLifeTime),
	particleFadeInTime(_particleFadeInTime),
	particleSizeStart(_particleSizeStart),
	particleSizeEnd(_particleSizeEnd),
	particleSpeed(_particleSpeed),
	particleAngularSpeed(_particleAngularSpeed),
	emitRate(_emitRate),
	emitLifeTime(_emitLifeTime),
	emitSize(_emitSize),
	emitConeAngle(_emitConeAngle),
	particleConeAngle(_particleConeAngle),
	particleGravity(_particleGravity),
	particleSpeedRandomness(_particleSpeedRandomness),
	particleAngularSpeedRandomness(_particleAngularSpeedRandomness),
	particleSizeRandomness(_particleSizeRandomness),
	particleLifeTimeRandomness(_particleLifeTimeRandomness),
	particleFlags(_particleFlags)
{
	ASSERT(particleLifeTime > 0 && particleSpeed >= 0);
	ASSERT(particleSizeStart >= 0 && particleSizeEnd >= 0 && emitSize >= 0);
	ASSERT(emitConeAngle >= 0 && emitConeAngle <= 2*PI);
	ASSERT(particleConeAngle >= 0 && particleConeAngle <= 2*PI);
	ASSERT(particleSpeedRandomness >= 0 && particleAngularSpeedRandomness >= 0);
	ASSERT(particleSizeRandomness >= 0 && particleLifeTimeRandomness >= 0);
}
	
// constructor for normal particle systems
ParticleSystemDef ParticleSystemDef::Build
(
	TextureID _texture,		// texture of particle (you must set this)
	const Color& _colorStart1,		// color at start of life
	const Color& _colorStart2,		// color at start of life
	const Color& _colorEnd1,		// color at end of life
	const Color& _colorEnd2,		// color at end of life

	float _particleLifeTime,		// amount of time particle can live for, (0 = render exactaly 1 frame)
	float _particleFadeInTime,		// how quickly to fade in particle (0 = instant, 1 = lifeTime)
	float _particleSizeStart,		// size at start of life
	float _particleSizeEnd,			// size at end of life
	float _particleSpeed,			// start speed for particles
	float _particleAngularSpeed,	// start angular speed for particles
	float _emitRate,				// rate in seconds per particle (higher = slower)
	float _emitLifeTime,			// how long to emit for before dying (0 = forever)
	float _emitSize,				// size of the emit radius (0 = point emitter)

	float _randomness,				// use the same randomness for all settings
	float _emitConeAngle,			// angle in radians of the emit cone in local space
	float _particleConeAngle,		// angle in radians of the particle's initial rotation cone
	float _particleGravity,			// how much does gravity effect particles
	ParticleFlags _particleFlags	// list of flags for the system
)
{
	return ParticleSystemDef
	(
		_texture,
		_colorStart1, _colorStart2,
		_colorEnd1, _colorEnd2,
		_particleLifeTime, _particleFadeInTime,
		_particleSizeStart, _particleSizeEnd,
		_particleSpeed, _particleAngularSpeed,
		_emitRate, _emitLifeTime, _emitSize, 
		_randomness, _randomness, _randomness, _randomness,
		_emitConeAngle, _particleConeAngle,
		_particleGravity, _particleFlags
	);
}

// constructor for trail particles
ParticleSystemDef ParticleSystemDef::BuildLineTrail
(
	const Color& _colorStart1,		// color at start of life
	const Color& _colorStart2,		// color at start of life
	const Color& _colorEnd1,		// color at end of life
	const Color& _colorEnd2,		// color at end of life
	float _particleLifeTime,		// amount of time particle can live for, (0 = render exactaly 1 frame)
	float _particleSpeed,			// particle speed
	float _emitRate,				// how often to emit new particles
	float _emitLifeTime,			// how long to emit for before dying (0 = forever)
	float _particleGravity,			// how much does gravity effect particles
	ParticleFlags _particleFlags	// list of flags for the system (will be or'd with trail line flag)
)
{
	return ParticleSystemDef
	(
		Texture_Invalid,
		_colorStart1, _colorStart2,
		_colorEnd1, _colorEnd2,
		_particleLifeTime, 0,
		0, 0,
		_particleSpeed, 0,
		_emitRate, _emitLifeTime, 0, 
		0, 0, 0, 0, PI, PI,
		_particleGravity,
		_particleFlags|ParticleFlags::TrailLine
	);
}

// constructor for trail particles
ParticleSystemDef ParticleSystemDef::BuildRibbonTrail
(
	const Color& _colorStart1,		// color at start of life
	const Color& _colorStart2,		// color at start of life
	const Color& _colorEnd1,		// color at end of life
	const Color& _colorEnd2,		// color at end of life
	float _particleSizeStart,		// size at start of life
	float _particleSizeEnd,			// size at end of life
	float _particleLifeTime,		// amount of time particle can live for, (0 = render exactaly 1 frame)
	float _particleSpeed,			// particle speed
	float _emitRate,				// how often to emit new particles
	float _emitLifeTime,			// how long to emit for before dying (0 = forever)
	float _particleGravity,			// how much does gravity effect particles
	float _randomness,				// use the same randomness for all settings
	ParticleFlags _particleFlags	// list of flags for the system (will be or'd with trail line flag)
)
{
	return ParticleSystemDef
	(
		Texture_Invalid,
		_colorStart1, _colorStart2,
		_colorEnd1, _colorEnd2,
		_particleLifeTime, 0,
		_particleSizeStart, _particleSizeEnd,
		_particleSpeed, 0,
		_emitRate, _emitLifeTime, 0, 
		_randomness, _randomness, _randomness, 0, PI, 0,
		_particleGravity,
		_particleFlags|ParticleFlags::TrailRibbon
	);
}

// lerp between two particle systems
ParticleSystemDef ParticleSystemDef::Lerp(float percent, const ParticleSystemDef& p1, const ParticleSystemDef& p2)
{
	return ParticleSystemDef
	(
		p1.texture,
		FrankMath::Lerp(percent, p1.colorStart1,						p2.colorStart1),					
		FrankMath::Lerp(percent, p1.colorStart2,						p2.colorStart2),
		FrankMath::Lerp(percent, p1.colorEnd1,							p2.colorEnd1),
		FrankMath::Lerp(percent, p1.colorEnd2,							p2.colorEnd2),
		FrankMath::Lerp(percent, p1.particleLifeTime,					p2.particleLifeTime),
		FrankMath::Lerp(percent, p1.particleFadeInTime,					p2.particleFadeInTime),
		FrankMath::Lerp(percent, p1.particleSizeStart,					p2.particleSizeStart),
		FrankMath::Lerp(percent, p1.particleSizeEnd,					p2.particleSizeEnd),
		FrankMath::Lerp(percent, p1.particleSpeed,						p2.particleSpeed),
		FrankMath::Lerp(percent, p1.particleAngularSpeed,				p2.particleAngularSpeed),
		FrankMath::Lerp(percent, p1.emitRate,							p2.emitRate),
		FrankMath::Lerp(percent, p1.emitLifeTime,						p2.emitLifeTime),
		FrankMath::Lerp(percent, p1.emitSize,							p2.emitSize),
		FrankMath::Lerp(percent, p1.particleSpeedRandomness,			p2.particleSpeedRandomness),
		FrankMath::Lerp(percent, p1.particleAngularSpeedRandomness,		p2.particleAngularSpeedRandomness),
		FrankMath::Lerp(percent, p1.particleSizeRandomness,				p2.particleSizeRandomness),
		FrankMath::Lerp(percent, p1.particleLifeTimeRandomness,			p2.particleLifeTimeRandomness),
		FrankMath::Lerp(percent, p1.emitConeAngle,						p2.emitConeAngle),
		FrankMath::Lerp(percent, p1.particleConeAngle,					p2.particleConeAngle),
		FrankMath::Lerp(percent, p1.particleGravity,					p2.particleGravity),
		p1.particleFlags
	);
}

ParticleSystemDef& ParticleSystemDef::Scale(float scale)
{
	// roughly scale up the size of the particle system
	particleSizeStart *= scale;
	particleSizeEnd *= scale;
	particleSpeed *= scale;
	emitSize *= scale;
	return *this; 
}

ParticleSystemDef& ParticleSystemDef::SetColors(const Color& color1, const Color& color2, float endAlpha)
{ 
	colorStart1 = colorEnd1 = color1; 
	colorStart2 = colorEnd2 = color2; 
	colorEnd1.a = colorEnd2.a = endAlpha; 
	return *this; 
}

ParticleSystemDef& ParticleSystemDef::SetRandomValues( float lifeTimeRandomness, float sizeRandomness, float speedRandomness, float angularSpeedRandomness )
{
	particleLifeTimeRandomness		= lifeTimeRandomness;
	particleSizeRandomness			= sizeRandomness;
	particleSpeedRandomness			= speedRandomness;
	particleAngularSpeedRandomness	= angularSpeedRandomness;
	return *this;
}

///////////////////////////////////////////////////////////
// stub functions
///////////////////////////////////////////////////////////

// constructor to build particle def from attributes string
ParticleSystemDef ParticleSystemDef::BuildFromAttributes(const char* attributes, float emitLifeTime, int* _renderGroup, bool* _warmUp)
{
	float life = 1.0f;
	float fade = 0.2f;
	float sizeStart = 1.0f;
	float sizeEnd = 2.0f;
	float speed = 1.0f;
	float angular = 0.5f;
	float emitRate = 10;
	float emitSize = 0;
	float randomness = 0.2f;
	float emitConeDegrees = 180;
	float particleConeDegrees = 180;
	TextureID texture = Texture_Smoke;
	UINT flags = 0;
	float gravity = 0;
	Color c1 = Color::White(.5);
	Color c2 = Color(-1, -1, -1, -1);
	int renderGroup = -100000;
	bool warmUp = true;

	ObjectAttributesParser parser(attributes);
	parser.ParseValue(life);
	parser.ParseValue(fade);
	parser.ParseValue(sizeStart);
	parser.ParseValue(sizeEnd);
	parser.ParseValue(speed);
	parser.ParseValue(angular);
	parser.ParseValue(emitRate);
	parser.ParseValue(emitSize);
	parser.ParseValue(randomness);
	parser.ParseValue(emitConeDegrees);
	parser.ParseValue(particleConeDegrees);
	parser.ParseValue(flags);
	parser.ParseValue(gravity);
	parser.ParseValue(c1);
	parser.ParseValue(c2);
	parser.ParseValue(renderGroup);
	parser.ParseValue(warmUp);
	if (parser.SkipToMarker('#'))
		parser.ParseTextureName(texture);
	
	if (c2.r == -1)
		c2 = c1; // use c1 as c2
	randomness = Cap(randomness, 0.0f, 1.0f);
	texture = (TextureID)Cap((int)(texture), 0, MAX_TEXTURE_COUNT-1);
	life = Min(life, 10.0f);

	if (_renderGroup)
		*_renderGroup = renderGroup;
	if (_warmUp)
		*_warmUp = warmUp;

	return ParticleSystemDef
	(
		texture,
		c1, c2,
		c1.ZeroAlpha(), c2.ZeroAlpha(),
		life, fade,
		sizeStart, sizeEnd,
		speed, angular,
		emitRate, emitLifeTime, emitSize, 
		randomness, randomness, randomness, randomness,
		DegreesToRadians(emitConeDegrees), DegreesToRadians(particleConeDegrees),
		gravity, ParticleFlags(flags)
	);
}


ParticleEmitter::ParticleEmitter(const GameObjectStub& stub) : 
	GameObject(stub),
	createdFromStub(true)
{
	++totalEmitterCount;
	lifeTimer.Set();
	
	int renderGroup;
	bool warmUp;
	systemDef = ParticleSystemDef::BuildFromAttributes(stub.attributes, 0, &renderGroup, &warmUp);
	emitBox = Box2AABB(-stub.size, stub.size);

	SetPaused(systemDef.HasFlags(ParticleFlags::StartPaused), false);
	if (g_gameControlBase->IsEditPreviewMode() || warmUp)
		WarmUp(Min(systemDef.particleLifeTime, 4.0f));
	if (renderGroup > -100000)
		SetRenderGroup(renderGroup);
}

void ParticleEmitter::StubRender(const GameObjectStub& stub, float alpha)
{
	const ParticleSystemDef systemDef = ParticleSystemDef::BuildFromAttributes(stub.attributes);
	
	DeferredRender::AdditiveRenderBlock additiveRenderBlock(systemDef.HasFlags(ParticleFlags::Additive));
	
	if (g_editor.IsObjectEdit() && (!g_editor.GetObjectEditor().IsSelected(stub) || systemDef.emitRate <= 0))
		g_render->RenderQuad(stub.xf, stub.size, systemDef.colorStart1.ScaleAlpha(alpha), TextureID(systemDef.texture));
	if (systemDef.emitRate <= 0)
		return;
	
	// slow animate speed when not in edit mode
	float animateSpeed = 1.0f;
	if (!g_gameControlBase->IsObjectEditMode() || !stub.IsThisTypeSelected())
		animateSpeed = 0.1f;
	
	// draw a preview of the particle system
	const float inverseEmitRate = 1 / systemDef.emitRate;
	const Vector2 objectGravity = g_gameControlBase->GetGravity(stub.xf.position);
	float cyclePercent = 0, cycleCount = 0;
	cyclePercent = modf((animateSpeed * g_gameControlBase->GetResetTime() / inverseEmitRate), &cycleCount);

	for (float time = cyclePercent*inverseEmitRate; time < systemDef.particleLifeTime; time += inverseEmitRate, cycleCount -= 1)
	{
		FrankRand::SaveSeedBlock randomSeedBlock(int(stub.handle) + 33*int(cycleCount));
		const float lifePercent = Percent(time, 0.0f, systemDef.particleLifeTime);

		// get size
		const float randomFlip = systemDef.HasFlags(ParticleFlags::TrailLine|ParticleFlags::TrailRibbon|ParticleFlags::DontFlip)? 1 : (float)RAND_SIGN;
		const float sizeRandomness = (1 + systemDef.particleSizeRandomness*RAND_BETWEEN(-1, 1));
		const float sizeStart = randomFlip * systemDef.particleSizeStart * sizeRandomness;
		const float sizeEnd = randomFlip * systemDef.particleSizeEnd * sizeRandomness;
		Vector2 size = Vector2(Lerp(lifePercent, sizeStart, sizeEnd));

		// get speed	
		const float speed = systemDef.particleSpeed * (1 + systemDef.particleSpeedRandomness*RAND_BETWEEN(-1.0f,1.0f));
		float angularSpeed = systemDef.particleAngularSpeed * (1 + systemDef.particleAngularSpeedRandomness*RAND_BETWEEN(-1.0f,1.0f));
		if (!systemDef.HasFlags(ParticleFlags::DontFlipAngular))
			angularSpeed *= RAND_SIGN;

		// get angle
		const float angle = stub.xf.angle + RAND_BETWEEN(-systemDef.particleConeAngle, systemDef.particleConeAngle) + angularSpeed * time;

		// get color
		float a = alpha;
		if (lifePercent < systemDef.particleFadeInTime)
			a *= (lifePercent / systemDef.particleFadeInTime);
		const Color cStart = Color::RandBetween(systemDef.colorStart1, systemDef.colorStart2);
		const Color cEnd = Color::RandBetween(systemDef.colorEnd1, systemDef.colorEnd2);
		const Color c = Lerp(lifePercent, cStart, cEnd).ScaleAlpha(a);
		
		// get emit direction
		const float emitAngle = stub.xf.angle + RAND_BETWEEN(-systemDef.emitConeAngle, systemDef.emitConeAngle);
		const Vector2 emitDirection = Vector2::BuildFromAngle(emitAngle);
		Vector2 emitPos = Vector2::BuildRandomInCircle(systemDef.emitSize) + Vector2::BuildRandomInBox(Box2AABB(-stub.size, stub.size));

		// calculate particle position
		Vector2 pos = stub.xf.TransformCoord(emitPos) + emitDirection * speed * time - objectGravity * (0.5f * systemDef.particleGravity * time * time);
		g_render->RenderQuad(XForm2(pos, angle), size, c, TextureID(systemDef.texture));
	}

	if (stub.IsThisTypeSelected())
	{
		// only show preview when this object type is selected
		// draw emmter size
		if (systemDef.emitConeAngle > 0 && systemDef.emitConeAngle < 180)
			g_render->DrawCone(stub.xf, systemDef.emitSize, DegreesToRadians(systemDef.emitConeAngle), Color::Magenta(0.5f));
		g_render->DrawCircle(stub.xf, systemDef.emitSize, Color::Magenta(0.5f));
	}
}