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
// Health.cpp
//
// implementation of CHudHealth class
//

#include "stdio.h"
#include "stdlib.h"

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

// sprites
#define BACKGROUND_SPRITE "sprites/healthback.spr"
#define HEALTH_SPRITE "sprites/healthlogo.spr"
#define LOW_HEALTH_SPRITE "sprites/lowhealth.spr"
#define LOW_ARMOR_SPRITE "sprites/lowarmor.spr"

// sprites color
#define BACKGROUND_COLOR Vector( 251, 177, 43)
#define HEALTH_COLOR Vector( 251, 177, 43)
#define INFO_COLOR Vector( 15, 251, 177)


DECLARE_MESSAGE(m_Health, Health )
DECLARE_MESSAGE(m_Health, Damage )
DECLARE_MESSAGE(m_Health, Stamina)

#define PAIN_NAME "sprites/%d_pain.spr"
#define DAMAGE_NAME "sprites/%d_dmg.spr"

int giDmgHeight, giDmgWidth;

int giDmgFlags[NUM_DMG_TYPES] = 
{
	DMG_POISON,
	DMG_ACID,
	DMG_FREEZE|DMG_SLOWFREEZE,
	DMG_DROWN,
	DMG_BURN|DMG_SLOWBURN,
	DMG_NERVEGAS, 
	DMG_RADIATION,
	DMG_SHOCK,
	DMG_CALTROP,
	DMG_TRANQ,
	DMG_CONCUSS,
	DMG_HALLUC
};

int CHudHealth::Init()
{
	HOOK_MESSAGE(Health);
	HOOK_MESSAGE(Damage);
	HOOK_MESSAGE(Stamina);
	m_iHealth = 100;
	m_fFade = 0;
	m_iFlags = 0;
	m_bitsDamage = 0;
	m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;
	giDmgHeight = 0;
	giDmgWidth = 0;

	memset(m_dmg, 0, sizeof(DAMAGE_IMAGE) * NUM_DMG_TYPES);


	gHUD.AddHudElem(this);
	return 1;
}

void CHudHealth::Reset()
{
	// make sure the pain compass is cleared when the player respawns
	m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;


	// force all the flashing damage icons to expire
	m_bitsDamage = 0;
	for ( int i = 0; i < NUM_DMG_TYPES; i++ )
	{
		m_dmg[i].fExpire = 0;
	}
}

int CHudHealth::VidInit()
{
	m_hSprite = 0;

	m_HUD_dmg_bio = gHUD.GetSpriteIndex( "dmg_bio" ) + 1;
	m_HUD_cross = gHUD.GetSpriteIndex( "cross" );

	giDmgHeight = gHUD.GetSpriteRect(m_HUD_dmg_bio).right - gHUD.GetSpriteRect(m_HUD_dmg_bio).left;
	giDmgWidth = gHUD.GetSpriteRect(m_HUD_dmg_bio).bottom - gHUD.GetSpriteRect(m_HUD_dmg_bio).top;

	// reset when level change
	nextBeatUpdate = 0;
	return 1;
}

int CHudHealth:: MsgFunc_Health(const char *pszName,  int iSize, void *pbuf )
{
	// TODO: update local health data
	BEGIN_READ( pbuf, iSize );
	int x = READ_SHORT();

	m_iFlags |= HUD_ACTIVE;

	// Only update the fade if we've changed health
	if (x != m_iHealth)
	{
		m_fFade = FADE_TIME;
		m_iHealth = x;
	}

	return 1;
}


int CHudHealth:: MsgFunc_Damage(const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	int armor = READ_BYTE();	// armor
	int damageTaken = READ_BYTE();	// health
	long bitsDamage = READ_LONG(); // damage bits

	Vector vecFrom;

	for ( int i = 0 ; i < 3 ; i++)
		vecFrom[i] = READ_COORD();

	UpdateTiles(gHUD.m_flTime, bitsDamage);

	// Actually took damage?
	if ( damageTaken > 0 || armor > 0 )
		CalcDamageDirection(vecFrom);

	return 1;
}

int CHudHealth::MsgFunc_Stamina(const char* pszName, int iSize, void* pbuf)
{
	// TODO: update local health data
	BEGIN_READ( pbuf, iSize );
	m_iStamina = READ_SHORT();
	gHUD.wallType = READ_SHORT();
	gHUD.isClimbing = READ_SHORT();
	gHUD.slowmoBar = READ_SHORT();
	gHUD.isSlowmo = READ_SHORT();
	gHUD.isRunning = (bool)READ_BYTE();
	
	return 1;
}


// Returns back a color from the
// Green <-> Yellow <-> Red ramp
void CHudHealth::GetPainColor( int &r, int &g, int &b )
{
	int iHealth = m_iHealth;

	if (iHealth > 25)
		iHealth -= 25;
	else if ( iHealth < 0 )
		iHealth = 0;
#if 0
	g = iHealth * 255 / 100;
	r = 255 - g;
	b = 0;
#else
	if (m_iHealth > 25)
	{
		r = giR;
		g = giG;
		b = giB;
	}
	else
	{
		r = 250;
		g = 0;
		b = 0;
	}
#endif 
}

void DrawFrame(float xmin, float ymin, float xmax, float ymax, char* sprite, Vector color, int mode, int frame)
{
	//setup
	gEngfuncs.pTriAPI->RenderMode(mode);
	gEngfuncs.pTriAPI->Brightness(1.0f);
	gEngfuncs.pTriAPI->Color4ub(color.x, color.y, color.z, 255);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(SPR_Load(sprite)), frame);

	//start drawing
	gEngfuncs.pTriAPI->Begin(TRI_QUADS);

	//top left
	gEngfuncs.pTriAPI->TexCoord2f(0, 0);
	gEngfuncs.pTriAPI->Vertex3f(xmin, ymin, 0);
	//bottom left
	gEngfuncs.pTriAPI->TexCoord2f(0, 1);
	gEngfuncs.pTriAPI->Vertex3f(xmin, ymax, 0);
	//bottom right
	gEngfuncs.pTriAPI->TexCoord2f(1, 1);
	gEngfuncs.pTriAPI->Vertex3f(xmax, ymax, 0);
	//top right
	gEngfuncs.pTriAPI->TexCoord2f(1, 0);
	gEngfuncs.pTriAPI->Vertex3f(xmax, ymin, 0);

	//end
	gEngfuncs.pTriAPI->End();
	gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
}

int CHudHealth::Draw(float flTime)
{
	int r, g, b;
	int a = 0, x, y;
	int HealthWidth;
	int scale;

	if ( (gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH) || gEngfuncs.IsSpectateOnly() )
		return 1;

	if ( !m_hSprite )
		m_hSprite = LoadSprite(PAIN_NAME);
	
	// Has health changed? Flash the health #
	if (m_fFade)
	{
		m_fFade -= (gHUD.m_flTimeDelta * 20);
		if (m_fFade <= 0)
		{
			a = MIN_ALPHA;
			m_fFade = 0;
		}

		// Fade the health number back to dim

		a = MIN_ALPHA +  (m_fFade/FADE_TIME) * 128;

	}
	else
		a = MIN_ALPHA;

	// If health is getting low, make it bright red
	if (m_iHealth <= 15)
		a = 255;
		
	GetPainColor( r, g, b );
	ScaleColors(r, g, b, a );

	// find heartbeat speed
	if (m_iHealth > 70)
	{
		animSpeed = 0.05f;
	}
	else if (m_iHealth <= 70 && m_iHealth > 15)
	{
		animSpeed = 0.08f;
	}
	else
	{
		animSpeed = 0.15f;
	}

	// beating heart logic
	if (nextBeatUpdate < gHUD.m_flTime)
	{
		beatSequence++;
		if (beatSequence < 5)
		{
			heartScaler = beatSequence;
		}
		else if (beatSequence >= 5 && beatSequence < 10)
		{
			heartScaler = 10 - beatSequence;
		}
		else if (beatSequence == 10)
		{
			beatSequence = 0;
		}
		nextBeatUpdate = gHUD.m_flTime + animSpeed;
	}

	// cardio line thing
	if (nextBeatFrame < gHUD.m_flTime)
	{
		beatFrame++;

		int maxFrame;
		if (m_iHealth > 80)
		{
			maxFrame = 24;
		}
		else if (m_iHealth > 60)
		{
			maxFrame = 25;
		}
		else if (m_iHealth > 40)
		{
			maxFrame = 23;
		}
		else if (m_iHealth > 20)
		{
			maxFrame = 23;
		}
		else if (m_iHealth > 0)
		{
			maxFrame = 25;
		}
		else
		{
			maxFrame = 12;
		}

		if (beatFrame >= maxFrame) beatFrame = 0;

		nextBeatFrame = gHUD.m_flTime + 0.05f;
	}


	// Only draw health if we have the suit.
	if (gHUD.m_iWeaponBits & (1<<(WEAPON_SUIT)))
	{
		HealthWidth = gHUD.GetSpriteRect(gHUD.m_HUD_number_0).right - gHUD.GetSpriteRect(gHUD.m_HUD_number_0).left;
		int CrossWidth = gHUD.GetSpriteRect(m_HUD_cross).right - gHUD.GetSpriteRect(m_HUD_cross).left;

		// draw background
		x = 50 + gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3 + gHUD.camValue[0] * 0.1f;
		y = ScreenHeight + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 - 90 - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2 + gHUD.camValue[1] * 0.1f;

		gHUD.DrawBackground(x, y, x + 350, y + 100, BACKGROUND_SPRITE, BACKGROUND_COLOR, kRenderTransTexture);

		// draw health
		x = 190 + gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3 + gHUD.camValue[0] * 0.1f;
		y = ScreenHeight + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 - 70 - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2 + gHUD.camValue[1] * 0.1f;

		gHUD.DrawHudNumber(x, y, DHN_DRAWZERO, m_iHealth, r, g, b);

		// draw cardio lines
		x = 85 + gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3 + gHUD.camValue[0] * 0.1f;
		y = ScreenHeight - 98 - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2 + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 + gHUD.camValue[1] * 0.1f;

		if (m_iHealth > 80)
		{
			DrawFrame(x, y, x + 120, y + 120, "sprites/a.spr", Vector(94, 235, 33), kRenderTransAdd, beatFrame);
		}
		else if (m_iHealth > 60)
		{
			DrawFrame(x, y, x + 120, y + 120, "sprites/b.spr", Vector(94, 235, 33), kRenderTransAdd, beatFrame);
		}
		else if (m_iHealth > 40)
		{
			DrawFrame(x, y, x + 120, y + 120, "sprites/c.spr", Vector(94, 235, 33), kRenderTransAdd, beatFrame);
		}
		else if (m_iHealth > 20)
		{
			DrawFrame(x, y, x + 120, y + 120, "sprites/d.spr", Vector(94, 235, 33), kRenderTransAdd, beatFrame);
		}
		else if (m_iHealth > 0)
		{
			DrawFrame(x, y, x + 120, y + 120, "sprites/e.spr", Vector(94, 235, 33), kRenderTransAdd, beatFrame);
		}
		else
		{
			DrawFrame(x, y, x + 120, y + 120, "sprites/f.spr", Vector(94, 235, 33), kRenderTransAdd, beatFrame);
		}

		// draw health logo
		x = 100 + gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3 + gHUD.camValue[0] * 0.1f;
		y = ScreenHeight + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 - 78 - gHUD.m_iFontHeight - gHUD.m_iFontHeight / 2 + gHUD.camValue[1] * 0.1f;

		gHUD.DrawBackground(x - heartScaler*2, y - heartScaler*2, x + 80 + heartScaler*2, y + 80 + heartScaler * 2, HEALTH_SPRITE, HEALTH_COLOR, kRenderTransAdd);

		// draw battery empty bar
		/*
		x = 200 + gHUD.m_Battery.m_iBat * 1.3f + gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3 + gHUD.camValue[0] * 0.1f;
		y = ScreenHeight + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 - 70 + gHUD.camValue[1] * 0.1f;
		scale = (100 - gHUD.m_Battery.m_iBat) * 1.3f;
		static float scaleLerpEmpty = 0.0f;
		scaleLerpEmpty = lerp(scaleLerpEmpty, scale, gHUD.m_flTimeDelta);

		FillRGBA(x, y, scaleLerpEmpty, 15, 144, 144, 144, 100);
		*/

		// draw battery
		x = 200 + gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3 + gHUD.camValue[0] * 0.1f;
		y = ScreenHeight + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 - 70 + gHUD.camValue[1] * 0.1f;
		scale = gHUD.m_Battery.m_iBat * 1.3f;
		static float scaleLerp = 0.0f;
		scaleLerp = lerp(scaleLerp, scale, gHUD.m_flTimeDelta);

		FillRGBA(x, y, 100 * 1.3f, 15, 144, 144, 144, 100);
		FillRGBA(x, y, scaleLerp, 15, 251, 177, 43, 255);

		// draw stamina empty bar
		x = 200 + m_iStamina * 1.3f + gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3 + gHUD.camValue[0] * 0.1f;
		y = ScreenHeight + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 - 50 + gHUD.camValue[1] * 0.1f;
		int stamina = (100-m_iStamina) * 1.3f;

		FillRGBA(x, y, stamina, 5, 144, 144, 144, 100);

		// draw stamina
		x = 200 + gHUD.bobValue[0] * 2.5f - gHUD.lagangle_x * 3 + gHUD.camValue[0] * 0.1f;
		y = ScreenHeight + gHUD.bobValue[1] * 2.5f + gHUD.velz * 10 - 50 + gHUD.camValue[1] * 0.1f;
		stamina = m_iStamina * 1.3f;

		FillRGBA(x, y, stamina, 5, 249, 111, 45, 255);

		// draw slowmo bar
		x = (ScreenWidth - gHUD.slowmoBar * 1.3f) / 2;
		y = 200;
		int slowmo = gHUD.slowmoBar * 1.3f;

		if (gHUD.isSlowmo)
		{
			//FillRGBA(x, y, slowmo, 5, 249, 111, 45, 255);
			//gHUD.DrawHudString(x, y - 20, 512, "Slowmo!", 249, 111, 45);
		}

	}

	DrawDamage(flTime);
	return DrawPain(flTime);
}

void CHudHealth::CalcDamageDirection(Vector vecFrom)
{
	Vector	forward, right, up;
	float	side, front;
	Vector vecOrigin, vecAngles;

	if (!vecFrom[0] && !vecFrom[1] && !vecFrom[2])
	{
		m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 0;
		return;
	}


	memcpy(vecOrigin, gHUD.m_vecOrigin, sizeof(Vector));
	memcpy(vecAngles, gHUD.m_vecAngles, sizeof(Vector));


	VectorSubtract (vecFrom, vecOrigin, vecFrom);

	float flDistToTarget = vecFrom.Length();

	vecFrom = vecFrom.Normalize();
	AngleVectors (vecAngles, forward, right, up);

	front = DotProduct (vecFrom, right);
	side = DotProduct (vecFrom, forward);

	if (flDistToTarget <= 50)
	{
		m_fAttackFront = m_fAttackRear = m_fAttackRight = m_fAttackLeft = 1;
	}
	else 
	{
		if (side > 0)
		{
			if (side > 0.3)
				m_fAttackFront = V_max(m_fAttackFront, side);
		}
		else
		{
			float f = fabs(side);
			if (f > 0.3)
				m_fAttackRear = V_max(m_fAttackRear, f);
		}

		if (front > 0)
		{
			if (front > 0.3)
				m_fAttackRight = V_max(m_fAttackRight, front);
		}
		else
		{
			float f = fabs(front);
			if (f > 0.3)
				m_fAttackLeft = V_max(m_fAttackLeft, f);
		}
	}
}

int CHudHealth::DrawPain(float flTime)
{
	if (!(m_fAttackFront || m_fAttackRear || m_fAttackLeft || m_fAttackRight))
		return 1;

	int r, g, b;
	int x, y, a, shade;

	// TODO:  get the shift value of the health
	a = 255;	// max brightness until then

	float fFade = gHUD.m_flTimeDelta * 2;
	
	// SPR_Draw top
	if (m_fAttackFront > 0.4)
	{
		GetPainColor(r,g,b);
		shade = a * V_max( m_fAttackFront, 0.5 );
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b );

		x = ScreenWidth/2 - SPR_Width(m_hSprite, 0)/2;
		y = ScreenHeight/2 - SPR_Height(m_hSprite,0) * 3;
		SPR_DrawAdditive(0, x, y, nullptr);
		m_fAttackFront = V_max( 0, m_fAttackFront - fFade );
	} else
		m_fAttackFront = 0;

	if (m_fAttackRight > 0.4)
	{
		GetPainColor(r,g,b);
		shade = a * V_max( m_fAttackRight, 0.5 );
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b );

		x = ScreenWidth/2 + SPR_Width(m_hSprite, 1) * 2;
		y = ScreenHeight/2 - SPR_Height(m_hSprite,1)/2;
		SPR_DrawAdditive(1, x, y, nullptr);
		m_fAttackRight = V_max( 0, m_fAttackRight - fFade );
	} else
		m_fAttackRight = 0;

	if (m_fAttackRear > 0.4)
	{
		GetPainColor(r,g,b);
		shade = a * V_max( m_fAttackRear, 0.5 );
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b );

		x = ScreenWidth/2 - SPR_Width(m_hSprite, 2)/2;
		y = ScreenHeight/2 + SPR_Height(m_hSprite,2) * 2;
		SPR_DrawAdditive(2, x, y, nullptr);
		m_fAttackRear = V_max( 0, m_fAttackRear - fFade );
	} else
		m_fAttackRear = 0;

	if (m_fAttackLeft > 0.4)
	{
		GetPainColor(r,g,b);
		shade = a * V_max( m_fAttackLeft, 0.5 );
		ScaleColors(r, g, b, shade);
		SPR_Set(m_hSprite, r, g, b );

		x = ScreenWidth/2 - SPR_Width(m_hSprite, 3) * 3;
		y = ScreenHeight/2 - SPR_Height(m_hSprite,3)/2;
		SPR_DrawAdditive(3, x, y, nullptr);

		m_fAttackLeft = V_max( 0, m_fAttackLeft - fFade );
	} else
		m_fAttackLeft = 0;

	return 1;
}

int CHudHealth::DrawDamage(float flTime)
{
	int r, g, b, a;
	DAMAGE_IMAGE *pdmg;

	if (!m_bitsDamage)
		return 1;

	r = giR;
	g = giG;
	b = giB;
	
	a = (int)( fabs(sin(flTime*2)) * 256.0);

	ScaleColors(r, g, b, a);

	// Draw all the items
	int i;
	for ( i = 0; i < NUM_DMG_TYPES; i++)
	{
		if (m_bitsDamage & giDmgFlags[i])
		{
			pdmg = &m_dmg[i];
			SPR_Set(gHUD.GetSprite(m_HUD_dmg_bio + i), r, g, b );
			SPR_DrawAdditive(0, pdmg->x, pdmg->y, &gHUD.GetSpriteRect(m_HUD_dmg_bio + i));
		}
	}


	// check for bits that should be expired
	for ( i = 0; i < NUM_DMG_TYPES; i++ )
	{
		DAMAGE_IMAGE *pdmg = &m_dmg[i];

		if ( m_bitsDamage & giDmgFlags[i] )
		{
			pdmg->fExpire = V_min( flTime + DMG_IMAGE_LIFE, pdmg->fExpire );

			if ( pdmg->fExpire <= flTime		// when the time has expired
				&& a < 40 )						// and the flash is at the low point of the cycle
			{
				pdmg->fExpire = 0;

				int y = pdmg->y;
				pdmg->x = pdmg->y = 0;

				// move everyone above down
				for (int j = 0; j < NUM_DMG_TYPES; j++)
				{
					pdmg = &m_dmg[j];
					if ((pdmg->y) && (pdmg->y < y))
						pdmg->y += giDmgHeight;

				}

				m_bitsDamage &= ~giDmgFlags[i];  // clear the bits
			}
		}
	}

	return 1;
}
 

void CHudHealth::UpdateTiles(float flTime, long bitsDamage)
{	
	DAMAGE_IMAGE *pdmg;

	// Which types are new?
	long bitsOn = ~m_bitsDamage & bitsDamage;
	
	for (int i = 0; i < NUM_DMG_TYPES; i++)
	{
		pdmg = &m_dmg[i];

		// Is this one already on?
		if (m_bitsDamage & giDmgFlags[i])
		{
			pdmg->fExpire = flTime + DMG_IMAGE_LIFE; // extend the duration
			if (!pdmg->fBaseline)
				pdmg->fBaseline = flTime;
		}

		// Are we just turning it on?
		if (bitsOn & giDmgFlags[i])
		{
			// put this one at the bottom
			pdmg->x = giDmgWidth/8;
			pdmg->y = ScreenHeight - giDmgHeight * 2;
			pdmg->fExpire=flTime + DMG_IMAGE_LIFE;
			
			// move everyone else up
			for (int j = 0; j < NUM_DMG_TYPES; j++)
			{
				if (j == i)
					continue;

				pdmg = &m_dmg[j];
				if (pdmg->y)
					pdmg->y -= giDmgHeight;

			}
			pdmg = &m_dmg[i];
		}	
	}	

	// damage bits are only turned on here;  they are turned off when the draw time has expired (in DrawDamage())
	m_bitsDamage |= bitsDamage;
}
