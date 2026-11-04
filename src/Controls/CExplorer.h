// CExplorer - WebBrowser (Shell.Explorer) host on ATL's CAxWindow.
#pragma once
#include "ControlUtil.h"
#include <atlhost.h>
#include <exdisp.h>

class CExplorer
{
public:
    CExplorer() : m_hWnd(nullptr) {}
    ~CExplorer();

    // creates the control as a child of 'parent'. Calls AtlAxWinInit() once.
    BOOL Create(LPCWSTR windowName, DWORD style, const RECT& rect, HWND parent, UINT id);

    // DISPID 104 (IWebBrowser::Navigate) with no flags/frame/post data/headers.
    HRESULT Navigate(LPCWSTR url);
    // DISPID 553 (IWebBrowser2::put_RegisterAsDropTarget); the dialog passes FALSE.
    HRESULT PutRegisterAsDropTarget(BOOL registerIt);

    HWND GetHwnd() const { return m_hWnd; }
    BOOL MoveWindow(int x, int y, int w, int h, BOOL repaint) { return ::MoveWindow(m_hWnd, x, y, w, h, repaint); }
    BOOL ShowWindow(int cmd) { return ::ShowWindow(m_hWnd, cmd); }
    operator HWND() const { return m_hWnd; }

private:
    CComPtr<IWebBrowser2> Browser();

    HWND      m_hWnd;
    CAxWindow m_ax;
};
