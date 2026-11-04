// FileDownloader.h - FileDownloader (single file) and FileDownloadManager (file list).
#pragma once
#include <vector>
#include "Connecter.h"

// Object size 0x838. Downloads one FileInfo over a connecter into FileInfo::localPath.
class FileDownloader
{
public:
    FileDownloader(IDownloaderStateEvent* listener, UpdateServerInfo* server, FileInfo* file);
    ~FileDownloader();                                   // (CloseAll)

    WZResult Download();
    void CloseAll();

    volatile LONG breakFlag;

private:
    IConnecter* CreateConnecter();
    WZResult    CreateConnection();
    void        CreateConnectionThread();                // ( thread entry)
    WZResult    CreateLocalFile();
    WZResult    TransferRemoteFile();
    WZResult    WriteLocalFile(const void* data, DWORD size);
    bool        Ok();
    static unsigned __stdcall ConnectionThreadProc(void* self);

    WZResult               m_result;
    IDownloaderStateEvent* m_listener;
    UpdateServerInfo*            m_server;
    FileInfo*              m_file;
    IConnecter*            m_connecter;
    HINTERNET              m_session;
    HINTERNET              m_connection;
    HINTERNET              m_remoteFile;
    HANDLE                 m_localFile;
    __int64                m_remoteSize;
};

// Object size 0x81C. Vtable. Downloads every FileInfo in a list, forwarding events.
class FileDownloadManager : public IDownloaderStateEvent
{
public:
    explicit FileDownloadManager(IUpdaterStateEvent* listener);
    ~FileDownloadManager();

    WZResult DownloadFiles(std::vector<FileInfo*>& files, UpdateServerInfo* server);
    void Break();                                               // see WZUpdater::Break

    void OnStart(const FileInfo* file, unsigned __int64 size) override;
    void OnProgress(const FileInfo* file, unsigned __int64 done) override;
    void OnEnd(WZResult result) override;

    volatile LONG   breakFlag;
    FileDownloader* current;

private:
    WZResult            m_result;
    IUpdaterStateEvent* m_listener;
};
