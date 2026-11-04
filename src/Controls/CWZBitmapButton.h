// CWZBitmapButton - owner-drawn bitmap button with 4-frame strips (normal, pressed, hover, disabled).
#pragma once
#include "ControlUtil.h"

class CWZBitmapButton : public CWindowImpl<CWZBitmapButton, CWindow>
{
public:
    CWZBitmapButton();                      // zero-initialised ( region)
    ~CWZBitmapButton();

    // Subclasses the dialog item and makes it BS_OWNERDRAW.
    BOOL SubclassDlgItem(UINT id, HWND parent);

    // single strip. Takes ownership of the bitmap.
    void SetBitmap(HBITMAP strip);
    // toggle button. 'on' is drawn when toggled, 'off' while not toggled (state starts off).
    void SetToggleBitmaps(HBITMAP on, HBITMAP off);

    bool IsToggled() const { return m_toggled != 0; }
    void Toggle();

    // (DrawItem). itemState is ODS_*; only ODS_DISABLED matters.
    void DrawItem(HDC target, UINT itemState);

    BEGIN_MSG_MAP(CWZBitmapButton)
        MESSAGE_HANDLER(WM_PAINT, OnPaint)
        MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
        MESSAGE_HANDLER(WM_MOUSELEAVE, OnMouseLeave)
        MESSAGE_HANDLER(WM_LBUTTONDOWN, OnLButtonDown)
        MESSAGE_HANDLER(WM_LBUTTONUP, OnLButtonUp)
        MESSAGE_HANDLER(BM_CLICK, OnBmClick)
        MESSAGE_HANDLER(WM_KEYUP, OnKeyUp)
    END_MSG_MAP()

    // Public so the dialog can read them.
    HIMAGELIST m_himl1;
    HIMAGELIST m_himl2;
    int        m_ownsLists;
    int        m_pressed;
    int        m_hover;
    int        m_toggleMode;
    int        m_toggled;

private:
    LRESULT OnPaint(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseMove(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnMouseLeave(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonDown(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnLButtonUp(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnBmClick(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnKeyUp(UINT, WPARAM, LPARAM, BOOL&);

    void ReleaseLists();
    void DrawFrames(HDC dc, const RECT& rc, bool disabled);

    bool m_down;            // mouse went down on this control (stands in for the BN_CLICKED reflection)
};
