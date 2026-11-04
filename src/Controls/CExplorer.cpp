#include "CExplorer.h"

CExplorer::~CExplorer()
{
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
