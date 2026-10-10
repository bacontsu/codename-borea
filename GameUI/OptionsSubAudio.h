//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_AUDIO_H
#define OPTIONS_SUB_AUDIO_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/PropertyPage.h"

class CLabeledCommandComboBox;
class CCvarSlider;
class CCvarToggleCheckButton;

//-----------------------------------------------------------------------------
// Purpose: Audio Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubAudio: public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubAudio, vgui2::PropertyPage);

public:
	COptionsSubAudio(vgui2::Panel* parent);
	~COptionsSubAudio();

	virtual void OnResetData();
	virtual void OnApplyChanges();
	virtual void OnCommand(const char* command);
	bool RequiresRestart();

private:
	MESSAGE_FUNC(OnControlModified, "ControlModified");
	MESSAGE_FUNC(OnTextChanged, "TextChanged")
	{
		OnControlModified();
	}

	MESSAGE_FUNC(RunTestSpeakers, "RunTestSpeakers");

	vgui2::ComboBox* m_pSoundQualityCombo = nullptr;
	CCvarSlider* m_pSFXSlider = nullptr;
	CCvarSlider* m_pMusicSlider = nullptr;
	CCvarSlider* m_pVideoVol_Slider = nullptr;
	vgui2::ComboBox* m_pSubtitlesCombo = nullptr;
	vgui2::ComboBox* m_pSubtitlesLangCombo = nullptr;
	vgui2::ComboBox* m_pTitlesLangCombo = nullptr;
	bool m_bRequireRestart = false;
	MESSAGE_FUNC(OpenThirdPartySoundCreditsDialog, "OpenThirdPartySoundCreditsDialog");
	vgui2::DHANDLE<class COptionsSubAudioThirdPartyCreditsDlg> m_OptionsSubAudioThirdPartyCreditsDlg;
	//ELanguage m_nCurrentAudioLanguage; we dont plan on adding different languages
};



#endif // OPTIONS_SUB_AUDIO_H