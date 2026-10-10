//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef CHANGEGAMEDIALOG_H
#define CHANGEGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>

//-----------------------------------------------------------------------------
// Purpose: Dialogs for use to change current loaded mod
//-----------------------------------------------------------------------------
class CChangeGameDialog: public vgui2::Frame
{
public:
	CChangeGameDialog(vgui2::Panel* parent);
	~CChangeGameDialog();

	virtual void OnCommand(const char* command);
	virtual void OnClose();

private:
	void LoadModList();

	vgui2::ListPanel* m_pModList = nullptr;

	typedef vgui2::Frame BaseClass;
};


#endif // CHANGEGAMEDIALOG_H
