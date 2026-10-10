/***
*
*	Copyright (c) 1999, Valve LLC. All rights reserved.
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
//
// cl_util.h
//

#pragma once
#include "cvardef.h"

#include "Platform.h"

// Macros to hook function calls into the HUD object
#define HOOK_MESSAGE(x) gEngfuncs.pfnHookUserMsg(#x, __MsgFunc_##x );

#define DECLARE_MESSAGE(y, x) int __MsgFunc_##x(const char *pszName, int iSize, void *pbuf) \
							{ \
							return gHUD.y.MsgFunc_##x(pszName, iSize, pbuf ); \
							}


#define HOOK_COMMAND(x, y) gEngfuncs.pfnAddCommand( x, __CmdFunc_##y );
#define DECLARE_COMMAND(y, x) void __CmdFunc_##x() \
							{ \
								gHUD.y.UserCmd_##x( ); \
							}

inline float CVAR_GET_FLOAT( const char *x ) {	return gEngfuncs.pfnGetCvarFloat( (char*)x ); }
inline const char* CVAR_GET_STRING( const char *x ) {	return gEngfuncs.pfnGetCvarString( (char*)x ); }
inline struct cvar_s *CVAR_CREATE( const char *cv, const char *val, const int flags ) {	return gEngfuncs.pfnRegisterVariable( (char*)cv, (char*)val, flags ); }


extern void SPR_Init();

extern HSPRITE_GOLDSRC SPR_Load(const char* szPicName);
extern int SPR_Frames(HSPRITE_GOLDSRC hPic);
extern int SPR_Height(HSPRITE_GOLDSRC hPic, int frame);
extern int SPR_Width(HSPRITE_GOLDSRC hPic, int frame);
extern void SPR_Set(HSPRITE_GOLDSRC hPic, int r, int g, int b);
extern void SPR_Draw(int frame, int x, int y, const Rect* prc);
extern void SPR_DrawHoles(int frame, int x, int y, const Rect* prc);
extern void SPR_DrawAdditive(int frame, int x, int y, const Rect* prc);
extern void SPR_EnableScissor(int x, int y, int width, int height);
extern void SPR_DisableScissor(void);

extern model_t* GetSpritePointer(HSPRITE_GOLDSRC hSprite);

extern void SetCrosshair(HSPRITE_GOLDSRC hspr, Rect rc, RGBA color);
extern void SetCrosshairColor(RGBA color);
extern void DrawCrosshair();


extern void FillRGBA(float x, float y, float w, float h, int r, int g, int b, int a);

#define SPR_GetList (*gEngfuncs.pfnSPR_GetList)


// ScreenHeight returns the height of the screen, in pixels
#define ScreenHeight (gHUD.m_scrinfo.iHeight)
// ScreenWidth returns the width of the screen, in pixels
#define ScreenWidth (gHUD.m_scrinfo.iWidth)

#define BASE_XRES 640.f

// use this to project world coordinates to screen coordinates
#define XPROJECT(x)	( (1.0f+(x))*ScreenWidth*0.5f )
#define YPROJECT(y) ( (1.0f-(y))*ScreenHeight*0.5f )

#define XRES(x)					(x  * ((float)ScreenWidth / 640))
#define YRES(y)					(y  * ((float)ScreenHeight / 480))

#define GetScreenInfo (*gEngfuncs.pfnGetScreenInfo)
#define ServerCmd (*gEngfuncs.pfnServerCmd)
#define EngineClientCmd (*gEngfuncs.pfnClientCmd)


// All monsters need this data (from server util.h)
#define		DONT_BLEED			-1
#define		BLOOD_COLOR_RED		(BYTE)247
#define		BLOOD_COLOR_YELLOW	(BYTE)195
#define		BLOOD_COLOR_GREEN	BLOOD_COLOR_YELLOW


inline int DrawConsoleString( int x, int y, const char *string )
{
	return gEngfuncs.pfnDrawConsoleString( x, y, (char*) string );
}

inline void GetConsoleStringSize( const char *string, int *width, int *height )
{
	gEngfuncs.pfnDrawConsoleStringLen( string, width, height );
}

inline int ConsoleStringLen( const char *string )
{
	int _width, _height;
	GetConsoleStringSize( string, &_width, &_height );
	return _width;
}

inline void ConsolePrint( const char *string )
{
	gEngfuncs.pfnConsolePrint( string );
}

inline void CenterPrint( const char *string )
{
	gEngfuncs.pfnCenterPrint( string );
}


inline char *safe_strcpy( char *dst, const char *src, int len_dst)
{
	if( len_dst <= 0 )
	{
		return nullptr; // this is bad
	}

	strncpy(dst,src,len_dst);
	dst[ len_dst - 1 ] = '\0';

	return dst;
}

inline int safe_sprintf( char *dst, int len_dst, const char *format, ...)
{
	if( len_dst <= 0 )
	{
		return -1; // this is bad
	}

	va_list v;

    va_start(v, format);

	_vsnprintf(dst,len_dst,format,v);

	va_end(v);

	dst[ len_dst - 1 ] = '\0';

	return 0;
}

// sound functions
inline void PlaySound( const char *szSound, float vol ) { gEngfuncs.pfnPlaySoundByName( szSound, vol ); }
inline void PlaySound( int iSound, float vol ) { gEngfuncs.pfnPlaySoundByIndex( iSound, vol ); }

void ScaleColors( int &r, int &g, int &b, int a );

float Length(const float *v);
void VectorMA (const float *veca, float scale, const float *vecb, float *vecc);
void VectorScale (const float *in, float scale, float *out);
float VectorNormalize (float *v);
void VectorInverse ( float *v );

// disable 'possible loss of data converting float to int' warning message
#pragma warning( disable: 4244 )
// disable 'truncation from 'const double' to 'float' warning message
#pragma warning( disable: 4305 )

HSPRITE_GOLDSRC LoadSprite(const char *pszName);

void GetFallbackDir(char* falldir);
char* strUpper(char* str);