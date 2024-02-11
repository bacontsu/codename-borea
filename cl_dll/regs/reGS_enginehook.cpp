// reGS_enginehook.cpp: hw.dll hooking
// Used libraries: SPTLib/MinHook

//#include "hud.h"
#include "minhook/MinHook.h"
#include "sptlib/patterns.hpp"
#include "sptlib/MemUtils.h"
#include "sptlib/Utils.hpp"
#include "reGS_enginehook.h"
#include "reGS_patterns.hpp"

#include "PlatformHeaders.h"



Utils utils = Utils::Utils(NULL, NULL, NULL);

typedef void (*_Host_Quit_Restart_f)();
_Host_Quit_Restart_f ORIG_Host_Quit_Restart_f = NULL;
void Host_Quit_Restart_f();

typedef void (*_S_ExtraUpdate)();
_S_ExtraUpdate ORIG_S_ExtraUpdate = NULL;
void S_ExtraUpdate();

typedef void (*_R_DrawWorld)();
_R_DrawWorld ORIG_R_DrawWorld = NULL;
void R_DrawWorld();

typedef void (*_Sys_Error)(const char* error, ...);
_Sys_Error ORIG_Sys_Error = NULL;
void Sys_Error(const char* error, ...);


void S_ExtraUpdate()
{
	ORIG_S_ExtraUpdate();
}

void Host_Quit_Restart_f()
{
	ORIG_Host_Quit_Restart_f();
}

void R_DrawWorld()
{

}

void Sys_Error(const char* error, ...)
{
	//ORIG_Sys_Error(error);
}

#include "hud.h"

void HWHook()
{
	void* handle;
	void* base;
	size_t size;

	if (!MemUtils::GetModuleInfo(L"hw.dll", &handle, &base, &size))
	{
		//gEngfuncs.Con_DPrintf("HWHook: can't get module info about hw.dll! Stopping hooking...\n");
		return;
	}

	utils = Utils::Utils(handle, base, size);

	/* Hooking all necessary funcs */
	Hook(Host_Quit_Restart_f);
	Hook(S_ExtraUpdate);
	Hook(R_DrawWorld);
	Hook(Sys_Error);

	MH_EnableHook(MH_ALL_HOOKS);
}