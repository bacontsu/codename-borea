/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
// mathlib.h

#pragma once

#include <cmath>
#include <cassert>

typedef float vec_t;

#include "vector.h"

typedef vec_t vec4_t[4]; // x,y,z,w
typedef vec_t vec5_t[5];

typedef short vec_s_t;
typedef vec_s_t vec3s_t[3];
typedef vec_s_t vec4s_t[4]; // x,y,z,w
typedef vec_s_t vec5s_t[5];

typedef int fixed4_t;
typedef int fixed8_t;
typedef int fixed16_t;
#ifndef M_PI
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h
#endif

#ifndef M_PI2
#define M_PI2 6.28318530717958647692
#endif

#ifndef RAD2DEG
#define RAD2DEG(x) ((float)(x) * (float)(180.f / M_PI))
#endif

#ifndef DEG2RAD
#define DEG2RAD(x) ((float)(x) * (float)(M_PI / 180.f))
#endif

#define bound(min, num, max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#define V_min(a, b) (((a) < (b)) ? (a) : (b))
#define V_max(a, b) (((a) > (b)) ? (a) : (b))


struct matrix3x4_t
{
	inline float* operator[](int i)
	{
		assert((i >= 0) && (i < 3));
		return m_flMatVal[i];
	}
	inline const float* operator[](int i) const
	{
		assert((i >= 0) && (i < 3));
		return m_flMatVal[i];
	}
	inline float* Base() { return &m_flMatVal[0][0]; }
	inline const float* Base() const { return &m_flMatVal[0][0]; }

	float m_flMatVal[3][4];
};

struct mplane_s;

#if !defined(GAMEUI_UNMANGLER)

constexpr Vector vec3_origin(0, 0, 0);
constexpr Vector g_vecZero(0, 0, 0);

#endif
extern int nanmask;

#define IS_NAN(x) (((*(int*)&x) & nanmask) == nanmask)

#define VectorSubtract(a, b, c)   \
	{                             \
		(c)[0] = (a)[0] - (b)[0]; \
		(c)[1] = (a)[1] - (b)[1]; \
		(c)[2] = (a)[2] - (b)[2]; \
	}
#define VectorAdd(a, b, c)        \
	{                             \
		(c)[0] = (a)[0] + (b)[0]; \
		(c)[1] = (a)[1] + (b)[1]; \
		(c)[2] = (a)[2] + (b)[2]; \
	}
#define VectorCopy(a, b) \
	{                    \
		(b)[0] = (a)[0]; \
		(b)[1] = (a)[1]; \
		(b)[2] = (a)[2]; \
	}

#if !defined(GAMEUI_UNMANGLER)
inline void VectorMin(const Vector& a, const Vector& b, Vector& result)
{
	result.x = V_min(a.x, b.x);
	result.y = V_min(a.y, b.y);
	result.z = V_min(a.z, b.z);
}

inline void VectorMax(const Vector& a, const Vector& b, Vector& result)
{
	result.x = V_max(a.x, b.x);
	result.y = V_max(a.y, b.y);
	result.z = V_max(a.z, b.z);
}
#endif

inline void VectorClear(float* a)
{
	a[0] = 0.0;
	a[1] = 0.0;
	a[2] = 0.0;
}

void VectorMA(const float* veca, float scale, const float* vecb, float* vecc);

bool VectorCompare(const float* v1, const float* v2);
float Length(const float* v);
void CrossProduct(const float* v1, const float* v2, float* cross);
float VectorNormalize(float* v); // returns vector length
void VectorInverse(float* v);
void VectorScale(const float* in, float scale, float* out);
int Q_log2(int val);

void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3]);
void R_ConcatTransforms(const matrix3x4_t in1, const matrix3x4_t in2, matrix3x4_t& out);

void FloorDivMod(double numer, double denom, int* quotient,
	int* rem);
fixed16_t Invert24To16(fixed16_t val);
int GreatestCommonDivisor(int i1, int i2);

void AngleVectors(const Vector& angles, Vector* forward, Vector* right, Vector* up);
void AngleVectorsTranspose(const Vector& angles, Vector* forward, Vector* right, Vector* up);
#define AngleIVectors AngleVectorsTranspose

void AngleMatrix(const float* angles, matrix3x4_t& matrix);
void AngleIMatrix(const Vector& angles, matrix3x4_t& matrix);

void MatrixAngles(const matrix3x4_t matrix, Vector& angles, Vector& position);
void MatrixAngles(const matrix3x4_t matrix, float* angles);

void VectorTransform(const float* in1, const matrix3x4_t& in2, float* out);

void NormalizeAngles(float* angles);
void InterpolateAngles(float* start, float* end, float* output, float frac);
float AngleBetweenVectors(const float* v1, const float* v2);


void VectorMatrix(const Vector& forward, Vector& right, Vector& up);
void VectorAngles(const float* forward, float* angles);

int InvertMatrix(const float* m, float* out);

float anglemod(float a);

float lerp(float start, float end, float frac);



#define BOX_ON_PLANE_SIDE(emins, emaxs, p)                                                                 \
	(((p)->type < 3) ? (                                                                                   \
						   ((p)->dist <= (emins)[(p)->type]) ? 1                                           \
															 : (                                           \
																   ((p)->dist >= (emaxs)[(p)->type]) ? 2   \
																									 : 3)) \
					 : BoxOnPlaneSide((emins), (emaxs), (p)))
