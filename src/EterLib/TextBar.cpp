#include "StdAfx.h"
#include "TextBar.h"
#include "EterLib/Util.h"

#include <utf8.h>

void CTextBar::__SetFont(int fontSize, bool isBold)
{
	LOGFONTW logFont{};

	logFont.lfHeight = fontSize;
	logFont.lfEscapement = 0;
	logFont.lfOrientation = 0;
	logFont.lfWeight = isBold ? FW_BOLD : FW_NORMAL;
	logFont.lfItalic = FALSE;
	logFont.lfUnderline = FALSE;
	logFont.lfStrikeOut = FALSE;
	logFont.lfCharSet = DEFAULT_CHARSET;
	logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logFont.lfQuality = ANTIALIASED_QUALITY;
	logFont.lfPitchAndFamily = DEFAULT_PITCH;
	wcscpy_s(logFont.lfFaceName, LF_FACESIZE, L"Tahoma");

	m_hFont = CreateFontIndirect(&logFont);

	HDC hdc = m_dib.GetDCHandle();
	m_hOldFont = (HFONT)SelectObject(hdc, m_hFont);
}

void CTextBar::SetTextColor(int r, int g, int b)
{
	HDC hDC = m_dib.GetDCHandle();
	::SetTextColor(hDC, RGB(r, g, b));
}

void CTextBar::GetTextExtent(const char* c_szText, SIZE* p_size)
{
	if (!c_szText || !p_size)
	{
		if (p_size)
		{
			p_size->cx = 0;
			p_size->cy = 0;
		}
		return;
	}

	HDC hDC = m_dib.GetDCHandle();

	// UTF-8 → UTF-16
	std::wstring wText = Utf8ToWide(c_szText);
	GetTextExtentPoint32W(hDC, wText.c_str(), static_cast<int>(wText.length()), p_size);
}

void CTextBar::TextOut(int ix, int iy, const char * c_szText)
{
	m_dib.TextOut(ix, iy, c_szText);
	Invalidate();
}

void CTextBar::OnCreate()
{
	m_dib.SetBkMode(TRANSPARENT);

	__SetFont(m_fontSize, m_isBold);
}

CTextBar::CTextBar(int fontSize, bool isBold)
{
	m_hOldFont = NULL;
	m_fontSize = fontSize;
	m_isBold = isBold;
}

CTextBar::~CTextBar()
{
	HDC hdc = m_dib.GetDCHandle();
	SelectObject(hdc, m_hOldFont);
}
