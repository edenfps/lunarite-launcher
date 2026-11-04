#include "CTransparentStatic.h"

BOOL CTransparentStatic::SubclassDlgItem(UINT id, HWND parent)
{
    HWND h = ::GetDlgItem(parent, id);
    return h && SubclassWindow(h);
}

HBRUSH CTransparentStatic::OnCtlColor(HDC dc)
{
    SetBkMode(dc, TRANSPARENT);
    return (HBRUSH)GetStockObject(NULL_BRUSH);
}

LRESULT CTransparentStatic::OnSetText(UINT msg, WPARAM wp, LPARAM lp, BOOL& handled)
{
    LRESULT r = DefWindowProc(msg, wp, lp);
    RECT rc;
    GetWindowRect(&rc);
    HWND parent = GetParent();
    ::ScreenToClient(parent, (LPPOINT)&rc.left);
    ::ScreenToClient(parent, (LPPOINT)&rc.right);
    ::InvalidateRect(parent, &rc, TRUE);
    ::UpdateWindow(parent);
    handled = TRUE;
    return r;
}

LRESULT CTransparentStatic::OnReflectedCtlColor(UINT, WPARAM wp, LPARAM, BOOL& handled)
{
    handled = TRUE;
    return (LRESULT)OnCtlColor((HDC)wp);
}
