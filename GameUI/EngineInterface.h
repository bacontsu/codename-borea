#ifndef GAMEUI_ENGINEINTERFACE_H
#define GAMEUI_ENGINEINTERFACE_H

#define GAMEUI_UNMANGLER

#include "Platform.h"

typedef int (*pfnUserMsgHook)(const char* pszName, int iSize, void* pbuf);

#include "mathlib.h"
#include "cdll_int.h"
#include "cvardef.h"

#undef ARRAYSIZE
#define ARRAYSIZE(p) (sizeof(p) / sizeof(p[0]))

extern cl_enginefunc_t gEngfuncs;

extern cl_enginefunc_t* engine;

#endif // GAMEUI_ENGINEINTERFACE_H
