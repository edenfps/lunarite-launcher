#include "FileDownloader.h"
#include "PathUtil.h"
#include "Common/Log.h"
#include <process.h>
#include <vector>

// ---------------------------------------------------------------- FileInfo

FileInfo::FileInfo()
{
    memset(name, 0, sizeof(name));
    memset(localPath, 0, sizeof(localPath));
    memset(remotePath, 0, sizeof(remotePath));
    memset(reserved, 0, sizeof(reserved));
    size = 0;
}

void FileInfo::Set(const wchar_t* nameIn, const wchar_t* local, const wchar_t* remote)
{
    PathUtil::Copy(name, 260, nameIn);
    PathUtil::Copy(localPath, 260, local);
    PathUtil::Copy(remotePath, 260, remote);
}

// ---------------------------------------------------------------- FileDownloader

FileDownloader::FileDownloader(IDownloaderStateEvent* listener, UpdateServerInfo* server, FileInfo* file)
    : breakFlag(0), m_listener(listener), m_server(server), m_file(file), m_connecter(nullptr), m_session(nullptr),
      m_connection(nullptr), m_remoteFile(nullptr), m_localFile(INVALID_HANDLE_VALUE), m_remoteSize(0)
{
}

FileDownloader::~FileDownloader()
{
    CloseAll();
}

void FileDownloader::CloseAll()
{
    if (m_localFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_localFile);
        m_localFile = INVALID_HANDLE_VALUE;
    }
    if (m_remoteFile)
    {
        InternetCloseHandle(m_remoteFile);
        m_remoteFile = nullptr;
    }
    if (m_connection)
    {
        InternetCloseHandle(m_connection);
        m_connection = nullptr;
    }
    if (m_session)
    {
        InternetCloseHandle(m_session);
        m_session = nullptr;
    }
    delete m_connecter;
    m_connecter = nullptr;
}

bool FileDownloader::Ok()
{
    if (breakFlag)
        m_result.Set(1, 0, L"[FileDownloader] User Break");
    return m_result.IsSuccess();
}

WZResult FileDownloader::Download()
{
    breakFlag = 0;
    m_remoteSize = 0;
    CloseAll();

    m_connecter = CreateConnecter();
    m_result = m_connecter->CreateSession(&m_session);
    if (!Ok())
        return m_result;

    m_result = CreateConnection();
    if (!Ok())
        return m_result;

    m_result = m_connecter->OpenRemoteFile(&m_connection, &m_remoteFile, &m_remoteSize);
    if (!Ok())
        return m_result;

    m_result = CreateLocalFile();
    if (!Ok())
        return m_result;

    m_file->size = static_cast<unsigned __int64>(m_remoteSize);
    return TransferRemoteFile();
}

IConnecter* FileDownloader::CreateConnecter()
{
    if (m_server->IsHttp == 1)
        return new HTTPConnecter(m_server, m_file);
    return new FTPConnecter(m_server, m_file);
}

// /
unsigned __stdcall FileDownloader::ConnectionThreadProc(void* self)
{
    static_cast<FileDownloader*>(self)->CreateConnectionThread();
    return 0;
}

void FileDownloader::CreateConnectionThread()
{
    m_result = m_connecter->CreateConnection(&m_session, &m_connection);
}

// the connect is bounded by UpdateServerInfo::timeoutMs when that is non-zero.
WZResult FileDownloader::CreateConnection()
{
    DWORD timeout = m_server->Reserved;
    if (!timeout)
    {
        CreateConnectionThread();
        return m_result;
    }
    unsigned threadId = 0;
    uintptr_t thread = _beginthreadex(nullptr, 0, ConnectionThreadProc, this, 0, &threadId);
    if (thread == static_cast<uintptr_t>(-1))
    {
        m_result.Set(0x20000003, GetLastError(), L"[FileDownloader::CreateConnection] Fail : _beginthreadex, FileName = %s",
                     m_file->remotePath);
        return m_result;
    }
    HANDLE handle = reinterpret_cast<HANDLE>(thread);
    if (WaitForSingleObject(handle, timeout) == WAIT_TIMEOUT)
    {
        InternetCloseHandle(m_session);
        m_session = nullptr;
        WaitForSingleObject(handle, INFINITE);
        CloseHandle(handle);
        m_result.Set(0x20000004, 0, L"[FileDownloader::CreateConnection] Fail : WAIT_TIMEOUT, FileName = %s",
                     m_file->remotePath);
    }
    else
    {
        CloseHandle(handle);
    }
    return m_result;
}

WZResult FileDownloader::CreateLocalFile()
{
    const wchar_t* local = m_file->localPath;
    if (GetFileAttributesW(local) == INVALID_FILE_ATTRIBUTES || m_server->NoResume)
    {
        PathUtil::CreateDirectoryRecursive(local, true);
        DWORD attr = GetFileAttributesW(local);
        if (attr & FILE_ATTRIBUTE_READONLY)
            SetFileAttributesW(local, attr & ~FILE_ATTRIBUTE_READONLY);
        m_localFile = CreateFileW(local, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_localFile == INVALID_HANDLE_VALUE)
            m_result.Set(0x20000006, GetLastError(), L"[FileDownloader::CreateLocalFile] Fail : CreateFile - %s", local);
        else
            m_result.SetSuccess();
    }
    else
    {
        m_result.Set(0x20000005, 0, L"[FileDownloader::CreateLocalFile] Fail : Local File Exists - %s", local);
    }
    return m_result;
}

WZResult FileDownloader::WriteLocalFile(const void* data, DWORD size)
{
    DWORD written = 0;
    if (WriteFile(m_localFile, data, size, &written, nullptr) && size == written)
        m_result.SetSuccess();
    else
        m_result.Set(0x20000008, GetLastError(), L"[FileDownloader::WriteLocalFile] Fail : WriteFile - %s",
                     m_file->localPath);
    return m_result;
}

WZResult FileDownloader::TransferRemoteFile()
{
    std::vector<unsigned char> buffer(m_server->PacketSize);
    unsigned __int64 total = 0;
    if (m_listener)
        m_listener->OnStart(m_file, static_cast<unsigned __int64>(m_remoteSize));
    for (;;)
    {
        DWORD bytes = 0;
        m_result = m_connecter->ReadRemoteFile(&m_remoteFile, buffer.data(), &bytes);
        if (!Ok())
            break;
        if (bytes)
        {
            m_result = WriteLocalFile(buffer.data(), bytes);
            if (!Ok())
                break;
            total += bytes;
            if (m_listener)
                m_listener->OnProgress(m_file, total);
        }
        if (bytes == 0 || breakFlag)
        {
            if (Ok())
            {
                if (total >= static_cast<unsigned __int64>(m_remoteSize))
                    m_result.SetSuccess();
                else
                    m_result.Set(0x20000007, 0, L"[FileDownloader::TransferRemoteFile] Fail : Different Down File Size, FileName = %s",
                                 m_file->remotePath);
            }
            break;
        }
    }
    if (m_listener)
        m_listener->OnEnd(m_result);
    return m_result;
}

// ---------------------------------------------------------------- FileDownloadManager

FileDownloadManager::FileDownloadManager(IUpdaterStateEvent* listener)
    : breakFlag(0), m_listener(listener), m_files(nullptr), m_server(nullptr), m_next(0), m_doneBytes(0)
{
    InitializeCriticalSection(&m_lock);
    InitializeCriticalSection(&m_activeLock);
}

FileDownloadManager::~FileDownloadManager()
{
    DeleteCriticalSection(&m_activeLock);
    DeleteCriticalSection(&m_lock);
}

void FileDownloadManager::Break()
{
    breakFlag = 1;
    EnterCriticalSection(&m_activeLock);
    for (FileDownloader* downloader : m_active)
        downloader->breakFlag = 1;
    LeaveCriticalSection(&m_activeLock);
}

WZResult FileDownloadManager::DownloadFiles(std::vector<FileInfo*>& files, UpdateServerInfo* server)
{
    m_files = &files;
    m_server = server;
    m_next = 0;
    m_doneBytes = 0;
    m_inFlight.clear();
    m_result.SetSuccess();

    // WinINet allows 2 connections per HTTP/1.1 server and 4 per HTTP/1.0 server by default.
    DWORD connections = DownloadThreads;
    InternetSetOptionW(nullptr, INTERNET_OPTION_MAX_CONNS_PER_SERVER, &connections, sizeof(connections));
    InternetSetOptionW(nullptr, INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER, &connections, sizeof(connections));

    std::vector<HANDLE> workers;
    for (int i = 0; i < DownloadThreads; ++i)
    {
        uintptr_t thread = _beginthreadex(nullptr, 0, WorkerProc, this, 0, nullptr);
        if (thread != 0 && thread != static_cast<uintptr_t>(-1))
            workers.push_back(reinterpret_cast<HANDLE>(thread));
    }
    if (workers.empty())
        Worker();
    for (HANDLE worker : workers)
    {
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
    }
    if (breakFlag && m_result.IsSuccess())
        m_result.Set(1, 0, L"[FileDownloadManager] User Break");
    return m_result;
}

unsigned __stdcall FileDownloadManager::WorkerProc(void* self)
{
    static_cast<FileDownloadManager*>(self)->Worker();
    return 0;
}

// Takes the next file until the list is done, the user breaks, or another file failed for good.
void FileDownloadManager::Worker()
{
    for (;;)
    {
        LONG index = InterlockedIncrement(&m_next) - 1;
        if (breakFlag || index >= static_cast<LONG>(m_files->size()))
            return;
        FileInfo* file = (*m_files)[index];
        if (!file)
            continue;
        WZResult result;
        for (int attempt = 1; attempt <= DownloadAttempts && !breakFlag; ++attempt)
        {
            FileDownloader downloader(this, m_server, file);
            EnterCriticalSection(&m_activeLock);
            m_active.insert(&downloader);
            LeaveCriticalSection(&m_activeLock);
            result = downloader.Download();
            EnterCriticalSection(&m_activeLock);
            m_active.erase(&downloader);
            LeaveCriticalSection(&m_activeLock);
            if (result.IsSuccess() || breakFlag)
                break;
            WZLog(L"Download attempt %d of %s failed: %X %s", attempt, file->name, result.code, result.message);
            for (int waited = 0; waited < (attempt * 5000 < 30000 ? attempt * 5000 : 30000) && !breakFlag; waited += 250)
                Sleep(250);
        }
        FileFinished(file, result);
        if (!result.IsSuccess())
        {
            // The update stops on a failed file, as it did with one file at a time.
            breakFlag = 1;
            Break();
            return;
        }
    }
}

void FileDownloadManager::FileFinished(const FileInfo* file, const WZResult& result)
{
    EnterCriticalSection(&m_lock);
    m_inFlight.erase(file);
    if (result.IsSuccess())
        m_doneBytes += file->size;
    else if (m_result.IsSuccess())
        m_result = result;
    if (m_listener)
        m_listener->OnDownloadEnd(result);
    LeaveCriticalSection(&m_lock);
}

void FileDownloadManager::OnStart(const FileInfo* file, unsigned __int64 size)
{
    EnterCriticalSection(&m_lock);
    m_inFlight[file] = 0;
    if (m_listener)
        m_listener->OnDownloadStart(file, size);
    LeaveCriticalSection(&m_lock);
}

void FileDownloadManager::OnProgress(const FileInfo* file, unsigned __int64 done)
{
    EnterCriticalSection(&m_lock);
    m_inFlight[file] = done;
    unsigned __int64 total = m_doneBytes;
    for (const auto& entry : m_inFlight)
        total += entry.second;
    if (m_listener)
    {
        m_listener->OnDownloadTotal(total);
        m_listener->OnDownloadProgress(file, done);
    }
    LeaveCriticalSection(&m_lock);
}

// A file's End goes out from FileFinished, once its retries are over.
void FileDownloadManager::OnEnd(WZResult result)
{
}
