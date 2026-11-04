#include "FileDownloader.h"
#include "PathUtil.h"
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
    : breakFlag(0), current(nullptr), m_listener(listener)
{
}

FileDownloadManager::~FileDownloadManager()
{
    delete current;
    current = nullptr;
}

void FileDownloadManager::Break()
{
    breakFlag = 1;
    if (current)
        current->breakFlag = 1;
}

WZResult FileDownloadManager::DownloadFiles(std::vector<FileInfo*>& files, UpdateServerInfo* server)
{
    for (FileInfo* file : files)
    {
        if (!file)
            continue;
        current = new FileDownloader(this, server, file);
        m_result = current->Download();
        delete current;
        current = nullptr;
        if (breakFlag)
            m_result.Set(1, 0, L"[FileDownloadManager] User Break");
        if (m_result.code || m_result.subCode)
            return m_result;
    }
    return m_result;
}

void FileDownloadManager::OnStart(const FileInfo* file, unsigned __int64 size)
{
    if (m_listener)
        m_listener->OnDownloadStart(file, size);
}

void FileDownloadManager::OnProgress(const FileInfo* file, unsigned __int64 done)
{
    if (m_listener)
        m_listener->OnDownloadProgress(file, done);
}

void FileDownloadManager::OnEnd(WZResult result)
{
    if (m_listener)
        m_listener->OnDownloadEnd(result);
}
