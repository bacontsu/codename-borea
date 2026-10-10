//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef LOADGAMEDIALOG_H
#define LOADGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "BaseSaveGameDialog.h"
#include "SaveGameDialog.h"
#include "BasePanel.h"

//-----------------------------------------------------------------------------
// Purpose: Displays game loading options
//-----------------------------------------------------------------------------
class CLoadGameDialog: public CBaseSaveGameDialog
{
	DECLARE_CLASS_SIMPLE(CLoadGameDialog, CBaseSaveGameDialog);

public:
	CLoadGameDialog(vgui2::Panel* parent);
	~CLoadGameDialog();

	virtual void OnCommand(const char* command);
};

#endif // LOADGAMEDIALOG_H