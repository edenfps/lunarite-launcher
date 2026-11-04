#include "CWZBitmapButton.h"

CWZBitmapButton::CWZBitmapButton()
    : m_himl1(nullptr), m_himl2(nullptr), m_ownsLists(0), m_pressed(0), m_hover(0),
      m_toggleMode(0), m_toggled(0), m_down(false)
{
}

CWZBitmapButton::~CWZBitmapButton()
{
    ReleaseLists();
    m_ownsLists = 0;
}

void CWZBitmapButton::ReleaseLists()
{
    if (m_ownsLists)
    {
        if (m_himl1)
        {
            ImageList_Destroy(m_himl1);
            m_himl1 = nullptr;
        }
        if (m_himl2)
        {
            ImageList_Destroy(m_himl2);
            m_himl2 = nullptr;
        }
    }
}

BOOL CWZBitmapButton::SubclassDlgItem(UINT id, HWND parent)
{
    HWND h = ::GetDlgItem(parent, id);
    if (!h || !SubclassWindow(h))
        return FALSE;
    ModifyStyle(0xF, 0xB, SWP_FRAMECHANGED);    // BS_OWNERDRAW
    return TRUE;
}

void CWZBitmapButton::SetBitmap(HBITMAP strip)
{
    ReleaseLists();
    m_ownsLists = 1;
    m_himl1 = WZCreateFrameImageList(strip);
}

void CWZBitmapButton::SetToggleBitmaps(HBITMAP on, HBITMAP off)
{
    ReleaseLists();
    m_ownsLists = 1;
    m_toggleMode = 1;
    m_himl1 = WZCreateFrameImageList(on);
    m_himl2 = WZCreateFrameImageList(off);
}

void CWZBitmapButton::Toggle()
{
    if (m_toggleMode)
    {
        m_toggled = (m_toggled == 0);
        InvalidateRect(nullptr, TRUE);
    }
}

void CWZBitmapButton::DrawFrames(HDC dc, const RECT& rc, bool disabled)
{
    int frame;
    if (disabled)       frame = 3;
    else if (m_pressed) frame = 1;
    else                frame = m_hover ? 2 : 0;

    if (m_himl1 || m_ownsLists)
    {
        HIMAGELIST src = m_himl1;
        if (src)
        {
            if (m_toggleMode && !m_toggled && m_himl2)
                src = m_himl2;
            ImageList_DrawEx(src, frame, dc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                             CLR_NONE, CLR_NONE, ILD_NORMAL);
        }
        else
        {
            WZFillRect(dc, rc, GetSysColor(COLOR_HIGHLIGHT));
        }
    }
}

void CWZBitmapButton::DrawItem(HDC target, UINT itemState)
{
    RECT rc;
    GetClientRect(&rc);
    WZMemDC mem(target, rc.right - rc.left, rc.bottom - rc.top);
    DrawFrames(mem, rc, (itemState & ODS_DISABLED) != 0);
    BitBlt(target, 0, 0, rc.right - rc.left, rc.bottom - rc.top, mem, 0, 0, SRCCOPY);
}

LRESULT CWZBitmapButton::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(&ps);
    DrawItem(dc, IsWindowEnabled() ? 0 : ODS_DISABLED);
    EndPaint(&ps);
    return 0;
}

LRESULT CWZBitmapButton::OnMouseMove(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    LRESULT r = DefWindowProc(msg, wp, lp);
    bool hover = m_hover != 0;
    POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
    WZTrackHover(m_hWnd, pt, hover);
    m_hover = hover;
    handled = TRUE;
    return r;
}

LRESULT CWZBitmapButton::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL& handled)
{
    m_hover = 0;
    InvalidateRect(nullptr, TRUE);
    handled = TRUE;
    return 0;
}

LRESULT CWZBitmapButton::OnLButtonDown(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    m_pressed = 1;
    m_down = true;
    InvalidateRect(nullptr, TRUE);
    handled = TRUE;
    return DefWindowProc(msg, wp, lp);
}

// (+ the BN_CLICKED reflection that triggers Toggle)
LRESULT CWZBitmapButton::OnLButtonUp(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    m_pressed = 0;
    InvalidateRect(nullptr, TRUE);
    bool click = false;
    if (m_down)
    {
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        RECT rc;
        GetClientRect(&rc);
        click = PtInRect(&rc, pt) != FALSE;
    }
    m_down = false;
    handled = TRUE;
    LRESULT r = DefWindowProc(msg, wp, lp);
    if (click)
        Toggle();
    return r;
}

LRESULT CWZBitmapButton::OnBmClick(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    handled = TRUE;
    LRESULT r = DefWindowProc(msg, wp, lp);
    Toggle();
    return r;
}

LRESULT CWZBitmapButton::OnKeyUp(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    handled = TRUE;
    LRESULT r = DefWindowProc(msg, wp, lp);
    if (wp == VK_SPACE)
        Toggle();
    return r;
}
