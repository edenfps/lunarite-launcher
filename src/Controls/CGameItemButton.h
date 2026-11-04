// CGameItemButton - one owner-drawn row of CGameListControl.
#pragma once
#include "ControlUtil.h"
#include "GameItemInfo.h"

class CGameItemButton : public CWindowImpl<CGameItemButton, CWindow>
{
public:
    explicit CGameItemButton(const GameItemInfo& info);
    ~CGameItemButton();

    // CButton::Create(id, WS_CHILD|WS_VISIBLE, rect, parent, 7092) + owner-draw.
    BOOL CreateItem(HWND parent, const RECT& rc);

    void SetStateColors(const COLORREF bg[4], const COLORREF border[4], int borderFlag);
    // text colours + the four fonts derived from 'baseFont' (400 weight; pressed = 600).
    void SetTextStyle(HFONT baseFont, const COLORREF text[4]);

    const wchar_t* Id() const { return m_info.id; }
    void SetStateText(const wchar_t* s) { m_info.SetDetail(s); }   // writes the detail text

    void DrawItem(HDC target, UINT itemState);

    BEGIN_MSG_MAP(CGameItemButton)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(BM_CLICK, OnBmClick)
        MESSAGE_HANDLER(WM_KEYUP, OnKeyUp)
    END_MSG_MAP()

    GameItemInfo m_info;
    int      m_pressed;
    int      m_hover;
    int      m_useColors;
    int      m_useBorder;
    COLORREF m_bgColors[4];
    COLORREF m_borderColors[4];
    COLORREF m_textColors[4];
    HFONT    m_fonts[4];        // (normal, pressed, hover, disabled)
    HIMAGELIST m_himl;

private:
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnBmClick(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnKeyUp(UINT, WPARAM, LPARAM, BOOL&);

    void OnClicked();
    void DrawBackground(HDC dc, const RECT& rc, bool disabled);
    void DrawImage(HDC dc, const RECT& rc, bool disabled);
    void DrawBorder(HDC dc, const RECT& rc, bool disabled);
    void DrawItemIcon(HDC dc, const RECT& rc);
    void DrawLabel(HDC dc, const RECT& rc, bool disabled);

    bool m_down;
};
