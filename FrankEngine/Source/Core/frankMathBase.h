////////////////////////////////////////////////////////////////////////////////////////
/*
	Frank Engine Portable Math Base
	Copyright 2013 Frank Force - http://www.frankforce.com

	- portable replacements for the D3DX math types and functions frankMath uses
	- layout-identical to D3DXCOLOR / D3DXVECTOR3 / D3DXMATRIX / D3DXQUATERNION so
	  the Windows D3D9 renderer can cast without copies (see frankMath.h interop)
	- conventions match D3DX exactly: row-major storage, row-vector transforms
	  (v * M), left-handed - enforced numerically by web/mathTest against real D3DX
	- header only, no dependencies beyond <cmath>
*/
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>

namespace FrankMath {

////////////////////////////////////////////////////////////////////////////////////////
// FrankColorBase - layout and semantics of D3DXCOLOR
////////////////////////////////////////////////////////////////////////////////////////

struct FrankColorBase
{
	float r, g, b, a;

	FrankColorBase() {}
	FrankColorBase(float _r, float _g, float _b, float _a) : r(_r), g(_g), b(_b), a(_a) {}
	explicit FrankColorBase(unsigned long argb)
	{
		const float f = 1.0f / 255.0f;
		a = f * (float)(unsigned char)(argb >> 24);
		r = f * (float)(unsigned char)(argb >> 16);
		g = f * (float)(unsigned char)(argb >>  8);
		b = f * (float)(unsigned char)(argb >>  0);
	}

	// saturating ARGB8 pack, replicating D3DXCOLOR::operator DWORD
	unsigned long ToDWORD() const
	{
		const unsigned long dwR = r >= 1.0f ? 0xff : (r <= 0.0f ? 0x00 : (unsigned long)(r * 255.0f + 0.5f));
		const unsigned long dwG = g >= 1.0f ? 0xff : (g <= 0.0f ? 0x00 : (unsigned long)(g * 255.0f + 0.5f));
		const unsigned long dwB = b >= 1.0f ? 0xff : (b <= 0.0f ? 0x00 : (unsigned long)(b * 255.0f + 0.5f));
		const unsigned long dwA = a >= 1.0f ? 0xff : (a <= 0.0f ? 0x00 : (unsigned long)(a * 255.0f + 0.5f));
		return (dwA << 24) | (dwR << 16) | (dwG << 8) | dwB;
	}

	// implicit conversions D3DXCOLOR had - game code relies on arithmetic results
	// (which are FrankColorBase, not Color) converting to DWORD, e.g. grass.cpp
	operator unsigned long () const							{ return ToDWORD(); }
	operator float* ()										{ return &r; }
	operator const float* () const							{ return &r; }

	FrankColorBase& operator += (const FrankColorBase& c)	{ r += c.r; g += c.g; b += c.b; a += c.a; return *this; }
	FrankColorBase& operator -= (const FrankColorBase& c)	{ r -= c.r; g -= c.g; b -= c.b; a -= c.a; return *this; }
	FrankColorBase& operator *= (float s)					{ r *= s; g *= s; b *= s; a *= s; return *this; }
	FrankColorBase& operator /= (float s)					{ const float i = 1.0f / s; r *= i; g *= i; b *= i; a *= i; return *this; }

	FrankColorBase operator + () const						{ return *this; }
	FrankColorBase operator - () const						{ return FrankColorBase(-r, -g, -b, -a); }
	FrankColorBase operator + (const FrankColorBase& c) const { return FrankColorBase(r + c.r, g + c.g, b + c.b, a + c.a); }
	FrankColorBase operator - (const FrankColorBase& c) const { return FrankColorBase(r - c.r, g - c.g, b - c.b, a - c.a); }
	FrankColorBase operator * (float s) const				{ return FrankColorBase(r*s, g*s, b*s, a*s); }
	FrankColorBase operator / (float s) const				{ const float i = 1.0f / s; return FrankColorBase(r*i, g*i, b*i, a*i); }
	friend FrankColorBase operator * (float s, const FrankColorBase& c) { return FrankColorBase(c.r*s, c.g*s, c.b*s, c.a*s); }

	bool operator == (const FrankColorBase& c) const		{ return r == c.r && g == c.g && b == c.b && a == c.a; }
	bool operator != (const FrankColorBase& c) const		{ return !(*this == c); }
};

////////////////////////////////////////////////////////////////////////////////////////
// FrankVec3Base - layout and semantics of D3DXVECTOR3
////////////////////////////////////////////////////////////////////////////////////////

struct FrankVec3Base
{
	float x, y, z;

	FrankVec3Base() {}
	FrankVec3Base(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	operator float* ()										{ return &x; }
	operator const float* () const							{ return &x; }

	FrankVec3Base& operator += (const FrankVec3Base& v)		{ x += v.x; y += v.y; z += v.z; return *this; }
	FrankVec3Base& operator -= (const FrankVec3Base& v)		{ x -= v.x; y -= v.y; z -= v.z; return *this; }
	FrankVec3Base& operator *= (float s)					{ x *= s; y *= s; z *= s; return *this; }
	FrankVec3Base& operator /= (float s)					{ const float i = 1.0f / s; x *= i; y *= i; z *= i; return *this; }

	FrankVec3Base operator + () const						{ return *this; }
	FrankVec3Base operator - () const						{ return FrankVec3Base(-x, -y, -z); }
	FrankVec3Base operator + (const FrankVec3Base& v) const	{ return FrankVec3Base(x + v.x, y + v.y, z + v.z); }
	FrankVec3Base operator - (const FrankVec3Base& v) const	{ return FrankVec3Base(x - v.x, y - v.y, z - v.z); }
	FrankVec3Base operator * (float s) const				{ return FrankVec3Base(x*s, y*s, z*s); }
	FrankVec3Base operator / (float s) const				{ const float i = 1.0f / s; return FrankVec3Base(x*i, y*i, z*i); }
	friend FrankVec3Base operator * (float s, const FrankVec3Base& v) { return FrankVec3Base(v.x*s, v.y*s, v.z*s); }

	bool operator == (const FrankVec3Base& v) const			{ return x == v.x && y == v.y && z == v.z; }
	bool operator != (const FrankVec3Base& v) const			{ return !(*this == v); }
};

inline float FrankVec3Length(const FrankVec3Base& v)		{ return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z); }
inline float FrankVec3LengthSq(const FrankVec3Base& v)		{ return v.x*v.x + v.y*v.y + v.z*v.z; }
inline float FrankVec3Dot(const FrankVec3Base& a, const FrankVec3Base& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

inline void FrankVec3Cross(FrankVec3Base& out, const FrankVec3Base& a, const FrankVec3Base& b)
{
	const FrankVec3Base result(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
	out = result;
}

inline void FrankVec3Normalize(FrankVec3Base& out, const FrankVec3Base& v)
{
	const float length = FrankVec3Length(v);
	if (length == 0)
		out = FrankVec3Base(0, 0, 0);	// matches D3DXVec3Normalize on zero input
	else
	{
		const float i = 1.0f / length;
		out = FrankVec3Base(v.x*i, v.y*i, v.z*i);
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// FrankMat44Base - layout and semantics of D3DXMATRIX (row-major, row-vector, LH)
////////////////////////////////////////////////////////////////////////////////////////

struct FrankMat44Base
{
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
			float _41, _42, _43, _44;
		};
		float m[4][4];
	};

	FrankMat44Base() {}

	// element-wise constructor matching D3DXMATRIX's
	FrankMat44Base(float m11, float m12, float m13, float m14,
				   float m21, float m22, float m23, float m24,
				   float m31, float m32, float m33, float m34,
				   float m41, float m42, float m43, float m44)
	{
		_11 = m11; _12 = m12; _13 = m13; _14 = m14;
		_21 = m21; _22 = m22; _23 = m23; _24 = m24;
		_31 = m31; _32 = m32; _33 = m33; _34 = m34;
		_41 = m41; _42 = m42; _43 = m43; _44 = m44;
	}

	float& operator () (int row, int col)					{ return m[row][col]; }
	float  operator () (int row, int col) const				{ return m[row][col]; }

	operator float* ()										{ return &_11; }
	operator const float* () const							{ return &_11; }

	FrankMat44Base operator * (const FrankMat44Base& b) const
	{
		FrankMat44Base out;
		for (int r = 0; r < 4; ++r)
		for (int c = 0; c < 4; ++c)
			out.m[r][c] = m[r][0]*b.m[0][c] + m[r][1]*b.m[1][c] + m[r][2]*b.m[2][c] + m[r][3]*b.m[3][c];
		return out;
	}

	FrankMat44Base& operator *= (const FrankMat44Base& b)	{ *this = *this * b; return *this; }
	FrankMat44Base& operator += (const FrankMat44Base& b)	{ for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m[i][j] += b.m[i][j]; return *this; }
	FrankMat44Base& operator -= (const FrankMat44Base& b)	{ for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m[i][j] -= b.m[i][j]; return *this; }
	FrankMat44Base& operator *= (float s)					{ for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m[i][j] *= s; return *this; }
	FrankMat44Base& operator /= (float s)					{ const float i2 = 1.0f / s; return *this *= i2; }

	FrankMat44Base operator + (const FrankMat44Base& b) const { FrankMat44Base out(*this); return out += b; }
	FrankMat44Base operator - (const FrankMat44Base& b) const { FrankMat44Base out(*this); return out -= b; }
	FrankMat44Base operator * (float s) const				{ FrankMat44Base out(*this); return out *= s; }
	FrankMat44Base operator / (float s) const				{ FrankMat44Base out(*this); return out /= s; }
	FrankMat44Base operator - () const						{ FrankMat44Base out(*this); return out *= -1.0f; }
	friend FrankMat44Base operator * (float s, const FrankMat44Base& b) { FrankMat44Base out(b); return out *= s; }

	bool operator == (const FrankMat44Base& b) const
	{
		for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) if (m[i][j] != b.m[i][j]) return false;
		return true;
	}
	bool operator != (const FrankMat44Base& b) const		{ return !(*this == b); }
};

inline void FrankMatrixIdentity(FrankMat44Base& out)
{
	for (int r = 0; r < 4; ++r)
	for (int c = 0; c < 4; ++c)
		out.m[r][c] = (r == c) ? 1.0f : 0.0f;
}

inline void FrankMatrixTranslation(FrankMat44Base& out, float x, float y, float z)
{
	FrankMatrixIdentity(out);
	out._41 = x; out._42 = y; out._43 = z;
}

inline void FrankMatrixScaling(FrankMat44Base& out, float x, float y, float z)
{
	FrankMatrixIdentity(out);
	out._11 = x; out._22 = y; out._33 = z;
}

inline void FrankMatrixRotationX(FrankMat44Base& out, float angle)
{
	const float c = cosf(angle), s = sinf(angle);
	FrankMatrixIdentity(out);
	out._22 = c; out._23 = s;
	out._32 = -s; out._33 = c;
}

inline void FrankMatrixRotationY(FrankMat44Base& out, float angle)
{
	const float c = cosf(angle), s = sinf(angle);
	FrankMatrixIdentity(out);
	out._11 = c; out._13 = -s;
	out._31 = s; out._33 = c;
}

inline void FrankMatrixRotationZ(FrankMat44Base& out, float angle)
{
	const float c = cosf(angle), s = sinf(angle);
	FrankMatrixIdentity(out);
	out._11 = c; out._12 = s;
	out._21 = -s; out._22 = c;
}

// matches D3DX: applies roll (Z), then pitch (X), then yaw (Y) in row-vector order
inline void FrankMatrixRotationYawPitchRoll(FrankMat44Base& out, float yaw, float pitch, float roll)
{
	FrankMat44Base rz, rx, ry;
	FrankMatrixRotationZ(rz, roll);
	FrankMatrixRotationX(rx, pitch);
	FrankMatrixRotationY(ry, yaw);
	out = rz * rx * ry;
}

// Rodrigues rotation, row-vector convention; axis must be normalized
inline void FrankMatrixRotationAxis(FrankMat44Base& out, const FrankVec3Base& axis, float angle)
{
	const float c = cosf(angle), s = sinf(angle), t = 1.0f - c;
	const float x = axis.x, y = axis.y, z = axis.z;
	FrankMatrixIdentity(out);
	out._11 = c + t*x*x;	out._12 = t*x*y + s*z;	out._13 = t*x*z - s*y;
	out._21 = t*x*y - s*z;	out._22 = c + t*y*y;	out._23 = t*y*z + s*x;
	out._31 = t*x*z + s*y;	out._32 = t*y*z - s*x;	out._33 = c + t*z*z;
}

inline void FrankMatrixTranspose(FrankMat44Base& out, const FrankMat44Base& in)
{
	const FrankMat44Base a = in;	// copy so out may alias in
	for (int r = 0; r < 4; ++r)
	for (int c = 0; c < 4; ++c)
		out.m[r][c] = a.m[c][r];
}

// general inverse via adjugate, same result as D3DXMatrixInverse
inline void FrankMatrixInverse(FrankMat44Base& out, const FrankMat44Base& in)
{
	const float* s = &in._11;
	float inv[16];

	inv[0] =  s[5]*s[10]*s[15] - s[5]*s[11]*s[14] - s[9]*s[6]*s[15] + s[9]*s[7]*s[14] + s[13]*s[6]*s[11] - s[13]*s[7]*s[10];
	inv[4] = -s[4]*s[10]*s[15] + s[4]*s[11]*s[14] + s[8]*s[6]*s[15] - s[8]*s[7]*s[14] - s[12]*s[6]*s[11] + s[12]*s[7]*s[10];
	inv[8] =  s[4]*s[9]*s[15] - s[4]*s[11]*s[13] - s[8]*s[5]*s[15] + s[8]*s[7]*s[13] + s[12]*s[5]*s[11] - s[12]*s[7]*s[9];
	inv[12] = -s[4]*s[9]*s[14] + s[4]*s[10]*s[13] + s[8]*s[5]*s[14] - s[8]*s[6]*s[13] - s[12]*s[5]*s[10] + s[12]*s[6]*s[9];
	inv[1] = -s[1]*s[10]*s[15] + s[1]*s[11]*s[14] + s[9]*s[2]*s[15] - s[9]*s[3]*s[14] - s[13]*s[2]*s[11] + s[13]*s[3]*s[10];
	inv[5] =  s[0]*s[10]*s[15] - s[0]*s[11]*s[14] - s[8]*s[2]*s[15] + s[8]*s[3]*s[14] + s[12]*s[2]*s[11] - s[12]*s[3]*s[10];
	inv[9] = -s[0]*s[9]*s[15] + s[0]*s[11]*s[13] + s[8]*s[1]*s[15] - s[8]*s[3]*s[13] - s[12]*s[1]*s[11] + s[12]*s[3]*s[9];
	inv[13] = s[0]*s[9]*s[14] - s[0]*s[10]*s[13] - s[8]*s[1]*s[14] + s[8]*s[2]*s[13] + s[12]*s[1]*s[10] - s[12]*s[2]*s[9];
	inv[2] =  s[1]*s[6]*s[15] - s[1]*s[7]*s[14] - s[5]*s[2]*s[15] + s[5]*s[3]*s[14] + s[13]*s[2]*s[7] - s[13]*s[3]*s[6];
	inv[6] = -s[0]*s[6]*s[15] + s[0]*s[7]*s[14] + s[4]*s[2]*s[15] - s[4]*s[3]*s[14] - s[12]*s[2]*s[7] + s[12]*s[3]*s[6];
	inv[10] = s[0]*s[5]*s[15] - s[0]*s[7]*s[13] - s[4]*s[1]*s[15] + s[4]*s[3]*s[13] + s[12]*s[1]*s[7] - s[12]*s[3]*s[5];
	inv[14] = -s[0]*s[5]*s[14] + s[0]*s[6]*s[13] + s[4]*s[1]*s[14] - s[4]*s[2]*s[13] - s[12]*s[1]*s[6] + s[12]*s[2]*s[5];
	inv[3] = -s[1]*s[6]*s[11] + s[1]*s[7]*s[10] + s[5]*s[2]*s[11] - s[5]*s[3]*s[10] - s[9]*s[2]*s[7] + s[9]*s[3]*s[6];
	inv[7] =  s[0]*s[6]*s[11] - s[0]*s[7]*s[10] - s[4]*s[2]*s[11] + s[4]*s[3]*s[10] + s[8]*s[2]*s[7] - s[8]*s[3]*s[6];
	inv[11] = -s[0]*s[5]*s[11] + s[0]*s[7]*s[9] + s[4]*s[1]*s[11] - s[4]*s[3]*s[9] - s[8]*s[1]*s[7] + s[8]*s[3]*s[5];
	inv[15] = s[0]*s[5]*s[10] - s[0]*s[6]*s[9] - s[4]*s[1]*s[10] + s[4]*s[2]*s[9] + s[8]*s[1]*s[6] - s[8]*s[2]*s[5];

	float det = s[0]*inv[0] + s[1]*inv[4] + s[2]*inv[8] + s[3]*inv[12];
	if (det != 0)
		det = 1.0f / det;

	float* d = &out._11;
	for (int i = 0; i < 16; ++i)
		d[i] = inv[i] * det;
}

inline void FrankMatrixLookAtLH(FrankMat44Base& out, const FrankVec3Base& eye, const FrankVec3Base& at, const FrankVec3Base& up)
{
	FrankVec3Base zaxis, xaxis, yaxis;
	FrankVec3Normalize(zaxis, at - eye);
	FrankVec3Cross(xaxis, up, zaxis);
	FrankVec3Normalize(xaxis, xaxis);
	FrankVec3Cross(yaxis, zaxis, xaxis);

	out._11 = xaxis.x;	out._12 = yaxis.x;	out._13 = zaxis.x;	out._14 = 0;
	out._21 = xaxis.y;	out._22 = yaxis.y;	out._23 = zaxis.y;	out._24 = 0;
	out._31 = xaxis.z;	out._32 = yaxis.z;	out._33 = zaxis.z;	out._34 = 0;
	out._41 = -FrankVec3Dot(xaxis, eye);
	out._42 = -FrankVec3Dot(yaxis, eye);
	out._43 = -FrankVec3Dot(zaxis, eye);
	out._44 = 1;
}

inline void FrankMatrixOrthoLH(FrankMat44Base& out, float width, float height, float zNear, float zFar)
{
	FrankMatrixIdentity(out);
	out._11 = 2.0f / width;
	out._22 = 2.0f / height;
	out._33 = 1.0f / (zFar - zNear);
	out._43 = -zNear / (zFar - zNear);
}

inline void FrankVec3TransformCoord(FrankVec3Base& out, const FrankVec3Base& v, const FrankMat44Base& mat)
{
	const float x = v.x*mat._11 + v.y*mat._21 + v.z*mat._31 + mat._41;
	const float y = v.x*mat._12 + v.y*mat._22 + v.z*mat._32 + mat._42;
	const float z = v.x*mat._13 + v.y*mat._23 + v.z*mat._33 + mat._43;
	const float w = v.x*mat._14 + v.y*mat._24 + v.z*mat._34 + mat._44;
	const float i = (w == 0) ? 1.0f : 1.0f / w;
	out = FrankVec3Base(x*i, y*i, z*i);
}

inline void FrankVec3TransformNormal(FrankVec3Base& out, const FrankVec3Base& v, const FrankMat44Base& mat)
{
	const FrankVec3Base result(
		v.x*mat._11 + v.y*mat._21 + v.z*mat._31,
		v.x*mat._12 + v.y*mat._22 + v.z*mat._32,
		v.x*mat._13 + v.y*mat._23 + v.z*mat._33);
	out = result;
}

////////////////////////////////////////////////////////////////////////////////////////
// FrankQuatBase - layout and semantics of D3DXQUATERNION
////////////////////////////////////////////////////////////////////////////////////////

struct FrankQuatBase
{
	float x, y, z, w;

	FrankQuatBase() {}
	FrankQuatBase(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

	// D3DX order: Out = Q1 * Q2 applies Q1's rotation, then Q2's
	FrankQuatBase operator * (const FrankQuatBase& q) const
	{
		return FrankQuatBase(
			q.w*x + q.x*w + q.y*z - q.z*y,
			q.w*y + q.y*w + q.z*x - q.x*z,
			q.w*z + q.z*w + q.x*y - q.y*x,
			q.w*w - q.x*x - q.y*y - q.z*z);
	}

	FrankQuatBase& operator *= (const FrankQuatBase& q)		{ *this = *this * q; return *this; }
	FrankQuatBase& operator += (const FrankQuatBase& q)		{ x += q.x; y += q.y; z += q.z; w += q.w; return *this; }
	FrankQuatBase& operator -= (const FrankQuatBase& q)		{ x -= q.x; y -= q.y; z -= q.z; w -= q.w; return *this; }
	FrankQuatBase& operator *= (float s)					{ x *= s; y *= s; z *= s; w *= s; return *this; }
	FrankQuatBase& operator /= (float s)					{ const float i = 1.0f / s; x *= i; y *= i; z *= i; w *= i; return *this; }

	FrankQuatBase operator + (const FrankQuatBase& q) const	{ return FrankQuatBase(x + q.x, y + q.y, z + q.z, w + q.w); }
	FrankQuatBase operator - (const FrankQuatBase& q) const	{ return FrankQuatBase(x - q.x, y - q.y, z - q.z, w - q.w); }
	FrankQuatBase operator * (float s) const				{ return FrankQuatBase(x*s, y*s, z*s, w*s); }
	FrankQuatBase operator / (float s) const				{ const float i = 1.0f / s; return FrankQuatBase(x*i, y*i, z*i, w*i); }
	FrankQuatBase operator - () const						{ return FrankQuatBase(-x, -y, -z, -w); }
	friend FrankQuatBase operator * (float s, const FrankQuatBase& q) { return FrankQuatBase(q.x*s, q.y*s, q.z*s, q.w*s); }

	bool operator == (const FrankQuatBase& q) const			{ return x == q.x && y == q.y && z == q.z && w == q.w; }
	bool operator != (const FrankQuatBase& q) const			{ return !(*this == q); }
};

inline void FrankQuaternionIdentity(FrankQuatBase& out)		{ out = FrankQuatBase(0, 0, 0, 1); }
inline float FrankQuaternionDot(const FrankQuatBase& a, const FrankQuatBase& b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }

inline void FrankQuaternionNormalize(FrankQuatBase& out, const FrankQuatBase& q)
{
	const float length = sqrtf(FrankQuaternionDot(q, q));
	if (length == 0)
		out = FrankQuatBase(0, 0, 0, 0);	// matches D3DX on zero input
	else
	{
		const float i = 1.0f / length;
		out = FrankQuatBase(q.x*i, q.y*i, q.z*i, q.w*i);
	}
}

// conjugate / normSq, same as D3DXQuaternionInverse
inline void FrankQuaternionInverse(FrankQuatBase& out, const FrankQuatBase& q)
{
	const float normSq = FrankQuaternionDot(q, q);
	const float i = (normSq == 0) ? 0.0f : 1.0f / normSq;
	out = FrankQuatBase(-q.x*i, -q.y*i, -q.z*i, q.w*i);
}

// axis must be normalized
inline void FrankQuaternionRotationAxis(FrankQuatBase& out, const FrankVec3Base& axis, float angle)
{
	const float h = angle * 0.5f, s = sinf(h);
	out = FrankQuatBase(axis.x*s, axis.y*s, axis.z*s, cosf(h));
}

// matches D3DX and FrankMatrixRotationYawPitchRoll: roll, then pitch, then yaw
inline void FrankQuaternionRotationYawPitchRoll(FrankQuatBase& out, float yaw, float pitch, float roll)
{
	FrankQuatBase qz, qx, qy;
	FrankQuaternionRotationAxis(qz, FrankVec3Base(0, 0, 1), roll);
	FrankQuaternionRotationAxis(qx, FrankVec3Base(1, 0, 0), pitch);
	FrankQuaternionRotationAxis(qy, FrankVec3Base(0, 1, 0), yaw);
	out = qz * qx * qy;
}

// max-diagonal branch method; matrix must be a pure rotation
inline void FrankQuaternionRotationMatrix(FrankQuatBase& out, const FrankMat44Base& m)
{
	const float trace = m._11 + m._22 + m._33;
	if (trace > 0)
	{
		const float s = sqrtf(trace + 1.0f) * 2;
		out = FrankQuatBase((m._23 - m._32)/s, (m._31 - m._13)/s, (m._12 - m._21)/s, 0.25f*s);
	}
	else if (m._11 > m._22 && m._11 > m._33)
	{
		const float s = sqrtf(1.0f + m._11 - m._22 - m._33) * 2;
		out = FrankQuatBase(0.25f*s, (m._21 + m._12)/s, (m._31 + m._13)/s, (m._23 - m._32)/s);
	}
	else if (m._22 > m._33)
	{
		const float s = sqrtf(1.0f + m._22 - m._11 - m._33) * 2;
		out = FrankQuatBase((m._21 + m._12)/s, 0.25f*s, (m._32 + m._23)/s, (m._31 - m._13)/s);
	}
	else
	{
		const float s = sqrtf(1.0f + m._33 - m._11 - m._22) * 2;
		out = FrankQuatBase((m._31 + m._13)/s, (m._32 + m._23)/s, 0.25f*s, (m._12 - m._21)/s);
	}
}

inline void FrankMatrixRotationQuaternion(FrankMat44Base& out, const FrankQuatBase& q)
{
	const float x = q.x, y = q.y, z = q.z, w = q.w;
	FrankMatrixIdentity(out);
	out._11 = 1 - 2*(y*y + z*z);	out._12 = 2*(x*y + z*w);		out._13 = 2*(x*z - y*w);
	out._21 = 2*(x*y - z*w);		out._22 = 1 - 2*(x*x + z*z);	out._23 = 2*(y*z + x*w);
	out._31 = 2*(x*z + y*w);		out._32 = 2*(y*z - x*w);		out._33 = 1 - 2*(x*x + y*y);
}

// shortest-arc slerp with lerp fallback for nearly-parallel quats, matching D3DX
inline void FrankQuaternionSlerp(FrankQuatBase& out, const FrankQuatBase& a, const FrankQuatBase& b, float t)
{
	float d = FrankQuaternionDot(a, b);
	FrankQuatBase b2 = b;
	if (d < 0)
	{
		d = -d;
		b2 = -b;
	}

	float wa, wb;
	if (d > 0.9999f)
	{
		// nearly parallel - lerp and normalize
		wa = 1.0f - t;
		wb = t;
	}
	else
	{
		const float theta = acosf(d), s = sinf(theta);
		wa = sinf((1.0f - t) * theta) / s;
		wb = sinf(t * theta) / s;
	}

	FrankQuatBase result(a.x*wa + b2.x*wb, a.y*wa + b2.y*wb, a.z*wa + b2.z*wb, a.w*wa + b2.w*wb);
	FrankQuaternionNormalize(out, result);
}

// D3DX semantics: axis is NOT normalized on output
inline void FrankQuaternionToAxisAngle(const FrankQuatBase& q, FrankVec3Base& axis, float& angle)
{
	axis = FrankVec3Base(q.x, q.y, q.z);
	const float w = q.w > 1.0f ? 1.0f : (q.w < -1.0f ? -1.0f : q.w);
	angle = 2.0f * acosf(w);
}

} // namespace FrankMath
