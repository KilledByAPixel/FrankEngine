////////////////////////////////////////////////////////////////////////////////////////

/* coherent noise function over 1, 2 or 3 dimensions */
/* (copyright Ken Perlin) */

////////////////////////////////////////////////////////////////////////////////////////

#include "frankEngine.h"
#include "frankMath.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

namespace PerlineNoise
{
	#define B 0x100
	#define BM 0xff

	#define N 0x1000
	#define NP 12   /* 2^N */
	#define NM 0xfff

	static int p[B + B + 2];
	static float g3[B + B + 2][3];
	static float g2[B + B + 2][2];
	static float g1[B + B + 2];
	static int start = 1;

	static void init(void);

	#define s_curve(t) ( t * t * (3. - 2. * t) )

	#define lerp(t, a, b) ( a + t * (b - a) )

	#define setup(i,b0,b1,r0,r1)\
		t = vec[i] + N;\
		b0 = ((int)t) & BM;\
		b1 = (b0+1) & BM;\
		r0 = t - (int)t;\
		r1 = r0 - 1.f;

	float noise1(float arg)
	{
		int bx0, bx1;
		float rx0, rx1, sx, t, u, v, vec[1];

		vec[0] = arg;
		if (start) {
			start = 0;
			init();
		}

		setup(0, bx0,bx1, rx0,rx1);

		sx = (float)s_curve(rx0);

		u = rx0 * g1[ p[ bx0 ] ];
		v = rx1 * g1[ p[ bx1 ] ];

		return lerp(sx, u, v);
	}

	float noise2(float vec[2])
	{
		int bx0, bx1, by0, by1, b00, b10, b01, b11;
		float rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
		register int i, j;

		if (start) {
			start = 0;
			init();
		}

		setup(0, bx0,bx1, rx0,rx1);
		setup(1, by0,by1, ry0,ry1);

		i = p[ bx0 ];
		j = p[ bx1 ];

		b00 = p[ i + by0 ];
		b10 = p[ j + by0 ];
		b01 = p[ i + by1 ];
		b11 = p[ j + by1 ];

		sx = (float)s_curve(rx0);
		sy = (float)s_curve(ry0);

	#define at2(rx,ry) ( rx * q[0] + ry * q[1] )

		q = g2[ b00 ] ; u = at2(rx0,ry0);
		q = g2[ b10 ] ; v = at2(rx1,ry0);
		a = lerp(sx, u, v);

		q = g2[ b01 ] ; u = at2(rx0,ry1);
		q = g2[ b11 ] ; v = at2(rx1,ry1);
		b = lerp(sx, u, v);

		return lerp(sy, a, b);
	}

	float noise3(float vec[3])
	{
		int bx0, bx1, by0, by1, bz0, bz1, b00, b10, b01, b11;
		float rx0, rx1, ry0, ry1, rz0, rz1, *q, sy, sz, a, b, c, d, t, u, v;
		register int i, j;

		if (start) {
			start = 0;
			init();
		}

		setup(0, bx0,bx1, rx0,rx1);
		setup(1, by0,by1, ry0,ry1);
		setup(2, bz0,bz1, rz0,rz1);

		i = p[ bx0 ];
		j = p[ bx1 ];

		b00 = p[ i + by0 ];
		b10 = p[ j + by0 ];
		b01 = p[ i + by1 ];
		b11 = p[ j + by1 ];

		t  = (float)s_curve(rx0);
		sy = (float)s_curve(ry0);
		sz = (float)s_curve(rz0);

	#define at3(rx,ry,rz) ( rx * q[0] + ry * q[1] + rz * q[2] )

		q = g3[ b00 + bz0 ] ; u = at3(rx0,ry0,rz0);
		q = g3[ b10 + bz0 ] ; v = at3(rx1,ry0,rz0);
		a = lerp(t, u, v);

		q = g3[ b01 + bz0 ] ; u = at3(rx0,ry1,rz0);
		q = g3[ b11 + bz0 ] ; v = at3(rx1,ry1,rz0);
		b = lerp(t, u, v);

		c = lerp(sy, a, b);

		q = g3[ b00 + bz1 ] ; u = at3(rx0,ry0,rz1);
		q = g3[ b10 + bz1 ] ; v = at3(rx1,ry0,rz1);
		a = lerp(t, u, v);

		q = g3[ b01 + bz1 ] ; u = at3(rx0,ry1,rz1);
		q = g3[ b11 + bz1 ] ; v = at3(rx1,ry1,rz1);
		b = lerp(t, u, v);

		d = lerp(sy, a, b);

		return lerp(sz, c, d);
	}

	static inline void normalize2(float v[2])
	{
		float s = sqrt(v[0] * v[0] + v[1] * v[1]);
		s = 1 / s;
		v[0] *= s;
		v[1] *= s;
	}

	static inline void normalize3(float v[3])
	{
		float s = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
		s = 1 / s;
		v[0] *= s;
		v[1] *= s;
		v[2] *= s;
	}

	static void init(void)
	{
		int i, j;

		for (i = 0 ; i < B ; i++) {
			p[i] = i;

			g1[i] = RAND_SIGN*RAND_PERCENT;

			for (j = 0 ; j < 2 ; j++)
				g2[i][j] = RAND_SIGN*RAND_PERCENT;
			normalize2(g2[i]);

			for (j = 0 ; j < 3 ; j++)
				g3[i][j] = RAND_SIGN*RAND_PERCENT;
			normalize3(g3[i]);
		}

		while (--i) {
			int k = p[i];
			p[i] = p[j = RAND_INT % B];
			p[j] = k;
		}

		for (i = 0 ; i < B + 2 ; i++) {
			p[B + i] = p[i];
			g1[B + i] = g1[i];
			for (j = 0 ; j < 2 ; j++)
				g2[B + i][j] = g2[i][j];
			for (j = 0 ; j < 3 ; j++)
				g3[B + i][j] = g3[i][j];
		}
	}
}