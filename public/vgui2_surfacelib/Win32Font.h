//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose:
//
// $NoKeywords: $
//=============================================================================

#ifndef WIN32FONT_H
#define WIN32FONT_H

#ifdef _WIN32
#pragma once
#endif

#include <tier1/UtlRBTree.h>

#include <winsani_in.h>
#include <windows.h>
#include <winsani_out.h>

#undef GetCharABCWidths

extern bool s_bSupportsUnicode;

//-----------------------------------------------------------------------------
// Purpose: Win32 Font
//-----------------------------------------------------------------------------
class CWin32Font
{
private:
	struct abc_t
	{
		short b;
		char a;
		char c;
	};

	struct abc_cache_t
	{
		wchar_t wch;
		abc_t abc;
	};

public:
	CWin32Font(void);
	~CWin32Font(void);

	bool Create(const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	void ApplyRotaryEffectToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, byte* rgba);

	void ApplyScanlineEffectToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, byte* rgba);

	void ApplyDropShadowToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, int charWide, int charTall, unsigned char* rgba);

	void GetBlurValueForPixel(unsigned char* src, int blur, float* gaussianDistribution, int srcX, int srcY, int srcWide, int srcTall, unsigned char* dest);

	void ApplyOutlineToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, int charWide, int charTall, unsigned char* rgba);

	void ApplyGaussianBlurToTexture(int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, byte* rgba);

	void GetCharABCWidths(int ch, int& a, int& b, int& c);

	void GetCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, unsigned char* rgba);

	bool IsEqualTo(const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags);

	bool IsValid(void)
	{
		return m_szName[0] != '\0';
	}

	const char* GetName(void)
	{
		return m_szName;
	}

	int GetHeight(void)
	{
		return m_iHeight;
	}

	int GetAscent(void)
	{
		return m_iAscent;
	}

	int GetMaxCharWidth(void)
	{
		return m_iMaxCharWidth;
	}

	int GetFlags(void)
	{
		return m_iFlags;
	}

	bool GetUnderlined(void)
	{
		return m_bUnderlined;
	}

private:
	static bool ExtendedABCWidthsCacheLessFunc(const abc_cache_t& lhs, const abc_cache_t& rhs);

private:
	char m_szName[32];
	int m_iTall;
	int m_iWeight;
	int m_iFlags;

	bool m_bAntiAliased;
	bool m_bRotary;
	bool m_bAdditive;
	int m_iDropShadowOffset;
	int m_iOutlineSize;
	bool m_bUnderlined;
	int m_iHeight;
	int m_iMaxCharWidth;
	int m_iAscent;

	enum
	{
		ABCWIDTHS_CACHE_SIZE = 256
	};
	abc_t m_ABCWidthsCache[ABCWIDTHS_CACHE_SIZE];

	CUtlRBTree<abc_cache_t> m_ExtendedABCWidthsCache;

	HFONT m_hFont;
	HDC m_hDC;
	HBITMAP m_hDIB;
	int m_rgiBitmapSize[2];
	unsigned char* m_pBuf; // pointer to buffer for use when generated bitmap versions of a texture

	int m_iScanLines;
	int m_iBlur;
	float* m_pGaussianDistribution;
};

#endif // WIN32FONT_H
