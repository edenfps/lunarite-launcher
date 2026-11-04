// CGameListControl - vertical stack of CGameItemButton rows.
#pragma once
#include "ControlUtil.h"
#include "CGameItemButton.h"
#include <vector>

class CGameListControl : public CWindowImpl<CGameListControl, CWindow>
{
public:
    CGameListControl();
    ~CGameListControl();

    BOOL SubclassDlgItem(UINT id, HWND parent);

    // creates one row for 'info' (the row consumes the descriptor's bitmap and icon).
    // Returns FALSE when a row with the very same id buffer already exists, or on creation failure.
    BOOL AddItem(const GameItemInfo& info);

    int GetItemCount() const { return (int)m_items.size(); }
    CGameItemButton* GetItem(int i) const { return (i >= 0 && i < (int)m_items.size()) ? m_items[i] : nullptr; }
    CGameItemButton* FindItem(const wchar_t* idBuffer) const;   // (compares buffer addresses)

    // (DrawItem)
    void DrawItem(HDC target, UINT itemState);

    BEGIN_MSG_MAP(CGameListControl)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WZ_ITEM_CLICKED, OnItemClicked)
    END_MSG_MAP()

    std::vector<CGameItemButton*> m_items;
    int      m_itemHeight;
    int      m_drawFlag;
    HBITMAP  m_bgBitmap;
    int      m_fillBackground;
    COLORREF m_bgColor;
    int      m_drawFrame;
    COLORREF m_frameColor;
    int      m_useItemColors;
    COLORREF m_itemBg[4];       // (normal, pressed, hover, disabled)
    COLORREF m_itemBorder[4];
    COLORREF m_itemText[4];
    HFONT    m_itemFont;        // (base for the row fonts; DEFAULT_GUI_FONT when null)
    int      m_sticky;

private:
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnItemClicked(UINT, WPARAM, LPARAM, BOOL&);
    void DrawBackgroundBitmap(HDC dc, const RECT& rc);
};
