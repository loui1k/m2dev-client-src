#include "StdAfx.h"
#include "GrpTextInstance.h"
#include "StateManager.h"
#include "IME.h"
#include "TextTag.h"
#include "EterBase/Utils.h"
#include "EterLocale/Arabic.h"

#include <unordered_map>
#include <utf8.h>

// Forward declaration to avoid header conflicts
extern bool IsRTL();

const float c_fFontFeather = 0.5f;

CDynamicPool<CGraphicTextInstance> CGraphicTextInstance::ms_kPool;

static int gs_mx = 0;
static int gs_my = 0;

static std::wstring gs_hyperlinkText;

void CGraphicTextInstance::Hyperlink_UpdateMousePos(int x, int y)
{
	gs_mx = x;
	gs_my = y;
	gs_hyperlinkText = L"";
}

int CGraphicTextInstance::Hyperlink_GetText(char* buf, int len)
{
	if (gs_hyperlinkText.empty())
		return 0;

	int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, gs_hyperlinkText.c_str(), (int)gs_hyperlinkText.length(), buf, len, nullptr, nullptr);

	return (written > 0) ? written : 0;
}

int CGraphicTextInstance::__DrawCharacter(CGraphicFontTexture * pFontTexture, wchar_t text, DWORD dwColor)
{
	CGraphicFontTexture::TCharacterInfomation* pInsCharInfo = pFontTexture->GetCharacterInfomation(text);

	if (pInsCharInfo)
	{
		m_dwColorInfoVector.push_back(dwColor);
		m_pCharInfoVector.push_back(pInsCharInfo);

		m_textWidth += pInsCharInfo->advance;
		m_textHeight = std::max((WORD)pInsCharInfo->height, m_textHeight);
		return pInsCharInfo->advance;
	}

	return 0;
}

void CGraphicTextInstance::__GetTextPos(DWORD index, float* x, float* y)
{
	index = std::min((size_t)index, m_pCharInfoVector.size());

	float sx = 0;
	float sy = 0;
	float fFontMaxHeight = 0;

	for(DWORD i=0; i<index; ++i)
	{
		if (sx+float(m_pCharInfoVector[i]->width) > m_fLimitWidth)
		{
			sx = 0;
			sy += fFontMaxHeight;
		}

		sx += float(m_pCharInfoVector[i]->advance);
		fFontMaxHeight = std::max(float(m_pCharInfoVector[i]->height), fFontMaxHeight);
	}

	*x = sx;
	*y = sy;
}

void CGraphicTextInstance::Update()
{
	if (m_isUpdate)
		return;

	// Get space height first for empty text cursor rendering
	int spaceHeight = 12; // default fallback
	if (!m_roText.IsNull() && !m_roText->IsEmpty())
	{
		CGraphicFontTexture* pFontTexture = m_roText->GetFontTexturePointer();
		if (pFontTexture)
		{
			CGraphicFontTexture::TCharacterInfomation* pSpaceInfo = pFontTexture->GetCharacterInfomation(L' ');
			spaceHeight = pSpaceInfo ? pSpaceInfo->height : 12;
		}
	}

	auto ResetState = [&, spaceHeight]()
		{
			m_pCharInfoVector.clear();
			m_dwColorInfoVector.clear();
			m_hyperlinkVector.clear();
			m_logicalToVisualPos.clear();
			m_visualToLogicalPos.clear();
			m_textWidth = 0;
			m_textHeight = spaceHeight; // Use space height instead of 0 for cursor rendering
			m_computedRTL = (m_direction == ETextDirection::RTL);
			m_isUpdate = true;
		};

	if (m_roText.IsNull() || m_roText->IsEmpty())
	{
		ResetState();
		return;
	}

	CGraphicFontTexture* pFontTexture = m_roText->GetFontTexturePointer();
	if (!pFontTexture)
	{
		ResetState();
		return;
	}

	m_pCharInfoVector.clear();
	m_dwColorInfoVector.clear();
	m_hyperlinkVector.clear();

	m_textWidth = 0;
	m_textHeight = spaceHeight;

	const char* utf8 = m_stText.c_str();
	const int utf8Len = (int)m_stText.size();
	const DWORD defaultColor = m_dwTextColor;

	// UTF-8 -> UTF-16 conversion - reserve enough space to avoid reallocation
	std::vector<wchar_t> wTextBuf((size_t)utf8Len + 1u, 0);
	const int wTextLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, utf8Len, wTextBuf.data(), (int)wTextBuf.size());
	if (wTextLen <= 0)
	{
		ResetState();
		return;
	}


	// Detect user-typed text direction (skip hyperlink and color tags)
	// Used to determine segment order
	bool userTextIsRTL = false;
	bool foundUserText = false;
	{
		int hyperlinkStep = 0; // 0 = normal, 1 = in metadata (hidden), 2 = in visible text
		const int wTextLenMinusOne = wTextLen - 1;

		for (int i = 0; i < wTextLen; ++i)
		{
			// Check for tags (cache bounds check)
			if (i < wTextLenMinusOne && wTextBuf[i] == L'|')
			{
				if (wTextBuf[i + 1] == L'H')
				{
					hyperlinkStep = 1; // Start metadata
					++i;
					continue;
				}
				else if (wTextBuf[i + 1] == L'h')
				{
					if (hyperlinkStep == 1)
						hyperlinkStep = 2; // End metadata, start visible
					else if (hyperlinkStep == 2)
						hyperlinkStep = 0; // End visible
					++i;
					continue;
				}
				else if (wTextBuf[i + 1] == L'c' && i + 10 <= wTextLen)
				{
					// Color tag |cFFFFFFFF - skip 10 characters
					i += 9; // +1 from loop increment = 10 total
					continue;
				}
				else if (wTextBuf[i + 1] == L'r')
				{
					// Color end tag |r - skip
					++i;
					continue;
				}
			}

			// Only check user-typed text (step 0 = normal text)
			// SKIP hyperlink visible text (step 2) to prevent hyperlink language from affecting direction
			if (hyperlinkStep == 0)
			{
				if (IsRTLCodepoint(wTextBuf[i]))
				{
					userTextIsRTL = true;
					foundUserText = true;
					break;
				}
				if (IsStrongAlpha(wTextBuf[i]))
				{
					userTextIsRTL = false;
					foundUserText = true;
					break;
				}
			}
		}
	}

	// Base direction for BiDi algorithm (for non-hyperlink text reordering)
	const bool baseRTL =
		(m_direction == ETextDirection::RTL) ? true :
		(m_direction == ETextDirection::LTR) ? false :
		userTextIsRTL;

	// Computed direction for rendering and alignment
	// Always use baseRTL to respect the UI direction setting
	// In RTL UI, all text (input and display) should use RTL alignment
	m_computedRTL = baseRTL;

	// Secret: draw '*' but keep direction
	if (m_isSecret)
	{
		for (int i = 0; i < wTextLen; ++i)
			__DrawCharacter(pFontTexture, L'*', defaultColor);

		pFontTexture->UpdateTexture();
		m_isUpdate = true;
		return;
	}

	const bool hasTags = (std::find(wTextBuf.begin(), wTextBuf.begin() + wTextLen, L'|') != (wTextBuf.begin() + wTextLen));

	// ========================================================================
	// Case 1: No tags - Simple BiDi reordering
	// ========================================================================
	if (!hasTags)
	{
		// Build identity mapping (logical == visual for tagless text)
		const size_t mappingSize = (size_t)wTextLen + 1;
		m_logicalToVisualPos.resize(mappingSize);
		m_visualToLogicalPos.resize(mappingSize);
		for (int i = 0; i <= wTextLen; ++i)
		{
			m_logicalToVisualPos[i] = i;
			m_visualToLogicalPos[i] = i;
		}

		// Check for RTL characters and chat message format in single pass
		bool hasRTL = false;
		bool isChatMessage = false;
		const wchar_t* wTextPtr = wTextBuf.data();

		for (int i = 0; i < wTextLen; ++i)
		{
			if (!hasRTL && IsRTLCodepoint(wTextPtr[i]))
				hasRTL = true;

			if (!isChatMessage && i < wTextLen - 2 &&
			    wTextPtr[i] == L' ' && wTextPtr[i + 1] == L':' && wTextPtr[i + 2] == L' ')
				isChatMessage = true;

			// Early exit if both found
			if (hasRTL && isChatMessage)
				break;
		}

		// Apply BiDi if text contains RTL OR is a chat message in RTL UI
		// Skip BiDi for regular input text like :)) in RTL UI
		if (hasRTL || (baseRTL && isChatMessage))
		{
			std::vector<wchar_t> visual = BuildVisualBidiText_Tagless(wTextBuf.data(), wTextLen, baseRTL);
			for (size_t i = 0; i < visual.size(); ++i)
				__DrawCharacter(pFontTexture, visual[i], defaultColor);
		}
		else
		{
			// Pure LTR text or non-chat input - no BiDi processing
			for (int i = 0; i < wTextLen; ++i)
				__DrawCharacter(pFontTexture, wTextBuf[i], defaultColor);
		}
	}
	// ========================================================================
	// Case 2: Has tags - Parse tags and apply BiDi to segments
	// ========================================================================
	else
	{
		// Check if text contains RTL characters (cache pointer for performance)
		bool hasRTL = false;
		const wchar_t* wTextPtr = wTextBuf.data();
		for (int i = 0; i < wTextLen; ++i)
		{
			if (IsRTLCodepoint(wTextPtr[i]))
			{
				hasRTL = true;
				break;
			}
		}
		struct TVisChar
		{
			wchar_t ch;
			DWORD color;
			int linkIndex; // -1 = none, otherwise index into linkTargets
			int logicalPos;  // logical index in original wTextBuf (includes tags)
		};

		auto ReorderTaggedWithBidi = [&](std::vector<TVisChar>& vis, bool forceRTL)
		{
			if (vis.empty())
				return;

			// Extract only characters
			std::vector<wchar_t> buf;
			buf.reserve(vis.size());
			for (const auto& vc : vis)
				buf.push_back(vc.ch);

			// Use the exact same BiDi engine as tagless text
			std::vector<wchar_t> visual = BuildVisualBidiText_Tagless(buf.data(), (int)buf.size(), forceRTL);

			// If size differs (rare, but can happen with Arabic shaping expansion),
			// do a safe best-effort resize while preserving style.
			if ((int)visual.size() != (int)vis.size())
			{
				// Keep style from nearest original character
				std::vector<TVisChar> resized;
				resized.reserve(visual.size());

				if (vis.empty())
					return;

				for (size_t i = 0; i < visual.size(); ++i)
				{
					size_t src = (i < vis.size()) ? i : (vis.size() - 1);
					TVisChar tmp = vis[src];
					tmp.ch = visual[i];
					resized.push_back(tmp);
				}
				vis.swap(resized);
				return;
			}

			// Same size: just write back characters, keep color + linkIndex intact
			for (size_t i = 0; i < vis.size(); ++i)
				vis[i].ch = visual[i];
		};

		DWORD curColor = defaultColor;

		// hyperlinkStep: 0=none, 1=collecting target after |H, 2=visible section between |h and |h
		int hyperlinkStep = 0;
		std::wstring hyperlinkTarget;
		hyperlinkTarget.reserve(64); // Reserve typical hyperlink target size
		int activeLinkIndex = -1;

		std::vector<std::wstring> linkTargets; // linkTargets[i] is target text for link i
		linkTargets.reserve(4); // Reserve space for typical number of links

		std::vector<TVisChar> logicalVis;
		logicalVis.reserve((size_t)wTextLen); // Reserve max possible size

		// Build logical->visual position mapping (reserve to avoid reallocation)
		const size_t mappingSize = (size_t)wTextLen + 1;
		m_logicalToVisualPos.resize(mappingSize, 0);

		// ====================================================================
		// PHASE 1: Parse tags and collect visible characters
		// ====================================================================
		int tagLen = 1;
		std::wstring tagExtra;

		for (int i = 0; i < wTextLen; )
		{
			m_logicalToVisualPos[i] = (int)logicalVis.size();

			tagExtra.clear();
			int ret = GetTextTag(&wTextBuf[i], wTextLen - i, tagLen, tagExtra);
			if (tagLen <= 0) tagLen = 1;

			if (ret == TEXT_TAG_PLAIN)
			{
				wchar_t ch = wTextBuf[i];

				if (hyperlinkStep == 1)
				{
					// Collect hyperlink target text between |H and first |h
					hyperlinkTarget.push_back(ch);
				}
				else
				{
					// Regular visible character
					logicalVis.push_back(TVisChar{ ch, curColor, activeLinkIndex, i });
				}

				i += 1;
				continue;
			}

			// Tag handling
			if (ret == TEXT_TAG_COLOR)
			{
				curColor = htoi(tagExtra.c_str(), 8);
			}
			else if (ret == TEXT_TAG_RESTORE_COLOR)
			{
				curColor = defaultColor;
			}
			else if (ret == TEXT_TAG_HYPERLINK_START)
			{
				hyperlinkStep = 1;
				hyperlinkTarget.clear();
				activeLinkIndex = -1;
			}
			else if (ret == TEXT_TAG_HYPERLINK_END)
			{
				if (hyperlinkStep == 1)
				{
					// End metadata => start visible section
					hyperlinkStep = 2;

					linkTargets.push_back(hyperlinkTarget);
					activeLinkIndex = (int)linkTargets.size() - 1;
				}
				else if (hyperlinkStep == 2)
				{
					// End visible section
					hyperlinkStep = 0;
					activeLinkIndex = -1;
					hyperlinkTarget.clear();
				}
			}

			i += tagLen;
		}

		// ====================================================================
		// PHASE 2: Apply BiDi to hyperlinks (if RTL text or RTL UI)
		// ====================================================================
		if (hasRTL || baseRTL)
		{
			// Collect all hyperlink ranges (reserve typical count)
			struct LinkRange { int start; int end; int linkIdx; };
			std::vector<LinkRange> linkRanges;
			linkRanges.reserve(linkTargets.size());

			int currentLink = -1;
			int linkStart = -1;
			const int logicalVisCount = (int)logicalVis.size();

			for (int i = 0; i <= logicalVisCount; ++i)
			{
				const int linkIdx = (i < logicalVisCount) ? logicalVis[i].linkIndex : -1;

				if (linkIdx != currentLink)
				{
					if (currentLink >= 0 && linkStart >= 0)
					{
						linkRanges.push_back({linkStart, i, currentLink});
					}

					currentLink = linkIdx;
					linkStart = (currentLink >= 0) ? i : -1;
				}
			}

			// Process hyperlinks in reverse order to avoid index shifting
			const int numRanges = (int)linkRanges.size();
			for (int rangeIdx = numRanges - 1; rangeIdx >= 0; --rangeIdx)
			{
				const LinkRange& range = linkRanges[rangeIdx];
				const int linkStart = range.start;
				const int linkEnd = range.end;
				const int linkLength = linkEnd - linkStart;

				// Extract hyperlink text (pre-reserve exact size)
				std::vector<wchar_t> linkBuf;
				linkBuf.reserve(linkLength);
				for (int j = linkStart; j < linkEnd; ++j)
					linkBuf.push_back(logicalVis[j].ch);

				// Apply BiDi with LTR base direction (hyperlinks use LTR structure like [+9 item])
				std::vector<wchar_t> linkVisual = BuildVisualBidiText_Tagless(linkBuf.data(), (int)linkBuf.size(), false);

				// Normalize brackets and enhancement markers
				const int linkVisualSize = (int)linkVisual.size();
				if (linkVisualSize > 0)
				{
					// Find first '[' and first ']' (cache size)
					int openBracket = -1, closeBracket = -1;
					for (int j = 0; j < linkVisualSize; ++j)
					{
						if (linkVisual[j] == L'[' && openBracket < 0) openBracket = j;
						if (linkVisual[j] == L']' && closeBracket < 0) closeBracket = j;
					}

					// Case 1: Brackets are reversed "]text[" => "[text]"
					if (closeBracket >= 0 && openBracket > closeBracket)
					{
						std::vector<wchar_t> normalized;
						normalized.reserve(linkVisual.size());

						// Rebuild: [ + (before ]) + (between ] and [) + (after [) + ]
						normalized.push_back(L'[');

						for (int j = 0; j < closeBracket; ++j)
							normalized.push_back(linkVisual[j]);

						for (int j = closeBracket + 1; j < openBracket; ++j)
							normalized.push_back(linkVisual[j]);

						for (int j = openBracket + 1; j < (int)linkVisual.size(); ++j)
							normalized.push_back(linkVisual[j]);

						normalized.push_back(L']');

						linkVisual = normalized;
						openBracket = 0;
						closeBracket = (int)linkVisual.size() - 1;
					}

					// Case 2: Normal brackets "[...]" - check for normalization
					if (openBracket >= 0 && closeBracket > openBracket)
					{
						int pos = openBracket + 1;

						// Skip leading spaces inside brackets
						while (pos < closeBracket && linkVisual[pos] == L' ')
						{
							linkVisual.erase(linkVisual.begin() + pos);
							closeBracket--;
						}

						// Check for "+<digits>" pattern and reverse to "<digits>+"
						if (pos < closeBracket && linkVisual[pos] == L'+')
						{
							int digitStart = pos + 1;
							int digitEnd = digitStart;

							while (digitEnd < closeBracket && (linkVisual[digitEnd] >= L'0' && linkVisual[digitEnd] <= L'9'))
								digitEnd++;

							if (digitEnd > digitStart)
							{
								wchar_t plus = L'+';
								for (int k = pos; k < digitEnd - 1; ++k)
									linkVisual[k] = linkVisual[k + 1];
								linkVisual[digitEnd - 1] = plus;
							}
						}
					}
				}

				// Write back - handle size changes by erasing/inserting
				const int originalSize = linkLength;
				const int newSize = (int)linkVisual.size();
				const int sizeDiff = newSize - originalSize;

				// Replace existing characters (cache min for performance)
				const int copyCount = (std::min)(originalSize, newSize);
				for (int j = 0; j < copyCount; ++j)
					logicalVis[linkStart + j].ch = linkVisual[j];

				if (sizeDiff < 0)
				{
					// Shrunk - remove extra characters
					logicalVis.erase(logicalVis.begin() + linkStart + newSize,
					                 logicalVis.begin() + linkStart + originalSize);
				}
				else if (sizeDiff > 0)
				{
					// Grew - insert new characters
					TVisChar templateChar = logicalVis[linkStart];
					templateChar.logicalPos = logicalVis[linkStart].logicalPos;
					for (int j = originalSize; j < newSize; ++j)
					{
						templateChar.ch = linkVisual[j];
						logicalVis.insert(logicalVis.begin() + linkStart + j, templateChar);
					}
				}
			}
		}

		// Apply BiDi to non-hyperlink segments and reorder segments for RTL UI
		if (hasRTL || baseRTL)
		{
			// Split text into hyperlink and non-hyperlink segments (reserve typical count)
			const size_t estimatedSegments = linkTargets.size() * 2 + 1;
			std::vector<std::vector<TVisChar>> segments;
			segments.reserve(estimatedSegments); // Estimate: links + text between

			std::vector<bool> isHyperlink; // true if segment is a hyperlink
			isHyperlink.reserve(estimatedSegments);

			int segStart = 0;
			int currentLinkIdx = (logicalVis.empty() ? -1 : logicalVis[0].linkIndex);
			const int logicalVisSize2 = (int)logicalVis.size();

			for (int i = 1; i <= logicalVisSize2; ++i)
			{
				const int linkIdx = (i < logicalVisSize2) ? logicalVis[i].linkIndex : -1;

				if (linkIdx != currentLinkIdx)
				{
					// Segment boundary
					std::vector<TVisChar> seg(logicalVis.begin() + segStart, logicalVis.begin() + i);
					segments.push_back(seg);
					isHyperlink.push_back(currentLinkIdx >= 0);

					segStart = i;
					currentLinkIdx = linkIdx;
				}
			}

			// Apply BiDi to non-hyperlink segments only (cache segment count)
			const size_t numSegments = segments.size();
			for (size_t s = 0; s < numSegments; ++s)
			{
				if (!isHyperlink[s])
					ReorderTaggedWithBidi(segments[s], baseRTL);
			}

			// Rebuild text from segments (reverse order for RTL UI)
			logicalVis.clear();
			logicalVis.reserve(logicalVisSize2); // Reserve original size

			if (baseRTL)
			{
				// RTL UI - reverse segments for right-to-left reading
				for (int s = (int)numSegments - 1; s >= 0; --s)
				{
					logicalVis.insert(logicalVis.end(), segments[s].begin(), segments[s].end());
				}
			}
			else
			{
				// LTR UI - keep original segment order
				for (size_t s = 0; s < numSegments; ++s)
				{
					logicalVis.insert(logicalVis.end(), segments[s].begin(), segments[s].end());
				}
			}
		}

		// ====================================================================
		// FINAL: Rebuild visual<->logical mapping AFTER all BiDi/tag reordering
		// ====================================================================

		m_visualToLogicalPos.clear();
		m_logicalToVisualPos.clear();

		// logical positions refer to indices in wTextBuf (tagged string)
		m_logicalToVisualPos.resize((size_t)wTextLen + 1, -1);
		m_visualToLogicalPos.resize((size_t)logicalVis.size() + 1, wTextLen);

		// Fill visual->logical from stored glyph origin
		for (size_t v = 0; v < logicalVis.size(); ++v)
		{
			int lp = logicalVis[v].logicalPos;
			if (lp < 0) lp = 0;
			if (lp > wTextLen) lp = wTextLen;

			m_visualToLogicalPos[v] = lp;

			// For logical->visual, keep the first visual position that maps to lp
			if (m_logicalToVisualPos[(size_t)lp] < 0)
				m_logicalToVisualPos[(size_t)lp] = (int)v;
		}

		// End positions
		m_visualToLogicalPos[logicalVis.size()] = wTextLen;
		m_logicalToVisualPos[(size_t)wTextLen] = (int)logicalVis.size();

		// Fill gaps in logical->visual so cursor movement doesn't break on tag-only regions
		int last = 0;
		for (int i = 0; i <= wTextLen; ++i)
		{
			if (m_logicalToVisualPos[(size_t)i] < 0)
				m_logicalToVisualPos[(size_t)i] = last;
			else
				last = m_logicalToVisualPos[(size_t)i];
		}

		// ====================================================================
		// PHASE 3: Render and build hyperlink ranges
		// ====================================================================
		m_hyperlinkVector.clear();
		m_hyperlinkVector.reserve(linkTargets.size()); // Reserve for known hyperlinks

		int x = 0;
		int currentLink = -1;
		SHyperlink curLinkRange{};
		curLinkRange.sx = 0;
		curLinkRange.ex = 0;

		// Cache size for loop (avoid repeated size() calls)
		const size_t logicalVisRenderSize = logicalVis.size();
		for (size_t idx = 0; idx < logicalVisRenderSize; ++idx)
		{
			const TVisChar& vc = logicalVis[idx];
			const int charWidth = __DrawCharacter(pFontTexture, vc.ch, vc.color);

			// Hyperlink range tracking
			const int linkIdx = vc.linkIndex;

			if (linkIdx != currentLink)
			{
				// Close previous hyperlink
				if (currentLink >= 0)
				{
					curLinkRange.text = linkTargets[(size_t)currentLink];
					m_hyperlinkVector.push_back(curLinkRange);
				}

				// Open new hyperlink
				currentLink = linkIdx;
				if (currentLink >= 0)
				{
					curLinkRange = SHyperlink{};
					curLinkRange.sx = (short)x;
					curLinkRange.ex = (short)x;
				}
			}

			if (currentLink >= 0)
			{
				curLinkRange.ex = (short)(curLinkRange.ex + charWidth);
			}

			x += charWidth;
		}

		// Close last hyperlink
		if (currentLink >= 0)
		{
			curLinkRange.text = linkTargets[(size_t)currentLink];
			m_hyperlinkVector.push_back(curLinkRange);
		}
	}

	pFontTexture->UpdateTexture();
	m_isUpdate = true;
}

void CGraphicTextInstance::Render(RECT * pClipRect)
{
	if (!m_isUpdate)
		return;

	CGraphicText* pkText=m_roText.GetPointer();
	if (!pkText)
		return;

	CGraphicFontTexture* pFontTexture = pkText->GetFontTexturePointer();
	if (!pFontTexture)
		return;

	float fStanX = m_v3Position.x;
	float fStanY = m_v3Position.y + 1.0f;

	// Use the computed direction for this text instance, not the global UI direction
	if (m_computedRTL)
	{
		switch (m_hAlign)
		{
			case HORIZONTAL_ALIGN_LEFT:
				fStanX -= m_textWidth;
				break;

			case HORIZONTAL_ALIGN_CENTER:
				fStanX -= float(m_textWidth / 2);
				break;
		}
	}
	else
	{
		switch (m_hAlign)
		{
			case HORIZONTAL_ALIGN_RIGHT:
				fStanX -= m_textWidth;
				break;

			case HORIZONTAL_ALIGN_CENTER:
				fStanX -= float(m_textWidth / 2);
				break;
		}
	}

	switch (m_vAlign)
	{
		case VERTICAL_ALIGN_BOTTOM:
			fStanY -= m_textHeight;
			break;

		case VERTICAL_ALIGN_CENTER:
			fStanY -= float(m_textHeight) / 2.0f;
			break;
	}

	static std::unordered_map<LPDIRECT3DTEXTURE9, std::vector<SVertex>> s_vtxBatches;
	for (auto& [pTexture, vtxBatch] : s_vtxBatches) {
		vtxBatch.clear();
	}

	STATEMANAGER.SaveRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	STATEMANAGER.SaveRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	DWORD dwFogEnable = STATEMANAGER.GetRenderState(D3DRS_FOGENABLE);
	DWORD dwLighting = STATEMANAGER.GetRenderState(D3DRS_LIGHTING);
	STATEMANAGER.SetRenderState(D3DRS_FOGENABLE, FALSE);
	STATEMANAGER.SetRenderState(D3DRS_LIGHTING, FALSE);

	STATEMANAGER.SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLORARG1,	D3DTA_TEXTURE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLORARG2,	D3DTA_DIFFUSE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_COLOROP,	D3DTOP_MODULATE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAARG1,	D3DTA_TEXTURE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAARG2,	D3DTA_DIFFUSE);
	STATEMANAGER.SetTextureStageState(0, D3DTSS_ALPHAOP,	D3DTOP_MODULATE);

	{
		const float fFontHalfWeight=1.0f;

		float fCurX;
		float fCurY;

		float fFontSx;
		float fFontSy;
		float fFontEx;
		float fFontEy;
		float fFontWidth;
		float fFontHeight;
		float fFontMaxHeight;
		float fFontAdvance;

		SVertex akVertex[4];
		akVertex[0].z=m_v3Position.z;
		akVertex[1].z=m_v3Position.z;
		akVertex[2].z=m_v3Position.z;
		akVertex[3].z=m_v3Position.z;

		CGraphicFontTexture::TCharacterInfomation* pCurCharInfo;

		if (m_isOutline)
		{
			fCurX=fStanX;
			fCurY=fStanY;
			fFontMaxHeight=0.0f;

			CGraphicFontTexture::TPCharacterInfomationVector::iterator i;
			for (i=m_pCharInfoVector.begin(); i!=m_pCharInfoVector.end(); ++i)
			{
				pCurCharInfo = *i;

				fFontWidth=float(pCurCharInfo->width);
				fFontHeight=float(pCurCharInfo->height);
				fFontAdvance=float(pCurCharInfo->advance);

				if ((fCurX+fFontWidth)-m_v3Position.x > m_fLimitWidth)
				{
					if (m_isMultiLine)
					{
						fCurX=fStanX;
						fCurY+=fFontMaxHeight;
					}
					else
					{
						break;
					}
				}

				if (pClipRect)
				{
					if (fCurY <= pClipRect->top)
					{
						fCurX += fFontAdvance;
						continue;
					}
				}

				fFontSx = fCurX - 0.5f;
				fFontSy = fCurY - 0.5f;
				fFontEx = fFontSx + fFontWidth;
				fFontEy = fFontSy + fFontHeight;

				pFontTexture->SelectTexture(pCurCharInfo->index);
				std::vector<SVertex>& vtxBatch = s_vtxBatches[pFontTexture->GetD3DTexture()];

				akVertex[0].u=pCurCharInfo->left;
				akVertex[0].v=pCurCharInfo->top;
				akVertex[1].u=pCurCharInfo->left;
				akVertex[1].v=pCurCharInfo->bottom;
				akVertex[2].u=pCurCharInfo->right;
				akVertex[2].v=pCurCharInfo->top;
				akVertex[3].u=pCurCharInfo->right;
				akVertex[3].v=pCurCharInfo->bottom;

				akVertex[3].color = akVertex[2].color = akVertex[1].color = akVertex[0].color = m_dwOutLineColor;

				float feather = 0.0f; // m_fFontFeather

				akVertex[0].y=fFontSy-feather;
				akVertex[1].y=fFontEy+feather;
				akVertex[2].y=fFontSy-feather;
				akVertex[3].y=fFontEy+feather;

				// 왼
				akVertex[0].x=fFontSx-fFontHalfWeight-feather;
				akVertex[1].x=fFontSx-fFontHalfWeight-feather;
				akVertex[2].x=fFontEx-fFontHalfWeight+feather;
				akVertex[3].x=fFontEx-fFontHalfWeight+feather;

				vtxBatch.insert(vtxBatch.end(), std::begin(akVertex), std::end(akVertex));

				// 오른
				akVertex[0].x=fFontSx+fFontHalfWeight-feather;
				akVertex[1].x=fFontSx+fFontHalfWeight-feather;
				akVertex[2].x=fFontEx+fFontHalfWeight+feather;
				akVertex[3].x=fFontEx+fFontHalfWeight+feather;

				vtxBatch.insert(vtxBatch.end(), std::begin(akVertex), std::end(akVertex));

				akVertex[0].x=fFontSx-feather;
				akVertex[1].x=fFontSx-feather;
				akVertex[2].x=fFontEx+feather;
				akVertex[3].x=fFontEx+feather;

				// 위
				akVertex[0].y=fFontSy-fFontHalfWeight-feather;
				akVertex[1].y=fFontEy-fFontHalfWeight+feather;
				akVertex[2].y=fFontSy-fFontHalfWeight-feather;
				akVertex[3].y=fFontEy-fFontHalfWeight+feather;

				vtxBatch.insert(vtxBatch.end(), std::begin(akVertex), std::end(akVertex));

				// 아래
				akVertex[0].y=fFontSy+fFontHalfWeight-feather;
				akVertex[1].y=fFontEy+fFontHalfWeight+feather;
				akVertex[2].y=fFontSy+fFontHalfWeight-feather;
				akVertex[3].y=fFontEy+fFontHalfWeight+feather;

				vtxBatch.insert(vtxBatch.end(), std::begin(akVertex), std::end(akVertex));

				fCurX += fFontAdvance;
			}
		}

		fCurX=fStanX;
		fCurY=fStanY;
		fFontMaxHeight=0.0f;

		for (int i = 0; i < m_pCharInfoVector.size(); ++i)
		{
			pCurCharInfo = m_pCharInfoVector[i];

			fFontWidth=float(pCurCharInfo->width);
			fFontHeight=float(pCurCharInfo->height);
			fFontMaxHeight=(std::max)(fFontHeight, (float)pCurCharInfo->height);
			fFontAdvance=float(pCurCharInfo->advance);

			if ((fCurX+fFontWidth)-m_v3Position.x > m_fLimitWidth)
			{
				if (m_isMultiLine)
				{
					fCurX=fStanX;
					fCurY+=fFontMaxHeight;
				}
				else
				{
					break;
				}
			}

			if (pClipRect)
			{
				if (fCurY <= pClipRect->top)
				{
					fCurX += fFontAdvance;
					continue;
				}
			}

			fFontSx = fCurX-0.5f;
			fFontSy = fCurY-0.5f;
			fFontEx = fFontSx + fFontWidth;
			fFontEy = fFontSy + fFontHeight;

			pFontTexture->SelectTexture(pCurCharInfo->index);
			std::vector<SVertex>& vtxBatch = s_vtxBatches[pFontTexture->GetD3DTexture()];

			akVertex[0].x=fFontSx;
			akVertex[0].y=fFontSy;
			akVertex[0].u=pCurCharInfo->left;
			akVertex[0].v=pCurCharInfo->top;

			akVertex[1].x=fFontSx;
			akVertex[1].y=fFontEy;
			akVertex[1].u=pCurCharInfo->left;
			akVertex[1].v=pCurCharInfo->bottom;

			akVertex[2].x=fFontEx;
			akVertex[2].y=fFontSy;
			akVertex[2].u=pCurCharInfo->right;
			akVertex[2].v=pCurCharInfo->top;

			akVertex[3].x=fFontEx;
			akVertex[3].y=fFontEy;
			akVertex[3].u=pCurCharInfo->right;
			akVertex[3].v=pCurCharInfo->bottom;

			akVertex[0].color = akVertex[1].color = akVertex[2].color = akVertex[3].color = m_dwColorInfoVector[i];

			vtxBatch.insert(vtxBatch.end(), std::begin(akVertex), std::end(akVertex));

			fCurX += fFontAdvance;
		}
	}

	// --- Selection background (Ctrl+A / shift-select) ---
	if (m_isCursor && CIME::ms_bCaptureInput)
	{
		int selBegin = CIME::GetSelBegin();
		int selEnd = CIME::GetSelEnd();

		if (selBegin > selEnd) std::swap(selBegin, selEnd);

		if (selBegin != selEnd)
		{
			float sx, sy, ex, ey;

			// Convert logical selection positions to visual positions (handles tags)
			int visualSelBegin = selBegin;
			int visualSelEnd = selEnd;
			if (!m_logicalToVisualPos.empty())
			{
				if (selBegin >= 0 && selBegin < (int)m_logicalToVisualPos.size())
					visualSelBegin = m_logicalToVisualPos[selBegin];
				if (selEnd >= 0 && selEnd < (int)m_logicalToVisualPos.size())
					visualSelEnd = m_logicalToVisualPos[selEnd];
			}

			__GetTextPos(visualSelBegin, &sx, &sy);
			__GetTextPos(visualSelEnd,   &ex, &sy);

			// Handle RTL - use the computed direction for this text instance
			if (m_computedRTL)
			{
				sx += m_v3Position.x - m_textWidth;
				ex += m_v3Position.x - m_textWidth;
			}
			else
			{
				sx += m_v3Position.x;
				ex += m_v3Position.x;
			}

			// Apply vertical alignment
			float top = m_v3Position.y;
			float bot = m_v3Position.y + m_textHeight;

			switch (m_vAlign)
			{
				case VERTICAL_ALIGN_BOTTOM:
					top -= m_textHeight;
					bot -= m_textHeight;
					break;

				case VERTICAL_ALIGN_CENTER:
					top -= float(m_textHeight) / 2.0f;
					bot -= float(m_textHeight) / 2.0f;
					break;
			}

			TPDTVertex vertices[4];
			vertices[0].diffuse = 0x80339CFF;
			vertices[1].diffuse = 0x80339CFF;
			vertices[2].diffuse = 0x80339CFF;
			vertices[3].diffuse = 0x80339CFF;

			vertices[0].position = TPosition(sx, top, 0.0f);
			vertices[1].position = TPosition(ex, top, 0.0f);
			vertices[2].position = TPosition(sx, bot, 0.0f);
			vertices[3].position = TPosition(ex, bot, 0.0f);

			STATEMANAGER.SetTexture(0, NULL);
			CGraphicBase::SetDefaultIndexBuffer(CGraphicBase::DEFAULT_IB_FILL_RECT);
			if (CGraphicBase::SetPDTStream(vertices, 4))
				STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4, 0, 2);
		}
	}

	for (const auto& [pTexture, vtxBatch] : s_vtxBatches) {
		if (vtxBatch.empty())
			continue;

		STATEMANAGER.SetTexture(0, pTexture);

		// Each character is 4 vertices forming a quad (0=TL, 1=BL, 2=TR, 3=BR)
		// We need to convert quads to triangle list format
		// Triangle list needs 6 vertices per quad: v0,v1,v2, v2,v1,v3

		size_t numQuads = vtxBatch.size() / 4;
		std::vector<SVertex> triangleVerts;
		triangleVerts.reserve(numQuads * 6);

		for (size_t i = 0; i < numQuads; ++i)
		{
			size_t baseIdx = i * 4;
			const SVertex& v0 = vtxBatch[baseIdx + 0]; // TL
			const SVertex& v1 = vtxBatch[baseIdx + 1]; // BL
			const SVertex& v2 = vtxBatch[baseIdx + 2]; // TR
			const SVertex& v3 = vtxBatch[baseIdx + 3]; // BR

			// First triangle: TL, BL, TR
			triangleVerts.push_back(v0);
			triangleVerts.push_back(v1);
			triangleVerts.push_back(v2);

			// Second triangle: TR, BL, BR
			triangleVerts.push_back(v2);
			triangleVerts.push_back(v1);
			triangleVerts.push_back(v3);
		}

		if (!triangleVerts.empty())
		{
			STATEMANAGER.DrawPrimitiveUP(D3DPT_TRIANGLELIST, triangleVerts.size() / 3, triangleVerts.data(), sizeof(SVertex));
		}
	}

	if (m_isCursor)
	{
		// Draw Cursor
		float sx, sy, ex, ey;
		TDiffuse diffuse;

		int curpos = CIME::GetCurPos();
		int compend = curpos + CIME::GetCompLen();

		// Convert logical cursor position to visual position (handles tags)
		int visualCurpos = curpos;
		int visualCompend = compend;
		if (!m_logicalToVisualPos.empty())
		{
			if (curpos >= 0 && curpos < (int)m_logicalToVisualPos.size())
				visualCurpos = m_logicalToVisualPos[curpos];
			if (compend >= 0 && compend < (int)m_logicalToVisualPos.size())
				visualCompend = m_logicalToVisualPos[compend];
		}

		__GetTextPos(visualCurpos, &sx, &sy);

		// If Composition
		if(visualCurpos<visualCompend)
		{
			diffuse = 0x7fffffff;
			__GetTextPos(visualCompend, &ex, &sy);
		}
		else
		{
			diffuse = 0xffffffff;
			ex = sx + 2;
		}

		// FOR_ARABIC_ALIGN
		// Use the computed direction for this text instance, not the global UI direction
		if (m_computedRTL)
		{
			sx += m_v3Position.x - m_textWidth;
			ex += m_v3Position.x - m_textWidth;
			sy += m_v3Position.y;
		}
		else
		{
			sx += m_v3Position.x;
			sy += m_v3Position.y;
			ex += m_v3Position.x;
		}

		// Apply vertical alignment adjustment BEFORE calculating ey
		switch (m_vAlign)
		{
			case VERTICAL_ALIGN_BOTTOM:
				sy -= m_textHeight;
				break;

			case VERTICAL_ALIGN_CENTER:
				sy -= float(m_textHeight) / 2.0f;
				break;
		}

		// NOW calculate ey after sy has been adjusted
		ey = sy + m_textHeight;

		TPDTVertex vertices[4];
		vertices[0].diffuse = diffuse;
		vertices[1].diffuse = diffuse;
		vertices[2].diffuse = diffuse;
		vertices[3].diffuse = diffuse;
		vertices[0].position = TPosition(sx, sy, 0.0f);
		vertices[1].position = TPosition(ex, sy, 0.0f);
		vertices[2].position = TPosition(sx, ey, 0.0f);
		vertices[3].position = TPosition(ex, ey, 0.0f);

		STATEMANAGER.SetTexture(0, NULL);

		CGraphicBase::SetDefaultIndexBuffer(CGraphicBase::DEFAULT_IB_FILL_RECT);
		if (CGraphicBase::SetPDTStream(vertices, 4))
			STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

		int ulbegin = CIME::GetULBegin();
		int ulend = CIME::GetULEnd();

		if (ulbegin < ulend)
		{
			__GetTextPos(curpos+ulbegin, &sx, &sy);
			__GetTextPos(curpos+ulend, &ex, &sy);

			sx += m_v3Position.x;
			sy += m_v3Position.y + m_textHeight;
			ex += m_v3Position.x;
			ey = sy + 2;

			vertices[0].diffuse = 0xFFFF0000;
			vertices[1].diffuse = 0xFFFF0000;
			vertices[2].diffuse = 0xFFFF0000;
			vertices[3].diffuse = 0xFFFF0000;
			vertices[0].position = TPosition(sx, sy, 0.0f);
			vertices[1].position = TPosition(ex, sy, 0.0f);
			vertices[2].position = TPosition(sx, ey, 0.0f);
			vertices[3].position = TPosition(ex, ey, 0.0f);

			STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, c_FillRectIndices, D3DFMT_INDEX16, vertices, sizeof(TPDTVertex));
		}
	}

	STATEMANAGER.RestoreRenderState(D3DRS_SRCBLEND);
	STATEMANAGER.RestoreRenderState(D3DRS_DESTBLEND);

	STATEMANAGER.SetRenderState(D3DRS_FOGENABLE, dwFogEnable);
	STATEMANAGER.SetRenderState(D3DRS_LIGHTING, dwLighting);

	if (m_hyperlinkVector.size() != 0)
	{
		// FOR_ARABIC_ALIGN: RTL text is drawn with offset (m_v3Position.x - m_textWidth)
		// Use the computed direction for this text instance, not the global UI direction
		int textOffsetX = m_computedRTL ? (int)(m_v3Position.x - m_textWidth) : (int)m_v3Position.x;

		int lx = gs_mx - textOffsetX;
		int ly = gs_my - (int)m_v3Position.y;

		if (lx >= 0 && ly >= 0 && lx < m_textWidth && ly < m_textHeight)
		{
			std::vector<SHyperlink>::iterator it = m_hyperlinkVector.begin();

			while (it != m_hyperlinkVector.end())
			{
				SHyperlink & link = *it++;
				if (lx >= link.sx && lx < link.ex)
				{
					gs_hyperlinkText = link.text;
					break;
				}
			}
		}
	}
}

void CGraphicTextInstance::CreateSystem(UINT uCapacity)
{
	ms_kPool.Create(uCapacity);
}

void CGraphicTextInstance::DestroySystem()
{
	ms_kPool.Destroy();
}

CGraphicTextInstance* CGraphicTextInstance::New()
{
	return ms_kPool.Alloc();
}

void CGraphicTextInstance::Delete(CGraphicTextInstance* pkInst)
{
	pkInst->Destroy();
	ms_kPool.Free(pkInst);
}

void CGraphicTextInstance::ShowCursor()
{
	m_isCursor = true;
}

void CGraphicTextInstance::HideCursor()
{
	m_isCursor = false;
}

void CGraphicTextInstance::ShowOutLine()
{
	m_isOutline = true;
}

void CGraphicTextInstance::HideOutLine()
{
	m_isOutline = false;
}

void CGraphicTextInstance::SetColor(DWORD color)
{
	if (m_dwTextColor != color)
	{
		for (int i = 0; i < m_pCharInfoVector.size(); ++i)
			if (m_dwColorInfoVector[i] == m_dwTextColor)
				m_dwColorInfoVector[i] = color;

		m_dwTextColor = color;
	}
}

void CGraphicTextInstance::SetColor(float r, float g, float b, float a)
{
	SetColor(D3DXCOLOR(r, g, b, a));
}

void CGraphicTextInstance::SetOutLineColor(DWORD color)
{
	m_dwOutLineColor=color;
}

void CGraphicTextInstance::SetOutLineColor(float r, float g, float b, float a)
{
	m_dwOutLineColor=D3DXCOLOR(r, g, b, a);
}

void CGraphicTextInstance::SetSecret(bool Value)
{
	m_isSecret = Value;
}

void CGraphicTextInstance::SetOutline(bool Value)
{
	m_isOutline = Value;
}

void CGraphicTextInstance::SetFeather(bool Value)
{
	if (Value)
	{
		m_fFontFeather = c_fFontFeather;
	}
	else
	{
		m_fFontFeather = 0.0f;
	}
}

void CGraphicTextInstance::SetMultiLine(bool Value)
{
	m_isMultiLine = Value;
}

void CGraphicTextInstance::SetHorizonalAlign(int hAlign)
{
	m_hAlign = hAlign;
}

void CGraphicTextInstance::SetVerticalAlign(int vAlign)
{
	m_vAlign = vAlign;
}

void CGraphicTextInstance::SetMax(int iMax)
{
	m_iMax = iMax;
}

void CGraphicTextInstance::SetLimitWidth(float fWidth)
{
	m_fLimitWidth = fWidth;
}

void CGraphicTextInstance::SetValueString(const std::string& c_stValue)
{
	if (0 == m_stText.compare(c_stValue))
		return;

	m_stText = c_stValue;
	m_isUpdate = false;
}

void CGraphicTextInstance::SetValue(const char* c_szText, size_t len)
{
	if (0 == m_stText.compare(c_szText))
		return;

	m_stText = c_szText;
	m_isUpdate = false;
}

void CGraphicTextInstance::SetPosition(float fx, float fy, float fz)
{
	m_v3Position.x = fx;
	m_v3Position.y = fy;
	m_v3Position.z = fz;
}

void CGraphicTextInstance::SetTextPointer(CGraphicText* pText)
{
	m_roText = pText;
}

void CGraphicTextInstance::SetTextDirection(ETextDirection dir)
{
	if (m_direction == dir)
		return;

	m_direction = dir;
	m_isUpdate = false;
}

const std::string & CGraphicTextInstance::GetValueStringReference()
{
	return m_stText;
}

WORD CGraphicTextInstance::GetTextLineCount()
{
	CGraphicFontTexture::TCharacterInfomation* pCurCharInfo;
	CGraphicFontTexture::TPCharacterInfomationVector::iterator itor;

	float fx = 0.0f;
	WORD wLineCount = 1;
	for (itor=m_pCharInfoVector.begin(); itor!=m_pCharInfoVector.end(); ++itor)
	{
		pCurCharInfo = *itor;

		float fFontWidth=float(pCurCharInfo->width);
		float fFontAdvance=float(pCurCharInfo->advance);
		//float fFontHeight=float(pCurCharInfo->height);

		// NOTE : 폰트 출력에 Width 제한을 둡니다. - [levites]
		if (fx+fFontWidth > m_fLimitWidth)
		{
			fx = 0.0f;
			++wLineCount;
		}

		fx += fFontAdvance;
	}

	return wLineCount;
}

void CGraphicTextInstance::GetTextSize(int* pRetWidth, int* pRetHeight)
{
	*pRetWidth = m_textWidth;
	*pRetHeight = m_textHeight;
}

int CGraphicTextInstance::PixelPositionToCharacterPosition(int iPixelPosition)
{
	// Clamp to valid range [0, textWidth]
	int adjustedPixelPos = iPixelPosition;
	if (adjustedPixelPos < 0)
		adjustedPixelPos = 0;
	if (adjustedPixelPos > m_textWidth)
		adjustedPixelPos = m_textWidth;

	// RTL: interpret click from right edge of rendered text
	if (m_computedRTL)
		adjustedPixelPos = m_textWidth - adjustedPixelPos;

	int icurPosition = 0;
	int visualPos = -1;

	for (int i = 0; i < (int)m_pCharInfoVector.size(); ++i)
	{
		CGraphicFontTexture::TCharacterInfomation* pCurCharInfo = m_pCharInfoVector[i];

		// Use advance instead of width (width is not reliable for cursor hit-testing)
		int adv = pCurCharInfo->advance;
		if (adv <= 0)
			adv = pCurCharInfo->width;

		icurPosition += adv;

		if (adjustedPixelPos < icurPosition)
		{
			visualPos = i;
			break;
		}
	}

	if (visualPos < 0)
		visualPos = (int)m_pCharInfoVector.size();

	if (!m_visualToLogicalPos.empty() && visualPos >= 0 && visualPos < (int)m_visualToLogicalPos.size())
		return m_visualToLogicalPos[visualPos];

	return visualPos;
}

int CGraphicTextInstance::GetHorizontalAlign()
{
	return m_hAlign;
}

void CGraphicTextInstance::__Initialize()
{
	m_roText = NULL;

	m_hAlign = HORIZONTAL_ALIGN_LEFT;
	m_vAlign = VERTICAL_ALIGN_TOP;

	m_iMax = 0;
	m_fLimitWidth = 1600.0f;

	m_isCursor = false;
	m_isSecret = false;
	m_isMultiLine = false;

	m_isOutline = false;
	m_fFontFeather = c_fFontFeather;

	m_isUpdate = false;
	// Use Auto direction for input fields - they should auto-detect based on content
	// Only chat messages should be explicitly set to RTL
	m_direction = ETextDirection::Auto;
	m_computedRTL = false;

	m_textWidth = 0;
	m_textHeight = 0;

	m_v3Position.x = m_v3Position.y = m_v3Position.z = 0.0f;

	m_dwOutLineColor=0xff000000;
}

void CGraphicTextInstance::Destroy()
{
	m_stText="";
	m_pCharInfoVector.clear();
	m_dwColorInfoVector.clear();
	m_hyperlinkVector.clear();
	m_logicalToVisualPos.clear();
	m_visualToLogicalPos.clear();

	__Initialize();
}

CGraphicTextInstance::CGraphicTextInstance()
{
	__Initialize();
}

CGraphicTextInstance::~CGraphicTextInstance()
{
	Destroy();
}
