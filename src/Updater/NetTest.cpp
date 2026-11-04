#include "NetTest.h"
#include "PathUtil.h"
#include "UpdaterRegistry.h"
#include <process.h>
#include <vector>

NetTest::NetTest(UpdateServerInfo* server, TestNetworkInfo* test)
    : m_downloader(nullptr), m_server(server), m_test(test), m_abort(0), m_done(0), m_startTick(0), m_endTick(0),
      m_downloadThread(nullptr), m_monitorThread(nullptr)
{
    memset(m_downloadDir, 0, sizeof(m_downloadDir));
    memset(m_localFile, 0, sizeof(m_localFile));
}

NetTest::~NetTest()
{
    if (m_downloadThread)
    {
        TerminateThread(m_downloadThread, 0);
        CloseHandle(m_downloadThread);
        m_downloadThread = nullptr;
    }
    if (m_monitorThread)
    {
        TerminateThread(m_monitorThread, 0);
        CloseHandle(m_monitorThread);
        m_monitorThread = nullptr;
    }
    delete m_downloader;
    m_downloader = nullptr;
}

void NetTest::Start(const wchar_t* downloadDir)
{
    if (!m_test->Enabled)
    {
        m_done = 1;
        return;
    }
    PathUtil::Copy(m_downloadDir, 260, downloadDir);
    unsigned id = 0;
    m_downloadThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, DownloadThreadProc, this, 0, &id));
    m_monitorThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, MonitorThreadProc, this, 0, &id));
}

void NetTest::Stop()
{
    m_abort = 1;
    if (m_downloader)
        m_downloader->Break();
}

unsigned __stdcall NetTest::DownloadThreadProc(void* self)
{
    static_cast<NetTest*>(self)->RunDownload();
    return 0;
}

unsigned __stdcall NetTest::MonitorThreadProc(void* self)
{
    static_cast<NetTest*>(self)->RunMonitor();
    return 0;
}

// downloads the dummy file into <downloadDir>\<file name> and records the start/end ticks.
void NetTest::RunDownload()
{
    m_test->Testing = 1;

    wchar_t dir[260] = {};
    PathUtil::Copy(dir, 260, m_downloadDir);
    PathUtil::AddBackslash(dir);
    wchar_t fileName[260] = {};
    PathUtil::Copy(fileName, 260, m_test->DownloadFile);
    PathUtil::GetFileName(fileName);
    WZStrPrintf(m_localFile, 260, L"%s%s", dir, fileName);

    std::vector<FileInfo*> files;
    FileInfo file;
    file.Set(m_test->DownloadFile, m_localFile, m_test->DownloadFile);
    files.push_back(&file);

    m_startTick = GetTickCount();
    m_downloader = new FileDownloadManager(nullptr);
    m_downloader->DownloadFiles(files, m_server);
    m_endTick = GetTickCount();
}

// lets the download run for at most 5 seconds, then stores bytes/second.
void NetTest::RunMonitor()
{
    DWORD now, start;
    do
    {
        Sleep(1);
        now = GetTickCount();
        start = m_startTick;
    } while ((!start || now - start < 5000) && !m_endTick);

    if (m_downloader)
        m_downloader->Break();
    while (!m_endTick)
        Sleep(10);

    unsigned __int64 bytes = PathUtil::GetFileSize64(m_localFile);
    DWORD seconds = (m_endTick - m_startTick) / 1000;
    if (!seconds)
        seconds = 1;
    UpdaterRegistry::SetDword(m_test->RegistrySubKey, m_test->RegistryRootKey, m_test->ResultValueName,
                              static_cast<DWORD>(bytes / seconds));
    m_done = 1;
    m_test->Testing = 0;
}
