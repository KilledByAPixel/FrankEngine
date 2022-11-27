////////////////////////////////////////////////////////////////////////////////////////

/* coherent noise function over 1, 2 or 3 dimensions */
/* (copyright Ken Perlin) */
/* noise values produced are between -0.5 and 0.5) */

////////////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace PerlineNoise
{
	float noise1(float arg);
	float noise2(float vec[2]);
	float noise3(float vec[3]);

	inline float noise(float arg)			{ return noise1(arg); }
	inline float noise(const Vector2& v)	{ float p[2] = {v.x, v.y}; return noise2(p); }
	inline float noise(const Vector3& v)	{ float p[3] = {v.x, v.y, v.z}; return noise3(p); }
	inline Vector2 noiseVector2(float arg)	{ return Vector2(noise1(arg), noise1(arg + 76544.321f)); }
}