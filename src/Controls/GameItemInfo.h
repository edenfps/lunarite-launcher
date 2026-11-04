// GameItemInfo - descriptor passed by value to CGameListControl::AddItem and copied into CGameItemButton.
#pragma once
#include "Common/WZCommon.h"

struct GameItemInfo
{
    int      iconSize;
    int      hasIcon;
    wchar_t  id[260];
    wchar_t  title[260];
    wchar_t  detail[260];
    HICON    icon;
    HBITMAP  bitmap;

    GameItemInfo()
        : iconSize(16), hasIcon(0), icon(nullptr), bitmap(nullptr)
    {
        for (int i = 0; i < 260; ++i)
            id[i] = title[i] = detail[i] = 0;
    }

    void SetId(const wchar_t* s)        { WZStrCopy(id, 260, s); }
    void SetTitle(const wchar_t* s)     { WZStrCopy(title, 260, s); }       // target
    void SetDetail(const wchar_t* s)    { WZStrCopy(detail, 260, s); }

    // A by-value copy must not destroy the icon and bitmap when it goes out of scope;
    // call this on the caller's instance once AddItem has consumed it.
    void Release()
    {
        if (icon)   { DestroyIcon(icon); icon = nullptr; }
        if (bitmap) { DeleteObject(bitmap); bitmap = nullptr; }
    }
};
static_assert(sizeof(GameItemInfo) == 0x628, "GameItemInfo must stay 0x628 bytes");
