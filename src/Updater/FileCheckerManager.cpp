#include "FileCheckerManager.h"
#include "PathUtil.h"
#include <algorithm>
#include <cstdlib>
#include <cwchar>
#include <fstream>

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

            bool mismatch = PathUtil::GetFileSize64(local) != size;
            if (!mismatch)
            {
                DWORD table[256];
                PathUtil::BuildCrcTable(table);
                DWORD crc = PathUtil::FileCrc32(local, table);
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
    return true;
}
