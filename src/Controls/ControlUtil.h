// ControlUtil.h - shared GDI helpers for the owner-drawn controls.
#pragma once
#include "Common/WZCommon.h"
#include <atlbase.h>
#include <atlwin.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

// Custom notification posted from CGameItemButton to its list and from the list to the dialog.
// 0x9BB4 == 0x8000 + 7092; 7092 is also the child-control id used for item buttons.
const UINT WZ_ITEM_CLICKED   = 0x9BB4;
const UINT WZ_ITEM_CONTROLID = 7092;

// width/height of a bitmap (0,0 for NULL).
inline SIZE WZGetBitmapSize(HBITMAP bmp)
{
    SIZE sz = { 0, 0 };
    if (bmp)
    {
        BITMAP bm = {};
        GetObjectW(bmp, sizeof(bm), &bm);
        sz.cx = bm.bmWidth;
        sz.cy = bm.bmHeight;
    }
    return sz;
}

// build a 4-frame image list from a horizontal strip (normal, pressed, hover, disabled).
// Frame width = width / 4, 32bpp, grow 4. The bitmap is consumed (deleted).
inline HIMAGELIST WZCreateFrameImageList(HBITMAP bmp)
{
    if (!bmp)
        return nullptr;
    SIZE sz = WZGetBitmapSize(bmp);
    HIMAGELIST himl = ImageList_Create(sz.cx / 4, sz.cy, ILC_COLOR32, 4, 4);
    if (himl)
        ImageList_AddMasked(himl, bmp, 0);
    DeleteObject(bmp);
    return himl;
}

// Off-screen buffer used by every control's paint path: compatible DC + client-sized bitmap.
class WZMemDC
{
public:
    WZMemDC(HDC target, int w, int h) : m_dc(CreateCompatibleDC(target)), m_bmp(CreateCompatibleBitmap(target, w, h))
    {
        SetBkMode(m_dc, TRANSPARENT);
        m_old = SelectObject(m_dc, m_bmp);
    }
    ~WZMemDC()
    {
        SelectObject(m_dc, m_old);
        DeleteObject(m_bmp);
        DeleteDC(m_dc);
    }
    operator HDC() const { return m_dc; }
private:
    HDC     m_dc;
    HBITMAP m_bmp;
    HGDIOBJ m_old;
};

inline void WZFillRect(HDC dc, const RECT& rc, COLORREF color)
{
    HBRUSH br = CreateSolidBrush(color);
    FillRect(dc, &rc, br);
    DeleteObject(br);
}

inline void WZFrameRect(HDC dc, const RECT& rc, COLORREF color)
{
    HBRUSH br = CreateSolidBrush(color);
    FrameRect(dc, &rc, br);
    DeleteObject(br);
}

// Hover tracking shared by CWZBitmapButton and CGameItemButton:
// set 'hover' only while the mouse is really over this window; ask for WM_MOUSELEAVE.
inline void WZTrackHover(HWND hwnd, POINT pt, bool& hover)
{
    ClientToScreen(hwnd, &pt);
    if (WindowFromPoint(pt) == hwnd)
    {
        if (!hover)
        {
            hover = true;
            InvalidateRect(hwnd, nullptr, TRUE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
    }
    else
    {
        hover = false;
        InvalidateRect(hwnd, nullptr, TRUE);
    }
}
