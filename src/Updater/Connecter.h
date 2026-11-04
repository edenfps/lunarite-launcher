// Connecter.h - IConnecter / HTTPConnecter / FTPConnecter (WinINet transports).
#pragma once
#include "Common/WZCommon.h"
#include <wininet.h>
#include "UpdaterEvents.h"
#include "UpdaterInfo.h"

// Holds a WZResult, the UpdateServerInfo and the FileInfo it works on.
class IConnecter
{
public:
    IConnecter(UpdateServerInfo* server, FileInfo* file) : m_server(server), m_file(file) {}
    virtual ~IConnecter() {}

    virtual WZResult CreateSession(HINTERNET* session) = 0;                                        // [0]
    virtual WZResult CreateConnection(HINTERNET* session, HINTERNET* connection) = 0;              // [1]
    virtual WZResult OpenRemoteFile(HINTERNET* connection, HINTERNET* file, __int64* size) = 0;    // [2]
    virtual WZResult ReadRemoteFile(HINTERNET* file, void* buffer, DWORD* bytesRead) = 0;          // [3]

protected:
    WZResult    m_result;
    UpdateServerInfo* m_server;
    FileInfo*   m_file;
};

// Vtable:,,,.
class HTTPConnecter : public IConnecter
{
public:
    using IConnecter::IConnecter;
    WZResult CreateSession(HINTERNET* session) override;
    WZResult CreateConnection(HINTERNET* session, HINTERNET* connection) override;
    WZResult OpenRemoteFile(HINTERNET* connection, HINTERNET* file, __int64* size) override;
    WZResult ReadRemoteFile(HINTERNET* file, void* buffer, DWORD* bytesRead) override;
};

// Vtable:,,,.
class FTPConnecter : public IConnecter
{
public:
    using IConnecter::IConnecter;
    WZResult CreateSession(HINTERNET* session) override;
    WZResult CreateConnection(HINTERNET* session, HINTERNET* connection) override;
    WZResult OpenRemoteFile(HINTERNET* connection, HINTERNET* file, __int64* size) override;
    WZResult ReadRemoteFile(HINTERNET* file, void* buffer, DWORD* bytesRead) override;
};
