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
//
// battery.cpp
//
// implementation of CHudBattery class
//

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>

DECLARE_MESSAGE(m_Battery, Battery)

int CHudBattery::Init()
{
	m_iBat = 0;
	m_fFade = 0;
	m_iFlags = 0;

	HOOK_MESSAGE(Battery);

	gHUD.AddHudElem(this);

	return 1;
};


int CHudBattery::VidInit()
{
	int HUD_suit_empty = gHUD.GetSpriteIndex( "suit_empty" );
	int HUD_suit_full = gHUD.GetSpriteIndex( "suit_full" );

	m_hSprite1 = m_hSprite2 = 0;  // delaying get sprite handles until we know the sprites are loaded
	m_prc1 = &gHUD.GetSpriteRect( HUD_suit_empty );
	m_prc2 = &gHUD.GetSpriteRect( HUD_suit_full );
	m_iHeight = m_prc2->bottom - m_prc1->top;
	m_fFade = 0;
	return 1;
};

int CHudBattery:: MsgFunc_Battery(const char *pszName,  int iSize, void *pbuf )
{
	m_iFlags |= HUD_ACTIVE;
	
	BEGIN_READ( pbuf, iSize );
	int x = READ_SHORT();

#if defined( _TFC )
	int y = READ_SHORT();

	if ( x != m_iBat || y != m_iBatMax )
	{
		m_fFade = FADE_TIME;
		m_iBat = x;
		m_iBatMax = y;
	}
#else
	if ( x != m_iBat )
	{
		m_fFade = FADE_TIME;
		m_iBat = x;
	}
#endif

	return 1;
}


int CHudBattery::Draw(float flTime)
{
	if ( gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH )
		return 1;

	/*
	int x, y, scale;

	x = 100 + +gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3;
	y = ScreenHeight + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 - 70;
	scale = m_iBat * 1.3f;

	FillRGBA(x, y, scale, 20, 251, 177, 43, 255);
	*/

	return 1;
}