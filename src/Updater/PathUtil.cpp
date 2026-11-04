#include "PathUtil.h"
#include <algorithm>
#include <cwchar>
#include <fstream>
#include <vector>

namespace PathUtil
{
void Copy(wchar_t* dst, size_t cch, const wchar_t* src)
{
    size_t i = 0;
    while (i < cch && src[i])
    {
        dst[i] = src[i];
        ++i;
    }
    if (i == cch)
        --i;
    dst[i] = 0;
}

static size_t BoundedLen(const wchar_t* s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n])
        ++n;
    return n;
}

void AddBackslash(wchar_t* path)
{
    if (!path)
        return;
    if (!*path)
    {
        path[0] = L'\\';
        path[1] = 0;
        return;
    }
    size_t len = BoundedLen(path, 260);
    if (len == 260)
        return;
    if (path[len - 1] != L'\\')
    {
        path[len] = L'\\';
        path[len + 1] = 0;
    }
}

void RemoveBackslash(wchar_t* path)
{
    if (!path || !*path)
        return;
    size_t len = BoundedLen(path, 260);
    if (len != 260)
    {
        if (len != 0)
        {
            wchar_t c = path[len - 1];
            if (c == L'\\' || c == L'/' || c == L'"')
                path[len - 1] = 0;
        }
        if (len >= 2)
        {
            wchar_t c = path[len - 2];
            if (c == L'\\' || c == L'/' || c == L'"')
                path[len - 2] = 0;
        }
    }
    if (*path == L'"')
    {
        size_t i = 0;
        while (i < 260 && path[i + 1])
        {
            path[i] = path[i + 1];
            ++i;
        }
        path[i] = 0;
    }
}

void RemoveFileSpec(wchar_t* path)
{
    if (!path || !*path)
        return;
    wchar_t* p = wcsrchr(path, L'\\');
    if (!p)
        p = wcsrchr(path, L'/');
    if (p)
    {
        *p = 0;
        return;
    }
    path[0] = L'.';
    path[1] = 0;
}

void GetFileName(wchar_t* path)
{
    if (!path || !*path)
        return;
    wchar_t* p = wcsrchr(path, L'\\');
    if (!p)
        p = wcsrchr(path, L'/');
    if (!p)
        return;
    const wchar_t* src = p + 1;
    size_t i = 0;
    while (i < 260 && src[i])
    {
        path[i] = src[i];
        ++i;
    }
    if (i == 260)
        --i;
    path[i] = 0;
}

void ToBackslash(wchar_t* path)
{
    if (!path || !*path)
        return;
    size_t len = BoundedLen(path, 260);
    for (size_t i = 0; i < len; ++i)
        if (path[i] == L'/')
            path[i] = L'\\';
}

void ToSlash(wchar_t* path)
{
    if (!path || !*path)
        return;
    size_t len = BoundedLen(path, 260);
    for (size_t i = 0; i < len; ++i)
        if (path[i] == L'\\')
            path[i] = L'/';
}

bool ReadFileLastLine(const wchar_t* file, wchar_t* out)
{
    if (!file || !out || !*file)
        return false;
    std::wifstream in(file, std::ios::in | std::ios::binary);
    if (!in.is_open())
        return false;
    std::vector<wchar_t> line(1024);
    while (!in.eof())
    {
        std::fill(line.begin(), line.end(), L'\0');
        in.getline(line.data(), 1024);
        size_t len = BoundedLen(line.data(), 1024);
        if (len != 0 && len != 1)
            Copy(out, 1024, line.data());
    }
    in.close();
    size_t len = BoundedLen(out, 1024);
    return len != 0 && len != 1;
}

bool WriteNewFile(const wchar_t* file, const wchar_t* text, size_t len)
{
    if (!text || !file)
        return false;
    std::wofstream out(file, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open())
        return false;
    out.write(text, static_cast<std::streamsize>(len));
    out.close();
    return true;
}

bool CreateDirectoryRecursive(const wchar_t* path, bool stripFileName)
{
    if (!path || !*path)
        return false;
    wchar_t dir[260] = {};
    Copy(dir, 260, path);
    if (stripFileName)
        RemoveFileSpec(dir);
    if (CreateDirectoryW(dir, nullptr))
        return true;
    wchar_t parent[260] = {};
    Copy(parent, 260, dir);
    wchar_t* last = wcsrchr(parent, L'\\');
    wchar_t* first = wcschr(parent, L'\\');
    return last && first && last != first && (*last = 0, CreateDirectoryRecursive(parent, false)) &&
           CreateDirectoryW(dir, nullptr);
}

bool RemoveDirectoryRecursive(const wchar_t* path)
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
                DWORD attr = fd.dwFileAttributes;
                if (attr & FILE_ATTRIBUTE_READONLY)
                    SetFileAttributesW(full, attr & ~FILE_ATTRIBUTE_READONLY);
                if (attr & FILE_ATTRIBUTE_DIRECTORY)
                    RemoveDirectoryRecursive(full);
                else
                    DeleteFileW(full);
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return RemoveDirectoryW(path) != FALSE;
}

unsigned __int64 GetFileSize64(const wchar_t* file)
{
    HANDLE h = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    LARGE_INTEGER size;
    size.QuadPart = 0;
    BOOL ok = GetFileSizeEx(h, &size);
    CloseHandle(h);
    return ok ? static_cast<unsigned __int64>(size.QuadPart) : 0;
}

// the standard reflected CRC-32 table (poly 0x04C11DB7, reflected 0xEDB88320)
void BuildCrcTable(DWORD table[256])
{
    for (DWORD i = 0; i < 256; ++i)
    {
        DWORD c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : (c >> 1);
        table[i] = c;
    }
}

// final (inverted) CRC-32 of the file, 0 when it cannot be read
DWORD FileCrc32(const wchar_t* file, const DWORD table[256])
{
    HANDLE h = CreateFileW(file, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    const DWORD kChunk = 200000000;
    DWORD size = GetFileSize(h, nullptr);
    std::vector<unsigned char> buffer(size > kChunk ? kChunk : size);
    DWORD crc = 0xFFFFFFFFu;
    DWORD remaining = size;
    while (remaining)
    {
        DWORD want = remaining < kChunk ? remaining : kChunk;
        DWORD got = 0;
        ReadFile(h, buffer.data(), want, &got, nullptr);
        if (size <= kChunk && got != size)
        {
            CloseHandle(h);
            return 0;
        }
        for (DWORD i = 0; i < want; ++i)
            crc = table[(buffer[i] ^ crc) & 0xFF] ^ (crc >> 8);
        remaining -= want;
    }
    CloseHandle(h);
    return ~crc;
}
}
