#include "WZLauncherController.h"

#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../Common/Registry.h"

namespace
{
    // Windows XP and Vista can't open the news page's https site (its host needs TLS 1.2, which their Internet
    // Explorer lacks), so the panel showed "Navigation to the webpage was canceled". There the page comes from the
    // update server over plain HTTP instead: same file name, at http://<ServerURL>:<Port>/ (the Lunarite patch server
    // serves a copy of the site's launcher page and its images there).
    bool OldInternetExplorer()
    {
        OSVERSIONINFOW os = { sizeof(os) };
#pragma warning(suppress : 4996)
        if (!GetVersionExW(&os))
            return false;
        return os.dwMajorVersion < 6 || (os.dwMajorVersion == 6 && os.dwMinorVersion == 0);
    }

    std::wstring WebUrlFor(GameEntry* entry)
    {
        if (!entry || !entry->ui)
            return std::wstring();
        std::wstring url = entry->ui->WebURL;
        UpdateServerInfo* server = entry->update ? entry->update->Server : nullptr;
        if (url.compare(0, 8, L"https://") != 0 || !server || !server->ServerURL[0] || !OldInternetExplorer())
            return url;
        size_t slash = url.find_last_of(L'/');
        std::wstring page = slash == std::wstring::npos ? std::wstring() : url.substr(slash + 1);
        if (page.empty())
            page = L"launcher.html";
        return std::wstring(L"http://") + server->ServerURL + L":" + std::to_wstring(server->Port) + L"/" + page;
    }

    // ensure a trailing backslash.
    void AddBackslash(wchar_t* path)
    {
        if (!path)
            return;
        size_t len = wcsnlen(path, 260);
        if (path[0] == 0)
        {
            path[0] = L'\\';
            path[1] = 0;
            return;
        }
        if (len == 260)
            len = 0;
        if (path[len - 1] != L'\\')
        {
            path[len] = L'\\';
            path[len + 1] = 0;
        }
    }

    // drop trailing '\', '/' or '"' (up to two) and a leading '"'.
    void TrimPathQuotes(wchar_t* path)
    {
        if (!path || !*path)
            return;
        size_t len = wcsnlen(path, 260);
        if (len < 260)
        {
            auto isSep = [](wchar_t c) { return c == L'\\' || c == L'/' || c == L'"'; };
            if (len != 0 && isSep(path[len - 1]))
                path[len - 1] = 0;
            if (len >= 2 && isSep(path[len - 2]))
                path[len - 2] = 0;
        }
        if (*path == L'"')
        {
            size_t i = 0;
            for (; i < 259 && path[i + 1]; ++i)
                path[i] = path[i + 1];
            path[i] = 0;
        }
    }

    // keep only the file name after the last '\' or '/'.
    void StripToFileName(wchar_t* path)
    {
        if (!path || !*path)
            return;
        wchar_t* p = wcsrchr(path, L'\\');
        if (!p)
            p = wcsrchr(path, L'/');
        if (!p)
            return;
        ++p;
        size_t i = 0;
        for (; i < 259 && p[i]; ++i)
            path[i] = p[i];
        path[i] = 0;
    }

    // bounded append into a 260 character buffer.
    void AppendString(wchar_t* buf, const wchar_t* tail)
    {
        size_t len = wcsnlen(buf, 260);
        if (len < 260)
            WZStrCopy(buf + len, 260 - len, tail);
    }

    bool IsNetError(DWORD code)
    {
        return (code & WZ_RESULT_NET_ERROR) != 0;
    }

    // Delete the game files named in <RemoveFiles> (';'-separated paths
    // relative to the game root; anything with ".." or a drive is ignored).
    void RemoveConfiguredFiles(const GameEntry* entry)
    {
        const PathInfo* path = entry->update->Path;
        wchar_t list[1024] = {};
        WZStrCopy(list, 1024, entry->removeFiles.c_str());
        wchar_t* ctx = nullptr;
        for (wchar_t* item = wcstok_s(list, L";", &ctx); item; item = wcstok_s(nullptr, L";", &ctx))
        {
            while (*item == L' ' || *item == L'\\' || *item == L'/')
                ++item;
            if (!*item || wcsstr(item, L"..") || wcschr(item, L':'))
                continue;
            wchar_t file[260] = {};
            WZStrCopy(file, 260, path->RootDir);
            AddBackslash(file);
            AppendString(file, item);
            if (DeleteFileW(file))
                WZLog(L"Removed %s", file);
        }
    }
}

WZLauncherController::WZLauncherController()
    : m_bUserBreak(FALSE)
    , m_bAutoExecute(FALSE)
    , m_hEvent(INVALID_HANDLE_VALUE)
    , m_pMonitorThread(nullptr)
    , m_pWorkThread(nullptr)
    , m_stage(STAGE_END_LAUNCHER_UPDATE)
    , m_updateState(0)
    , m_totalSize(0)
    , m_doneSize(0)
    , m_curFileSize(0)
    , m_totalFiles(0)
    , m_curFileIndex(0)
    , m_curFile(nullptr)
    , m_curFileBytes(0)
    , m_downloadTotal(~0ull)
{
    memset(m_logPath, 0, sizeof(m_logPath));
    memset(m_gameName, 0, sizeof(m_gameName));
    memset(m_curFileName, 0, sizeof(m_curFileName));

    // <exe dir>\WZLauncher.Log plus the start banner.
    WZStrCopy(m_logPath, 260, WZGetModuleDir().c_str());
    AddBackslash(m_logPath);
    AppendString(m_logPath, L"WZLauncher.Log");
    WZLogBanner(L"Start WZLauncher");
}

// (scalar deleting) /
WZLauncherController::~WZLauncherController()
{
    DestroyThreads();
}

void WZLauncherController::DestroyThreads()
{
    if (m_hEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hEvent);
        m_hEvent = INVALID_HANDLE_VALUE;
    }
    KillThread(m_pMonitorThread);
    KillThread(m_pWorkThread);
}

void WZLauncherController::KillThread(WZThread*& slot)
{
    if (!slot)
        return;
    if (slot->Handle() != INVALID_HANDLE_VALUE)
    {
        TerminateThread(slot->Handle(), 0);
        slot->ClearHandle();
    }
    delete slot;
    slot = nullptr;
}

void WZLauncherController::ReplaceThread(WZThread*& slot, WZThread::ThreadFunc fn, void* self)
{
    KillThread(slot);
    slot = new WZThread(fn, self);
    slot->Start();
}

// value following "/d" or "/D" on the command line.
void WZLauncherController::GetRequestedGameName(wchar_t* out)
{
    const std::vector<std::wstring>& args = m_settings.GetArguments();
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == L"/d" || args[i] == L"/D")
        {
            if (i + 1 < args.size())
                WZStrCopy(out, 260, args[i + 1].c_str());
            return;
        }
    }
}

// value following "/g" or "/G" (optionally preceded by "-name").
void WZLauncherController::BuildGameArguments(wchar_t* out)
{
    const std::vector<std::wstring>& args = m_settings.GetArguments();
    for (size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == L"/g" || args[i] == L"/G")
        {
            size_t v = i + 1;
            if (v >= args.size())
                return;
            if (args[v] == L"-name")
            {
                ++v;
                if (v >= args.size())
                    return;
            }
            WZStrCopy(out, 4096, args[v].c_str());
            return;
        }
    }
}

bool WZLauncherController::Initialize()
{
    WZResult init = m_settings.Load();
    if (init.code || init.subCode)
    {
        WZLog(L"Launcher Init Fail");
        return false;
    }

    wchar_t requested[260] = {};
    GetRequestedGameName(requested);
    WZStrCopy(m_gameName, 260, requested);

    GameEntry* entry = m_settings.FindGame(m_gameName);
    if (!entry)
    {
        memset(m_gameName, 0, sizeof(m_gameName));
        entry = m_settings.FindGame(m_gameName);
        if (!entry)
            return false;
    }
    WZStrCopy(m_gameName, 260, entry->ui->Guid);
    if (!m_gameName[0])
        return false;

    m_hEvent = CreateEventW(nullptr, FALSE, FALSE, L"WZLauncherController");
    return true;
}

void StartCrashReport(const wchar_t* host, unsigned short port, const wchar_t* executablePath);

void WZLauncherController::Start()
{
    GameEntry* entry = m_settings.FindGame(m_gameName);
    // Lunarite: send the last session's log and minidump when it crashed (CrashReport.cpp).
    if (entry && entry->update && entry->update->Server && entry->update->Path)
        StartCrashReport(entry->update->Server->ServerURL, (unsigned short)entry->update->Server->Port,
                         entry->update->Path->ExecutablePath);
    std::wstring webUrl = WebUrlFor(entry);
    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETWEBURL, (WPARAM)(entry ? webUrl.c_str() : nullptr), 0);

    if (!GetCommonInfo()->UsePurpleLauncher)
    {
        StartLauncherStateThread();
    }
    else
    {
        m_stage = STAGE_GAME;
        StartGameLauncher();
    }
}

UpdateInfo* WZLauncherController::GetSelectedGameInfo()
{
    GameEntry* entry = m_settings.FindGame(m_gameName);
    return entry ? entry->update : nullptr;
}

GameEntry* WZLauncherController::GetSelectedEntry()
{
    return m_settings.FindGame(m_gameName);
}

void WZLauncherController::StartLauncherStateThread()
{
    ReplaceThread(m_pWorkThread, &WZLauncherController::LauncherStateThreadProc, this);
}

void WZLauncherController::StartGameUpdate()
{
    ReplaceThread(m_pWorkThread, &WZLauncherController::UpdateGameThreadProc, this);
}

void WZLauncherController::StartCompletionMonitor()
{
    ReplaceThread(m_pMonitorThread, &WZLauncherController::CompletionMonitorProc, this);
}

void __cdecl WZLauncherController::LauncherStateThreadProc(void* self)
{
    static_cast<WZLauncherController*>(self)->LauncherStateThread();
}

void __cdecl WZLauncherController::UpdateGameThreadProc(void* self)
{
    static_cast<WZLauncherController*>(self)->UpdateGameThread();
}

void __cdecl WZLauncherController::CompletionMonitorProc(void* self)
{
    static_cast<WZLauncherController*>(self)->CompletionMonitorThread();
}

void WZLauncherController::LauncherStateThread()
{
    m_bUserBreak = FALSE;
    StartCompletionMonitor();

    DWORD changerFailed = 0;
    GameEntry* entry = m_settings.FindGame(m_gameName);
    PathInfo* gamePath = (entry && entry->update) ? entry->update->Path : nullptr;
    if (gamePath)
        WZRegReadDword(gamePath->RegistryRootKey, gamePath->RegistrySubKey, L"WZLUpdateState", &changerFailed);

    if (changerFailed)
    {
        WZLog(L"Self Update Changer Fail - Start Game Update");
        SetEvent(m_hEvent);
        return;
    }

    UpdateInfo* launcher = m_settings.GetLauncherUpdateInfo();
    WZLog(L"Get Launcher State");
    m_result = m_updaterManager.CheckVersion(launcher, this);

    if (m_result.code < 2)
    {
        int state = launcher->Version ? launcher->Version->State : 0;
        m_updateState = state;
        if (!launcher->Version || state)
        {
            WZLog(L"The Latest Launcher Version");
            SetEvent(m_hEvent);
        }
        else
        {
            WZLog(L"Start Update Launcher");
            m_stage = STAGE_UPDATE_LAUNCHER;
            m_result = m_updaterManager.Update(launcher, this);
        }
    }
    else
    {
        CStringInfo* strings = m_settings.GetStringInfo();
        m_msg.SetFileStatic(L"");
        bool net = IsNetError(m_result.code);
        if (net)
            WZLog(L"Get Launcher State Fail : Net Error");
        else
            WZLog(L"Get Launcher State Fail : Version Error");
        m_msg.SetTotalStatic(strings->GetString(net ? DESC_ERROR_NETWORK : DESC_UNKNOWN_VERSION));
        SetEvent(m_hEvent);
    }
}

void WZLauncherController::UpdateGameThread()
{
    m_bUserBreak = FALSE;
    StartCompletionMonitor();
    WZLog(L"Start Update Selected Game");
    m_updaterManager.Update(GetSelectedGameInfo(), this);
}

void WZLauncherController::CompletionMonitorThread()
{
    CStringInfo* strings = m_settings.GetStringInfo();
    WaitForSingleObject(m_hEvent, INFINITE);

    switch (m_stage)
    {
    case STAGE_EXIT:
        WZLog(L"Exit Launcher");
        ExitLauncher();
        return;

    case STAGE_END_LAUNCHER_UPDATE:
        WZLog(L"CompleteEvent : End Launcher Update");
        m_stage = STAGE_GAME;
        StartGameLauncher();
        return;

    case STAGE_UPDATE_LAUNCHER:
        if (m_updateState == UPDATE_STATE_LATEST)
        {
            WZLog(L"CompleteEvent : Update Launcher Success");
            if (SelfUpdate())
            {
                WZLog(L"Exit Launcher");
                ExitLauncher();
                return;
            }
        }
        WZLog(L"CompleteEvent : Update Launcher Fail, End Launcher Update");
        m_stage = STAGE_GAME;
        StartGameLauncher();
        return;

    case STAGE_GAME:
        if (m_updateState == UPDATE_STATE_NEED_UPDATE)
        {
            WZLog(L"CompleteEvent : Need Update Game");
            m_msg.SetGameListStateText(m_gameName, strings->GetString(DESC_UPDATE_REQUIRED));
            if (GetCommonInfo()->UsePurpleLauncher)
                m_msg.PopErrorMessage(m_result.message, m_result.code, 1);
            ApplyUpdateStateToUI();
        }
        else if (m_updateState == UPDATE_STATE_LATEST)
        {
            WZLog(L"CompleteEvent : Update Game Success");
            m_updaterManager.UpdateVersion(GetSelectedGameInfo(), this);
            FinishUpdate();
            ApplyUpdateStateToUI();
        }
        else if (m_updateState == UPDATE_STATE_NET_ERROR)
        {
            WZLog(L"CompleteEvent : Update Game Fail : Net Error");
            m_msg.SetGameListStateText(m_gameName, strings->GetString(DESC_ERROR_NETWORK));
            if (GetCommonInfo()->UsePurpleLauncher)
                m_msg.PopErrorMessage(m_result.message, m_result.code, 1);
            ApplyUpdateStateToUI();
        }
        else
        {
            ApplyUpdateStateToUI();
        }
        return;
    }
}

bool WZLauncherController::SelfUpdate()
{
    UpdateInfo* launcher = m_settings.GetLauncherUpdateInfo();
    wchar_t installedChanger[260] = {};     // <launcher dir>\WZLauncherChanger.exe
    wchar_t downloadedChanger[260] = {};    // <download temp>\WZLauncherChanger.exe

    WZStrCopy(installedChanger, 260, launcher->Path->RootDir);
    WZStrCopy(downloadedChanger, 260, launcher->Path->DownloadTempDir);
    AddBackslash(installedChanger);
    AddBackslash(downloadedChanger);
    AppendString(installedChanger, L"WZLauncherChanger.exe");
    AppendString(downloadedChanger, L"WZLauncherChanger.exe");

    if (GetFileAttributesW(downloadedChanger) != INVALID_FILE_ATTRIBUTES)
    {
        DeleteFileW(installedChanger);
        if (!MoveFileW(downloadedChanger, installedChanger))
            return false;
    }

    wchar_t tempDir[260] = {};
    WZStrCopy(tempDir, 260, launcher->Path->DownloadTempDir);
    TrimPathQuotes(tempDir);

    wchar_t versionFile[260] = {};
    WZStrCopy(versionFile, 260, launcher->Version->LocalVersionFile);
    StripToFileName(versionFile);

    GameEntry* entry = m_settings.FindGame(m_gameName);
    PathInfo* gamePath = (entry && entry->update) ? entry->update->Path : nullptr;

    wchar_t args[4096] = {};
    WZStrPrintf(args, 4096, L"/P \"%s\" /O \"%s\" /S \"%s\" /V \"%s\" /K \"%s\" /R \"%s\"",
                m_settings.GetCommandLineText(),
                launcher->Path->ExecutablePath,
                tempDir,
                versionFile,
                gamePath ? gamePath->RegistryRootKey : L"",
                gamePath ? gamePath->RegistrySubKey : L"");

    SHELLEXECUTEINFOW exec = {};
    exec.cbSize = sizeof(SHELLEXECUTEINFOW);
    exec.fMask = SEE_MASK_NOCLOSEPROCESS;
    exec.lpVerb = L"open";
    exec.lpFile = installedChanger;
    exec.lpParameters = args;
    exec.lpDirectory = launcher->Path->RootDir;
    exec.nShow = SW_SHOW;

    WZLog(L"Launcher Changer Param : %s", args);
    if (!ShellExecuteExW(&exec))
    {
        WZLog(L"Start Fail Launcher Changer");
        GetLastError();
        return false;
    }

    WZLog(L"Start Launcher Changer");
    if (gamePath)
        WZRegWriteDword(gamePath->RegistryRootKey, gamePath->RegistrySubKey, L"WZLUpdateState", 1);
    return true;
}

void WZLauncherController::QueryGameStates()
{
    CStringInfo* strings = m_settings.GetStringInfo();
    std::vector<GameEntry*> games = m_settings.GetGames();

    for (GameEntry* entry : games)
    {
        if (!entry || !entry->ui || !entry->update)
            continue;
        UpdateInfo* game = entry->update;

        wchar_t path[260] = {};
        WZStrCopy(path, 260, game->Path->RootDir);
        WZLog(L"Game Path : %s", path);

        if (!path[0] || GetFileAttributesW(game->Path->RootDir) == INVALID_FILE_ATTRIBUTES)
        {
            WZLog(L"Invalid Game Path : Not Install");
            game->Version->State = UPDATE_STATE_NEED_INSTALL;
        }
        else
        {
            WZLog(L"Get Game State");
            m_result = m_updaterManager.CheckVersion(game, this);
        }

        if (m_result.code)
        {
            bool net = IsNetError(m_result.code);
            if (net)
                WZLog(L"Get Game State Fail : Net Error");
            else
                WZLog(L"Get Game State Fail : Version Error");
            m_msg.SetTotalStatic(strings->GetString(net ? DESC_ERROR_NETWORK : DESC_UNKNOWN_VERSION));
        }
        else if (game->Version->State)
        {
            WZLog(L"Game State : COMPLETED_UPDATE");
            m_msg.SetGameListStateText(entry->ui->Guid, L"");
        }
        else
        {
            WZLog(L"Game State : NEED_UPDATE");
            m_msg.SetGameListStateText(entry->ui->Guid, strings->GetString(DESC_UPDATE_REQUIRED));
        }
    }

    UpdateInfo* selected = GetSelectedGameInfo();
    m_updateState = selected ? selected->Version->State : 0;
    ApplyUpdateStateToUI();

    wchar_t dbg[260];
    WZStrPrintf(dbg, 260, L"Update State is %d", m_updateState);
    OutputDebugStringW(dbg);
}

void WZLauncherController::ApplyUpdateStateToUI()
{
    CStringInfo* strings = m_settings.GetStringInfo();

    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETUPDATESTATE, (WPARAM)m_updateState, 0);
    m_msg.SetFileStatic(L"");

    switch (m_updateState)
    {
    case UPDATE_STATE_NEED_UPDATE:
        m_msg.SetTotalStatic(strings->GetString(DESC_UPDATE_REQUIRED));
        break;
    case UPDATE_STATE_LATEST:
        m_msg.SetTotalStatic(strings->GetString(DESC_UPDATE_COMPLETED));
        break;
    case UPDATE_STATE_UPDATING:
        break;
    case UPDATE_STATE_NEED_INSTALL:
        m_msg.SetTotalStatic(strings->GetString(DESC_INSTALL_REQUIRED));
        break;
    case UPDATE_STATE_NET_ERROR:
        m_msg.SetTotalStatic(strings->GetString(DESC_ERROR_NETWORK));
        break;
    default:
        m_msg.SetTotalStatic(strings->GetString(DESC_UPDATE_COMPLETED));
        break;
    }
}

bool WZLauncherController::SelectGame(const wchar_t* gameName)
{
    CStringInfo* strings = m_settings.GetStringInfo();

    if (m_updateState == UPDATE_STATE_UPDATING)
    {
        m_msg.PopErrorMessage(
            L"\uC5C5\uB370\uC774\uD2B8 \uC911\uC785\uB2C8\uB2E4.\n"
            L"\uB2E4\uB978 \uAC8C\uC784\uC744 \uC120\uD0DD \uD560 \uACBD\uC6B0 \uBA3C\uC800 \uC5C5\uB370\uC774\uD2B8\uB97C \uC911\uC9C0\uD574\uC57C \uD569\uB2C8\uB2E4.",
            2, 0);
        return false;
    }

    WZStrCopy(m_gameName, 260, gameName);

    GameEntry* entry = m_settings.FindGame(m_gameName);
    UpdateInfo* game = entry ? entry->update : nullptr;

    wchar_t path[260] = {};
    if (game)
        WZStrCopy(path, 260, game->Path->RootDir);

    if (!path[0] || GetFileAttributesW(game->Path->RootDir) == INVALID_FILE_ATTRIBUTES)
    {
        if (entry)
            game->Version->State = UPDATE_STATE_NEED_INSTALL;
    }
    else
    {
        m_result = m_updaterManager.CheckVersion(game, this);
        if (m_result.code >= 2)
        {
            m_msg.SetFileStatic(L"");
            bool net = IsNetError(m_result.code);
            m_msg.SetTotalStatic(strings->GetString(net ? DESC_ERROR_NETWORK : DESC_UNKNOWN_VERSION));
        }
    }

    UpdateInfo* selected = GetSelectedGameInfo();
    m_updateState = selected ? selected->Version->State : 0;

    GameEntry* current = m_settings.FindGame(m_gameName);
    std::wstring webUrl = WebUrlFor(current);
    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETWEBURL, (WPARAM)(current ? webUrl.c_str() : nullptr), 0);

    m_msg.SetUpdateProgress(0);
    m_msg.SetFileProgress(0);
    ApplyUpdateStateToUI();
    return true;
}

void WZLauncherController::UserClickedExit()
{
    WZLog(L"User Clicked Exit");
    bool updating = (m_updateState == UPDATE_STATE_UPDATING);
    m_stage = STAGE_EXIT;
    if (updating)
    {
        UserBreak();
        return;
    }
    WZLog(L"Exit Launcher");
    ExitLauncher();
}

void WZLauncherController::ExitLauncher()
{
    ShowWindow(GetLauncherMainWnd(), SW_HIDE);

    UpdateInfo* game = GetSelectedGameInfo();
    TestNetworkInfo* testNet = game ? game->TestNetwork : nullptr;
    if (testNet)
    {
        for (int i = 0; testNet->Testing; ++i)
        {
            if (i >= 10)
                break;
            Sleep(500);
        }
    }
    m_msg.ExitLauncher();
}

void WZLauncherController::UserClickedConfig()
{
    WZLog(L"User Clicked Config");

    GameEntry* entry = m_settings.FindGame(m_gameName);
    if (!entry || !entry->ui->GameOptionDll[0])
        return;

    HMODULE module = LoadLibraryW(entry->ui->GameOptionDll);
    if (!module)
        return;

    typedef void (__cdecl *SetGameOptionFn)(const wchar_t*);
    SetGameOptionFn setGameOption = (SetGameOptionFn)GetProcAddress(module, "SetGameOption");
    if (setGameOption)
        setGameOption(entry->update->Path->RootDir);
    FreeLibrary(module);
}

void WZLauncherController::StartGameLauncher()
{
    QueryGameStates();
    m_msg.StartLauncher((WPARAM)m_gameName);
    WZLog(L"Start Game Launcher");

    switch (m_updateState)
    {
    case UPDATE_STATE_NEED_UPDATE:
        WZLog(L"Need Game Update");
        StartGameUpdate();
        break;

    case UPDATE_STATE_LATEST:
        WZLog(L"The Latest Game Version");
        if (GetCommonInfo()->UsePurpleLauncher)
        {
            ExitLauncher();
        }
        else
        {
            m_updaterManager.StartNetTest(GetSelectedGameInfo(), this);
            FinishUpdate();
        }
        break;

    case UPDATE_STATE_NEED_INSTALL:
        WZLog(L"Need Game Install");
        break;

    case UPDATE_STATE_NET_ERROR:
        WZLog(L"Net Error");
        if (GetCommonInfo()->UsePurpleLauncher)
            m_msg.PopErrorMessage(m_result.message, m_result.code, 1);
        break;

    case UPDATE_STATE_UNKNOWN:
    {
        WZLog(L"Unknown State");
        UpdateInfo* game = GetSelectedGameInfo();
        if (game && game->Patcher->UseCheckFiles)
            StartGameUpdate();
        break;
    }
    }
}

void WZLauncherController::PopUpInstallPage()
{
    WZLog(L"Pop Up Game Install Page");
    GameEntry* entry = m_settings.FindGame(m_gameName);
    const wchar_t* url = entry ? entry->ui->InstallURL : L"";
    if (ShellExecuteW(nullptr, L"open", L"iexplore.exe", url, nullptr, SW_SHOWNORMAL))
    {
        WZLog(L"Exit Launcher");
        ExitLauncher();
    }
}

void WZLauncherController::UserBreak()
{
    WZLog(L"User Break");
    m_bUserBreak = TRUE;
    m_updaterManager.Break();
    if (WZUpdater* updater = m_updaterManager.Updater())
        m_result = updater->GetResult();
}

void WZLauncherController::FinishUpdate()
{
    m_msg.SetUpdateProgress(100);
    m_msg.SetFileProgress(100);
    if (m_bAutoExecute && GetCommonInfo()->hExecuteButton)
    {
        WZLog(L"Auto Execute Game");
        ExecuteGame();
    }
}

DWORD WZLauncherController::ExecuteGame()
{
    wchar_t params[4096] = {};
    BuildGameArguments(params);

    GameEntry* entry = m_settings.FindGame(m_gameName);
    PathInfo* path = entry->update->Path;
    RemoveConfiguredFiles(entry);
    ApplyDxvk(path->ExecutablePath);

    SHELLEXECUTEINFOW exec = {};
    exec.cbSize = sizeof(SHELLEXECUTEINFOW);
    exec.fMask = SEE_MASK_NOCLOSEPROCESS;
    exec.lpVerb = L"open";
    exec.lpFile = path->ExecutablePath;
    exec.lpParameters = params;
    exec.lpDirectory = path->RootDir;
    exec.nShow = SW_SHOW;

    // Lunarite: start the game DPI-aware, as the Properties > Compatibility "Override high DPI
    // scaling behavior: Application" box does, so resolutions are real pixels on scaled desktops.
    // The compatibility layer is inherited by the game process only; no setting is written.
    wchar_t layers[512] = {};
    GetEnvironmentVariableW(L"__COMPAT_LAYER", layers, 512);
    if (!wcsstr(layers, L"HighDpiAware"))
    {
        wchar_t updated[512] = {};
        WZStrPrintf(updated, 512, layers[0] ? L"%s HighDpiAware" : L"%sHighDpiAware", layers);
        SetEnvironmentVariableW(L"__COMPAT_LAYER", updated);
    }

    if (ShellExecuteExW(&exec))
    {
        WZLog(L"Execute Game Success");
        WZLog(L"Exit Launcher");
        ExitLauncher();
        return 0;
    }
    WZLog(L"Execute Game Fail");
    return GetLastError();
}

void WZLauncherController::SetAutoExecute(BOOL enable)
{
    m_bAutoExecute = enable;

    GameEntry* entry = m_settings.FindGame(m_gameName);
    PathInfo* path = (entry && entry->update) ? entry->update->Path : nullptr;
    if (path)
    {
        DWORD data = (enable != 0);
        HKEY key;
        if (RegOpenKeyExW(WZRegRootFromName(path->RegistryRootKey), path->RegistrySubKey, 0, KEY_WRITE, &key) == ERROR_SUCCESS)
        {
            RegSetValueExW(key, L"AutoExecuteGameOption", 0, REG_DWORD, (const BYTE*)&data, sizeof(data));
            RegCloseKey(key);
        }
    }

    if (m_bAutoExecute)
        WZLog(L"Set Auto Execute : TRUE");
    else
        WZLog(L"Set Auto Execute : FALSE");
}

// The DXVK choice is per user (HKCU\Software\Lunarite): the install key is under HKLM, which a launcher without
// admin rights cannot write, so a choice made once from an elevated launcher (Setup's "Launch now") could never be
// turned off again. The old HKLM value is ignored, so everyone starts from "off".
static const wchar_t* kLunariteKey = L"Software\\Lunarite";

void WZLauncherController::SetUseDxvk(BOOL enable)
{
    DWORD data = (enable != 0);
    HKEY key;
    LONG result = RegCreateKeyExW(HKEY_CURRENT_USER, kLunariteKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
    if (result == ERROR_SUCCESS)
    {
        result = RegSetValueExW(key, L"UseDxvk", 0, REG_DWORD, (const BYTE*)&data, sizeof(data));
        RegCloseKey(key);
    }
    WZLog(result != ERROR_SUCCESS ? L"Set DXVK : save failed" : enable ? L"Set DXVK : TRUE" : L"Set DXVK : FALSE");
}

BOOL WZLauncherController::GetUseDxvk()
{
    DWORD data = 0, size = sizeof(data), type = 0;
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kLunariteKey, 0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        if (RegQueryValueExW(key, L"UseDxvk", nullptr, &type, (BYTE*)&data, &size) != ERROR_SUCCESS || type != REG_DWORD)
            data = 0;
        RegCloseKey(key);
    }
    return data != 0;
}

bool WZLauncherController::VulkanAvailable()
{
    wchar_t found[MAX_PATH] = {};
    return SearchPathW(nullptr, L"vulkan-1.dll", nullptr, MAX_PATH, found, nullptr) != 0;
}

void WZLauncherController::ApplyDxvk(const wchar_t* executablePath)
{
    wchar_t dir[MAX_PATH] = {}, source[MAX_PATH] = {}, target[MAX_PATH] = {};
    WZStrCopy(dir, MAX_PATH, executablePath);
    TrimPathQuotes(dir);
    wchar_t* slash = wcsrchr(dir, L'\\');
    if (!slash)
        return;
    slash[1] = L'\0';
    swprintf_s(source, L"%sdxvk\\d3d9.dll", dir);
    swprintf_s(target, L"%sd3d9.dll", dir);
    WIN32_FILE_ATTRIBUTE_DATA src = {}, dst = {};
    bool haveSource = GetFileAttributesExW(source, GetFileExInfoStandard, &src) != FALSE;
    bool haveTarget = GetFileAttributesExW(target, GetFileExInfoStandard, &dst) != FALSE;
    if (GetUseDxvk() && haveSource && VulkanAvailable())
    {
        if (!haveTarget || src.nFileSizeLow != dst.nFileSizeLow
            || CompareFileTime(&src.ftLastWriteTime, &dst.ftLastWriteTime) != 0)
            WZLog(CopyFileW(source, target, FALSE) ? L"DXVK : on" : L"DXVK : copy failed");
    }
    else if (haveTarget && haveSource && src.nFileSizeLow == dst.nFileSizeLow)
    {
        // Only our copy is removed; a d3d9.dll the player put there themselves stays.
        WZLog(DeleteFileW(target) ? L"DXVK : off" : L"DXVK : remove failed");
    }
}

BOOL WZLauncherController::GetAutoExecute()
{
    DWORD data = 0;
    GameEntry* entry = m_settings.FindGame(m_gameName);
    PathInfo* path = (entry && entry->update) ? entry->update->Path : nullptr;
    if (path)
        WZRegReadDword(path->RegistryRootKey, path->RegistrySubKey, L"AutoExecuteGameOption", &data);

    m_bAutoExecute = data;
    if (data)
        WZLog(L"Get Auto Execute : TRUE");
    else
        WZLog(L"Get Auto Execute : FALSE");
    return m_bAutoExecute;
}

// (download) / (patch)
void WZLauncherController::UpdateFileProgress(const wchar_t* text, const wchar_t* fileName,
                                              ULONGLONG bytes, bool patching)
{
    if (m_totalSize != 0 && m_curFileSize != 0)
    {
        int divisor = 1;
        UpdateInfo* launcher = m_settings.GetLauncherUpdateInfo();
        UpdateInfo* game = GetSelectedGameInfo();
        if (patching)
        {
            UpdateInfo* target = (m_stage == STAGE_UPDATE_LAUNCHER) ? launcher : game;
            if (target->Patcher->HasPatcher)
                divisor = 2;
        }
        else if (m_stage == STAGE_UPDATE_LAUNCHER)
        {
            if (launcher->Patcher->HasPatcher)
                divisor = 2;
        }
        else if (game->Patcher->HasPatcher)
        {
            divisor = 2 - (game->Patcher->CheckFilesDone != 0);
        }

        int filePercent = (int)((100 * bytes) / m_curFileSize);
        m_msg.SetFileProgress(filePercent);

        wchar_t shortName[260] = {};
        WZStrCopy(shortName, 260, fileName);
        StripToFileName(shortName);

        wchar_t buf[260] = {};
        WZStrPrintf(buf, 260, L"%s (%d%%)", shortName, filePercent);
        m_msg.SetFileStatic(buf);
        if (!patching)
            OutputDebugStringW(buf);

        ULONGLONG done = (!patching && m_downloadTotal != ~0ull) ? m_downloadTotal : bytes + m_doneSize;
        int totalPercent = (int)((100 * done) / m_totalSize) / divisor;
        m_msg.SetUpdateProgress(totalPercent);

        memset(buf, 0, sizeof(buf));
        WZStrPrintf(buf, 260, L"%s (%d%%)", text, totalPercent);
        SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC, (WPARAM)buf, 0);
    }
    else
    {
        m_msg.SetFileProgress(0);
        m_msg.SetUpdateProgress(patching ? 50 : 0);
        SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC, (WPARAM)text, 0);
        m_msg.SetFileStatic(fileName);
    }
}

void WZLauncherController::OnUpdateBegin()
{
    m_updateState = UPDATE_STATE_UPDATING;
    m_msg.SetUpdateProgress(0);
    m_msg.SetFileProgress(0);
    ApplyUpdateStateToUI();
    WZLog(L"Started Update");
}

void WZLauncherController::OnUpdateEnd(WZResult result)
{
    if (result.code || result.subCode)
    {
        m_updateState = (result.code >> 27) & 4;
    }
    else
    {
        m_updateState = UPDATE_STATE_LATEST;
        m_msg.SetUpdateProgress(100);
        m_msg.SetFileProgress(100);
    }
    ApplyUpdateStateToUI();
    WZLog(L"Completed Update - ErrorCode : %X, ErrorString : %s", result.code, result.message);

    if (m_hEvent != INVALID_HANDLE_VALUE)
        SetEvent(m_hEvent);
    if (GetCommonInfo()->UsePurpleLauncher)
        ExitLauncher();
}

void WZLauncherController::OnCheckVersionBegin()
{
    WZLog(L"Started Check Version");
}

void WZLauncherController::OnCheckVersionEnd(WZResult result, int fileCount, unsigned __int64 downloadSize)
{
    CStringInfo* strings = m_settings.GetStringInfo();

    if (!result.code && !result.subCode)
    {
        m_totalSize = downloadSize;
        m_doneSize = 0;
        m_curFileSize = 0;
        m_totalFiles = fileCount;
        m_curFileIndex = 0;
        m_curFile = nullptr;
        m_curFileBytes = 0;
        m_downloadTotal = ~0ull;
        m_msg.SetFileCount(0, m_totalFiles);
        m_msg.SetFileStatic(L"");
        m_msg.SetTotalStatic(L"");
    }
    else
    {
        m_msg.SetFileStatic(L"");
        if (!IsNetError(m_result.code))
            m_msg.SetTotalStatic(strings->GetString(DESC_UNKNOWN_VERSION));
        else
            m_msg.SetTotalStatic(strings->GetString(DESC_ERROR_NETWORK));
    }

    WZLog(L"Completed CheckVersion - ErrorCode : %X, ErrorString : %s", result.code, result.message);
    WZLog(L"File Count : %d, Download Size : %d", fileCount, (DWORD)downloadSize);
}

void WZLauncherController::OnDownloadStart(const FileInfo* file, unsigned __int64 size)
{
    const wchar_t* fileName = file->name;
    CStringInfo* strings = m_settings.GetStringInfo();

    WZStrCopy(m_curFileName, 260, fileName);
    m_curFileSize = size;
    m_curFile = file;
    m_curFileBytes = 0;

    wchar_t shortName[260] = {};
    WZStrCopy(shortName, 260, fileName);
    StripToFileName(shortName);
    UpdateFileProgress(strings->GetString(DESC_DOWNLOADING_FILES), shortName, 0, false);

    WZLog(L"Started Download File - FileName : %s, FileLength : %d bytes", fileName, (DWORD)size);
}

// Several files download at once: the file line follows the newest one, the bar the total.
void WZLauncherController::OnDownloadProgress(const FileInfo* file, unsigned __int64 bytes)
{
    CStringInfo* strings = m_settings.GetStringInfo();

    if (file == m_curFile)
        m_curFileBytes = bytes;
    UpdateFileProgress(strings->GetString(DESC_DOWNLOADING_FILES), m_curFileName, m_curFileBytes, false);
}

void WZLauncherController::OnDownloadTotal(unsigned __int64 doneBytes)
{
    m_downloadTotal = doneBytes;
}

void WZLauncherController::OnDownloadEnd(WZResult result)
{
    CStringInfo* strings = m_settings.GetStringInfo();

    m_doneSize = m_downloadTotal != ~0ull ? m_downloadTotal : m_doneSize + m_curFileSize;
    ++m_curFileIndex;
    m_msg.SetFileCount(m_curFileIndex, m_totalFiles);
    if (result.code || result.subCode)
        SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC,
                     (WPARAM)strings->GetString(DESC_ERROR_DOWNLOADING_FILES), 0);

    WZLog(L"%d. Completed Download File - ErrorCode : %X, ErrorString : %s",
          m_curFileIndex, result.code, result.message);
}

void WZLauncherController::OnPatchBegin()
{
    m_curFileIndex = 0;
    m_msg.SetFileCount(0, m_totalFiles);
}

void WZLauncherController::OnPatchFileStart(const FileInfo* file, unsigned __int64 size)
{
    const wchar_t* fileName = file->name;
    CStringInfo* strings = m_settings.GetStringInfo();

    SendMessageW(GetLauncherMainWnd(), WM_WZ_ENABLEBUTTONS, 0, 0);

    WZStrCopy(m_curFileName, 260, fileName);
    m_curFileSize = size;

    wchar_t shortName[260] = {};
    WZStrCopy(shortName, 260, fileName);
    StripToFileName(shortName);
    UpdateFileProgress(strings->GetString(DESC_PATCHING_FILES), shortName, 0, true);

    WZLog(L"%d. Started Patch File - FileName : %s, FileLength : %d bytes",
          m_curFileIndex + 1, fileName, (DWORD)size);
}

void WZLauncherController::OnPatchFileProgress(const FileInfo* file, unsigned __int64 bytes)
{
    const wchar_t* fileName = file->name;
    CStringInfo* strings = m_settings.GetStringInfo();

    wchar_t shortName[260] = {};
    WZStrCopy(shortName, 260, fileName);
    StripToFileName(shortName);
    UpdateFileProgress(strings->GetString(DESC_PATCHING_FILES), shortName, bytes, true);
}

void WZLauncherController::OnPatchFileEnd(WZResult result)
{
    CStringInfo* strings = m_settings.GetStringInfo();

    m_doneSize += m_curFileSize;
    ++m_curFileIndex;
    m_msg.SetFileCount(m_curFileIndex, m_totalFiles);
    if (result.code || result.subCode)
        SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC,
                     (WPARAM)strings->GetString(DESC_ERROR_PATCHING_FILES), 0);

    WZLog(L"%d. Completed Patch File - ErrorCode : %X, ErrorString : %s",
          m_curFileIndex, result.code, result.message);
    SendMessageW(GetLauncherMainWnd(), WM_WZ_ENABLEBUTTONS, 1, 0);
}

void WZLauncherController::OnCheckFilesBegin()
{
    CStringInfo* strings = m_settings.GetStringInfo();

    m_msg.SetUpdateProgress(0);
    m_msg.SetFileProgress(0);
    m_msg.SetFileStatic(L"");
    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC,
                 (WPARAM)strings->GetString(DESC_CHECKING_FILES), 0);
    WZLog(L"Started Check Full Files");
}

// /
void WZLauncherController::OnCheckFilesProgress(int total, int current)
{
    CStringInfo* strings = m_settings.GetStringInfo();

    wchar_t buf[260] = {};
    WZStrPrintf(buf, 260, L"%s (%d/%d)", strings->GetString(DESC_CHECKING_FILES), current, total);
    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC, (WPARAM)buf, 0);
}

void WZLauncherController::OnCheckFilesEnd(WZResult result, int fileCount, unsigned __int64 downloadSize)
{
    CStringInfo* strings = m_settings.GetStringInfo();

    if (result.code || result.subCode)
    {
        m_msg.SetFileStatic(L"");
        if (IsNetError(m_result.code))
            SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC,
                         (WPARAM)strings->GetString(DESC_ERROR_NETWORK), 0);
        else
            SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC,
                         (WPARAM)strings->GetString(DESC_ERROR_CHECKING_FILES), 0);
    }
    else
    {
        m_totalSize = downloadSize;
        m_doneSize = 0;
        m_curFileSize = 0;
        m_totalFiles = fileCount;
        m_curFileIndex = 0;
        m_curFile = nullptr;
        m_curFileBytes = 0;
        m_downloadTotal = ~0ull;
        m_msg.SetFileCount(0, m_totalFiles);
        m_msg.SetFileStatic(L"");
        SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC, (WPARAM)L"", 0);
    }

    UpdateInfo* game = GetSelectedGameInfo();
    if (game)
        game->Patcher->CheckFilesDone = 1;

    WZLog(L"Completed Check  Full Files - ErrorCode : %X, ErrorString : %s", result.code, result.message);
    WZLog(L"File Count : %d, Download Size : %I64u", fileCount, downloadSize);
}
