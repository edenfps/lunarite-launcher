// CExplorer - WebBrowser (Shell.Explorer) host on ATL's CAxWindow.
#pragma once
#include "ControlUtil.h"
#include <atlhost.h>
#include <exdisp.h>

// Opens links that would pop a new IE window (target="_blank") in the user's default browser.
class CExplorerEvents : public IDispatch
{
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override { return 2; }   // lives as long as its CExplorer
    STDMETHODIMP_(ULONG) Release() override { return 1; }
    STDMETHODIMP GetTypeInfoCount(UINT* n) override { *n = 0; return S_OK; }
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override { return E_NOTIMPL; }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }
    STDMETHODIMP Invoke(DISPID id, REFIID, LCID, WORD, DISPPARAMS* params, VARIANT*, EXCEPINFO*, UINT*) override;
};

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
    CExplorerEvents m_events;
    DWORD     m_cookie = 0;
};
