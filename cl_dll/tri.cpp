//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
//RENDERERS START
#include "bsprenderer.h"
#include "propmanager.h"
#include "particle_engine.h"
#include "watershader.h"
#include "mirrormanager.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"

extern CGameStudioModelRenderer g_StudioRenderer;
//RENDERERS END
#include "rain.h"
#include "com_model.h"
#include "studio_util.h"

#include "Exports.h"
#include "tri.h"

#include "glInclude.h"
#include "blur.h"

#include "svd_render.h"

extern int g_iWaterLevel;
extern Vector v_origin;

int UseTexture(HL_HSPRITE &hsprSpr, char * str)
{
	if (hsprSpr == 0)
	{
		char sz[256];
		sprintf( sz, "%s", str );
		hsprSpr = SPR_Load( sz );
	}

	return gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( hsprSpr ), 0 );
}

//
//-----------------------------------------------------
//

void SetPoint( float x, float y, float z, float (*matrix)[4])
{
	Vector point, result;
	point[0] = x;
	point[1] = y;
	point[2] = z;

	VectorTransform(point, matrix, result);

	gEngfuncs.pTriAPI->Vertex3f(result[0], result[1], result[2]);
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles()
{
//	RecClDrawNormalTriangles();

	//RENDERERS START
	//2012-02-25
	R_DrawNormalTriangles();
	//RENDERERS END

    SVD_DrawNormalTriangles();
	
	gHUD.m_Spectator.DrawOverview();
}

#if defined( _TFC )
void RunEventList();
#endif

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/

void DLLEXPORT HUD_DrawTransparentTriangles( void )
{
//	RecClDrawTransparentTriangles();
	//RENDERERS START
	//2012-02-25
	R_DrawTransparentTriangles();
	//RENDERERS END

#if defined( _TFC )
	RunEventList();
#endif

	BlackFog();
    
	// LRC: find out the time elapsed since the last redraw
	static float fOldTime, fTime;
	fOldTime = fTime;
	fTime = gEngfuncs.GetClientTime();
}

// Draw Blood

void DrawBloodOverlay()
{
    if (gHUD.m_Health.m_iHealth < 30) {
        gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
        gEngfuncs.pTriAPI->Color4f(1, 1, 1, 1); //set 

        //calculate opacity
        float scale = (30 - gHUD.m_Health.m_iHealth) / 30.0f;
        if (gHUD.m_Health.m_iHealth != 0)
            gEngfuncs.pTriAPI->Brightness(scale);
        else
            gEngfuncs.pTriAPI->Brightness(1);

        //gEngfuncs.Con_Printf("scale :  %f health : %i", scale, gHUD.m_Health.m_iHealth);

        gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)
            gEngfuncs.GetSpritePointer(SPR_Load("sprites/damagehud.spr")), 4);
        gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
        gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad

        //top left
        gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f);
        gEngfuncs.pTriAPI->Vertex3f(0, 0, 0);

        //bottom left
        gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
        gEngfuncs.pTriAPI->Vertex3f(0, ScreenHeight, 0);

        //bottom right
        gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f);
        gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, ScreenHeight, 0);

        //top right
        gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
        gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, 0, 0);

        gEngfuncs.pTriAPI->End(); //end our list of vertexes
        gEngfuncs.pTriAPI->RenderMode(kRenderNormal); //return to normal
    }

    if (gHUD.isSlowmo)
    {
        gEngfuncs.pTriAPI->RenderMode(kRenderTransAdd); //additive
        gEngfuncs.pTriAPI->Color4f(1, 1, 1, 1); //set 

        //calculate opacity
        gEngfuncs.pTriAPI->Brightness(gHUD.slowmoStrength);

        //gEngfuncs.Con_Printf("scale :  %f health : %i", scale, gHUD.m_Health.m_iHealth);

        gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)
            gEngfuncs.GetSpritePointer(SPR_Load("sprites/slowmo.spr")), 4);
        gEngfuncs.pTriAPI->CullFace(TRI_NONE); //no culling
        gEngfuncs.pTriAPI->Begin(TRI_QUADS); //start our quad

        //top left
        gEngfuncs.pTriAPI->TexCoord2f(0.0f, 1.0f);
        gEngfuncs.pTriAPI->Vertex3f(0, 0, 0);

        //bottom left
        gEngfuncs.pTriAPI->TexCoord2f(0.0f, 0.0f);
        gEngfuncs.pTriAPI->Vertex3f(0, ScreenHeight, 0);

        //bottom right
        gEngfuncs.pTriAPI->TexCoord2f(1.0f, 0.0f);
        gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, ScreenHeight, 0);

        //top right
        gEngfuncs.pTriAPI->TexCoord2f(1.0f, 1.0f);
        gEngfuncs.pTriAPI->Vertex3f(ScreenWidth, 0, 0);

        gEngfuncs.pTriAPI->End(); //end our list of vertexes
        gEngfuncs.pTriAPI->RenderMode(kRenderNormal); //return to normal
    }

    if (gHUD.slowmoUpdate < gHUD.m_flTime)
    {
        if (gHUD.slowmoStrength <= 0.5f)
            gHUD.slowmoMode = 1;
        else if (gHUD.slowmoStrength >= 1.0f)
            gHUD.slowmoMode = 2;

        if(gHUD.slowmoMode == 1)
            gHUD.slowmoStrength += 0.05f;
        else if (gHUD.slowmoMode == 2)
            gHUD.slowmoStrength -= 0.05f;

        gHUD.slowmoUpdate = gHUD.m_flTime + 0.01f;
    }

}

void HUD_DrawBloodOverlay(void)
{
    DrawBloodOverlay();
}
