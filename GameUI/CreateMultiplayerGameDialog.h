#ifndef GAMEUI_CREATEMULTIPLAYERGAMEDIALOG_H
#define GAMEUI_CREATEMULTIPLAYERGAMEDIALOG_H

#include <vgui_controls/PropertyDialog.h>

class CCreateMultiplayerGameServerPage;
class CCreateMultiplayerGameGameplayPage;
class CCreateMultiplayerGameBotPage;

//-----------------------------------------------------------------------------
// Purpose: dialog for launching a listenserver
//-----------------------------------------------------------------------------
class CCreateMultiplayerGameDialog: public vgui2::PropertyDialog
{
	DECLARE_CLASS_SIMPLE(CCreateMultiplayerGameDialog, vgui2::PropertyDialog);

public:
	CCreateMultiplayerGameDialog(vgui2::Panel* parent);
	~CCreateMultiplayerGameDialog();

protected:
	void SetTitle(const char* title, bool surfaceTitle) override;

	virtual bool OnOK(bool applyOnly);

	void OnClose() override;

private:
	CCreateMultiplayerGameServerPage* m_pServerPage = nullptr;
	CCreateMultiplayerGameGameplayPage* m_pGameplayPage = nullptr;
	CCreateMultiplayerGameBotPage* m_pBotPage = nullptr;

	bool m_bBotsEnabled = false;

	// for loading/saving game config
	KeyValues* m_pBotSavedData = nullptr;
};

#endif // GAMEUI_CREATEMULTIPLAYERGAMEDIALOG_H
