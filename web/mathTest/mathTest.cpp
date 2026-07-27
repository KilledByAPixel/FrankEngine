////////////////////////////////////////////////////////////////////////////////////////
/*
	Phase 1 math parity suite - see trunk/docs/plans/2026-07-22-piroot-web-phase1-math.md

	Compiled 3 ways, same checks every time:
	  MATHTEST_USE_D3DX  - Windows, analytic checks against real D3DX (proves expectations)
	  MATHTEST_COMPARE   - Windows, D3DX and frankMathBase head-to-head (proves parity)
	  (default)          - frankMathBase only, also builds under em++ (proves portability)
*/
////////////////////////////////////////////////////////////////////////////////////////

#include <cstdio>
#include <cmath>

static int failCount = 0;
static int checkCount = 0;
#define CHECK_NEAR(a, b) \
	do { ++checkCount; if (fabs((double)(a) - (double)(b)) > 1e-4) { \
		printf("FAIL %d  %s=%g  %s=%g\n", __LINE__, #a, (double)(a), #b, (double)(b)); \
		++failCount; } } while(0)
#define CHECK_TRUE(c) \
	do { ++checkCount; if (!(c)) { printf("FAIL %d  %s\n", __LINE__, #c); ++failCount; } } while(0)

static const float TEST_PI = 3.14159265358979f;

#if defined(MATHTEST_USE_D3DX) || defined(MATHTEST_COMPARE)
#include <d3dx9math.h>
#endif

#ifdef MATHTEST_USE_D3DX

////////////////////////////////////////////////////////////////////////////////////////
// Config A: analytic checks against real D3DX
////////////////////////////////////////////////////////////////////////////////////////

static void CheckV3(const D3DXVECTOR3& v, float x, float y, float z)
{
	CHECK_NEAR(v.x, x); CHECK_NEAR(v.y, y); CHECK_NEAR(v.z, z);
}

static void CheckIdentity(const D3DXMATRIX& m)
{
	for (int r = 0; r < 4; ++r)
	for (int c = 0; c < 4; ++c)
		CHECK_NEAR(m(r, c), (r == c) ? 1.0f : 0.0f);
}

static void PrintGoldenM(const char* name, const D3DXMATRIX& m)
{
	printf("golden: %s =", name);
	for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) printf(" %.6f", m(r, c));
	printf("\n");
}

int main()
{
	// --- vector basics ---
	D3DXVECTOR3 a(3, -4, 12), b(2, 0.5f, -1), out;
	CHECK_NEAR(D3DXVec3Length(&a), 13.0f);
	CHECK_NEAR(D3DXVec3LengthSq(&a), 169.0f);
	CHECK_NEAR(D3DXVec3Dot(&a, &b), 3*2 + -4*0.5f + 12*-1);
	D3DXVec3Cross(&out, &a, &b);
	CheckV3(out, (-4)*(-1) - 12*0.5f, 12*2 - 3*(-1), 3*0.5f - (-4)*2);
	D3DXVec3Normalize(&out, &a);
	CheckV3(out, 3/13.0f, -4/13.0f, 12/13.0f);

	// --- rotation Z: row-vector convention, X axis rotates toward Y ---
	D3DXMATRIX rz;
	D3DXMatrixRotationZ(&rz, TEST_PI/2);
	D3DXVECTOR3 xAxis(1, 0, 0);
	D3DXVec3TransformCoord(&out, &xAxis, &rz);
	CheckV3(out, 0, 1, 0);
	D3DXMatrixRotationZ(&rz, TEST_PI/6);
	D3DXVec3TransformCoord(&out, &xAxis, &rz);
	CheckV3(out, cosf(TEST_PI/6), sinf(TEST_PI/6), 0);

	// --- translation: TransformCoord applies it, TransformNormal ignores it ---
	D3DXMATRIX tr;
	D3DXMatrixTranslation(&tr, 10, -20, 30);
	D3DXVECTOR3 p(1, 2, 3);
	D3DXVec3TransformCoord(&out, &p, &tr);
	CheckV3(out, 11, -18, 33);
	D3DXVec3TransformNormal(&out, &p, &tr);
	CheckV3(out, 1, 2, 3);

	// --- composed TRS, inverse, transpose ---
	D3DXMATRIX s, rot, trs, inv, m2;
	D3DXMatrixScaling(&s, 2, 3, 4);
	D3DXMatrixRotationYawPitchRoll(&rot, 0.7f, -0.3f, 1.9f);
	trs = s * rot * tr;						// row-vector: scale, then rotate, then translate
	D3DXMatrixInverse(&inv, NULL, &trs);
	m2 = trs * inv;
	CheckIdentity(m2);
	D3DXMatrixTranspose(&m2, &trs);
	D3DXMatrixTranspose(&m2, &m2);
	for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) CHECK_NEAR(m2(r, c), trs(r, c));
	PrintGoldenM("trs", trs);
	PrintGoldenM("trsInverse", inv);

	// --- rotation about arbitrary axis matches quaternion route ---
	D3DXVECTOR3 axis(1, 2, -0.5f);
	D3DXVec3Normalize(&axis, &axis);
	D3DXMATRIX ra, rq;
	D3DXMatrixRotationAxis(&ra, &axis, 1.23f);
	D3DXQUATERNION q, q2;
	D3DXQuaternionRotationAxis(&q, &axis, 1.23f);
	D3DXMatrixRotationQuaternion(&rq, &q);
	for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) CHECK_NEAR(rq(r, c), ra(r, c));

	// --- YPR: matrix route == quaternion route ---
	D3DXMatrixRotationYawPitchRoll(&rot, 0.7f, -0.3f, 1.9f);
	D3DXQuaternionRotationYawPitchRoll(&q, 0.7f, -0.3f, 1.9f);
	D3DXMatrixRotationQuaternion(&rq, &q);
	for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) CHECK_NEAR(rq(r, c), rot(r, c));

	// --- quaternion round trip matrix -> quat -> matrix ---
	D3DXQuaternionRotationMatrix(&q2, &rot);
	CHECK_TRUE(fabs(D3DXQuaternionDot(&q, &q2)) > 1 - 1e-4);	// q == +-q2

	// --- quaternion normalize / inverse / dot ---
	D3DXQUATERNION qraw(1, 2, 3, 4), qn, qi, qid;
	D3DXQuaternionNormalize(&qn, &qraw);
	CHECK_NEAR(sqrtf(qn.x*qn.x + qn.y*qn.y + qn.z*qn.z + qn.w*qn.w), 1.0f);
	D3DXQuaternionInverse(&qi, &qn);
	qid = qn * qi;								// should be identity rotation
	CHECK_NEAR(qid.w, 1.0f); CHECK_NEAR(qid.x, 0.0f); CHECK_NEAR(qid.y, 0.0f); CHECK_NEAR(qid.z, 0.0f);

	// --- slerp: endpoints exact, midpoint of identity->RotZ(90) == RotZ(45) ---
	D3DXQUATERNION qa, qb, qs, qexpect;
	D3DXQuaternionIdentity(&qa);
	D3DXVECTOR3 zAxis(0, 0, 1);
	D3DXQuaternionRotationAxis(&qb, &zAxis, TEST_PI/2);
	D3DXQuaternionSlerp(&qs, &qa, &qb, 0.0f);
	CHECK_NEAR(D3DXQuaternionDot(&qs, &qa), 1.0f);
	D3DXQuaternionSlerp(&qs, &qa, &qb, 1.0f);
	CHECK_TRUE(fabs(D3DXQuaternionDot(&qs, &qb)) > 1 - 1e-4);
	D3DXQuaternionSlerp(&qs, &qa, &qb, 0.5f);
	D3DXQuaternionRotationAxis(&qexpect, &zAxis, TEST_PI/4);
	CHECK_TRUE(fabs(D3DXQuaternionDot(&qs, &qexpect)) > 1 - 1e-4);
	printf("golden: slerpMid = %.6f %.6f %.6f %.6f\n", qs.x, qs.y, qs.z, qs.w);

	// --- axis-angle round trip ---
	D3DXVECTOR3 axisOut;
	float angleOut;
	D3DXQuaternionToAxisAngle(&qb, &axisOut, &angleOut);
	D3DXVec3Normalize(&axisOut, &axisOut);
	CHECK_NEAR(angleOut, TEST_PI/2);
	CheckV3(axisOut, 0, 0, 1);

	// --- lookAtLH: eye maps to origin, target maps to +Z at its distance ---
	D3DXVECTOR3 eye(0, 0, -10), at(0, 0, 0), up(0, 1, 0);
	D3DXMATRIX view;
	D3DXMatrixLookAtLH(&view, &eye, &at, &up);
	D3DXVec3TransformCoord(&out, &eye, &view);
	CheckV3(out, 0, 0, 0);
	D3DXVec3TransformCoord(&out, &at, &view);
	CheckV3(out, 0, 0, 10);
	PrintGoldenM("lookAt", view);

	// --- color DWORD packing: saturating ARGB8 ---
	CHECK_TRUE((DWORD)D3DXCOLOR(1, 1, 1, 1) == 0xFFFFFFFFu);
	CHECK_TRUE((DWORD)D3DXCOLOR(0, 0, 0, 0) == 0x00000000u);
	CHECK_TRUE((DWORD)D3DXCOLOR(1.5f, -0.5f, 0.5f, 1) == 0xFFFF0080u);	// sat, sat, round(127.5+.5)
	printf("golden: colorHalf = 0x%08X\n", (unsigned)(DWORD)D3DXCOLOR(0.5f, 0.25f, 0.75f, 0.1f));

	if (failCount == 0)
		printf("ALL PASS (%d checks)\n", checkCount);
	else
		printf("%d/%d CHECKS FAILED\n", failCount, checkCount);
	return failCount == 0 ? 0 : 1;
}

#elif defined(MATHTEST_COMPARE)

////////////////////////////////////////////////////////////////////////////////////////
// Config B: D3DX vs frankMathBase head-to-head - the real drop-in-equivalence proof
////////////////////////////////////////////////////////////////////////////////////////

#include "../../FrankEngine/Source/Core/frankMathBase.h"
using namespace FrankMath;

static void CmpV3(const FrankVec3Base& f, const D3DXVECTOR3& d)
{
	CHECK_NEAR(f.x, d.x); CHECK_NEAR(f.y, d.y); CHECK_NEAR(f.z, d.z);
}

static void CmpM(const FrankMat44Base& f, const D3DXMATRIX& d)
{
	for (int r = 0; r < 4; ++r)
	for (int c = 0; c < 4; ++c)
		CHECK_NEAR(f(r, c), d(r, c));
}

// quaternions compare up to global sign (q and -q are the same rotation)
static void CmpQ(const FrankQuatBase& f, const D3DXQUATERNION& d)
{
	const float sign = (f.x*d.x + f.y*d.y + f.z*d.z + f.w*d.w) < 0 ? -1.0f : 1.0f;
	CHECK_NEAR(f.x*sign, d.x); CHECK_NEAR(f.y*sign, d.y);
	CHECK_NEAR(f.z*sign, d.z); CHECK_NEAR(f.w*sign, d.w);
}

// same quat both ways from one input triple
static FrankQuatBase FQ(float x, float y, float z, float w) { return FrankQuatBase(x, y, z, w); }
static D3DXQUATERNION DQ(float x, float y, float z, float w) { return D3DXQUATERNION(x, y, z, w); }

int main()
{
	const float angles[] = { 0.0f, 0.5236f, 1.5708f, 2.1538f, -0.7f, 3.1f, -2.9f };
	const int numAngles = sizeof(angles) / sizeof(angles[0]);
	const float vecs[][3] = { {1,0,0}, {0,1,0}, {0,0,1}, {3,-4,12}, {-0.2f,7,1.5f}, {2,2,2} };
	const int numVecs = sizeof(vecs) / sizeof(vecs[0]);

	// --- vec3 functions ---
	for (int i = 0; i < numVecs; ++i)
	for (int j = 0; j < numVecs; ++j)
	{
		const FrankVec3Base fa(vecs[i][0], vecs[i][1], vecs[i][2]), fb(vecs[j][0], vecs[j][1], vecs[j][2]);
		const D3DXVECTOR3 da(vecs[i][0], vecs[i][1], vecs[i][2]), db(vecs[j][0], vecs[j][1], vecs[j][2]);
		CHECK_NEAR(FrankVec3Length(fa), D3DXVec3Length(&da));
		CHECK_NEAR(FrankVec3LengthSq(fa), D3DXVec3LengthSq(&da));
		CHECK_NEAR(FrankVec3Dot(fa, fb), D3DXVec3Dot(&da, &db));
		FrankVec3Base fo; D3DXVECTOR3 dv;
		FrankVec3Cross(fo, fa, fb); D3DXVec3Cross(&dv, &da, &db); CmpV3(fo, dv);
		FrankVec3Normalize(fo, fa); D3DXVec3Normalize(&dv, &da); CmpV3(fo, dv);
	}

	// --- simple matrix builders ---
	for (int i = 0; i < numAngles; ++i)
	{
		FrankMat44Base f; D3DXMATRIX d;
		FrankMatrixRotationX(f, angles[i]); D3DXMatrixRotationX(&d, angles[i]); CmpM(f, d);
		FrankMatrixRotationY(f, angles[i]); D3DXMatrixRotationY(&d, angles[i]); CmpM(f, d);
		FrankMatrixRotationZ(f, angles[i]); D3DXMatrixRotationZ(&d, angles[i]); CmpM(f, d);
	}
	{
		FrankMat44Base f; D3DXMATRIX d;
		FrankMatrixTranslation(f, 10, -20, 30); D3DXMatrixTranslation(&d, 10, -20, 30); CmpM(f, d);
		FrankMatrixScaling(f, 2, -3, 0.5f); D3DXMatrixScaling(&d, 2, -3, 0.5f); CmpM(f, d);
		FrankMatrixIdentity(f); D3DXMatrixIdentity(&d); CmpM(f, d);
	}

	// --- yaw/pitch/roll, matrix and quaternion routes ---
	for (int i = 0; i < numAngles; ++i)
	for (int j = 0; j < numAngles; ++j)
	{
		const float yaw = angles[i], pitch = angles[j], roll = angles[(i + j) % numAngles];
		FrankMat44Base f; D3DXMATRIX d;
		FrankMatrixRotationYawPitchRoll(f, yaw, pitch, roll);
		D3DXMatrixRotationYawPitchRoll(&d, yaw, pitch, roll);
		CmpM(f, d);
		FrankQuatBase fq; D3DXQUATERNION dq;
		FrankQuaternionRotationYawPitchRoll(fq, yaw, pitch, roll);
		D3DXQuaternionRotationYawPitchRoll(&dq, yaw, pitch, roll);
		CmpQ(fq, dq);
	}

	// --- axis rotations, both routes, plus matrix<->quat round trips ---
	for (int i = 0; i < numVecs; ++i)
	for (int j = 0; j < numAngles; ++j)
	{
		FrankVec3Base faxis(vecs[i][0], vecs[i][1], vecs[i][2]);
		D3DXVECTOR3 daxis(vecs[i][0], vecs[i][1], vecs[i][2]);
		FrankVec3Normalize(faxis, faxis);
		D3DXVec3Normalize(&daxis, &daxis);

		FrankMat44Base f; D3DXMATRIX d;
		FrankMatrixRotationAxis(f, faxis, angles[j]);
		D3DXMatrixRotationAxis(&d, &daxis, angles[j]);
		CmpM(f, d);

		FrankQuatBase fq; D3DXQUATERNION dq;
		FrankQuaternionRotationAxis(fq, faxis, angles[j]);
		D3DXQuaternionRotationAxis(&dq, &daxis, angles[j]);
		CmpQ(fq, dq);

		// matrix from quat
		FrankMat44Base fmq; D3DXMATRIX dmq;
		FrankMatrixRotationQuaternion(fmq, fq);
		D3DXMatrixRotationQuaternion(&dmq, &dq);
		CmpM(fmq, dmq);

		// quat from matrix
		FrankQuatBase fq2; D3DXQUATERNION dq2;
		FrankQuaternionRotationMatrix(fq2, f);
		D3DXQuaternionRotationMatrix(&dq2, &d);
		CmpQ(fq2, dq2);

		// axis-angle round trip (D3DX leaves axis unnormalized; compare raw)
		FrankVec3Base fao; float fang;
		D3DXVECTOR3 dao; float dang;
		FrankQuaternionToAxisAngle(fq, fao, fang);
		D3DXQuaternionToAxisAngle(&dq, &dao, &dang);
		CmpV3(fao, dao);
		CHECK_NEAR(fang, dang);
	}

	// --- composed TRS: multiply, inverse, transpose, transforms ---
	{
		FrankMat44Base fs, fr, ft, ftrs, finv, ftr;
		D3DXMATRIX ds, dr, dt, dtrs, dinv, dtr;
		FrankMatrixScaling(fs, 2, 3, 4);			D3DXMatrixScaling(&ds, 2, 3, 4);
		FrankMatrixRotationYawPitchRoll(fr, 0.7f, -0.3f, 1.9f);
		D3DXMatrixRotationYawPitchRoll(&dr, 0.7f, -0.3f, 1.9f);
		FrankMatrixTranslation(ft, 10, -20, 30);	D3DXMatrixTranslation(&dt, 10, -20, 30);
		ftrs = fs * fr * ft;						dtrs = ds * dr * dt;
		CmpM(ftrs, dtrs);
		FrankMatrixInverse(finv, ftrs);				D3DXMatrixInverse(&dinv, NULL, &dtrs);
		CmpM(finv, dinv);
		FrankMatrixTranspose(ftr, ftrs);			D3DXMatrixTranspose(&dtr, &dtrs);
		CmpM(ftr, dtr);

		for (int i = 0; i < numVecs; ++i)
		{
			const FrankVec3Base fv(vecs[i][0], vecs[i][1], vecs[i][2]);
			const D3DXVECTOR3 dv(vecs[i][0], vecs[i][1], vecs[i][2]);
			FrankVec3Base fo; D3DXVECTOR3 dvo;
			FrankVec3TransformCoord(fo, fv, ftrs); D3DXVec3TransformCoord(&dvo, &dv, &dtrs); CmpV3(fo, dvo);
			FrankVec3TransformNormal(fo, fv, ftrs); D3DXVec3TransformNormal(&dvo, &dv, &dtrs); CmpV3(fo, dvo);
		}
	}

	// --- lookAtLH ---
	{
		const float eyes[][3] = { {0,0,-10}, {5,3,-2}, {-1,20,4} };
		for (int i = 0; i < 3; ++i)
		{
			FrankMat44Base f; D3DXMATRIX d;
			FrankMatrixLookAtLH(f, FrankVec3Base(eyes[i][0], eyes[i][1], eyes[i][2]), FrankVec3Base(0, 1, 2), FrankVec3Base(0, 1, 0));
			D3DXVECTOR3 de(eyes[i][0], eyes[i][1], eyes[i][2]), da(0, 1, 2), du(0, 1, 0);
			D3DXMatrixLookAtLH(&d, &de, &da, &du);
			CmpM(f, d);
		}
	}

	// --- orthoLH (added for camera.cpp's projection matrix) ---
	{
		const float dims[][4] = { {20, 15, 1, 1000}, {64, 36, 0.1f, 10}, {2, 2, -1, 1} };
		for (int i = 0; i < 3; ++i)
		{
			FrankMat44Base f; D3DXMATRIX d;
			FrankMatrixOrthoLH(f, dims[i][0], dims[i][1], dims[i][2], dims[i][3]);
			D3DXMatrixOrthoLH(&d, dims[i][0], dims[i][1], dims[i][2], dims[i][3]);
			CmpM(f, d);
		}
	}

	// --- quaternion ops: multiply order, inverse, normalize, dot, slerp ---
	{
		const FrankQuatBase fq1n = FQ(1, 2, 3, 4), fq2n = FQ(-2, 0.5f, 1, 3);
		const D3DXQUATERNION dq1n = DQ(1, 2, 3, 4), dq2n = DQ(-2, 0.5f, 1, 3);
		FrankQuatBase fq1, fq2, fo;
		D3DXQUATERNION dq1, dq2, dw;
		FrankQuaternionNormalize(fq1, fq1n);	D3DXQuaternionNormalize(&dq1, &dq1n);	CmpQ(fq1, dq1);
		FrankQuaternionNormalize(fq2, fq2n);	D3DXQuaternionNormalize(&dq2, &dq2n);
		fo = fq1 * fq2;							dw = dq1 * dq2;							CmpQ(fo, dw);
		FrankQuaternionInverse(fo, fq1n);		D3DXQuaternionInverse(&dw, &dq1n);		CmpQ(fo, dw);
		CHECK_NEAR(FrankQuaternionDot(fq1, fq2), D3DXQuaternionDot(&dq1, &dq2));

		const float ts[] = { 0.0f, 0.13f, 0.5f, 0.87f, 1.0f };
		for (int i = 0; i < 5; ++i)
		{
			FrankQuaternionSlerp(fo, fq1, fq2, ts[i]);
			D3DXQuaternionSlerp(&dw, &dq1, &dq2, ts[i]);
			CmpQ(fo, dw);
			// opposite-hemisphere pair exercises the shortest-arc negation
			FrankQuaternionSlerp(fo, fq1, -fq2, ts[i]);
			D3DXQUATERNION dq2neg(-dq2.x, -dq2.y, -dq2.z, -dq2.w);
			D3DXQuaternionSlerp(&dw, &dq1, &dq2neg, ts[i]);
			CmpQ(fo, dw);
			// nearly-parallel pair exercises the lerp fallback
			D3DXQUATERNION dqClose(dq1.x + 1e-5f, dq1.y, dq1.z, dq1.w);
			D3DXQuaternionNormalize(&dqClose, &dqClose);
			FrankQuatBase fqClose(dqClose.x, dqClose.y, dqClose.z, dqClose.w);
			FrankQuaternionSlerp(fo, fq1, fqClose, ts[i]);
			D3DXQuaternionSlerp(&dw, &dq1, &dqClose, ts[i]);
			CmpQ(fo, dw);
		}
	}

	// --- color: DWORD pack and unpack across the range, incl out-of-range ---
	{
		const float colorVals[] = { -0.5f, 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.999f, 1.0f, 1.5f };
		for (int i = 0; i < 9; ++i)
		for (int j = 0; j < 9; ++j)
		{
			const float r = colorVals[i], g = colorVals[j], b = colorVals[(i+j) % 9], a = colorVals[(i*3+j) % 9];
			CHECK_TRUE(FrankColorBase(r, g, b, a).ToDWORD() == (DWORD)D3DXCOLOR(r, g, b, a));
		}
		const DWORD packed[] = { 0x00000000u, 0xFFFFFFFFu, 0x1A8040BFu, 0x80FF0001u };
		for (int i = 0; i < 4; ++i)
		{
			const FrankColorBase fc((unsigned long)packed[i]);
			const D3DXCOLOR dc(packed[i]);
			CHECK_NEAR(fc.r, dc.r); CHECK_NEAR(fc.g, dc.g); CHECK_NEAR(fc.b, dc.b); CHECK_NEAR(fc.a, dc.a);
		}
	}

	// layout compatibility - the Windows interop casts depend on this
	CHECK_TRUE(sizeof(FrankColorBase) == sizeof(D3DXCOLOR));
	CHECK_TRUE(sizeof(FrankVec3Base) == sizeof(D3DXVECTOR3));
	CHECK_TRUE(sizeof(FrankMat44Base) == sizeof(D3DXMATRIX));
	CHECK_TRUE(sizeof(FrankQuatBase) == sizeof(D3DXQUATERNION));

	if (failCount == 0)
		printf("ALL PASS (%d checks)\n", checkCount);
	else
		printf("%d/%d CHECKS FAILED\n", failCount, checkCount);
	return failCount == 0 ? 0 : 1;
}

#else

////////////////////////////////////////////////////////////////////////////////////////
// Config C: the real frankMath public API on frankMathBase - no D3DX anywhere,
// builds under cl AND em++. Cross-checked against frankMathBase directly, which
// config B proved numerically identical to D3DX.
////////////////////////////////////////////////////////////////////////////////////////

// minimal shim for the engine environment frankMath.h expects from the PCH
#include <cassert>
#include <cstdlib>
#include <climits>
#include <cstring>
typedef float FLOAT;
typedef int INT;
typedef unsigned int UINT;
typedef unsigned long DWORD;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef char INT8;
typedef short INT16;
typedef int INT32;
typedef long LONG;
typedef wchar_t WCHAR;
// frankMath.h defines its own ASSERT; Box2AABB has an operator RECT (win32 type)
struct RECT { LONG left; LONG top; LONG right; LONG bottom; };
// engine global used by Interpolated<T>::GetInterpolated (normally from frankEngine.h)
float g_interpolatePercent = 0;
#include "Box2D/Box2D.h"

#include "../../FrankEngine/Source/Core/frankMath.h"
using namespace FrankMath;

static void CmpV3(const Vector3& a, const FrankVec3Base& b)
{
	CHECK_NEAR(a.x, b.x); CHECK_NEAR(a.y, b.y); CHECK_NEAR(a.z, b.z);
}

static void CmpM(const Matrix44& a, const FrankMat44Base& b)
{
	for (int r = 0; r < 4; ++r)
	for (int c = 0; c < 4; ++c)
		CHECK_NEAR(a(r, c), b(r, c));
}

static void CmpQ(const Quaternion& a, const FrankQuatBase& b)
{
	const FrankQuatBase& qa = a.GetQuaternionBase();
	const float sign = FrankQuaternionDot(qa, b) < 0 ? -1.0f : 1.0f;
	CHECK_NEAR(qa.x*sign, b.x); CHECK_NEAR(qa.y*sign, b.y);
	CHECK_NEAR(qa.z*sign, b.z); CHECK_NEAR(qa.w*sign, b.w);
}

int main()
{
	const float angles[] = { 0.0f, 0.5236f, 1.5708f, 2.1538f, -0.7f, 3.1f, -2.9f };
	const int numAngles = sizeof(angles) / sizeof(angles[0]);

	// --- Vector3 basics against base functions ---
	{
		const Vector3 a(3, -4, 12), b(2, 0.5f, -1);
		CHECK_NEAR(a.Length(), 13.0f);
		CHECK_NEAR(a.LengthSquared(), 169.0f);
		CHECK_NEAR(a.Dot(b), FrankVec3Dot(a, b));
		FrankVec3Base fo;
		FrankVec3Cross(fo, a, b);		CmpV3(a.Cross(b), fo);
		FrankVec3Normalize(fo, a);		CmpV3(a.Normalize(), fo);
		CmpV3(a + b, FrankVec3Base(5, -3.5f, 11));
		CmpV3(a - b, FrankVec3Base(1, -4.5f, 13));
		CmpV3(a * 2.0f, FrankVec3Base(6, -8, 24));
		CmpV3(2.0f * a, FrankVec3Base(6, -8, 24));
		CmpV3(a / 2.0f, FrankVec3Base(1.5f, -2, 6));
		CmpV3(-a, FrankVec3Base(-3, 4, -12));
		Vector3 acc(1, 1, 1); acc += b; CmpV3(acc, FrankVec3Base(3, 1.5f, 0));
		acc *= 2.0f; CmpV3(acc, FrankVec3Base(6, 3, 0));
	}

	// --- Matrix44 builders against base functions ---
	for (int i = 0; i < numAngles; ++i)
	{
		FrankMat44Base f;
		FrankMatrixRotationZ(f, angles[i]);
		CmpM(Matrix44::BuildRotateZ(angles[i]), f);
		FrankMatrixRotationYawPitchRoll(f, angles[i], angles[(i+1) % numAngles], angles[(i+2) % numAngles]);
		CmpM(Matrix44::BuildRotate(angles[i], angles[(i+1) % numAngles], angles[(i+2) % numAngles]), f);
	}
	{
		FrankMat44Base f;
		FrankMatrixTranslation(f, 10, -20, 30);	CmpM(Matrix44::BuildTranslate(10, -20, 30), f);
		FrankMatrixScaling(f, 2, -3, 0.5f);		CmpM(Matrix44::BuildScale(2, -3, 0.5f), f);
		FrankMatrixIdentity(f);					CmpM(Matrix44::Identity(), f);

		Vector3 axis(1, 2, -0.5f);
		axis.NormalizeThis();
		FrankMatrixRotationAxis(f, axis, 1.23f);
		CmpM(Matrix44::BuildRotate(axis, 1.23f), f);

		FrankMatrixLookAtLH(f, FrankVec3Base(5, 3, -2), FrankVec3Base(0, 1, 2), FrankVec3Base(0, 1, 0));
		CmpM(Matrix44::BuildLookAtLH(Vector3(5, 3, -2), Vector3(0, 1, 2), Vector3(0, 1, 0)), f);
	}

	// --- composed TRS: multiply, inverse, transpose, transforms ---
	{
		const Matrix44 trs = Matrix44::BuildScale(2, 3, 4) * Matrix44::BuildRotate(0.7f, -0.3f, 1.9f) * Matrix44::BuildTranslate(10, -20, 30);
		FrankMat44Base fs, fr, ft, ftrs;
		FrankMatrixScaling(fs, 2, 3, 4);
		FrankMatrixRotationYawPitchRoll(fr, 0.7f, -0.3f, 1.9f);
		FrankMatrixTranslation(ft, 10, -20, 30);
		ftrs = fs * fr * ft;
		CmpM(trs, ftrs);

		FrankMat44Base finv;
		FrankMatrixInverse(finv, ftrs);
		CmpM(trs.Inverse(), finv);

		FrankMat44Base ftr;
		FrankMatrixTranspose(ftr, ftrs);
		CmpM(trs.Transpose(), ftr);

		const Vector3 v(1, 2, 3);
		FrankVec3Base fo;
		FrankVec3TransformCoord(fo, v, ftrs);	CmpV3(trs.TransformCoord(v), fo);
		FrankVec3TransformNormal(fo, v, ftrs);	CmpV3(trs.TransformNormal(v), fo);

		// accessors
		CmpV3(trs.GetPos(), FrankVec3Base(ftrs._41, ftrs._42, ftrs._43));
		CmpV3(trs.GetRight(), FrankVec3Base(ftrs._11, ftrs._12, ftrs._13));
	}

	// --- Quaternion against base functions ---
	{
		Vector3 axis(1, 2, -0.5f);
		axis.NormalizeThis();
		FrankQuatBase fq;
		FrankQuaternionRotationAxis(fq, axis, 1.23f);
		const Quaternion q(axis, 1.23f);
		CmpQ(q, fq);

		FrankQuatBase fypr;
		FrankQuaternionRotationYawPitchRoll(fypr, 0.7f, -0.3f, 1.9f);
		const Quaternion qypr((Vector3(0.7f, -0.3f, 1.9f)));
		CmpQ(qypr, fypr);

		// matrix <-> quat round trips through the public API
		const Matrix44 rotM(qypr);
		FrankMat44Base frotM;
		FrankMatrixRotationQuaternion(frotM, qypr.GetQuaternionBase());
		CmpM(rotM, frotM);
		const Quaternion qBack(rotM);
		CmpQ(qBack, qypr.GetQuaternionBase());

		FrankQuatBase fo;
		FrankQuaternionInverse(fo, q.GetQuaternionBase());
		CmpQ(q.Inverse(), fo);
		FrankQuaternionNormalize(fo, FrankQuatBase(1, 2, 3, 4));
		CmpQ(Quaternion(1, 2, 3, 4).Normalize(), fo);
		CHECK_NEAR(q.DotProduct(qypr), FrankQuaternionDot(q.GetQuaternionBase(), qypr.GetQuaternionBase()));

		const float ts[] = { 0.0f, 0.37f, 1.0f };
		for (int i = 0; i < 3; ++i)
		{
			FrankQuaternionSlerp(fo, q.GetQuaternionBase(), qypr.GetQuaternionBase(), ts[i]);
			CmpQ(q.Slerp(qypr, ts[i]), fo);
		}

		Vector3 axisOut;
		float angleOut;
		q.GetAxisAngle(axisOut, angleOut);
		FrankVec3Base fao; float fang;
		FrankQuaternionToAxisAngle(q.GetQuaternionBase(), fao, fang);
		CmpV3(axisOut, fao);
		CHECK_NEAR(angleOut, fang);

		// multiply through public API vs base
		const FrankQuatBase fmul = q.GetQuaternionBase() * qypr.GetQuaternionBase();
		CmpQ(q * qypr, fmul);
	}

	// --- Color ---
	{
		CHECK_TRUE((DWORD)Color(1, 1, 1, 1) == 0xFFFFFFFFul);
		CHECK_TRUE((DWORD)Color(1.5f, -0.5f, 0.5f, 1) == 0xFFFF0080ul);
		CHECK_TRUE((DWORD)Color(0.5f, 0.25f, 0.75f, 0.1f) == 0x1A8040BFul);	// golden from config A
		const Color c(0.25f, 0.5f, 0.75f, 1.0f);
		const Color c2 = c.ScaleColor(2.0f);
		CHECK_NEAR(c2.r, 0.5f); CHECK_NEAR(c2.g, 1.0f); CHECK_NEAR(c2.a, 1.0f);
		const Color cInv = c.GetInverse();
		CHECK_NEAR(cInv.r, 0.75f); CHECK_NEAR(cInv.b, 0.25f);
	}

	// --- Vector2/XForm2 smoke test (already portable, must still compile & behave) ---
	{
		const Vector2 v(3, 4);
		CHECK_NEAR(v.Length(), 5.0f);
		const XForm2 xf(Vector2(10, 20), TEST_PI/2);
		const Vector2 t = xf.TransformCoord(Vector2(1, 0));
		CHECK_NEAR(t.x, 10.0f); CHECK_NEAR(t.y, 21.0f);
	}

	if (failCount == 0)
		printf("ALL PASS (%d checks)\n", checkCount);
	else
		printf("%d/%d CHECKS FAILED\n", failCount, checkCount);
	return failCount == 0 ? 0 : 1;
}

#endif
