// GameUIInfo.h - per-game list/UI info (<Game><UIInfo>) and CStringInfo (<Descriptions>).
#pragma once

#include "../Common/WZCommon.h"
#include <string>
#include <vector>

// ctor, dtor. 14080 bytes on x86.
class GameUIInfo
{
public:
    GameUIInfo();
    virtual ~GameUIInfo();

    void SetGuid(const wchar_t* s);   // max 260 chars
    void SetName(const wchar_t* s);   // max 260 chars
    void SetWebURL(const wchar_t* s);   // max 2084 chars
    void SetInstallURL(const wchar_t* s);   // max 2084 chars
    void SetGameOptionDll(const wchar_t* s);   // max 260 chars

    int     ListIndex;
    wchar_t Guid[260];
    wchar_t Name[260];
    wchar_t WebURL[2084];
    wchar_t InstallURL[2084];
    wchar_t Reserved[2084];                     // (never written)
    HICON   hListIcon;                          // (16x16 icon, LoadImage)
    HBITMAP hListImage;
    wchar_t GameOptionDll[260];

    GameUIInfo(const GameUIInfo&) = delete;
    GameUIInfo& operator=(const GameUIInfo&) = delete;
};

#ifdef _M_IX86
#include <cstddef>
static_assert(offsetof(GameUIInfo, Guid) == 8, "layout");
static_assert(offsetof(GameUIInfo, WebURL) == 1048, "layout");
static_assert(offsetof(GameUIInfo, InstallURL) == 5216, "layout");
static_assert(offsetof(GameUIInfo, hListIcon) == 13552, "layout");
static_assert(offsetof(GameUIInfo, GameOptionDll) == 13560, "layout");
static_assert(sizeof(GameUIInfo) == 14080, "layout");
#endif

// Indices of the <Descriptions> strings (push order in).
enum DescriptionId
{
    DESC_UPDATE_REQUIRED = 0,
    DESC_UPDATE_COMPLETED,
    DESC_UNKNOWN_VERSION,
    DESC_DOWNLOADING_FILES,
    DESC_ERROR_DOWNLOADING_FILES,
    DESC_PATCHING_FILES,
    DESC_ERROR_PATCHING_FILES,
    DESC_CHECKING_FILES,
    DESC_ERROR_CHECKING_FILES,
    DESC_INSTALL_REQUIRED,
    DESC_ERROR_NETWORK,
    DESC_RUN_ON_THE_HOMEPAGE,
};

// List of strings.
class CStringInfo
{
public:
    CStringInfo() = default;
    virtual ~CStringInfo() = default;

    void Add(const std::wstring& s) { m_Strings.push_back(s); }
    // out-of-range index returns L"".
    const wchar_t* GetString(size_t index) const
    {
        return index < m_Strings.size() ? m_Strings[index].c_str() : L"";
    }
    size_t Count() const { return m_Strings.size(); }

private:
    std::vector<std::wstring> m_Strings;
};

// The two objects that make up one <Game>.
class UpdateInfo;
struct GameEntry
{
    GameUIInfo* ui;
    UpdateInfo* update;
    // <RemoveFiles> of the game's VersionAndRegistryInfo, paths relative
    // to the game root separated by ';', deleted before the game starts.
    std::wstring removeFiles;
};
