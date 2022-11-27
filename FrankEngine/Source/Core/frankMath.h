////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Math Lib
	Copyright 2013 Frank Force - http://www.frankforce.com

	- full math libray for use with frank engine
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace FrankMath {

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////
/*
	Forward Declarations & Type Defs
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Vector2;
struct IntVector2;
struct ByteVector2;
struct Vector3;
struct Matrix44;
struct Quaternion;
struct Sphere;
struct Circle;
struct Line3;
struct Line2;
struct XForm2;
struct Color;
struct Box2AABB;

////////////////////////////////////////////////////////////////////////////////////////
/*
	Numerical Constants
*/
////////////////////////////////////////////////////////////////////////////////////////

#define FRANK_EPSILON		(0.001f)
#define PI					(3.14159265359f)
#define NATURAL_LOG			(2.71828182846f)
#define ROOT_2				(1.41421356237f)

////////////////////////////////////////////////////////////////////////////////////////
/*
	Asserts and Debug Functions
*/
////////////////////////////////////////////////////////////////////////////////////////

#ifdef _FINALRELEASE
#define _ASSERTS_DISABLE
#endif

// basic assert functionality
#ifndef _ASSERTS_DISABLE
#define ASSERT(e) { if (!(e)) {_CrtDbgBreak(); } }
#else
#define ASSERT(unused) {}
#endif

////////////////////////////////////////////////////////////////////////////////////////
/*
	Useful functions
*/
////////////////////////////////////////////////////////////////////////////////////////

// min, max and capping functions
template <class T> const T& Max(const T& a, const T& b) { return (a > b) ? a: b; }
template <class T> const T& Min(const T& a, const T& b) { return (a < b) ? a: b; }
template <class T> const T& Cap(const T& v, const T& min, const T& max)
{ return (v < min) ? min : ((v > max) ? max : v); }

// cap value beween 0 and 1
inline float CapPercent(float p) { return Cap(p, 0.0f, 1.0f); }

// Cap a wrapped percent value between 0 and 1
inline float CapWrappedPercent(float p)
{
	float intPart;
	return modf(p, &intPart) + ((p > 0) ? 0 : 1);
}

// cap angle between -PI and PI
inline float CapAngle(float a)
{
	a = (a + PI) / (2 * PI);
	a = CapWrappedPercent(a);
	a = a * 2 * PI - PI;
	return a;
}

template <class T, class U>
void SetFlags(T& flags, U flag) { flags = T(flags | T(flag)); }

template <class T, class U>
void UnsetFlags(T& flags, U flag) { flags = T(flags & T(~flag)); }

// get the number of elements in an array (from http://reedbeta.com/blog/cpp-compile-time-array-size/)
template <typename T, int N> char(&array_size_helper(T(&)[N]))[N];
#define ARRAY_SIZE(a) (sizeof(array_size_helper(a)))

// square a value
template <class T>
inline T Square(T v) { return v*v; }

// cue a value
template <class T>
inline T Cube(T v) { return v*v*v; }

template <class T>
inline T GetSign(T v) { return (v == T(0))? T(0) : ((v > T(0))? T(1) : T(-1)); }

template <class T> const T Lerp(const float percent, const T&a, const T&b)
{ return static_cast<T>(a + CapPercent(percent) * (b - a)); }

template <class T> const T LerpUncapped(const float percent, const T&a, const T&b)
{ return static_cast<T>(a + percent * (b - a)); }

// get what percent value is when it's 0 at a and 1 at b
template <class T, class U> const float Percent(const U& value, const T& a, const T& b)
{ return (a == b)? 0 : CapPercent((value - a) / (b - a)); }

// get percent between a and b, followed by a lerp between c and d
template <class T, class U, class S>
const S PercentLerp(const U& value, const T&a, const T&b, const S&c, const S&d)
{ return Lerp(Percent(value, a, b), c, d); }

// get a sine wave between 0-1 over time, at 1 cycle per second
inline float SineWavePulse(float time) { return 0.5f * (1.0f + sinf(2*PI*time)); }

// convert to and from radians
inline float RadiansToDegrees(float a)		{ return a * (180 / PI); }
inline float DegreesToRadians(float a)		{ return a * (PI / 180); }

inline unsigned Log2Int(unsigned x)
{
	int result = 0;
	while (x >>= 1) ++result;
	return result; 
}

// convert time in seconds to formatted text
void FormatTime(float time, WCHAR* text, bool tryToSkipHours = true);

// get equation for parabola passing between 2 points
void GetParabola(const Vector2& p1, const Vector2& p2, float& a, float& b, float& c);

////////////////////////////////////////////////////////////////////////////////////////
/*
	Ease functions

	adapted from
	- https://easings.net/
	- https://github.com/nicolausYes/easing-functions
*/
////////////////////////////////////////////////////////////////////////////////////////

inline float EaseInSine(float p)		{ return 1 + sinf( (PI/2) * (p-1)); }
inline float EaseOutSine(float p)		{ return sinf( (PI/2) * p);  }
inline float EaseInOutSine(float p)		{ return 0.5f * (1 + sinf(PI * (p - 0.5f))); }

inline float EaseInPow(float p, float pow)		{ return powf(p, pow); }
inline float EaseOutPow(float p, float pow)		{ return EaseInPow(p, 1.0f / pow); }
inline float EaseInOutPow(float p, float pow)	{ return 0.5f*((p < 0.5f) ? EaseInPow(2*p, pow) : 1 + EaseOutPow(2*(p-0.5f), pow)); }

inline float EaseInCircle(float p)		{ return 1 - sqrtf( 1 - p ); }
inline float EaseOutCircle(float p)		{ return sqrtf(p);  }
inline float EaseInOutCirc(float p)		{ return 0.5f * ((p < 0.5f) ? (1 - sqrtf(1 - 2 * p)) : (1 + sqrtf(2 * p - 1))); }

inline float EaseInBack(float p)		{ return p * p * (2.70158f * p - 1.70158f); }
inline float EaseOutBack(float p)		{ return  1 + (--p) * p  * (2.70158f * p  + 1.70158f);  }
inline float EaseInOutBack(float p)		{ return (p < 0.5f) ? (p * p * (7 * p - 2.5f) * 2) : (1 + (--p) * p * 2 * (7 * p + 2.5f)); }

inline float EaseOutElastic(float p, float elasticity = 0.7f, float bounces = 5) { return p = ((1 - cosf(p*PI*bounces))* (1 - p) + p / elasticity) * elasticity; }
inline float EaseInElastic(float p, float elasticity = 0.7f, float bounces = 5) { return 1 - EaseOutElastic(1 - p, elasticity, bounces); }
inline float EaseInOutElastic(float p)
{
    if (p < 0.45f) { float p2 = p * p;  return 8 * p2 * p2 * sinf( p * PI * 9 ); } 
	else if (p >= 0.55f) { float p2 = (p - 1) * (p - 1);  return 1 - 8 * p2 * p2 * sinf( p * PI * 9 ); }
	else return 0.5f + 0.75f * sinf( p * PI * 4 );
}

inline float EaseInBounce(float p)		{ return powf( 2, 6 * (p - 1) ) * fabs( sinf( p * PI * 3.5f ) ); }
inline float EaseOutBounce(float p)		{ return 1 - powf( 2, -6 * p ) * fabs( cosf( p * PI * 3.5f ) ); }
inline float EaseInOutBounce(float p)	{ return (p < 0.5f) ? (8 * powf(2, 8 * (p - 1)) * fabs(sinf(p * PI * 7))) : (1 - 8 * powf(2, -8 * p) * fabs(sinf(p * PI * 7))); }

inline float BounceSine(float p)		{ return sinf(PI * p); }
inline float BounceTriangle(float p)	{ return 1 - fabs(p*2 - 1); }

////////////////////////////////////////////////////////////////////////////////////////
/*
	Base class that can't be coppied
*/
////////////////////////////////////////////////////////////////////////////////////////

class Uncopyable
{
protected:
	Uncopyable() {}
	~Uncopyable() {}

private:
	Uncopyable(const Uncopyable&) = delete;
	Uncopyable& operator=(const Uncopyable&) = delete;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Colors
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Color : D3DXCOLOR
{
	Color() 
	{
		#ifdef DEBUG
			// init to bad values in debug
			r = g = b = a = NAN;
		#endif
	}
	Color( FLOAT r, FLOAT g, FLOAT b, FLOAT a = 1 ) : D3DXCOLOR(r, g, b, a ) {}
	Color( const D3DXCOLOR& c ) : D3DXCOLOR(c) {}
	Color( const b2Color& c) : D3DXCOLOR(c.r, c.g, c.b, c.a) {}
	
	operator DWORD () const
	{
		ASSERT(IsFinite());
		return (D3DXCOLOR)(*this);
	}
	const Color CapValues() const;
	const Color HSVtoRGB() const;
	const Color GetInverse() const { return Color(1 - r, 1 - g, 1 - b, a); }
	static const Color BuildBytes( UINT8 r, UINT8 g, UINT8 b, UINT8 a = 255 );
	static const Color BuildHSV(float h, float s=1, float v=1, float a = 1);

	static const Color RandBetween(const Color& c1, const Color& c2);
	static const Color RandBetweenComponents(const Color& c1, const Color& c2);

	const Color ScaleColor(float scale) const { return Color(r*scale, g*scale, b*scale, a); }
	const Color ScaleAlpha(float scale) const { return Color(r, g, b, a*scale); }
	const Color ZeroAlpha() const { return Color(r, g, b, 0); }

	bool operator < (const Color& c) const;
	bool IsFinite() const { return isfinite(r) && isfinite(g) && isfinite(b) && isfinite(a); }

	///////////////////////////////////////
    // Class Statics
	///////////////////////////////////////

	static const Color Red		(float a=1, float v=1)	{ return Color(v, 0, 0, a); }
	static const Color Orange	(float a=1, float v=1)	{ return Color(v, v*0.5f, 0, a); }
	static const Color Yellow	(float a=1, float v=1)	{ return Color(v, v, 0, a); }
	static const Color Green	(float a=1, float v=1)	{ return Color(0, v, 0, a); }
	static const Color Cyan		(float a=1, float v=1)	{ return Color(0, v, v, a); }
	static const Color Blue		(float a=1, float v=1)	{ return Color(0, 0, v, a); }
	static const Color Purple	(float a=1, float v=1)	{ return Color(v*0.5f, 0, v, a); }
	static const Color Magenta	(float a=1, float v=1)	{ return Color(v, 0, v, a); }
	static const Color White	(float a=1)				{ return Color(1, 1, 1, a); }
	static const Color Black	(float a=1)				{ return Color(0, 0, 0, a); }
	static const Color Clear	()						{ return Color(0, 0, 0, 0); }
	static const Color Grey		(float a=1, float v=0.5f) { return Color(v, v, v, a); }
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Random Numbers
*/
////////////////////////////////////////////////////////////////////////////////////////

struct FrankRand
{
	static void SetSeed(unsigned int newSeed);
	static unsigned int GetSeed() { return randomSeed; }
	static unsigned int GetInt();
	static int GetInt(int min, int max);
	static float GetFloat();
	static float GetFloat(float min, float max);
	static float GetSign();
	static const Color GetColor();
	static float GetAngle();
	static bool RollDie(unsigned int sides);
	static float GetGaussian(float variance = 1, float mean = 0);
	static const Vector2 GetRandomUnitVector();
	static const Vector2 GetRandomInBox(const Box2AABB& box);
	static const Vector2 GetRandomInCircle(float radius = 1, float minRadius = 0);
	
	// easy way to use custom seeds in a block without messing with everything else
	struct SaveSeedBlock
	{
		SaveSeedBlock(unsigned int tempSeed = 0) : seed(FrankRand::GetSeed()) { FrankRand::SetSeed(tempSeed); }
		~SaveSeedBlock() { FrankRand::SetSeed(seed); }
		private:
		unsigned int seed;
	};

private:

	// make sure no one can create an object of this type
	FrankRand() {}

	static unsigned int randomSeed;
	static const unsigned int maxRand = 0xffffffff;
};

// quick rand functions
#define RAND_BETWEEN(min, max)		(FrankRand::GetFloat(min, max))
#define RAND_INT					(FrankRand::GetInt())
#define RAND_INT_BETWEEN(min, max)	(FrankRand::GetInt(min, max))
#define RAND_PERCENT				(FrankRand::GetFloat())
#define RAND_SIGN					(FrankRand::GetSign())
#define RAND_INT_SIGN				((int)FrankRand::GetSign())
#define RAND_DIE(sides)				(FrankRand::RollDie(sides))
#define RAND_COLOR					(FrankRand::GetColor())
#define RAND_ANGLE					(FrankRand::GetAngle())
#define RAND_CHAR					((char)(FrankRand::GetInt(CHAR_MIN, CHAR_MAX)))
#define RAND_ARRAY_ELEMENT(_array)	(RAND_INT_BETWEEN(0, ARRAY_SIZE(_array)-1))

////////////////////////////////////////////////////////////////////////////////////////
/*
	Game Timer

	simple timer class to automatically keep track of timers
	- uses miliseconds under the hood to keep track of time for maximum accuracy
	- converts to float seconds value automatically
	- also keeps a left over time value so interpolation is automatically taken into account
*/
////////////////////////////////////////////////////////////////////////////////////////

struct GameTimer
{
	GameTimer()											: endTime(invalidTime) {}
	explicit GameTimer(float currentTime)				: endTime(interpolatedTime - (long long)(conversion * currentTime)) {}

	void Set()											{ endTime = interpolatedTime; }
	void SetTimeElapsed(float timeElapsed)				{ endTime = interpolatedTime - (long long)(conversion * timeElapsed); }
	void SetTimeRemaining(float timeRemaining)			{ endTime = interpolatedTime + (long long)(conversion * timeRemaining); }
	void Unset()										{ endTime = invalidTime; }
	bool IsSet() const									{ return endTime != invalidTime; }

	bool HasElapsed() const								{ return gameTime >= endTime; }
	float GetTimeElapsed() const						{ return IsSet()? GetTimeElapsedInternal() : 0; }
	operator float () const								{ return GetTimeElapsed(); }

	bool HasTimeRemaining() const						{ return IsSet() && gameTime < endTime; }
	float GetTimeRemaining() const						{ return IsSet()? -GetTimeElapsedInternal() : 0; }

	static void ResetGlobal()							{ gameTime = 0; interpolatedTime = 0; }
	static void UpdateGlobal(float delta)				{ gameTime += (long long)(conversion * delta); interpolatedTime = gameTime; }
	static void UpdateInterpolatedGlobal(float extra)	{ interpolatedTime = gameTime + (long long)(conversion * extra); }
	static float GetTimeGlobal()						{ return interpolatedTime/conversion; }

protected:

	float GetTimeElapsedInternal() const				{ ASSERT(IsSet()); return (interpolatedTime - endTime)/conversion; }
	
	long long endTime;
	static long long interpolatedTime;
	static long long gameTime;
	static const long long invalidTime;
	static const float conversion;
};	

// basically the same as GameTimer except it keeps running even when the game is paused
// this is useful for interface type stuff
struct GamePauseTimer
{
	GamePauseTimer()								: endTime(invalidTime) {}
	explicit GamePauseTimer(float currentTime)		: endTime(gameTime - (long long)(conversion * currentTime)) {}
	
	void Set()										{ endTime = gameTime; }
	void SetTimeElapsed(float timeElapsed)			{ ASSERT(timeElapsed >= 0);   endTime = gameTime - (long long)(conversion * timeElapsed); }
	void SetTimeRemaining(float timeRemaining)		{ ASSERT(timeRemaining >= 0); endTime = gameTime + (long long)(conversion * timeRemaining); }
	void Unset()									{ endTime = invalidTime; }
	bool IsSet() const								{ return endTime != invalidTime; }

	bool HasElapsed() const							{ return gameTime >= endTime; }
	float GetTimeElapsed() const					{ return IsSet()? GetTimeElapsedInternal() : 0; }
	operator float () const							{ return GetTimeElapsed(); }

	bool HasTimeRemaining() const					{ return IsSet() && gameTime < endTime; }
	float GetTimeRemaining() const					{ return IsSet()? -GetTimeElapsedInternal() : 0; }

	static void ResetGlobal()						{ gameTime = 0; }
	static void UpdateGlobal(float timeDelta)		{ gameTime += (long long)(conversion * timeDelta); }
	static float GetTimeGlobal()					{ return gameTime/conversion; }

protected:

	float GetTimeElapsedInternal() const			{ ASSERT(IsSet()); return (gameTime - endTime)/conversion; }
	
	long long endTime;
	static long long gameTime;
	static const long long invalidTime;
	static const float conversion;
};	

struct GameTimerPercent : public GameTimer
{
	explicit GameTimerPercent(float _maxTime = 0)		: maxTime(_maxTime) {}
	GameTimerPercent(float _maxTime, float currentTime)	: maxTime(_maxTime), GameTimer(currentTime - _maxTime)  {}

	void Set(float _maxTime, float currentTime = 0)		{ maxTime = _maxTime; GameTimer::SetTimeRemaining(_maxTime - currentTime); }
	operator float () const								{ return IsSet()? CapPercent((maxTime - GetTimeRemaining()) / maxTime) : 0; }
	float GetTimeLeft() const							{ return IsSet()? (maxTime - GetTimeElapsedInternal()) : 0; }
	float GetTimePast() const							{ return IsSet()? GetTimeElapsedInternal() : 0; }

private:
	
	void SetTimeElapsed(float timeElapsed)				{ ASSERT(false); }
	void SetTimeRemaining(float timeRemaining)			{ ASSERT(false); }
	float maxTime;
};

///////////////////////////////////////////////////////////////////////////////////////
/*
	ValueInterpolator
		
	- an easier way to make a value interpolated
*/
////////////////////////////////////////////////////////////////////////////////////////

template <class T>
struct ValueInterpolator
{
	ValueInterpolator() {}
	ValueInterpolator(const T& v) : value(v), valueLast(v) {}
	
	operator T&()							{ return value; }
	operator const T&() const				{ return value; }
	T& operator = (const T& v)				{ return value = v; }
	void Init(const T& v)					{ value = valueLast = v; }
	void SaveLast()							{ valueLast = value; }
	const T GetInterpolated() const			{ return Lerp(g_interpolatePercent, value, valueLast); }
	T& Get()								{ return value; }
	T& GetLast()							{ return valueLast; }
	const T& Get() const					{ return value; }
	const T& GetLast() const				{ return valueLast; }

protected:

	T value;
	T valueLast;
};

struct AngleInterpolator : public ValueInterpolator<float>
{
	AngleInterpolator() : ValueInterpolator(0)	{}
	float operator = (const float v)			{ return value = v; }
	float GetInterpolated(float p) const		{ return value + CapPercent(p) * CapAngle(valueLast - value); }
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Vector2 Class
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Vector2
{
	Vector2();
	Vector2(const Vector2& v);
	Vector2(const IntVector2& v);
	Vector2(const ByteVector2& v);
	Vector2(float _x, float _y);
	Vector2(const b2Vec2& v);
	explicit Vector2(float _v);
	explicit Vector2(const Vector3& v);

	static inline const Vector2 BuildFromAngle(float angle);
	static inline const Vector2 BuildRandomUnitVector();
	static inline const Vector2 BuildRandomInBox(const Box2AABB& box);
	static inline const Vector2 BuildRandomInCircle(float radius = 1, float minRadius = 0);
	static inline const Vector2 BuildGaussian(const Vector2& variance = Vector2(1), const Vector2& mean = Vector2(0));
	const D3DXVECTOR3 GetD3DXVECTOR3(float z = 0) const;

	void RenderDebug(const Color& color = Color::White(0.5f), float radius = 0.1f, float time = 0.0f) const;

	///////////////////////////////////////
    // Basic Vector Math Functionality
	///////////////////////////////////////

	float GetAngle() const;
	float Length() const						{ return (sqrtf(x*x + y*y)); }
	float LengthSquared() const					{ return (x*x + y*y); }
	const Vector2 Normalize() const;
	Vector2& NormalizeThis();
	bool IsNormalized() const;
	const Vector2 Normalize(float length) const;
	float LengthAndNormalizeThis();
	const Vector2 CapLength(float maxLength) const;
	Vector2& CapLengthThis(float maxLength);
	const Vector2 Floor() const;
	Vector2& FloorThis();
	const Vector2 Round() const;
	Vector2& RoundThis();

	float Dot(const Vector2& v) const			{ return (x*v.x + y*v.y); }
	float Cross(const Vector2& v) const			{ return (x*v.y - y*v.x); }
	const Vector2 Abs() const					{ return Vector2(fabs(x), fabs(y)); }
	const Vector2 ZeroThis()					{ return *this = Vector2(0,0); }
	bool IsZero() const							{ return (x == 0) & (y == 0); }
	float LengthManhattan()	const				{ return fabs(x) + fabs(y); }
	bool IsFinite() const						{ return isfinite(x) && isfinite(y); }

	// returns angle betwen 2 vecs
	// result is between 0 and 2*PI
	float AngleBetween(const Vector2& v) const;
	
	// returns signed angle betwen 2 vecs
	// result is between -PI and PI
	float SignedAngleBetween(const Vector2& v) const;

	const Vector2 TransformCoord(const XForm2& xf) const;
	Vector2 &TransformThisCoord(const XForm2& xf);

	const Vector2 Rotate(const Vector2& v) const;
	Vector2& RotateThis(const Vector2& v);
	const Vector2 Rotate(float theta) const;
	Vector2& RotateThis(float theta);

	const Vector2 RotateRightAngle() const		{ return Invert(); }
	Vector2& RotateRightAngleThis()				{ return InvertThis(); }
	const Vector2 Invert() const				{ return Vector2(y, -x); }
	Vector2& InvertThis()						{ return *this = this->Invert(); }
	const Vector2 Reflect(const Vector2& normal) const;

	///////////////////////////////////////
	// Operators
	///////////////////////////////////////

	Vector2& operator += (const Vector2& v)				{ return *this = *this + v; }
	Vector2& operator -= (const Vector2& v)				{ return *this = *this - v; }
	Vector2& operator *= (const Vector2& v)				{ return *this = *this * v; }
	Vector2& operator /= (const Vector2& v)				{ return *this = *this / v; }
	Vector2& operator *= (float scale)					{ return *this = *this * scale; }
	Vector2& operator /= (float scale)					{ return *this = *this / scale; }
	const Vector2 operator - () const					{ return Vector2(-x, -y); }
	const Vector2 operator + (const Vector2& v) const	{ return Vector2(x + v.x, y + v.y); }
	const Vector2 operator - (const Vector2& v) const	{ return Vector2(x - v.x, y - v.y); }
	const Vector2 operator * (const Vector2& v) const	{ return Vector2(x * v.x, y * v.y);}
	const Vector2 operator / (const Vector2& v) const	{ return Vector2(x / v.x, y / v.y); }
	const Vector2 operator * (float scale) const		{ return Vector2(x * scale, y * scale); }
	const Vector2 operator / (float scale) const		{ float s = 1.0f/scale; return Vector2(x *s, y *s); }
	const XForm2 operator * (const XForm2& xf) const;
	friend const Vector2 operator * (float scale, const Vector2& v) { return v * scale;}
	friend const Vector2 operator / (float scale, const Vector2& v) { return Vector2(scale) / v;}
	bool operator == (const Vector2& other) const { return x == other.x && y == other.y; }
	bool operator != (const Vector2& other) const { return !(*this == other); }
	operator b2Vec2() const { return b2Vec2(x, y); }

	///////////////////////////////////////
	// Tests
	///////////////////////////////////////

	// line intersection tests
	static bool LineSegmentIntersection( const Vector2& s1a, const Vector2& s1b, const Vector2& s2a, const Vector2& s2b, Vector2& point);
	static bool LineSegmentIntersection( const Vector2& s1a, const Vector2& s1b, const Vector2& s2a, const Vector2& s2b);
	static bool LineIntersection(const Vector2& v1, const Vector2& v2, const Vector2& v3, const Vector2& v4, Vector2& point);
	Vector2 NearestPointOnLine(const Vector2& s1a, const Vector2& s1b) const;
	Vector2 NearestPointOnLineSegment(const Vector2& s1a, const Vector2& s1b) const;
	float DistanceFromLine(const Vector2& lineStart, const Vector2& lineEnd) const;

	// distance of point from line segment test
	float DistanceFromLineSegement(const Vector2& lineStart, const Vector2& lineEnd) const;
	static float DistanceFromLineSegement(const Vector2& lineStart, const Vector2& lineEnd, const Vector2& point);

	// returns shortest distance between 2 line segments
	static float DistanceBetweenLineSegments(const Vector2& segment1a, const Vector2& segment1b, const Vector2& segment2a, const Vector2& segment2b);

	// returns true if verts form clockwise triangle
	static bool IsClockwise( const Vector2& v1, const Vector2& v2, const Vector2& v3);

	// returns true if directions are winding clockwise
	static bool IsClockwise(const Vector2& d1, const Vector2& d2) ;

	// returns true if point is inside box
	static inline bool InsideBox(const XForm2& xf, const Vector2& size, const Vector2& point);

	// returns true if point is inside clockwise winding convex polygon
	static inline bool InsideConvexPolygon(const Vector2* vertexList, int vertexCount, const Vector2& point);

	///////////////////////////////////////
	// Class Statics
	///////////////////////////////////////

	static const Vector2 Zero()		{ return Vector2(0, 0); }
	static const Vector2 XAxis()	{ return Vector2(1, 0); }
	static const Vector2 YAxis()	{ return Vector2(0, 1); }

	///////////////////////////////////////
    // Data
	///////////////////////////////////////

	float x, y;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	ByteVector2 Class
*/
////////////////////////////////////////////////////////////////////////////////////////

struct ByteVector2
{
	ByteVector2()
	{
		#ifdef DEBUG
			// init to bad values in debug
			x = CHAR_MAX;
			y = CHAR_MAX;
		#endif
	}
	explicit ByteVector2(char _s): x(_s), y(_s) {}
	ByteVector2(char _x, char _y): x(_x), y(_y) {}
	explicit ByteVector2(int _s);
	ByteVector2(int _x, int _y);

	ByteVector2(const IntVector2& v);
	ByteVector2(const Vector2& v);
	bool IsZero() const { return (x == 0 && y == 0); }
	
	bool operator == (const ByteVector2& other) const			{ return x == other.x && y == other.y; }
	bool operator != (const ByteVector2& other) const			{ return !(*this == other); }
	const ByteVector2 operator + (const ByteVector2& v) const	{ return ByteVector2(x + v.x, y + v.y); }
	const ByteVector2 operator - (const ByteVector2& v) const	{ return ByteVector2(x - v.x, y - v.y); }

	char x, y;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	IntVector2 Class
*/
////////////////////////////////////////////////////////////////////////////////////////

struct IntVector2
{
	IntVector2()
	{
		#ifdef DEBUG
			// init to bad values in debug
			x = INT_MAX;
			y = INT_MAX;
		#endif
	}
	explicit IntVector2(int _s) : x(_s), y(_s) {}
	IntVector2(int _x, int _y) : x(_x), y(_y) {}
	IntVector2(const ByteVector2& v) : x(int(v.x)), y(int(v.y)) {}
	IntVector2(const Vector2& v) : x(int(v.x)), y(int(v.y)) {}

	///////////////////////////////////////
	// Operators
	///////////////////////////////////////
	
	int Dot(const IntVector2& v) const									{ return (x*v.x + y*v.y); }
	int Cross(const IntVector2& v) const								{ return (x*v.y - y*v.x); }
	IntVector2& operator += (const IntVector2& v)						{ return *this = *this + v; }
	IntVector2& operator -= (const IntVector2& v)						{ return *this = *this - v; }
	IntVector2& operator *= (const IntVector2& v)						{ return *this = *this * v; }
	IntVector2& operator /= (const IntVector2& v)						{ return *this = *this / v; }
	IntVector2& operator *= (int scale)									{ return *this = *this * scale; }
	IntVector2& operator /= (int scale)									{ return *this = *this / scale; }
	const IntVector2 operator - () const								{ return IntVector2(-x, -y); }
	const IntVector2 operator + (const IntVector2& v) const				{ return IntVector2(x + v.x, y + v.y); }
	const IntVector2 operator - (const IntVector2& v) const				{ return IntVector2(x - v.x, y - v.y); }
	const IntVector2 operator * (const IntVector2& v) const				{ return IntVector2(x * v.x, y * v.y);}
	const IntVector2 operator / (const IntVector2& v) const				{ return IntVector2(x / v.x, y / v.y); }
	const IntVector2 operator * (int s) const							{ return IntVector2(x * s, y * s); }
	const IntVector2 operator / (int s) const							{ return IntVector2(x / s, y /s); }
	friend const IntVector2 operator * (int scale, const IntVector2& v)	{ return v * scale;}
	friend const IntVector2 operator / (int scale, const IntVector2& v)	{ return IntVector2(scale) / v;}
	bool IsOnLine(const IntVector2& v1, const IntVector2& v2) const
	{
		IntVector2 d1 = *this - v1;
		IntVector2 d2 = *this - v2;
		return (d1.Cross(d2) == 0);
	}
	
	float Length() const					{ return (sqrtf(float(x*x + y*y))); }
	int LengthSquared() const				{ return (x*x + y*y); }
	int LengthManhattan() const				{ return (abs(x) + abs(y)); }
	int LengthChebyshev() const				{ return Max(abs(x), abs(y)); }
	const IntVector2 Abs() const			{ return IntVector2(abs(x), abs(y)); }

	static const IntVector2 BuildIntRotation(int rotation)
	{
		ASSERT(rotation >= 0 && rotation <= 3);
		switch (rotation)
		{
			default:
			case 0: return Vector2( 0, 1);
			case 1: return Vector2( 1, 0);
			case 2: return Vector2( 0,-1);
			case 3: return Vector2(-1, 0);
		};
	}

	bool operator == (const IntVector2& other) const { return x == other.x && y == other.y; }
	bool operator != (const IntVector2& other) const { return !(*this == other); }

	int x, y;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Vector3 Class
	- extend the D3DXVECTOR3 implementation
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Vector3 : public D3DXVECTOR3
{
	// constructors to make the vector3
	Vector3() {}
	Vector3(const D3DXVECTOR3& v);
	Vector3(float _x, float _y, float _z);
	explicit Vector3(float _v) ;
	Vector3(const Vector2& v);

	const DWORD GetDWORD(float w = 0)
	{
		const Vector3 v = this->Normalize();
		DWORD r = (DWORD)( 127.0f * v.x + 128.0f );
		DWORD g = (DWORD)( 127.0f * v.y + 128.0f );
		DWORD b = (DWORD)( 127.0f * v.z + 128.0f );
		DWORD a = (DWORD)( 255.0f * w );
    
		return( (a<<24L) + (r<<16L) + (g<<8L) + (b<<0L) );
	}

	// quick way to get a randomized vector
	static const Vector3 BuildRandomQuick();

	// normalized evenly distributed vector, slower then quick version
	static inline Vector3 BuildRandomNormalized();

	void RenderDebug(const Color& color = Color::White(0.5f), float radius = 0.1f, float time = 0.0f) const;

	///////////////////////////////////////
    // Basic Math Functionality
	///////////////////////////////////////

	const Vector3 Normalize() const;
	Vector3& NormalizeThis();
	const Vector3 Normalize(float length) const;
	bool IsNormalized() const;
	float Length() const;
	float LengthSquared() const;
	float LengthAndNormalize();
	Vector3& ZeroThis();
	bool IsZero() const { return (x == 0 && y == 0 && z == 0); }

	// dot product
	float Dot(const Vector3& v) const;
    const Vector3 operator * (const Vector3& v) const;

	// cross product
	const Vector3 Cross(const Vector3& v) const;
    const Vector3 operator ^ (const Vector3& v) const;

	// returns angle betwen 2 vecs,
	// result is between 0 and 2*PI
	// (use cross product to get sign if necessary)
	float AngleBetween(const Vector3& v) const;

	// get the euler rotation this vector represents
	const Vector3 GetRotation() const;
	void CapRotation();

	///////////////////////////////////////
    // Operators
	///////////////////////////////////////

    Vector3& operator += (const Vector3& v);
    Vector3& operator -= (const Vector3& v);
    Vector3& operator *= (float scale);
    Vector3& operator /= (float scale);

    const Vector3 operator - () const;
    const Vector3 operator + (const Vector3& v) const;
    const Vector3 operator - (const Vector3& v) const;
    const Vector3 operator * (float scale) const;
    const Vector3 operator / (float scale) const;
    friend const Vector3 operator * (float scale, const Vector3& v);

	///////////////////////////////////////
    // Tests
	///////////////////////////////////////

	float DistanceFromLineSegementSquared(const Vector3& lineStart, const Vector3& lineEnd) const;

	// A slightly faster version where user provides pre calcualted deltapos
	// useful if you are doing many tests with the same start and end
	float DistanceFromLineSegementSquared(const Vector3& lineStart, const Vector3& lineEnd, const Vector3& deltaPos, float deltaPosMagSquared) const;

	// A slightly faster version where user provides pre calcualted deltapos
	// useful if you are doing many tests with the same start and end
	// this version also returns a percentage of the distance along the line
	float DistanceFromLineSegementSquared(const Vector3& lineStart, const Vector3& lineEnd, const Vector3& deltaPos, float deltaPosMagSquared, float& percentDistance) const;
	float DistanceFromLineSegement(const Vector3& lineStart, const Vector3& lineEnd) const;

	///////////////////////////////////////
    // Class Statics
	///////////////////////////////////////

	static const Vector3 Zero()		{ return Vector3(0, 0, 0); }
	static const Vector3 One()		{ return Vector3(1, 1, 1); }
	static const Vector3 XAxis()	{ return Vector3(1, 0, 0); }
	static const Vector3 YAxis()	{ return Vector3(0, 1, 0); }
	static const Vector3 ZAxis()	{ return Vector3(0, 0, 1); }
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Matrix44
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Matrix44
{
	Matrix44() {}
	Matrix44(const Matrix44& m)  : matrix(m.matrix) {}
	Matrix44(const D3DMATRIX& m)  : matrix(m) {}
	explicit Matrix44(const XForm2& xf, float z = 0);
	explicit Matrix44(const Vector2& v);
	explicit Matrix44(const Quaternion& q);

    float& operator () (int row, int column);
    float operator () (int row, int column) const;

	///////////////////////////////////////
    // Basic Math Functionality
	///////////////////////////////////////

    Matrix44& operator *= (const Matrix44& m);
    Matrix44& operator += (const Matrix44& m);
    Matrix44& operator -= (const Matrix44& m);
    Matrix44& operator *= (float s);
    Matrix44& operator /= (float s);
    Matrix44& operator += (const Vector3& v);
    Matrix44& operator -= (const Vector3& v);

    const Matrix44 operator - () const;
    const Matrix44 operator * (const Matrix44& m) const;
    const Matrix44 operator + (const Matrix44& m) const;
    const Matrix44 operator - (const Matrix44& m) const;
    const Matrix44 operator * (float s) const;
	const Matrix44 operator / (float s) const;
    friend const Matrix44 operator * (float s, const Matrix44& m);

	const Matrix44 Inverse() const;
	Matrix44& InverseThis();
	const Matrix44 Transpose() const;
	Matrix44& TransposeThis();

	///////////////////////////////////////
    // Transform Constructors
	///////////////////////////////////////

	static const Matrix44 BuildRotate(float x, float y, float z);
	static const Matrix44 BuildXFormZ(const Vector2& pos, float angle, float zPlain = 0);
	static const Matrix44 BuildRotateZ(float angle);
	static const Matrix44 BuildRotate(const Vector3& v);
	static const Matrix44 BuildRotate(const Vector3& axis, float angle);
	static const Matrix44 BuildScale(float x, float y, float z);
	static const Matrix44 BuildScale(float scale);
	static const Matrix44 BuildScale(const Vector3& v);
	static const Matrix44 BuildTranslate(float x, float y, float z);
	static const Matrix44 BuildTranslate(const Vector3& v);
	static const Matrix44 BuildLookAtLH(const Vector3& pos, const Vector3& at, const Vector3& up);

	const Vector3 TransformCoord(const Vector3& v) const;
	const Vector3 TransformNormal(const Vector3& v) const;

	///////////////////////////////////////
    // Accessors
	///////////////////////////////////////

	const Vector3 GetRight() const;
	const Vector3 GetUp() const;
	const Vector3 GetForward() const;
	const Vector3 GetPos() const;
	const Vector2 GetPosXY() const;

	void SetRight(const Vector3& v);
	void SetUp(const Vector3& v);
	void SetForward(const Vector3& v);
	void SetPos(const Vector3& v);

	float GetAngleZ() const;
	void GetYawPitchRoll(Vector3& rotation) const;

	D3DXMATRIX& GetD3DXMatrix()				{ return matrix; }
	const D3DXMATRIX& GetD3DXMatrix() const { return matrix; }

	///////////////////////////////////////
    // Class Statics
	///////////////////////////////////////

	static Matrix44 const Identity();

private:

	///////////////////////////////////////
    // Data
	///////////////////////////////////////

	D3DXMATRIX matrix;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Quaternion
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Quaternion
{
	Quaternion();
	Quaternion(const D3DXQUATERNION& q);
	Quaternion(const Matrix44& m);
	Quaternion(const Vector3& rotation);
	Quaternion(const Vector3& axis, float angle);
	Quaternion(float x, float y, float z, float w);

	D3DXQUATERNION& GetD3DXQuaternion();
	const D3DXQUATERNION& GetD3DXQuaternion() const;

	///////////////////////////////////////
    // Assignment Operators
	///////////////////////////////////////

    Quaternion& operator *= (const Quaternion& q);
    Quaternion& operator += (const Quaternion& q);
    Quaternion& operator -= (const Quaternion& q);
    Quaternion& operator *= (float s);
    Quaternion& operator /= (float s);

	///////////////////////////////////////
    // Basic Math Functionality
	///////////////////////////////////////

    const Quaternion operator * (const Quaternion& q) const;
    const Quaternion operator + (const Quaternion& q) const;
    const Quaternion operator - (const Quaternion& q) const;
    const Quaternion operator * (float s) const;
    const Quaternion operator / (float s) const;

	const Quaternion Normalize() const;
	Quaternion& NormalizeThis();
	float DotProduct(const Quaternion& q) const;
	const Quaternion Inverse() const;

	Quaternion& SlerpThis(const Quaternion& q, float percent);
	const Quaternion Slerp(const Quaternion& q, float percent) const;
	void GetAxisAngle(Vector3& axis, float& angle) const;

	///////////////////////////////////////
    // Class Statics
	///////////////////////////////////////

	static const Quaternion Identity();

private:

	D3DXQUATERNION quaternion;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	2d Transform Class
*/
////////////////////////////////////////////////////////////////////////////////////////

struct XForm2
{
	XForm2();
	XForm2(const XForm2& otherXForm);
	XForm2(const b2Transform& otherXForm);
	XForm2(const Vector2& _position, float _angle = 0);
	explicit XForm2(float _angle);

	static XForm2 BuiltIntAngle(int angle)
	{
		switch (angle)
		{
			default:
			case 0: return XForm2(0);
			case 1: return XForm2(PI/2.0f);
			case 2: return XForm2(PI);
			case 3: return XForm2(PI*3.0f/2.0f);
		};
	}

	void RenderDebug(const Color& color = Color::White(0.5f), float size = 0.1f, float time = 0.0f) const;

	const XForm2 Inverse()const;
	const Vector2 GetUp() const;
	const Vector2 GetRight() const;
	bool IsFinite() const						{ return position.IsFinite() && isfinite(angle); }

    XForm2& operator *= (const XForm2& xf);
    const XForm2 operator * (const XForm2& xf) const;
    const XForm2 operator + (const XForm2& xf) const;
    const XForm2 operator - (const XForm2& xf) const;
    const XForm2 operator * (float scale) const;
	friend const XForm2 operator * (float scale, const XForm2& xf) { return xf * scale; }
    const XForm2 operator / (float scale) const { return *this * (1/scale); }
	friend const XForm2 operator / (float scale, const XForm2& xf) { return xf / scale; }
	
	const XForm2 ScalePos(float scale) const { return XForm2(position*scale, angle); }
	const XForm2 ScalePos(const Vector2& scale) const { return XForm2(position*scale, angle); }

	const XForm2 Interpolate(const XForm2& xfDelta, float percent) const;

	const Vector2 TransformCoord(const Vector2& v) const;
	const Vector2 TransformVector(const Vector2& v) const;

	static const XForm2 Identity();
	bool operator == (const XForm2& other) const { return position == other.position && angle == other.angle; }
	bool operator != (const XForm2& other) const { return !(*this == other); }

	Vector2 position;
	float angle;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Circle
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Circle
{
	Circle();
	Circle(const Circle& _c);
	Circle(const Vector2& _position, float _radius);
	explicit Circle(float _radius);

	int LineSegmentIntersection(const Line2& l, Vector2* point1 = NULL, Vector2* point2 = NULL) const;

	void RenderDebug(const Color& color = Color::White(0.5f), float time = 0.0f, int sides = 24) const;

    Circle& operator += (const Circle& c);
    const Circle operator + (const Circle& c) const;

	Vector2 position;
	float radius;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Cone
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Cone
{
	Cone();
	Cone(const Cone& _c);
	Cone(const XForm2& _xf, float _radius, float _angle);

	void RenderDebug(const Color& color = Color::White(0.5f), float time = 0.0f, int sides = 24) const;

	XForm2 xf;
	float radius;
	float angle;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Sphere
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Sphere
{
	Sphere();
	Sphere(const Sphere& _s);
	Sphere(const Vector3& _position, float _radius);
	explicit Sphere(float _radius);

	void RenderDebug(const Color& color = Color::White(0.5f), float time = 0.0f) const;

    Sphere& operator += (const Sphere& s);
    const Sphere operator + (const Sphere& s) const;

	Vector3 position;
	float radius;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	BBox
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Box2AABB : public b2AABB
{
	Box2AABB();
	explicit Box2AABB(const Vector2& point);
	Box2AABB(const Vector2& point1, const Vector2& point2);
	Box2AABB(const b2AABB& b);
	Box2AABB(const XForm2& xf, const Vector2& size)
	{
		const Vector2 v1 = size.Rotate(xf.angle);
		const Vector2 v2 = (size*Vector2(-1,1)).Rotate(xf.angle);
		const Vector2 v
		(
			Max(fabs(v1.x), fabs(v2.x)),
			Max(fabs(v1.y), fabs(v2.y))
		);

		lowerBound = xf.position - v;
		upperBound = xf.position + v;
	}
	
	Box2AABB(const XForm2& xf, const Box2AABB& box, float scale = 1)
	{
		// build a bigger box that encapsulates the transformed box
		const Vector2 pos1 = xf.TransformCoord(scale*Vector2(box.lowerBound));
		const Vector2 pos2 = xf.TransformCoord(scale*Vector2(box.lowerBound.x, box.upperBound.y));
		const Vector2 pos3 = xf.TransformCoord(scale*Vector2(box.upperBound));
		const Vector2 pos4 = xf.TransformCoord(scale*Vector2(box.upperBound.x, box.lowerBound.y));

		lowerBound = Vector2(Min(Min(Min(pos1.x, pos2.x), pos3.x), pos4.x), Min(Min(Min(pos1.y, pos2.y), pos3.y), pos4.y));
		upperBound = Vector2(Max(Max(Max(pos1.x, pos2.x), pos3.x), pos4.x), Max(Max(Max(pos1.y, pos2.y), pos3.y), pos4.y));
	}

	Box2AABB SortBounds() const
	{
		return Box2AABB
		(
			Vector2(Min(lowerBound.x, upperBound.x), Min(lowerBound.y, upperBound.y)),
			Vector2(Max(lowerBound.x, upperBound.x), Max(lowerBound.y, upperBound.y))
		);
	}

	operator RECT () const
	{
		RECT r = { int(lowerBound.x), int(lowerBound.y), int(upperBound.x), int(upperBound.y) };
		return r;
	}

	const Vector2 GetSize() const { return (upperBound - lowerBound); }

	void RenderDebug(const Color& color = Color::White(0.5f), float time = 0.0f) const;
	void RenderDebug(const XForm2& xf, const Color& color = Color::White(0.5f), float time = 0.0f) const;

	// combine boxes
    const Box2AABB operator + (const Box2AABB& b) const;
    const Box2AABB operator + (const Vector2& v) const;
    Box2AABB& operator += (const Box2AABB& b);
    Box2AABB& operator += (const Vector2& v);

	const Box2AABB Inflate(float v) const { return Box2AABB(upperBound + Vector2(v), lowerBound - Vector2(v)).SortBounds(); }

	bool Contains(const Vector2& v) const;
	bool FullyContains(const Box2AABB& b) const;
	bool PartiallyContains(const Box2AABB& b) const;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Line2 Class
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Line2
{
	Line2();
	Line2(const Vector2& _p1, const Vector2& _p2);

	void RenderDebug(const Color& color = Color::White(0.5f), float time = 0.0f) const;

	float Length() const { return (p1 - p2).Length(); }

    Line2& operator += (const Vector2& v);
    Line2& operator -= (const Vector2& v);
    const Line2 operator + (const Vector2& v) const;
    const Line2 operator - (const Vector2& v) const;

	Vector2 p1, p2;
};

////////////////////////////////////////////////////////////////////////////////////////
/*
	Line3 Class
*/
////////////////////////////////////////////////////////////////////////////////////////

struct Line3
{
	Line3();
	Line3(const Vector3& _start, const Vector3& _end);

	void RenderDebug(const Color& color = Color::White(0.5f), float time = 0.0f) const;

	Vector3 p1, p2;
};

#include "frankMath.inl"

} // namespace FrankMath