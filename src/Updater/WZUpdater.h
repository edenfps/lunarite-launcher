// WZUpdater.h - orchestrates version check, download, patch and full-file verification.
#pragma once
#include <vector>
#include "FileCheckerManager.h"
#include "FileDownloader.h"
#include "FilePatcherManager.h"
#include "NetTest.h"
#include "UpdaterEvents.h"
#include "UpdaterInfo.h"
#include "VersionManager.h"

// Object size 0x858. Ctor, dtor.
class WZUpdater
{
public:
    explicit WZUpdater(IUpdaterStateEvent* listener);
    ~WZUpdater();

    WZResult Init(UpdateInfo* info);
    WZResult CheckVersion();                  // fetch the version file and compute VersionInfo::status
    WZResult Update();                        // full update run (check, download, patch)
    void     StartNetTest();
    WZResult UpdateVersion();                 // write the new version to the local version file
    void Break();
    WZResult GetResult() const { return m_result; }

private:
    void      ClearSubObjects();
    void      ClearFileList();
    bool      Ok();
    WZResult  DownloadVersionFile();
    WZResult  CheckLastPatch();
    WZResult  CheckVersionChain();
    WZResult  DownloadFiles();
    WZResult  PatchFiles();
    WZResult  Recover();                      // full file check after a failed run
    WZResult  CheckFullFiles();
    WZResult  DownloadCheckList();
    void      WaitNetTest();

    volatile LONG           m_break;
    WZResult                m_result;
    UpdaterStateEventSender m_sender;         // (listener)
    VersionManager*         m_versionManager;
    FileDownloadManager*    m_downloadManager;
    FilePatcherManager*     m_patcher;
    CFileCheckerManager*    m_checker;
    NetTest*                m_netTest;
    UpdateInfo*             m_updateInfo;
    UpdateServerInfo*             m_server;
    VersionInfo*            m_version;
    PatcherInfo*            m_patcherInfo;
    TestNetworkInfo*        m_testNetwork;
    PathInfo*               m_path;
    std::vector<FileInfo*>  m_files;
};
