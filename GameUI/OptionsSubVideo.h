//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef OPTIONS_SUB_VIDEO_H
#define OPTIONS_SUB_VIDEO_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI2.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/PropertyPage.h>
#include "EngineInterface.h"
#include "IGameUIFuncs.h"

class CCvarSlider;

//-----------------------------------------------------------------------------
// Purpose: Video Details, Part of OptionsDialog
//-----------------------------------------------------------------------------
class COptionsSubVideo: public vgui2::PropertyPage
{
	DECLARE_CLASS_SIMPLE(COptionsSubVideo, vgui2::PropertyPage);

public:
	COptionsSubVideo(vgui2::Panel* parent);
	~COptionsSubVideo();

	virtual void OnResetData();
	virtual void OnApplyChanges();
	virtual void PerformLayout();

	virtual bool RequiresRestart();

private:
	void SetCurrentResolutionComboItem();
	void EnableOrDisableWindowedForVR();

	MESSAGE_FUNC(OnDataChanged, "ControlModified");
	MESSAGE_FUNC_PTR_CHARPTR(OnTextChanged, "TextChanged", panel, text);
	MESSAGE_FUNC(OpenAdvanced, "OpenAdvanced");


	void PrepareResolutionList();

	bool BUseHDContent();
	void SetUseHDContent(bool bUse);

	int m_nSelectedMode; // -1 if we are running in a nonstandard mode

	vgui2::ComboBox* m_pMode = nullptr;
	vgui2::ComboBox* m_pWindowed = nullptr;
	vgui2::ComboBox* m_pAspectRatio = nullptr;
	vgui2::Button* m_pAdvanced = nullptr;
	vgui2::CheckButton* m_pHDContent = nullptr;
	CCvarSlider* m_pLightGammaSlider;

	vgui2::DHANDLE<class COptionsSubVideoAdvancedDlg> m_hOptionsSubVideoAdvancedDlg;
	vgui2::DHANDLE<class CGammaDialog> m_hGammaDialog;

	struct CVidSettings
	{
		int w;
		int h;
		int bpp;
		int windowed;
		int hdmodels;
		int addons_folder;
		char renderer[128];
	};

	bool m_bStartWidescreen;

	static CVidSettings m_OrigSettings;
	static CVidSettings m_CurrentSettings;

	enum
	{
		MAX_RENDERERS = 3,
		MAX_ASPECTS = 2,
		MAX_ITEM_NAME = 32,
	};

	char m_pszAspectName[MAX_ASPECTS][MAX_ITEM_NAME];

	bool m_bRequireRestart;
	MESSAGE_FUNC(OpenThirdPartyVideoCreditsDialog, "OpenThirdPartyVideoCreditsDialog");
	vgui2::DHANDLE<class COptionsSubVideoThirdPartyCreditsDlg> m_OptionsSubVideoThirdPartyCreditsDlg;
};



#endif // OPTIONS_SUB_VIDEO_H