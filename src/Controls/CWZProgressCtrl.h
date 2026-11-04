// CWZProgressCtrl - bitmap/colour filled progress bar.
#pragma once
#include "ControlUtil.h"

class CWZProgressCtrl : public CWindowImpl<CWZProgressCtrl, CWindow>
{
public:
    CWZProgressCtrl();

    // Attach to a dialog item (SubclassDlgItem).
    BOOL SubclassDlgItem(UINT id, HWND parent);

    // Same effect as the dialog's SetPos helpers ( /): store and redraw immediately.
    void SetPos(int pos);

    BEGIN_MSG_MAP(CWZProgressCtrl)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_SETTEXT, OnSetText)
        MESSAGE_HANDLER(WM_GETTEXT, OnGetText)
    END_MSG_MAP()

    int      m_pos;
    int      m_max;
    int      m_min;
    int      m_reserved;
    int      m_hasBitmap;
    HBITMAP  m_bitmap;          // (not owned)
    wchar_t  m_text[260];
    COLORREF m_fillColor;
    COLORREF m_color2;
    COLORREF m_color3;
    COLORREF m_color4;

private:
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSetText(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnGetText(UINT, WPARAM, LPARAM, BOOL&);
    void DrawFill(HDC dc, const RECT& rc);
};
