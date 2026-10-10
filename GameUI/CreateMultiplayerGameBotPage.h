//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//
#ifndef GAMEUI_CREATEMULTIPLAYERGAMEBOTPAGE_H
#define GAMEUI_CREATEMULTIPLAYERGAMEBOTPAGE_H

#include <vgui_controls/PropertyPage.h>

class CPanelListPanel;
class CDescription;
class mpcontrol_t;
class CCvarToggleCheckButton;

//-----------------------------------------------------------------------------
// Purpose: advanced bot properties page of the create game server dialog
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameBotPage: public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(CCreateMultiplayerGameBotPage, vgui2::PropertyPage);

public:
	CCreateMultiplayerGameBotPage(vgui2::Panel* parent, const char* name, KeyValues* botKeys);
	~CCreateMultiplayerGameBotPage();

	void UpdateKeys(KeyValues* botKeys);

protected:
	virtual void OnResetChanges();
	virtual void OnApplyChanges();

private:
	CCvarToggleCheckButton* m_joinAfterPlayer = nullptr;

	CCvarToggleCheckButton* m_allowRogues = nullptr;

	CCvarToggleCheckButton* m_allowPistols = nullptr;
	CCvarToggleCheckButton* m_allowShotguns = nullptr;
	CCvarToggleCheckButton* m_allowSubmachineGuns = nullptr;
	CCvarToggleCheckButton* m_allowMachineGuns = nullptr;
	CCvarToggleCheckButton* m_allowRifles = nullptr;
	CCvarToggleCheckButton* m_allowGrenades = nullptr;
#ifdef CS_SHIELD_ENABLED
	CCvarToggleCheckButton* m_allowShields= nullptr;
#endif // CS_SHIELD_ENABLED
	CCvarToggleCheckButton* m_allowSnipers= nullptr;
							
	CCvarToggleCheckButton* m_deferToHuman= nullptr;

	vgui2::ComboBox* m_joinTeamCombo = nullptr;
	void SetJoinTeamCombo(const char* team);

	vgui2::ComboBox* m_chatterCombo = nullptr;
	void SetChatterCombo(const char* team);

	vgui2::TextEntry* m_quotaEntry = nullptr;

	vgui2::TextEntry* m_prefixEntry = nullptr;
};

#endif // GAMEUI_CREATEMULTIPLAYERGAMEBOTPAGE_H
