#include "StdAfx.h"

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>

#include "Debug.h"
#include "Singleton.h"
#include "Timer.h"
#include <filesystem>
#include <utf8.h>

const DWORD DEBUG_STRING_MAX_LEN = 1024;

static int isLogFile = false;
HWND g_PopupHwnd = NULL;

// Convert UTF-8 char* -> wide and send to debugger (NO helper function, just a macro)
#ifdef _DEBUG
#define DBG_OUT_W_UTF8(psz)                                                   \
    do {                                                                      \
        const char* __s = (psz) ? (psz) : "";                                 \
        std::wstring __w = Utf8ToWide(__s);                                   \
        OutputDebugStringW(__w.c_str());                                      \
    } while (0)
#else
#define DBG_OUT_W_UTF8(psz) do { (void)(psz); } while (0)
#endif

class CLogFile : public CSingleton<CLogFile>
{
    public:
        CLogFile() : m_fp(NULL) {}

        virtual ~CLogFile()
        {
            if (m_fp)
                fclose(m_fp);

            m_fp = NULL;
        }

        void Initialize()
        {
            m_fp = fopen("log/log.txt", "w");
        }

        void Write(const char* c_pszMsg)
        {
            if (!m_fp)
                return;

            time_t ct = time(0);
            struct tm ctm = *localtime(&ct);

            fprintf(m_fp, "%02d%02d %02d:%02d:%05d :: %s",
                ctm.tm_mon + 1,
                ctm.tm_mday,
                ctm.tm_hour,
                ctm.tm_min,
                ELTimer_GetMSec() % 60000,
                c_pszMsg);

            fflush(m_fp);
        }

    protected:
        FILE* m_fp;
};

static CLogFile gs_logfile;

static UINT gs_uLevel = 0;

void SetLogLevel(UINT uLevel)
{
    gs_uLevel = uLevel;
}

void Log(UINT uLevel, const char* c_szMsg)
{
    if (uLevel >= gs_uLevel)
        Trace(c_szMsg);
}

void Logn(UINT uLevel, const char* c_szMsg)
{
    if (uLevel >= gs_uLevel)
        Tracen(c_szMsg);
}

void Logf(UINT uLevel, const char* c_szFormat, ...)
{
    if (uLevel < gs_uLevel)
        return;

    char szBuf[DEBUG_STRING_MAX_LEN + 1];

    va_list args;
    va_start(args, c_szFormat);
    _vsnprintf_s(szBuf, sizeof(szBuf), _TRUNCATE, c_szFormat, args);
    va_end(args);

#ifdef _DEBUG
    DBG_OUT_W_UTF8(szBuf);
    fputs(szBuf, stdout);
#endif

    if (isLogFile)
        LogFile(szBuf);
}

void Lognf(UINT uLevel, const char* c_szFormat, ...)
{
    if (uLevel < gs_uLevel)
        return;

    char szBuf[DEBUG_STRING_MAX_LEN + 2];

    va_list args;
    va_start(args, c_szFormat);
    _vsnprintf_s(szBuf, sizeof(szBuf), _TRUNCATE, c_szFormat, args);
    va_end(args);

    size_t cur = strnlen(szBuf, sizeof(szBuf));
    if (cur + 1 < sizeof(szBuf)) {
        szBuf[cur] = '\n';
        szBuf[cur + 1] = '\0';
    }
    else {
        szBuf[sizeof(szBuf) - 2] = '\n';
        szBuf[sizeof(szBuf) - 1] = '\0';
    }

#ifdef _DEBUG
    DBG_OUT_W_UTF8(szBuf);
    fputs(szBuf, stdout);
#endif

    if (isLogFile)
        LogFile(szBuf);
}

void Trace(const char* c_szMsg)
{
#ifdef _DEBUG
    DBG_OUT_W_UTF8(c_szMsg);
    printf("%s", c_szMsg ? c_szMsg : "");
#endif

    if (isLogFile)
        LogFile(c_szMsg ? c_szMsg : "");
}

void Tracen(const char* c_szMsg)
{
#ifdef _DEBUG
    char szBuf[DEBUG_STRING_MAX_LEN + 2];
    _snprintf_s(szBuf, sizeof(szBuf), _TRUNCATE, "%s\n", c_szMsg ? c_szMsg : "");

    DBG_OUT_W_UTF8(szBuf);

    fputs(szBuf, stdout);

    if (isLogFile)
        LogFile(szBuf);
#else
    if (isLogFile)
    {
        LogFile(c_szMsg ? c_szMsg : "");
        LogFile("\n");
    }
#endif
}

void Tracenf(const char* c_szFormat, ...)
{
    char szBuf[DEBUG_STRING_MAX_LEN + 2];

    va_list args;
    va_start(args, c_szFormat);
    _vsnprintf_s(szBuf, sizeof(szBuf), _TRUNCATE, c_szFormat, args);
    va_end(args);

    size_t cur = strnlen(szBuf, sizeof(szBuf));
    if (cur + 1 < sizeof(szBuf)) {
        szBuf[cur] = '\n';
        szBuf[cur + 1] = '\0';
    }
    else {
        szBuf[sizeof(szBuf) - 2] = '\n';
        szBuf[sizeof(szBuf) - 1] = '\0';
    }

#ifdef _DEBUG
    DBG_OUT_W_UTF8(szBuf);
    fputs(szBuf, stdout);
#endif

    if (isLogFile)
        LogFile(szBuf);
}

void Tracef(const char* c_szFormat, ...)
{
    char szBuf[DEBUG_STRING_MAX_LEN + 1];

    va_list args;
    va_start(args, c_szFormat);
    _vsnprintf_s(szBuf, sizeof(szBuf), _TRUNCATE, c_szFormat, args);
    va_end(args);

#ifdef _DEBUG
    DBG_OUT_W_UTF8(szBuf);
    fputs(szBuf, stdout);
#endif

    if (isLogFile)
        LogFile(szBuf);
}

void TraceError(const char* c_szFormat, ...)
{
#ifndef _DISTRIBUTE 
    char szBuf[DEBUG_STRING_MAX_LEN + 2];

    strncpy_s(szBuf, sizeof(szBuf), "SYSERR: ", _TRUNCATE);
    int prefixLen = (int)strlen(szBuf);

    va_list args;
    va_start(args, c_szFormat);
    _vsnprintf_s(szBuf + prefixLen, sizeof(szBuf) - prefixLen, _TRUNCATE, c_szFormat, args);
    va_end(args);

    size_t cur = strnlen(szBuf, sizeof(szBuf));
    if (cur + 1 < sizeof(szBuf)) {
        szBuf[cur] = '\n';
        szBuf[cur + 1] = '\0';
    }
    else {
        szBuf[sizeof(szBuf) - 2] = '\n';
        szBuf[sizeof(szBuf) - 1] = '\0';
    }

    time_t ct = time(0);
    struct tm ctm = *localtime(&ct);

    fprintf(stderr, "%02d%02d %02d:%02d:%05d :: %s",
        ctm.tm_mon + 1,
        ctm.tm_mday,
        ctm.tm_hour,
        ctm.tm_min,
        ELTimer_GetMSec() % 60000,
        szBuf + 8);
    fflush(stderr);

#ifdef _DEBUG
    DBG_OUT_W_UTF8(szBuf);
    fputs(szBuf, stdout);
#endif

    if (isLogFile)
        LogFile(szBuf);
#endif
}

void TraceErrorWithoutEnter(const char* c_szFormat, ...)
{
#ifndef _DISTRIBUTE 

    char szBuf[DEBUG_STRING_MAX_LEN];

    va_list args;
    va_start(args, c_szFormat);
    _vsnprintf_s(szBuf, sizeof(szBuf), _TRUNCATE, c_szFormat, args);
    va_end(args);

    time_t ct = time(0);
    struct tm ctm = *localtime(&ct);

    fprintf(stderr, "%02d%02d %02d:%02d:%05d :: %s",
        ctm.tm_mon + 1,
        ctm.tm_mday,
        ctm.tm_hour,
        ctm.tm_min,
        ELTimer_GetMSec() % 60000,
        szBuf + 8);
    fflush(stderr);

#ifdef _DEBUG
    DBG_OUT_W_UTF8(szBuf);
    fputs(szBuf, stdout);
#endif

    if (isLogFile)
        LogFile(szBuf);
#endif
}

void LogBoxf(const char* c_szFormat, ...)
{
    va_list args;
    va_start(args, c_szFormat);

    char szBuf[2048];
    _vsnprintf_s(szBuf, sizeof(szBuf), _TRUNCATE, c_szFormat, args);

    va_end(args);

    LogBox(szBuf);
}

void LogBox(const char* c_szMsg, const char* c_szCaption, HWND hWnd)
{
    if (!hWnd)
        hWnd = g_PopupHwnd;

    std::wstring wMsg = Utf8ToWide(c_szMsg ? c_szMsg : "");
    std::wstring wCaption = Utf8ToWide(c_szCaption ? c_szCaption : "LOG");

    MessageBoxW(hWnd, wMsg.c_str(), wCaption.c_str(), MB_OK);

    // Logging stays UTF-8
    Tracen(c_szMsg ? c_szMsg : "");
}

void LogFile(const char* c_szMsg)
{
    CLogFile::Instance().Write(c_szMsg);
}

void LogFilef(const char* c_szMessage, ...)
{
    va_list args;
    va_start(args, c_szMessage);

    char szBuf[DEBUG_STRING_MAX_LEN + 1];
    _vsnprintf_s(szBuf, sizeof(szBuf), _TRUNCATE, c_szMessage, args);

    va_end(args);

    CLogFile::Instance().Write(szBuf);
}

void OpenLogFile(bool bUseLogFIle)
{
    if (!std::filesystem::exists("log")) {
        std::filesystem::create_directory("log");
    }

#ifndef _DISTRIBUTE 
    _wfreopen(L"log/syserr.txt", L"w", stderr);

    if (bUseLogFIle)
    {
        isLogFile = true;
        CLogFile::Instance().Initialize();
    }
#endif
}

void OpenConsoleWindow()
{
    AllocConsole();

    _wfreopen(L"CONOUT$", L"a", stdout);
    _wfreopen(L"CONIN$", L"r", stdin);
}
