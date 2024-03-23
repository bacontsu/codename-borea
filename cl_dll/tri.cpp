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

// buz start

#include "PlatformHeaders.h"
#include "SDL2/SDL_opengl.h"

#include "r_studioint.h"
extern engine_studio_api_t IEngineStudio;

#include "com_model.h"
#include "studio.h"

#include "StudioModelRenderer.h"
#include "GameStudioModelRenderer.h"
extern CGameStudioModelRenderer g_StudioRenderer;

#define SURF_PLANEBACK 2
#define SURF_DRAWSKY 4
#define SURF_DRAWSPRITE 8
#define SURF_DRAWTURB 0x10
#define SURF_DRAWTILED 0x20
#define SURF_DRAWBACKGROUND 0x40
#define SURF_UNDERWATER 0x80
#define SURF_DONTWARP 0x100
#define BACKFACE_EPSILON 0.01

// 0-2 are axial planes
#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2

// buz end

// buz start

void ClearBuffer(void);
extern bool g_bShadows;


/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void DLLEXPORT HUD_DrawNormalTriangles()
{
//	RecClDrawNormalTriangles();
	ClearBuffer();

	//RENDERERS START
	//2012-02-25
	R_DrawNormalTriangles();
	//RENDERERS END

    // buz start
    // òóò ìîæíî áþëî ïðîñòî íàðèñîâàòü ñåðûé êâàäðàò íà âåñü ýêðàí, êàê ýòî ÷àñòî äåëàþò
    // â ñòåíñèëüíûõ òåíÿõ òàêîãî ðîäà, îäíàêî âìåñòî ýòîãî ÿ ïðîáåãàþñü ïî ïîëèãîíàì world'à,
    // è ðèñóþ ñåðûì òîëüêî òå, êîòîðûå îáðàùåíû ê "ñîëíûøêó", ÷òîáû òåíü íå ðèñîâàëàñü
    // íà "îáðàòíûõ" ñòåíêàõ.
    if (g_bShadows && IEngineStudio.IsHardware())
    {
        glPushAttrib(GL_ALL_ATTRIB_BITS);

        // buz: workaround half-life's bug, when multitexturing left enabled after
        // rendering brush entities
        gBSPRenderer.glActiveTextureARB(GL_TEXTURE1_ARB);
        glDisable(GL_TEXTURE_2D);
        gBSPRenderer.glActiveTextureARB(GL_TEXTURE0_ARB);

        glDepthMask(GL_FALSE);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(GL_ZERO, GL_ZERO, GL_ZERO, 0.5);

        glStencilFunc(GL_NOTEQUAL, 0, ~0);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glEnable(GL_STENCIL_TEST);

        // draw world
        gBSPRenderer.RecursiveWorldNodeSolid(gBSPRenderer.m_pWorld->nodes);

        glPopAttrib();
    }

    g_bShadows = false;
    // buz end

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
