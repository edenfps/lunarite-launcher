// Lunarite: crash reports (2026-11-13). When the last game session crashed, the launcher sends its log, and the
// minidump the game wrote, to the patch server (POST /huxley/Report/Crash?kind=log|dump, server: CrashReports.cs),
// so players do not have to find and send files themselves.
//
// A crashed session: HuxleyMMOGame\Logs\Launch.log holds "=== Critical error: ===" (appError still closes the log
// normally), or lacks the final "Log file closed" line (killed, frozen, power cut). The game rotates Launch.log to
// Launch-backup-*.log on its next start, so the launcher, which runs first, still sees the last session's log.
// Sent: the first 64 KB (machine, drivers, settings) and the last 448 KB. The minidump: the newest
// Binaries\huxley-v*.dmp written during that session. Each log is sent once (its size and write time are kept in
// HKCU\Software\Lunarite\LastCrashReport). Runs on its own thread; any failure is silently skipped.
// After a crash the same launch also re-hashes every game file (g_LunariteVerifyAllFiles, FileCheckerManager).
#include <windows.h>
#include <wininet.h>
#include <string>
#include <vector>

#pragma comment(lib, "wininet.lib")

namespace
{
    const wchar_t* kKey = L"Software\\Lunarite";
    const DWORD kHead = 64 * 1024, kTail = 448 * 1024, kMaxDump = 8 * 1024 * 1024;

    struct Job
    {
        std::wstring host, gameDir;
        INTERNET_PORT port;
    };

    bool ReadRange(HANDLE file, ULONGLONG offset, DWORD count, std::string& out)
    {
        LARGE_INTEGER at;
        at.QuadPart = (LONGLONG)offset;
        if (!SetFilePointerEx(file, at, nullptr, FILE_BEGIN))
            return false;
        size_t start = out.size();
        out.resize(start + count);
        DWORD read = 0;
        if (!ReadFile(file, &out[start], count, &read, nullptr))
            return false;
        out.resize(start + read);
        return true;
    }

    std::wstring Stamp(ULONGLONG size, FILETIME written)
    {
        wchar_t text[64];
        swprintf_s(text, L"%llu:%lu:%lu", size, written.dwHighDateTime, written.dwLowDateTime);
        return text;
    }

    std::wstring LastSent()
    {
        wchar_t value[64] = {};
        DWORD size = sizeof(value), type = 0;
        HKEY key;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, kKey, 0, KEY_READ, &key) == ERROR_SUCCESS)
        {
            if (RegQueryValueExW(key, L"LastCrashReport", nullptr, &type, (BYTE*)value, &size) != ERROR_SUCCESS
                || type != REG_SZ)
                value[0] = 0;
            RegCloseKey(key);
        }
        value[63] = 0;
        return value;
    }

    void RememberSent(const std::wstring& stamp)
    {
        HKEY key;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, kKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) == ERROR_SUCCESS)
        {
            RegSetValueExW(key, L"LastCrashReport", 0, REG_SZ, (const BYTE*)stamp.c_str(),
                           (DWORD)((stamp.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(key);
        }
    }

    bool Post(const Job& job, const wchar_t* kind, const std::string& body)
    {
        HINTERNET session = InternetOpenW(L"WZLauncher.exe", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!session)
            return false;
        bool ok = false;
        HINTERNET connection = InternetConnectW(session, job.host.c_str(), job.port, nullptr, nullptr,
                                                INTERNET_SERVICE_HTTP, 0, 0);
        if (connection)
        {
            std::wstring path = std::wstring(L"/huxley/Report/Crash?kind=") + kind;
            HINTERNET request = HttpOpenRequestW(connection, L"POST", path.c_str(), L"HTTP/1.0", nullptr, nullptr,
                                                 INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
            if (request)
            {
                const wchar_t* type = L"Content-Type: application/octet-stream\r\n";
                if (HttpSendRequestW(request, type, (DWORD)-1, (LPVOID)body.data(), (DWORD)body.size()))
                {
                    DWORD status = 0, length = sizeof(status);
                    ok = HttpQueryInfoW(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &length,
                                        nullptr) && status == 200;
                }
                InternetCloseHandle(request);
            }
            InternetCloseHandle(connection);
        }
        InternetCloseHandle(session);
        return ok;
    }

    // Newest Binaries\huxley-v*.dmp written at or after the session start (the log's creation time).
    bool NewestDump(const std::wstring& binaries, FILETIME sessionStart, std::wstring& found)
    {
        WIN32_FIND_DATAW data;
        HANDLE search = FindFirstFileW((binaries + L"huxley-v*.dmp").c_str(), &data);
        if (search == INVALID_HANDLE_VALUE)
            return false;
        FILETIME best = sessionStart;
        bool any = false;
        do
        {
            if (CompareFileTime(&data.ftLastWriteTime, &best) >= 0 && data.nFileSizeHigh == 0
                && data.nFileSizeLow > 0 && data.nFileSizeLow <= kMaxDump)
            {
                best = data.ftLastWriteTime;
                found = binaries + data.cFileName;
                any = true;
            }
        } while (FindNextFileW(search, &data));
        FindClose(search);
        return any;
    }

    DWORD WINAPI Run(LPVOID parameter)
    {
        Job* job = static_cast<Job*>(parameter);
        std::wstring logPath = job->gameDir + L"..\\HuxleyMMOGame\\Logs\\Launch.log";
        HANDLE file = CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            BY_HANDLE_FILE_INFORMATION info = {};
            GetFileInformationByHandle(file, &info);
            ULONGLONG size = ((ULONGLONG)info.nFileSizeHigh << 32) | info.nFileSizeLow;
            std::wstring stamp = Stamp(size, info.ftLastWriteTime);
            std::string tail;
            DWORD tailCount = (DWORD)min(size, (ULONGLONG)kTail);
            bool crashed = false;
            if (size > 0 && stamp != LastSent() && ReadRange(file, size - tailCount, tailCount, tail))
            {
                bool critical = tail.find("=== Critical error: ===") != std::string::npos;
                bool closed = tail.find("Log file closed") != std::string::npos;
                crashed = critical || !closed;
            }
            if (crashed)
            {
                std::string body = "Lunarite crash report\r\nLog: " + std::string(size > kHead + kTail ? "head+tail" : "whole")
                    + "\r\n\r\n";
                if (size > (ULONGLONG)kHead + kTail)
                {
                    ReadRange(file, 0, kHead, body);
                    body += "\r\n\r\n[... cut ...]\r\n\r\n";
                    body += tail;
                }
                else
                {
                    ReadRange(file, 0, (DWORD)size, body);
                }
                CloseHandle(file);
                file = INVALID_HANDLE_VALUE;
                if (Post(*job, L"log", body))
                {
                    RememberSent(stamp);
                    std::wstring dump;
                    if (NewestDump(job->gameDir, info.ftCreationTime, dump))
                    {
                        HANDLE dumpFile = CreateFileW(dump.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                                      OPEN_EXISTING, 0, nullptr);
                        if (dumpFile != INVALID_HANDLE_VALUE)
                        {
                            std::string bytes;
                            DWORD dumpSize = GetFileSize(dumpFile, nullptr);
                            if (dumpSize != INVALID_FILE_SIZE && ReadRange(dumpFile, 0, dumpSize, bytes))
                                Post(*job, L"dump", bytes);
                            CloseHandle(dumpFile);
                        }
                    }
                }
            }
            if (file != INVALID_HANDLE_VALUE)
                CloseHandle(file);
        }
        delete job;
        return 0;
    }
}

// Set before the file check when the last session crashed: that launch re-hashes every game file instead of
// trusting WZLauncher.crc (a file damaged in place keeps its size and write time, so the cache would pass it).
bool g_LunariteVerifyAllFiles = false;

namespace
{
    bool LastSessionCrashed(const std::wstring& gameDir)
    {
        std::wstring logPath = gameDir + L"..\\HuxleyMMOGame\\Logs\\Launch.log";
        HANDLE file = CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                  OPEN_EXISTING, 0, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        BY_HANDLE_FILE_INFORMATION info = {};
        GetFileInformationByHandle(file, &info);
        ULONGLONG size = ((ULONGLONG)info.nFileSizeHigh << 32) | info.nFileSizeLow;
        std::string tail;
        DWORD tailCount = (DWORD)min(size, (ULONGLONG)kTail);
        bool crashed = size > 0 && Stamp(size, info.ftLastWriteTime) != LastSent()
            && ReadRange(file, size - tailCount, tailCount, tail)
            && (tail.find("=== Critical error: ===") != std::string::npos
                || tail.find("Log file closed") == std::string::npos);
        CloseHandle(file);
        return crashed;
    }
}

// executablePath: the game exe (Binaries\HuxleyMMOGame.exe); host/port: the patch server.
void StartCrashReport(const wchar_t* host, unsigned short port, const wchar_t* executablePath)
{
    if (!host || !host[0] || !executablePath || !executablePath[0])
        return;
    std::wstring exe = executablePath;
    if (!exe.empty() && exe.front() == L'"')
        exe = exe.substr(1, exe.find(L'"', 1) - 1);
    size_t slash = exe.find_last_of(L'\\');
    if (slash == std::wstring::npos)
        return;
    std::wstring gameDir = exe.substr(0, slash + 1);
    if (LastSessionCrashed(gameDir))
        g_LunariteVerifyAllFiles = true;
    Job* job = new Job{ host, gameDir, (INTERNET_PORT)port };
    HANDLE thread = CreateThread(nullptr, 0, Run, job, 0, nullptr);
    if (thread)
        CloseHandle(thread);
    else
        delete job;
}
