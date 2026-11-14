// WZCommon.cpp - WZResult helpers and bounded string helpers.
#include "WZCommon.h"
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

namespace
{
    // format into a 1024-char temp (max 1023 chars), then store codes + message.
    void FormatInto(WZResult* r, DWORD code, DWORD sub, const wchar_t* fmt, va_list args)
    {
        wchar_t buffer[1024] = {};
        int n = _vsnwprintf(buffer, 1023, fmt, args);
        if (n < 0 || n >= 1023)
            buffer[1023] = 0;

        r->code = code;
        r->subCode = sub;
        wchar_t* msg = r->message;
        int m = _snwprintf(msg, 1023, L"%s", buffer);
        if (m < 0 || m >= 1023)
            msg[1023] = 0;
    }
}

// ( default construction)
void WZResult::SetSuccess()
{
    code = 0;
    subCode = 0;
    WZStrCopy(message, 1024, L"Success");
}

WZResult::WZResult(DWORD c, DWORD sub, const wchar_t* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    FormatInto(this, c, sub, fmt, args);
    va_end(args);
}

void WZResult::Set(DWORD c, DWORD sub, const wchar_t* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    FormatInto(this, c, sub, fmt, args);
    va_end(args);
}

// StringCchCopy-style inlined loop (e.g.,): truncates and always terminates.
void WZStrCopy(wchar_t* dst, size_t cch, const wchar_t* src)
{
    if (!dst || cch == 0)
        return;
    size_t i = 0;
    if (src)
    {
        while (i < cch && src[i])
        {
            dst[i] = src[i];
            ++i;
        }
    }
    if (i == cch)
        --i;
    dst[i] = 0;
}

// (StringCchPrintf). Returns 0, 0x80070057 (bad size) or 0x8007007A (truncated).
int WZStrPrintf(wchar_t* dst, size_t cch, const wchar_t* fmt, ...)
{
    if ((int)cch <= 0)
        return (int)0x80070057;

    size_t limit = cch - 1;
    va_list args;
    va_start(args, fmt);
    int n = _vsnwprintf(dst, limit, fmt, args);
    va_end(args);

    if (n < 0 || (size_t)n > limit)
    {
        dst[limit] = 0;
        return (int)0x8007007A;
    }
    if ((size_t)n == limit)
        dst[limit] = 0;
    return 0;
}

// GetModuleFileNameW(NULL) with the file name stripped.
std::wstring WZGetModuleDir()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash)
        *slash = 0;
    return path;
}

void WZSetUserLanguage(bool korean)
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Lunarite", 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr)
        == ERROR_SUCCESS)
    {
        const wchar_t* value = korean ? L"ko" : L"en";
        RegSetValueExW(key, L"Language", 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
    }
}

bool WZUserIsKorean()
{
    // LUNARITE_LANGUAGE=ko or en overrides the Windows language (testing, or a player's choice).
    wchar_t forced[8] = {};
    if (GetEnvironmentVariableW(L"LUNARITE_LANGUAGE", forced, 8) && forced[0])
        return _wcsnicmp(forced, L"ko", 2) == 0;
    // The launcher's "한국어 / Korean" toggle (HKCU\Software\Lunarite\Language = ko | en).
    wchar_t saved[8] = {};
    DWORD size = sizeof(saved) - sizeof(wchar_t), type = 0;
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Lunarite", 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        bool found = RegQueryValueExW(key, L"Language", nullptr, &type, (BYTE*)saved, &size) == ERROR_SUCCESS
            && type == REG_SZ && saved[0];
        RegCloseKey(key);
        if (found)
            return _wcsnicmp(saved, L"ko", 2) == 0;
    }
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_KOREAN;
}
