#include "CommonInfo.h"
#include <cstddef>
#include <cstring>

namespace
{
    SIZE BitmapSize(HBITMAP h)
    {
        SIZE s = {0, 0};
        BITMAP bm;
        if (h && GetObjectW(h, sizeof(bm), &bm))
        {
            s.cx = bm.bmWidth;
            s.cy = bm.bmHeight;
        }
        return s;
    }

    HBITMAP LoadBmp(const wchar_t* path)
    {
        return (path && *path) ? (HBITMAP)LoadImageW(nullptr, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE) : nullptr;
    }

    HFONT MakeFont(const LOGFONTW& base, const wchar_t* face, LONG weight)
    {
        LOGFONTW lf = base;
        WZStrCopy(lf.lfFaceName, 32, face);
        lf.lfWeight = weight;
        return CreateFontIndirectW(&lf);
    }

    void SetStripRect(RECT* rc, HBITMAP bmp, int x, int y)
    {
        SIZE s = BitmapSize(bmp);
        SetRect(rc, x, y, x + s.cx / 4, y + s.cy);
    }
}

CommonInfo::CommonInfo()
{
    memset(&hBackground, 0, sizeof(CommonInfo) - offsetof(CommonInfo, hBackground));
    GameListLineHeight = 30;
}

CommonInfo::~CommonInfo()
{
    if (hTitleFont)
        DeleteObject(hTitleFont);
    if (hStaticFont)
        DeleteObject(hStaticFont);
    if (hBackground)
        DeleteObject(hBackground);
    if (hIcon)
        DestroyIcon(hIcon);
}

void CommonInfo::LoadBackground(const wchar_t* path)
{
    hBackground = LoadBmp(path);
    SIZE s = BitmapSize(hBackground);
    BackgroundWidth = s.cx;
    BackgroundHeight = s.cy;
}

void CommonInfo::CreateFonts(const wchar_t* titleText, const wchar_t* titleFace, int titleColor,
                             const wchar_t* staticFace, int staticColor)
{
    WZStrCopy(Title, 260, titleText);
    TitleColor = titleColor;
    StaticColor = staticColor;

    LOGFONTW lf = {};
    GetObjectW(GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);
    hTitleFont = MakeFont(lf, titleFace, 600);
    hStaticFont = MakeFont(lf, staticFace, 400);
}

void CommonInfo::SetMinimizeButton(const wchar_t* path, int x, int y)
{
    hMinimizeButton = LoadBmp(path);
    SetStripRect(&MinimizeRect, hMinimizeButton, x, y);
}

void CommonInfo::SetExitButton(const wchar_t* path, int x, int y)
{
    hExitButton = LoadBmp(path);
    SetStripRect(&ExitRect, hExitButton, x, y);
}

void CommonInfo::SetConfigurationButton(const wchar_t* path, int x, int y)
{
    hConfigurationButton = LoadBmp(path);
    SetStripRect(&ConfigurationRect, hConfigurationButton, x, y);
}

void CommonInfo::SetCheckBox(const wchar_t* checkedPath, const wchar_t* uncheckedPath, int x, int y)
{
    hCheckBoxChecked = LoadBmp(checkedPath);
    hCheckBoxUnchecked = LoadBmp(uncheckedPath);
    SetStripRect(&CheckBoxRect, hCheckBoxChecked, x, y);
}

void CommonInfo::SetActionButtons(const wchar_t* updatePath, const wchar_t* executePath,
                                  const wchar_t* stopPath, const wchar_t* installPath, int x, int y)
{
    hUpdateButton = LoadBmp(updatePath);
    hExecuteButton = LoadBmp(executePath);
    hStopButton = LoadBmp(stopPath);
    hInstallButton = LoadBmp(installPath);

    // Size reference.
    HBITMAP ref = hInstallButton;
    if (hUpdateButton == nullptr)
    {
        if (hExecuteButton)
            ref = hExecuteButton;
        else if (hStopButton)
            ref = hStopButton;
    }
    else
    {
        ref = hExecuteButton;
    }
    SetStripRect(&ActionButtonRect, ref, x, y);
}
