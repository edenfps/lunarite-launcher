// VersionManager.h - version file parsing and patch-chain planning.
#pragma once
#include <vector>
#include "UpdaterEvents.h"
#include "UpdaterInfo.h"

// Object size 0xA20. The version file is a text file, one entry per line:
//     <fromVersion> <toVersion> <patchFile> <size>\r\n
// (fields separated by single spaces, version tokens at most 49 characters).
class VersionManager
{
public:
    explicit VersionManager(IUpdaterStateEvent* listener);
    ~VersionManager() {}

    WZResult GetUpdateState(VersionInfo* info);
    WZResult GetCurrentVersion(VersionInfo* info);
    WZResult GetLastVersion(VersionInfo* info);
    WZResult CheckValidCurrentVersion(VersionInfo* info);
    WZResult CheckValidLauncherCurrentVersion(VersionInfo* info);
    WZResult CheckVersion(VersionInfo* info, std::vector<FileInfo*>& files, const wchar_t* downloadDir);
    WZResult ChangeCurrentVersion(VersionInfo* info);

    volatile LONG breakFlag;

private:
    void FindBigNewVersion(VersionInfo* info, const wchar_t* from, wchar_t* out);
    void AddDownloadFileInfo(VersionInfo* info, std::vector<FileInfo*>& files, const wchar_t* downloadDir,
                             const wchar_t* from, const wchar_t* to, unsigned __int64* totalSize);
    static bool ContainsLocalPath(const std::vector<FileInfo*>& files, const wchar_t* path);
    bool Ok();

    WZResult                m_result;
    UpdaterStateEventSender m_sender;                        // (listener)
};
