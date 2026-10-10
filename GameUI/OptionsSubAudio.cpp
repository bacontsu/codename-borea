//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "OptionsSubAudio.h"

#include "cvarslider.h"
#include "EngineInterface.h"
#include "ModInfo.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/QueryBox.h"
#include "CvarToggleCheckButton.h"
#include "tier1/KeyValues.h"
#include "tier1/convar.h"
#include <vgui/IInputInternal.h>
#include <FileSystem.h>
#include "filesystem_helpers.h"
#include <cstdio>
#include <steam/steam_api.h>
#include <tier1/strtools.h>
#include <string>
#include <vector>


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui2;

enum SoundQuality_e
{
	SOUNDQUALITY_LOW,
	SOUNDQUALITY_HIGH,
};

struct lang_map_t
{
	char language_name[64];
	char language_cvarname[64];
	char font_cvarname[64];
};

static std::vector<lang_map_t> s_subtitlelanguages;
static std::vector<lang_map_t> s_titlelanguages;

extern bool g_ReloadedGame;

static void InitLanguages()
{
	static bool initiated = false;
	if (initiated)
		return;

	initiated = true;
	auto hFile = vgui2::filesystem()->Open("resource/subtitles_langcfg.txt", "rt");
	if (!hFile)
		return;

	uint32_t filesize = vgui2::filesystem()->Size(hFile);

	char* bytes = (char*)malloc(filesize);

	vgui2::filesystem()->Read(bytes, filesize, hFile);

	vgui2::filesystem()->Close(hFile);

	const char* mem = bytes;
	while (true)
	{
		char languagename[512];
		char languagecvarname[512];
		char fontcvarname[512];
		mem = ParseFile(mem, languagename, nullptr);
		mem = ParseFile(mem, languagecvarname, nullptr);
		mem = ParseFile(mem, fontcvarname, nullptr);

		if (!mem || (mem - bytes) > filesize)
			break;

		lang_map_t langmap;
		Q_strcpy(langmap.language_name, languagename);
		Q_strcpy(langmap.language_cvarname, languagecvarname);
		Q_strcpy(langmap.font_cvarname, fontcvarname);

		s_subtitlelanguages.push_back(langmap);
	}

	free(bytes);


	//titles.txt

	hFile = vgui2::filesystem()->Open("resource/titles_langcfg.txt", "rt");
	if (!hFile)
		return;

	filesize = vgui2::filesystem()->Size(hFile);

	bytes = (char*)malloc(filesize);

	vgui2::filesystem()->Read(bytes, filesize, hFile);

	vgui2::filesystem()->Close(hFile);

	mem = bytes;
	while (true)
	{
		char languagename[512];
		char languagecvarname[512];
		char fontcvarname[512];
		mem = ParseFile(mem, languagename, nullptr);
		mem = ParseFile(mem, languagecvarname, nullptr);
		mem = ParseFile(mem, fontcvarname, nullptr);

		if (!mem || (mem - bytes) > filesize)
			break;

		lang_map_t langmap;
		Q_strcpy(langmap.language_name, languagename);
		Q_strcpy(langmap.language_cvarname, languagecvarname);
		Q_strcpy(langmap.font_cvarname, fontcvarname);

		s_titlelanguages.push_back(langmap);
	}

	free(bytes);
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
COptionsSubAudio::COptionsSubAudio(vgui2::Panel* parent): PropertyPage(parent, NULL)
{
	InitLanguages();

	m_pSFXSlider = new CCvarSlider(this, "SFXSlider", "#GameUI_SoundEffectVolume", 0.0f, 1.0f, "volume", false);
	m_pMusicSlider = new CCvarSlider(this, "MusicSlider", "#GameUI_MusicVolume", 0.0f, 1.0f, "MP3Volume", false);
	m_pVideoVol_Slider = new CCvarSlider(this, "VideoVol_Slider", "#GameUI_VideoVolume", 0.0f, 100.0f, "ffmpeg_soundvolume", false);

	m_pSubtitlesCombo = new ComboBox(this, "CloseCaptionCheck", 6, false);
	m_pSubtitlesCombo->AddItem("#GameUI_Disabled", NULL);
	m_pSubtitlesCombo->AddItem("#GameUI_Enabled", NULL);

	m_pSubtitlesCombo->ActivateItem(engine->pfnGetCvarFloat("r_subtitles") ? 1 : 0);

	m_pSubtitlesLangCombo = new ComboBox(this, "CloseCaptionLang", 6, false);
	cvar_t* r_subtitleslang = engine->pfnGetCvarPointer("r_subtitleslang");
	for (int i = 0; i < s_subtitlelanguages.size(); i++)
	{
		m_pSubtitlesLangCombo->AddItem(s_subtitlelanguages[i].language_name, NULL);
		if (r_subtitleslang->string)
			if (!strcmp(s_subtitlelanguages[i].language_cvarname, r_subtitleslang->string))
				m_pSubtitlesLangCombo->ActivateItem(i);
	}
	if (r_subtitleslang->string)
	{
		for (int i = 0; i < s_subtitlelanguages.size(); i++)
		{
			if (!strcmp(s_subtitlelanguages[i].language_cvarname, r_subtitleslang->string))
				m_pSubtitlesLangCombo->ActivateItem(i);
		}
	}

	m_pTitlesLangCombo = new ComboBox(this, "TitlesLang", 6, false);
	cvar_t* r_titleslang = engine->pfnGetCvarPointer("r_titleslang");
	for (int i = 0; i < s_titlelanguages.size(); i++)
	{
		m_pTitlesLangCombo->AddItem(s_titlelanguages[i].language_name, NULL);
		if (r_titleslang->string)
			if (!strcmp(s_titlelanguages[i].language_cvarname, r_titleslang->string))
				m_pTitlesLangCombo->ActivateItem(i);
	}
	if (r_titleslang->string)
	{
		for (int i = 0; i < s_titlelanguages.size(); i++)
		{
			if (!strcmp(s_titlelanguages[i].language_cvarname, r_titleslang->string))
				m_pTitlesLangCombo->ActivateItem(i);
		}
	}


	m_pSoundQualityCombo = new ComboBox(this, "SoundQuality", 6, false);
	m_pSoundQualityCombo->AddItem("#GameUI_Low", new KeyValues("SoundQuality", "quality", SOUNDQUALITY_LOW));
	m_pSoundQualityCombo->AddItem("#GameUI_High", new KeyValues("SoundQuality", "quality", SOUNDQUALITY_HIGH));

	m_pSoundQualityCombo->ActivateItem(engine->pfnGetCvarFloat("hisound") ? 1 : 0);

	LoadControlSettings("Resource\\OptionsSubAudio.res");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
COptionsSubAudio::~COptionsSubAudio()
{
}

//-----------------------------------------------------------------------------
// Purpose: Reloads data
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnResetData()
{
	m_bRequireRestart = false;
	m_pSFXSlider->Reset();
	m_pMusicSlider->Reset();
	m_pVideoVol_Slider->Reset();

	// reset the combo boxes

	// close captions
	if (engine->pfnGetCvarPointer("r_subtitles")->value)
		m_pSubtitlesCombo->ActivateItem(1);
	else
		m_pSubtitlesCombo->ActivateItem(0);
	/*

	// speakers
	ConVarRef snd_surround_speakers("Snd_Surround_Speakers");
	int speakers = snd_surround_speakers.GetInt();

#ifdef POSIX
	// On Posix there is no headphone option, so we upgrade to 2 speakers if Snd_Surround_Speakers == 0
	if (speakers == 0)
		speakers = 2;
#endif

	// if Snd_Surround_Speakers is -1, then upgrade to 2 speakers
	if (speakers < 0)
		speakers = 2;

	{
		for (int itemID = 0; itemID < m_pSpeakerSetupCombo->GetItemCount(); itemID++)
		{
			KeyValues* kv = m_pSpeakerSetupCombo->GetItemUserData(itemID);
			if (kv && kv->GetInt("speakers") == speakers)
			{
				m_pSpeakerSetupCombo->ActivateItem(itemID);
				break;
			}
		}
	}

	// sound quality is made up from several cvars
	ConVarRef Snd_PitchQuality("Snd_PitchQuality");
	ConVarRef dsp_slow_cpu("dsp_slow_cpu");
	int quality = SOUNDQUALITY_LOW;
	if (dsp_slow_cpu.GetBool() == false)
	{
		quality = SOUNDQUALITY_MEDIUM;
	}
	if (Snd_PitchQuality.GetBool())
	{
		quality = SOUNDQUALITY_HIGH;
	}
	// find the item in the list and activate it
	{
		for (int itemID = 0; itemID < m_pSoundQualityCombo->GetItemCount(); itemID++)
		{
			KeyValues* kv = m_pSoundQualityCombo->GetItemUserData(itemID);
			if (kv && kv->GetInt("quality") == quality)
			{
				m_pSoundQualityCombo->ActivateItem(itemID);
			}
		}
	}

	// Activate the current language in the combo
	{
		for (int itemID = 0; itemID < m_pSpokenLanguageCombo->GetItemCount(); itemID++)
		{
			KeyValues* kv = m_pSpokenLanguageCombo->GetItemUserData(itemID);
			if (kv && kv->GetInt("language") == m_nCurrentAudioLanguage)
			{
				m_pSpokenLanguageCombo->ActivateItem(itemID);
				break;
			}
		}
	}

	m_pSoundMuteLoseFocusCheckButton->Reset();
	*/
}

//-----------------------------------------------------------------------------
// Purpose: Applies changes
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnApplyChanges()
{
	m_pSFXSlider->ApplyChanges();
	m_pMusicSlider->ApplyChanges();
	m_pVideoVol_Slider->ApplyChanges();

	// quality
	int quality = m_pSoundQualityCombo->GetActiveItem();
	switch (quality)
	{
	case SOUNDQUALITY_LOW:
		engine->Cvar_SetValue("hisound", 0);
		break;
	default:
		Assert("Undefined sound quality setting.");
	case SOUNDQUALITY_HIGH:
		engine->Cvar_SetValue("hisound", 1);
		break;
	};

	bool on = m_pSubtitlesCombo->GetActiveItem();
	engine->Cvar_SetValue("r_subtitles", on ? 1 : 0);

	/*
	// headphones at high quality get enhanced stereo turned on
	ConVarRef dsp_enhance_stereo("dsp_enhance_stereo");
	if (speakers == 0 && quality == SOUNDQUALITY_HIGH)
	{
		dsp_enhance_stereo.SetValue(1);
	}
	else
	{
		dsp_enhance_stereo.SetValue(0);
	}

	// Audio spoken language
	KeyValues* kv = m_pSpokenLanguageCombo->GetItemUserData(m_pSpokenLanguageCombo->GetActiveItem());
	const ELanguage nUpdatedAudioLanguage = (ELanguage)(kv ? kv->GetInt("language") : k_Lang_English);

	if (nUpdatedAudioLanguage != m_nCurrentAudioLanguage)
	{
		// Store new language in static member so that it can be accessed during shutdown when this instance is gone
		m_pchUpdatedAudioLanguage = (char*)GetLanguageShortName(nUpdatedAudioLanguage);

		// Inform user that they need to restart in order change language at this time
		QueryBox* qb = new QueryBox("#GameUI_ChangeLanguageRestart_Title", "#GameUI_ChangeLanguageRestart_Info", GetParent()->GetParent()->GetParent());
		if (qb != NULL)
		{
			qb->SetOKCommand(new KeyValues("Command", "command", "RestartWithNewLanguage"));
			qb->SetOKButtonText("#GameUI_ChangeLanguageRestart_OkButton");
			qb->SetCancelButtonText("#GameUI_ChangeLanguageRestart_CancelButton");
			qb->AddActionSignalTarget(GetParent()->GetParent()->GetParent());
			qb->DoModal();
		}
	}

	m_pSoundMuteLoseFocusCheckButton->ApplyChanges();
	*/

	if (strcmp(engine->pfnGetCvarString("r_subtitleslang"), s_subtitlelanguages[m_pSubtitlesLangCombo->GetActiveItem()].language_cvarname))
		goto reloadgame;

	if (strcmp(engine->pfnGetCvarString("r_titleslang"), s_titlelanguages[m_pTitlesLangCombo->GetActiveItem()].language_cvarname))
		goto reloadgame;

	return;

reloadgame:
	auto pszLevelName = engine->pfnGetLevelName();
	if ((pszLevelName && *pszLevelName) && (!g_ReloadedGame))
	{
		// restart level
		engine->pfnClientCmd("save quick");
		engine->pfnClientCmd("load quick");
		g_ReloadedGame = true;
	}

	engine->Cvar_Set("r_subtitlesfont", s_subtitlelanguages[m_pSubtitlesLangCombo->GetActiveItem()].font_cvarname);
	engine->Cvar_Set("r_subtitleslang", s_subtitlelanguages[m_pSubtitlesLangCombo->GetActiveItem()].language_cvarname);
	engine->Cvar_Set("r_titleslang", s_titlelanguages[m_pTitlesLangCombo->GetActiveItem()].language_cvarname);
	engine->Cvar_Set("r_titlesfont", s_titlelanguages[m_pTitlesLangCombo->GetActiveItem()].font_cvarname);
}

//-----------------------------------------------------------------------------
// Purpose: Called on controls changing, enables the Apply button
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnControlModified()
{
	PostActionSignal(new KeyValues("ApplyButtonEnable"));
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the engine needs to be restarted
//-----------------------------------------------------------------------------
bool COptionsSubAudio::RequiresRestart()
{
	// nothing in audio requires a restart like now
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void COptionsSubAudio::OnCommand(const char* command)
{
	if (!stricmp(command, "TestSpeakers"))
	{
		// ask them if they REALLY want to test the speakers if they're in a game already.
		if (engine->pfnGetLevelName()[0])
		{
			QueryBox* qb = new QueryBox("#GameUI_TestSpeakersWarning_Title", "#GameUI_TestSpeakersWarning_Info");
			if (qb != NULL)
			{
				qb->SetOKCommand(new KeyValues("RunTestSpeakers"));
				qb->SetOKButtonText("#GameUI_TestSpeakersWarning_OkButton");
				qb->SetCancelButtonText("#GameUI_TestSpeakersWarning_CancelButton");
				qb->AddActionSignalTarget(this);
				qb->DoModal();
			}
			else
			{
				// couldn't create the warning dialog for some reason, so just test the speakers.
				RunTestSpeakers();
			}
		}
		else
		{
			// player isn't connected to a game so there's no reason to warn them about being disconnected.
			// create the command to execute
			RunTestSpeakers();
		}
	}

	BaseClass::OnCommand(command);
}

//-----------------------------------------------------------------------------
// Purpose: Run the test speakers map.
//-----------------------------------------------------------------------------
void COptionsSubAudio::RunTestSpeakers()
{
	//engine->ClientCmd_Unrestricted("disconnect\nwait\nwait\nsv_lan 1\nsetmaster enable\nmaxplayers 1\n\nhostname \"Speaker Test\"\nprogress_enable\nmap test_speakers\n");
}

//-----------------------------------------------------------------------------
// Purpose: third-party audio credits dialog
//-----------------------------------------------------------------------------
class COptionsSubAudioThirdPartyCreditsDlg: public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(COptionsSubAudioThirdPartyCreditsDlg, vgui2::Frame);

public:
	COptionsSubAudioThirdPartyCreditsDlg(vgui2::VPANEL hParent): BaseClass(NULL, NULL)
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

		SetTitle("#GameUI_ThirdPartyAudio_Title", true);
		SetSize(w, h);
		LoadControlSettings("resource/OptionsSubAudioThirdPartyDlg.res");
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
void COptionsSubAudio::OpenThirdPartySoundCreditsDialog()
{
	if (!m_OptionsSubAudioThirdPartyCreditsDlg.Get())
	{
		m_OptionsSubAudioThirdPartyCreditsDlg = new COptionsSubAudioThirdPartyCreditsDlg(GetVParent());
	}
	m_OptionsSubAudioThirdPartyCreditsDlg->Activate();
}