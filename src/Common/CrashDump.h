// CrashDump.h - unhandled exception filter: text report (registers + optional stack walk) and minidump.
#pragma once
#include "WZCommon.h"

namespace CrashDump
{
    // Returns the full path of the .dmp file to write (dword_473244).
    typedef const wchar_t* (*DumpFileNameProc)();
    // Receives the text report and the dump file name after the dump is written (dword_473240).
    typedef int (__cdecl* CrashReportProc)(const wchar_t* report, const wchar_t* dumpFile);

    // installs the filter. stackWalk enables the StackWalk/SymFromAddr section of the report.
    void Install(bool stackWalk, DumpFileNameProc dumpFileName, CrashReportProc onCrash);

    // restores the previous filter and clears the callbacks.
    void Uninstall();
}
