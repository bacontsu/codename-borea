#include <algorithm>
#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "parsemsg.h"
#include "vgui_TeamFortressViewport.h"
#include "triangleapi.h"

#include "pm_defs.h"
#include "event_api.h"
#include "pmtrace.h"

DECLARE_MESSAGE( gLensflare, Lensflare )

int CHudLensflare::Init(void)
{
	HOOK_MESSAGE( Lensflare );

    m_iFlags |= HUD_ACTIVE;
    gHUD.AddHudElem(this);

    srand( (unsigned)time( NULL ) );

    return 1;
}

int CHudLensflare::VidInit(void)
{
    return 1;
}

int CHudLensflare::MsgFunc_Lensflare(const char *pszName,  int iSize, void *pbuf)
{
    BEGIN_READ( pbuf, iSize );

	Sunanglex = READ_COORD();
	Sunangley = READ_COORD();
	SunEnabled = READ_BYTE();

	m_iFlags |= HUD_ACTIVE;
    return 1;
}

extern Vector v_angles, v_origin;

pmtrace_t TraceIgnoreSky(Vector start, Vector end, int ignore = -1)
{
	physent_t* pe = nullptr;
	pmtrace_t tr = { 0 };

	tr = *(gEngfuncs.PM_TraceLine(start, end, PM_NORMAL, 2, ignore));
	pe = gEngfuncs.pEventAPI->EV_GetPhysent(tr.ent);

	if (!pe)
		return tr;

	// ignore models with sky_ as their name
	if (!strncmp(pe->name + 7, "sky_", 4))
		return TraceIgnoreSky(start, end, tr.ent);

	return tr;
}

int CHudLensflare::Draw(float flTime)
{  
	if (!(int)CVAR_GET_FLOAT("te_sunflare"))
		return 0;

	Vector sunangles, sundir, suntarget;
	Vector v_forward, v_right, v_up, angles;
	Vector forward, right, up, screen;
	pmtrace_t tr;

	if (SunEnabled == TRUE)
	{
		//gEngfuncs.Con_Printf("Lensflare is rendering\n");

		//Sun position
		if (Sunanglex != NULL && Sunangley != NULL)
		{
			sunangles.x = Sunanglex;
			sunangles.y = Sunangley;
		}
		else
		{
			sunangles.x = -45;
			sunangles.y = 0;
		}

		//sunangles.x = -45;
		//gEngfuncs.Con_Printf("pitch %f\n", sunangles.x);

		int vp[4];
		glGetIntegerv(GL_VIEWPORT, vp);

#ifdef ScreenWidth
#undef ScreenWidth
#define ScreenWidth vp[2]
#endif

#ifdef ScreenHeight
#undef ScreenHeight
#define ScreenHeight vp[3]
#endif

		float screenscale = (float)ScreenHeight / 480.0f;

		text[0] = SPR_Load("sprites/sunflare01.spr");
		red[0] = green[0] = blue[0] = 1.0;
		scale[0] = 45 * 2 * screenscale;
		multi[0] = -0.45 * 2 * screenscale;

		text[1] = SPR_Load("sprites/sunflare02.spr");
		red[1] = green[0] = blue[0] = 1.0;
		scale[1] = 25 * 2 * screenscale;
		multi[1] = 0.2 * 2 * screenscale;

		text[2] = SPR_Load("sprites/sunflare04.spr");
		red[2] = 132 / 255;
		green[2] = 1.0;
		blue[2] = 153 / 255;
		scale[2] = 35 * 2 * screenscale;
		multi[2] = 0.3 * 2 * screenscale;

		text[3] = SPR_Load("sprites/sunflare05.spr");
		red[3] = 1.0;
		green[3] = 164 / 255;
		blue[3] = 164 / 255;
		scale[3] = 40 * 2 * screenscale;
		multi[3] = 0.46 * 2 * screenscale;

		text[4] = SPR_Load("sprites/sunflare01.spr");
		red[4] = 1.0;
		green[4] = 164 / 255;
		blue[4] = 164 / 255;
		scale[4] = 52 * 2 * screenscale;
		multi[4] = 0.5 * 2 * screenscale;

		text[5] = SPR_Load("sprites/sunflare02.spr");
		red[5] = green[5] = blue[5] = 1.0;
		scale[5] = 31 * 2 * screenscale;
		multi[5] = 0.54 * 2 * screenscale;

		text[6] = SPR_Load("sprites/sunflare01.spr");
		red[6] = 0.6;
		green[6] = 1.0;
		blue[6] = 0.6;
		scale[6] = 26 * 2 * screenscale;
		multi[6] = 0.64 * 2 * screenscale;

		text[7] = SPR_Load("sprites/sunflare04.spr");
		red[7] = 0.5;
		green[7] = 1.0;
		blue[7] = 0.5;
		scale[7] = 20 * 2 * screenscale;
		multi[7] = 0.77 * 2 * screenscale;

		text[8] = SPR_Load("sprites/sunflare02.spr");

		text[9] = SPR_Load("sprites/sunflare01.spr");


		flPlayerBlend = 0.0;
		flPlayerBlend2 = 0.0;

		AngleVectors(v_angles, forward, null, null);

		AngleVectors(sunangles, sundir, null, null);

		suntarget = v_origin + sundir * 16384;

		tr = TraceIgnoreSky(v_origin, suntarget);

		if (gEngfuncs.PM_PointContents(tr.endpos, null) == CONTENTS_SKY)
		{
			flPlayerBlend = std::max(DotProduct(forward, sundir) - 0.85, 0.0) * 6.8;
			if (flPlayerBlend > 1.0)
				flPlayerBlend = 1.0;

			flPlayerBlend4 = std::max(DotProduct(forward, sundir) - 0.90, 0.0) * 6.6;
			if (flPlayerBlend4 > 1.0)
				flPlayerBlend4 = 1.0;

			flPlayerBlend6 = std::max(DotProduct(forward, sundir) - 0.80, 0.0) * 6.7;
			if (flPlayerBlend6 > 1.0)
				flPlayerBlend6 = 1.0;

			flPlayerBlend2 = flPlayerBlend6 * 140.0;
			flPlayerBlend3 = flPlayerBlend * 190.0;
			flPlayerBlend5 = flPlayerBlend4 * 222.0;

			Vector normal, point, origin;

			gEngfuncs.GetViewAngles((float*)normal);
			AngleVectors(normal, forward, right, up);

			VectorCopy(tr.endpos, origin);

			gEngfuncs.pTriAPI->WorldToScreen(tr.endpos, screen);

			Suncoordx = XPROJECT(screen[0]);
			Suncoordy = YPROJECT(screen[1]);

			if (Suncoordx < XRES(-10) || Suncoordx > XRES(650) || Suncoordy < YRES(-10) || Suncoordy > YRES(490))
				return 1;

			Screenmx = ScreenWidth / 2;
			Screenmy = ScreenHeight / 2;

			//gEngfuncs.Con_Printf("%i\n", ScreenHeight);

			Sundistx = Screenmx - Suncoordx;
			Sundisty = Screenmy - Suncoordy;

			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(SPR_Load("sprites/sunflare04.spr")), 0);//use hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(1.0, 1.0, 155 / 255.0, flPlayerBlend2 / 355.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 355.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Suncoordx + 190, Suncoordy + 190, 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Suncoordx + 190, Suncoordy - 190, 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Suncoordx - 190, Suncoordy - 190, 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Suncoordx - 190, Suncoordy + 190, 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);


			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(SPR_Load("sprites/sunflare05.spr")), 0);//use hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(1.0, 1.0, 1.0, flPlayerBlend3 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend3 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Suncoordx + 160, Suncoordy + 160, 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Suncoordx + 160, Suncoordy - 160, 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Suncoordx - 160, Suncoordy - 160, 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Suncoordx - 160, Suncoordy + 160, 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);


			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(SPR_Load("sprites/sunflare04.spr")), 0);//use hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(1.0, 1.0, 1.0, flPlayerBlend5 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend5 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(0, 0, 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(0, ScreenHeight, 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, ScreenHeight, 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, 0, 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			int i = 1;
			Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
			Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(red[i], green[i], green[i], flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] + scale[i], 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] - scale[i], 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] - scale[i], 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] + scale[i], 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			i++;
			Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
			Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(red[i], green[i], green[i], flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] + scale[i], 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] - scale[i], 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] - scale[i], 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] + scale[i], 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			i++;
			Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
			Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(red[i], green[i], green[i], flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] + scale[i], 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] - scale[i], 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] - scale[i], 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] + scale[i], 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			i++;
			Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
			Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(red[i], green[i], green[i], flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] + scale[i], 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] - scale[i], 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] - scale[i], 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] + scale[i], 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			i++;
			Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
			Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(red[i], green[i], green[i], flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] + scale[i], 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] - scale[i], 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] - scale[i], 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] + scale[i], 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			i++;
			Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
			Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(red[i], green[i], green[i], flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] + scale[i], 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] - scale[i], 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] - scale[i], 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] + scale[i], 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			i++;
			Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
			Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(red[i], green[i], green[i], flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] + scale[i], 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] + scale[i], Lensy[i] - scale[i], 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] - scale[i], 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx[i] - scale[i], Lensy[i] + scale[i], 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			i++;
			int scale1 = 32;
			int Lensx1, Lensy1 = 0;
			Lensx1 = (Suncoordx + (Sundistx * 0.88));
			Lensy1 = (Suncoordy + (Sundisty * 0.88));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(0.9, 0.9, 0.9, flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx1 + scale1, Lensy1 + scale1, 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx1 + scale1, Lensy1 - scale1, 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx1 - scale1, Lensy1 - scale1, 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx1 - scale1, Lensy1 + scale1, 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			i++;
			scale1 = 140;
			Lensx1 = (Suncoordx + (Sundistx * 1.1));
			Lensy1 = (Suncoordy + (Sundisty * 1.1));
			gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
			gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)gEngfuncs.GetSpritePointer(text[i]), 0); //hotglow, or any other sprite for the texture
			gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
			gEngfuncs.pTriAPI->Color4f(0.9, 0.9, 0.9, flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Brightness(flPlayerBlend2 / 255.0);
			gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx1 + scale1, Lensy1 + scale1, 0); //top left
			gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx1 + scale1, Lensy1 - scale1, 0); //bottom left
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx1 - scale1, Lensy1 - scale1, 0); //bottom right
			gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f); gEngfuncs.pTriAPI->Vertex3f(Lensx1 - scale1, Lensy1 + scale1, 0); //top right
			gEngfuncs.pTriAPI->End(); //end our list of vertexes
			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
		}
	}
	return 1;
}