//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONSDIALOG_H
#define OPTIONSDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PropertyDialog.h"

//-----------------------------------------------------------------------------
// Purpose: Holds all the game option pages
//-----------------------------------------------------------------------------
class COptionsDialog: public vgui2::PropertyDialog
{
	DECLARE_CLASS_SIMPLE(COptionsDialog, vgui2::PropertyDialog);

public:
	COptionsDialog(vgui2::Panel* parent);
	~COptionsDialog();

	void Run();
	virtual void Activate();

	void OnKeyCodePressed(vgui2::KeyCode code);

	//vgui2::PropertyPage* GetOptionsSubMultiplayer(void) { return m_pOptionsSubMultiplayer; }

	MESSAGE_FUNC(OnGameUIHidden, "GameUIHidden"); // called when the GameUI is hidden

private:
	class COptionsSubAudio* m_pOptionsSubAudio = nullptr;
	class COptionsSubVideo* m_pOptionsSubVideo = nullptr;
};


#define OPTIONS_MAX_NUM_ITEMS 15

struct OptionData_t;
#endif // OPTIONSDIALOG_H