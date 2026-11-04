// FileCheckerManager.h - CFileCheckerManager: verifies installed files against FullFile.txt.
#pragma once
#include <vector>
#include "UpdaterEvents.h"
#include "UpdaterInfo.h"

// FullFile.txt (PatcherInfo::checkListRemote, downloaded to PatcherInfo::checkListLocal) has one
// line per installed file:   "relative/path" <size> <CRC32 in %X form>\r\n
// Lunarite: CRCs are cached in <game root>\WZLauncher.crc by size and last-write time, so a check
// only reads files that changed since the last one (the stock check read the whole 23 GB game).
class CFileCheckerManager
{
public:
    explicit CFileCheckerManager(IUpdaterStateEvent* listener);       // inline ctor in

    WZResult CheckFiles(PatcherInfo* patcher, UpdateServerInfo* server, std::vector<FileInfo*>& mismatches);

    volatile LONG breakFlag;

private:
    bool CheckFullFiles(unsigned __int64* totalSize, PatcherInfo* patcher, std::vector<FileInfo*>& mismatches);
    int  CountLines(const wchar_t* file);
    bool Ok();

    WZResult                m_result;
    UpdaterStateEventSender m_sender;                                  // (listener)
};
