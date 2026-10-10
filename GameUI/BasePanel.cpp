#include <tier0/platform.h>

#include <FileSystem.h>
#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "BasePanel.h"
#include "EngineInterface.h"
#include "GameUI_Interface.h"
#include "igameuifuncs.h"
#include "Taskbar.h"

#include "tga_loader.h"

extern bool g_ReloadedGame;

CBasePanel::CBasePanel()
	: vgui2::Panel(nullptr, "BasePanel")
{
}

vgui2::VPANEL CBasePanel::IsWithinTraverse(int x, int y, bool traversePopups)
{
	if (!IsVisible())
		return NULL_HANDLE;

	return BaseClass::IsWithinTraverse(x, y, false);
}

void CBasePanel::OnChildAdded(vgui2::VPANEL child)
{
	if (g_pTaskbar)
		g_pTaskbar->AddTask(child);
}

void CBasePanel::PaintBackground()
{
	auto pszLevelName = engine->pfnGetLevelName();

	g_ReloadedGame = false;

	// Draw transparently if a level is loaded.
	// But not if we're in a career match.
	if (pszLevelName && *pszLevelName &&
		m_eBackgroundState != BACKGROUND_CAREERLOAD)
	{
		int swide, stall;
		vgui2::surface()->GetScreenSize(swide, stall);
		vgui2::surface()->DrawSetColor(0, 0, 0, 128);
		vgui2::surface()->DrawFilledRect(0, 0, swide, stall);
	}
	else
	{
		switch (m_eBackgroundState)
		{
		case BACKGROUND_BLACK:
		case BACKGROUND_CAREERLOAD:
		{
			// Draw black screen if loading a career match.
			int swide, stall;
			vgui2::surface()->GetScreenSize(swide, stall);
			vgui2::surface()->DrawSetColor(0, 0, 0, 255);
			vgui2::surface()->DrawFilledRect(0, 0, swide, stall);
			break;
		}

		case BACKGROUND_DESKTOPIMAGE:
		case BACKGROUND_LOADING:
		{
			DrawBackgroundImage();
			break;
		}

		default: break;
		}
	}
}

void CBasePanel::ApplySchemeSettings(vgui2::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	//auto hFile = vgui2::filesystem()->Open("resource/BackgroundLayout.txt", "rt");

	int vid_level = 0;
	#ifdef HL25_UPDATE
	gameuifuncs->GetCurrentRenderer(nullptr, 0, nullptr, nullptr, nullptr);
	#else
	gameuifuncs->GetCurrentRenderer(nullptr, 0, nullptr, nullptr, nullptr, &vid_level);
	#endif


	iImageID = vgui2::surface()->CreateNewTextureID();

	char imagepath[256];
	sprintf_s(imagepath, "%s/resource/game_background.tga", gEngfuncs.pfnGetGameDirectory());

	unsigned char* dummy = new unsigned char[8 * 8 * 4];

	for (int y = 0; y < 8; ++y)
	{
		for (int x = 0; x < 8; ++x)
		{
			int offset = (y * 8 + x) * 4;

			dummy[offset + 0] = 255;
			dummy[offset + 1] = 255;
			dummy[offset + 2] = 255;
			dummy[offset + 3] = 255;
		}
	}

	//make sure goldsrc finds this imageid valid so that staticTextureCurrent isnt 0.
	vgui2::surface()->DrawSetTextureRGBA(iImageID, dummy, 8, 8, vid_level == 0, false);
	delete[] dummy;

	int width = 0, height = 0;

	unsigned char* imagebuffer = LoadTGA_Manually_because_goldsrc_is_a_fucking_cunt(imagepath, width, height, iImageID);

	if(imagebuffer)
		delete[] imagebuffer;
}

void CBasePanel::SetBackgroundRenderState(EBackgroundState state)
{
	m_eBackgroundState = state;
}

void CBasePanel::DrawBackgroundImage()
{
	int wide, tall;
	GetSize(wide, tall);

	int swide, stall;
	vgui2::surface()->GetScreenSize(swide, stall);

	const float xScale = static_cast<double>(swide) / m_iBaseResX;
	const float yScale = static_cast<double>(stall) / m_iBaseResY;

	vgui2::surface()->DrawSetColor(255, 255, 255, 255);
	vgui2::surface()->DrawSetTexture(iImageID);
	vgui2::surface()->DrawTexturedRect(0, 0, swide, stall);
}
