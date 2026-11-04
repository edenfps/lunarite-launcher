#include "CWZProgressCtrl.h"
#include <wchar.h>

CWZProgressCtrl::CWZProgressCtrl()
    : m_pos(0), m_max(100), m_min(0), m_reserved(0), m_hasBitmap(0), m_bitmap(nullptr),
      m_fillColor(GetSysColor(COLOR_HIGHLIGHT)), m_color2(GetSysColor(COLOR_WINDOW)),
      m_color3(GetSysColor(COLOR_HIGHLIGHT)), m_color4(GetSysColor(COLOR_WINDOW))
{
    memset(m_text, 0, sizeof(m_text));
}

BOOL CWZProgressCtrl::SubclassDlgItem(UINT id, HWND parent)
{
    HWND h = ::GetDlgItem(parent, id);
    return h && SubclassWindow(h);
}

void CWZProgressCtrl::SetPos(int pos)
{
    if (m_pos != pos)
    {
        m_pos = pos;
        if (IsWindow())
            RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }
}

void CWZProgressCtrl::DrawFill(HDC dc, const RECT& rc)
{
    if (m_hasBitmap)
    {
        HDC src = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(src, m_bitmap);
        SIZE sz = WZGetBitmapSize(m_bitmap);
        StretchBlt(dc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                   src, 0, 0, sz.cx, sz.cy, SRCCOPY);
        SelectObject(src, old);
        DeleteDC(src);
    }
    else
    {
        WZFillRect(dc, rc, m_fillColor);
    }
}

LRESULT CWZProgressCtrl::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(&ps);
    if (m_min < m_max)
    {
        RECT client;
        GetClientRect(&client);

        RECT rc;
        rc.left = client.left;
        rc.top = client.top;
        rc.bottom = client.bottom;
        rc.right = client.left +
            (int)((double)(m_pos - m_min) / (double)(m_max - m_min) * (double)(client.right - client.left));

        // The buffer is only as wide as the filled part; the BitBlt below copies the full client size.
        WZMemDC mem(dc, rc.right - rc.left, rc.bottom - rc.top);
        DrawFill(mem, rc);
        BitBlt(dc, 0, 0, client.right - client.left, client.bottom - client.top, mem, 0, 0, SRCCOPY);
    }
    EndPaint(&ps);
    return 0;
}

LRESULT CWZProgressCtrl::OnSetText(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    LRESULT r = DefWindowProc(msg, wp, lp);
    const wchar_t* text = (const wchar_t*)lp;
    if (text && *text)
    {
        if (wcscmp(m_text, text) != 0)
        {
            WZStrCopy(m_text, 260, text);
            InvalidateRect(nullptr, TRUE);
        }
    }
    handled = TRUE;
    return r;
}

LRESULT CWZProgressCtrl::OnGetText(UINT, WPARAM wp, LPARAM lp, BOOL& handled)
{
    handled = TRUE;
    wchar_t* dest = (wchar_t*)lp;
    size_t count = (size_t)wp;
    if (!dest)
        return 0;
    wcsncpy(dest, m_text, count);
    size_t len = wcsnlen(m_text, 260);
    if (len >= 260)
        return 0;
    return (LRESULT)(count < len ? count : len);
}
