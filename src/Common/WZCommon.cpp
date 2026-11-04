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

// /: GetModuleFileNameW(NULL) with the file name stripped.
std::wstring WZGetModuleDir()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash)
        *slash = 0;
    return path;
}
