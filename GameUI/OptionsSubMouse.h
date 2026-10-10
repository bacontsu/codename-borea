//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_MOUSE_H
#define OPTIONS_SUB_MOUSE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/PropertyPage.h>

class CCvarNegateCheckButton;
class CKeyToggleCheckButton;
class CCvarToggleCheckButton;
class CCvarSlider;

namespace vgui2
{
class Label;
class Panel;
}

//-----------------------------------------------------------------------------
// Purpose: Mouse Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubMouse: public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubMouse, vgui2::PropertyPage);

public:
	COptionsSubMouse(vgui2::Panel* parent);
	~COptionsSubMouse();

	virtual void OnResetData();
	virtual void OnApplyChanges();

protected:
	virtual void ApplySchemeSettings(vgui2::IScheme* pScheme);

	MESSAGE_FUNC_PTR(OnControlModified, "ControlModified", panel);
	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
	MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel)
	{
		OnControlModified(panel);
	}

	void UpdateSensitivityLabel();
	void UpdateAccelerationLabel();
	void UpdateJoystickPanels();

private:
	CCvarNegateCheckButton* m_pReverseMouseCheckBox = nullptr;
	CCvarToggleCheckButton* m_pMouseFilterCheckBox = nullptr;
	CCvarToggleCheckButton* m_pMouseRawCheckBox = nullptr;
	vgui2::CheckButton* m_pMouseAccelerationCheckBox = nullptr;

	CCvarToggleCheckButton* m_pJoystickCheckBox = nullptr;
	CCvarToggleCheckButton* m_pJoystickSouthpawCheckBox = nullptr;
	CCvarToggleCheckButton* m_pQuickInfoCheckBox = nullptr;
	CCvarToggleCheckButton* m_pReverseJoystickCheckBox = nullptr;

	CCvarSlider* m_pMouseSensitivitySlider = nullptr;
	vgui2::TextEntry* m_pMouseSensitivityLabel = nullptr;

	CCvarSlider* m_pMouseAccelExponentSlider = nullptr;
	vgui2::TextEntry* m_pMouseAccelExponentLabel = nullptr;

	CCvarSlider* m_pJoyYawSensitivitySlider = nullptr;
	vgui2::Label* m_pJoyYawSensitivityPreLabel = nullptr;
	CCvarSlider* m_pJoyPitchSensitivitySlider = nullptr;
	vgui2::Label* m_pJoyPitchSensitivityPreLabel = nullptr;

	CCvarToggleCheckButton* m_pAutoAimCheckBox = nullptr;
};



#endif // OPTIONS_SUB_MOUSE_H