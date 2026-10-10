//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//


#include "BaseSaveGameDialog.h"
#include "filesystem.h"
#include "savegame_version.h"
#include "vgui_controls/PanelListPanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Button.h"
#include "tier1/utlbuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include "filesystem.h"
#include "EngineInterface.h"

#include <vgui_controls/Image.h>

#include "cvardef.h"

//#include "MouseMessageForwardingPanel.h"
//#include "TGAImagePanel.h"

#include <time.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui2;

#define TGA_IMAGE_PANEL_WIDTH 180
#define TGA_IMAGE_PANEL_HEIGHT 100

#define MAX_LISTED_SAVE_GAMES 128

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

using namespace vgui2;

class CSaveGamePanel;

class ImagePanel_Selectable : public ImagePanel
{
	DECLARE_CLASS_SIMPLE(ImagePanel_Selectable, ImagePanel)

public:
	ImagePanel_Selectable(Panel* parent, const char* name);


	MESSAGE_FUNC_INT(OnPanelSelected, "PanelSelected", state)
	{
		PostMessage(GetVParent(), new KeyValues("PanelSelected"));
	}

	virtual void OnMousePressed(vgui2::MouseCode code)
	{
		m_pParent->OnMousePressed(code);
	}

	virtual void OnMouseDoublePressed(vgui2::MouseCode code)
	{
		m_pParent->OnMouseDoublePressed(code);
	}

protected:
	virtual void ApplySchemeSettings(IScheme* pScheme);

private:
	IImage* m_pImage;
	char* m_pszImageName;
	char* m_pszFillColorName;
	char* m_pszDrawColorName;
	bool m_bCenterImage;
	bool m_bScaleImage;
	bool m_bTileImage;
	bool m_bTileHorizontally;
	bool m_bTileVertically;
	float m_fScaleAmount;
	Color m_FillColor;
	Color m_DrawColor;

	Panel* m_pParent;
};

ImagePanel_Selectable::ImagePanel_Selectable(Panel* parent, const char* name): ImagePanel(parent, name)
{
	m_pImage = NULL;
	m_pszImageName = NULL;
	m_pszFillColorName = NULL;
	m_pszDrawColorName = NULL; // HPE addition
	m_bCenterImage = false;
	m_bScaleImage = false;
	m_bTileImage = false;
	m_bTileHorizontally = false;
	m_bTileVertically = false;
	m_fScaleAmount = 0.0f;
	m_FillColor = Color(0, 0, 0, 0);
	m_DrawColor = Color(255, 255, 255, 255);

	m_pParent = parent;

	SetImage(m_pImage);

	REGISTER_COLOR_AS_OVERRIDABLE(m_FillColor, "fillcolor_override");
	REGISTER_COLOR_AS_OVERRIDABLE(m_DrawColor, "drawcolor_override");
}

void ImagePanel_Selectable::ApplySchemeSettings(IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	if (m_pszImageName && strlen(m_pszImageName) > 0)
	{
		SetImage(scheme()->GetImage(m_pszImageName, m_bScaleImage));
		int wide, tall;
		m_pImage->GetSize(wide, tall);
		SetSize(wide, tall);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Describes the layout of a same game pic
//-----------------------------------------------------------------------------
class CSaveGamePanel: public vgui2::EditablePanel
{
	DECLARE_CLASS_SIMPLE(CSaveGamePanel, vgui2::EditablePanel);

public:
	CSaveGamePanel(PanelListPanel* parent, const char* name, int saveGameListItemID): BaseClass(parent, name)
	{
		m_iSaveGameListItemID = saveGameListItemID;
		m_pParent = parent;
		m_pSaveGameImage = new ImagePanel_Selectable(this, "SaveGameImage");
		m_pAutoSaveImage = new ImagePanel_Selectable(this, "AutoSaveImage");
		m_pSaveGameScreenshotBackground = new ImagePanel_Selectable(this, "SaveGameScreenshotBackground");
		m_pChapterLabel = new Label(this, "ChapterLabel", "");
		m_pTypeLabel = new Label(this, "TypeLabel", "");
		m_pElapsedTimeLabel = new Label(this, "ElapsedTimeLabel", "");
		m_pFileTimeLabel = new Label(this, "FileTimeLabel", "");

		SetSize(200, 140);

		LoadControlSettings("resource/SaveGamePanel.res");

		m_FillColor = m_pSaveGameScreenshotBackground->GetFillColor();
	}

	void SetSaveGameInfo(SaveGameDescription_t& save)
	{
		// set the bitmap to display
		char tga[_MAX_PATH];
		Q_strncpy(tga, save.szFileName, sizeof(tga));
		char* ext = strstr(tga, ".sav");
		if (ext)
		{
			strcpy(ext, ".tga");
		}

		// If a TGA file exists then it is a user created savegame
		if (filesystem()->FileExists(tga))
		{
			m_pSaveGameImage->SetImage(tga);
		}
		// If there is no TGA then it is either an autosave or the user TGA file has been deleted
		else
		{
			m_pSaveGameImage->SetVisible(false);
			m_pAutoSaveImage->SetVisible(true);
			m_pAutoSaveImage->SetImage("materials\\vgui\\resource\\autosave");
		}

		// set the title text
		m_pChapterLabel->SetText(save.szComment);

		// type
		SetControlString("TypeLabel", save.szType);
		SetControlString("ElapsedTimeLabel", save.szElapsedTime);
		SetControlString("FileTimeLabel", save.szFileTime);
	}

	MESSAGE_FUNC_INT(OnPanelSelected, "PanelSelected", state)
	{
		if (state)
		{
			// set the text color to be orange, and the pic border to be orange
			m_pSaveGameScreenshotBackground->SetFillColor(m_SelectedColor);
			m_pChapterLabel->SetFgColor(m_SelectedColor);
			m_pTypeLabel->SetFgColor(m_SelectedColor);
			m_pElapsedTimeLabel->SetFgColor(m_SelectedColor);
			m_pFileTimeLabel->SetFgColor(m_SelectedColor);
		}
		else
		{
			m_pSaveGameScreenshotBackground->SetFillColor(m_FillColor);
			m_pChapterLabel->SetFgColor(m_TextColor);
			m_pTypeLabel->SetFgColor(m_TextColor);
			m_pElapsedTimeLabel->SetFgColor(m_TextColor);
			m_pFileTimeLabel->SetFgColor(m_TextColor);
		}

		PostMessage(m_pParent->GetVParent(), new KeyValues("PanelSelected"));
	}

	virtual void OnMousePressed(vgui2::MouseCode code)
	{
		m_pParent->SetSelectedPanel(this);
	}

	virtual void ApplySchemeSettings(IScheme* pScheme)
	{
		m_TextColor = pScheme->GetColor("NewGame.TextColor", Color(255, 255, 255, 255));
		m_SelectedColor = pScheme->GetColor("NewGame.SelectionColor", Color(255, 255, 255, 255));

		BaseClass::ApplySchemeSettings(pScheme);
	}

	virtual void OnMouseDoublePressed(vgui2::MouseCode code)
	{
		// call the panel
		OnMousePressed(code);
		PostMessage(m_pParent->GetParent(), new KeyValues("Command", "command", "loadsave"));
	}

	int GetSaveGameListItemID()
	{
		return m_iSaveGameListItemID;
	}

private:
	vgui2::PanelListPanel* m_pParent;
	vgui2::Label* m_pChapterLabel;
	ImagePanel_Selectable* m_pSaveGameImage;
	ImagePanel_Selectable* m_pAutoSaveImage;

	// things to change color when the selection changes
	ImagePanel_Selectable* m_pSaveGameScreenshotBackground;
	Label* m_pTypeLabel;
	Label* m_pElapsedTimeLabel;
	Label* m_pFileTimeLabel;
	Color m_TextColor, m_FillColor, m_SelectedColor;

	int m_iSaveGameListItemID;
};


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseSaveGameDialog::CBaseSaveGameDialog(vgui2::Panel* parent, const char* name): BaseClass(parent, name)
{
	CreateSavedGamesList();
	ScanSavedGames();

	m_pLoadButton = new vgui2::Button(this, "loadsave", "");
	SetControlEnabled("loadsave", false);
}

//-----------------------------------------------------------------------------
// Purpose: Creates the load game display list
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::CreateSavedGamesList()
{
	m_pGameList = new vgui2::PanelListPanel(this, "listpanel_loadgame");
	m_pGameList->SetFirstColumnWidth(0);
}

//-----------------------------------------------------------------------------
// Purpose: returns the save file name of the selected item
//-----------------------------------------------------------------------------
int CBaseSaveGameDialog::GetSelectedItemSaveIndex()
{
	CSaveGamePanel* panel = dynamic_cast<CSaveGamePanel*>(m_pGameList->GetSelectedPanel());
	if (panel)
	{
		// find the panel in the list
		for (int i = 0; i < m_SaveGames.Count(); i++)
		{
			if (i == panel->GetSaveGameListItemID())
			{
				return i;
			}
		}
	}
	return m_SaveGames.InvalidIndex();
}

#include "vector"
#include "string"

//-----------------------------------------------------------------------------
// Purpose: builds save game list from directory
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::ScanSavedGames()
{
	// populate list box with all saved games on record:
	char szDirectory[_MAX_PATH];
	Q_snprintf(szDirectory, sizeof(szDirectory), "save/*.sav");

	// clear the current list
	m_pGameList->DeleteAllItems();
	m_SaveGames.RemoveAll();

	// iterate the saved files
	FileFindHandle_t handle;
	const char* pFileName = filesystem()->FindFirst(szDirectory, &handle);

	std::vector<std::string> m_sAlreadyScanned; //fix save games being displayed twice
	while (pFileName)
	{
		bool shouldskip = false;
		if (!Q_strnicmp(pFileName, "HLSave", strlen("HLSave")))
		{
			pFileName = filesystem()->FindNext(handle);
			continue;
		}

		char szFileName[_MAX_PATH];
		Q_snprintf(szFileName, sizeof(szFileName), "save/%s", pFileName);

		for (auto string : m_sAlreadyScanned)
		{
			if ( !Q_strcmp(szFileName, string.c_str()) )
			{
				pFileName = filesystem()->FindNext(handle);
				shouldskip = true;
				break;
			}
		}
		if (shouldskip)
			continue;

		m_sAlreadyScanned.push_back(szFileName);

		// Only load save games from the current mod's save dir
		if (!filesystem()->FileExists(szFileName))
		{
			pFileName = filesystem()->FindNext(handle);
			continue;
		}

		SaveGameDescription_t save;
		if (ParseSaveData(szFileName, pFileName, save))
		{
			m_SaveGames.AddToTail(save);
		}

		pFileName = filesystem()->FindNext(handle);
	}

	filesystem()->FindClose(handle);

	// notify derived classes that save games are being scanned (so they can insert their own)
	OnScanningSaveGames();

	// sort the save list
	qsort(m_SaveGames.Base(), m_SaveGames.Count(), sizeof(SaveGameDescription_t), &SaveGameSortFunc);

	// add to the list
	for (int saveIndex = 0; saveIndex < m_SaveGames.Count() && saveIndex < MAX_LISTED_SAVE_GAMES; saveIndex++)
	{
		// add the item to the panel
		AddSaveGameItemToList(saveIndex);
	}

	// display a message if there are no save games
	if (!m_SaveGames.Count())
	{
		vgui2::Label* pNoSavesLabel = SETUP_PANEL(new Label(m_pGameList, "NoSavesLabel", "#GameUI_NoSaveGamesToDisplay"));
		pNoSavesLabel->SetTextColorState(vgui2::Label::CS_DULL);
		m_pGameList->AddItem(NULL, pNoSavesLabel);
	}

	SetControlEnabled("loadsave", false);
	SetControlEnabled("delete", false);
}

//-----------------------------------------------------------------------------
// Purpose: Adds an item to the list
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::AddSaveGameItemToList(int saveIndex)
{
	// create the new panel and add to the list
	CSaveGamePanel* saveGamePanel = new CSaveGamePanel(m_pGameList, "SaveGamePanel", saveIndex);
	saveGamePanel->SetSaveGameInfo(m_SaveGames[saveIndex]);
	m_pGameList->AddItem(NULL, saveGamePanel);
}

//-----------------------------------------------------------------------------
// Purpose: Parses the save game info out of the .sav file header
//-----------------------------------------------------------------------------
bool CBaseSaveGameDialog::ParseSaveData(char const* pszFileName, char const* pszShortName, SaveGameDescription_t& save)
{
	char szMapName[SAVEGAME_MAPNAME_LEN];
	char szComment[SAVEGAME_COMMENT_LEN];
	char szElapsedTime[SAVEGAME_ELAPSED_LEN];

	if (!pszFileName || !pszShortName)
		return false;

	Q_strncpy(save.szShortName, pszShortName, sizeof(save.szShortName));
	Q_strncpy(save.szFileName, pszFileName, sizeof(save.szFileName));

	FileHandle_t fh = filesystem()->Open(pszFileName, "rb", "GAMECONFIG");
	if (fh == FILESYSTEM_INVALID_HANDLE)
		return false;

	int readok = SaveReadNameAndComment(fh, szMapName, ARRAYSIZE(szMapName), szComment, ARRAYSIZE(szComment));
	filesystem()->Close(fh);

	if (!readok)
	{
		return false;
	}

	Q_strncpy(save.szMapName, szMapName, sizeof(save.szMapName));

	// Elapsed time is the last 6 characters in comment. (mmm:ss)
	int i;
	i = strlen(szComment);
	Q_strncpy(szElapsedTime, "??", sizeof(szElapsedTime));
	if (i >= 6)
	{
		Q_strncpy(szElapsedTime, (char*)&szComment[i - 6], 7);
		szElapsedTime[6] = '\0';

		// parse out
		int minutes = atoi(szElapsedTime);
		int seconds = atoi(szElapsedTime + 4);

		// reformat
		if (minutes)
		{
			Q_snprintf(szElapsedTime, sizeof(szElapsedTime), "%d %s %d seconds", minutes, minutes > 1 ? "minutes" : "minute", seconds);
		}
		else
		{
			Q_snprintf(szElapsedTime, sizeof(szElapsedTime), "%d seconds", seconds);
		}

		// Chop elapsed out of comment.
		int n;

		n = i - 6;
		szComment[n] = '\0';

		n--;

		// Strip back the spaces at the end.
		while ((n >= 1) &&
			   szComment[n] &&
			   szComment[n] == ' ')
		{
			szComment[n--] = '\0';
		}
	}

	// calculate the file name to print
	const char* pszType = "";
	if (strstr(pszFileName, "quick"))
	{
		pszType = "#GameUI_QuickSave";
	}
	else if (strstr(pszFileName, "autosave"))
	{
		pszType = "#GameUI_AutoSave";
	}

	Q_strncpy(save.szType, pszType, sizeof(save.szType));
	Q_strncpy(save.szComment, szComment, sizeof(save.szComment));
	Q_strncpy(save.szElapsedTime, szElapsedTime, sizeof(save.szElapsedTime));

	// Now get file time stamp.
	time_t fileTime = filesystem()->GetFileTime(pszFileName);
	char szFileTime[32];
	filesystem()->FileTimeToString(szFileTime, sizeof(szFileTime), fileTime);
	char* newline = strstr(szFileTime, "\n");
	if (newline)
	{
		*newline = 0;
	}
	Q_strncpy(save.szFileTime, szFileTime, sizeof(save.szFileTime));
	save.iTimestamp = fileTime;
	return true;
}

void CBaseSaveGameDialog::OnKeyCodeTyped(vgui2::KeyCode code)
{
	if (code == KEY_ESCAPE)
	{
		OnCommand("Close");
		return;
	}

	BaseClass::OnKeyCodeTyped(code);
}

void CBaseSaveGameDialog::OnKeyCodePressed(vgui2::KeyCode code)
{
	if (code == KEY_XBUTTON_B)
	{
		OnCommand("Close");
		return;
	}
	else if (code == KEY_XSTICK1_DOWN ||
			 code == KEY_XSTICK2_DOWN ||
			 code == KEY_XBUTTON_DOWN ||
			 code == KEY_DOWN)
	{
		if (m_pGameList->GetItemCount())
		{
			Panel* pSelectedPanel = m_pGameList->GetSelectedPanel();
			if (!pSelectedPanel)
			{
				m_pGameList->SetSelectedPanel(m_pGameList->GetItemPanel(m_pGameList->FirstItem()));
				m_pGameList->ScrollToItem(m_pGameList->FirstItem());
				return;
			}
			else
			{
				int nNextPanelID = m_pGameList->FirstItem();
				while (nNextPanelID != m_pGameList->InvalidItemID())
				{
					if (m_pGameList->GetItemPanel(nNextPanelID) == pSelectedPanel)
					{
						nNextPanelID = m_pGameList->NextItem(nNextPanelID);
						if (nNextPanelID != m_pGameList->InvalidItemID())
						{
							m_pGameList->SetSelectedPanel(m_pGameList->GetItemPanel(nNextPanelID));
							m_pGameList->ScrollToItem(nNextPanelID);
							return;
						}

						break;
					}

					nNextPanelID = m_pGameList->NextItem(nNextPanelID);
				}
			}
		}
	}
	else if (code == KEY_XSTICK1_UP ||
			 code == KEY_XSTICK2_UP ||
			 code == KEY_XBUTTON_UP ||
			 code == KEY_UP)
	{
		if (m_pGameList->GetItemCount())
		{
			Panel* pSelectedPanel = m_pGameList->GetSelectedPanel();
			if (!pSelectedPanel)
			{
				m_pGameList->SetSelectedPanel(m_pGameList->GetItemPanel(m_pGameList->FirstItem()));
				m_pGameList->ScrollToItem(m_pGameList->FirstItem());
				return;
			}
			else
			{
				int nNextPanelID = m_pGameList->FirstItem();
				if (m_pGameList->GetItemPanel(nNextPanelID) != pSelectedPanel)
				{
					while (nNextPanelID != m_pGameList->InvalidItemID())
					{
						int nOldPanelID = nNextPanelID;
						nNextPanelID = m_pGameList->NextItem(nNextPanelID);

						if (nNextPanelID != m_pGameList->InvalidItemID())
						{
							if (m_pGameList->GetItemPanel(nNextPanelID) == pSelectedPanel)
							{
								m_pGameList->SetSelectedPanel(m_pGameList->GetItemPanel(nOldPanelID));
								m_pGameList->ScrollToItem(nOldPanelID);
								return;
							}
						}
					}
				}
			}
		}
	}
	else if (code == KEY_ENTER || code == KEY_XBUTTON_A)
	{
		Panel* pSelectedPanel = m_pGameList->GetSelectedPanel();
		if (pSelectedPanel)
		{
			if (code == KEY_XBUTTON_A)
			{
				cvar_t* var = engine->pfnGetCvarPointer("joystick");
				if (!var->value)
				{
					var->value = 1;
				}

				cvar_t* var2 = engine->pfnGetCvarPointer("hud_fastswitch");
				if (var2->value != 2)
				{
					var2->value = 2;
				}
			}

			m_pLoadButton->DoClick();
			return;
		}
	}

	BaseClass::OnKeyCodePressed(code);
}

//-----------------------------------------------------------------------------
// Purpose: timestamp sort function for savegames
//-----------------------------------------------------------------------------
int CBaseSaveGameDialog::SaveGameSortFunc(const void* lhs, const void* rhs)
{
	const SaveGameDescription_t* s1 = (const SaveGameDescription_t*)lhs;
	const SaveGameDescription_t* s2 = (const SaveGameDescription_t*)rhs;

	if (s1->iTimestamp < s2->iTimestamp)
		return 1;
	else if (s1->iTimestamp > s2->iTimestamp)
		return -1;

	// timestamps are equal, so just sort by filename
	return strcmp(s1->szFileName, s2->szFileName);
}

#define MAKEID(d, c, b, a) (((int)(a) << 24) | ((int)(b) << 16) | ((int)(c) << 8) | ((int)(d)))

int SaveReadNameAndComment(FileHandle_t f, OUT_Z_CAP(nameSize) char* name, int nameSize, OUT_Z_CAP(commentSize) char* comment, int commentSize)
{
	int i, tag, size, tokenSize, tokenCount;
	char *pSaveData, *pFieldName, **pTokenList;

	name[0] = '\0';
	comment[0] = '\0';

	filesystem()->Read(&tag, sizeof(int), f);
	if (tag != MAKEID('J', 'S', 'A', 'V'))
	{
		return 0;
	}

	filesystem()->Read(&tag, sizeof(int), f);
	if (tag != SAVEGAME_VERSION) // Enforce version for now
	{
		return 0;
	}

	filesystem()->Read(&size, sizeof(int), f);

	filesystem()->Read(&tokenCount, sizeof(int), f); // These two ints are the token list
	filesystem()->Read(&tokenSize, sizeof(int), f);
	size += tokenSize;

	// Sanity Check.
	if (tokenCount < 0 || tokenCount > 1024 * 1024 * 32)
	{
		return 0;
	}

	if (tokenSize < 0 || tokenSize > 1024 * 1024 * 32)
	{
		return 0;
	}

	pSaveData = (char*)new char[size];
	filesystem()->Read(pSaveData, size, f);

	int nNumberOfFields;

	char* pData;
	short nFieldSize;

	pData = pSaveData;

	// Allocate a table for the strings, and parse the table
	if (tokenSize > 0)
	{
		pTokenList = new char*[tokenCount];

		// Make sure the token strings pointed to by the pToken hashtable.
		for (i = 0; i < tokenCount; i++)
		{
			pTokenList[i] = *pData ? pData : NULL; // Point to each string in the pToken table
			while (*pData++)
				; // Find next token (after next null)
		}
	}
	else
		pTokenList = NULL;

	// short, short (size, index of field name)
	memcpy(&nFieldSize, pData, sizeof(short));

	pData += sizeof(short);
	short index;
	memcpy(&index, pData, sizeof(short));
	pFieldName = pTokenList[index];

	if (stricmp(pFieldName, "GameHeader"))
	{
		delete[] pSaveData;
		return 0;
	};

	// int (fieldcount)
	pData += sizeof(short);
	memcpy(&nNumberOfFields, pData, sizeof(int));
	pData += nFieldSize;

	// Each field is a short (size), short (index of name), binary string of "size" bytes (data)
	for (i = 0; i < nNumberOfFields; i++)
	{
		// Data order is:
		// Size
		// szName
		// Actual Data

		memcpy(&nFieldSize, pData, sizeof(short));
		pData += sizeof(short);

		short index;
		memcpy(&index, pData, sizeof(short));
		pFieldName = pTokenList[index];
		pData += sizeof(short);

		if (!stricmp(pFieldName, "comment"))
		{
			int copySize = max(commentSize, nFieldSize);
			Q_strncpy(comment, pData, copySize);
		}
		else if (!stricmp(pFieldName, "mapName"))
		{
			int copySize = max(nameSize, nFieldSize);
			Q_strncpy(name, pData, copySize);
		};

		// Move to Start of next field.
		pData += nFieldSize;
	};

	// Delete the string table we allocated
	delete[] pTokenList;
	delete[] pSaveData;

	if (strlen(name) > 0 && strlen(comment) > 0)
		return 1;

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: deletes an existing save game
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::DeleteSaveGame(const char* fileName)
{
	if (!fileName || !fileName[0])
		return;

	// delete the save game file
	filesystem()->RemoveFile(fileName, "GAMECONFIG");

	// delete the associated tga
	char tga[_MAX_PATH];
	Q_strncpy(tga, fileName, sizeof(tga));
	char* ext = strstr(tga, ".sav");
	if (ext)
	{
		strcpy(ext, ".tga");
	}
	filesystem()->RemoveFile(tga, "GAMECONFIG");
}

//-----------------------------------------------------------------------------
// Purpose: One item has been selected
//-----------------------------------------------------------------------------
void CBaseSaveGameDialog::OnPanelSelected()
{
	SetControlEnabled("loadsave", true);
	SetControlEnabled("delete", true);
}
