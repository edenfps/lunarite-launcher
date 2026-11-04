// SettingsUtil.h - small helpers shared by the Settings module.
#pragma once

#include "../Common/WZCommon.h"
#include "../Common/XmlParser.h"
#include <cwchar>
#include <cstdlib>

namespace settings_detail
{
    // (case-insensitive child lookup). Iterates the circular child list from the first child
    // and stops when it wraps. Returns nullptr when absent.
    inline XmlNode* FindChild(const XmlNode* parent, const wchar_t* name)
    {
        if (!parent || !parent->firstChild)
            return nullptr;
        XmlNode* n = parent->firstChild;
        do
        {
            if (!_wcsicmp(n->name.c_str(), name))
                return n;
            n = n->next;
        } while (n && n != parent->firstChild);
        return nullptr;
    }

    // (case-insensitive attribute lookup). A missing attribute reads as an empty string.
    inline const wchar_t* Attr(const XmlNode* node, const wchar_t* name)
    {
        if (node)
        {
            for (const XmlAttribute* a = node->attributes; a; a = a->next)
                if (!_wcsicmp(a->name.c_str(), name))
                    return a->value.c_str();
        }
        return L"";
    }

    // Text of a child element, nullptr if the child does not exist.
    inline const wchar_t* ChildText(const XmlNode* parent, const wchar_t* name)
    {
        XmlNode* c = FindChild(parent, name);
        return c ? c->value.c_str() : nullptr;
    }

    inline bool IsTrue(const wchar_t* s) { return !_wcsicmp(s, L"true"); }

    // append a backslash unless the string already ends with one ("" becomes "\").
    inline void AddBackslash(wchar_t* p)
    {
        if (!p)
            return;
        if (!*p)
        {
            p[0] = L'\\';
            p[1] = 0;
            return;
        }
        size_t n = wcsnlen(p, 260);
        if (n == 0 || n >= 259)
            return;
        if (p[n - 1] != L'\\')
        {
            p[n] = L'\\';
            p[n + 1] = 0;
        }
    }

    // strip up to two trailing '\', '/' or '"' characters (checks the last and the
    // second-to-last character) and a leading '"'.
    inline void TrimPath(wchar_t* p)
    {
        if (!p || !*p)
            return;
        size_t n = wcsnlen(p, 260);
        if (n < 260)
        {
            auto sep = [](wchar_t c) { return c == L'\\' || c == L'/' || c == L'"'; };
            if (n != 0 && sep(p[n - 1]))
                p[n - 1] = 0;
            if (n >= 2 && sep(p[n - 2]))
                p[n - 2] = 0;
        }
        if (*p == L'"')
        {
            size_t i = 0;
            while (i < 259 && p[i + 1])
            {
                p[i] = p[i + 1];
                ++i;
            }
            p[i] = 0;
        }
    }

    // replace the path with its file name (text after the last '\' or '/').
    inline void ExtractFileName(wchar_t* p)
    {
        if (!p || !*p)
            return;
        wchar_t* s = wcsrchr(p, L'\\');
        if (!s)
            s = wcsrchr(p, L'/');
        if (!s)
            return;
        WZStrCopy(p, 260, s + 1);
    }

    // bounded wcscat into a 260 character buffer.
    inline void Append(wchar_t* dst, const wchar_t* src)
    {
        size_t n = wcsnlen(dst, 260);
        if (n < 260)
            WZStrCopy(dst + n, 260 - n, src);
    }

    // "%s%s" with a leading backslash of the relative path dropped (the "&v[2*(c==92)-2]" idiom).
    inline void BuildImagePath(wchar_t* dst, const wchar_t* dirWithSlash, const wchar_t* relative)
    {
        WZStrPrintf(dst, 260, L"%s%s", dirWithSlash, relative + (relative[0] == L'\\' ? 1 : 0));
    }

    // delete everything below 'path' and then the directory itself.
    inline BOOL DeleteDirectoryTree(const wchar_t* path)
    {
        wchar_t pattern[260] = {};
        WZStrPrintf(pattern, 260, L"%s\\*", path);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                wchar_t full[260] = {};
                WZStrPrintf(full, 260, L"%s\\%s", path, fd.cFileName);
                if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L".."))
                {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY)
                        SetFileAttributesW(full, fd.dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                        DeleteDirectoryTree(full);
                    else
                        DeleteFileW(full);
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        return RemoveDirectoryW(path);
    }
}
