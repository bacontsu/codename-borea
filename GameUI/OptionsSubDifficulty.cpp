//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "OptionsSubDifficulty.h"
#include "tier1/convar.h"
#include "EngineInterface.h"
#include "tier1/KeyValues.h"

#include "vgui_controls/RadioButton.h"

using namespace vgui2;


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubDifficulty::COptionsSubDifficulty(vgui2::Panel* parent): BaseClass(parent, NULL)
{
	m_pEasyRadio = new RadioButton(this, "Skill1Radio", "#GameUI_SkillEasy");
	m_pNormalRadio = new RadioButton(this, "Skill2Radio", "#GameUI_SkillNormal");
	m_pHardRadio = new RadioButton(this, "Skill3Radio", "#GameUI_SkillHard");

	OnResetData();

	LoadControlSettings("Resource/OptionsSubDifficulty.res");


}

//-----------------------------------------------------------------------------
// Purpose: resets controls
//-----------------------------------------------------------------------------
void COptionsSubDifficulty::OnResetData()
{
	int skill = engine->pfnGetCvarFloat("skill");

	if (skill == 1)
	{
		m_pEasyRadio->SetSelected(true);
	}
	else if (skill == 3)
	{
		m_pHardRadio->SetSelected(true);
	}
	else
	{
		m_pNormalRadio->SetSelected(true);
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets data based on control settings
//-----------------------------------------------------------------------------
void COptionsSubDifficulty::OnApplyChanges()
{
	cvar_t* skill = engine->pfnGetCvarPointer("skill");

	if (m_pEasyRadio->IsSelected())
	{
		skill->value = 1;
	}
	else if (m_pHardRadio->IsSelected())
	{
		skill->value = 3;
	}
	else
	{
		skill->value = 2;
	}

	engine->Cvar_SetValue("skill", skill->value);//bruh
}


//-----------------------------------------------------------------------------
// Purpose: enables apply button on radio buttons being pressed
//-----------------------------------------------------------------------------
void COptionsSubDifficulty::OnRadioButtonChecked()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}