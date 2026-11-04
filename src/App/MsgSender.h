// MsgSender.h - posts WM_* messages to the launcher dialog and mirrors progress to the
// "Purple" outbound launcher window via WM_COPYDATA.
#pragma once

#include "../Common/WZCommon.h"

// Private window messages handled by CWZLauncherDlg (message map).
enum WZLauncherMessage : UINT
{
    WM_WZ_POPERROR          = 0x9A0A,   // wParam = text, lParam = disable-update-button flag
    WM_WZ_STARTLAUNCHER     = 0x9B58,   // wParam = game name
    WM_WZ_EXITLAUNCHER      = 0x9B59,
    WM_WZ_SETWEBURL         = 0x9B5B,   // wParam = url
    WM_WZ_SETUPDATESTATE    = 0x9B5C,   // wParam = update state
    WM_WZ_ENABLEBUTTONS     = 0x9B5D,   // wParam = enable
    WM_WZ_SETGAMELISTSTATE  = 0x9B5E,   // wParam = game name, lParam = text
    WM_WZ_SETGAMELISTENABLE = 0x9B5F,   // wParam = value stored in the game list
    WM_WZ_SETUPDATEPROGRESS = 0x9B60,   // wParam = percent (total bar)
    WM_WZ_SETFILEPROGRESS   = 0x9B61,   // wParam = percent (file bar)
    WM_WZ_SETTOTALSTATIC    = 0x9B62,   // wParam = text
    WM_WZ_SETFILESTATIC     = 0x9B63,   // wParam = text
    WM_WZ_SELECTGAME        = 0x9BB4,   // wParam = game name
};

// COPYDATASTRUCT::dwData values sent to the Purple launcher window.
enum PurpleMessageType : int
{
    PURPLE_FILE_PROGRESS    = 0x12,
    PURPLE_UNKNOWN_1B       = 0x1B,
    PURPLE_UNKNOWN_1F       = 0x1F,
    PURPLE_FILE_STATIC      = 0x24,
    PURPLE_UPDATE_PROGRESS  = 0x2D,
    PURPLE_FILE_COUNT       = 0x36,
    PURPLE_ENDING           = 0x3F,
};

// Handle of the launcher dialog.
HWND GetLauncherMainWnd();

// Small object embedded in WZLauncherController.
class MsgSender
{
public:
    MsgSender();
    virtual ~MsgSender();

    void PopErrorMessage(const wchar_t* text, DWORD code, LPARAM lParam);
    void StartLauncher(WPARAM gameName);
    void ExitLauncher();
    void SetGameListStateText(const wchar_t* gameName, const wchar_t* text);
    void SetUpdateProgress(int percent);
    void SetFileProgress(int percent);
    void SetTotalStatic(const wchar_t* text);
    void SetFileStatic(const wchar_t* text);
    void ClearOutboundParam();
    void SendPurple(int type, const wchar_t* text, int param);
    // Inlined in the controller callbacks: OutputDebugString + PURPLE_FILE_COUNT.
    void SetFileCount(unsigned current, unsigned total);

    BOOL m_bOutbound;       // Purple outbound mode
    HWND m_hPurpleWnd;      // target window for WM_COPYDATA
};
