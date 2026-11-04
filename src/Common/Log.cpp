// Log.cpp - WZLauncher.Log writer.
#include "Log.h"
#include <fstream>
#include <stdarg.h>
#include <wchar.h>

namespace
{
    wchar_t g_logPath[260] = {};

    const wchar_t kRule[] = L"================================================================================\r\n";

    // append a trailing backslash if missing.
    void AppendBackslash(wchar_t* path)
    {
        size_t len = wcsnlen(path, 260);
        if (len == 0)
        {
            path[0] = L'\\';
            path[1] = 0;
        }
        else if (path[len - 1] != L'\\')
        {
            path[len] = L'\\';
            path[len + 1] = 0;
        }
    }

    // /: bounded append.
    void AppendBounded(wchar_t* dst, size_t cch, const wchar_t* src)
    {
        size_t len = wcsnlen(dst, cch);
        if (len >= cch)
            return;
        WZStrCopy(dst + len, cch - len, src);
    }

    void BuildTimestamp(wchar_t* out, size_t cch)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        WZStrPrintf(out, cch, L"[%.4d-%.2d-%.2d %.2d:%.2d:%.2d, %.3d] ",
                    st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    }

    // truncate-and-write (ios::out|trunc|binary).
    bool WriteTruncated(const wchar_t* path, const wchar_t* text, size_t len)
    {
        if (!path || !text)
            return false;
        std::wofstream f(path, std::ios::out | std::ios::trunc | std::ios::binary);
        if (!f.is_open())
            return false;
        f.write(text, (std::streamsize)len);
        return true;
    }
}

// (path part)
void WZLogInitPath(wchar_t* out)
{
    wchar_t* p = out ? out : g_logPath;
    GetModuleFileNameW(nullptr, p, MAX_PATH);
    wchar_t* slash = wcsrchr(p, L'\\');
    if (slash)
        *slash = 0;
    AppendBackslash(p);
    AppendBounded(p, 260, L"WZLauncher.Log");
    if (out)
        WZStrCopy(g_logPath, 260, out);
}

const wchar_t* WZLogGetPath()
{
    return g_logPath;
}

void WZLogBanner(const wchar_t* title)
{
    wchar_t path[260] = {};
    WZLogInitPath(path);

    wchar_t ts[1048] = {};
    BuildTimestamp(ts, 1048);
    // "%s[%.4d-%.2d-%.2d %.2d:%.2d:%.2d, %.3d] %s\r\n%s\r\n\r\n" with %s = 80 '=' + CRLF
    wchar_t text[1048] = {};
    WZStrPrintf(text, 1048, L"%s%s%s\r\n%s\r\n\r\n", kRule, ts, title, kRule);

    size_t len = wcsnlen(text, 1048);
    if (len >= 1048)
        len = 0;
    WriteTruncated(path, text, len);
}

void WZLog(const wchar_t* fmt, ...)
{
    wchar_t line[1048] = {};
    BuildTimestamp(line, 1048);

    wchar_t message[2050] = {};
    va_list args;
    va_start(args, fmt);
    int n = _vsnwprintf(message, 1022, fmt, args);
    va_end(args);
    if (n < 0 || n >= 1022)
        message[1022] = 0;

    // /: bounded appends into a 1023-char line buffer.
    size_t used = wcsnlen(line, 1023);
    if (used < 1023)
        WZStrCopy(line + used, 1023 - used, message);
    used = wcsnlen(line, 1023);
    if (used < 1023)
        WZStrCopy(line + used, 1023 - used, L"\r\n");

    if (g_logPath[0] == 0)
        WZLogInitPath(nullptr);

    // Lines longer than 260 characters are written empty.
    size_t len = wcsnlen(line, 260);
    if (len >= 260)
        len = 0;

    std::wofstream f(g_logPath, std::ios::out | std::ios::app | std::ios::binary);
    if (f.is_open())
        f.write(line, (std::streamsize)len);
}
