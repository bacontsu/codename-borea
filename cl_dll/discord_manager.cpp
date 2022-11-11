#include "stdio.h"
#include "stdlib.h"
#include <string>
#include <vector>
#include <string.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "vgui_int.h"
#include "vgui_TeamFortressViewport.h"
#include "triangleapi.h"
#include "r_studioint.h"
#include "com_model.h"

#include "include/discord_register.h"
#include "include/discord_rpc.h"
#include "time.h"

cvar_t* discord_rpc_updaterate = nullptr;

bool CDiscordRPCManager::Init()
{
	discord_rpc_updaterate = CVAR_CREATE("discord_rpc_updaterate", "5", FCVAR_ARCHIVE); // discord rpc update rate in seconds

	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	Discord_Initialize("1040499533492265051", &handlers, true, nullptr);
	runningTime = time(0); // initialize time

	// log
	gEngfuncs.Con_Printf("Discord RPC has been Initialized!\n");

	return true;
}

bool CDiscordRPCManager::VidInit()
{
	return true;
}

void CDiscordRPCManager::Shutdown()
{
	Discord_Shutdown(); // goodbye
}

void CDiscordRPCManager::Update()
{
	if (discordUpdate > gEngfuncs.GetAbsoluteTime())
		return;

	DiscordRichPresence discordPresence;
	memset(&discordPresence, 0, sizeof(discordPresence));

	// set logo and name
	discordPresence.largeImageKey = "logo"; // large image file name no extension
	discordPresence.largeImageText = "Codename Bor3A";
	discordPresence.smallImageKey = "   ";	// same as large
	discordPresence.smallImageText = "   "; // displays on hover


	if (strlen(gEngfuncs.pfnGetLevelName()) > 4)
	{
		discordPresence.state = "Playing In-game";
		std::string details = std::string("Map: ") + (gEngfuncs.pfnGetLevelName() + 5);;
		if (!chapterName.empty())
		{
			// show map chapter name if it exist
			details = std::string("Chapter: ") + chapterName;
		}
		
		discordPresence.details = details.c_str();
		discordPresence.startTimestamp = runningTime; // use recorded time
	}
	else
	{
		discordPresence.state = "in Menu";
		discordPresence.details = nullptr;
		discordPresence.startTimestamp = 0; // reset
	}

	Discord_UpdatePresence(&discordPresence); // do the do
	discordUpdate = gEngfuncs.GetAbsoluteTime() + discord_rpc_updaterate->value;
}