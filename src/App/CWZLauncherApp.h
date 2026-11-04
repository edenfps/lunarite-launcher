// CWZLauncherApp.h - application object and entry point.
#pragma once

#include "../Common/WZCommon.h"

class CWZLauncherApp
{
public:
    CWZLauncherApp();                   // installs the crash handler
    ~CWZLauncherApp();

    BOOL InitInstance();
    int  Run();

    // Crash handler callbacks handed to the DbgHelp unhandled-exception filter.
    static const wchar_t* GetDumpFilePath();
    static int __cdecl WriteCrashLog(const wchar_t* info, const wchar_t* dumpPath);

    HINSTANCE m_hInstance;
};

extern CWZLauncherApp theApp;
