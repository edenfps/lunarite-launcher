// CTransparentStatic - static text whose background is the parent's.
#pragma once
#include "ControlUtil.h"

class CTransparentStatic : public CWindowImpl<CTransparentStatic, CWindow>
{
public:
    BOOL SubclassDlgItem(UINT id, HWND parent);

    // transparent background mode + NULL brush. Call from the parent's WM_CTLCOLORSTATIC
    // (or rely on REFLECT_NOTIFICATIONS in the parent, which arrives as OCM_CTLCOLORSTATIC).
    HBRUSH OnCtlColor(HDC dc);

    BOOL GetWindowRectOf(LPRECT rc) { return GetWindowRect(rc); }

    BEGIN_MSG_MAP(CTransparentStatic)
        MESSAGE_HANDLER(WM_SETTEXT, OnSetText)
        MESSAGE_HANDLER(OCM_CTLCOLORSTATIC, OnReflectedCtlColor)
    END_MSG_MAP()

private:
    LRESULT OnSetText(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnReflectedCtlColor(UINT, WPARAM, LPARAM, BOOL&);
};
