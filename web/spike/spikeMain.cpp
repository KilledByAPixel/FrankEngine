////////////////////////////////////////////////////////////////////////////////////////
/*
	Phase 0 spike: prove Box2D + WebGL2 + a fixed-timestep main loop run under
	Emscripten. Throwaway scaffolding - never ships, deleted when the real web
	build lands. See trunk/docs/plans/2026-07-21-piroot-web-demo.md
*/
////////////////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cmath>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include "Box2D/Box2D.h"

// The vendored Box2D is patched (b2Island.cpp) to pull per-body gravity/damping
// from the engine (physics.cpp:802) - stub with stock Box2D behavior here
b2Vec2 FrankEngineGetGravity(b2Body* b)			{ return b->GetWorld()->GetGravity(); }
float FrankEngineGetLinearDamping(b2Body* b)	{ return b->GetLinearDamping(); }
float FrankEngineGetAngularDamping(b2Body* b)	{ return b->GetAngularDamping(); }

static b2World* world;
static b2Body* box;
static double lastTime = 0;
static double accumulator = 0;
static int frameCount = 0;
static const float timeStep = 1.0f / 60.0f;	// GAME_TIME_STEP

static void MainLoop()
{
	double now = emscripten_get_now() / 1000.0;
	double delta = now - lastTime;
	lastTime = now;
	if (delta > 0.25)
		delta = 0.25;	// clamp after tab-switch stalls

	// same accumulator shape as GameControlBase::Update
	accumulator += delta;
	while (accumulator >= timeStep)
	{
		world->Step(timeStep, 24, 8);	// 24 velocity iterations, matching Piroot
		accumulator -= timeStep;
	}

	// pulse the clear color so a live render loop is visually obvious
	float t = 0.5f + 0.5f * (float)sin(now * 2);
	glClearColor(0.1f, 0.2f * t, 0.4f * t, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	if (++frameCount % 60 == 0)
		printf("box y = %.3f\n", box->GetPosition().y);
}

int main()
{
	EmscriptenWebGLContextAttributes attrs;
	emscripten_webgl_init_context_attributes(&attrs);
	attrs.majorVersion = 2;	// WebGL2 required - fail loudly if unavailable
	attrs.minorVersion = 0;
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attrs);
	if (ctx <= 0)
	{
		printf("FAILED: could not create WebGL2 context (%d)\n", (int)ctx);
		return 1;
	}
	emscripten_webgl_make_context_current(ctx);

	world = new b2World(b2Vec2(0, -10));

	b2BodyDef groundDef;
	groundDef.position.Set(0, -10);
	b2Body* ground = world->CreateBody(&groundDef);
	b2PolygonShape groundShape;
	groundShape.SetAsBox(50, 10);
	ground->CreateFixture(&groundShape, 0);

	b2BodyDef boxDef;
	boxDef.type = b2_dynamicBody;
	boxDef.position.Set(0, 20);
	box = world->CreateBody(&boxDef);
	b2PolygonShape boxShape;
	boxShape.SetAsBox(1, 1);
	box->CreateFixture(&boxShape, 1.0f);

	printf("spike start: box falling from y=20\n");
	lastTime = emscripten_get_now() / 1000.0;
	emscripten_set_main_loop(MainLoop, 0, 1);
	return 0;
}
