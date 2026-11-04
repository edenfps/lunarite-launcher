#include "CWZLauncherDlg.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include "resource.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// Set by the dialog when its window exists; used by MsgSender.
static HWND g_hLauncherMainWnd = nullptr;

HWND GetLauncherMainWnd()
{
    return g_hLauncherMainWnd;
}

namespace
{
    // Message box caption: the exe file name without extension.
    const wchar_t* AppTitle()
    {
        static wchar_t title[MAX_PATH];
        if (!title[0])
        {
            wchar_t path[MAX_PATH] = {};
            GetModuleFileNameW(nullptr, path, MAX_PATH);
            const wchar_t* name = wcsrchr(path, L'\\');
            WZStrCopy(title, MAX_PATH, name ? name + 1 : path);
            if (wchar_t* dot = wcsrchr(title, L'.'))
                *dot = 0;
        }
        return title;
    }

    void ModifyStyle(HWND hwnd, DWORD remove, DWORD add)
    {
        DWORD style = GetWindowLongW(hwnd, GWL_STYLE);
        SetWindowLongW(hwnd, GWL_STYLE, (style & ~remove) | add);
    }

    int RectWidth(const RECT& r)  { return r.right - r.left; }
    int RectHeight(const RECT& r) { return r.bottom - r.top; }

    void MoveToRect(CWindow& wnd, const RECT& r)
    {
        wnd.MoveWindow(r.left, r.top, RectWidth(r), RectHeight(r), TRUE);
    }

    // region of all pixels that differ from the mask colour (tolerance +16 per channel).
    HRGN BitmapToRegion(HBITMAP hBmp, COLORREF mask)
    {
        HRGN result = nullptr;
        if (!hBmp)
            return nullptr;

        HDC hdc = CreateCompatibleDC(nullptr);
        if (!hdc)
            return nullptr;

        BITMAP bm;
        GetObjectW(hBmp, sizeof(bm), &bm);

        BITMAPINFO bmi;
        memset(&bmi, 0, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = bm.bmWidth;
        bmi.bmiHeader.biHeight = bm.bmHeight;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;

        void* bits = nullptr;
        HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dib)
        {
            HGDIOBJ oldDib = SelectObject(hdc, dib);
            HDC srcDc = CreateCompatibleDC(hdc);
            if (srcDc)
            {
                BITMAP dibInfo;
                GetObjectW(dib, sizeof(dibInfo), &dibInfo);
                int widthBytes = dibInfo.bmWidthBytes;
                while (widthBytes % 4)
                    ++widthBytes;

                HGDIOBJ oldSrc = SelectObject(srcDc, hBmp);
                BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, srcDc, 0, 0, SRCCOPY);

                const DWORD kAllocUnit = 100;
                DWORD maxRects = kAllocUnit;
                HGLOBAL hData = GlobalAlloc(GMEM_MOVEABLE, sizeof(RGNDATAHEADER) + sizeof(RECT) * maxRects);
                RGNDATA* data = (RGNDATA*)GlobalLock(hData);
                data->rdh.dwSize = sizeof(RGNDATAHEADER);
                data->rdh.iType = RDH_RECTANGLES;
                data->rdh.nCount = 0;
                data->rdh.nRgnSize = 0;
                SetRect(&data->rdh.rcBound, 0x7FFFFFFF, 0x7FFFFFFF, 0, 0);

                BYTE lo0 = (BYTE)mask;
                BYTE lo1 = (BYTE)(mask >> 8);
                BYTE lo2 = (BYTE)(mask >> 16);
                BYTE hi0 = (lo0 + 16 <= 255) ? (BYTE)(lo0 + 16) : 255;
                BYTE hi1 = (lo1 + 16 <= 255) ? (BYTE)(lo1 + 16) : 255;
                BYTE hi2 = (lo2 + 16 <= 255) ? (BYTE)(lo2 + 16) : 255;

                BYTE* row = (BYTE*)dibInfo.bmBits + widthBytes * (dibInfo.bmHeight - 1);
                for (int y = 0; y < bm.bmHeight; ++y)
                {
                    for (int x = 0; x < bm.bmWidth; ++x)
                    {
                        int xLeft = x;
                        const BYTE* p = row + 4 * x;
                        for (; x < bm.bmWidth; ++x, p += 4)
                        {
                            if (p[0] >= lo0 && p[0] <= hi0 &&
                                p[1] >= lo1 && p[1] <= hi1 &&
                                p[2] >= lo2 && p[2] <= hi2)
                                break;
                        }
                        if (x > xLeft)
                        {
                            if (data->rdh.nCount >= maxRects)
                            {
                                GlobalUnlock(hData);
                                maxRects += kAllocUnit;
                                hData = GlobalReAlloc(hData, sizeof(RGNDATAHEADER) + sizeof(RECT) * maxRects, GMEM_MOVEABLE);
                                data = (RGNDATA*)GlobalLock(hData);
                            }
                            RECT* rects = (RECT*)data->Buffer;
                            SetRect(&rects[data->rdh.nCount], xLeft, y, x, y + 1);
                            if (xLeft < data->rdh.rcBound.left)   data->rdh.rcBound.left = xLeft;
                            if (y < data->rdh.rcBound.top)        data->rdh.rcBound.top = y;
                            if (x > data->rdh.rcBound.right)      data->rdh.rcBound.right = x;
                            if (y + 1 > data->rdh.rcBound.bottom) data->rdh.rcBound.bottom = y + 1;
                            if (++data->rdh.nCount == 2000)
                            {
                                HRGN part = ExtCreateRegion(nullptr, sizeof(RGNDATAHEADER) + sizeof(RECT) * maxRects, data);
                                if (result)
                                {
                                    CombineRgn(result, result, part, RGN_OR);
                                    DeleteObject(part);
                                }
                                else
                                {
                                    result = part;
                                }
                                data->rdh.nCount = 0;
                                SetRect(&data->rdh.rcBound, 0x7FFFFFFF, 0x7FFFFFFF, 0, 0);
                            }
                        }
                    }
                    row -= widthBytes;
                }

                HRGN part = ExtCreateRegion(nullptr, sizeof(RGNDATAHEADER) + sizeof(RECT) * maxRects, data);
                if (result)
                {
                    CombineRgn(result, result, part, RGN_OR);
                    DeleteObject(part);
                }
                else
                {
                    result = part;
                }
                GlobalFree(hData);
                SelectObject(srcDc, oldSrc);
                DeleteDC(srcDc);
            }
            DeleteObject(SelectObject(hdc, oldDib));
        }
        DeleteDC(hdc);
        return result;
    }
}

CWZLauncherDlg::CWZLauncherDlg()
    : m_hWnd(nullptr)
    , m_pCommonInfo(nullptr)
    , m_bVisible(FALSE)
    , m_hMutex(nullptr)
    , m_hIcon(nullptr)
    , m_bEnded(false)
    , m_nResult(IDCANCEL)
{
    m_hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_WZLAUNCHER));
}

CWZLauncherDlg::~CWZLauncherDlg()
{
    if (m_hMutex)
    {
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
    }
}

INT_PTR CWZLauncherDlg::DoModal()
{
    HWND hDlg = CreateDialogParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_WZLAUNCHER_DIALOG),
                                   nullptr, &CWZLauncherDlg::DialogProc, (LPARAM)this);
    if (!hDlg)
        return -1;

    MSG msg;
    while (!m_bEnded && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (PreTranslateMessage(&msg))
            continue;
        if (IsDialogMessageW(hDlg, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (IsWindow(hDlg))
        DestroyWindow(hDlg);
    return m_nResult;
}

void CWZLauncherDlg::EndDialog(INT_PTR result)
{
    m_nResult = result;
    m_bEnded = true;
    if (m_hWnd)
        DestroyWindow(m_hWnd);
}

// Enter, Space and Escape never reach the dialog.
BOOL CWZLauncherDlg::PreTranslateMessage(const MSG* msg)
{
    if (msg->message == WM_KEYDOWN &&
        (msg->wParam == VK_RETURN || msg->wParam == VK_SPACE || msg->wParam == VK_ESCAPE))
        return TRUE;
    return FALSE;
}

INT_PTR CALLBACK CWZLauncherDlg::DialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CWZLauncherDlg* self;
    if (msg == WM_INITDIALOG)
    {
        self = reinterpret_cast<CWZLauncherDlg*>(lParam);
        SetWindowLongPtrW(hDlg, DWLP_USER, (LONG_PTR)self);
        self->m_hWnd = hDlg;
        g_hLauncherMainWnd = hDlg;
    }
    else
    {
        self = reinterpret_cast<CWZLauncherDlg*>(GetWindowLongPtrW(hDlg, DWLP_USER));
        if (!self)
            return FALSE;
    }
    return self->HandleMessage(msg, wParam, lParam);
}

INT_PTR CWZLauncherDlg::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto result = [this](LRESULT value) -> INT_PTR
    {
        SetWindowLongPtrW(m_hWnd, DWLP_MSGRESULT, (LONG_PTR)value);
        return TRUE;
    };

    switch (msg)
    {
    case WM_INITDIALOG:
        return OnInitDialog();

    case WM_PAINT:
        if (IsIconic(m_hWnd))
        {
            OnPaint();
            return TRUE;
        }
        return FALSE;

    case WM_CTLCOLORSTATIC:
        if (!m_pCommonInfo)
            return FALSE;
        return (INT_PTR)OnCtlColor((HDC)wParam, (HWND)lParam, msg, wParam, lParam);

    case WM_ERASEBKGND:
        return result(OnEraseBkgnd((HDC)wParam));

    case WM_LBUTTONDOWN:
        if (!m_pCommonInfo || m_pCommonInfo->TitleStaticRect.bottom <= GET_Y_LPARAM(lParam))
            return FALSE;
        OnLButtonDown((UINT)wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return TRUE;

    case WM_QUERYDRAGICON:
        return result((LRESULT)OnQueryDragIcon());

    case WM_WINDOWPOSCHANGING:
        OnWindowPosChanging((WINDOWPOS*)lParam);
        return FALSE;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_BTN_MINIMIZE:  OnMinimize();           return TRUE;
            case IDC_BTN_EXIT:      OnExitClicked();        return TRUE;
            case IDC_BTN_CONFIG:    OnConfigClicked();      return TRUE;
            case IDC_CHK_AUTOEXEC:  OnAutoExecuteClicked(); return TRUE;
            case IDC_BTN_UPDATE:    OnUpdateClicked();      return TRUE;
            case IDC_BTN_EXECUTE:   OnExecuteClicked();     return TRUE;
            case IDC_BTN_STOP:      OnStopClicked();        return TRUE;
            case IDC_BTN_INSTALL:   OnInstallClicked();     return TRUE;
            }
        }
        if (LOWORD(wParam) == IDCANCEL || LOWORD(wParam) == IDOK)
        {
            EndDialog(LOWORD(wParam));
            return TRUE;
        }
        return FALSE;

    case WM_WZ_SELECTGAME:          return result(OnSelectGame(wParam, lParam));
    case WM_WZ_POPERROR:            return result(OnPopError(wParam, lParam));
    case WM_WZ_STARTLAUNCHER:       return result(OnStartLauncher(wParam, lParam));
    case WM_WZ_EXITLAUNCHER:        return result(OnExitLauncher(wParam, lParam));
    case WM_WZ_SETWEBURL:           return result(OnSetWebURL(wParam, lParam));
    case WM_WZ_SETUPDATESTATE:      return result(OnSetUpdateState(wParam, lParam));
    case WM_WZ_ENABLEBUTTONS:       return result(OnEnableButtons(wParam, lParam));
    case WM_WZ_SETGAMELISTSTATE:    return result(OnSetGameListState(wParam, lParam));
    case WM_WZ_SETGAMELISTENABLE:   return result(OnSetGameListEnable(wParam, lParam));
    case WM_WZ_SETUPDATEPROGRESS:   return result(OnSetUpdateProgress(wParam, lParam));
    case WM_WZ_SETFILEPROGRESS:     return result(OnSetFileProgress(wParam, lParam));
    case WM_WZ_SETTOTALSTATIC:      return result(OnSetTotalStatic(wParam, lParam));
    case WM_WZ_SETFILESTATIC:       return result(OnSetFileStatic(wParam, lParam));
    }
    return FALSE;
}

int CWZLauncherDlg::ShowMessage(const wchar_t* text)
{
    // MB_ICONEXCLAMATION when no icon is given.
    return ::MessageBoxW(m_hWnd, text, AppTitle(), MB_OK | MB_ICONEXCLAMATION);
}

void CWZLauncherDlg::CenterWindow()
{
    RECT rcWindow;
    GetWindowRect(m_hWnd, &rcWindow);

    HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(monitor, &mi);
    const RECT& area = mi.rcWork;

    int w = RectWidth(rcWindow);
    int h = RectHeight(rcWindow);
    int x = (area.left + area.right) / 2 - w / 2;
    int y = (area.top + area.bottom) / 2 - h / 2;
    if (x + w > area.right)  x = area.right - w;
    if (y + h > area.bottom) y = area.bottom - h;
    if (x < area.left) x = area.left;
    if (y < area.top)  y = area.top;
    SetWindowPos(m_hWnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// The WebBrowser control is created in code; the template only holds a placeholder static.
void CWZLauncherDlg::CreateExplorer()
{
    HWND placeholder = GetDlgItem(m_hWnd, IDC_EXPLORER);
    RECT rc = {};
    HWND prev = nullptr;
    if (placeholder)
    {
        GetWindowRect(placeholder, &rc);
        MapWindowPoints(nullptr, m_hWnd, (POINT*)&rc, 2);
        prev = GetWindow(placeholder, GW_HWNDPREV);
        DestroyWindow(placeholder);
    }
    if (m_explorer.Create(L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP, rc, m_hWnd, IDC_EXPLORER))
        SetWindowPos(m_explorer.GetHwnd(), prev ? prev : HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

BOOL CWZLauncherDlg::OnInitDialog()
{
    // DDX_Control
    m_btnMinimize.SubclassDlgItem(IDC_BTN_MINIMIZE, m_hWnd);
    m_btnExit.SubclassDlgItem(IDC_BTN_EXIT, m_hWnd);
    m_btnConfig.SubclassDlgItem(IDC_BTN_CONFIG, m_hWnd);
    m_btnAutoExecute.SubclassDlgItem(IDC_CHK_AUTOEXEC, m_hWnd);
    m_btnUpdate.SubclassDlgItem(IDC_BTN_UPDATE, m_hWnd);
    m_btnExecute.SubclassDlgItem(IDC_BTN_EXECUTE, m_hWnd);
    m_btnStop.SubclassDlgItem(IDC_BTN_STOP, m_hWnd);
    m_btnInstall.SubclassDlgItem(IDC_BTN_INSTALL, m_hWnd);
    CreateExplorer();
    m_stTitle.SubclassDlgItem(IDC_STATIC_TITLE, m_hWnd);
    m_stTotal.SubclassDlgItem(IDC_STATIC_TOTAL, m_hWnd);
    m_stFile.SubclassDlgItem(IDC_STATIC_FILE, m_hWnd);
    m_progressTotal.SubclassDlgItem(IDC_PROGRESS_TOTAL, m_hWnd);
    m_progressFile.SubclassDlgItem(IDC_PROGRESS_FILE, m_hWnd);

    SendMessageW(m_hWnd, WM_SETICON, TRUE, (LPARAM)m_hIcon);
    SendMessageW(m_hWnd, WM_SETICON, FALSE, (LPARAM)m_hIcon);

    // Single instance: a mutex named after the exe file name.
    wchar_t exePath[260] = {};
    GetModuleFileNameW(GetModuleHandleW(nullptr), exePath, 260);
    const wchar_t* exeName = wcsrchr(exePath, L'\\') + 1;
    m_hMutex = CreateMutexW(nullptr, TRUE, exeName);
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        PostMessageW(m_hWnd, WM_WZ_EXITLAUNCHER, 0, 0);
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
        return FALSE;
    }

    bool initialized = m_controller.Initialize();
    CStringInfo* strings = m_controller.Settings().GetStringInfo();

    // SettingManager::ParseCommandLine supplies default arguments, so this
    // only fires when the configuration could not be read.
    if (m_controller.Settings().GetArguments().empty())
    {
        ShowMessage(strings->GetString(DESC_RUN_ON_THE_HOMEPAGE));
        PostMessageW(m_hWnd, WM_WZ_EXITLAUNCHER, 0, 0);
        return FALSE;
    }

    m_pCommonInfo = m_controller.GetCommonInfo();
    if (!m_pCommonInfo)
    {
        ShowMessage(L"An error occurred reading configuration file.");
        PostMessageW(m_hWnd, WM_WZ_EXITLAUNCHER, 0, 0);
        return FALSE;
    }

    if (m_pCommonInfo->UsePurpleLauncher)
    {
        HWND purple = m_pCommonInfo->hPurpleWnd;
        if (!purple)
        {
            ShowMessage(L"Not Exist Target Window(Purple Launcher) Handle");
            OnExitLauncher(0, 0);
            return FALSE;
        }
        OutputDebugStringW(L"MsgSender::SetOutBound");
        m_controller.Msg().m_bOutbound = TRUE;
        m_controller.Msg().m_hPurpleWnd = purple;
        m_controller.Msg().ClearOutboundParam();
        m_bVisible = FALSE;
        if (m_hWnd)
            ShowWindow(m_hWnd, SW_HIDE);
        if (!initialized)
        {
            m_controller.Msg().SendPurple(PURPLE_ENDING, L"An error occurred reading configuration file.", -2);
            OnExitLauncher(0, 0);
            return FALSE;
        }
    }
    else
    {
        if (!initialized)
        {
            ShowMessage(L"An error occurred reading configuration file.");
            OnExitLauncher(0, 0);
            return FALSE;
        }
        InitUI();
        m_bVisible = TRUE;
        if (m_hWnd)
            ShowWindow(m_hWnd, SW_SHOW);
        OutputDebugStringW(L"MsgSender::SetOutBound");
        m_controller.Msg().m_bOutbound = FALSE;
        m_controller.Msg().m_hPurpleWnd = nullptr;
    }

    m_controller.Start();
    return TRUE;
}

void CWZLauncherDlg::InitUI()
{
    CommonInfo* ui = m_pCommonInfo;

    ModifyStyle(m_hWnd, 0, WS_SYSMENU | WS_MINIMIZEBOX);
    ModifyStyle(m_stTotal.m_hWnd, 0, 0x8000);
    ModifyStyle(m_stFile.m_hWnd, 0, 0x8000);
    SetWindowLongW(m_progressTotal.m_hWnd, GWL_EXSTYLE, 0x8000);
    SetWindowLongW(m_progressFile.m_hWnd, GWL_EXSTYLE, 0x8000);

    SetWindowTextW(m_hWnd, ui->Title);
    SendMessageW(m_stTitle.m_hWnd, WM_SETFONT, (WPARAM)ui->hTitleFont, TRUE);
    SetWindowTextW(m_stTitle.m_hWnd, ui->Title);
    m_hIcon = ui->hIcon;

    MoveWindow(m_hWnd, 0, 0, ui->BackgroundWidth, ui->BackgroundHeight, TRUE);
    if (ui->BackgroundMaskEnabled)
    {
        HRGN rgn = BitmapToRegion(ui->hBackground, ui->BackgroundMaskColor);
        if (rgn)
        {
            SetWindowRgn(m_hWnd, rgn, TRUE);
            DeleteObject(rgn);
        }
    }

    { CWindow web(m_explorer.GetHwnd()); MoveToRect(web, ui->WebRect); }
    MoveToRect(m_progressTotal, ui->TotalProgressRect);
    MoveToRect(m_progressFile, ui->FileProgressRect);
    CenterWindow();
    MoveToRect(m_stTitle, ui->TitleStaticRect);
    MoveToRect(m_stTotal, ui->TotalStaticRect);
    MoveToRect(m_stFile, ui->FileStaticRect);
    SendMessageW(m_stTotal.m_hWnd, WM_SETFONT, (WPARAM)ui->hStaticFont, TRUE);
    SendMessageW(m_stFile.m_hWnd, WM_SETFONT, (WPARAM)ui->hStaticFont, TRUE);

    m_explorer.PutRegisterAsDropTarget(FALSE);
    SetupButtons();

    m_progressTotal.m_min = 0;
    m_progressTotal.m_max = 100;
    m_progressFile.m_min = 0;
    m_progressFile.m_max = 100;
    m_progressTotal.m_hasBitmap = TRUE;
    m_progressTotal.m_bitmap = ui->hFileProgressBar;
    m_progressFile.m_hasBitmap = TRUE;
    m_progressFile.m_bitmap = ui->hFileProgressBar;

    SetupGameList();
    SetWindowTextW(m_stTotal.m_hWnd, L"Total");
    SetWindowTextW(m_stFile.m_hWnd, L"File");
}

void CWZLauncherDlg::SetupButtons()
{
    CommonInfo* ui = m_pCommonInfo;

    if (ui->hMinimizeButton)
    {
        m_btnMinimize.SetBitmap(ui->hMinimizeButton);
        MoveToRect(m_btnMinimize, ui->MinimizeRect);
    }
    else
        m_btnMinimize.ShowWindow(SW_HIDE);

    if (ui->hExitButton)
    {
        m_btnExit.SetBitmap(ui->hExitButton);
        MoveToRect(m_btnExit, ui->ExitRect);
    }
    else
        m_btnExit.ShowWindow(SW_HIDE);

    if (ui->hConfigurationButton)
    {
        m_btnConfig.SetBitmap(ui->hConfigurationButton);
        MoveToRect(m_btnConfig, ui->ConfigurationRect);
    }
    else
        m_btnConfig.ShowWindow(SW_HIDE);

    if (ui->hCheckBoxChecked)
    {
        m_btnAutoExecute.SetToggleBitmaps(ui->hCheckBoxChecked, ui->hCheckBoxUnchecked);
        MoveToRect(m_btnAutoExecute, ui->CheckBoxRect);
        m_btnAutoExecute.m_toggled = m_controller.GetAutoExecute();
    }
    else
        m_btnAutoExecute.ShowWindow(SW_HIDE);

    m_btnUpdate.ShowWindow(SW_HIDE);
    m_btnExecute.ShowWindow(SW_HIDE);
    m_btnStop.ShowWindow(SW_HIDE);
    m_btnInstall.ShowWindow(SW_HIDE);

    // All four action buttons share one rectangle.
    if (ui->hUpdateButton)
    {
        m_btnUpdate.SetBitmap(ui->hUpdateButton);
        MoveToRect(m_btnUpdate, ui->ActionButtonRect);
    }
    if (ui->hExecuteButton)
    {
        m_btnExecute.SetBitmap(ui->hExecuteButton);
        MoveToRect(m_btnExecute, ui->ActionButtonRect);
    }
    if (ui->hStopButton)
    {
        m_btnStop.SetBitmap(ui->hStopButton);
        MoveToRect(m_btnStop, ui->ActionButtonRect);
    }
    if (ui->hInstallButton)
    {
        m_btnInstall.SetBitmap(ui->hInstallButton);
        MoveToRect(m_btnInstall, ui->ActionButtonRect);
    }
}

void CWZLauncherDlg::SetupGameList()
{
    CommonInfo* ui = m_pCommonInfo;
    if (!ui->GameListEnabled)
        return;

    RECT client;
    GetClientRect(m_hWnd, &client);

    if (m_gameList.Create(m_hWnd, ui->GameListRect, nullptr, WS_CHILD | WS_VISIBLE, 0, 1001u))
        m_gameList.ShowWindow(SW_SHOWNORMAL);

    m_gameList.m_itemHeight = ui->GameListLineHeight;
    m_gameList.m_itemFont = ui->hStaticFont;
    m_gameList.m_itemText[0] = 100;          // RGB(100, 0, 0)
    m_gameList.m_itemText[1] = 25600;        // RGB(0, 100, 0)
    m_gameList.m_itemText[2] = 6553600;      // RGB(0, 0, 100)
    m_gameList.m_itemText[3] = 6579300;      // RGB(100, 100, 100)

    std::vector<GameEntry*> games = m_controller.Settings().GetGames();
    for (GameEntry* entry : games)
    {
        if (!entry || !entry->ui)
            continue;

        GameItemInfo item;
        item.SetId(entry->ui->Guid);
        item.SetTitle(entry->ui->Name);
        HICON icon = entry->ui->hListIcon;
        HBITMAP image = entry->ui->hListImage;
        item.hasIcon = 1;
        item.icon = icon;
        item.bitmap = image;
        m_gameList.AddItem(item);
        if (icon)
            DestroyIcon(icon);
        if (image)
            DeleteObject(image);
    }
    m_gameList.EnableWindow(FALSE);
}

void CWZLauncherDlg::OnPaint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(m_hWnd, &ps);
    SendMessageW(m_hWnd, WM_ICONERASEBKGND, (WPARAM)hdc, 0);

    int cxIcon = GetSystemMetrics(SM_CXICON);
    int cyIcon = GetSystemMetrics(SM_CYICON);
    RECT rc;
    GetClientRect(m_hWnd, &rc);
    DrawIcon(hdc, (rc.right - rc.left - cxIcon + 1) / 2, (rc.bottom - rc.top - cyIcon + 1) / 2, m_hIcon);
    EndPaint(m_hWnd, &ps);
}

BOOL CWZLauncherDlg::OnEraseBkgnd(HDC hdc)
{
    RECT rc;
    GetClientRect(m_hWnd, &rc);

    HDC mem = CreateCompatibleDC(hdc);
    SetBkMode(mem, TRANSPARENT);
    HBITMAP bitmap = CreateCompatibleBitmap(hdc, RectWidth(rc), RectHeight(rc));
    HGDIOBJ old = SelectObject(mem, bitmap);

    DrawBackground(mem, rc);
    DrawTitleIcon(mem);
    BitBlt(hdc, 0, 0, RectWidth(rc), RectHeight(rc), mem, 0, 0, SRCCOPY);

    SelectObject(mem, old);
    DeleteDC(mem);
    DeleteObject(bitmap);
    return FALSE;
}

void CWZLauncherDlg::DrawBackground(HDC hdc, const RECT& rc)
{
    HDC src = CreateCompatibleDC(hdc);
    HGDIOBJ old = SelectObject(src, m_pCommonInfo ? m_pCommonInfo->hBackground : nullptr);
    if (m_pCommonInfo && m_pCommonInfo->hBackground)
    {
        BitBlt(hdc, rc.left, rc.top, RectWidth(rc), RectHeight(rc), src, 0, 0, SRCCOPY);
    }
    else
    {
        HBRUSH brush = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
    }
    SelectObject(src, old);
    DeleteDC(src);
}

void CWZLauncherDlg::DrawTitleIcon(HDC hdc)
{
    if (m_pCommonInfo && m_pCommonInfo->hIcon)
        DrawStateW(hdc, nullptr, nullptr, (LPARAM)m_pCommonInfo->hIcon, 0,
                   m_pCommonInfo->TitleStaticRect.left - 19, m_pCommonInfo->TitleStaticRect.top - 1,
                   16, 16, DST_ICON | DSS_NORMAL);
}

HBRUSH CWZLauncherDlg::OnCtlColor(HDC hdc, HWND hwndCtl, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // First let the control handle the reflected message: the three statics are
    // CTransparentStatic ( -> transparent mode, NULL brush).
    int id = GetDlgCtrlID(hwndCtl);
    if (id != IDC_STATIC_TITLE && id != IDC_STATIC_TOTAL && id != IDC_STATIC_FILE)
        return (HBRUSH)DefWindowProcW(m_hWnd, msg, wParam, lParam);

    HBRUSH brush = CTransparentStatic().OnCtlColor(hdc);
    SetTextColor(hdc, id == IDC_STATIC_TITLE ? m_pCommonInfo->TitleColor : m_pCommonInfo->StaticColor);
    return brush;
}

void CWZLauncherDlg::OnLButtonDown(UINT /*flags*/, int /*x*/, int /*y*/)
{
    SendMessageW(m_hWnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

HICON CWZLauncherDlg::OnQueryDragIcon()
{
    return m_hIcon;
}

void CWZLauncherDlg::OnWindowPosChanging(WINDOWPOS* pos)
{
    if (m_bVisible)
        pos->flags |= SWP_SHOWWINDOW;
    else
        pos->flags &= ~SWP_SHOWWINDOW;
}

void CWZLauncherDlg::OnMinimize()
{
    ShowWindow(m_hWnd, SW_MINIMIZE);
}

void CWZLauncherDlg::OnExitClicked()
{
    m_controller.UserClickedExit();
}

void CWZLauncherDlg::OnConfigClicked()
{
    m_controller.UserClickedConfig();
}

void CWZLauncherDlg::OnAutoExecuteClicked()
{
    m_controller.SetAutoExecute(m_btnAutoExecute.m_toggled);
}

void CWZLauncherDlg::OnUpdateClicked()
{
    m_controller.StartGameUpdate();
}

void CWZLauncherDlg::OnExecuteClicked()
{
    m_controller.ExecuteGame();
}

void CWZLauncherDlg::OnStopClicked()
{
    m_controller.UserBreak();
}

void CWZLauncherDlg::OnInstallClicked()
{
    m_controller.PopUpInstallPage();
}

LRESULT CWZLauncherDlg::OnSelectGame(WPARAM wParam, LPARAM)
{
    m_controller.SelectGame((const wchar_t*)wParam);
    return 0;
}

LRESULT CWZLauncherDlg::OnPopError(WPARAM wParam, LPARAM lParam)
{
    if (!m_pCommonInfo->UsePurpleLauncher)
        ShowMessage((const wchar_t*)wParam);
    if (lParam)
        m_btnUpdate.EnableWindow(FALSE);
    return 0;
}

LRESULT CWZLauncherDlg::OnStartLauncher(WPARAM wParam, LPARAM)
{
    if (!m_pCommonInfo->GameListEnabled)
        return 0;

    m_gameList.EnableWindow(TRUE);

    CGameItemButton* item = nullptr;
    for (int i = 0; i < m_gameList.GetItemCount(); ++i)
    {
        CGameItemButton* candidate = m_gameList.GetItem(i);
        if (candidate && candidate->Id() == (const wchar_t*)wParam)
        {
            item = candidate;
            break;
        }
    }
    if (!item && m_gameList.GetItemCount() > 0)
        item = m_gameList.GetItem(0);

    if (item)
    {
        SendMessageW(item->m_hWnd, WM_LBUTTONDOWN, 0, 0);
        SendMessageW(item->m_hWnd, WM_LBUTTONUP, 0, 0);
    }
    return 0;
}

LRESULT CWZLauncherDlg::OnExitLauncher(WPARAM, LPARAM)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT CWZLauncherDlg::OnSetWebURL(WPARAM wParam, LPARAM)
{
    m_explorer.Navigate((const wchar_t*)wParam);
    return 0;
}

LRESULT CWZLauncherDlg::OnSetUpdateState(WPARAM wParam, LPARAM)
{
    CommonInfo* ui = m_pCommonInfo;

    switch ((int)wParam)
    {
    case UPDATE_STATE_NEED_UPDATE:
        m_btnExecute.ShowWindow(SW_HIDE);
        m_btnStop.ShowWindow(SW_HIDE);
        m_btnInstall.ShowWindow(SW_HIDE);
        if (ui->hUpdateButton)
        {
            m_btnUpdate.EnableWindow(TRUE);
            m_btnUpdate.ShowWindow(SW_SHOW);
        }
        else
            m_btnUpdate.ShowWindow(SW_HIDE);
        break;

    case UPDATE_STATE_UPDATING:
        m_btnUpdate.ShowWindow(SW_HIDE);
        m_btnInstall.ShowWindow(SW_HIDE);
        if (ui->hStopButton)
        {
            m_btnExecute.ShowWindow(SW_HIDE);
            m_btnStop.EnableWindow(TRUE);
            m_btnStop.ShowWindow(SW_SHOW);
        }
        else if (ui->hExecuteButton)
        {
            m_btnStop.ShowWindow(SW_HIDE);
            m_btnExecute.EnableWindow(FALSE);
            m_btnExecute.ShowWindow(SW_SHOW);
        }
        else
        {
            m_btnStop.ShowWindow(SW_HIDE);
            m_btnExecute.ShowWindow(SW_HIDE);
        }
        break;

    case UPDATE_STATE_NEED_INSTALL:
        m_btnUpdate.ShowWindow(SW_HIDE);
        m_btnExecute.ShowWindow(SW_HIDE);
        m_btnStop.ShowWindow(SW_HIDE);
        if (ui->hInstallButton)
        {
            m_btnInstall.EnableWindow(TRUE);
            m_btnInstall.ShowWindow(SW_SHOW);
        }
        else
            m_btnInstall.ShowWindow(SW_HIDE);
        break;

    case UPDATE_STATE_NET_ERROR:
        m_btnUpdate.ShowWindow(SW_HIDE);
        m_btnExecute.ShowWindow(SW_HIDE);
        m_btnStop.ShowWindow(SW_HIDE);
        m_btnInstall.ShowWindow(SW_HIDE);
        if (ui->hUpdateButton)
        {
            m_btnUpdate.EnableWindow(FALSE);
            m_btnUpdate.ShowWindow(SW_SHOW);
        }
        else if (ui->hExecuteButton)
        {
            m_btnExecute.EnableWindow(FALSE);
            m_btnExecute.ShowWindow(SW_SHOW);
        }
        break;

    default:
        m_btnUpdate.ShowWindow(SW_HIDE);
        m_btnStop.ShowWindow(SW_HIDE);
        m_btnInstall.ShowWindow(SW_HIDE);
        if (ui->hExecuteButton)
        {
            m_btnExecute.EnableWindow(TRUE);
            m_btnExecute.ShowWindow(SW_SHOW);
        }
        else
            m_btnExecute.ShowWindow(SW_HIDE);
        break;
    }
    return 0;
}

LRESULT CWZLauncherDlg::OnEnableButtons(WPARAM wParam, LPARAM)
{
    m_btnExit.EnableWindow((BOOL)wParam);
    if (m_pCommonInfo->hStopButton)
        m_btnStop.EnableWindow((BOOL)wParam);
    return 0;
}

LRESULT CWZLauncherDlg::OnSetGameListState(WPARAM wParam, LPARAM lParam)
{
    if (!m_pCommonInfo->GameListEnabled)
        return 0;

    CGameItemButton* item = nullptr;
    for (int i = 0; i < m_gameList.GetItemCount(); ++i)
    {
        CGameItemButton* candidate = m_gameList.GetItem(i);
        if (candidate && candidate->Id() == (const wchar_t*)wParam)
        {
            item = candidate;
            break;
        }
    }
    if (item)
    {
        item->SetStateText((const wchar_t*)lParam);
        InvalidateRect(item->m_hWnd, nullptr, TRUE);
    }
    return 0;
}

LRESULT CWZLauncherDlg::OnSetGameListEnable(WPARAM wParam, LPARAM)
{
    if (m_pCommonInfo->GameListEnabled)
        m_gameList.m_sticky = (BOOL)wParam;
    return 0;
}

LRESULT CWZLauncherDlg::OnSetUpdateProgress(WPARAM wParam, LPARAM)
{
    int pos = (int)wParam;
    int old = m_progressTotal.m_pos;
    if (old != pos)
    {
        if (IsWindow(m_progressTotal.m_hWnd))
        {
            int prev = m_progressTotal.m_pos;
            m_progressTotal.m_pos = pos;
            if (prev != pos)
                RedrawWindow(m_progressTotal.m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
        }
        if (old > pos)
        {
            RECT rc = m_pCommonInfo->TotalProgressRect;
            InvalidateRect(m_hWnd, &rc, TRUE);
        }
    }
    return 0;
}

LRESULT CWZLauncherDlg::OnSetFileProgress(WPARAM wParam, LPARAM)
{
    int pos = (int)wParam;
    int old = m_progressFile.m_pos;
    if (old != pos)
    {
        if (IsWindow(m_progressFile.m_hWnd))
        {
            int prev = m_progressFile.m_pos;
            m_progressFile.m_pos = pos;
            if (prev != pos)
                RedrawWindow(m_progressFile.m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
        }
        if (old > pos)
        {
            RECT rc = m_pCommonInfo->FileProgressRect;
            InvalidateRect(m_hWnd, &rc, TRUE);
        }
    }
    return 0;
}

LRESULT CWZLauncherDlg::OnSetTotalStatic(WPARAM wParam, LPARAM)
{
    wchar_t current[260] = {};
    GetWindowTextW(m_stTotal.m_hWnd, current, 260);
    if (wcscmp((const wchar_t*)wParam, current))
        SetWindowTextW(m_stTotal.m_hWnd, (const wchar_t*)wParam);
    return 0;
}

LRESULT CWZLauncherDlg::OnSetFileStatic(WPARAM wParam, LPARAM)
{
    wchar_t current[260] = {};
    GetWindowTextW(m_stFile.m_hWnd, current, 260);
    if (wcscmp((const wchar_t*)wParam, current))
        SetWindowTextW(m_stFile.m_hWnd, (const wchar_t*)wParam);
    return 0;
}
