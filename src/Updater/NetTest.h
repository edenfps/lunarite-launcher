// NetTest.h - download speed probe (NetTestDummyFile.dmy), result stored in the registry value "NPT".
#pragma once
#include "FileDownloader.h"

// Object size 0x434 (class name not recoverable from RTTI). Created by WZUpdater::StartNetTest.
class NetTest
{
public:
    NetTest(UpdateServerInfo* server, TestNetworkInfo* test);
    ~NetTest();

    void Start(const wchar_t* downloadDir);
    void MarkDone() { m_done = 1; }                       // no TestNetworkInfo: nothing to wait for
    void Stop();                                          // break request (see WZUpdater::Break)
    bool IsDone() const { return m_done != 0; }

private:
    static unsigned __stdcall DownloadThreadProc(void* self);
    static unsigned __stdcall MonitorThreadProc(void* self);
    void RunDownload();
    void RunMonitor();

    FileDownloadManager* m_downloader;
    UpdateServerInfo*          m_server;
    TestNetworkInfo*     m_test;
    wchar_t              m_downloadDir[260];
    wchar_t              m_localFile[260];
    volatile LONG        m_abort;
    volatile LONG        m_done;
    volatile DWORD       m_startTick;
    volatile DWORD       m_endTick;
    HANDLE               m_downloadThread;
    HANDLE               m_monitorThread;
};
