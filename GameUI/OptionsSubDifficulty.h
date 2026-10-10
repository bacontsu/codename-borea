//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_DIFFICULTY_H
#define OPTIONS_SUB_DIFFICULTY_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PropertyPage.h"

//-----------------------------------------------------------------------------
// Purpose: Difficulty selection options
//-----------------------------------------------------------------------------
class COptionsSubDifficulty: public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubDifficulty, vgui2::PropertyPage);

public:
	COptionsSubDifficulty(vgui2::Panel* parent);

	virtual void OnResetData();
	virtual void OnApplyChanges();

	MESSAGE_FUNC(OnRadioButtonChecked, "RadioButtonChecked");

private:
	vgui2::RadioButton* m_pEasyRadio = nullptr;
	vgui2::RadioButton* m_pNormalRadio = nullptr;
	vgui2::RadioButton* m_pHardRadio = nullptr;
};


#endif // OPTIONS_SUB_DIFFICULTY_H