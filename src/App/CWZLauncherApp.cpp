#include "CWZLauncherApp.h"

#include <commctrl.h>
#include <objbase.h>
#include <atlbase.h>
#include <atlhost.h>
#include <cstdio>
#include <fstream>

#include "../Common/CrashDump.h"
#include "CWZLauncherDlg.h"

#pragma comment(lib, "comctl32.lib")

// ATL needs a module object for CAxWindow hosting.
class CWZLauncherAtlModule : public ATL::CAtlModuleT<CWZLauncherAtlModule> {};
static CWZLauncherAtlModule s_atlModule;

CWZLauncherApp theApp;

CWZLauncherApp::CWZLauncherApp() : m_hInstance(nullptr)
{
    CrashDump::Install(true, &CWZLauncherApp::GetDumpFilePath, &CWZLauncherApp::WriteCrashLog);
}

CWZLauncherApp::~CWZLauncherApp()
{
}

// "<exe path>_dump_YYYYMMDD_HHMMSS.dmp"
const wchar_t* CWZLauncherApp::GetDumpFilePath()
{
    static wchar_t path[260];
    wchar_t exe[260] = {};
    SYSTEMTIME st;

    GetModuleFileNameW(nullptr, exe, 260);
    GetLocalTime(&st);
    WZStrPrintf(path, 260, L"%s_dump_%d%02d%02d_%02d%02d%02d.dmp",
                exe, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return path;
}

// "<exe path>_Log_YYYYMMDD_HHMMSS.txt" with the dump location and exception text.
int __cdecl CWZLauncherApp::WriteCrashLog(const wchar_t* info, const wchar_t* dumpPath)
{
    wchar_t exe[260] = {};
    wchar_t logPath[260] = {};
    SYSTEMTIME st;

    GetModuleFileNameW(nullptr, exe, 260);
    GetLocalTime(&st);
    WZStrPrintf(logPath, 260, L"%s_Log_%d%02d%02d_%02d%02d%02d.txt",
                exe, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    std::wofstream log(logPath, std::ios::out | std::ios::app);
    if (!log.is_open())
        return 0;

    wchar_t line[1024] = {};
    WZStrPrintf(line, 1024, L"Dump file path : %s\r\n\r\n", dumpPath);
    log << line;
    if (info)
        log << info;
    return 0;
}

BOOL CWZLauncherApp::InitInstance()
{
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = 0xFF;
    InitCommonControlsEx(&icc);

    // The WebBrowser ActiveX needs OLE and the ATL host window class.
    OleInitialize(nullptr);
    AtlAxWinInit();

    CWZLauncherDlg dlg;
    dlg.DoModal();
    return FALSE;
}

int CWZLauncherApp::Run()
{
    InitInstance();
    AtlAxWinTerm();
    OleUninitialize();
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    theApp.m_hInstance = hInstance;
    return theApp.Run();
}
