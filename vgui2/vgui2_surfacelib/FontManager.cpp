//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <cwctype>
#include <clocale>

#include <tier0/platform.h>

#include "vgui2_surfacelib/FontManager.h"
#include "vgui2_surfacelib/Win32Font.h"

#undef CreateFont

static CFontManager s_FontManager;

#define MAX_INITIAL_FONTS 100

struct Win98ForeignFallbackFont_t
{
	const char* language;
	const char* fallbackFont;
};

Win98ForeignFallbackFont_t g_Win98ForeignFallbackFonts[] =
	{
		{"russian", "system"},
		{"japanese", "win98japanese"},
		{"thai", "system"},
		{nullptr, "Tahoma"}};

struct FallbackFont_t
{
	const char* font;
	const char* fallbackFont;
};

// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
	{
		{"Times New Roman", "Courier New"},
		{"Courier New", "Courier"},
		{"Verdana", "Arial"},
		{"Trebuchet MS", "Arial"},
		{"Tahoma", nullptr},
		{nullptr, "Tahoma"} // every other font falls back to this
};

static const char* const g_szValidAsianFonts[] =
	{
		"Marlett",
		nullptr};

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CFontManager& FontManager()
{
	return s_FontManager;
}

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontManager::CFontManager(void)
{
	// add a single empty font, to act as an invalid font handle 0
	m_FontAmalgams.EnsureCapacity(MAX_INITIAL_FONTS);
	m_FontAmalgams.AddToTail();
	m_Win32Fonts.EnsureCapacity(MAX_INITIAL_FONTS);

	// setup our text locale
	setlocale(LC_CTYPE, "");
	setlocale(LC_TIME, "");
	setlocale(LC_COLLATE, "");
	setlocale(LC_MONETARY, "");
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontManager::~CFontManager(void)
{
	ClearAllFonts();
}

//-----------------------------------------------------------------------------
// Purpose: Sets a language
//-----------------------------------------------------------------------------
void CFontManager::SetLanguage(const char* language)
{
	Q_strncpy(m_szLanguage, language, sizeof(m_szLanguage));
}

//-----------------------------------------------------------------------------
// Purpose: Clears all fonts
//-----------------------------------------------------------------------------
void CFontManager::ClearAllFonts(void)
{
	// free the fonts
	for (int i = 0; i < m_Win32Fonts.Count(); i++)
	{
		delete m_Win32Fonts[i];
	}
	m_Win32Fonts.RemoveAll();
}

//-----------------------------------------------------------------------------
// Purpose: Returns a font by it's name
//-----------------------------------------------------------------------------
int CFontManager::GetFontByName(const char* name)
{
	for (int i = 1; i < m_FontAmalgams.Count(); i++)
	{
		if (!stricmp(name, m_FontAmalgams[i].Name()))
		{
			return i;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a font by char
//-----------------------------------------------------------------------------
CWin32Font* CFontManager::GetFontForChar(vgui2::HFont font, wchar_t wch)
{
	return m_FontAmalgams[font].GetFontForChar(wch);
}

//-----------------------------------------------------------------------------
// Purpose: Returns ABC of char
//-----------------------------------------------------------------------------
void CFontManager::GetCharABCwide(vgui2::HFont font, int ch, int& a, int& b, int& c)
{
	CWin32Font* winFont = m_FontAmalgams[font].GetFontForChar(ch);

	if (winFont)
	{
		winFont->GetCharABCWidths(ch, a, b, c);
	}
	else
	{
		// no font for this range, just use the default width
		a = c = 0;
		b = m_FontAmalgams[font].GetFontMaxWidth();
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CFontManager::GetFontTall(vgui2::HFont font)
{
	return m_FontAmalgams[font].GetFontHeight();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CFontManager::GetFontAscent(vgui2::HFont font, wchar_t wch)
{
	CWin32Font* winFont = m_FontAmalgams[font].GetFontForChar(wch);
	if (winFont)
	{
		return winFont->GetAscent();
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int CFontManager::GetCharacterWidth(vgui2::HFont font, int ch)
{
	if (!iswcntrl(ch))
	{
		int a, b, c;
		GetCharABCwide(font, ch, a, b, c);
		return (a + b + c);
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CFontManager::GetTextSize(vgui2::HFont font, const wchar_t* text, int& wide, int& tall)
{
	wide = 0;
	tall = 0;

	if (!text)
		return;

	tall = GetFontTall(font);

	int xx = 0;
	for (int i = 0;; i++)
	{
		wchar_t ch = text[i];
		if (ch == 0)
		{
			break;
		}
		else if (ch == '\n')
		{
			tall += GetFontTall(font);
			xx = 0;
		}
		else if (ch == '&')
		{
			// underscore character, so skip
		}
		else
		{
			xx += GetCharacterWidth(font, ch);
			if (xx > wide)
			{
				wide = xx;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns true if the font is in the list of OK asian fonts
//-----------------------------------------------------------------------------
bool CFontManager::IsFontForeignLanguageCapable(const char* windowsFontName)
{
	for (int i = 0; g_szValidAsianFonts[i] != nullptr; i++)
	{
		if (!stricmp(g_szValidAsianFonts[i], windowsFontName))
			return true;
	}

	// typeface isn't supported by asian languages
	return false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
const char* CFontManager::GetFallbackFontName(const char* windowsFontName)
{
	int i;

	for (i = 0; g_FallbackFonts[i].font != nullptr; i++)
	{
		if (!stricmp(g_FallbackFonts[i].font, windowsFontName))
			return g_FallbackFonts[i].fallbackFont;
	}

	// the ultimate fallback
	return g_FallbackFonts[i].fallbackFont;
}

//-----------------------------------------------------------------------------
// Purpose: specialized fonts
//-----------------------------------------------------------------------------
const char* CFontManager::GetForeignFallbackFontName(void)
{
	if (s_bSupportsUnicode)
	{
		// tahoma has all the necessary characters for asian/russian languages for winXP/2K+
		return "Tahoma";
	}

	int i;

	for (i = 0; g_Win98ForeignFallbackFonts[i].language != nullptr; i++)
	{
		if (!stricmp(g_Win98ForeignFallbackFonts[i].language, m_szLanguage))
			return g_Win98ForeignFallbackFonts[i].fallbackFont;
	}

	// the ultimate fallback
	return g_Win98ForeignFallbackFonts[i].fallbackFont;
}

//-----------------------------------------------------------------------------
// Purpose: Returns an underlined font
//-----------------------------------------------------------------------------
bool CFontManager::GetFontUnderlined(vgui2::HFont font)
{
	return m_FontAmalgams[font].GetUnderlined();
}

//-----------------------------------------------------------------------------
// Purpose: Creates a font
//-----------------------------------------------------------------------------
vgui2::HFont CFontManager::CreateFont(void)
{
	return m_FontAmalgams.AddToTail();
}

//-----------------------------------------------------------------------------
// Purpose: Creates a new win32 font, or reuses one if possible
//-----------------------------------------------------------------------------
CWin32Font* CFontManager::CreateOrFindWin32Font(const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// see if we already have the win32 font
	CWin32Font* winFont = nullptr;
	int i;
	for (i = 0; i < m_Win32Fonts.Count(); i++)
	{
		if (m_Win32Fonts[i]->IsEqualTo(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			winFont = m_Win32Fonts[i];
			break;
		}
	}

	// create the new win32font if we didn't find it
	if (!winFont)
	{
		MEM_ALLOC_CREDIT();

		i = m_Win32Fonts.AddToTail();

		m_Win32Fonts[i] = new CWin32Font();
		if (m_Win32Fonts[i]->Create(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			// add to the list
			winFont = m_Win32Fonts[i];
		}
		else
		{
			// failed to create, remove
			delete m_Win32Fonts[i];
			m_Win32Fonts.Remove(i);
			return nullptr;
		}
	}

	return winFont;
}

//-----------------------------------------------------------------------------
// Purpose: adds glyphs to a font created by CreateFont()
//-----------------------------------------------------------------------------
bool CFontManager::AddGlyphSetToFont(vgui2::HFont font, const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// see if we already have the win32 font
	CWin32Font* winFont = nullptr;
	CWin32Font* winFont2 = nullptr;

	const char* pszFontName = nullptr;
	const char* pszForeignFallback = nullptr;

	if (m_FontAmalgams[font].GetCount() > 0)
		return false;

	winFont = CreateOrFindWin32Font(windowsFontName, tall, weight, blur, scanlines, flags);

	for (pszFontName = windowsFontName; pszFontName; pszFontName = GetFallbackFontName(pszFontName))
	{
		pszForeignFallback = GetForeignFallbackFontName();

		if (winFont && (IsFontForeignLanguageCapable(pszFontName) || !stricmp(pszForeignFallback, pszFontName)))
		{
			m_FontAmalgams[font].AddFont(winFont, 0, 0xFFFF);
			return true;
		}

		winFont2 = CreateOrFindWin32Font(pszForeignFallback, tall, weight, blur, scanlines, flags);

		if (winFont2)
		{
			if (!winFont)
			{
				m_FontAmalgams[font].AddFont(winFont2, 0, 0xFFFF);
			}
			else
			{
				m_FontAmalgams[font].AddFont(winFont, 0, 0xFF);
				m_FontAmalgams[font].AddFont(winFont2, 0x100, 0xFFFF);
			}

			return true;
		}
	}

	return false;
}
