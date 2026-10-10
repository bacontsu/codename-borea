//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================//

#ifndef NEWGAMEDIALOG_H
#define NEWGAMEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Frame.h"
#include "vgui_controls/Button.h"
#include "EngineInterface.h"
#include "cvardef.h"
#include "utlvector.h"

using namespace vgui2;

class CGameChapterPanel;
class CSkillSelectionDialog;

// Slot indices in new game menu
#define INVALID_INDEX -1
#define SLOT_OFFLEFT 0
#define SLOT_LEFT 1
#define SLOT_CENTER 2
#define SLOT_RIGHT 3
#define SLOT_OFFRIGHT 4
#define NUM_SLOTS 5


class CNewGamePlayButton: public vgui2::Button
{
	DECLARE_CLASS_SIMPLE(CNewGamePlayButton, vgui2::Button);

public:
	CNewGamePlayButton(Panel* parent, const char* panelName, const char* text, Panel* pActionSignalTarget = NULL, const char* pCmd = NULL)
		: vgui2::Button(parent, panelName, text, pActionSignalTarget, pCmd)
	{
	}

	void OnKeyCodePressed(vgui2::KeyCode code)
	{
		if (code == KEY_XBUTTON_A)
		{
			cvar_t* var = engine->pfnGetCvarPointer("joystick");
			if (var != nullptr)
			{
				if (var->value == 0)
					var->value = true;
			}

			cvar_t* var2 = engine->pfnGetCvarPointer("hud_fastswitch");
			if (var2 != nullptr)
			{
				if (var2->value != 2)
					var2->value = 2;
			}
			DoClick();
			return;
		}

		BaseClass::OnKeyCodePressed(code);
	}
};

//-----------------------------------------------------------------------------
// Purpose: Handles starting a new game, skill and chapter selection
//-----------------------------------------------------------------------------
class CNewGameDialog: public vgui2::Frame
{
	DECLARE_CLASS_SIMPLE(CNewGameDialog, vgui2::Frame);

public:
	MESSAGE_FUNC(FinishScroll, "FinishScroll");
	MESSAGE_FUNC(StartGame, "StartGame");

	CNewGameDialog(vgui2::Panel* parent, bool bCommentaryMode);
	~CNewGameDialog();

	virtual void Activate(void);

	virtual void ApplySettings(KeyValues* inResourceData);
	virtual void ApplySchemeSettings(vgui2::IScheme* pScheme);
	virtual void OnCommand(const char* command);
	virtual void OnClose(void);
	virtual void PaintBackground();
	void SetSelectedChapterIndex(int index);
	void SetSelectedChapter(const char* chapter);
	void UpdatePanelLockedStatus(/*int iUnlockedChapter,*/ int i, CGameChapterPanel* pChapterPanel);

	void SetCommentaryMode(bool bCommentary) { m_bCommentaryMode = bCommentary; }

	virtual void OnKeyCodePressed(vgui2::KeyCode code);
	virtual void OnKeyCodeReleased(vgui2::KeyCode code);
	virtual void OnThink();

	// Xbox: Defined values are also used to shift the slot indices
	enum EScrollDirection
	{
		SCROLL_RIGHT = -1,
		SCROLL_NONE = 0,
		SCROLL_LEFT = 1
	};
	EScrollDirection m_ScrollDirection;

private:
	int m_iSelectedChapter;

	CUtlVector<CGameChapterPanel*> m_ChapterPanels;

	vgui2::Button* m_pPlayButton = nullptr;
	vgui2::Button* m_pNextButton = nullptr;
	vgui2::Button* m_pPrevButton = nullptr;
	vgui2::Panel* m_pCenterBg;
	bool m_bCommentaryMode;

	// Xbox
	void ScrollSelectionPanels(EScrollDirection dir);
	void ScrollBonusSelection(EScrollDirection dir);
	void PreScroll(EScrollDirection dir);
	void PostScroll(EScrollDirection dir);
	void SetFastScroll(bool fast);
	void ContinueScrolling(void);
	void AnimateSelectionPanels(void);
	void ShiftPanelIndices(int offset);
	bool IsValidPanel(const int idx);
	void InitPanelIndexForDisplay(const int idx);
	void UpdateMenuComponents(EScrollDirection dir);
	void UpdateBonusSelection(void);

	int m_PanelXPos[NUM_SLOTS];
	int m_PanelYPos[NUM_SLOTS];
	float m_PanelAlpha[NUM_SLOTS];
	int m_PanelIndex[NUM_SLOTS];
	float m_ScrollSpeed;
	int m_ButtonPressed;
	int m_ScrollCt;
	bool m_bScrolling;
	char m_ActiveTitleIdx;
	bool m_bMapStarting;
	int m_iBonusSelection;
	bool m_bScrollToFirstBonusMap;

	// vgui2::CKeyRepeatHandler m_KeyRepeat;
};

#endif // NEWGAMEDIALOG_H