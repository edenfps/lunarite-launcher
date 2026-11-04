#include "FileCheckerManager.h"
#include "PathUtil.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <string>
#include <unordered_map>

namespace
{
    struct CachedCrc
    {
        unsigned __int64 size;
        unsigned __int64 writeTime;
        DWORD crc;
    };
    typedef std::unordered_map<std::wstring, CachedCrc> CrcCache;

    // One line per file: <size> <last-write FILETIME> <CRC %X> <relative path>
    void LoadCrcCache(const wchar_t* path, CrcCache& cache)
    {
        FILE* in = nullptr;
        if (_wfopen_s(&in, path, L"rt, ccs=UTF-8") != 0 || !in)
            return;
        wchar_t line[400];
        while (fgetws(line, 400, in))
        {
            CachedCrc entry = {};
            int consumed = 0;
            if (swscanf_s(line, L"%llu %llu %lX %n", &entry.size, &entry.writeTime, &entry.crc, &consumed) < 3 || !consumed)
                continue;
            std::wstring name = line + consumed;
            while (!name.empty() && (name.back() == L'\n' || name.back() == L'\r'))
                name.pop_back();
            if (!name.empty())
                cache[name] = entry;
        }
        fclose(in);
    }

    void SaveCrcCache(const wchar_t* path, const CrcCache& cache)
    {
        std::wstring temp = std::wstring(path) + L".tmp";
        FILE* out = nullptr;
        if (_wfopen_s(&out, temp.c_str(), L"wt, ccs=UTF-8") != 0 || !out)
            return;
        for (const auto& entry : cache)
            fwprintf(out, L"%llu %llu %lX %s\n", entry.second.size, entry.second.writeTime, entry.second.crc,
                     entry.first.c_str());
        fclose(out);
        MoveFileExW(temp.c_str(), path, MOVEFILE_REPLACE_EXISTING);
    }
}

CFileCheckerManager::CFileCheckerManager(IUpdaterStateEvent* listener) : breakFlag(0)
{
    m_sender.listener = listener;
}

bool CFileCheckerManager::Ok()
{
    if (breakFlag)
        m_result.Set(1, 0, L"[CFileCheckerManager] User Break");
    return m_result.IsSuccess();
}

WZResult CFileCheckerManager::CheckFiles(PatcherInfo* patcher, UpdateServerInfo* server, std::vector<FileInfo*>& mismatches)
{
    if (!patcher || !server)
    {
        m_result.Set(0x40000002, 0, L"[CFileCheckerManager::CheckFiles] Fail : Not Patcher or Server Info");
        return m_result;
    }
    if (patcher->UseCheckFiles)
    {
        if (m_sender.listener)
            m_sender.listener->OnCheckFilesBegin();
        unsigned __int64 total = 0;
        if (CheckFullFiles(&total, patcher, mismatches))
            m_result.SetSuccess();
        else
            m_result.Set(0x40000007, 0, L"[CFileCheckerManager::CheckFiles] Fail : CheckFullFiles");
        m_sender.CheckFilesEnd(m_result, static_cast<int>(mismatches.size()), total);
        if (!Ok())
            return m_result;
    }
    m_result.SetSuccess();
    return m_result;
}

int CFileCheckerManager::CountLines(const wchar_t* file)
{
    std::wifstream in(file, std::ios::in | std::ios::binary);
    if (!in.is_open())
        return 0;
    int lines = 0;
    std::vector<wchar_t> buffer(2048);
    while (!in.eof() && !breakFlag)
    {
        std::fill(buffer.begin(), buffer.end(), L'\0');
        in.getline(buffer.data(), 2048);
        ++lines;
    }
    in.close();
    return breakFlag == 0 ? lines : 0;
}

bool CFileCheckerManager::CheckFullFiles(unsigned __int64* totalSize, PatcherInfo* patcher,
                                         std::vector<FileInfo*>& mismatches)
{
    // remote directory = directory part of the check list URL, without leading slash
    wchar_t remoteDir[260] = {};
    PathUtil::Copy(remoteDir, 260, patcher->CheckListFileUrl);
    PathUtil::RemoveFileSpec(remoteDir);
    if (remoteDir[0] == L'\\' || remoteDir[0] == L'/')
        wmemmove(remoteDir, remoteDir + 1, wcslen(remoteDir));

    wchar_t root[260] = {};
    PathUtil::Copy(root, 260, patcher->TargetDir);
    PathUtil::AddBackslash(root);

    int total = CountLines(patcher->CheckListTempPath);
    if (!total)
        return false;

    std::wifstream in(patcher->CheckListTempPath, std::ios::in | std::ios::binary);
    if (!in.is_open())
        return false;

    wchar_t cachePath[260] = {};
    WZStrPrintf(cachePath, 260, L"%sWZLauncher.crc", root);
    CrcCache cache, seen;
    LoadCrcCache(cachePath, cache);
    DWORD table[256];
    PathUtil::BuildCrcTable(table);

    int index = 0;
    while (!in.eof() && !breakFlag)
    {
        ++index;
        if (m_sender.listener)
            m_sender.listener->OnCheckFilesProgress(total, index);

        wchar_t path[260] = {};
        in.getline(path, 260, L'"');
        in.getline(path, 260, L'"');
        if (path[0])
        {
            if (path[0] == L'\\' || path[0] == L'/')
                wmemmove(path, path + 1, wcslen(path));

            wchar_t sizeText[260] = {};
            in.getline(sizeText, 260, L' ');
            in.getline(sizeText, 260, L' ');
            unsigned __int64 size = static_cast<unsigned __int64>(static_cast<__int64>(_wtoi(sizeText)));

            wchar_t crcText[260] = {};
            in.getline(crcText, 260, L'\r');

            wchar_t remote[260] = {};
            WZStrPrintf(remote, 260, L"/%s/%s", remoteDir, path);
            PathUtil::ToSlash(remote);

            wchar_t local[260] = {};
            WZStrPrintf(local, 260, L"%s%s", root, path);
            PathUtil::ToBackslash(local);

            WIN32_FILE_ATTRIBUTE_DATA attributes = {};
            bool exists = GetFileAttributesExW(local, GetFileExInfoStandard, &attributes) != 0;
            unsigned __int64 localSize = exists
                ? (static_cast<unsigned __int64>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow : 0;
            unsigned __int64 writeTime = exists
                ? (static_cast<unsigned __int64>(attributes.ftLastWriteTime.dwHighDateTime) << 32)
                    | attributes.ftLastWriteTime.dwLowDateTime : 0;
            bool mismatch = !exists || localSize != size;
            if (!mismatch)
            {
                DWORD crc;
                auto cached = cache.find(path);
                if (cached != cache.end() && cached->second.size == localSize && cached->second.writeTime == writeTime)
                    crc = cached->second.crc;
                else
                    crc = PathUtil::FileCrc32(local, table);
                seen[path] = CachedCrc{ localSize, writeTime, crc };
                wchar_t actual[260] = {};
                WZStrPrintf(actual, 260, L"%X", crc);
                mismatch = _wcsicmp(crcText, actual) != 0;
            }
            if (mismatch)
            {
                FileInfo* file = new FileInfo;
                file->Set(path, local, remote);
                file->size = size;
                mismatches.push_back(file);
                *totalSize += size;
            }
        }
        else
        {
            in.getline(path, 260, L'\r');
        }
    }
    in.close();
    if (!breakFlag)
        SaveCrcCache(cachePath, seen);
    return true;
}
