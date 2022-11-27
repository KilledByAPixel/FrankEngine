////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Math Lib
	Copyright 2013 Frank Force - http://www.frankforce.com
*/
////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "../core/frankMath.h"

////////////////////////////////////////////////////////////////////////////////////////
/*
	Math Lib Globals
*/
////////////////////////////////////////////////////////////////////////////////////////

// random seed
unsigned int FrankRand::randomSeed = 0;

float FrankRand::GetGaussian(float variance, float mean)
{
	// Marsaglia Polar Method
	// http://en.wikipedia.org/wiki/Marsaglia_polar_method

	static bool hasSpare = false;
	static float spare;
 
	if (hasSpare)
	{
		hasSpare = false;
		return mean + variance * spare;
	}
 
	hasSpare = true;
	float u, v, s;

	do
	{
		u = RAND_BETWEEN(-1.0f, 1.0f);
		v = RAND_BETWEEN(-1.0f, 1.0f);
		s = u * u + v * v;
	}
	while (s >= 1 || s <= 0);
 
	s = sqrtf(-2.0f * logf(s) / s);
	spare = v * s;
	return mean + variance * u * s;
}

void FrankMath::FormatTime(float time, WCHAR* text, bool tryToSkipHours)
{
	int hours = int(time / (60*60));
	time -= hours * (60*60);
	int minutes = int(time / (60));
	time -= minutes * (60);
	int seconds = int(time);
	
	if (tryToSkipHours && hours == 0)
		swprintf_s(text, 256, L"%02d:%02d", minutes, seconds);
	else
		swprintf_s(text, 256, L"%02d:%02d:%02d", minutes, seconds, hours);
}

void FrankMath::GetParabola(const Vector2& p1, const Vector2& p2, float& a, float& b, float& c)
{
	if ((p1 - p2).IsZero())
	{
		a = b = c = 0;
		return;
	}

	a = -((p2.y - p1.y) / (p2.x - p1.x)) * (p1.x + p2.x) + p1.y + p2.y;
	a /= 2 * (Square(p1.x) - Square(p2.x)) / (p2.x - p1.x) + Square(p1.x);
	b = a * (Square(p1.x) - Square(p2.x)) + p2.y - p1.y;
	b /= p2.x - p1.x;
	c = p1.y - a * Square(p1.x) - b * p1.x;
}
 
long long GameTimer::interpolatedTime = 0;
long long GameTimer::gameTime = 0;
const float GameTimer::conversion = 1000000;
const long long GameTimer::invalidTime = LLONG_MAX;
long long GamePauseTimer::gameTime = 0;
const long long GamePauseTimer::invalidTime = LLONG_MAX;
const float GamePauseTimer::conversion = 1000000;

const Color Color::HSVtoRGB() const
{
	float x = r;
	float y = g;
	float z = b;

	ASSERT(x >= 0 && y >= 0 && y <=1 && z >= 0 && z <= 1);

	// adapted from http://www.cs.rit.edu/~ncs/color/t_convert.html
	Color cRGB;
	cRGB.a = a;

	if (y == 0) 
	{
		// grey
		cRGB.r = cRGB.g = cRGB.b = z;
		return cRGB;
	}

	const float h = x * 6;
	const int sector = (int)(h);
	const float f = h - sector;
	const float p = z * (1 - y);
	const float q = z * (1 - y * f);
	const float t = z * (1 - y * (1 - f));

	switch (sector % 6) 
	{
		case 0: // red
			cRGB.r = z;
			cRGB.g = t;
			cRGB.b = p;
			break;

		case 1: // orange
			cRGB.r = q;
			cRGB.g = z;
			cRGB.b = p;
			break;

		case 2: // yellow
			cRGB.r = p;
			cRGB.g = z;
			cRGB.b = t;
			break;

		case 3: // green
			cRGB.r = p;
			cRGB.g = q;
			cRGB.b = z;
			break;

		case 4: // blue 
			cRGB.r = t;
			cRGB.g = p;
			cRGB.b = z;
			break;

		default: // purple
			cRGB.r = z;
			cRGB.g = p;
			cRGB.b = q;
			break;
	}
	return cRGB;
}

/*float Vector3::DistanceBetweenLineSegementsSquared(const Vector3& lineStart1, const Vector3& lineEnd1, const Vector3& lineStart2, const Vector3& lineEnd2)
{
	// http://geomalgorithms.com/a07-_distance.html

	// this code does not work yet
	Vector3 u = (lineEnd1 - lineStart1).Normalize();
	Vector3 v = (lineEnd2 - lineStart2).Normalize();
	Vector3 w0 = lineStart1 - lineStart2;

	float a = u.Dot(u);
	float b = u.Dot(v);
	float c = v.Dot(v);
	float d = u.Dot(w0);
	float e = v.Dot(w0);

	Vector3 deltaClosest = w0 + ((b*e-c*d)*u - (a*e-b*d)*v) / (a*c - b*b);
	float distanceSquared = deltaClosest.LengthSquared();

	return distanceSquared;
}*/

////////////////////////////////////////////////////////////////////////////////////////
/*
	Debug Rendering
*/
////////////////////////////////////////////////////////////////////////////////////////

void Vector2::RenderDebug(const Color& color, float radius, float time) const
{
	g_debugRender.RenderPoint(*this, color, radius, time);
}

void Vector3::RenderDebug(const Color& color, float radius, float time) const
{
	g_debugRender.RenderPoint(*this, color, radius, time);
}
	
void XForm2::RenderDebug(const Color& color, float size, float time) const
{
	g_debugRender.RenderLine(position, this->TransformCoord(Vector2(size, 0)), color, time);
	g_debugRender.RenderLine(position, this->TransformCoord(Vector2(0, size)), color, time);
	//g_debugRender.RenderLine(position, GetUp()*size, color, time);
	//g_debugRender.RenderLine(position, GetRight()*size, color, time);
}

void Circle::RenderDebug(const Color& color, float time, int sides) const
{
	g_debugRender.RenderCircle(*this, color, time, sides);
}

void Cone::RenderDebug(const Color& color, float time, int sides) const
{
	g_debugRender.RenderCone(*this, color, time, sides);
}

void Sphere::RenderDebug(const Color& color, float time) const
{
	g_debugRender.RenderPoint(position, color, radius, time);
}

void Box2AABB::RenderDebug(const Color& color, float time) const
{
	g_debugRender.RenderBox(*this, color, time);
}

void Box2AABB::RenderDebug(const XForm2& xf, const Color& color, float time) const
{
	g_debugRender.RenderBox(xf, *this, color, time);
}

void Line2::RenderDebug(const Color& color, float time) const
{
	g_debugRender.RenderLine(p1, p2, color, time);
}

void Line3::RenderDebug(const Color& color, float time) const
{
	g_debugRender.RenderLine(p1, p2, color, time);
}

int Circle::LineSegmentIntersection(const Line2& l, Vector2* point1, Vector2* point2) const
{
	const Vector2 d = l.p2 - l.p1;
	const Vector2& c = position;

	const float A = d.x * d.x + d.y * d.y;
	const float B = 2 * (d.x * (l.p1.x - c.x) + d.y * (l.p1.y - c.y));
	const float C = (l.p1.x - c.x) * (l.p1.x - c.x) + (l.p1.y - c.y) * (l.p1.y - c.y) - radius * radius;

	const float det = B * B - 4 * A * C;
	if ((A <= 0.0000001) || (det < 0))
	{
		// no solutions
		return 0;
	}
	else if (det == 0)
	{
		// one solution
		const float t = -B / (2 * A);
		if (t < 0 || t > 1)
			return 0;

		if (point1)
			*point1 = l.p1 + t * d;
		return 1;
	}
	else
	{
		// two solutions
		int solutionCount = 0;
		const float sqrtfdet = sqrtf(det);
		const float t1 = (-B + sqrtfdet) / (2 * A);
		if (t1 >= 0 && t1 <= 1)
		{
			if (point1)
				*point1 = l.p1 + t1 * d;
			++solutionCount;
		}

		const float t2 = (-B - sqrtfdet) / (2 * A);
		if (t2 >= 0 && t2 <= 1)
		{
			++solutionCount;
			const Vector2 point = l.p1 + t2 * d;
			if (solutionCount == 1 && point1)
				*point1 = point;
			else if (point2)
				*point2 = point;
			return solutionCount;
		}

		return 0;
	}
}