// WZLauncherController.h - launcher state machine; implements IUpdaterStateEvent.
#pragma once

#include "../Common/WZCommon.h"
#include "../Common/WZThread.h"
#include "../Settings/SettingManager.h"
#include "../Updater/UpdaterEvents.h"
#include "../Updater/UpdaterManager.h"
#include "MsgSender.h"

// VersionInfo::State values (mirrors the selected game's state).
enum UpdateState
{
    UPDATE_STATE_NEED_UPDATE  = 0,
    UPDATE_STATE_LATEST       = 1,
    UPDATE_STATE_UPDATING     = 2,
    UPDATE_STATE_NEED_INSTALL = 3,
    UPDATE_STATE_NET_ERROR    = 4,
    UPDATE_STATE_UNKNOWN      = 255,
};

enum LauncherStage
{
    STAGE_END_LAUNCHER_UPDATE = 0,
    STAGE_UPDATE_LAUNCHER     = 1,
    STAGE_GAME                = 2,
    STAGE_EXIT                = 3,
};

// WZResult::code bit set by the HTTP/FTP layer on network failures.
const DWORD WZ_RESULT_NET_ERROR = 0x20000000;

class WZLauncherController : public IUpdaterStateEvent
{
public:
    WZLauncherController();
    virtual ~WZLauncherController();                // /

    bool Initialize();
    void Start();

    // UI commands (dialog button handlers / messages).
    bool SelectGame(const wchar_t* gameName);
    void UserClickedExit();
    void UserClickedConfig();
    void PopUpInstallPage();
    void UserBreak();
    void StartGameUpdate();
    DWORD ExecuteGame();
    void SetAutoExecute(BOOL enable);
    BOOL GetAutoExecute();

    // IUpdaterStateEvent (vtable).
    virtual void OnUpdateBegin();
    virtual void OnUpdateEnd(WZResult result);
    virtual void OnCheckVersionBegin();
    virtual void OnCheckVersionEnd(WZResult result, int fileCount,
                                        unsigned __int64 downloadSize);
    virtual void OnDownloadStart(const FileInfo* file, unsigned __int64 size);
    virtual void OnDownloadProgress(const FileInfo* file, unsigned __int64 bytes);
    virtual void OnDownloadEnd(WZResult result);
    virtual void OnPatchBegin();
    virtual void OnPatchFileStart(const FileInfo* file, unsigned __int64 size);
    virtual void OnPatchFileProgress(const FileInfo* file, unsigned __int64 bytes);
    virtual void OnPatchFileEnd(WZResult result);
    virtual void OnCheckFilesBegin();
    virtual void OnCheckFilesProgress(int total, int current);
    virtual void OnCheckFilesEnd(WZResult result, int fileCount,
                                          unsigned __int64 downloadSize);

    SettingManager& Settings() { return m_settings; }
    CommonInfo*     GetCommonInfo() { return m_settings.GetCommonInfo(); }
    MsgSender&      Msg() { return m_msg; }

private:
    // Threads.
    void StartLauncherStateThread();
    void StartCompletionMonitor();
    static void __cdecl LauncherStateThreadProc(void* self);
    static void __cdecl UpdateGameThreadProc(void* self);
    static void __cdecl CompletionMonitorProc(void* self);
    void LauncherStateThread();
    void UpdateGameThread();
    void CompletionMonitorThread();
    void DestroyThreads();

    bool SelfUpdate();
    void QueryGameStates();
    void ApplyUpdateStateToUI();
    void ExitLauncher();
    void StartGameLauncher();
    void FinishUpdate();
    void BuildGameArguments(wchar_t* out4096);
    void GetRequestedGameName(wchar_t* out260);
    void UpdateFileProgress(const wchar_t* text, const wchar_t* fileName,
                            ULONGLONG bytes, bool patching);        // /

    UpdateInfo* GetSelectedGameInfo();
    GameEntry*  GetSelectedEntry();

    static void ReplaceThread(WZThread*& slot, WZThread::ThreadFunc fn, void* self);
    static void KillThread(WZThread*& slot);

public:
    BOOL        m_bUserBreak;
    WZResult    m_result;               // result of the last updater call
    UpdaterManager m_updaterManager;
    SettingManager m_settings;
    wchar_t     m_logPath[260];         // <exe dir>\WZLauncher.Log
    wchar_t     m_gameName[260];        // selected game (key of SettingManager::FindGame)
    BOOL        m_bAutoExecute;
    HANDLE      m_hEvent;               // signalled when an update stage completes
    WZThread*   m_pMonitorThread;       // runs CompletionMonitorThread
    WZThread*   m_pWorkThread;          // launcher check / game update thread
    int         m_stage;                // LauncherStage
    int         m_updateState;          // UpdateState of the selected game
    wchar_t     m_curFileName[260];
    ULONGLONG   m_totalSize;
    ULONGLONG   m_doneSize;
    ULONGLONG   m_curFileSize;
    DWORD       m_totalFiles;
    DWORD       m_curFileIndex;
    MsgSender   m_msg;
};
