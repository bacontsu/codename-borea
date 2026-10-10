//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef SAVEGAMEDIALOG_H
#define SAVEGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "BaseSaveGameDialog.h"

//-----------------------------------------------------------------------------
// Purpose: Save game dialog
//-----------------------------------------------------------------------------
class CSaveGameDialog: public CBaseSaveGameDialog
{
	DECLARE_CLASS_SIMPLE(CSaveGameDialog, CBaseSaveGameDialog);

public:
	CSaveGameDialog(vgui2::Panel* parent);
	~CSaveGameDialog();

	virtual void Activate();
	static void FindSaveSlot(char* buffer, int bufsize);

protected:
	virtual void OnCommand(const char* command);
	virtual void OnScanningSaveGames();
};

#define SAVE_NUM_ITEMS 4

#endif // SAVEGAMEDIALOG_H