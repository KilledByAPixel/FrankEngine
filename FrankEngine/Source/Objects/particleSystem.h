////////////////////////////////////////////////////////////////////////////////////////
/*
	Particle Emitter
	Copyright 2013 Frank Force - http://www.frankforce.com

	- controls particle system for a 2d game
	- simple, fast, easy to use particle system
	- does not keep references to particle or emitter definitions
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../objects/gameObject.h"

enum class ParticleFlags
{
	None					= 0,
	Additive				= (1 << 0),  // should particles use additive blending?
	CastShadows				= (1 << 1),  // should this particle system be used for drawing shadows
	Transparent				= (1 << 2),  // this particle is transparent
	StartPaused				= (1 << 3),  // should emitter be paused on startup
	CastShadowsDirectional	= (1 << 4),  // this particle casts shadows only for the background
	DontFlip				= (1 << 5),  // should we not randomly flip the sprite
	DontFlipAngular			= (1 << 6),  // should we not randomly flip the angular speed
	FakeAlpha				= (1 << 7),  // render with full alpha but using alpha test to fade
	DisableAlphaBlend		= (1 << 8),  // no alpha blending
	LocalSpace				= (1 << 9),  // are particles in the emitter's local space?
	CameraSpace				= (1 << 10), // are particles in camera space?
	TrailLine				= (1 << 11), // should particles render as a line trail?
	TrailRibbon				= (1 << 12), // should particles render as a ribbon trail?
	ManualRender			= (1 << 13), // this particle is only rendered manually
	IsPersistant			= (1 << 14), // can emitter stream out
	DontUseEmitAngle		= (1 << 15), // don't pass along emit angle to particles
};

inline ParticleFlags operator | (ParticleFlags a, ParticleFlags b) { return static_cast<ParticleFlags>(static_cast<int>(a) | static_cast<int>(b)); }
inline ParticleFlags operator & (ParticleFlags a, ParticleFlags b) { return static_cast<ParticleFlags>(static_cast<int>(a) & static_cast<int>(b)); }

struct ParticleSystemDef
{
	// constructor for normal particle systems
	static ParticleSystemDef Build
	(
		TextureID _texture,						// texture of particle (you must set this)
		const Color& _colorStart1,					// color at start of life
		const Color& _colorStart2,					// color at start of life
		const Color& _colorEnd1,					// color at end of life
		const Color& _colorEnd2,					// color at end of life
		float _particleLifeTime,					// amount of time particle can live for, (0 = render exactaly 1 frame)
		float _particleFadeInTime,					// how quickly to fade in particle (0 = instant, 1 = lifeTime)
		float _particleSizeStart,					// size at start of life
		float _particleSizeEnd,						// size at end of life
		float _particleSpeed,						// start speed for particles
		float _particleAngularSpeed,				// start angular speed for particles
		float _emitRate,							// rate in seconds per particle (higher = slower)
		float _emitLifeTime,						// how long to emit for before dying (0 = forever)
		float _emitSize,							// size of the emit radius (0 = point emitter)
		float _randomness = 0.2f,					// use the same randomness for all settings
		float _emitConeAngle = PI,					// angle in radians of the emit cone in local space
		float _particleConeAngle = PI,				// angle in radians of the particle's initial rotation cone
		float _particleGravity = 0,					// how much does gravity effect particles
		ParticleFlags _particleFlags = ParticleFlags::None	// list of flags for the system
	);

	// constructor for trail particles
	static ParticleSystemDef BuildLineTrail
	(
		const Color& _colorStart1,				// color at start of life
		const Color& _colorStart2,				// color at start of life
		const Color& _colorEnd1,				// color at end of life
		const Color& _colorEnd2,				// color at end of life
		float _particleLifeTime,				// amount of time particle can live for, (0 = render exactaly 1 frame)
		float _particleSpeed = 0,				// particle speed
		float _emitRate = 10,					// particles per second (higher = faster)
		float _emitLifeTime = 0,				// how long to emit for before dying (0 = forever)
		float _particleGravity = 0,				// how much does gravity effect particles
		ParticleFlags _particleFlags = ParticleFlags::None	// list of flags for the system (will be or'd with trail line flag)
	);

	// constructor for trail particles
	static ParticleSystemDef BuildRibbonTrail
	(
		const Color& _colorStart1,				// color at start of life
		const Color& _colorStart2,				// color at start of life
		const Color& _colorEnd1,				// color at end of life
		const Color& _colorEnd2,				// color at end of life
		float _particleSizeStart,				// size at start of life
		float _particleSizeEnd,					// size at end of life
		float _particleLifeTime,				// amount of time particle can live for, (0 = render exactaly 1 frame)
		float _particleSpeed,					// particle speed
		float _emitRate = 10,					// particles per second (higher = faster)
		float _emitLifeTime = 0,				// how long to emit for before dying (0 = forever)
		float _particleGravity = 0,				// how much does gravity effect particles
		float _randomness = 0.2f,				// use the same randomness for all settings
		ParticleFlags _particleFlags = ParticleFlags::None	// list of flags for the system (will be or'd with trail line flag)
	);

	static ParticleSystemDef Lerp(float percent, const ParticleSystemDef& p1, const ParticleSystemDef& p2);

	// default constructor, just set emit rate to -1 to signify the emitter is invalid
	ParticleSystemDef() { emitRate = -1; }

	// full constructor for particle systems
	ParticleSystemDef
	(
		TextureID _texture,						// texture of particle (you must set this)
		const Color& _colorStart1,					// color at start of life
		const Color& _colorStart2,					// color at start of life
		const Color& _colorEnd1,					// color at end of life
		const Color& _colorEnd2,					// color at end of life

		float _particleLifeTime,					// amount of time particle can live for, (0 = render exactaly 1 frame)
		float _particleFadeInTime,					// how quickly to fade in particle (0 = instant, 1 = lifeTime)
		float _particleSizeStart,					// size at start of life
		float _particleSizeEnd,						// size at end of life
		float _particleSpeed,						// start speed for particles
		float _particleAngularSpeed,				// start angular speed for particles
		float _emitRate,							// particles per second (higher = faster)
		float _emitLifeTime,						// how long to emit for before dying (0 = forever)
		float _emitSize,							// size of the emit radius (0 = point emitter)

		float _particleSpeedRandomness,				// randomness for particle speed
		float _particleAngularSpeedRandomness,		// randomness for particle angular speed
		float _particleSizeRandomness,				// randomness for particle size
		float _particleLifeTimeRandomness,			// randomness for particle life time

		float _emitConeAngle,						// angle in radians of the emit cone in local space
		float _particleConeAngle,					// angle in radians of the particle's initial rotation cone
		float _particleGravity,						// how much does gravity effect particles
		ParticleFlags _particleFlags				// list of flags for the system
	);
	
	static ParticleSystemDef BuildFromAttributes(const char* attributes, float emitLifeTime = 0, int* renderGroup = NULL, bool* warmup = NULL);
	ParticleSystemDef& Scale(float scale);
	ParticleSystemDef& SetColors(const Color& color1, const Color& color2, float endAlpha = 0);
	ParticleSystemDef& SetRandomValues(float lifeTimeRandomness, float sizeRandomness, float speedRandomness, float angularSpeedRandomness);
	ParticleSystemDef& SetSpeedRandomness(float speedRandomness) { particleSpeedRandomness = speedRandomness; return *this; }

	bool IsValid() const						{ return emitRate >= 0;  }
	bool HasFlags(ParticleFlags flags) const	{ return bool(particleFlags & flags); }
	void SetFlags(ParticleFlags flags)			{ particleFlags = particleFlags | flags; }
	void UnsetFlags(ParticleFlags flags)		{ particleFlags = particleFlags & static_cast<ParticleFlags>(~static_cast<int>(flags)); }
	
public:

	TextureID texture;						// texture of particle

	Color colorStart1 = Color::White();			// color at start of life
	Color colorStart2 = Color::White();			// color at start of life
	Color colorEnd1 = Color::White(0);			// color at end of life
	Color colorEnd2 = Color::White(0);			// color at end of life

	float particleLifeTime = 1;					// amount of time particle can live for, (0 = render for 1 frame)
	float particleLifeTimeRandomness = 0.2f;	// percent randomness for lifetime
	float particleFadeInTime = 0.2f;			// how quickly to fade in particle (0 = instant, 1 = lifeTime)

	float particleSizeStart = 1;				// size at start of life
	float particleSizeEnd = 2;					// size at end of life
	float particleSizeRandomness = 0.2f;		// percent randomness for size

	float particleSpeed = 1;					// start speed for particles
	float particleSpeedRandomness = 0.2f;		// percent randomness for particles start speed
	float particleAngularSpeed = 0.5f;			// start angular speed for particles
	float particleAngularSpeedRandomness = 0.2f;// percent randomness for particles start angular speed
	float particleGravity = 0;					// how much does gravity effect particles

	float emitRate = 10;						// particles per second (higher = faster)
	float emitLifeTime = 0;						// how long to emit for before dying (0 = forever)
	float emitSize = 0;							// size of the emit radius (0 = point emitter)

	float particleConeAngle = PI;				// how much to rotate angle by
	float emitConeAngle = PI;					// angle in radians of the emit cone in local space (0 = directional, PI = omnidirectional)
	ParticleFlags particleFlags;				// list of flags for the system
};

class Particle;

class ParticleEmitter : public GameObject
{
public:

	friend Particle;
	
	ParticleEmitter(const GameObjectStub& stub);
	ParticleEmitter(const ParticleSystemDef& _systemDef, const XForm2& xf = XForm2::Identity(), GameObject* _parent = NULL, float scale = 1.0f);
	~ParticleEmitter() override;

	bool IsParticleEmitter() const override { return true; }
	void WasDetachedFromParent() override { Kill(); }
	bool IsDead() const override { return (systemDef.emitLifeTime > 0 && lifeTimer >= systemDef.emitLifeTime); }
	void TriggerActivate(bool activate, GameObject* activator = NULL, int data = 0)	override { SetPaused(!activate); }

	void Kill()
	{
		// hack: set emitter to be past end of life
		if (systemDef.emitLifeTime == 0)
			systemDef.emitLifeTime = 0.0001f;
		lifeTimer.SetTimeElapsed(systemDef.emitLifeTime + 1.0f);
	}

	void SetPaused(bool _paused, bool fade = true);
	bool IsPaused() const { return paused; }

	// some effects may need this for when there is no parent or the parent dies
	void SetTrailEnd(const Vector2& trailEndPos);

	// resart the emitter
	void ResetLifeTime() { lifeTimer.Set(); }

	// get our copy of the system def
	ParticleSystemDef& GetDef() { return systemDef; }
	const ParticleSystemDef& GetDef() const { return systemDef; }

	// let the emitter run a bit to warm up the system
	void WarmUp(float time);

	// force it to spawn a particle
	void SpawnParticle();

	// persistant objects will not be removed or put to sleep when outside the window
	bool ShouldStreamOut() const override { return systemDef.HasFlags(ParticleFlags::IsPersistant) ? false : GameObject::ShouldStreamOut(); }

	// just kill the particle system when streamed out instead of destroying it
	void StreamOut() override;

	// restart particle system if created from stub
	void StreamIn() override;

	// particle systems can be rendered manually
	void ManualRender();

	// set overall alpha to multiple particle alpha by
	void SetAlphaScale(float _alphaScale) { alphaScale = _alphaScale; }
	
	// set emitter size for box shapped emitters
	void SetEmitBox(const Box2AABB& _emitBox) { emitBox = _emitBox; }
	Box2AABB GetEmitBox() const { return emitBox; }

	// physical emitters only collide with static objects
	void MakePhysical(float radius = 0.05f, float maxSpeed = 10.0f, bool box = false);
	bool ShouldCollide(const GameObject& otherObject, const b2Fixture* myFixture, const b2Fixture* otherFixture) const override;
	void CollisionAdd(GameObject& otherObject, const ContactEvent& contactEvent, b2Fixture* myFixture, b2Fixture* otherFixture);
	void SetMaxSpeed(float _maxSpeed) { maxSpeed = _maxSpeed; }

	// add a new particle to the emitter
	void AddParticle(const XForm2& xf, float startTime = 0);

public: // stub functions
	
	static void StubRender(const GameObjectStub& stub, float alpha);
	static WCHAR* StubDescription() { return L"Create custom particle system with given params"; }
	static WCHAR* StubAttributesDescription() { return L"life fade size0 size1 speed angular emitRate emitSize randomness cone particleCone flags gravity r0 g0 b0 a0 r1 g1 b1 a1 renderGroup warm #texture"; }
	static char* StubAttributesDefault() { return "1 .2 1 2 1 1 10 0 .2 180 180 0 0 1 1 1 .5 1 1 1 .5"; }

public: // static functions

	static void InitParticleSystem();
	static void EnableParticles(bool enable) { enableParticles = enable; }
	static bool AreParticlesEnabled() { return enableParticles; }

	static int GetTotalParticleCount() { return maxParticles - freeParticleList.size(); }
	static int GetTotalEmitterCount() { return totalEmitterCount; }

	static bool enableParticles;
	static bool particleDebug;
	static bool particleAlphaEffectsEnable;
	static int defaultRenderGroup;
	static int defaultAdditiveRenderGroup;
	static float particleStopRadius;		// does and on screen test with this radius and won't emit particles from offscreen (0 = always spawn)
	static float shadowRenderAlpha;

protected:

	static Particle* NewParticle();
	static void DeleteParticle(Particle* particle);

	void Render() override;
	void Update() override;
	void RenderInternal(bool allowAdditive = true);

private:

	bool setTrailEnd = false;
	bool paused = false;
	GameTimer lifeTimer;
	float pauseFade = 1;
	float emitRateTimer = 0;
	float alphaScale = 1;
	float maxSpeed = 0;
	bool createdFromStub = false;
	ParticleSystemDef systemDef;
	Box2AABB emitBox = Box2AABB(Vector2(0));

	list<Particle*> particles;

	static int totalEmitterCount;
	
	static const UINT32 maxParticles = 2000;
	static list<Particle*> freeParticleList;
	static Particle particlePool[maxParticles];
};

class Particle : private Uncopyable
{
public:

	friend ParticleEmitter;

	Particle() {}
	void Init(const ParticleEmitter* _emitter, const XForm2& _xf, const Vector2& _velocity, float _angularSpeed, float _startTime);

	void Update();

	void Render(const XForm2& xfParent);
	void RenderTrail(const XForm2& xfParent);
	void RenderRibbon(const XForm2& xfParent, Vector2& previousPos);

	bool IsDead() const { return time >= lifeTime + GAME_TIME_STEP; }

	XForm2 GetXFInterpolated(const XForm2& xfParent) const { return xf.Interpolate(xfDelta, g_interpolatePercent) * xfParent; }
	XForm2 GetXForm() const { return xf; }
	float GetLifetimePercent() const { return CapPercent((lifeTime == 0) ? 0 : (time - g_interpolatePercent * GAME_TIME_STEP) / lifeTime); }

private:

	XForm2 xf;
	XForm2 xfDelta;

	Vector2 velocity;
	float angularSpeed;
	float time;

	float lifeTime;
	float sizeStart;
	float sizeEnd;
	Color colorStart;
	Color colorDelta;

	// cache some particle data for faster rendering
	UINT cachedFrame;
	Color cachedColor;
	Vector2 cachedPos1;
	Vector2 cachedPos2;
	float cachedAngle;
	float cachedSize;

	const ParticleSystemDef* systemDef;
	const ParticleEmitter* emitter;

	static DWORD simpleVertsLastColor;
	static bool simpleVertsNeedsCap;
};
