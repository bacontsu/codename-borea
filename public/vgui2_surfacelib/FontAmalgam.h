//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef FONTAMALGAM_H
#define FONTAMALGAM_H

#ifdef _WIN32
#pragma once
#endif

#include <tier1/UtlVector.h>

class CWin32Font;

//-----------------------------------------------------------------------------
// Purpose: Font Amalgam
//-----------------------------------------------------------------------------
class CFontAmalgam
{
public:
	CFontAmalgam(void);
	~CFontAmalgam(void);

	char* Name(void) { return m_szName; }

	void SetName(const char* name);

	void AddFont(CWin32Font* font, int lowRange, int highRange);

	CWin32Font* GetFontForChar(int ch);

	int GetFontHeight(void);

	int GetFontMaxWidth(void);

	const char* GetFontName(int i);

	int GetFlags(int i);

	int GetCount(void);

	bool GetUnderlined(void);

	void RemoveAll(void);

private:
	struct TFontRange
	{
		int lowRange;
		int highRange;
		CWin32Font* font;
	};

	CUtlVector<TFontRange> m_Fonts;

	char m_szName[32];

	int m_iMaxWidth = 0;
	int m_iMaxHeight = 0;

private:
	CFontAmalgam(const CFontAmalgam&) = delete;
	CFontAmalgam& operator=(const CFontAmalgam&) = delete;
};

#endif // VGUI2_VGUI_SURFACELIB_FONTAMALGAM_H
