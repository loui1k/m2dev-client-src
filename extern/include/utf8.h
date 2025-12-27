#pragma once
#include <string>
#include <windows.h>
#include <vector>
#include <algorithm>
#include <cmath>

#include <EterLocale/Arabic.h>

// ============================================================================
// CONFIGURATION CONSTANTS
// ============================================================================

// Maximum text length for security/performance (prevent DoS attacks)
constexpr size_t MAX_TEXT_LENGTH = 65536; // 64KB of text
constexpr size_t MAX_CHAT_TEXT_LENGTH = 4096; // 4KB for chat messages

// Arabic shaping buffer size calculations
constexpr size_t ARABIC_SHAPING_EXPANSION_FACTOR = 2;
constexpr size_t ARABIC_SHAPING_SAFETY_MARGIN = 16;
constexpr size_t ARABIC_SHAPING_EXPANSION_FACTOR_RETRY = 4;
constexpr size_t ARABIC_SHAPING_SAFETY_MARGIN_RETRY = 64;

// ============================================================================
// DEBUG LOGGING (Uncomment to enable BiDi debugging)
// ============================================================================
// #define DEBUG_BIDI

#ifdef DEBUG_BIDI
	#include <cstdio>
	#define BIDI_LOG(fmt, ...) printf("[BiDi] " fmt "\n", __VA_ARGS__)
	#define BIDI_LOG_SIMPLE(msg) printf("[BiDi] %s\n", msg)
#else
	#define BIDI_LOG(fmt, ...) ((void)0)
	#define BIDI_LOG_SIMPLE(msg) ((void)0)
#endif

// ============================================================================
// UNICODE VALIDATION HELPERS
// ============================================================================

// Check if codepoint is a valid Unicode scalar value (not surrogate, not non-character)
static inline bool IsValidUnicodeScalar(wchar_t ch)
{
	// Reject surrogate pairs (UTF-16 encoding artifacts, invalid in UTF-8)
	if (ch >= 0xD800 && ch <= 0xDFFF)
		return false;

	// Reject non-characters (reserved by Unicode standard)
	if ((ch >= 0xFDD0 && ch <= 0xFDEF) || // Arabic Presentation Forms non-chars
	    (ch & 0xFFFE) == 0xFFFE)           // U+FFFE, U+FFFF, etc.
		return false;

	// Accept everything else in BMP (0x0000-0xFFFF)
	return true;
}

// Sanitize a wide string by removing invalid Unicode codepoints
static inline void SanitizeWideString(std::wstring& ws)
{
	ws.erase(std::remove_if(ws.begin(), ws.end(),
		[](wchar_t ch) { return !IsValidUnicodeScalar(ch); }),
		ws.end());
}

// UTF-8 -> UTF-16 (Windows wide)
inline std::wstring Utf8ToWide(const std::string& s)
{
	if (s.empty())
		return L"";

	// Validate size limits (prevent DoS and INT_MAX overflow)
	if (s.size() > MAX_TEXT_LENGTH || s.size() > INT_MAX)
	{
		BIDI_LOG("Utf8ToWide: String too large (%zu bytes)", s.size());
		return L""; // String too large
	}

	int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), nullptr, 0);
	if (wlen <= 0)
	{
		BIDI_LOG("Utf8ToWide: Invalid UTF-8 sequence (error %d)", GetLastError());
		return L"";
	}

	std::wstring out(wlen, L'\0');
	int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), (int)s.size(), out.data(), wlen);
	if (written <= 0 || written != wlen)
	{
		BIDI_LOG("Utf8ToWide: Second conversion failed (written=%d, expected=%d, error=%d)", written, wlen, GetLastError());
		return L""; // Conversion failed unexpectedly
	}

	// Optional: Sanitize to remove invalid Unicode codepoints (surrogates, non-characters)
	// Uncomment if you want strict validation
	// SanitizeWideString(out);

	return out;
}

// Convenience overload for char*
inline std::wstring Utf8ToWide(const char* s)
{
	if (!s || !*s)
		return L"";

	int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, nullptr, 0);
	if (wlen <= 0)
		return L"";

	// wlen includes terminating NUL
	std::wstring out(wlen, L'\0');

	int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, out.data(), wlen);
	if (written <= 0 || written != wlen)
	{
		BIDI_LOG("Utf8ToWide(char*): Conversion failed (written=%d, expected=%d, error=%d)", written, wlen, GetLastError());
		return L"";
	}

	// Drop the terminating NUL from std::wstring length
	if (!out.empty() && out.back() == L'\0')
		out.pop_back();

	// Optional: Sanitize to remove invalid Unicode codepoints
	// SanitizeWideString(out);

	return out;
}

// UTF-16 (Windows wide) -> UTF-8
inline std::string WideToUtf8(const std::wstring& ws)
{
	if (ws.empty())
		return "";

	// Validate size limits (prevent DoS and INT_MAX overflow)
	if (ws.size() > MAX_TEXT_LENGTH || ws.size() > INT_MAX)
		return ""; // String too large

	int len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
	if (len <= 0)
		return "";

	std::string out(len, '\0');
	int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ws.data(), (int)ws.size(), out.data(), len, nullptr, nullptr);
	if (written <= 0 || written != len)
	{
		BIDI_LOG("WideToUtf8: Conversion failed (written=%d, expected=%d, error=%d)", written, len, GetLastError());
		return ""; // Conversion failed
	}
	return out;
}

// Convenience overload for wchar_t*
inline std::string WideToUtf8(const wchar_t* ws)
{
	if (!ws)
		return "";
	return WideToUtf8(std::wstring(ws));
}

// ============================================================================
// RTL & BiDi formatting for RTL UI
// ============================================================================

enum class EBidiDir { LTR, RTL };
enum class ECharDir : unsigned char { Neutral, LTR, RTL };

struct TBidiRun
{
	EBidiDir dir;
	std::vector<wchar_t> text; // logical order
};

static inline bool IsRTLCodepoint(wchar_t ch)
{
	// Directional marks / isolates / embeddings that affect bidi
	if (ch == 0x200F || ch == 0x061C) return true; // RLM, ALM
	if (ch >= 0x202B && ch <= 0x202E) return true; // RLE/RLO/PDF/LRE/LRO
	if (ch >= 0x2066 && ch <= 0x2069) return true; // isolates

	// Hebrew + Arabic blocks (BMP)
	if (ch >= 0x0590 && ch <= 0x08FF) return true;

	// Presentation forms
	if (ch >= 0xFB1D && ch <= 0xFDFF) return true;
	if (ch >= 0xFE70 && ch <= 0xFEFF) return true;

	return false;
}

static inline bool IsStrongAlpha(wchar_t ch)
{
	// Use thread-local cache for BMP (Thread safety)
	thread_local static unsigned char cache[65536] = {}; // 0=unknown, 1=true, 2=false
	unsigned char& v = cache[(unsigned short)ch];
	if (v == 1) return true;
	if (v == 2) return false;

	WORD type = 0;
	bool ok = GetStringTypeW(CT_CTYPE1, &ch, 1, &type) && (type & C1_ALPHA);
	v = ok ? 1 : 2;
	return ok;
}

static inline bool IsDigit(wchar_t ch)
{
	// Fast path for ASCII digits (90%+ of digit checks)
	if (ch >= L'0' && ch <= L'9')
		return true;

	// For non-ASCII, use cache (Arabic-Indic digits, etc.)
	thread_local static unsigned char cache[65536] = {}; // 0=unknown, 1=true, 2=false
	unsigned char& v = cache[(unsigned short)ch];
	if (v == 1) return true;
	if (v == 2) return false;

	WORD type = 0;
	bool ok = GetStringTypeW(CT_CTYPE1, &ch, 1, &type) && (type & C1_DIGIT);
	v = ok ? 1 : 2;
	return ok;
}

static inline bool IsNameTokenPunct(wchar_t ch)
{
	switch (ch)
	{
		case L'#':
		case L'@':
		case L'$':
		case L'%':
		case L'&':
		case L'*':
		case L'+':
		case L'-':
		case L'_':
		case L'=':
		case L'.':
		case L',':
		case L'/':
		case L'\\':
		case L'(':
		case L')':
		case L'[':
		case L']':
		case L'{':
		case L'}':
		case L'<':
		case L'>':
			return true;
		default:
			return false;
	}
}

// Check RTL first to avoid classifying Arabic as LTR
static inline bool IsStrongLTR(wchar_t ch)
{
	if (IsRTLCodepoint(ch))
		return false;
	return IsStrongAlpha(ch) || IsDigit(ch);
}

static inline bool HasStrongLTRNeighbor(const wchar_t* s, int n, int i)
{
	// Remove null/size check (caller guarantees validity)
	// Early exit after first strong neighbor found

	// Check previous character
	if (i > 0 && IsStrongLTR(s[i - 1]))
		return true;

	// Check next character
	if (i + 1 < n && IsStrongLTR(s[i + 1]))
		return true;

	return false;
}

static inline ECharDir GetCharDir(wchar_t ch)
{
	if (IsRTLCodepoint(ch))
		return ECharDir::RTL;

	// Use IsStrongLTR which now correctly excludes RTL
	if (IsStrongLTR(ch))
		return ECharDir::LTR;

	return ECharDir::Neutral;
}

static inline ECharDir GetCharDirSmart(const wchar_t* s, int n, int i)
{
	wchar_t ch = s[i];

	// True RTL letters/marks
	if (IsRTLCodepoint(ch))
		return ECharDir::RTL;

	// True LTR letters/digits (now correctly excludes RTL)
	if (IsStrongLTR(ch))
		return ECharDir::LTR;

	// Name-token punctuation: if adjacent to LTR, treat as LTR to keep token intact
	if (IsNameTokenPunct(ch) && HasStrongLTRNeighbor(s, n, i))
		return ECharDir::LTR;

	return ECharDir::Neutral;
}

// Pre-computed strong character lookup for O(1) neutral resolution
struct TStrongDirCache
{
	std::vector<EBidiDir> nextStrong; // nextStrong[i] = direction of next strong char after position i
	EBidiDir baseDir;

	TStrongDirCache(const wchar_t* s, int n, EBidiDir base) : nextStrong(n), baseDir(base)
	{
		// Build reverse lookup: scan from end to beginning
		EBidiDir lastSeen = baseDir;
		for (int i = n - 1; i >= 0; --i)
		{
			ECharDir cd = GetCharDir(s[i]);
			if (cd == ECharDir::LTR)
				lastSeen = EBidiDir::LTR;
			else if (cd == ECharDir::RTL)
				lastSeen = EBidiDir::RTL;

			nextStrong[i] = lastSeen;
		}
	}

	EBidiDir GetNextStrong(int i) const
	{
		if (i + 1 < (int)nextStrong.size())
			return nextStrong[i + 1];
		return baseDir;
	}
};

static inline EBidiDir ResolveNeutralDir(const wchar_t* s, int n, int i, EBidiDir baseDir, EBidiDir lastStrong, const TStrongDirCache* cache = nullptr)
{
	// Use pre-computed cache if available (O(1) instead of O(n))
	EBidiDir nextStrong = baseDir;
	if (cache)
	{
		nextStrong = cache->GetNextStrong(i);
	}
	else
	{
		// Linear scan (slower, but works without cache)
		for (int j = i + 1; j < n; ++j)
		{
			ECharDir cd = GetCharDirSmart(s, n, j);
			if (cd == ECharDir::LTR) { nextStrong = EBidiDir::LTR; break; }
			if (cd == ECharDir::RTL) { nextStrong = EBidiDir::RTL; break; }
		}
	}

	// If both sides agree, neutral adopts that direction
	if (lastStrong == nextStrong)
		return lastStrong;

	// Handle edge cases for leading/trailing punctuation
	if (nextStrong == baseDir && lastStrong != baseDir)
		return lastStrong;

	if (lastStrong == baseDir && nextStrong != baseDir)
		return nextStrong;

	// Otherwise fall back to base direction
	return baseDir;
}

static EBidiDir DetectBaseDir_FirstStrong(const wchar_t* s, int n)
{
	if (!s || n <= 0)
		return EBidiDir::LTR;

	for (int i = 0; i < n; ++i)
	{
		const wchar_t ch = s[i];
		// Check RTL first, then alpha
		if (IsRTLCodepoint(ch))
			return EBidiDir::RTL;

		if (IsStrongAlpha(ch))
			return EBidiDir::LTR;
	}

	return EBidiDir::LTR;
}

static std::vector<wchar_t> BuildVisualBidiText_Tagless(const wchar_t* s, int n, bool forceRTL)
{
	if (!s || n <= 0)
		return {};

	// Detect chat format "name : msg" and extract components
	int chatSepPos = -1;
	for (int i = 0; i < n - 2; ++i)
	{
		if (s[i] == L' ' && s[i + 1] == L':' && s[i + 2] == L' ')
		{
			chatSepPos = i;
			break;
		}
	}

	// If chat format detected, process name and message separately
	if (chatSepPos > 0 && forceRTL)
	{
		// Use pointers instead of copying (zero-copy optimization)
		const wchar_t* name = s;
		const int nameLen = chatSepPos;

		const int msgStart = chatSepPos + 3;
		const wchar_t* msg = s + msgStart;
		const int msgLen = n - msgStart;

		// Check if message contains RTL
		bool msgHasRTL = false;
		for (int i = 0; i < msgLen; ++i)
		{
			if (IsRTLCodepoint(msg[i]))
			{
				msgHasRTL = true;
				break;
			}
		}

		// Build result based on message direction (pre-reserve exact size)
		std::vector<wchar_t> visual;
		visual.reserve((size_t)n);

		if (msgHasRTL)
		{
			// Arabic message: apply BiDi to message, then add " : name"
			std::vector<wchar_t> msgVisual = BuildVisualBidiText_Tagless(msg, msgLen, false);
			visual.insert(visual.end(), msgVisual.begin(), msgVisual.end());
			visual.push_back(L' ');
			visual.push_back(L':');
			visual.push_back(L' ');
			visual.insert(visual.end(), name, name + nameLen); // Direct pointer insert
		}
		else
		{
			// English message: "msg : name"
			visual.insert(visual.end(), msg, msg + msgLen); // Direct pointer insert
			visual.push_back(L' ');
			visual.push_back(L':');
			visual.push_back(L' ');
			visual.insert(visual.end(), name, name + nameLen); // Direct pointer insert
		}

		return visual;
	}

	// 1) base direction
	EBidiDir base = forceRTL ? EBidiDir::RTL : DetectBaseDir_FirstStrong(s, n);

	// Pre-compute strong character positions for O(1) neutral resolution
	TStrongDirCache strongCache(s, n, base);

	// 2) split into runs
	// Estimate runs based on text length (~1 per 50 chars, min 4)
	std::vector<TBidiRun> runs;
	const size_t estimatedRuns = (size_t)std::max(4, n / 50);
	runs.reserve(estimatedRuns);

	auto push_run = [&](EBidiDir d)
		{
			if (runs.empty() || runs.back().dir != d)
				runs.push_back(TBidiRun{ d, {} });
		};

	// start with base so leading neutrals attach predictably
	push_run(base);

	EBidiDir lastStrong = base;

	for (int i = 0; i < n; ++i)
	{
		wchar_t ch = s[i];

		EBidiDir d;
		ECharDir cd = GetCharDirSmart(s, n, i);

		if (cd == ECharDir::RTL)
		{
			d = EBidiDir::RTL;
			lastStrong = EBidiDir::RTL;
		}
		else if (cd == ECharDir::LTR)
		{
			d = EBidiDir::LTR;
			lastStrong = EBidiDir::LTR;
		}
		else
		{
			// Pass cache for O(1) lookup instead of O(n) scan
			d = ResolveNeutralDir(s, n, i, base, lastStrong, &strongCache);
		}

		push_run(d);
		runs.back().text.push_back(ch);
	}

	// 3) shape RTL runs in logical order (Arabic shaping)
	for (auto& r : runs)
	{
		if (r.dir != EBidiDir::RTL)
			continue;

		if (r.text.empty())
			continue;

		// Check for potential integer overflow before allocation
		if (r.text.size() > SIZE_MAX / ARABIC_SHAPING_EXPANSION_FACTOR_RETRY - ARABIC_SHAPING_SAFETY_MARGIN_RETRY)
		{
			BIDI_LOG("BuildVisualBidiText: RTL run too large for shaping (%zu chars)", r.text.size());
			continue; // Text too large to process safely
		}

		std::vector<wchar_t> shaped(r.text.size() * ARABIC_SHAPING_EXPANSION_FACTOR + ARABIC_SHAPING_SAFETY_MARGIN, 0);

		int outLen = Arabic_MakeShape(r.text.data(), (int)r.text.size(), shaped.data(), (int)shaped.size());
		if (outLen <= 0)
		{
			BIDI_LOG("Arabic_MakeShape failed for run of %zu chars", r.text.size());
			continue;
		}

		// Retry once if buffer too small
		if (outLen >= (int)shaped.size())
		{
			shaped.assign(r.text.size() * ARABIC_SHAPING_EXPANSION_FACTOR_RETRY + ARABIC_SHAPING_SAFETY_MARGIN_RETRY, 0);
			outLen = Arabic_MakeShape(r.text.data(), (int)r.text.size(), shaped.data(), (int)shaped.size());
			if (outLen <= 0)
				continue;
			// Add error check instead of silent truncation
			if (outLen > (int)shaped.size())
			{
				BIDI_LOG("Arabic_MakeShape: Buffer still too small after retry (%d > %zu)", outLen, shaped.size());
				// Shaping failed critically, use unshaped text
				continue;
			}
		}

		r.text.assign(shaped.begin(), shaped.begin() + outLen);
	}

	// 4) produce visual order:
	// - reverse RTL runs internally
	// - reverse run sequence if base RTL
	std::vector<wchar_t> visual;
	visual.reserve((size_t)n);

	auto emit_run = [&](const TBidiRun& r)
		{
			if (r.dir == EBidiDir::RTL)
			{
				for (int k = (int)r.text.size() - 1; k >= 0; --k)
					visual.push_back(r.text[(size_t)k]);
			}
			else
			{
				visual.insert(visual.end(), r.text.begin(), r.text.end());
			}
		};

	if (base == EBidiDir::LTR)
	{
		for (const auto& r : runs)
			emit_run(r);
	}
	else
	{
		for (int i = (int)runs.size() - 1; i >= 0; --i)
			emit_run(runs[(size_t)i]);
	}

	return visual;
}

// ============================================================================
// TextTail formatting for RTL UI
// ============================================================================

enum class EPlaceDir
{
	Left, // place block to the LEFT of the cursor (cursor is a right edge)
	Right // place block to the RIGHT of the cursor (cursor is a left edge)
};

template <typename TText>
inline float TextTailBiDi(TText* t, float cursorX, float y, float z, float fxAdd, EPlaceDir dir)
{
	if (!t)
		return cursorX;

	int w = 0, h = 0;
	t->GetTextSize(&w, &h);
	const float fw = static_cast<float>(w);

	float x;
	if (dir == EPlaceDir::Left)
	{
		x = t->IsRTL() ? cursorX : (cursorX - fw);
		// advance cursor left
		cursorX = cursorX - fw - fxAdd;
	}
	else
	{
		x = t->IsRTL() ? (cursorX + fw) : cursorX;
		// advance cursor right
		cursorX = cursorX + fw + fxAdd;
	}

	// SNAP to pixel grid to avoid "broken pixels"
	x = floorf(x + 0.5f);
	y = floorf(y + 0.5f);

	t->SetPosition(x, y, z);
	t->Update();

	return cursorX;
}
