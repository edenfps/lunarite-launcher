// FileDownloader.h - FileDownloader (single file) and FileDownloadManager (file list).
#pragma once
#include <map>
#include <set>
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
// Lunarite: several files at once (DownloadThreads workers), because one file per connection at a
// time over a long or lossy path left most of the line idle. Events reach the listener one at a
// time; a file's End is sent once it finished (after retries).
class FileDownloadManager : public IDownloaderStateEvent
{
public:
    static const int DownloadThreads = 6;
    static const int DownloadAttempts = 3;

    explicit FileDownloadManager(IUpdaterStateEvent* listener);
    ~FileDownloadManager();

    WZResult DownloadFiles(std::vector<FileInfo*>& files, UpdateServerInfo* server);
    void Break();                                               // see WZUpdater::Break

    void OnStart(const FileInfo* file, unsigned __int64 size) override;
    void OnProgress(const FileInfo* file, unsigned __int64 done) override;
    void OnEnd(WZResult result) override;

    volatile LONG   breakFlag;

private:
    static unsigned __stdcall WorkerProc(void* self);
    void Worker();
    void FileFinished(const FileInfo* file, const WZResult& result);

    WZResult            m_result;
    IUpdaterStateEvent* m_listener;
    CRITICAL_SECTION    m_lock;                                 // listener events and byte counts
    CRITICAL_SECTION    m_activeLock;                           // m_active only: Break comes from the
                                                                // UI thread, which m_lock may be waiting on
    std::vector<FileInfo*>* m_files;
    UpdateServerInfo*   m_server;
    volatile LONG       m_next;
    unsigned __int64    m_doneBytes;                            // finished files
    std::map<const FileInfo*, unsigned __int64> m_inFlight;     // bytes of files still downloading
    std::set<FileDownloader*> m_active;                         // for Break
};
