//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#include "OptionsSubVideo.h"
#include "cvarslider.h"
#include "EngineInterface.h"
#include "BasePanel.h"
#include "IGameUIFuncs.h"
#include "filesystem.h"
#include "GameUI_Interface.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Frame.h"
#include "vgui_controls/QueryBox.h"
#include "CvarToggleCheckButton.h"
#include "tier1/KeyValues.h"
#include "vgui/IInputInternal.h"
#include "vgui/ILocalize.h"
#include "vgui/ISystem.h"
#include "tier0/icommandline.h"
#include "tier1/convar.h"
#include "ModInfo.h"
#include "vgui_controls/Tooltip.h"

#if defined(USE_SDL)
#include "SDL.h"
#endif
// memdbgon must be the last include file in a .cpp file!!!

#include <windows.h>

#include "gl/GL.h"

#include "tier0/memdbgon.h"

COptionsSubVideo::CVidSettings COptionsSubVideo::m_OrigSettings;
COptionsSubVideo::CVidSettings COptionsSubVideo::m_CurrentSettings;

#undef MessageBox //cunt

bool g_ReloadedGame = false; //dont request save&load more than once in 1 frame. thats not fun


// taken from quake
static const struct
{
	const char* name;
	GLenum minimize, maximize;
} texModes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},								  // box filter, no mipmaps
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},								  // linear filter, no mipmaps
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST}, // no (box) filter
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},	  // bilinear filter
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}, // trilinear filter
};

using namespace vgui2;

//-----------------------------------------------------------------------------
// Purpose: aspect ratio mappings (for normal/widescreen combo)
//-----------------------------------------------------------------------------
struct RatioToAspectMode_t
{
	int anamorphic;
	float aspectRatio;
};
RatioToAspectMode_t g_RatioToAspectModes[] =
	{
		{0, 4.0f / 3.0f},
		{1, 16.0f / 9.0f},
		{2, 16.0f / 10.0f},
		{2, 1.0f},
};

struct AAMode_t
{
	int m_nNumSamples;
	int m_nQualityLevel;
};

//-----------------------------------------------------------------------------
// Purpose: returns the aspect ratio mode number for the given resolution
//-----------------------------------------------------------------------------
int GetScreenAspectMode(int width, int height)
{
	float aspectRatio = (float)width / (float)height;

	// just find the closest ratio
	float closestAspectRatioDist = 99999.0f;
	int closestAnamorphic = 0;
	for (int i = 0; i < ARRAYSIZE(g_RatioToAspectModes); i++)
	{
		float dist = fabs(g_RatioToAspectModes[i].aspectRatio - aspectRatio);
		if (dist < closestAspectRatioDist)
		{
			closestAspectRatioDist = dist;
			closestAnamorphic = g_RatioToAspectModes[i].anamorphic;
		}
	}

	return closestAnamorphic;
}

//-----------------------------------------------------------------------------
// Purpose: returns the string name of the specified resolution mode
//-----------------------------------------------------------------------------
static void GetResolutionName(vmode_t* mode, char* sz, int sizeofsz, int desktopWidth, int desktopHeight)
{
	Q_snprintf(sz, sizeofsz, "%i x %i%s", mode->width, mode->height,
		(mode->width == desktopWidth) && (mode->height == desktopHeight) ? " (native)" : "");
}


//-----------------------------------------------------------------------------
// Purpose: advanced keyboard settings dialog
//-----------------------------------------------------------------------------
class COptionsSubVideoAdvancedDlg: public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(COptionsSubVideoAdvancedDlg, vgui2::Frame);

public:
	COptionsSubVideoAdvancedDlg(vgui2::Panel* parent): BaseClass(parent, "OptionsSubVideoAdvancedDlg")
	{
		SetTitle("#GameUI_VideoAdvanced_Title", true);
		SetSize(260, 200);

		KeyValues* pKeyValues = new KeyValues("config");

		//m_pModelDetail = new ComboBox(this, "ModelDetail", 6, false);
		//m_pModelDetail->AddItem("#gameui_low", NULL);
		//m_pModelDetail->AddItem("#gameui_medium", NULL);
		//m_pModelDetail->AddItem("#gameui_high", NULL);
		//m_pModelDetail->AddItem("!!notimplemented!!", NULL);
		//m_pModelDetail->ActivateItem(0);

		// m_pTextureDetail = new ComboBox(this, "TextureDetail", 6, false);
		//m_pTextureDetail->AddItem("#gameui_low", NULL);
		//m_pTextureDetail->AddItem("#gameui_medium", NULL);
		//m_pTextureDetail->AddItem("#gameui_high", NULL);
		//m_pTextureDetail->AddItem("#gameui_ultra", NULL);
		//m_pTextureDetail->AddItem("!!notimplemented!!", NULL);
		//m_pTextureDetail->ActivateItem(0);

		m_pFilteringMode = new ComboBox(this, "FilteringMode", 6, false);

		m_pFilteringMode->AddItem("#GameUI_GL_Nearest", NULL);
		m_pFilteringMode->AddItem("#GameUI_GL_Linear", NULL);
		m_pFilteringMode->AddItem("#GameUI_GL_Nearest_MipMap_Nearest", NULL);
		m_pFilteringMode->AddItem("#GameUI_GL_Linear_MipMap_Nearest", NULL);
		m_pFilteringMode->AddItem("#GameUI_GL_Nearest_MipMap_Linear", NULL);
		m_pFilteringMode->AddItem("#GameUI_GL_Linear_MipMap_Linear", NULL);

		const char* texturemode = engine->pfnGetCvarString("gl_texturemode");
		for (int i = 0; i < sizeof(texModes) / sizeof(texModes[0]); i++)
		{
			if (stricmp(texModes[i].name, texturemode))
				continue;

			m_pFilteringMode->ActivateItem(i);
			//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texModes[i].maximize); the fuck? dont do this
			break;
		}

		m_pShadowDetail = new ComboBox(this, "ShadowDetail", 6, false);
		m_pShadowDetail->AddItem("#gameui_disabled", NULL);
		m_pShadowDetail->AddItem("#gameui_enabled", NULL);

		if (engine->pfnGetCvarPointer("r_shadowrendertotexture")->value > 0)
			m_pShadowDetail->ActivateItem(1);
		else
			m_pShadowDetail->ActivateItem(0);

		m_pWaterDetail = new ComboBox(this, "WaterDetail", 6, false);
		m_pWaterDetail->AddItem("#gameui_noreflections", NULL);
		m_pWaterDetail->AddItem("#gameui_reflectonlyworld", NULL);
		m_pWaterDetail->AddItem("#gameui_reflectall", NULL);

		int reflectworld = engine->pfnGetCvarPointer("r_waterforceexpensive")->value;
		int reflectentity = engine->pfnGetCvarPointer("r_waterforcereflectentities")->value;

		if (reflectworld && reflectentity)
			m_pWaterDetail->ActivateItem(2);
		else if (reflectworld)
			m_pWaterDetail->ActivateItem(1);
		else
			m_pWaterDetail->ActivateItem(0);


		m_pVSync = new ComboBox(this, "VSync", 2, false);
		m_pVSync->AddItem("#gameui_disabled", NULL);
		m_pVSync->AddItem("#gameui_enabled", NULL);

		m_pVSync->ActivateItem(engine->pfnGetCvarPointer("gl_vsync")->value);

		// m_pShaderDetail = new ComboBox(this, "ShaderDetail", 6, false);
		//m_pShaderDetail->AddItem("#gameui_low", NULL);
		//m_pShaderDetail->AddItem("#gameui_high", NULL);
		// m_pShaderDetail->AddItem("!!notimplemented!!", NULL);
		// m_pShaderDetail->ActivateItem(0);


		m_pBloom = new ComboBox(this, "Bloom", 2, false);
		m_pBloom->AddItem("#gameui_disabled", NULL);
		m_pBloom->AddItem("#gameui_enabled", NULL);

		m_pBloom->ActivateItem(engine->pfnGetCvarPointer("r_bloom")->value);

		m_pBlacknwhiteMode = new ComboBox(this, "Blacknwhite", 2, false);
		m_pBlacknwhiteMode->AddItem("#gameui_disabled", NULL);
		m_pBlacknwhiteMode->AddItem("#gameui_enabled", NULL);

		m_pBlacknwhiteMode->ActivateItem(engine->pfnGetCvarPointer("r_filter_blacknwhite")->value);

		m_pFOVSlider = new CCvarSlider(this, "FovSlider", "FovSlider", 0, 120, "default_fov", false);
		m_pBNW_FilmGrainSlider = new CCvarSlider(this, "BNW_FilmGrainSlider", "BNW_FilmGrainSlider", 0, 10, "r_filter_filmgrain_strength", false);

		LoadControlSettings("resource/OptionsSubVideoAdvancedDlg.res");
		MoveToCenterOfScreen();
		SetSizeable(false);

		m_pFOVValueLabel = (vgui2::Label*)FindChildByName("FovValueLabel");
		char value[64];
		snprintf(value, ARRAYSIZE(value), "%d", m_pFOVSlider->GetValue() / 100);
		m_pFOVValueLabel->SetText(value);

		m_pBNW_FilmGrainValueLabel = (vgui2::Label*)FindChildByName("BNW_FilmGrainValueLabel");
		snprintf(value, ARRAYSIZE(value), "%d", m_pBNW_FilmGrainSlider->GetValue() / 100);
		m_pBNW_FilmGrainValueLabel->SetText(value);

		if (engine->pfnGetCvarFloat("default_fov") == NULL) //was fov_desired
		{
			Panel* pFOV = FindChildByName("FovSlider");
			if (pFOV)
			{
				pFOV->SetVisible(false);
			}

			pFOV = FindChildByName("FovLabel");
			if (pFOV)
			{
				pFOV->SetVisible(false);
			}

			pFOV = FindChildByName("FovValueLabel");
			if (pFOV)
			{
				pFOV->SetVisible(false);
			}
		}

		MarkDefaultSettingsAsRecommended();

		m_bUseChanges = false;
	}

	virtual void Activate()
	{
		BaseClass::Activate();

		input()->SetAppModalSurface(GetVPanel());

		if (!m_bUseChanges)
		{
			// reset the data
			OnResetData();
		}
	}

	void SetComboItemAsRecommended(vgui2::ComboBox* combo, int iItem)
	{
		// get the item text
		wchar_t text[512];
		combo->GetItemText(iItem, text, sizeof(text));

		// append the recommended flag
		wchar_t newText[512];
		_snwprintf(newText, sizeof(newText) / sizeof(wchar_t), L"%ls *", text);

		// reset
		combo->UpdateItem(iItem, newText, NULL);
	}

	int FindMSAAMode(int nAASamples, int nAAQuality)
	{

		return 0; // Didn't find what we're looking for, so no AA
	}

	MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel)
	{
		//if (RequiresRestart())
		//{
		//	// notify the user that this will require a disconnect
		//	QueryBox* box = new QueryBox("#GameUI_SettingRequiresDisconnect_Title", "#GameUI_SettingRequiresDisconnect_Info", this);
		//	box->AddActionSignalTarget(this);
		//	box->SetCancelCommand(new KeyValues("ResetDXLevelCombo"));
		//	box->DoModal();
		//}
	}

	MESSAGE_FUNC_PTR(OnControlModified, "ControlModified", panel)
	{
		if (panel == m_pFOVSlider)
		{
			char value[64];
			snprintf(value, ARRAYSIZE(value), "%d", m_pFOVSlider->GetValue() / 100);
			m_pFOVValueLabel->SetText(value);
		}
		if (panel == m_pBNW_FilmGrainSlider)
		{
			char value[64];
			snprintf(value, ARRAYSIZE(value), "%d", m_pBNW_FilmGrainSlider->GetValue() / 100);
			m_pBNW_FilmGrainValueLabel->SetText(value);
		}
	}

	MESSAGE_FUNC(OnGameUIHidden, "GameUIHidden") // called when the GameUI is hidden
	{
		Close();
	}

	MESSAGE_FUNC(ResetDXLevelCombo, "ResetDXLevelCombo")
	{

	}

	MESSAGE_FUNC(OK_Confirmed, "OK_Confirmed")
	{
		m_bUseChanges = true;
		Close();
	}

	void MarkDefaultSettingsAsRecommended()
	{
	}

	void ApplyChangesToConVar(const char* pConVarName, int value)
	{
		//Assert(cvar->FindVar(pConVarName));
		char szCmd[256];
		Q_snprintf(szCmd, sizeof(szCmd), "%s %d\n", pConVarName, value);
		engine->pfnClientCmd(szCmd);
	}

	virtual void ApplyChanges()
	{
		if (!m_bUseChanges)
			return;

		//ApplyChangesToConVar("r_rootlod", 2 - m_pModelDetail->GetActiveItem());
		//ApplyChangesToConVar("mat_picmip", 2 - m_pTextureDetail->GetActiveItem());

		// reset everything tied to the filtering mode, then the switch sets the appropriate one

		if (m_pShadowDetail->GetActiveItem() == 0)
			ApplyChangesToConVar("r_shadowrendertotexture", 0);	 // Turn shadows
		else 
			ApplyChangesToConVar("r_shadowrendertotexture", 1);	

		// ApplyChangesToConVar("mat_reducefillrate", (m_pShaderDetail->GetActiveItem() > 0) ? 0 : 1);

		switch (m_pWaterDetail->GetActiveItem())
		{
		default:
		case 0:
			ApplyChangesToConVar("r_waterforceexpensive", false);
			ApplyChangesToConVar("r_waterforcereflectentities", false);
			break;
		case 1:
			ApplyChangesToConVar("r_waterforceexpensive", true);
			ApplyChangesToConVar("r_waterforcereflectentities", false);
			break;
		case 2:
			ApplyChangesToConVar("r_waterforceexpensive", true);
			ApplyChangesToConVar("r_waterforcereflectentities", true);
			break;
		}

		ApplyChangesToConVar("gl_vsync", m_pVSync->GetActiveItem());

		ApplyChangesToConVar("r_bloom", m_pBloom->GetActiveItem());

		ApplyChangesToConVar("r_filter_blacknwhite", m_pBlacknwhiteMode->GetActiveItem());


		CCvarSlider* pFOV = (CCvarSlider*)FindChildByName("FOVSlider");
		if (pFOV)
		{
			pFOV->ApplyChanges();
		}

		m_pBNW_FilmGrainSlider->ApplyChanges();


		const char* texturemode = texModes[m_pFilteringMode->GetActiveItem()].name;
		const char* curtexmode = engine->pfnGetCvarString("gl_texturemode");
		if (stricmp(curtexmode, texturemode))
		{
			engine->Cvar_Set("gl_texturemode", const_cast<char*>(texturemode));
			auto pszLevelName = engine->pfnGetLevelName();

			if ((pszLevelName && *pszLevelName) && (!g_ReloadedGame))
			{
				// restart level
				engine->pfnClientCmd("save quick");
				engine->pfnClientCmd("load quick");
				g_ReloadedGame = true;
			}
		}
	}

	virtual void OnResetData()
	{
	}

	virtual void OnCommand(const char* command)
	{
		if (!stricmp(command, "OK"))
		{
			if (RequiresRestart())
			{
				// Bring up the confirmation dialog
				QueryBox* box = new QueryBox("#GameUI_SettingRequiresDisconnect_Title", "#GameUI_SettingRequiresDisconnect_Info");
				box->AddActionSignalTarget(this);
				box->SetOKCommand(new KeyValues("OK_Confirmed"));
				box->SetCancelCommand(new KeyValues("ResetDXLevelCombo"));
				box->DoModal();
				box->MoveToFront();
				return;
			}

			m_bUseChanges = true;
			Close();
		}
		else
		{
			BaseClass::OnCommand(command);
		}
	}

	void OnKeyCodeTyped(KeyCode code)
	{
		// force ourselves to be closed if the escape key it pressed
		if (code == KEY_ESCAPE)
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodeTyped(code);
		}
	}

	bool RequiresRestart()
	{
		return false;
	}

private:
	bool m_bUseChanges;
	vgui2::ComboBox *m_pFilteringMode;
	vgui2::ComboBox *m_pShadowDetail, *m_pWaterDetail, *m_pVSync;
	vgui2::ComboBox* m_pBloom;
	vgui2::ComboBox* m_pBlacknwhiteMode;
	CCvarSlider *m_pFOVSlider;
	CCvarSlider* m_pBNW_FilmGrainSlider;
	vgui2::Label *m_pFOVValueLabel;
	vgui2::Label* m_pBNW_FilmGrainValueLabel;
};

#if defined(USE_SDL)

//-----------------------------------------------------------------------------
// Purpose: Get display index we will go fullscreen on.
//-----------------------------------------------------------------------------
static int getSDLDisplayIndex()
{
	static ConVarRef sdl_displayindex("sdl_displayindex");

	Assert(sdl_displayindex.IsValid());
	return sdl_displayindex.IsValid() ? sdl_displayindex.GetInt() : 0;
}

//-----------------------------------------------------------------------------
// Purpose: Get display index we are currently fullscreen on. (or -1 if none).
//-----------------------------------------------------------------------------
static int getSDLDisplayIndexFullscreen()
{
	static ConVarRef sdl_displayindex_fullscreen("sdl_displayindex_fullscreen");

	Assert(sdl_displayindex_fullscreen.IsValid());
	return sdl_displayindex_fullscreen.IsValid() ? sdl_displayindex_fullscreen.GetInt() : -1;
}

#endif // USE_SDL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubVideo::COptionsSubVideo(vgui2::Panel* parent): PropertyPage(parent, NULL)
{
	m_bRequireRestart = false;

	if (!m_OrigSettings.w && !m_OrigSettings.h)
	{

		gameuifuncs->GetCurrentVideoMode(
			&m_OrigSettings.w,
			&m_OrigSettings.h,
			&m_OrigSettings.bpp);

		int vid_level = 0;

#ifdef HL25_UPDATE
		gameuifuncs->GetCurrentRenderer(
			m_OrigSettings.renderer,
			ARRAYSIZE(m_OrigSettings.renderer),
			&m_OrigSettings.windowed,
			&m_OrigSettings.hdmodels,
			&m_OrigSettings.addons_folder);
#else
		gameuifuncs->GetCurrentRenderer(
			m_OrigSettings.renderer,
			ARRAYSIZE(m_OrigSettings.renderer),
			&m_OrigSettings.windowed,
			&m_OrigSettings.hdmodels,
			&m_OrigSettings.addons_folder, &vid_level);
#endif

		memcpy(&m_CurrentSettings, &m_OrigSettings, sizeof(m_OrigSettings));
	}

	m_pMode = new ComboBox(this, "Resolution", 8, false);
	m_pAspectRatio = new ComboBox(this, "AspectRatio", 2, false);
	m_pAdvanced = new Button(this, "AdvancedButton", "#GameUI_AdvancedEllipsis");
	m_pAdvanced->SetCommand(new KeyValues("OpenAdvanced"));
	//	m_pThirdPartyCredits->SetCommand(new KeyValues("OpenThirdPartyVideoCreditsDialog"));
	m_pHDContent = new CheckButton(this, "HDContentButton", "#GameUI_HDContent");

	m_pWindowed = new vgui2::ComboBox(this, "DisplayModeCombo", 6, false);

	m_pLightGammaSlider = new CCvarSlider(this, "LightGammaSlider", "LightGammaSlider", 1.8, 3.0, "lightgamma", false);

	m_pWindowed->AddItem("#GameUI_Fullscreen", NULL);
	m_pWindowed->AddItem("#GameUI_Windowed", NULL);

	m_pWindowed->ActivateItem(m_CurrentSettings.windowed != 0);

	LoadControlSettings("Resource\\OptionsSubVideo.res");

	vgui2::localize()->ConvertUnicodeToANSI(
		vgui2::localize()->Find("#GameUI_AspectNormal"),
		m_pszAspectName[0],
		ARRAYSIZE(m_pszAspectName[0]));

	vgui2::localize()->ConvertUnicodeToANSI(
		vgui2::localize()->Find("#GameUI_AspectWide"),
		m_pszAspectName[1],
		ARRAYSIZE(m_pszAspectName[1]));

	const auto iAspectNormal = m_pAspectRatio->AddItem(m_pszAspectName[0], nullptr);
	const auto iAspectWide = m_pAspectRatio->AddItem(m_pszAspectName[1], nullptr);

	// Determine which aspect is in use right now
	if (3 * m_CurrentSettings.w + -4 * m_CurrentSettings.h)
	{
		m_bStartWidescreen = true;
		m_pAspectRatio->ActivateItem(iAspectWide);
	}
	else
	{
		m_bStartWidescreen = false;
		m_pAspectRatio->ActivateItem(iAspectNormal);
	}

	// Moved down here so we can set the Drop down's
	// menu state after the default (disabled) value is loaded
	PrepareResolutionList();
}

//-----------------------------------------------------------------------------
// Purpose: Generates resolution list
//-----------------------------------------------------------------------------
void COptionsSubVideo::PrepareResolutionList()
{
	vmode_t* plist = nullptr;
	int count = 0;
	gameuifuncs->GetVideoModes(&plist, &count);

	m_pMode->DeleteAllItems();

	int nItemsAdded = 0;
	int selectedItemID = -1;
	bool bFoundWidescreen = false;
	char sz[256];

	for (int i = 0; i < count; ++i, ++plist)
	{
		snprintf(sz, ARRAYSIZE(sz), "%i x %i", plist->width, plist->height);

		int itemID = -1;

		const bool bIsWideScreen = (3 * plist->width + -4 * plist->height) != 0;
		int iAspectMode = GetScreenAspectMode(plist->width, plist->height);

		if (iAspectMode > 0)
		{
			m_pAspectRatio->SetItemEnabled(iAspectMode, true);
			bFoundWidescreen = true;
		}

		// filter the list for those matching the current aspect
		if (iAspectMode == m_pAspectRatio->GetActiveItem())
		{
			itemID = m_pMode->AddItem(sz, nullptr);
			++nItemsAdded;
		}

		// try and find the best match for the resolution to be selected
		if (plist->width == m_CurrentSettings.w && plist->height == m_CurrentSettings.h)
		{
			selectedItemID = itemID;
		}
		else if (selectedItemID == -1 && plist->width == m_CurrentSettings.w && plist->height == m_CurrentSettings.h)
		{
			selectedItemID = itemID;
		}
	}

	m_pAspectRatio->SetEnabled(bFoundWidescreen);

	if (selectedItemID != -1)
	{
		m_pMode->ActivateItem(selectedItemID);
	}

	// If no items could be added, the current system probably doesn't support the resolutions at this aspect.
	// Switch to the next aspect ratio and try again.
	// TODO: this could end up in an infinite loop if no resolutions work - Solokiller
	if (!nItemsAdded)
	{
		if (count)
		{
			m_pAspectRatio->ActivateItem((m_pAspectRatio->GetActiveItem() + 1) % m_pAspectRatio->GetItemCount());
			PrepareResolutionList();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
COptionsSubVideo::~COptionsSubVideo()
{
	if (m_hOptionsSubVideoAdvancedDlg.Get())
	{
		m_hOptionsSubVideoAdvancedDlg->MarkForDeletion();
	}
}


FILE* FOpenGameHDFile(const char* pchMode)
{
	const char* pGameDir = engine->pfnGetGameDirectory();
	char szModSteamInfPath[1024];
	V_ComposeFileName(pGameDir, "game_hd.txt", szModSteamInfPath, sizeof(szModSteamInfPath));

	FILE* fp = fopen(szModSteamInfPath, pchMode);
	return fp;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool COptionsSubVideo::BUseHDContent()
{
	FILE* fp = FOpenGameHDFile("rb");
	if (fp)
	{
		fclose(fp);
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Purpose: hint the engine to load HD content if possible, logic must match with engine/common.cpp BLoadHDContent
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetUseHDContent(bool bUse)
{
	if (bUse)
	{
		FILE* fp = FOpenGameHDFile("wb+");
		if (fp)
		{
			fprintf(fp, "If this file exists on disk HD content will be loaded.\n");
			fclose(fp);
		}
	}
	else
	{
		const char* pGameDir = engine->pfnGetGameDirectory();
		char szModSteamInfPath[1024];
		V_ComposeFileName(pGameDir, "game_hd.txt", szModSteamInfPath, sizeof(szModSteamInfPath));
		_unlink(szModSteamInfPath);
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnResetData()
{
	m_CurrentSettings = m_OrigSettings;

	SetCurrentResolutionComboItem();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::SetCurrentResolutionComboItem()
{
	vmode_t* plist = nullptr;
	int count = 0;
	gameuifuncs->GetVideoModes(&plist, &count);

	for (int i = 0; i < count; ++i, ++plist)
	{
		if (plist->width == m_CurrentSettings.w && plist->height == m_CurrentSettings.h)
		{
			char sz[256];
			snprintf(sz, ARRAYSIZE(sz), "%i x %i", m_CurrentSettings.w, m_CurrentSettings.h);
			m_pMode->SetText(sz);
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: restarts the game
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnApplyChanges()
{

	// apply advanced options
	if (m_hOptionsSubVideoAdvancedDlg.Get())
	{
		m_hOptionsSubVideoAdvancedDlg->ApplyChanges();
	}

	m_pLightGammaSlider->ApplyChanges();

	// resolution
	char sz[256];
	if (m_nSelectedMode == -1)
	{
		m_pMode->GetText(sz, 256);
	}
	else
	{
		m_pMode->GetItemText(m_nSelectedMode, sz, 256);
	}

	if (m_pMode)
	{
		char sz[256];
		m_pMode->GetText(sz, ARRAYSIZE(sz));

		int w, h;
		sscanf(sz, "%i x %i", &w, &h);

		m_CurrentSettings.w = w;
		m_CurrentSettings.h = h;
		m_CurrentSettings.bpp = 32;
		m_CurrentSettings.windowed = m_pWindowed->GetActiveItem();

		if (memcmp(&m_OrigSettings, &m_CurrentSettings, sizeof(m_OrigSettings)))
		{
			//_setvideomode sends the new video config data to windows registry
			// 
			//from what i understand if we were to port this mod to linux-
			//- we would just need to access hl.conf for all this info
			memcpy(&m_OrigSettings, &m_CurrentSettings, sizeof(m_OrigSettings));
			char szCmd[256];

			CVidSettings* p = &m_CurrentSettings;
			sprintf(
				szCmd,
				"_setvideomode %i %i %i\n",
				p->w,
				p->h,
				p->bpp);

			engine->pfnClientCmd(szCmd);

			sprintf(szCmd, "_setrenderer gl %s", m_CurrentSettings.windowed ? "windowed" : "fullscreen");

			engine->pfnClientCmd(szCmd);

			MessageBox* box = new MessageBox("#GameUI_SettingRequiresRestart_Title", "#GameUI_SettingRequiresRestart_Info", this);
			box->AddActionSignalTarget(this);
			//box->SetCancelCommand(new KeyValues("ResetDXLevelCombo"));
			box->DoModal();

			//todo: show box that notifies user to restart game himself

			//engine->pfnClientCmd("_restart\n");
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubVideo::PerformLayout()
{
	BaseClass::PerformLayout();

	//if (m_pGammaButton)
	//{
		//const MaterialSystem_Config_t& config = materials->GetCurrentConfigForVideoCard();
		//m_pGammaButton->SetEnabled(!config.Windowed());
	//}
}

//-----------------------------------------------------------------------------
// Purpose: enables apply button on data changing
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnTextChanged(Panel* pPanel, const char* pszText)
{
	if (pPanel == m_pMode)
	{

		m_nSelectedMode = m_pMode->GetActiveItem();

		int w = 0, h = 0;
		sscanf(pszText, "%i x %i", &w, &h);

		if(w != m_CurrentSettings.w || h != m_CurrentSettings.h)
			OnDataChanged();

	}
	else if (pPanel == m_pAspectRatio)
	{
		PrepareResolutionList();
	}
	else if (pPanel == m_pWindowed)
	{
		PrepareResolutionList();
		OnDataChanged();
	}
}


//-----------------------------------------------------------------------------
// Purpose: enables windowed combo box
//-----------------------------------------------------------------------------
void COptionsSubVideo::EnableOrDisableWindowedForVR()
{
}


//-----------------------------------------------------------------------------
// Purpose: enables apply button
//-----------------------------------------------------------------------------
void COptionsSubVideo::OnDataChanged()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if the changes requires a restart to take effect
//-----------------------------------------------------------------------------
bool COptionsSubVideo::RequiresRestart()
{
	if (m_hOptionsSubVideoAdvancedDlg.Get() && m_hOptionsSubVideoAdvancedDlg->RequiresRestart())
	{
		return true;
	}

	// make sure there is a change
	return m_bRequireRestart;
}

//-----------------------------------------------------------------------------
// Purpose: Opens advanced video mode options dialog
//-----------------------------------------------------------------------------
void COptionsSubVideo::OpenAdvanced()
{
	if (!m_hOptionsSubVideoAdvancedDlg.Get())
	{
		m_hOptionsSubVideoAdvancedDlg = new COptionsSubVideoAdvancedDlg(FindChildByName("OptionsDialog")); // we'll parent this to the OptionsDialog directly
	}

	m_hOptionsSubVideoAdvancedDlg->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: third-party audio credits dialog
//-----------------------------------------------------------------------------
class COptionsSubVideoThirdPartyCreditsDlg: public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(COptionsSubVideoThirdPartyCreditsDlg, vgui2::Frame);

public:
	COptionsSubVideoThirdPartyCreditsDlg(vgui2::VPANEL hParent): BaseClass(NULL, NULL)
	{
		// parent is ignored, since we want look like we're steal focus from the parent (we'll become modal below)
		int w = 500;
		int h = 200;
		if (ipanel()->IsProportional(hParent))
		{
			SetProportional(true);
			w = scheme()->GetProportionalScaledValueEx(GetScheme(), w);
			h = scheme()->GetProportionalScaledValueEx(GetScheme(), h);
		}

		SetTitle("#GameUI_ThirdPartyVideo_Title", true);
		SetSize(w, h);
		LoadControlSettings("resource/OptionsSubVideoThirdPartyDlg.res");
		MoveToCenterOfScreen();
		SetSizeable(false);
		SetDeleteSelfOnClose(true);
	}

	virtual void Activate()
	{
		BaseClass::Activate();

		input()->SetAppModalSurface(GetVPanel());
	}

	void OnKeyCodeTyped(KeyCode code)
	{
		// force ourselves to be closed if the escape key it pressed
		if (code == KEY_ESCAPE)
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodeTyped(code);
		}
	}
};


//-----------------------------------------------------------------------------
// Purpose: Open third party audio credits dialog
//-----------------------------------------------------------------------------
void COptionsSubVideo::OpenThirdPartyVideoCreditsDialog()
{
	if (!m_OptionsSubVideoThirdPartyCreditsDlg.Get())
	{
		m_OptionsSubVideoThirdPartyCreditsDlg = new COptionsSubVideoThirdPartyCreditsDlg(GetVParent());
	}
	m_OptionsSubVideoThirdPartyCreditsDlg->Activate();
}