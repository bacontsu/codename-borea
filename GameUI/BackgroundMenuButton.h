//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef BACKGROUNDMENUBUTTON_H
#define BACKGROUNDMENUBUTTON_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Button.h>

//-----------------------------------------------------------------------------
// Purpose: Baseclass for the left and right ingame menus that lay on the background
//-----------------------------------------------------------------------------
class CBackgroundMenuButton: public vgui2::Button
{
public:
	CBackgroundMenuButton(vgui2::Panel* parent, const char* name);
	~CBackgroundMenuButton();

	virtual void OnCommand(const char* command);

protected:
	vgui2::Menu* RecursiveLoadGameMenu(KeyValues* datafile);
	vgui2::Menu* m_pMenu = nullptr;

	virtual void ApplySchemeSettings(vgui2::IScheme* pScheme);

private:
	vgui2::IImage *m_pImage = nullptr, *m_pMouseOverImage = nullptr;
	typedef vgui2::Button BaseClass;
};


#endif // BACKGROUNDMENUBUTTON_H