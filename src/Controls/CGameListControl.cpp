#include "CGameListControl.h"
#include <wchar.h>

CGameListControl::CGameListControl()
    : m_itemHeight(20), m_drawFlag(0), m_bgBitmap(nullptr), m_fillBackground(0),
      m_bgColor(GetSysColor(COLOR_BTNFACE)), m_drawFrame(0), m_frameColor(GetSysColor(COLOR_ACTIVEBORDER)),
      m_useItemColors(0), m_itemFont(nullptr), m_sticky(1)
{
    for (int i = 0; i < 4; ++i)
    {
        m_itemBg[i]     = GetSysColor(COLOR_BTNFACE);
        m_itemBorder[i] = GetSysColor(COLOR_ACTIVEBORDER);
        m_itemText[i]   = GetSysColor(COLOR_BTNTEXT);
    }
}

CGameListControl::~CGameListControl()
{
    for (CGameItemButton* item : m_items)
    {
        if (item)
        {
            if (item->IsWindow())
                item->DestroyWindow();
            delete item;
        }
    }
    m_items.clear();
}

BOOL CGameListControl::SubclassDlgItem(UINT id, HWND parent)
{
    HWND h = ::GetDlgItem(parent, id);
    return h && SubclassWindow(h);
}

CGameItemButton* CGameListControl::FindItem(const wchar_t* idBuffer) const
{
    for (CGameItemButton* item : m_items)
        if (item && item->Id() == idBuffer)
            return item;
    return nullptr;
}

BOOL CGameListControl::AddItem(const GameItemInfo& info)
{
    if (FindItem(info.id))
        return FALSE;

    RECT client;
    GetClientRect(&client);
    int index = (int)m_items.size();

    CGameItemButton* item = new CGameItemButton(info);

    if (m_itemHeight < 5)
        m_itemHeight = 20;
    RECT rc;
    rc.left = client.left;
    rc.top = index * m_itemHeight;
    rc.right = client.right;
    rc.bottom = m_itemHeight * (index + 1);

    if (item->CreateItem(m_hWnd, rc))
        item->ShowWindow(SW_SHOW);

    if (m_useItemColors)
        item->SetStateColors(m_itemBg, m_itemBorder, m_drawFrame);
    item->SetTextStyle(m_itemFont ? m_itemFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT), m_itemText);

    m_items.push_back(item);
    return TRUE;
}

void CGameListControl::DrawBackgroundBitmap(HDC dc, const RECT& rc)
{
    if (m_fillBackground && m_bgBitmap)
    {
        HDC src = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(src, m_bgBitmap);
        BitBlt(dc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, src, 0, 0, SRCCOPY);
        SelectObject(src, old);
        DeleteDC(src);
    }
}

void CGameListControl::DrawItem(HDC target, UINT)
{
    if (!m_drawFlag && !m_fillBackground)
        return;

    RECT rc;
    GetClientRect(&rc);
    WZMemDC mem(target, rc.right - rc.left, rc.bottom - rc.top);
    if (m_fillBackground)
        WZFillRect(mem, rc, m_bgColor);
    DrawBackgroundBitmap(mem, rc);
    if (m_drawFrame)
        WZFrameRect(mem, rc, m_frameColor);
    BitBlt(target, 0, 0, rc.right - rc.left, rc.bottom - rc.top, mem, 0, 0, SRCCOPY);
}

LRESULT CGameListControl::OnPaint(UINT, WPARAM, LPARAM, BOOL&)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(&ps);
    DrawItem(dc, IsWindowEnabled() ? 0 : ODS_DISABLED);
    EndPaint(&ps);
    return 0;
}

// a row was clicked (wParam = its id buffer).
LRESULT CGameListControl::OnItemClicked(UINT, WPARAM wp, LPARAM, BOOL& handled)
{
    handled = TRUE;
    if (!IsWindowEnabled())
        return 0;

    const wchar_t* id = (const wchar_t*)wp;
    if (!m_sticky)
    {
        CGameItemButton* item = FindItem(id);
        if (item)
        {
            item->m_pressed = 0;
            item->InvalidateRect(nullptr, TRUE);
        }
        ::SendMessageW(GetParent(), WZ_ITEM_CLICKED, wp, 0);
        return 0;
    }

    for (CGameItemButton* item : m_items)
    {
        if (wcscmp(id, item->Id()) != 0)
        {
            item->m_pressed = 0;
            item->InvalidateRect(nullptr, TRUE);
        }
    }
    ::SendMessageW(GetParent(), WZ_ITEM_CLICKED, wp, 0);
    return 0;
}
