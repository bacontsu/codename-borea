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
// hud.cpp
//
// implementation of CHud class
//

//LRC - define to help track what calls are made on changelevel, save/restore, etc
#define ENGINE_DEBUG

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"
#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "mp3.h"
#include "demo.h"
#include "demo_api.h"
#include "vgui_ScorePanel.h"
#include "rain.h"
#include "blur.h"
#include "SDL2/SDL_syswm.h"

#include "videoengine.h"
#include "openal/OpenAL_System.h"
extern CSoundSystem gSoundSystem;

#include "tempents.h"

hud_player_info_t	 g_PlayerInfoList[MAX_PLAYERS+1];	   // player info from the engine
extra_player_info_t  g_PlayerExtraInfo[MAX_PLAYERS+1];   // additional player info sent directly to the client dll
//RENDERERS START
#include "bsprenderer.h"
#include "propmanager.h"
#include "textureloader.h"
#include "particle_engine.h"
#include "watershader.h"
#include "mirrormanager.h"
#include "goldsrc_spriterenderer.h"
#include "r_efx.h"

#include "studio.h"
#include "StudioModelRenderer.h"

#include "filesystem_utils.h"

extern CStudioModelRenderer g_StudioRenderer;
extern engine_studio_api_t IEngineStudio;
//RENDERERS END

int giR, giG, giB;

extern int giOldWeapons;
int g_iUseEnt;
std::string g_szUseEntClassname;

SDL_Window* m_hGameWindow;

class CHLVoiceStatusHelper : public IVoiceStatusHelper
{
public:
    void GetPlayerTextColor(int entindex, int color[3]) override
    {
		color[0] = color[1] = color[2] = 255;

		if( entindex >= 0 && entindex < sizeof(g_PlayerExtraInfo)/sizeof(g_PlayerExtraInfo[0]) )
		{
			int iTeam = g_PlayerExtraInfo[entindex].teamnumber;

			if ( iTeam < 0 )
			{
				iTeam = 0;
			}

			iTeam = iTeam % iNumberOfTeamColors;

			color[0] = iTeamColors[iTeam][0];
			color[1] = iTeamColors[iTeam][1];
			color[2] = iTeamColors[iTeam][2];
		}
	}

    void UpdateCursorState() override
    {
		gViewPort->UpdateCursorState();
	}

    int	GetAckIconHeight() override
    {
		return ScreenHeight - gHUD.m_iFontHeight*3 - 6;
	}

    bool CanShowSpeakerLabels() override
    {
		if( gViewPort && gViewPort->m_pScoreBoard )
			return !gViewPort->m_pScoreBoard->isVisible();
		else
			return false;
	}
};
static CHLVoiceStatusHelper g_VoiceStatusHelper;


extern client_sprite_t *GetSpriteList(client_sprite_t *pList, const char *psz, int iRes, int iCount);

extern cvar_t *sensitivity;
cvar_t *cl_lw = nullptr;
cvar_t* cl_rollangle = nullptr;
cvar_t* cl_rollspeed = nullptr;
cvar_t* cl_bobtilt = nullptr;

void ShutdownInput ();

int __MsgFunc_HudColor(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	//giR = READ_BYTE();
	//giG = READ_BYTE();
	//giB = READ_BYTE();
	return 1;
}

int __MsgFunc_OldWeapon(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	giOldWeapons = READ_BYTE();
	return 1;
}

//DECLARE_MESSAGE(m_Logo, Logo)
int __MsgFunc_Logo(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Logo(pszName, iSize, pbuf );
}

//LRC
/*
int __MsgFunc_HUDColor(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_HUDColor(pszName, iSize, pbuf );
}
*/

// Trinity
int __MsgFunc_SetFog(const char* pszName, int iSize, void* pbuf)
{
	gHUD.MsgFunc_SetFog(pszName, iSize, pbuf);
	return 1;
}

//LRC
int __MsgFunc_KeyedDLight(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_KeyedDLight( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_Test(const char *pszName, int iSize, void *pbuf)
{ return 1; }

//LRC
int __MsgFunc_SetSky(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_SetSky( pszName, iSize, pbuf );
	return 1;
}

// G-Cont. rain message
int __MsgFunc_RainData(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_RainData( pszName, iSize, pbuf );
}

//LRC 1.8
int __MsgFunc_ClampView(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_ClampView( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_ResetHUD(const char *pszName, int iSize, void *pbuf)
{
#ifdef ENGINE_DEBUG
	CONPRINT("## ResetHUD\n");
#endif
	return gHUD.MsgFunc_ResetHUD(pszName, iSize, pbuf );
}

int __MsgFunc_InitHUD(const char *pszName, int iSize, void *pbuf)
{
#ifdef ENGINE_DEBUG
	CONPRINT("## InitHUD\n");
#endif
	gHUD.MsgFunc_InitHUD( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_ViewMode(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_ViewMode( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_SetFOV(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_SetFOV( pszName, iSize, pbuf );
}

int __MsgFunc_Concuss(const char *pszName, int iSize, void *pbuf)
{
	return gHUD.MsgFunc_Concuss( pszName, iSize, pbuf );
}

int __MsgFunc_GameMode(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_GameMode( pszName, iSize, pbuf );
}

int __MsgFunc_PlayMP3(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_PlayMP3( pszName, iSize, pbuf );
}


int __MsgFunc_CamData(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_CamData( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_Inventory(const char *pszName, int iSize, void *pbuf)
{
	gHUD.MsgFunc_Inventory( pszName, iSize, pbuf );
	return 1;
}

int __MsgFunc_ChapterName(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	g_DiscordRPC.chapterName = READ_STRING();

	return 1;
}

int __MsgFunc_SendAnim(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	auto p = &engine_cl->viewent;
	int iAnim = READ_SHORT();
	int iBody = READ_SHORT();
	int iBlend = READ_BYTE();

	p->latched.prevsequence = p->curstate.sequence;
	engine_cl->weaponstarttime = 0;

	gEngfuncs.pfnWeaponAnim(iAnim, iBody);

	// doesnt do anything rn
	if (iBlend == 0)
		p->latched.prevsequence = iAnim;

	return true;

}

int __MsgFunc_PlayVideo(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	gVideoEngine.LoadVideo(READ_STRING());
	return true;
}

// TFFree Command Menu
void __CmdFunc_OpenCommandMenu()
{
	if ( gViewPort )
	{
		gViewPort->ShowCommandMenu( gViewPort->m_StandardMenu );
	}
}

// TFC "special" command
void __CmdFunc_InputPlayerSpecial()
{
	if ( gViewPort )
	{
		gViewPort->InputPlayerSpecial();
	}
}

void __CmdFunc_CloseCommandMenu()
{
	if ( gViewPort )
	{
		gViewPort->InputSignalHideCommandMenu();
	}
}

void __CmdFunc_ForceCloseCommandMenu()
{
	if ( gViewPort )
	{
		gViewPort->HideCommandMenu();
	}
}

void __CmdFunc_StopMP3( )
{
	gMP3.StopMP3();
}

// TFFree Command Menu Message Handlers
int __MsgFunc_ValClass(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ValClass( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamNames(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamNames( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Feign(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Feign( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_Detpack(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Detpack( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_VGUIMenu(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_VGUIMenu( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_MOTD(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_MOTD( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_BuildSt(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_BuildSt( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_RandomPC(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_RandomPC( pszName, iSize, pbuf );
	return 0;
}
 
int __MsgFunc_ServerName(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ServerName( pszName, iSize, pbuf );
	return 0;
}

/*
int __MsgFunc_ScoreInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ScoreInfo( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamScore(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamScore( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_TeamInfo(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_TeamInfo( pszName, iSize, pbuf );
	return 0;
}
*/

int __MsgFunc_Spectator(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_Spectator( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_SpecFade(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_SpecFade( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_ResetFade(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_ResetFade( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_AllowSpec(const char *pszName, int iSize, void *pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_AllowSpec( pszName, iSize, pbuf );
	return 0;
}
//RENDERERS START
int MsgFunc_SetFog(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_SetFog( pszName, iSize, pbuf );
}
int __MsgFunc_LightStyle(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_LightStyle( pszName, iSize, pbuf );
}
int __MsgFunc_CreateDecal(const char *pszName, int iSize, void *pbuf )
{
	return gBSPRenderer.MsgCustomDecal( pszName, iSize, pbuf );
}
int __MsgFunc_StudioDecal(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_StudioDecal( pszName, iSize, pbuf );
}
int __MsgFunc_MuzzleFlash(const char* pszName, int iSize, void* pbuf)
{
	return gHUD.MsgFunc_MuzzleFlash(pszName, iSize, pbuf);
}
int __MsgFunc_SkyMark_S(const char *pszName, int iSize, void *pbuf )
{
	return gBSPRenderer.MsgSkyMarker_Sky( pszName, iSize, pbuf );
}
int __MsgFunc_SkyMark_W(const char *pszName, int iSize, void *pbuf )
{
	return gBSPRenderer.MsgSkyMarker_World( pszName, iSize, pbuf );
}
int __MsgFunc_DynLight(const char *pszName, int iSize, void *pbuf )
{
	return gBSPRenderer.MsgDynLight( pszName, iSize, pbuf );
}
int __MsgFunc_FreeEnt(const char *pszName, int iSize, void *pbuf )
{
	return gHUD.MsgFunc_FreeEnt( pszName, iSize, pbuf );
}
int __MsgFunc_Particle(const char *pszName, int iSize, void *pbuf )
{
	return gParticleEngine.MsgCreateSystem( pszName, iSize, pbuf );
}
int __MsgFunc_WaterInfo(const char* pszName, int iSize, void* pbuf)
{
	return gWaterShader.MsgWaterInfo(pszName, iSize, pbuf);
}
int __MsgFunc_DebugLine(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	Vector p1, p2, color;
	p1.x = READ_COORD();
	p1.y = READ_COORD();
	p1.z = READ_COORD();

	p2.x = READ_COORD();
	p2.y = READ_COORD();
	p2.z = READ_COORD();

	color.x = READ_FLOAT();
	color.y = READ_FLOAT();
	color.z = READ_FLOAT();

	R_QueueDebugLine(p1, p2, color);
	return 1;
}
int __MsgFunc_WpnSkn(const char* pszName, int iSize, void* pbuf)
{
	gHUD.MsgFunc_WpnSkn(pszName, iSize, pbuf);
	return 1;
}
int __MsgFunc_UseEnt(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	g_iUseEnt = READ_SHORT();
	g_szUseEntClassname = READ_STRING();
	return 1;
}

int __MsgFunc_BloodPuddle(const char* pszName, int iSize, void* pbuf)
{
	//todo: make this use (and follow?) the entity's origin

	BEGIN_READ(pbuf, iSize);
	Vector origin;
	Vector angles;
	origin.x = READ_COORD();
	origin.y = READ_COORD();
	origin.z = READ_COORD();

	angles.x = READ_COORD();
	angles.y = READ_COORD();
	angles.z = READ_COORD();

	byte bloodcolor = READ_BYTE();
	byte bloodscale = READ_BYTE();

	TEMPENTITY* ent = gEngfuncs.pEfxAPI->CL_TempEntAllocNoModel(origin);

	switch (bloodcolor)
	{
		//case BLOOD_COLOR_GREEN:
	case BLOOD_COLOR_YELLOW:
		ent->entity.curstate.skin = 1;
		break;
	case BLOOD_COLOR_RED:
	default:
		ent->entity.curstate.skin = 0;
		break;
	}
	ent->entity.curstate.scale = bloodscale;

	new CTempBloodPuddle(ent);

	return 1;
}

// Aynekko: kick
int __MsgFunc_KickPunch( const char *pszName, int iSize, void *pbuf )
{
	gHUD.MsgFunc_KickPunch( pszName, iSize, pbuf );
	return 1;
}


//void InitPostEffects(); //Forward Declaration for Post-Processing

//RENDERERS END

int __MsgFunc_TeamFull( const char *pszName, int iSize, void *pbuf )
{
	if( gViewPort )
		return gViewPort->MsgFunc_TeamFull( pszName, iSize, pbuf );
	return 0;
}

int __MsgFunc_SetMenuTeam(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_SetMenuTeam(pszName, iSize, pbuf);
	return 0;
}

int __MsgFunc_StatsInfo(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_StatsInfo(pszName, iSize, pbuf);
	return 0;
}

int __MsgFunc_StatsPlayer(const char* pszName, int iSize, void* pbuf)
{
	if (gViewPort)
		return gViewPort->MsgFunc_StatsPlayer(pszName, iSize, pbuf);
	return 0;
}

cvar_t* gl_shadows_opacity;

extern std::vector<TEMPENTITY*> gpTempEnts;

//taken from GameEngine-reGoldSrc/ repository
void GetWindowNameFromGameDir(char* output, int outputBufferSize)
{
	FileHandle_t hLiblist = g_pFileSystem->Open("liblist.gam", "rt");

	if (hLiblist)
	{
		char line[512];

		while (!g_pFileSystem->EndOfFile(hLiblist))
		{
			*line = '\0';
			g_pFileSystem->ReadLine(line, sizeof(line) - 1, hLiblist);

			if (!strnicmp(line, "game", 4))
			{
				auto pszStart = strchr(line, '"');

				if (pszStart)
				{
					auto pszEnd = strchr(pszStart + 1, '"');

					if (pszEnd)
					{
						const int uiLength = pszEnd - pszStart;

						if ((uiLength - 1) < outputBufferSize)
						{
							strncpy(output, pszStart + 1, uiLength - 1);
							output[uiLength - 1] = '\0';
						}
					}
				}
				break;
			}
		}

		g_pFileSystem->Close(hLiblist);
	}
}

// This is called every time the DLL is loaded
void CHud :: Init()
{
#ifdef ENGINE_DEBUG
	CONPRINT("## CHud::Init\n");
#endif
	HOOK_MESSAGE( Logo );
	HOOK_MESSAGE( ResetHUD );
	HOOK_MESSAGE( GameMode );
	HOOK_MESSAGE( InitHUD );
	HOOK_MESSAGE( ViewMode );
	HOOK_MESSAGE( SetFOV );
	HOOK_MESSAGE( Concuss );
	//HOOK_MESSAGE( HUDColor ); //LRC
	HOOK_MESSAGE( KeyedDLight ); //LRC
//	HOOK_MESSAGE( KeyedELight ); //LRC
	HOOK_MESSAGE( Test ); //LRC
	HOOK_MESSAGE( SetSky ); //LRC
	HOOK_MESSAGE( CamData );//G-Cont. for new camera style 	
	HOOK_MESSAGE( RainData );//G-Cont. for rain control 
	HOOK_MESSAGE( Inventory ); //AJH Inventory system
	HOOK_MESSAGE( ClampView ); //LRC 1.8
	HOOK_MESSAGE( OldWeapon );

	//KILLAR: MP3	
	if(gMP3.Initialize())
	{
		HOOK_MESSAGE( PlayMP3 );
		HOOK_COMMAND( "stopaudio", StopMP3 );
	}

	// TFFree CommandMenu
	HOOK_COMMAND( "+commandmenu", OpenCommandMenu );
	HOOK_COMMAND( "-commandmenu", CloseCommandMenu );
	HOOK_COMMAND( "ForceCloseCommandMenu", ForceCloseCommandMenu );
	HOOK_COMMAND( "special", InputPlayerSpecial );

	HOOK_MESSAGE( ValClass );
	HOOK_MESSAGE( TeamNames );
	HOOK_MESSAGE( Feign );
	HOOK_MESSAGE( Detpack );
	HOOK_MESSAGE( MOTD );
	HOOK_MESSAGE( BuildSt );
	HOOK_MESSAGE( RandomPC );
	HOOK_MESSAGE( ServerName );
	/*
	HOOK_MESSAGE( ScoreInfo );
	HOOK_MESSAGE( TeamScore );
	HOOK_MESSAGE( TeamInfo );
	*/

	HOOK_MESSAGE( Spectator );
	HOOK_MESSAGE( AllowSpec );
	HOOK_MESSAGE( SetMenuTeam );
	HOOK_MESSAGE( StatsInfo );
	HOOK_MESSAGE( StatsPlayer );
	HOOK_MESSAGE( HudColor );
	HOOK_MESSAGE( SpecFade );
	HOOK_MESSAGE( ResetFade );
	HOOK_MESSAGE( TeamFull );

	HOOK_MESSAGE(BloodPuddle);

	// VGUI Menus
	HOOK_MESSAGE( VGUIMenu );
	HOOK_MESSAGE(ChapterName);

	CVAR_CREATE( "hud_classautokill", "1", FCVAR_ARCHIVE | FCVAR_USERINFO );		// controls whether or not to suicide immediately on TF class switch
	CVAR_CREATE( "hud_takesshots", "0", FCVAR_ARCHIVE );		// controls whether or not to automatically take screenshots at the end of a round
	
	CVAR_CREATE("cl_animbone", "1", FCVAR_ARCHIVE);
	CVAR_CREATE("cl_camoffset", "0", FCVAR_ARCHIVE);

	CVAR_CREATE("te_sunflare", "1", FCVAR_ARCHIVE);

	CVAR_CREATE("test1", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("test2", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("test3", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("test4", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("test5", "0", FCVAR_ARCHIVE);
	CVAR_CREATE("test6", "0", FCVAR_ARCHIVE);

	gl_shadows_opacity = CVAR_CREATE("gl_shadows_opacity", "0.5", FCVAR_ARCHIVE);

	//RENDERERS START
	HOOK_MESSAGE( SetFog );
	HOOK_MESSAGE( LightStyle );
	HOOK_MESSAGE( CreateDecal );
	HOOK_MESSAGE( StudioDecal );
	HOOK_MESSAGE( MuzzleFlash );
	HOOK_MESSAGE( SkyMark_S );
	HOOK_MESSAGE( SkyMark_W );
	HOOK_MESSAGE( DynLight );
	HOOK_MESSAGE( FreeEnt );
	HOOK_MESSAGE( Particle );
	HOOK_MESSAGE( DebugLine );
	HOOK_MESSAGE( WaterInfo );
	HOOK_MESSAGE( WpnSkn );
	HOOK_MESSAGE(UseEnt);
	HOOK_MESSAGE(SendAnim);
	HOOK_MESSAGE(PlayVideo);

	findColorFromTxtFile("hudcolors.txt", "COMMONHUD", &giR, &giG, &giB, nullptr );

	//R_Init(); do this right after getting ienginestudio

	gSoundSystem.Init();
	gVideoEngine.Init();

	// Aynekko
	HOOK_MESSAGE( KickPunch );

	g_ImGUIManager.Init();
	g_DiscordRPC.Init();
	
	//start glow effect --FragBait0
	CVAR_CREATE("r_glow", "0", FCVAR_ARCHIVE );
	//CVAR_CREATE("r_glowmode", "0", FCVAR_ARCHIVE ); //AJH this is now redundant
	CVAR_CREATE("r_glowstrength", "1", FCVAR_ARCHIVE );
	CVAR_CREATE("r_glowblur", "4", FCVAR_ARCHIVE );
	CVAR_CREATE("r_glowdark", "2", FCVAR_ARCHIVE );
	CVAR_CREATE("r_titleslang", "titles.txt", FCVAR_ARCHIVE);
	//end glow effect

	viewEntityIndex = 0; // trigger_viewset stuff
	viewFlags = 0;
	m_iLogo = 0;
	m_iFOV = 0;
	numMirrors = 0;

	CVAR_CREATE( "zoom_sensitivity_ratio", "1.2", 0 );
	CVAR_CREATE("cl_autowepswitch", "1", FCVAR_ARCHIVE | FCVAR_USERINFO);
	default_fov = CVAR_CREATE( "default_fov", "90", FCVAR_ARCHIVE);
	m_pCvarStealMouse = CVAR_CREATE( "hud_capturemouse", "1", FCVAR_ARCHIVE );
	m_pCvarDraw = CVAR_CREATE( "hud_draw", "1", FCVAR_ARCHIVE );
	cl_lw = gEngfuncs.pfnGetCvarPointer( "cl_lw" );
	cl_rollangle = CVAR_CREATE("cl_rollangle", "2.0", FCVAR_ARCHIVE);
	cl_rollspeed = CVAR_CREATE("cl_rollspeed", "200", FCVAR_ARCHIVE);
	cl_bobtilt = CVAR_CREATE("cl_bobtilt", "0", FCVAR_ARCHIVE);

	RainInfo = gEngfuncs.pfnRegisterVariable( "cl_raininfo", "0", 0 );
	m_pSpriteList = nullptr;

	// Clear any old HUD list
	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = nullptr;
	}

	// In case we get messages before the first update -- time will be valid
	m_flTime = 1.0;

	m_Ammo.Init();
	m_Health.Init();
	m_SayText.Init();
	m_Spectator.Init();
	m_Geiger.Init();
	m_Train.Init();
	m_Battery.Init();
	m_Flash.Init();
	m_Message.Init();
	m_Scoreboard.Init();
	m_StatusBar.Init();
	m_DeathNotice.Init();
	m_AmmoSecondary.Init();
	m_TextMessage.Init();
	m_StatusIcons.Init();
	m_FlagIcons.Init();
	m_PlayerBrowse.Init();
	GetClientVoiceMgr()->Init(&g_VoiceStatusHelper, (vgui::Panel**)&gViewPort);

	m_PointMessage.Init();

	m_Menu.Init();

	MsgFunc_ResetHUD(nullptr, 0, nullptr );

	SDL_Window* game_window = nullptr;

	char gameWindowName[512];
	GetWindowNameFromGameDir(gameWindowName, sizeof(gameWindowName));

	extern SDL_Window* hlWindow;

	m_hGameWindow = hlWindow;

	// pre-hl25 anniversary goldsrc uses sdl versions < 2.0.0, 
	// its very limited and buggy, for example, using SDL_SetWindowBordered
	// changes the window size, and thus switching back and forth
	// from bordered and not bordered makes window smaller. so
	// no special sdl code for now


}

// CHud destructor
// cleans up memory allocated for m_rg* arrays
CHud :: ~CHud()
{
	//	this is currently called in a moment where the engine functions have been shut down
	// so calling conprint just gives an error
//#ifdef ENGINE_DEBUG
//	CONPRINT("## CHud::destructor\n");
// 
//#endif
	delete [] m_rghSprites;
	delete [] m_rgrcRects;
	delete [] m_rgszSpriteNames;
	gMP3.Shutdown();

	if ( m_pHudList )
	{
		HUDLIST *pList;
		while ( m_pHudList )
		{
			pList = m_pHudList;
			m_pHudList = m_pHudList->pNext;
			free( pList );
		}
		m_pHudList = nullptr;
	}

	//RENDERERS START
	R_Shutdown();
	//RENDERERS END
}

// GetSpriteIndex()
// searches through the sprite list loaded from hud.txt for a name matching SpriteName
// returns an index into the gHUD.m_rghSprites[] array
// returns 0 if sprite not found
int CHud :: GetSpriteIndex( const char *SpriteName )
{
	// look through the loaded sprite name list for SpriteName
	for ( int i = 0; i < m_iSpriteCount; i++ )
	{
		if ( strncmp( SpriteName, m_rgszSpriteNames + (i * MAX_SPRITE_NAME_LENGTH), MAX_SPRITE_NAME_LENGTH ) == 0 )
			return i;
	}

	return -1; // invalid sprite
}

void CHud :: VidInit()
{
#ifdef ENGINE_DEBUG
	CONPRINT("## CHud::VidInit (hi from me)\n");
#endif
	m_scrinfo.iSize = sizeof(m_scrinfo);
	GetScreenInfo(&m_scrinfo);

	extern void GetDefaultSettings(int* width, int* height);
	GetDefaultSettings(&m_scrinfo.iWidth, &m_scrinfo.iHeight);

	// ----------
	// Load Sprites
	// ---------
//	m_hsprFont = LoadSprite("sprites/%d_font.spr");
	
	m_hsprLogo = 0;	
	m_hsprCursor = 0;
	numMirrors = 0;
	slowmoUpdate = 0;
	slowmoStrength = 0.5f;

	if (ScreenWidth < 640)
		m_iRes = 320;
	else
		m_iRes = 640;

	// Only load this once
	if ( !m_pSpriteList )
	{
		// we need to load the hud.txt, and all sprites within
		m_pSpriteList = SPR_GetList("sprites/hud.txt", &m_iSpriteCountAllRes);

		if (m_pSpriteList)
		{
			// count the number of sprites of the appropriate res
			m_iSpriteCount = 0;
			client_sprite_t *p = m_pSpriteList;
			int j;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
					m_iSpriteCount++;
				p++;
			}

			// allocated memory for sprite handle arrays
 			m_rghSprites = new HSPRITE_GOLDSRC[m_iSpriteCount];
			m_rgrcRects = new Rect[m_iSpriteCount];
			m_rgszSpriteNames = new char[m_iSpriteCount * MAX_SPRITE_NAME_LENGTH];

			p = m_pSpriteList;
			int index = 0;
			for ( j = 0; j < m_iSpriteCountAllRes; j++ )
			{
				if ( p->iRes == m_iRes )
				{
					char sz[256];
					sprintf(sz, "sprites/%s.spr", p->szSprite);
					m_rghSprites[index] = SPR_Load(sz);
					m_rgrcRects[index] = p->rc;
					strncpy( &m_rgszSpriteNames[index * MAX_SPRITE_NAME_LENGTH], p->szName, MAX_SPRITE_NAME_LENGTH );

					index++;
				}

				p++;
			}
		}
	}
	else
	{
		// we have already have loaded the sprite reference from hud.txt, but
		// we need to make sure all the sprites have been loaded (we've gone through a transition, or loaded a save game)
		client_sprite_t *p = m_pSpriteList;
		int index = 0;
		for ( int j = 0; j < m_iSpriteCountAllRes; j++ )
		{
			if ( p->iRes == m_iRes )
			{
				char sz[256];
				sprintf( sz, "sprites/%s.spr", p->szSprite );
				m_rghSprites[index] = SPR_Load(sz);
				index++;
			}

			p++;
		}
	}

	// assumption: number_1, number_2, etc, are all listed and loaded sequentially
	m_HUD_number_0 = GetSpriteIndex( "number_0" );

	m_iFontHeight = m_rgrcRects[m_HUD_number_0].bottom - m_rgrcRects[m_HUD_number_0].top;

	m_Ammo.VidInit();
	m_Health.VidInit();
	m_Spectator.VidInit();
	m_Geiger.VidInit();
	m_Train.VidInit();
	m_Battery.VidInit();
	m_Flash.VidInit();
	m_Message.VidInit();
	m_Scoreboard.VidInit();
	m_StatusBar.VidInit();
	m_DeathNotice.VidInit();
	m_SayText.VidInit();
	m_Menu.VidInit();
	m_AmmoSecondary.VidInit();
	m_TextMessage.VidInit();
	m_StatusIcons.VidInit();
	m_FlagIcons.VidInit();
	m_PlayerBrowse.VidInit();
	GetClientVoiceMgr()->VidInit();

	R_VidInit();

	g_ImGUIManager.VidInit();
	g_DiscordRPC.VidInit();

	m_PointMessage.VidInit();

	gVideoEngine.VidInit();
}

int CHud::MsgFunc_Logo(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	// update Train data
	m_iLogo = READ_BYTE();

	return 1;
}

//LRC
int CHud::MsgFunc_HUDColor(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	m_iHUDColor = READ_LONG();

	return 1;
}

float g_lastFOV = 0.0;

/*
============
COM_FileBase
============
*/
// Extracts the base name of a file (no path, no extension, assumes '/' as path separator)
void COM_FileBase ( const char *in, char *out)
{
	int len, start, end;

	len = strlen( in );
	
	// scan backward for '.'
	end = len - 1;
	while ( end && in[end] != '.' && in[end] != '/' && in[end] != '\\' )
		end--;
	
	if ( in[end] != '.' )		// no '.', copy to end
		end = len-1;
	else 
		end--;					// Found ',', copy to left of '.'


	// Scan backward for '/'
	start = len-1;
	while ( start >= 0 && in[start] != '/' && in[start] != '\\' )
		start--;

	if ( in[start] != '/' && in[start] != '\\' )
		start = 0;
	else 
		start++;

	// Length of new sting
	len = end - start + 1;

	// Copy partial string
	strncpy( out, &in[start], len );
	// Terminate it
	out[len] = 0;
}

/*
=================
HUD_IsGame

=================
*/
int HUD_IsGame( const char *game )
{
	const char *gamedir;
	char gd[ 1024 ];

	gamedir = gEngfuncs.pfnGetGameDirectory();
	if ( gamedir && gamedir[0] )
	{
		COM_FileBase( gamedir, gd );
		if ( !stricmp( gd, game ) )
			return 1;
	}
	return 0;
}

/*
=====================
HUD_GetFOV

Returns last FOV
=====================
*/
float HUD_GetFOV()
{
	if ( gEngfuncs.pDemoAPI->IsRecording() )
	{
		// Write it
		int i = 0;
		unsigned char buf[ 100 ];

		// Active
		*( float * )&buf[ i ] = g_lastFOV;
		i += sizeof( float );

		Demo_WriteBuffer( TYPE_ZOOM, i, buf );
	}

	if ( gEngfuncs.pDemoAPI->IsPlayingback() )
	{
		g_lastFOV = g_demozoom;
	}
	return g_lastFOV;
}

int CHud::MsgFunc_SetFOV(const char *pszName,  int iSize, void *pbuf)
{
	BEGIN_READ( pbuf, iSize );

	int newfov = READ_BYTE();
	int def_fov = CVAR_GET_FLOAT( "default_fov" );

	//Weapon prediction already takes care of changing the fog. ( g_lastFOV ).
	//But it doesn't restore correctly so this still needs to be used
	/*
	if ( cl_lw && cl_lw->value )
		return 1;
		*/

	g_lastFOV = newfov;

	if ( newfov == 0 )
	{
		m_iFOV = def_fov;
	}
	else
	{
		m_iFOV = newfov;
	}

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == def_fov )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)def_fov) * CVAR_GET_FLOAT("zoom_sensitivity_ratio");
	}

	return 1;
}


void CHud::AddHudElem(CHudBase *phudelem)
{
	HUDLIST *pdl, *ptemp;

//phudelem->Think();

	if (!phudelem)
		return;

	pdl = (HUDLIST *)malloc(sizeof(HUDLIST));
	if (!pdl)
		return;

	memset(pdl, 0, sizeof(HUDLIST));
	pdl->p = phudelem;

	if (!m_pHudList)
	{
		m_pHudList = pdl;
		return;
	}

	ptemp = m_pHudList;

	while (ptemp->pNext)
		ptemp = ptemp->pNext;

	ptemp->pNext = pdl;
}

float CHud::GetSensitivity()
{
	return m_flMouseSensitivity;
}

void CHud::DrawBackground(float xmin, float ymin, float xmax, float ymax, char* sprite, Vector color, int mode)
{
	//setup
	gEngfuncs.pTriAPI->RenderMode(mode);
	gEngfuncs.pTriAPI->Brightness(1.0f);
	glColor4ub(color.x, color.y, color.z, 255);
	gEngfuncs.pTriAPI->CullFace(TRI_NONE);
	model_t* model = (model_t*)GetSpritePointer(SPR_Load(sprite));
	mspriteframe_t* frame = g_LegacySpriteRenderer.GetSpriteFrame(model, 1, 0);
	gEngfuncs.pTriAPI->SpriteTexture((struct model_s*)GetSpritePointer(SPR_Load(sprite)), 0);
	//gEngfuncs.pfnDrawString seems to fuck this up so we need to bind texture manually
	glBindTexture(GL_TEXTURE_2D, frame->gl_texturenum);

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

namespace strutil
{
/**
  * @brief Splits input std::string str according to input std::string delim.
  *        Taken from: https://github.com/tgalaj/strutil/blob/master/strutil.h.
  * @param str - std::string that will be split.
  * @param delim - the delimiter.
  * @param keep_empty - if true (default), empty tokens are kept in the result;
  *                     if false, empty tokens are skipped.
  * @return std::vector<std::string> that contains all splitted tokens.
  */
	inline std::vector<std::string> split(const std::string& str, const std::string& delim, bool keep_empty = true)
	{
		size_t pos_start = 0, pos_end, delim_len = delim.length();
		std::string token;
		std::vector<std::string> tokens;

		while ((pos_end = str.find(delim, pos_start)) != std::string::npos)
		{
			token = str.substr(pos_start, pos_end - pos_start);
			pos_start = pos_end + delim_len;
			if (keep_empty || !token.empty())
			{
				tokens.push_back(token);
			}
		}

		token = str.substr(pos_start);
		if (keep_empty || !token.empty())
		{
			tokens.push_back(token);
		}
		return tokens;
	}

}


void findColorFromTxtFile(const char* filepath, const char* colorname, int* r_out, int* g_out, int* b_out, int* a_out)
{
	std::string path = gEngfuncs.pfnGetGameDirectory() + std::string("/") + std::string(filepath);
	FileHandle_t f = g_pFileSystem->Open(filepath, "r");
	if (f == FILESYSTEM_INVALID_HANDLE)
		return;
	while (!g_pFileSystem->EndOfFile(f)) {
		char buffer[512];
		g_pFileSystem->ReadLine(buffer, 512, f);
		std::vector<std::string> tokens = strutil::split(buffer, " ");
		if (tokens.empty())
			continue;
		if (tokens[0][0] == '/')
			continue;
		if (tokens[0] != colorname)
			continue;

		assert(tokens.size() == 5);
		if(r_out) *r_out = atoi(tokens[1].c_str());
		if(g_out) *g_out = atoi(tokens[2].c_str());
		if(b_out) *b_out = atoi(tokens[3].c_str());
		if(a_out) *a_out = atoi(tokens[4].c_str());
		break;
	}
	g_pFileSystem->Close(f);
}