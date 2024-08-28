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
// hud_redraw.cpp
//
#include "hud.h"
#include "cl_util.h"
#include "bench.h"

#include "vgui_TeamFortressViewport.h"
#include "vgui_StatsMenuPanel.h"

#include "postprocess.h"
#include "blur.h"

#include "com_model.h"
#include "triangleapi.h"

extern int g_iUseEnt;
extern std::string g_szUseEntClassname;
void HUD_MarkUsableEnt();
extern int KB_ConvertString(char* in, char** ppout);

void HUD_DrawBloodOverlay(void);

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};

inline float UTIL_Lerp( float lerpfactor, float A, float B ) { return A + lerpfactor*(B-A); }

extern int g_iVisibleMouse;

float HUD_GetFOV();

extern cvar_t *sensitivity;

// Think
void CHud::Think()
{
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	int newfov;
	HUDLIST *pList = m_pHudList;

	while (pList)
	{
		if (pList->p->m_iFlags & HUD_ACTIVE)
			pList->p->Think();
		pList = pList->pNext;
	}

	newfov = HUD_GetFOV();
	if ( newfov == 0 )
	{
		m_iFOV = default_fov->value;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == default_fov->value )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)default_fov->value) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	// think about default fov
	if ( m_iFOV == 0 )
	{  // only let players adjust up in fov,  and only if they are not overriden by something else
		m_iFOV = V_max( default_fov->value, 90 );  
	}
	
	if ( gEngfuncs.IsSpectateOnly() )
	{
		m_iFOV = gHUD.m_Spectator.GetFOV();	// default_fov->value;
	}

	Bench_CheckStart();
}

// 
// Redraw
// step through the local data,  placing the appropriate graphics & text as appropriate
// returns 1 if they've changed, 0 otherwise
int CHud :: Redraw( float flTime, int intermission )
{
	//RENDERERS START
	gHUD.gBloomRenderer.Draw();
	gPostProcess.ApplyPostEffects(); //PostProcessing
	gHUD.gLensflare.Draw(flTime);
	gBlur.DrawBlur();
	//RENDERERS END

	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	static float m_flShotTime = 0;
	
	//LRC - handle fog fading effects. (is this the right place for it?)
	if (g_fFogFadeDuration)
	{
		// Nicer might be to use some kind of logarithmic fade-in?
		double fFraction = m_flTimeDelta/g_fFogFadeDuration;
		if ( fFraction > 0 )
		{
			g_fFogFadeFraction += fFraction;

	//		CONPRINT("FogFading: %f - %f, frac %f, time %f, final %d\n", g_fStartDist, g_fEndDist, fFraction, flTime, g_iFinalEndDist);

			if (g_fFogFadeFraction >= 1.0f)
			{
				// fading complete
				g_fFogFadeFraction = 1.0f;
				g_fFogFadeDuration = 0.0f;
			}

			// set the new fog values
			g_fog.endDist = UTIL_Lerp( g_fFogFadeFraction, g_fogPreFade.endDist, g_fogPostFade.endDist );
			g_fog.startDist = UTIL_Lerp( g_fFogFadeFraction, g_fogPreFade.startDist, g_fogPostFade.startDist );
			g_fog.fogColor[0] = UTIL_Lerp( g_fFogFadeFraction, g_fogPreFade.fogColor[0], g_fogPostFade.fogColor[0] );
			g_fog.fogColor[1] = UTIL_Lerp( g_fFogFadeFraction, g_fogPreFade.fogColor[1], g_fogPostFade.fogColor[1] );
			g_fog.fogColor[2] = UTIL_Lerp( g_fFogFadeFraction, g_fogPreFade.fogColor[2], g_fogPostFade.fogColor[2] );
		}
	}
	
	// Clock was reset, reset delta
	if ( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;

	// Bring up the scoreboard during intermission
	if (gViewPort)
	{
		if ( m_iIntermission && !intermission )
		{
			// Have to do this here so the scoreboard goes away
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideScoreBoard();
			if (gViewPort->m_pStatsMenu && gViewPort->m_pStatsMenu->isVisible())
			{
				gViewPort->m_pStatsMenu->setVisible(true);
			}
			gViewPort->UpdateSpectatorPanel();
		}
		else if ( !m_iIntermission && intermission )
		{
			m_iIntermission = intermission;
			gViewPort->HideCommandMenu();
			gViewPort->HideVGUIMenu();
			gViewPort->ShowScoreBoard();
			gViewPort->UpdateSpectatorPanel();

			// Take a screenshot if the client's got the cvar set
			if ( CVAR_GET_FLOAT( "hud_takesshots" ) != 0 )
				m_flShotTime = flTime + 1.0;	// Take a screenshot in a second
		}
	}

	if (m_flShotTime && m_flShotTime < flTime)
	{
		gEngfuncs.pfnClientCmd("snapshot\n");
		m_flShotTime = 0;
	}

	m_iIntermission = intermission;

	// if no redrawing is necessary
	// return 0;

	// trigger_viewset stuff
	if ((viewFlags & 1) && (viewFlags & 4))	//AJH Draw the camera hud
	{
	
		int r, g, b, x, y, a;
		//wrect_t rc;
		HL_HSPRITE m_hCam1;
		int HUD_camera_active;
		int HUD_camera_rect;

		a = 225;

		UnpackRGB(r,g,b, gHUD.m_iHUDColor);
		ScaleColors(r, g, b, a);

		//Draw the flashing camera active logo
			HUD_camera_active = gHUD.GetSpriteIndex( "camera_active" );
			m_hCam1 = gHUD.GetSprite(HUD_camera_active);
			SPR_Set(m_hCam1, r, g, b );
			x = SPR_Width(m_hCam1, 0);
			x = ScreenWidth - x;
			y = SPR_Height(m_hCam1, 0)/2;
		
			// Draw the camera sprite at 1 fps
			int i = (int)(flTime) % 2;
			i = grgLogoFrame[i] - 1;

			SPR_DrawAdditive( i,  x, y, nullptr);

		//Draw the camera reticle (top left)
			HUD_camera_rect = gHUD.GetSpriteIndex( "camera_rect_tl" );
			m_hCam1 = gHUD.GetSprite(HUD_camera_rect);
			SPR_Set(m_hCam1, r, g, b );
			x = ScreenWidth/4;
			y = ScreenHeight/4;
		
			SPR_DrawAdditive( 0,  x, y, &gHUD.GetSpriteRect(HUD_camera_rect));

		//Draw the camera reticle (top right)
			HUD_camera_rect = gHUD.GetSpriteIndex( "camera_rect_tr" );
			m_hCam1 = gHUD.GetSprite(HUD_camera_rect);
			SPR_Set(m_hCam1, r, g, b );

			int w,h;
			w=SPR_Width(m_hCam1, 0)/2;
			h=SPR_Height(m_hCam1, 0)/2;

			x = ScreenWidth - ScreenWidth/4 - w ;
			y = ScreenHeight/4;
		
			SPR_DrawAdditive( 0,  x, y, &gHUD.GetSpriteRect(HUD_camera_rect));

		//Draw the camera reticle (bottom left)
			HUD_camera_rect = gHUD.GetSpriteIndex( "camera_rect_bl" );
			m_hCam1 = gHUD.GetSprite(HUD_camera_rect);
			SPR_Set(m_hCam1, r, g, b );
			x = ScreenWidth/4;
			y = ScreenHeight - ScreenHeight/4 - h;
		
			SPR_DrawAdditive( 0,  x, y, &gHUD.GetSpriteRect(HUD_camera_rect));

		//Draw the camera reticle (bottom right)
			HUD_camera_rect = gHUD.GetSpriteIndex( "camera_rect_br" );
			m_hCam1 = gHUD.GetSprite(HUD_camera_rect);
			SPR_Set(m_hCam1, r, g, b );
			x = ScreenWidth - ScreenWidth/4 - w ;
			y = ScreenHeight - ScreenHeight/4 - h;
		
			SPR_DrawAdditive( 0,  x, y, &gHUD.GetSpriteRect(HUD_camera_rect));
	}

	if ((viewFlags & 1) && !(viewFlags & 2)) // custom view active, and flag "draw hud" isnt set
		return 1;
	
	// BlueNightHawk - This fixes the viewmodel drawing on top of the hud
	glDepthRange(0.0f, 0.0f);

	HUD_MarkUsableEnt();
	HUD_DrawBloodOverlay();

	// draw all registered HUD elements
	if ( m_pCvarDraw->value )
	{
		HUDLIST *pList = m_pHudList;

		while (pList)
		{
			if ( !Bench_Active() )
			{
				if ( !intermission )
				{
					if ( (pList->p->m_iFlags & HUD_ACTIVE) && !(m_iHideHUDDisplay & HIDEHUD_ALL) )
						pList->p->Draw(flTime);
				}
				else
				{  // it's an intermission,  so only draw hud elements that are set to draw during intermissions
					if ( pList->p->m_iFlags & HUD_INTERMISSION )
						pList->p->Draw( flTime );
				}
			}
			else
			{
				if ( ( pList->p == &m_Benchmark ) &&
					 ( pList->p->m_iFlags & HUD_ACTIVE ) &&
					 !( m_iHideHUDDisplay & HIDEHUD_ALL ) )
				{
					pList->p->Draw(flTime);
				}
			}

			pList = pList->pNext;
		}
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if (m_iLogo)
	{
		int x, y, i;

		if (m_hsprLogo == 0)
			m_hsprLogo = LoadSprite("sprites/%d_logo.spr");

		SPR_Set(m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width(m_hsprLogo, 0);
		x = ScreenWidth - x;
		y = SPR_Height(m_hsprLogo, 0)/2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive(i, x, y, nullptr);
	}

	glDepthRange(0.0f, 1.0f);

	/*
	if ( g_iVisibleMouse )
	{
		void IN_GetMousePos( int *mx, int *my );
		int mx, my;

		IN_GetMousePos( &mx, &my );
		
		if (m_hsprCursor == 0)
		{
			char sz[256];
			sprintf( sz, "sprites/cursor.spr" );
			m_hsprCursor = SPR_Load( sz );
		}

		SPR_Set(m_hsprCursor, 250, 250, 250 );
		
		// Draw the logo at 20 fps
		SPR_DrawAdditive( 0, mx, my, NULL );
	}
	*/

	return 1;
}

void ScaleColors( int &r, int &g, int &b, int a )
{
	float x = (float)a / 255;
	r = (int)(r * x);
	g = (int)(g * x);
	b = (int)(b * x);
}

int CHud :: DrawHudString(int xpos, int ypos, int iMaxX, char *szIt, int r, int g, int b )
{
	return xpos + gEngfuncs.pfnDrawString( xpos, ypos, szIt, r, g, b);
}

int CHud :: DrawHudNumberString( int xpos, int ypos, int iMinX, int iNumber, int r, int g, int b )
{
	char szString[32];
	sprintf( szString, "%d", iNumber );
	return DrawHudStringReverse( xpos, ypos, iMinX, szString, r, g, b );

}

// draws a string from right to left (right-aligned)
int CHud :: DrawHudStringReverse( int xpos, int ypos, int iMinX, char *szString, int r, int g, int b )
{
	/*
	return xpos - gEngfuncs.pfnDrawStringReverse( xpos, ypos, szString, r, g, b);
	*/

	//Op4 uses custom reverse drawing to fix an issue with the letter k overlapping the letter i in the string "kills"

	if (!(*szString))
	{
		return xpos;
	}

	char* i;

	for (i = szString; *i; ++i)
	{
	}

	--i;

	int x = xpos - gHUD.m_scrinfo.charWidths[*i];

	if (iMinX > x)
	{
		return xpos;
	}

	while (true)
	{
		gEngfuncs.pfnDrawCharacter(x, ypos, *i, r, g, b);

		if (i == szString)
			break;

		--i;

		const int width = gHUD.m_scrinfo.charWidths[*i];

		if (x - width < iMinX)
			break;

		x -= width;
	}

	return x;
}

int CHud :: DrawHudNumber( int x, int y, int iFlags, int iNumber, int r, int g, int b)
{
	int iWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;
	int k;
	
	if (iNumber > 0)
	{
		// SPR_Draw 100's
		if (iNumber >= 100)
		{
			 k = iNumber/100;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_3DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw 10's
		if (iNumber >= 10)
		{
			k = (iNumber % 100)/10;
			SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
			SPR_DrawAdditive( 0, x, y, &GetSpriteRect(m_HUD_number_0 + k));
			x += iWidth;
		}
		else if (iFlags & (DHN_3DIGITS | DHN_2DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		k = iNumber % 10;
		SPR_Set(GetSprite(m_HUD_number_0 + k), r, g, b );
		SPR_DrawAdditive(0,  x, y, &GetSpriteRect(m_HUD_number_0 + k));
		x += iWidth;
	} 
	else if (iFlags & DHN_DRAWZERO) 
	{
		SPR_Set(GetSprite(m_HUD_number_0), r, g, b );

		// SPR_Draw 100's
		if (iFlags & (DHN_3DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		if (iFlags & (DHN_3DIGITS | DHN_2DIGITS))
		{
			//SPR_DrawAdditive( 0, x, y, &rc );
			x += iWidth;
		}

		// SPR_Draw ones
		
		SPR_DrawAdditive( 0,  x, y, &GetSpriteRect(m_HUD_number_0));
		x += iWidth;
	}

	return x;
}


int CHud::GetNumWidth( int iNumber, int iFlags )
{
	if (iFlags & (DHN_3DIGITS))
		return 3;

	if (iFlags & (DHN_2DIGITS))
		return 2;

	if (iNumber <= 0)
	{
		if (iFlags & (DHN_DRAWZERO))
			return 1;
		else
			return 0;
	}

	if (iNumber < 10)
		return 1;

	if (iNumber < 100)
		return 2;

	return 3;

}	

int CHud::GetHudNumberWidth(int number, int width, int flags)
{
	const int digitWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;

	int totalDigits = 0;

	if (number > 0)
	{
		totalDigits = static_cast<int>(log10(number)) + 1;
	}
	else if (flags & DHN_DRAWZERO)
	{
		totalDigits = 1;
	}

	totalDigits = V_max(totalDigits, width);

	return totalDigits * digitWidth;
}

int CHud::DrawHudNumberReverse(int x, int y, int number, int flags, int r, int g, int b)
{
	if (number > 0 || (flags & DHN_DRAWZERO))
	{
		const int digitWidth = GetSpriteRect(m_HUD_number_0).right - GetSpriteRect(m_HUD_number_0).left;

		int remainder = number;

		do
		{
			const int digit = remainder % 10;
			const int digitSpriteIndex = m_HUD_number_0 + digit;

			//This has to happen *before* drawing because we're drawing in reverse
			x -= digitWidth;

			SPR_Set(GetSprite(digitSpriteIndex), r, g, b);
			SPR_DrawAdditive(0, x, y, &GetSpriteRect(digitSpriteIndex));

			remainder /= 10;
		}
		while (remainder > 0);
	}

	return x;
}

// offset from the edges of the screen
#define BORDER_X XRES(40)
#define BORDER_Y YRES(40)
// line thickness
#define BRACKET_TX XRES(2)
#define BRACKET_TY YRES(2)
// line length
#define BRACKET_LX XRES(20)
#define BRACKET_LY YRES(20)

void HUD_MarkUsableEnt(void)
{
	if (g_iUseEnt <= 0)

		return;



	cl_entity_t* ent = gEngfuncs.GetEntityByIndex(g_iUseEnt);



	if (!ent)
		return;

	if (!ent->model)
		return;


	// Aynekko
	Vector CenterOffset = (ent->curstate.mins + ent->curstate.maxs) / 2.f;
	Vector EntOrigin = ent->curstate.origin + CenterOffset;
	Vector scr_new;
	gEngfuncs.pTriAPI->WorldToScreen( EntOrigin, scr_new );
	int x_coord = XPROJECT( scr_new[0] );
	int y_coord = YPROJECT( scr_new[1] );
	int r = 255;
	int g = 255;
	int b = 255;


	std::string sprite = "sprites/use.spr";

	const int spr_half_scale = 32;
	gHUD.DrawBackground( x_coord - spr_half_scale, y_coord - spr_half_scale, x_coord + spr_half_scale, y_coord + spr_half_scale, (char *)sprite.c_str(), Vector( r, g, b ), kRenderTransAdd );


	/*
	Vector org, tmp, modelmins, modelmaxs;
	int j;
	Vector p[8];
	float gap = 0.0f;
	Vector screen[8];
	int max_x, max_y, min_x, min_y;
	max_x = max_y = 0;
	min_x = min_y = 8192;

	if (ent->model)

	{

		VectorCopy(ent->origin, org);



		if (ent->model->type == mod_studio) // ??????

		{

			VectorCopy(ent->curstate.mins, modelmins);

			VectorCopy(ent->curstate.maxs, modelmaxs);



			for (j = 0; j < 8; j++)

			{

				tmp[0] = (j & 1) ? modelmins[0] : modelmaxs[0];

				tmp[1] = (j & 2) ? modelmins[1] : modelmaxs[1];

				tmp[2] = (j & 4) ? modelmins[2] : modelmaxs[2];



				VectorCopy(tmp, p[j]);

				VectorAdd(p[j], org, p[j]);



				gEngfuncs.pTriAPI->WorldToScreen(p[j], screen[j]);



				screen[j][0] = XPROJECT(screen[j][0]);

				screen[j][1] = YPROJECT(screen[j][1]);

				screen[j][2] = 0.0f;

			}

		}

		else // ????

		{

			VectorCopy(ent->model->mins, modelmins);

			VectorCopy(ent->model->maxs, modelmaxs);



			for (j = 0; j < 8; j++)

			{

				tmp[0] = (j & 1) ? modelmins[0] - gap : modelmaxs[0] + gap;

				tmp[1] = (j & 2) ? modelmins[1] - gap : modelmaxs[1] + gap;

				tmp[2] = (j & 4) ? modelmins[2] - gap : modelmaxs[2] + gap;



				VectorCopy(tmp, p[j]);

			}



			if (ent->angles[0] || ent->angles[1] || ent->angles[2])

			{

				Vector	forward, right, up;



				AngleVectors(ent->angles, forward, right, up);



				for (j = 0; j < 8; j++)

				{

					VectorCopy(p[j], tmp);



					p[j][0] = DotProduct(tmp, forward);

					p[j][1] = DotProduct(tmp, right);

					p[j][2] = DotProduct(tmp, up);

				}

			}



			for (j = 0; j < 8; j++)

			{

				VectorAdd(p[j], org, p[j]);



				gEngfuncs.pTriAPI->WorldToScreen(p[j], screen[j]);



				screen[j][0] = XPROJECT(screen[j][0]);

				screen[j][1] = YPROJECT(screen[j][1]);

				screen[j][2] = 0.0f;

			}

		}

	}

	else // ??? ??????

	{

		for (j = 0; j < 8; j++)

		{

			tmp[0] = (j & 1) ? ent->curstate.mins[0] : ent->curstate.maxs[0];

			tmp[1] = (j & 2) ? ent->curstate.mins[1] : ent->curstate.maxs[1];

			tmp[2] = (j & 4) ? ent->curstate.mins[2] : ent->curstate.maxs[2];



			VectorAdd(tmp, ent->origin, tmp);

			VectorCopy(tmp, p[j]);



			gEngfuncs.pTriAPI->WorldToScreen(p[j], screen[j]);



			screen[j][0] = XPROJECT(screen[j][0]);

			screen[j][1] = YPROJECT(screen[j][1]);

			screen[j][2] = 0.0f;

		}

	}



	// ???????? ??????????

	for (j = 0; j < 8; j++)

	{

		if (screen[j][0] > max_x) max_x = screen[j][0];

		if (screen[j][1] > max_y) max_y = screen[j][1];



		if (screen[j][0] < min_x) min_x = screen[j][0];

		if (screen[j][1] < min_y) min_y = screen[j][1];

	}



	if (max_x < BORDER_X) max_x = BORDER_X;

	if (max_x > ScreenWidth - BORDER_X) max_x = ScreenWidth - BORDER_X;



	if (min_x < BORDER_X) min_x = BORDER_X;

	if (min_x > ScreenWidth - BORDER_X) min_x = ScreenWidth - BORDER_X;



	if (max_y < BORDER_Y) max_y = BORDER_Y;

	if (max_y > ScreenHeight - BORDER_Y) max_y = ScreenHeight - BORDER_Y;



	if (min_y < BORDER_Y) min_y = BORDER_Y;

	if (min_y > ScreenHeight - BORDER_Y) min_y = ScreenHeight - BORDER_Y;



	// ?????????

	int r, g, b, a;

	max_x += 30;
	min_x -= 30;

	max_y += 15;
	min_y -= 15;

	a = MIN_ALPHA;

	UnpackRGB(r, g, b, RGB_YELLOWISH);



	// ????

	// ?

//	FillRGBA(min_x, min_y + BRACKET_TY, BRACKET_TX, BRACKET_LY, r, g, b, a); // ?

	//FillRGBA(min_x, min_y, BRACKET_LX, BRACKET_TY, r, g, b, a); // ?

	// ?

	//FillRGBA(max_x, min_y + BRACKET_TY, BRACKET_TX, BRACKET_LY, r, g, b, a); // ?

	//FillRGBA(max_x + BRACKET_TX, min_y, -BRACKET_LX, BRACKET_TY, r, g, b, a); // ?



	// ???

	// ?

	//FillRGBA(min_x, max_y, BRACKET_TX, -BRACKET_LY, r, g, b, a); // ?

	//FillRGBA(min_x, max_y, BRACKET_LX, BRACKET_TY, r, g, b, a); // ?

	// ?

	//FillRGBA(max_x, max_y, BRACKET_TX, -BRACKET_LY, r, g, b, a); // ?
	//FillRGBA(max_x + BRACKET_TX, max_y, -BRACKET_LX, BRACKET_TY, r, g, b, a); // ?

//	char* keyout;
//	KB_ConvertString("+use", &keyout);
//	strUpper(keyout);
//	std::string boundkey;
	std::string sprite;

	if (g_szUseEntClassname == "func_pushable")
	{
	//	boundkey = keyout;
		sprite = "sprites/grab.spr";
	}
	else
	{
	//	boundkey = keyout;
		sprite = "sprites/use.spr";
	}

	// drawing
	gHUD.DrawBackground(min_x + BRACKET_TX * 2, min_y + BRACKET_TY, min_x + BRACKET_TX * 2 + 50, min_y + BRACKET_TY + 50 ,(char*)sprite.c_str(), Vector(r, g, b), kRenderTransAdd);
//	gHUD.DrawHudString(min_x + BRACKET_TX * 2 + 60, min_y + BRACKET_TY + 10, BRACKET_LX * 2, (char*)boundkey.c_str(), r, g, b);
*/
}
