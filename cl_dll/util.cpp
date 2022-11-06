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
// util.cpp
//
// implementation of class-less helper functions
//

#include <cstdio>
#include <cstdlib>

#include "hud.h"
#include "cl_util.h"
#include <string.h>

HL_HSPRITE LoadSprite(const char *pszName)
{
	int i;
	char sz[256]; 

	if (ScreenWidth < 640)
		i = 320;
	else
		i = 640;

	sprintf(sz, pszName, i);

	return SPR_Load(sz);
}

void GetFallbackDir(char* falldir)
{
    char *pfile, *pfile2;
    pfile = pfile2 = (char*)gEngfuncs.COM_LoadFile("liblist.gam", 5, NULL);
    char token[1024];

    if (pfile == nullptr)
    {
        return;
    }

    while (pfile = gEngfuncs.COM_ParseFile(pfile, token))
    {
        if (!stricmp(token, "fallback_dir"))
        {
            pfile = gEngfuncs.COM_ParseFile(pfile, token);
            strcpy(falldir, token);
            break;
        }
    }

    gEngfuncs.COM_FreeFile(pfile2);
    pfile = pfile2 = nullptr;
}

//==========================
//	strUpper
//
//==========================
char* strUpper(char* str)
{
    char* temp;

    for (temp = str; *temp; temp++)
        *temp = toupper(*temp);

    return str;
}

// stub functions
void SET_MODEL(edict_t* e, const char* model) {}
int PRECACHE_MODEL(const char* s) { return 0; }