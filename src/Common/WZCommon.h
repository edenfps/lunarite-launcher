// WZCommon.h - shared foundation for the launcher.
// Plain Win32 + ATL, Unicode, x86.
#pragma once

#ifndef WINVER
#define WINVER 0x0501
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>
#include <vector>

// Result object passed around by value:
//   DWORD code; DWORD subCode; wchar_t message[1024];
// Default-constructed = { 0, 0, L"Success" }.
struct WZResult
{
    DWORD code;
    DWORD subCode;
    wchar_t message[1024];

    WZResult() { SetSuccess(); }
    WZResult(DWORD c, DWORD sub, const wchar_t* fmt, ...);

    void SetSuccess();
    void Set(DWORD c, DWORD sub, const wchar_t* fmt, ...);
    bool IsSuccess() const { return code == 0 && subCode == 0; }
};

// Error codes (passed as WZResult::code).
enum : DWORD
{
    WZ_ERR_GENERIC      = 0x80000001,
    WZ_ERR_INVALIDARG   = 0x80000002,
    WZ_ERR_LOADFAIL     = 0x80000003,   // LoadLibrary / GetProcAddress
    WZ_ERR_DECRYPT      = 0x80000004,
};

// Logger: appends "[YYYY-MM-DD HH:MM:SS, mmm] <msg>\r\n" to
// <exe dir>\WZLauncher.Log via std::wofstream. Implemented in Common/Log.cpp.
void WZLog(const wchar_t* fmt, ...);
// Writes the "=====\r\n[ts] Start WZLauncher\r\n=====" banner seen at startup.
void WZLogBanner(const wchar_t* title);

// Bounded wide-string helpers matching the inlined StringCchCopy/Printf loops.
void WZStrCopy(wchar_t* dst, size_t cch, const wchar_t* src);
int  WZStrPrintf(wchar_t* dst, size_t cch, const wchar_t* fmt, ...);

// Directory that holds the running exe, no trailing backslash.
std::wstring WZGetModuleDir();

// Lunarite: true when the Windows display language is Korean (LUNARITE_LANGUAGE=ko/en overrides). The launcher then reads
// WZLauncher.kor.wzc, starts the game with -LANGUAGE=KOR and tells the patch server "ko".
bool WZUserIsKorean();
