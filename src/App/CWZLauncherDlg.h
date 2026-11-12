// CWZLauncherDlg.h - the launcher main dialog (IDD 102).
#pragma once

#include "../Common/WZCommon.h"
#include "../Controls/CExplorer.h"
#include "../Controls/CGameListControl.h"
#include "../Controls/CTransparentStatic.h"
#include "../Controls/CWZBitmapButton.h"
#include "../Controls/CWZProgressCtrl.h"
#include "WZLauncherController.h"

class CWZLauncherDlg
{
public:
    CWZLauncherDlg();
    ~CWZLauncherDlg();

    INT_PTR DoModal();   // modal on template 102
    void    EndDialog(INT_PTR result);

    HWND    GetSafeHwnd() const { return m_hWnd; }
    WZLauncherController& GetController() { return m_controller; }

private:
    static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    BOOL    PreTranslateMessage(const MSG* msg);
    BOOL    OnInitDialog();
    void    InitUI();
    void    SetupButtons();
    void    SetupGameList();
    void    CreateExplorer();
    void    CenterWindow();

    void    OnPaint();
    BOOL    OnEraseBkgnd(HDC hdc);
    void    DrawBackground(HDC hdc, const RECT& rc);
    void    DrawTitleIcon(HDC hdc);
    HBRUSH  OnCtlColor(HDC hdc, HWND hwndCtl, UINT msg, WPARAM wParam, LPARAM lParam);
    void    OnLButtonDown(UINT flags, int x, int y);
    HICON   OnQueryDragIcon();
    void    OnWindowPosChanging(WINDOWPOS* pos);

    // Button handlers.
    void    OnMinimize();
    void    OnExitClicked();
    void    OnConfigClicked();
    void    OnAutoExecuteClicked();
    void    OnDxvkClicked();
    void    OnUpdateClicked();
    void    OnExecuteClicked();
    void    OnStopClicked();
    void    OnInstallClicked();

    // MsgSender message handlers.
    LRESULT OnSelectGame(WPARAM wParam, LPARAM lParam);
    LRESULT OnPopError(WPARAM wParam, LPARAM lParam);
    LRESULT OnStartLauncher(WPARAM wParam, LPARAM lParam);
    LRESULT OnExitLauncher(WPARAM wParam, LPARAM lParam);
    LRESULT OnSetWebURL(WPARAM wParam, LPARAM lParam);
    LRESULT OnSetUpdateState(WPARAM wParam, LPARAM lParam);
    LRESULT OnEnableButtons(WPARAM wParam, LPARAM lParam);
    LRESULT OnSetGameListState(WPARAM wParam, LPARAM lParam);
    LRESULT OnSetGameListEnable(WPARAM wParam, LPARAM lParam);
    LRESULT OnSetUpdateProgress(WPARAM wParam, LPARAM lParam);
    LRESULT OnSetFileProgress(WPARAM wParam, LPARAM lParam);
    LRESULT OnSetTotalStatic(WPARAM wParam, LPARAM lParam);
    LRESULT OnSetFileStatic(WPARAM wParam, LPARAM lParam);

    int     ShowMessage(const wchar_t* text);

public:
    HWND                m_hWnd;
    CTransparentStatic  m_stTitle;          // IDC_STATIC_TITLE
    CTransparentStatic  m_stTotal;          // IDC_STATIC_TOTAL
    CTransparentStatic  m_stFile;           // IDC_STATIC_FILE
    CExplorer           m_explorer;         // IDC_EXPLORER
    CWZBitmapButton     m_btnMinimize;      // IDC_BTN_MINIMIZE
    CWZBitmapButton     m_btnExit;          // IDC_BTN_EXIT
    CWZBitmapButton     m_btnConfig;        // IDC_BTN_CONFIG
    CWZBitmapButton     m_btnAutoExecute;   // IDC_CHK_AUTOEXEC
    CWZBitmapButton     m_btnDxvk;          // IDC_CHK_DXVK (Lunarite)
    CWZBitmapButton     m_btnUpdate;        // IDC_BTN_UPDATE
    CWZBitmapButton     m_btnExecute;       // IDC_BTN_EXECUTE
    CWZBitmapButton     m_btnStop;          // IDC_BTN_STOP
    CWZBitmapButton     m_btnInstall;       // IDC_BTN_INSTALL
    CWZProgressCtrl     m_progressTotal;    // IDC_PROGRESS_TOTAL
    CWZProgressCtrl     m_progressFile;     // IDC_PROGRESS_FILE
    CGameListControl    m_gameList;
    WZLauncherController m_controller;
    CommonInfo*         m_pCommonInfo;
    BOOL                m_bVisible;         // forced into WM_WINDOWPOSCHANGING
    HANDLE              m_hMutex;           // single-instance mutex (exe file name)
    HICON               m_hIcon;

    bool                m_bEnded;
    INT_PTR             m_nResult;
};
