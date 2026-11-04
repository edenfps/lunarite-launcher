#include "CrashDump.h"
#include <dbghelp.h>
#include <cstdio>
#include <cwchar>

#pragma comment(lib, "dbghelp.lib")

namespace
{
LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;   // lpTopLevelExceptionFilter
bool g_stackWalk = false;                                  // byte_473248
CrashDump::DumpFileNameProc g_dumpFileName = nullptr;      // dword_473244
CrashDump::CrashReportProc g_onCrash = nullptr;            // dword_473240

const size_t kReportChars = 8192;

void ExceptionName(wchar_t* out, DWORD code)
{
    switch (code)
    {
    case 0xC0000005: swprintf(out, 1024, L"Access violation"); break;
    case 0x80000002: swprintf(out, 1024, L"Data type misalignment"); break;
    case 0x80000003: swprintf(out, 1024, L"Break point"); break;
    case 0x80000004: swprintf(out, 1024, L"Single step"); break;
    case 0xC0000006: swprintf(out, 1024, L"In page error"); break;
    case 0xC000001D: swprintf(out, 1024, L"Illegal instruction"); break;
    case 0xC0000025: swprintf(out, 1024, L"Noncontinuable exception"); break;
    case 0xC0000026: swprintf(out, 1024, L"Invalid disposition"); break;
    case 0xC000008C: swprintf(out, 1024, L"Array bounds exceeded"); break;
    case 0xC000008D: swprintf(out, 1024, L"Float denormal operand"); break;
    case 0xC000008E: swprintf(out, 1024, L"Float divide by zero"); break;
    case 0xC000008F: swprintf(out, 1024, L"Float inexact result"); break;
    case 0xC0000090: swprintf(out, 1024, L"Float invalid operation"); break;
    case 0xC0000091: swprintf(out, 1024, L"Float overflow"); break;
    case 0xC0000092: swprintf(out, 1024, L"Float stack check"); break;
    case 0xC0000093: swprintf(out, 1024, L"Float underlow"); break;
    case 0xC0000094: swprintf(out, 1024, L"Integer divide by zero"); break;
    case 0xC0000095: swprintf(out, 1024, L"Integer overflow"); break;
    case 0xC0000096: swprintf(out, 1024, L"Privileged instruction"); break;
    case 0xC00000FD: swprintf(out, 1024, L"Stack overflow"); break;
    default: break;
    }
}

// Appends formatted text to the report buffer.
void Append(wchar_t* report, const wchar_t* fmt, ...)
{
    wchar_t line[1024] = {};
    va_list args;
    va_start(args, fmt);
    _vsnwprintf(line, 1023, fmt, args);
    va_end(args);
    size_t used = wcslen(report);
    size_t add = wcslen(line);
    if (used + add + 1 < kReportChars)
        memcpy(report + used, line, (add + 1) * sizeof(wchar_t));
}

bool WriteMiniDump(EXCEPTION_POINTERS* exception, const wchar_t* dumpFile)
{
    wchar_t path[260] = {};
    HMODULE dbghelp = nullptr;
    if (GetModuleFileNameW(nullptr, path, 260))
    {
        wchar_t* slash = wcsrchr(path, L'\\');
        if (slash)
        {
            wcscpy_s(slash + 1, 260 - (slash + 1 - path), L"DBGHELP.DLL");
            dbghelp = LoadLibraryW(path);
        }
    }
    if (!dbghelp)
    {
        dbghelp = LoadLibraryW(L"DBGHELP.DLL");
        if (!dbghelp)
            return false;
    }
    typedef BOOL(WINAPI * MiniDumpWriteDumpProc)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                                 PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
    MiniDumpWriteDumpProc write = reinterpret_cast<MiniDumpWriteDumpProc>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
    if (write)
    {
        HANDLE file = CreateFileW(dumpFile, GENERIC_WRITE, FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            MINIDUMP_EXCEPTION_INFORMATION info;
            info.ThreadId = GetCurrentThreadId();
            info.ExceptionPointers = exception;
            info.ClientPointers = FALSE;
            write(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, exception ? &info : nullptr,
                  nullptr, nullptr);
            CloseHandle(file);
        }
    }
    return true;
}

LONG WINAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* exception)
{
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    static wchar_t report[kReportChars];
    wchar_t name[1024] = {};
    wchar_t buffer[1024] = {};
    memset(report, 0, sizeof(report));
    const wchar_t* dumpFile = nullptr;

    if (!exception)
        return EXCEPTION_CONTINUE_SEARCH;

    if (g_dumpFileName)
    {
        dumpFile = g_dumpFileName();
        WriteMiniDump(exception, dumpFile);
    }

    SymSetOptions(0x214);
    if (SymInitialize(process, nullptr, TRUE))
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        Append(report, L"%4d.%2d.%2d  %d:%d:%d.%d\n\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
               st.wSecond, st.wMilliseconds);
        Append(report, L"Process Handle : %#x\t\t Thread Handle : %#x\r\n", process, thread);
        ExceptionName(name, exception->ExceptionRecord->ExceptionCode);
        Append(report, L"Type : %s\n", name);
        CONTEXT* ctx = exception->ContextRecord;
#ifdef _M_IX86
        Append(report, L"eax: %#08x ebx: %#08x\r\n", ctx->Eax, ctx->Ebx);
        Append(report, L"ecx: %#08x edx: %#08x\r\n", ctx->Ecx, ctx->Edx);
        Append(report, L"esi: %#08x edi: %#08x\r\n", ctx->Esi, ctx->Edi);
        Append(report, L"ebp: %#08x esp: %#08x\r\n", ctx->Ebp, ctx->Esp);
        Append(report, L"\r\n");

        STACKFRAME frame;
        memset(&frame, 0, sizeof(frame));
        frame.AddrPC.Offset = ctx->Eip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Offset = ctx->Esp;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = ctx->Ebp;
        frame.AddrFrame.Mode = AddrModeFlat;

        if (g_stackWalk)
        {
            for (int i = 0; i < 512; ++i)
            {
                if (!frame.AddrPC.Offset ||
                    !StackWalk(IMAGE_FILE_MACHINE_I386, process, thread, &frame, ctx, nullptr, SymFunctionTableAccess,
                               SymGetModuleBase, nullptr))
                    break;

                // SYMBOL_INFO.
                alignas(8) unsigned char symbolBuffer[2048] = {};
                SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
                symbol->MaxNameLen = 424;
                symbol->SizeOfStruct = 511;
                DWORD64 displacement = 0;
                if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol))
                {
                    IMAGEHLP_LINE64 line = {};
                    IMAGEHLP_LINE64 funcLine = {};
                    line.SizeOfStruct = 24;
                    funcLine.SizeOfStruct = 24;
                    DWORD lineDisplacement = 0;
                    BOOL hasLine = SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &line);
                    BOOL hasFuncLine = SymGetLineFromAddr64(process, static_cast<DWORD>(frame.AddrPC.Offset - displacement),
                                                            &lineDisplacement, &funcLine);
                    if (!hasLine || !hasFuncLine)
                    {
                        Append(report, L"%s() + offset : 0x%04x\r\n", symbol->Name, line.LineNumber - funcLine.LineNumber);
                        Append(report, L"(No line information...)\r\n");
                    }
                    else
                    {
                        Append(report, L"%s() + %d line [addr=%#x]\r\n", symbol->Name,
                               line.LineNumber - funcLine.LineNumber, static_cast<DWORD>(symbol->Address));
                        Append(report, L"-%s %d line\r\n", line.FileName, line.LineNumber);
                    }
                }
                else
                {
                    Append(report, L"0x%08x Address Lookup Error\r\n", frame.AddrPC.Offset);
                    Append(report, L"-SymFromAddr() returned error : %d\r\n", GetLastError());
                }
                Append(report, L"\r\n");
            }
        }
#endif
        SymCleanup(process);
    }

    if (g_onCrash)
        g_onCrash(report, dumpFile);
    return EXCEPTION_EXECUTE_HANDLER;
}
}

namespace CrashDump
{
void Install(bool stackWalk, DumpFileNameProc dumpFileName, CrashReportProc onCrash)
{
    g_previousFilter = SetUnhandledExceptionFilter(TopLevelExceptionFilter);
    g_stackWalk = stackWalk;
    g_dumpFileName = dumpFileName;
    g_onCrash = onCrash;
}

void Uninstall()
{
    if (g_previousFilter)
        SetUnhandledExceptionFilter(g_previousFilter);
    g_stackWalk = false;
    g_dumpFileName = nullptr;
    g_onCrash = nullptr;
}
}
