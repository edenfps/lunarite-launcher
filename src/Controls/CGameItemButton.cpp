#include "CGameItemButton.h"
#include <wchar.h>

CGameItemButton::CGameItemButton(const GameItemInfo& info)
    : m_info(info), m_pressed(0), m_hover(0), m_useColors(0), m_useBorder(0), m_himl(nullptr), m_down(false)
{
    for (int i = 0; i < 4; ++i)
    {
        m_bgColors[i]     = GetSysColor(COLOR_BTNFACE);
        m_borderColors[i] = GetSysColor(COLOR_ACTIVEBORDER);
        m_textColors[i]   = GetSysColor(COLOR_BTNTEXT);
        m_fonts[i]        = nullptr;
    }

    // the 4-frame strip becomes the image list (the bitmap is consumed).
    m_himl = WZCreateFrameImageList(m_info.bitmap);
    m_info.bitmap = nullptr;

    // The by-value descriptor copy is destroyed at the end of the constructor, taking the icon with it.
    if (info.icon)
    {
        DestroyIcon(info.icon);
        m_info.icon = nullptr;
    }
}

CGameItemButton::~CGameItemButton()
{
    if (m_himl)
        ImageList_Destroy(m_himl);
    for (int i = 0; i < 4; ++i)
        if (m_fonts[i])
            DeleteObject(m_fonts[i]);
    if (m_info.icon)
        DestroyIcon(m_info.icon);
    if (m_info.bitmap)
        DeleteObject(m_info.bitmap);
}

BOOL CGameItemButton::CreateItem(HWND parent, const RECT& rc)
{
    HWND h = CreateWindowExW(0, L"BUTTON", m_info.id, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                             rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                             parent, (HMENU)(UINT_PTR)WZ_ITEM_CONTROLID, GetModuleHandleW(nullptr), nullptr);
    if (!h)
        return FALSE;
    return SubclassWindow(h);
}

void CGameItemButton::SetStateColors(const COLORREF bg[4], const COLORREF border[4], int borderFlag)
{
    m_useColors = 1;
    for (int i = 0; i < 4; ++i)
        m_bgColors[i] = bg[i];
    if (borderFlag)
    {
        m_useBorder = borderFlag;
        for (int i = 0; i < 4; ++i)
            m_borderColors[i] = border[i];
    }
}

void CGameItemButton::SetTextStyle(HFONT baseFont, const COLORREF text[4])
{
    for (int i = 0; i < 4; ++i)
        m_textColors[i] = text[i];

    LOGFONTW lf = {};
    GetObjectW(baseFont, sizeof(lf), &lf);

    for (int i = 0; i < 4; ++i)
        if (m_fonts[i])
            DeleteObject(m_fonts[i]);

    lf.lfWeight = 400;
    m_fonts[0] = CreateFontIndirectW(&lf);     // normal
    m_fonts[2] = CreateFontIndirectW(&lf);     // hover
    m_fonts[3] = CreateFontIndirectW(&lf);     // disabled
    lf.lfWeight = 600;
    m_fonts[1] = CreateFontIndirectW(&lf);     // pressed
}

void CGameItemButton::DrawBackground(HDC dc, const RECT& rc, bool disabled)
{
    if (!m_useColors)
        return;
    COLORREF c;
    if (disabled)       c = m_bgColors[3];
    else if (m_pressed) c = m_bgColors[1];
    else if (m_hover)   c = m_bgColors[2];
    else                c = m_bgColors[0];
    WZFillRect(dc, rc, c);
}

void CGameItemButton::DrawImage(HDC dc, const RECT& rc, bool disabled)
{
    int frame;
    if (disabled)       frame = 3;
    else if (m_pressed) frame = 1;
    else                frame = m_hover ? 2 : 0;

    if (m_himl)
        ImageList_DrawEx(m_himl, frame, dc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                         CLR_NONE, CLR_NONE, ILD_NORMAL);
    else
        WZFillRect(dc, rc, GetSysColor(COLOR_HIGHLIGHT));
}

void CGameItemButton::DrawBorder(HDC dc, const RECT& rc, bool disabled)
{
    if (!m_useBorder)
        return;
    COLORREF c;
    if (disabled)       c = m_borderColors[3];
    else if (m_pressed) c = m_borderColors[1];
    else if (m_hover)   c = m_borderColors[2];
    else                c = m_borderColors[0];
    WZFrameRect(dc, rc, c);
}

void CGameItemButton::DrawItemIcon(HDC dc, const RECT& rc)
{
    if (!m_info.hasIcon || !m_info.icon)
        return;
    int size = m_info.iconSize;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int x = 0, y = 0;
    if (h > size)
        y = (h - size) / 2;
    if (w - 0 > size + 10)
        x = 5;
    else if (w < size + 10 && w > size)
        x = (w - size) / 2;
    DrawStateW(dc, nullptr, nullptr, (LPARAM)m_info.icon, 0, x, y, size, size, DST_ICON | DSS_NORMAL);
}

void CGameItemButton::DrawLabel(HDC dc, const RECT& rc, bool disabled)
{
    COLORREF color;
    HFONT font;
    if (disabled)       { color = m_textColors[3]; font = m_fonts[3]; }
    else if (m_pressed) { color = m_textColors[1]; font = m_fonts[1]; }
    else if (m_hover)   { color = m_textColors[2]; font = m_fonts[2]; }
    else                { color = m_textColors[0]; font = m_fonts[0]; }

    SetTextColor(dc, color);
    HGDIOBJ oldFont = SelectObject(dc, font);

    RECT rcDst = rc;
    wchar_t text[262] = {};
    WZStrCopy(text, 260, m_info.title);
    int len = (int)wcsnlen(text, 260);
    if (len >= 260)
        len = 0;

    SIZE ext = {};
    GetTextExtentPoint32W(dc, text, len, &ext);

    int h = rc.bottom - rc.top;
    if (h > ext.cy)
        rcDst.top = (h - ext.cy) / 2;
    if (rc.right - rc.left > 10)
    {
        rcDst.left = 5;
        rcDst.right = rc.right - 5;
        if (m_info.hasIcon)
            rcDst.left = m_info.iconSize + 10;
    }

    ::DrawTextW(dc, m_info.title, -1, &rcDst, DT_VCENTER);
    ::DrawTextW(dc, m_info.detail, -1, &rcDst, DT_VCENTER | DT_RIGHT);
    SelectObject(dc, oldFont);
}

void CGameItemButton::DrawItem(HDC target, UINT itemState)
{
    RECT rc;
    GetClientRect(&rc);
    WZMemDC mem(target, rc.right - rc.left, rc.bottom - rc.top);
    bool disabled = (itemState & ODS_DISABLED) != 0;
    DrawBackground(mem, rc, disabled);
    DrawImage(mem, rc, disabled);
    DrawBorder(mem, rc, disabled);
    DrawItemIcon(mem, rc);
    DrawLabel(mem, rc, disabled);
    BitBlt(target, 0, 0, rc.right - rc.left, rc.bottom - rc.top, mem, 0, 0, SRCCOPY);
}

LRESULT CGameItemButton::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(&ps);
    DrawItem(dc, IsWindowEnabled() ? 0 : ODS_DISABLED);
    EndPaint(&ps);
    return 0;
}

// (reflected BN_CLICKED)
void CGameItemButton::OnClicked()
{
    if (!m_pressed)
    {
        m_pressed = 1;
        InvalidateRect(nullptr, TRUE);
        ::SendMessageW(GetParent(), WZ_ITEM_CLICKED, (WPARAM)m_info.id, 0);
    }
}

LRESULT CGameItemButton::OnMouseMove(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    LRESULT r = DefWindowProc(msg, wp, lp);
    bool hover = m_hover != 0;
    POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
    WZTrackHover(m_hWnd, pt, hover);
    m_hover = hover;
    handled = TRUE;
    return r;
}

LRESULT CGameItemButton::OnMouseLeave(UINT, WPARAM, LPARAM, BOOL& handled)
{
    m_hover = 0;
    InvalidateRect(nullptr, TRUE);
    handled = TRUE;
    return 0;
}

LRESULT CGameItemButton::OnLButtonDown(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    m_down = true;
    handled = TRUE;
    return DefWindowProc(msg, wp, lp);
}

LRESULT CGameItemButton::OnLButtonUp(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
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
        OnClicked();
    return r;
}

LRESULT CGameItemButton::OnBmClick(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    handled = TRUE;
    LRESULT r = DefWindowProc(msg, wp, lp);
    OnClicked();
    return r;
}

LRESULT CGameItemButton::OnKeyUp(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    handled = TRUE;
    LRESULT r = DefWindowProc(msg, wp, lp);
    if (wp == VK_SPACE)
        OnClicked();
    return r;
}
