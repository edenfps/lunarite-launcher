#include "CExplorer.h"
#include <exdispid.h>
#include <shellapi.h>

STDMETHODIMP CExplorerEvents::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_IDispatch || riid == DIID_DWebBrowserEvents2)
    {
        *ppv = static_cast<IDispatch*>(this);
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

// NewWindow3(ppDisp, Cancel, dwFlags, bstrUrlContext, bstrUrl): DISPPARAMS holds them in reverse.
STDMETHODIMP CExplorerEvents::Invoke(DISPID id, REFIID, LCID, WORD, DISPPARAMS* params, VARIANT*, EXCEPINFO*, UINT*)
{
    if (id != DISPID_NEWWINDOW3 || !params || params->cArgs < 5)
        return S_OK;
    VARIANT& url = params->rgvarg[0];
    VARIANT& cancel = params->rgvarg[3];
    if (url.vt != VT_BSTR || !url.bstrVal || !(cancel.vt & VT_BYREF))
        return S_OK;
    // Web links only; anything else keeps IE's default handling.
    if (_wcsnicmp(url.bstrVal, L"https://", 8) != 0 && _wcsnicmp(url.bstrVal, L"http://", 7) != 0)
        return S_OK;
    ShellExecuteW(nullptr, L"open", url.bstrVal, nullptr, nullptr, SW_SHOWNORMAL);
    *cancel.pboolVal = VARIANT_TRUE;
    return S_OK;
}

CExplorer::~CExplorer()
{
    if (m_cookie)
        if (CComPtr<IWebBrowser2> wb = Browser())
            AtlUnadvise(wb, DIID_DWebBrowserEvents2, m_cookie);
    if (m_ax.IsWindow())
        m_ax.DestroyWindow();
}

BOOL CExplorer::Create(LPCWSTR windowName, DWORD style, const RECT& rect, HWND parent, UINT id)
{
    static bool s_axInit = false;
    if (!s_axInit)
    {
        AtlAxWinInit();
        s_axInit = true;
    }

    // Window name is the control's CLSID, exactly what CreateControl(clsid, ...) hosts.
    HWND h = m_ax.Create(parent, const_cast<RECT&>(rect), L"{8856F961-340A-11D0-A96B-00C04FD705A2}",
                         style, 0, id);
    m_hWnd = h;
    if (h && windowName && *windowName)
        ::SetWindowTextW(h, windowName);
    if (h)
        if (CComPtr<IWebBrowser2> wb = Browser())
            AtlAdvise(wb, &m_events, DIID_DWebBrowserEvents2, &m_cookie);
    return h != nullptr;
}

CComPtr<IWebBrowser2> CExplorer::Browser()
{
    CComPtr<IWebBrowser2> wb;
    if (m_ax.IsWindow())
        m_ax.QueryControl(&wb);
    return wb;
}

// DISPID 104
HRESULT CExplorer::Navigate(LPCWSTR url)
{
    CComPtr<IWebBrowser2> wb = Browser();
    if (!wb)
        return E_UNEXPECTED;
    CComBSTR bstr(url);
    return wb->Navigate(bstr, nullptr, nullptr, nullptr, nullptr);
}

// DISPID 553
HRESULT CExplorer::PutRegisterAsDropTarget(BOOL registerIt)
{
    CComPtr<IWebBrowser2> wb = Browser();
    if (!wb)
        return E_UNEXPECTED;
    return wb->put_RegisterAsDropTarget(registerIt ? VARIANT_TRUE : VARIANT_FALSE);
}
