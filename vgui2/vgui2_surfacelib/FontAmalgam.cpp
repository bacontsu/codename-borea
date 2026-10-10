//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#include <tier0/platform.h>

#include "vgui2_surfacelib/FontAmalgam.h"
#include "vgui2_surfacelib/Win32Font.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontAmalgam::CFontAmalgam(void): m_Fonts(0, 4)
{
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontAmalgam::~CFontAmalgam(void)
{
	m_Fonts.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Data accessor
//-----------------------------------------------------------------------------
void CFontAmalgam::SetName(const char* name)
{
	strncpy(m_szName, name, sizeof(m_szName) - 1);
	m_szName[sizeof(m_szName) - 1] = 0;
}

//-----------------------------------------------------------------------------
// Purpose: adds a font to the amalgam
//-----------------------------------------------------------------------------
void CFontAmalgam::AddFont(CWin32Font* font, int lowRange, int highRange)
{
	int i = m_Fonts.AddToTail();

	m_Fonts[i].font = font;
	m_Fonts[i].lowRange = lowRange;
	m_Fonts[i].highRange = highRange;

	m_iMaxHeight = max(font->GetHeight(), m_iMaxHeight);
	m_iMaxWidth = max(font->GetMaxCharWidth(), m_iMaxWidth);
}

//-----------------------------------------------------------------------------
// Purpose: returns the font for the given character
//-----------------------------------------------------------------------------
CWin32Font* CFontAmalgam::GetFontForChar(int ch)
{
	for (int i = 0; i < m_Fonts.Count(); i++)
	{
		if (ch >= m_Fonts[i].lowRange && ch <= m_Fonts[i].highRange)
		{
			// Assert(m_Fonts[i].font->IsValid());
			return m_Fonts[i].font;
		}
	}

	return nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of the font set
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFontHeight(void)
{
	if (!m_Fonts.Count())
	{
		return m_iMaxHeight;
	}
	return m_Fonts[0].font->GetHeight();
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character in a font
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFontMaxWidth(void)
{
	return m_iMaxWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the font that is loaded
//-----------------------------------------------------------------------------
const char* CFontAmalgam::GetFontName(int i)
{
	return m_Fonts[i].font->GetName();
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the font that is loaded
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFlags(int i)
{
	if (m_Fonts[i].font)
	{
		return m_Fonts[i].font->GetFlags();
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of fonts this amalgam contains
//-----------------------------------------------------------------------------
int CFontAmalgam::GetCount(void)
{
	return m_Fonts.Count();
}


//-----------------------------------------------------------------------------
// Purpose: returns the max height of the font set
//-----------------------------------------------------------------------------
bool CFontAmalgam::GetUnderlined(void)
{
	if (!m_Fonts.Count())
	{
		return false;
	}
	return m_Fonts[0].font->GetUnderlined();
}

//-----------------------------------------------------------------------------
// Purpose: Removes all fons
//-----------------------------------------------------------------------------
void CFontAmalgam::RemoveAll(void)
{
	// clear out
	m_Fonts.RemoveAll();

	m_iMaxWidth = 0;
	m_iMaxHeight = 0;
}
