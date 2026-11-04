#include "VersionManager.h"
#include "PathUtil.h"
#include <cstdlib>
#include <cwchar>
#include <fstream>

namespace
{
size_t TokenLength(const wchar_t* s, size_t max)
{
    size_t n = 0;
    while (n < max && s[n])
        ++n;
    return n;
}

// A token that fills its whole buffer (no terminator) is treated as unusable.
bool UsableToken(const wchar_t* token)
{
    size_t len = TokenLength(token, 50);
    return len < 50 && len >= 2 && token[0] != L'\r';
}

void ReadRestOfLine(std::wifstream& in)
{
    wchar_t rest[1024] = {};
    in.getline(rest, 1024);
}

std::wifstream OpenText(const wchar_t* path)
{
    return std::wifstream(path, std::ios::in | std::ios::binary);
}
}

VersionManager::VersionManager(IUpdaterStateEvent* listener) : breakFlag(0)
{
    m_sender.listener = listener;
}

bool VersionManager::Ok()
{
    if (breakFlag)
        m_result.Set(1, 0, L"[VersionManager] User Break");
    return m_result.IsSuccess();
}

WZResult VersionManager::GetUpdateState(VersionInfo* info)
{
    if (!info)
    {
        m_result.Set(0x10000002, 0, L"[VersionManager::GetUpdateState] Fail : Not Version Info");
        return m_result;
    }
    m_result = GetLastVersion(info);
    if (!Ok())
        return m_result;
    m_result = GetCurrentVersion(info);
    if (!Ok())
        return m_result;
    m_result = CheckValidCurrentVersion(info);
    if (!Ok())
        return m_result;
    info->State = _wcsicmp(info->ServerVersion, info->LocalVersion) == 0;
    m_result.SetSuccess();
    return m_result;
}

// the current version is the last line of the local version file, cut at the first space.
WZResult VersionManager::GetCurrentVersion(VersionInfo* info)
{
    wchar_t line[1024] = {};
    if (PathUtil::ReadFileLastLine(info->LocalVersionFile, line))
    {
        wchar_t* space = wcschr(line, L' ');
        if (space)
            *space = 0;
        PathUtil::Copy(info->LocalVersion, 260, line);
        m_result.SetSuccess();
    }
    else
    {
        m_result.Set(0x10000004, 0,
                     L"[VersionManager::GetCurrentVersion] Fail : Path::ReadFileLastLine, FileName : %s",
                     info->LocalVersionFile);
    }
    return m_result;
}

// the latest version is the greatest <toVersion> (case-insensitive string order) in the
// downloaded version file, starting from the current version.
WZResult VersionManager::GetLastVersion(VersionInfo* info)
{
    std::wifstream in = OpenText(info->TempVersionFile);
    if (!in.is_open())
    {
        m_result.Set(0x10000003, GetLastError(), L"[VersionManager::GetLastVersion] Fail : stream.is_open - %s",
                     info->TempVersionFile);
        return m_result;
    }
    wchar_t best[1024] = {};
    PathUtil::Copy(best, 260, info->LocalVersion);
    while (!in.eof() && !breakFlag && !in.fail())
    {
        wchar_t from[50] = {};
        wchar_t to[50] = {};
        in.getline(from, 50, L' ');
        if (UsableToken(from))
        {
            in.getline(to, 50, L' ');
            if (_wcsicmp(to, best) >= 0)
                PathUtil::Copy(best, 1024, to);
        }
        ReadRestOfLine(in);
    }
    in.close();
    PathUtil::Copy(info->ServerVersion, 260, best);
    m_result.SetSuccess();
    return m_result;
}

// the current version must appear (as either column) in the downloaded version file.
WZResult VersionManager::CheckValidCurrentVersion(VersionInfo* info)
{
    std::wifstream in = OpenText(info->TempVersionFile);
    if (!in.is_open())
    {
        m_result.Set(0x10000003, GetLastError(),
                     L"[VersionManager::CheckValidCurrentVersion] Fail : stream.is_open - %s",
                     info->TempVersionFile);
        return m_result;
    }
    while (!in.eof() && !breakFlag && !in.fail())
    {
        wchar_t from[50] = {};
        in.getline(from, 50, L' ');
        if (UsableToken(from))
        {
            wchar_t to[50] = {};
            if (!_wcsicmp(from, info->LocalVersion) || (in.getline(to, 50, L' '), !_wcsicmp(to, info->LocalVersion)))
            {
                m_result.SetSuccess();
                return m_result;
            }
        }
        ReadRestOfLine(in);
    }
    in.close();
    if (info->IsLauncher)
        return CheckValidLauncherCurrentVersion(info);
    m_result.Set(0x10000006, 0, L"[VersionManager::CheckValidCurrentVersion] Fail : Invalid Current Version - %s",
                 info->LocalVersion);
    return m_result;
}

WZResult VersionManager::CheckValidLauncherCurrentVersion(VersionInfo* info)
{
    wchar_t line[1024] = {};
    if (!PathUtil::ReadFileLastLine(info->LocalVersionFile, line))
    {
        m_result.Set(0x10000004, 0,
                     L"[VersionManager::CheckValidLauncherCurrentVersion] Fail : Path::ReadFileLastLine, FileName : %s",
                     info->LocalVersionFile);
        return m_result;
    }
    wchar_t* space = wcschr(line, L' ');
    if (space)
        *space = 0;
    PathUtil::Copy(info->LocalVersion, 260, line);
    size_t len = TokenLength(line, 1024);
    if (PathUtil::WriteNewFile(info->LocalVersionFile, line, len))
        m_result.SetSuccess();
    else
        m_result.Set(0x10000005, 0,
                     L"[VersionManager::CheckValidLauncherCurrentVersion] Fail : Path::WriteNewFile, FileName : %s",
                     info->LocalVersionFile);
    return m_result;
}

// walks from the current version to the latest one, following the biggest jump the
// version file offers at every step, and queues the patch file of every hop.
WZResult VersionManager::CheckVersion(VersionInfo* info, std::vector<FileInfo*>& files, const wchar_t* downloadDir)
{
    if (m_sender.listener)
        m_sender.listener->OnCheckVersionBegin();

    wchar_t from[260] = {};
    wchar_t to[260] = {};
    PathUtil::Copy(from, 260, info->LocalVersion);
    PathUtil::Copy(to, 260, info->LocalVersion);
    unsigned __int64 total = 0;

    while (!breakFlag)
    {
        FindBigNewVersion(info, from, to);
        if (!_wcsicmp(from, to))
            break;
        AddDownloadFileInfo(info, files, downloadDir, from, to, &total);
        PathUtil::Copy(from, 260, to);
        if (!_wcsicmp(to, info->ServerVersion))
            break;
    }

    if (breakFlag)
        m_result.Set(1, 0, L"[VersionManager::CheckVersion] User Break");
    else
        m_result.SetSuccess();
    if (files.empty())
        m_result.Set(0x10000006, 0, L"[VersionManager::CheckVersion] Invalid Version");

    m_sender.CheckVersionEnd(m_result, static_cast<int>(files.size()), total);
    return m_result;
}

// out = the greatest <toVersion> of all lines whose <fromVersion> equals 'from' (or 'from' itself).
void VersionManager::FindBigNewVersion(VersionInfo* info, const wchar_t* from, wchar_t* out)
{
    std::wifstream in = OpenText(info->TempVersionFile);
    if (!in.is_open())
    {
        m_result.Set(0x10000003, GetLastError(), L"[VersionManager::FindBigNewVersion] Fail : stream.is_open - %s",
                     info->TempVersionFile);
        return;
    }
    PathUtil::Copy(out, 260, from);
    while (!in.eof() && !breakFlag && !in.fail())
    {
        wchar_t first[50] = {};
        in.getline(first, 50, L' ');
        if (UsableToken(first) && !_wcsicmp(first, from))
        {
            wchar_t second[50] = {};
            in.getline(second, 50, L' ');
            if (_wcsicmp(out, second) < 0)
                PathUtil::Copy(out, 260, second);
        }
        ReadRestOfLine(in);
    }
    in.close();
    m_result.SetSuccess();
}

// a file is already queued when its relative name equals 'path' (case-insensitive).
bool VersionManager::ContainsLocalPath(const std::vector<FileInfo*>& files, const wchar_t* path)
{
    for (const FileInfo* f : files)
        if (f && !_wcsicmp(path, f->name))
            return true;
    return false;
}

// queues the patch file(s) of the hop from -> to.
void VersionManager::AddDownloadFileInfo(VersionInfo* info, std::vector<FileInfo*>& files, const wchar_t* downloadDir,
                                         const wchar_t* from, const wchar_t* to, unsigned __int64* totalSize)
{
    std::wifstream in = OpenText(info->TempVersionFile);
    if (!in.is_open())
    {
        m_result.Set(0x10000003, GetLastError(), L"[VersionManager::AddDownloadFileInfo] Fail : stream.is_open - %s",
                     info->TempVersionFile);
        return;
    }
    wchar_t remoteDir[260] = {};
    PathUtil::Copy(remoteDir, 260, info->VersionFileUrl);
    PathUtil::RemoveFileSpec(remoteDir);
    if (remoteDir[0] == L'\\' || remoteDir[0] == L'/')
        wmemmove(remoteDir, remoteDir + 1, wcslen(remoteDir));

    while (!in.eof() && !breakFlag && !in.fail())
    {
        wchar_t first[50] = {};
        in.getline(first, 50, L' ');
        if (TokenLength(first, 50) >= 50 || TokenLength(first, 50) < 2 || _wcsicmp(first, from))
        {
            ReadRestOfLine(in);
            continue;
        }
        wchar_t second[50] = {};
        in.getline(second, 50, L' ');
        if (_wcsicmp(second, to))
            continue;
        wchar_t path[260] = {};
        in.getline(path, 260, L' ');
        if (path[0] == L'\\' || path[0] == L'/')
            wmemmove(path, path + 1, wcslen(path));
        if (ContainsLocalPath(files, path))
            continue;
        wchar_t sizeText[20] = {};
        in.getline(sizeText, 20, L'\r');
        *totalSize += static_cast<unsigned __int64>(_wtoi(sizeText));

        wchar_t remote[260] = {};
        wchar_t local[260] = {};
        WZStrPrintf(remote, 260, L"/%s/%s", remoteDir, path);
        WZStrPrintf(local, 260, L"%s%s", downloadDir, path);
        FileInfo* file = new FileInfo;
        file->Set(path, local, remote);
        files.push_back(file);
    }
    in.close();
    m_result.SetSuccess();
}

WZResult VersionManager::ChangeCurrentVersion(VersionInfo* info)
{
    if (!info)
    {
        m_result.Set(0x10000002, 0, L"[VersionManager::ChangeCurrentVersion] Fail : Not Version Info");
        return m_result;
    }
    size_t len = TokenLength(info->ServerVersion, 260);
    if (PathUtil::WriteNewFile(info->LocalVersionFile, info->ServerVersion, len))
    {
        PathUtil::Copy(info->LocalVersion, 260, info->ServerVersion);
        info->State = 1;
        m_result.SetSuccess();
    }
    else
    {
        m_result.Set(0x10000005, 0, L"[VersionManager::ChangeCurrentVersion] Fail : Path::WriteNewFile, FileName : %s",
                     info->LocalVersionFile);
    }
    return m_result;
}
