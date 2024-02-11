#ifdef REGS_ENGINEHOOK_H_RECURSE_GUARD
#error Recursive header files inclusion detected in reGS_enginehook.h
#else //REGS_ENGINEHOOK_H_RECURSE_GUARD

#define REGS_ENGINEHOOK_H_RECURSE_GUARD

#ifndef REGS_ENGINEHOOK_H_GUARD
#define REGS_ENGINEHOOK_H_GUARD
#pragma once

void HWHook();

#define Find(future_name)                                                                                                                  \
	{                                                                                                                                      \
		auto f##future_name = utils.FindAsync(ORIG_##future_name, patterns::engine::future_name);                                          \
		auto pattern = f##future_name.get();                                                                                               \
		if (ORIG_##future_name)                                                                                                            \
		{                                                                                                                                  \
			gEngfuncs.Con_DPrintf("[hw dll] Found " #future_name " at %p (using the %s pattern).\n", ORIG_##future_name, pattern->name()); \
		}                                                                                                                                  \
	}

#define Hook(future_name)                                                                                                                  \
	{                                                                                                                                      \
		auto f##future_name = utils.FindAsync(ORIG_##future_name, patterns::engine::future_name);                                          \
		auto pattern = f##future_name.get();                                                                                               \
		if (ORIG_##future_name)                                                                                                            \
		{                                                                                                                                  \
			gEngfuncs.Con_DPrintf("[hw dll] Found " #future_name " at %p (using the %s pattern).\n", ORIG_##future_name, pattern->name()); \
			void* p##future_name = (void*)ORIG_##future_name;                                                                              \
			MH_CreateHook(p##future_name, (void*)##future_name, (void**)&ORIG_##future_name);                                       \
		}                                                                                                                                  \
		else                                                                                                                               \
		{                                                                                                                                  \
			gEngfuncs.Con_DPrintf("[hw dll] Could not find " #future_name ".\n");                                                          \
		}                                                                                                                                  \
	}

#endif //REGS_ENGINEHOOK_H_GUARD

#undef REGS_ENGINEHOOK_H_RECURSE_GUARD
#endif //REGS_ENGINEHOOK_H_RECURSE_GUARD