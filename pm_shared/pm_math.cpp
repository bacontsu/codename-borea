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
// pm_math.c -- math primitives

#include "Platform.h"
#include "mathlib.h"
#include "const.h"

// up / down
#define	PITCH	0
// left / right
#define	YAW		1
// fall over
#define	ROLL	2 

#pragma warning(disable : 4244)

int nanmask = 255<<23;

float	anglemod(float a)
{
	a = (360.0/65536) * ((int)(a*(65536/360.0)) & 65535);
	return a;
}

void AngleVectors (const Vector& angles, Vector* forward, Vector* right, Vector* up)
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	if (forward)
	{
		forward->x = cp*cy;
		forward->y = cp*sy;
		forward->z = -sp;
	}
	if (right)
	{
		right->x = (-1 * sr * sp * cy + -1 * cr * -sy);
		right->y = (-1 * sr * sp * sy + -1 * cr * cy);
		right->z = -1*sr*cp;
	}
	if (up)
	{
		up->x = (cr * sp * cy + -sr * -sy);
		up->y = (cr * sp * sy + -sr * cy);
		up->z = cr*cp;
	}
}

void AngleVectorsTranspose (const Vector& angles, Vector* forward, Vector* right, Vector* up)
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	if (forward)
	{
		forward->x = cp * cy;
		forward->y = (sr * sp * cy + cr * -sy);
		forward->z = (cr*sp*cy+-sr*-sy);
	}
	if (right)
	{
		right->x = cp * sy;
		right->y = (sr * sp * sy + cr * cy);
		right->z = (cr*sp*sy+-sr*cy);
	}
	if (up)
	{
		up->x = -sp;
		up->y = sr * cp;
		up->z = cr*cp;
	}
}

#ifndef DISABLE_VEC_FUNCS
void AngleMatrix (const float* angles, float (*matrix)[4] )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[1][0] = cp*sy;
	matrix[2][0] = -sp;
	matrix[0][1] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[2][1] = sr*cp;
	matrix[0][2] = (cr*sp*cy+-sr*-sy);
	matrix[1][2] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}

void AngleIMatrix (const Vector& angles, float matrix[3][4] )
{
	float		angle;
	float		sr, sp, sy, cr, cp, cy;
	
	angle = angles[YAW] * (M_PI*2 / 360);
	sy = sin(angle);
	cy = cos(angle);
	angle = angles[PITCH] * (M_PI*2 / 360);
	sp = sin(angle);
	cp = cos(angle);
	angle = angles[ROLL] * (M_PI*2 / 360);
	sr = sin(angle);
	cr = cos(angle);

	// matrix = (YAW * PITCH) * ROLL
	matrix[0][0] = cp*cy;
	matrix[0][1] = cp*sy;
	matrix[0][2] = -sp;
	matrix[1][0] = sr*sp*cy+cr*-sy;
	matrix[1][1] = sr*sp*sy+cr*cy;
	matrix[1][2] = sr*cp;
	matrix[2][0] = (cr*sp*cy+-sr*-sy);
	matrix[2][1] = (cr*sp*sy+-sr*cy);
	matrix[2][2] = cr*cp;
	matrix[0][3] = 0.0;
	matrix[1][3] = 0.0;
	matrix[2][3] = 0.0;
}
#endif

void NormalizeAngles( float* angles )
{
	int i;
	// Normalize angles
	for ( i = 0; i < 3; i++ )
	{
		if ( angles[i] > 180.0 )
		{
			angles[i] -= 360.0;
		}
		else if ( angles[i] < -180.0 )
		{
			angles[i] += 360.0;
		}
	}
}

/*
===================
InterpolateAngles

Interpolate Euler angles.
FIXME:  Use Quaternions to avoid discontinuities
Frac is 0.0 to 1.0 ( i.e., should probably be clamped, but doesn't have to be )
===================
*/
void InterpolateAngles( float *start, float *end, float *output, float frac )
{
	int i;
	float ang1, ang2;
	float d;
	
	NormalizeAngles( start );
	NormalizeAngles( end );

	for ( i = 0 ; i < 3 ; i++ )
	{
		ang1 = start[i];
		ang2 = end[i];

		d = ang2 - ang1;
		if ( d > 180 )
		{
			d -= 360;
		}
		else if ( d < -180 )
		{	
			d += 360;
		}

		output[i] = ang1 + d * frac;
	}

	NormalizeAngles( output );
}
 

/*
===================
AngleBetweenVectors

===================
*/
float AngleBetweenVectors( const Vector& v1, const Vector& v2 )
{
	float angle;
	float l1 = Length( v1 );
	float l2 = Length( v2 );

	if ( !l1 || !l2 )
		return 0.0f;

	angle = acos( DotProduct( v1, v2 ) ) / (l1*l2);
	angle = ( angle  * 180.0f ) / M_PI;

	return angle;
}

#ifndef DISABLE_VEC_FUNCS
void VectorTransform (const float* in1, float in2[3][4], float* out)
{
	out[0] = DotProduct(*reinterpret_cast<const Vector*>(in1), *reinterpret_cast<const Vector*>(in2[0])) + in2[0][3];
	out[1] = DotProduct(*reinterpret_cast<const Vector*>(in1), *reinterpret_cast<const Vector*>(in2[1])) + in2[1][3];
	out[2] = DotProduct(*reinterpret_cast<const Vector*>(in1), *reinterpret_cast<const Vector*>(in2[2])) + in2[2][3];
}

int VectorCompare (const float* v1, const float* v2)
{
	int		i;
	
	for (i=0 ; i<3 ; i++)
		if (v1[i] != v2[i])
			return 0;
			
	return 1;
}

void VectorMA (const float* veca, float scale, const float* vecb, float* vecc)
{
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}
#endif

#ifndef DISABLE_VEC_FUNCS
void CrossProduct (const float* v1, const float* v2, float* cross)
{
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}
#endif

#ifndef DISABLE_VEC_FUNCS
float Length(const float* v)
{
	int		i;
	float	length = 0.0f;
		
	for (i=0 ; i< 3 ; i++)
		length += v[i]*v[i];
	length = sqrt (length);		// FIXME

	return length;
}
#endif

float Distance(const float* v1, const float* v2)
{
	Vector d;
	VectorSubtract(v2,v1,d);
	return Length(d);
}

#ifndef DISABLE_VEC_FUNCS
float VectorNormalize (float* v)
{
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);		// FIXME

	if (length)
	{
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;

}

void VectorInverse (float* v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

void VectorScale (const float* in, float scale, float* out)
{
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}
#endif


int Q_log2(int val)
{
	int answer=0;
	while (val>>=1)
		answer++;
	return answer;
}

void VectorMatrix( const Vector& forward, Vector& right, Vector& up)
{
	Vector tmp;

	if (forward[0] == 0 && forward[1] == 0)
	{
		right[0] = 1;	
		right[1] = 0; 
		right[2] = 0;
		up[0] = -forward[2]; 
		up[1] = 0; 
		up[2] = 0;
		return;
	}

	tmp[0] = 0; tmp[1] = 0; tmp[2] = 1.0;
	CrossProduct( forward, tmp, right );
	VectorNormalize( right );
	CrossProduct( right, forward, up );
	VectorNormalize( up );
}


#ifndef DISABLE_VEC_FUNCS
void VectorAngles( const float* forward, float* angles )
{
	double	tmp, yaw, pitch;
	
	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt (forward[0]*forward[0] + forward[1]*forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}
	
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}
#endif

/*
================
ConcatTransforms

================
*/
void ConcatTransforms( float in1[ 3 ][ 4 ], float in2[ 3 ][ 4 ], float out[ 3 ][ 4 ] )
{
	out[ 0 ][ 0 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 0 ] +
		in1[ 0 ][ 2 ] * in2[ 2 ][ 0 ];
	out[ 0 ][ 1 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 1 ] +
		in1[ 0 ][ 2 ] * in2[ 2 ][ 1 ];
	out[ 0 ][ 2 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 2 ] +
		in1[ 0 ][ 2 ] * in2[ 2 ][ 2 ];
	out[ 0 ][ 3 ] = in1[ 0 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 0 ][ 1 ] * in2[ 1 ][ 3 ] +
		in1[ 0 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 0 ][ 3 ];
	out[ 1 ][ 0 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 0 ] +
		in1[ 1 ][ 2 ] * in2[ 2 ][ 0 ];
	out[ 1 ][ 1 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 1 ] +
		in1[ 1 ][ 2 ] * in2[ 2 ][ 1 ];
	out[ 1 ][ 2 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 2 ] +
		in1[ 1 ][ 2 ] * in2[ 2 ][ 2 ];
	out[ 1 ][ 3 ] = in1[ 1 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 1 ][ 1 ] * in2[ 1 ][ 3 ] +
		in1[ 1 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 1 ][ 3 ];
	out[ 2 ][ 0 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 0 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 0 ] +
		in1[ 2 ][ 2 ] * in2[ 2 ][ 0 ];
	out[ 2 ][ 1 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 1 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 1 ] +
		in1[ 2 ][ 2 ] * in2[ 2 ][ 1 ];
	out[ 2 ][ 2 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 2 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 2 ] +
		in1[ 2 ][ 2 ] * in2[ 2 ][ 2 ];
	out[ 2 ][ 3 ] = in1[ 2 ][ 0 ] * in2[ 0 ][ 3 ] + in1[ 2 ][ 1 ] * in2[ 1 ][ 3 ] +
		in1[ 2 ][ 2 ] * in2[ 2 ][ 3 ] + in1[ 2 ][ 3 ];
}

float lerp(float start, float end, float frac)
{
	// Exact, monotonic, bounded, determinate, and (for a=b=0) consistent:
	if (start <= 0 && end >= 0 || start >= 0 && end <= 0)
		return frac * end + (1 - frac) * start;

	if (frac == 1)
		return end; // exact
	// Exact at t=0, monotonic except near t=1,
	// bounded, determinate, and consistent:
	const float x = start + frac * (end - start);
	return frac > 1 == end > start ? V_max(end, x) : V_min(end, x); // monotonic near t=1
}

//-----------------------------------------------------------------------------
// Purpose: Generates Euler angles given a left-handed orientation matrix. The
//			columns of the matrix contain the forward, left, and up vectors.
// Input  : matrix - Left-handed orientation matrix.
//			angles[PITCH, YAW, ROLL]. Receives right-handed counterclockwise
//				rotations in degrees around Y, Z, and X respectively.
//-----------------------------------------------------------------------------

void MatrixAngles(const float matrix[3][4], Vector& angles, Vector& position)
{
	MatrixGetColumn(matrix, 3, position);
	MatrixAngles(matrix, angles);
}


void MatrixAngles(const float matrix[3][4], float* angles)
{
	float forward[3];
	float left[3];
	float up[3];

	//
	// Extract the basis vectors from the matrix. Since we only need the Z
	// component of the up vector, we don't get X and Y.
	//
	forward[0] = matrix[0][0];
	forward[1] = matrix[1][0];
	forward[2] = matrix[2][0];
	left[0] = matrix[0][1];
	left[1] = matrix[1][1];
	left[2] = matrix[2][1];
	up[2] = matrix[2][2];

	float xyDist = sqrtf(forward[0] * forward[0] + forward[1] * forward[1]);

	// enough here to get angles?
	if (xyDist > 0.001f)
	{
		// (yaw)	y = ATAN( forward.y, forward.x );		-- in our space, forward is the X axis
		angles[1] = RAD2DEG(atan2f(forward[1], forward[0]));

		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG(atan2f(-forward[2], xyDist));

		// (roll)	z = ATAN( left.z, up.z );
		angles[2] = RAD2DEG(atan2f(left[2], up[2]));
	}
	else // forward is mostly Z, gimbal lock-
	{
		// (yaw)	y = ATAN( -left.x, left.y );			-- forward is mostly z, so use right for yaw
		angles[1] = RAD2DEG(atan2f(-left[0], left[1]));

		// (pitch)	x = ATAN( -forward.z, sqrt(forward.x*forward.x+forward.y*forward.y) );
		angles[0] = RAD2DEG(atan2f(-forward[2], xyDist));

		// Assume no roll in this case as one degree of freedom has been lost (i.e. yaw == roll)
		angles[2] = 0;
	}
}

void MatrixGetColumn(const float in[3][4], int column, Vector& out)
{
	out.x = in[0][column];
	out.y = in[1][column];
	out.z = in[2][column];
}

void MatrixSetColumn(const Vector& in, int column, float out[3][4])
{
	out[0][column] = in.x;
	out[1][column] = in.y;
	out[2][column] = in.z;
}