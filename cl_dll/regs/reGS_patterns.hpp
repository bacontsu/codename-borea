// BunnymodXT contributors & SmileyAG provided patterns.

#ifdef REGS_PATTERNS_HPP_RECURSE_GUARD
#error Recursive header files inclusion detected in reGS_patterns.hpp
#else //REGS_PATTERNS_HPP_RECURSE_GUARD

#define REGS_PATTERNS_HPP_RECURSE_GUARD

#ifndef REGS_PATTERNS_HPP_GUARD
#define REGS_PATTERNS_HPP_GUARD
#pragma once

#include "sptlib/patterns.hpp"
#include "sptlib/MemUtils.h"

namespace patterns
{
namespace engine
{
	PATTERNS(Host_Quit_Restart_f,
		"HL-SteamPipe-8684",
		"8B 0D ?? ?? ?? ?? B8 05 00 00 00");

	PATTERNS(S_ExtraUpdate,
		"HL-SteamPipe-8684",
		"E8 ?? ?? ?? ?? 85 C0 75 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? D9 05 ?? ?? ?? ??");

	PATTERNS(R_DrawWorld,
		"HL-SteamPipe-8684",
		"55 8B EC 81 EC B8 0B 00 00");

	PATTERNS(Sys_Error,
		"HL-SteamPipe-8684",
		"55 8B EC 81 EC ?? ?? ?? ?? 8B 4D ?? 8D 45 ?? 50 51 8D 95 ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 C4 ?? 85 C0 74 ?? 8D 85");
}
}

#endif //REGS_PATTERNS_HPP_GUARD

#undef REGS_PATTERNS_HPP_RECURSE_GUARD
#endif //REGS_PATTERNS_HPP_RECURSE_GUARD