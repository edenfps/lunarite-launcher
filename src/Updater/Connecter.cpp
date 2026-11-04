#include "Connecter.h"
#include "PathUtil.h"
#include <cstdlib>

#pragma comment(lib, "wininet.lib")

// InternetOpen user agent = file name of the running exe.
static HINTERNET OpenSession()
{
    wchar_t module[260] = {};
    GetModuleFileNameW(nullptr, module, 260);
    PathUtil::GetFileName(module);
    return InternetOpenW(module, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
}

// ---------------------------------------------------------------- HTTP

WZResult HTTPConnecter::CreateSession(HINTERNET* session)
{
    *session = OpenSession();
    if (*session)
        m_result.SetSuccess();
    else
        m_result.Set(0x20000009, GetLastError(), L"[HTTPConnecter::CreateSession] Fail : InternetOpen, FileName = %s",
                     m_file->remotePath);
    return m_result;
}

WZResult HTTPConnecter::CreateConnection(HINTERNET* session, HINTERNET* connection)
{
    *connection = InternetConnectW(*session, m_server->ServerURL, m_server->Port, m_server->Account, m_server->Password,
                                   INTERNET_SERVICE_HTTP, 0, reinterpret_cast<DWORD_PTR>(this));
    if (*connection)
        m_result.SetSuccess();
    else
        m_result.Set(0x2000000A, GetLastError(),
                     L"[HTTPConnecter::CreateConnection] Fail : InternetConnect, FileName = %s", m_file->remotePath);
    return m_result;
}

WZResult HTTPConnecter::OpenRemoteFile(HINTERNET* connection, HINTERNET* file, __int64* size)
{
    *file = HttpOpenRequestW(*connection, L"GET", m_file->remotePath, L"HTTP/1.0", nullptr, nullptr,
                             INTERNET_FLAG_RELOAD, 0);
    if (!*file)
    {
        m_result.Set(0x2000000C, GetLastError(),
                     L"[HTTPConnecter::OpenRemoteFile] Fail : HttpOpenRequest, FileName = %s", m_file->remotePath);
        return m_result;
    }
    // The patch server remembers each address's language for the game server's own messages.
    const wchar_t* language = WZUserIsKorean() ? L"Accept-Language: ko\r\n" : L"Accept-Language: en\r\n";
    HttpSendRequestW(*file, language, static_cast<DWORD>(-1), nullptr, 0);
    wchar_t buffer[32] = {};
    DWORD length = 32;
    if (!HttpQueryInfoW(*file, HTTP_QUERY_STATUS_CODE, buffer, &length, nullptr))
    {
        m_result.Set(0x2000000E, GetLastError(),
                     L"[HTTPConnecter::OpenRemoteFile] Fail : HttpQueryInfo - HTTP_QUERY_STATUS_CODE, FileName = %s",
                     m_file->remotePath);
        return m_result;
    }
    if (_wtoi64(buffer) != 200)
    {
        m_result.Set(0x2000000F, 0, L"[HTTPConnecter::OpenRemoteFile] Fail : Not HTTP_STATUS_OK, FileName = %s",
                     m_file->remotePath);
        return m_result;
    }
    memset(buffer, 0, sizeof(buffer));
    length = 32;
    if (!HttpQueryInfoW(*file, HTTP_QUERY_CONTENT_LENGTH, buffer, &length, nullptr))
    {
        m_result.Set(0x2000000B, GetLastError(),
                     L"[HTTPConnecter::OpenRemoteFile] Fail : HttpQueryInfo - HTTP_QUERY_CONTENT_LENGTH, FileName = %s",
                     m_file->remotePath);
        return m_result;
    }
    *size = _wtoi64(buffer);
    m_result.SetSuccess();
    return m_result;
}

WZResult HTTPConnecter::ReadRemoteFile(HINTERNET* file, void* buffer, DWORD* bytesRead)
{
    DWORD available = 0;
    InternetQueryDataAvailable(*file, &available, 0, 0);
    if (InternetReadFile(*file, buffer, m_server->PacketSize, bytesRead))
        m_result.SetSuccess();
    else
        m_result.Set(0x2000000D, GetLastError(),
                     L"[HTTPConnecter::ReadRemoteFile] Fail : InternetReadFile, FileName = %s", m_file->remotePath);
    return m_result;
}

// ---------------------------------------------------------------- FTP

WZResult FTPConnecter::CreateSession(HINTERNET* session)
{
    *session = OpenSession();
    if (*session)
        m_result.SetSuccess();
    else
        m_result.Set(0x20000009, GetLastError(), L"[FTPConnecter::CreateSession] Fail : InternetOpen, FileName = %s",
                     m_file->remotePath);
    return m_result;
}

WZResult FTPConnecter::CreateConnection(HINTERNET* session, HINTERNET* connection)
{
    *connection = InternetConnectW(*session, m_server->ServerURL, m_server->Port, m_server->Account, m_server->Password,
                                   INTERNET_SERVICE_FTP, m_server->IsPassive != 0 ? INTERNET_FLAG_PASSIVE : 0,
                                   reinterpret_cast<DWORD_PTR>(this));
    if (*connection)
        m_result.SetSuccess();
    else
        m_result.Set(0x2000000A, GetLastError(),
                     L"[FTPConnecter::CreateConnection] Fail : InternetConnect, FileName = %s", m_file->remotePath);
    return m_result;
}

WZResult FTPConnecter::OpenRemoteFile(HINTERNET* connection, HINTERNET* file, __int64* size)
{
    WIN32_FIND_DATAW fd;
    HINTERNET find = FtpFindFirstFileW(*connection, m_file->remotePath, &fd,
                                       INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
                                       reinterpret_cast<DWORD_PTR>(this));
    if (!find)
    {
        m_result.Set(0x2000000B, 0, L"[FTPConnecter::OpenRemoteFile] Fail : FtpFindFirstFile, FileName = %s",
                     m_file->remotePath);
        return m_result;
    }
    *size = (static_cast<__int64>(fd.nFileSizeHigh) << 32) + fd.nFileSizeLow;
    InternetCloseHandle(find);
    *file = FtpOpenFileW(*connection, m_file->remotePath, GENERIC_READ,
                         INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | FTP_TRANSFER_TYPE_BINARY,
                         reinterpret_cast<DWORD_PTR>(this));
    if (*file)
        m_result.SetSuccess();
    else
        m_result.Set(0x2000000C, GetLastError(), L"[FTPConnecter::OpenRemoteFile] Fail : FtpOpenFile, FileName = %s",
                     m_file->remotePath);
    return m_result;
}

WZResult FTPConnecter::ReadRemoteFile(HINTERNET* file, void* buffer, DWORD* bytesRead)
{
    if (InternetReadFile(*file, buffer, m_server->PacketSize, bytesRead))
        m_result.SetSuccess();
    else
        m_result.Set(0x2000000D, GetLastError(), L"[FTPConnecter::ReadRemoteFile] Fail : InternetReadFile, FileName = %s",
                     m_file->remotePath);
    return m_result;
}
