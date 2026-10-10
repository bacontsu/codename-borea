//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#ifdef _WIN32
#pragma once
#endif

#include <tier1/UtlVector.h>

#include <vgui/VGUI2.h>

#include "vgui2_surfacelib/FontAmalgam.h"

#undef CreateFont

class CWin32Font;

//-----------------------------------------------------------------------------
// Purpose: Font Manager
//-----------------------------------------------------------------------------
class CFontManager
{
public:
	CFontManager(void);
	~CFontManager(void);

	void SetLanguage(const char* language);

	void ClearAllFonts(void);

	int GetFontByName(const char* name);
	CWin32Font* GetFontForChar(vgui2::HFont font, wchar_t wch);

	void GetCharABCwide(vgui2::HFont font, int ch, int& a, int& b, int& c);

	int GetFontTall(vgui2::HFont font);
	int GetFontAscent(vgui2::HFont font, wchar_t wch);
	int GetCharacterWidth(vgui2::HFont font, int ch);

	void GetTextSize(vgui2::HFont font, const wchar_t* text, int& wide, int& tall);

	bool IsFontForeignLanguageCapable(const char* windowsFontName);

	const char* GetFallbackFontName(const char* windowsFontName);
	const char* GetForeignFallbackFontName(void);

	bool GetFontUnderlined(vgui2::HFont font);

	vgui2::HFont CreateFont(void);

	CWin32Font* CreateOrFindWin32Font(const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	bool AddGlyphSetToFont(vgui2::HFont font, const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

private:
	CUtlVector<CFontAmalgam> m_FontAmalgams;
	CUtlVector<CWin32Font*> m_Win32Fonts;
	char m_szLanguage[64];

private:
	CFontManager(const CFontManager&) = delete;
	CFontManager& operator=(const CFontManager&) = delete;
};

CFontManager& FontManager(void);

#endif // FONTMANAGER_H
