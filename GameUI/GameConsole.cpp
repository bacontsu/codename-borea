#include <vgui/ISurface.h>
#include <vgui_controls/Controls.h>

#include "EngineInterface.h"
#include "GameConsole.h"
#include "GameConsoleDialog.h"
#include "LoadingDialog.h"
#include "GameUI_Interface.h"
#include "Taskbar.h"

CGameConsole g_GameConsole;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(
	CGameConsole, IGameConsole,
	GAMECONSOLE_INTERFACE_VERSION,
	g_GameConsole);

CGameConsole::CGameConsole()
{
}

CGameConsole::~CGameConsole()
{
	m_bInitialized = false;
}

void CGameConsole::Activate()
{
	if (m_bInitialized)
	{
		if (!LoadingDialog())
		{
			vgui2::surface()->RestrictPaintToSinglePanel(NULL_HANDLE);
			m_pConsole->Activate();
		}
	}
}

void CGameConsole::Initialize()
{
	m_pConsole = vgui2::SETUP_PANEL(new CGameConsoleDialog()); // we add text before displaying this so set it up now!

	// set the console to taking up most of the right-half of the screen
	int swide, stall;
	vgui2::surface()->GetScreenSize(swide, stall);
	int offsetx = vgui2::scheme()->GetProportionalScaledValue(16);
	int offsety = vgui2::scheme()->GetProportionalScaledValue(64);

	m_pConsole->SetBounds(
		swide / 2 - offsetx,
		offsety,
		swide / 2,
		1.f * (stall - (offsety * 2)));

	m_bInitialized = true;

	gEngfuncs.pfnAddCommand("condump", &CGameConsole::OnCmdCondump);
}

void CGameConsole::Hide()
{
	if (m_bInitialized)
	{
		m_pConsole->Hide();
		m_pConsole->SetVisible(false);
	}
}

void CGameConsole::Clear()
{
	if (m_bInitialized)
		m_pConsole->Clear();
}

bool CGameConsole::IsConsoleVisible()
{
	if (!m_bInitialized)
		return false;

	return m_pConsole->IsVisible();
}

void CGameConsole::Printf(const char* format, ...)
{
	char msg[4096];

	va_list va;

	va_start(va, format);

	if (m_bInitialized)
	{
		vsnprintf(msg, ARRAYSIZE(msg), format, va);
		msg[ARRAYSIZE(msg) - 1] = '\0';
		m_pConsole->Print(msg);
	}

	va_end(va);
}

void CGameConsole::DPrintf(const char* format, ...)
{
	char msg[4096];

	va_list va;

	va_start(va, format);

	if (m_bInitialized)
	{
		vsnprintf(msg, ARRAYSIZE(msg), format, va);
		msg[ARRAYSIZE(msg) - 1] = '\0';
		m_pConsole->DPrint(msg);
	}

	va_end(va);
}

void CGameConsole::SetParent(vgui2::VPANEL parent)
{
	if (m_bInitialized)
		m_pConsole->SetParent(parent);
}

void CGameConsole::ActivateDelayed(float time)
{
	if (!m_bInitialized)
		return;

	m_pConsole->PostMessage(m_pConsole, new KeyValues("Activate"), time);
}

void CGameConsole::OnCmdCondump()
{
	g_GameConsole.m_pConsole->DumpConsoleTextToFile();
}