// CommonInfo.h - launcher window/UI settings read from <LauncherUI>.
#pragma once

#include "../Common/WZCommon.h"

class CommonInfo
{
public:
    CommonInfo();
    virtual ~CommonInfo();                                              // /

    // load the background bitmap and remember its size.
    void LoadBackground(const wchar_t* path);
    // title text, title/static colours and fonts (title bold 600, static 400).
    void CreateFonts(const wchar_t* titleText, const wchar_t* titleFace, int titleColor,
                     const wchar_t* staticFace, int staticColor);
    // / /: 4-state strip bitmap + rect (width/4 x height).
    void SetMinimizeButton(const wchar_t* path, int x, int y);
    void SetExitButton(const wchar_t* path, int x, int y);
    void SetConfigurationButton(const wchar_t* path, int x, int y);
    // check box (checked / unchecked bitmaps).
    void SetCheckBox(const wchar_t* checkedPath, const wchar_t* uncheckedPath, int x, int y);
    // Update/Execute/Stop/Install bitmaps share one rect.
    void SetActionButtons(const wchar_t* updatePath, const wchar_t* executePath,
                          const wchar_t* stopPath, const wchar_t* installPath, int x, int y);

    HBITMAP hBackground;
    int     BackgroundMaskEnabled;
    int     BackgroundMaskColor;
    int     BackgroundWidth;
    int     BackgroundHeight;
    HICON   hIcon;
    wchar_t Title[260];
    HFONT   hTitleFont;
    int     TitleColor;
    int     GameListEnabled;
    RECT    GameListRect;
    int     GameListLineHeight;         // (default 30)
    RECT    WebRect;
    HFONT   hStaticFont;
    int     StaticColor;
    RECT    TitleStaticRect;
    RECT    TotalStaticRect;
    RECT    FileStaticRect;
    HBITMAP hMinimizeButton;
    RECT    MinimizeRect;
    HBITMAP hExitButton;
    RECT    ExitRect;
    HBITMAP hConfigurationButton;
    RECT    ConfigurationRect;
    HBITMAP hCheckBoxChecked;
    HBITMAP hCheckBoxUnchecked;
    RECT    CheckBoxRect;
    HBITMAP hUpdateButton;
    HBITMAP hExecuteButton;
    HBITMAP hStopButton;
    HBITMAP hInstallButton;
    RECT    ActionButtonRect;
    HBITMAP hTotalProgressBar;
    RECT    TotalProgressRect;
    HBITMAP hFileProgressBar;
    RECT    FileProgressRect;
    int     UsePurpleLauncher;
    HWND    hPurpleWnd;

    CommonInfo(const CommonInfo&) = delete;
    CommonInfo& operator=(const CommonInfo&) = delete;
};

#ifdef _M_IX86
#include <cstddef>
static_assert(offsetof(CommonInfo, hBackground) == 4, "layout");
static_assert(offsetof(CommonInfo, Title) == 28, "layout");
static_assert(offsetof(CommonInfo, hTitleFont) == 548, "layout");
static_assert(offsetof(CommonInfo, GameListLineHeight) == 576, "layout");
static_assert(offsetof(CommonInfo, hStaticFont) == 596, "layout");
static_assert(offsetof(CommonInfo, hMinimizeButton) == 652, "layout");
static_assert(offsetof(CommonInfo, hUpdateButton) == 736, "layout");
static_assert(offsetof(CommonInfo, hFileProgressBar) == 788, "layout");
static_assert(offsetof(CommonInfo, UsePurpleLauncher) == 808, "layout");
static_assert(sizeof(CommonInfo) == 816, "layout");
#endif
