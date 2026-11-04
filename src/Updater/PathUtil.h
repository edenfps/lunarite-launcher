// PathUtil.h - path / file helpers used by the updater.
#pragma once
#include "Common/WZCommon.h"

namespace PathUtil
{
    void AddBackslash(wchar_t* path);
    void RemoveBackslash(wchar_t* path);
    void RemoveFileSpec(wchar_t* path);
    void GetFileName(wchar_t* path);
    void ToBackslash(wchar_t* path);
    void ToSlash(wchar_t* path);
    bool ReadFileLastLine(const wchar_t* file, wchar_t* out1024);
    bool WriteNewFile(const wchar_t* file, const wchar_t* text, size_t len);
    bool CreateDirectoryRecursive(const wchar_t* path, bool stripFileName);
    bool RemoveDirectoryRecursive(const wchar_t* path);
    unsigned __int64 GetFileSize64(const wchar_t* file);
    void BuildCrcTable(DWORD table[256]);
    DWORD FileCrc32(const wchar_t* file, const DWORD table[256]);
    void Copy(wchar_t* dst, size_t cch, const wchar_t* src);            // inlined bounded copy
}
