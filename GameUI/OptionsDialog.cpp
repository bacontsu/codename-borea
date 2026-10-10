//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "BasePanel.h"
#include "OptionsDialog.h"

#include "vgui_controls/Button.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/PropertySheet.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/QueryBox.h"

#include "vgui/ILocalize.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"

#include "KeyValues.h"
#include "OptionsSubKeyboard.h"
#include "OptionsSubMouse.h"
#include "OptionsSubAudio.h"
#include "OptionsSubVideo.h"
#include "OptionsSubVoice.h"
#include "OptionsSubMultiplayer.h"
#include "OptionsSubDifficulty.h"
#include "ModInfo.h"

using namespace vgui2;

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>

//-----------------------------------------------------------------------------
// Purpose: Basic help dialog
//-----------------------------------------------------------------------------
COptionsDialog::COptionsDialog(vgui2::Panel* parent): PropertyDialog(parent, "OptionsDialog")
{
	SetDeleteSelfOnClose(true);

	int w = 512;
	int h = 406;
	if (IsProportional())
	{
		w = scheme()->GetProportionalScaledValueEx(GetScheme(), w);
		h = scheme()->GetProportionalScaledValueEx(GetScheme(), h);
	}

	SetBounds(0, 0, w, h);

	SetSizeable(false);

	SetTitle("#GameUI_Options", true);

	// debug timing code, this function takes too long
	//	double s4 = system()->GetCurrentTime();

	auto levelname = engine->pfnGetLevelName();
	bool bInGame = levelname && *levelname;

	if (ModInfo().IsSinglePlayerOnly() && !ModInfo().NoDifficulty() 
		&& !bInGame)
	{
		AddPage(new COptionsSubDifficulty(this), "#GameUI_Difficulty");
	}

	AddPage(new COptionsSubKeyboard(this), "#GameUI_Keyboard");
	AddPage(new COptionsSubMouse(this), "#GameUI_Mouse");

	m_pOptionsSubAudio = new COptionsSubAudio(this);
	AddPage(m_pOptionsSubAudio, "#GameUI_Audio");
	m_pOptionsSubVideo = new COptionsSubVideo(this);
	AddPage(m_pOptionsSubVideo, "#GameUI_Video");

	if (!ModInfo().IsSinglePlayerOnly())
	{
		AddPage(new COptionsSubVoice(this), "#GameUI_Voice");
	}

	//	double s5 = system()->GetCurrentTime();
	//	Msg("COptionsDialog::COptionsDialog(): %.3fms\n", (float)(s5 - s4) * 1000.0f);

	SetApplyButtonVisible(true);
	GetPropertySheet()->SetTabWidth(84);
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsDialog::~COptionsDialog()
{
}

//-----------------------------------------------------------------------------
// Purpose: Brings the dialog to the fore
//-----------------------------------------------------------------------------
void COptionsDialog::Activate()
{
	BaseClass::Activate();
	EnableApplyButton(false);
}

void COptionsDialog::OnKeyCodePressed(KeyCode code)
{
	switch (code)
	{
	case KEY_XBUTTON_B:
		OnCommand("Cancel");
		return;
	}

	BaseClass::OnKeyCodePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose: Opens the dialog
//-----------------------------------------------------------------------------
void COptionsDialog::Run()
{
	SetTitle("#GameUI_Options", true);
	Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Called when the GameUI is hidden
//-----------------------------------------------------------------------------
void COptionsDialog::OnGameUIHidden()
{
	// tell our children about it
	for (int i = 0; i < GetChildCount(); i++)
	{
		Panel* pChild = GetChild(i);
		if (pChild)
		{
			PostMessage(pChild, new KeyValues("GameUIHidden"));
		}
	}
}