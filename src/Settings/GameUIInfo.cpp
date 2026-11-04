#include "GameUIInfo.h"
#include <cstddef>
#include <cstring>

GameUIInfo::GameUIInfo()
{
    memset(&ListIndex, 0, sizeof(GameUIInfo) - offsetof(GameUIInfo, ListIndex));
}

GameUIInfo::~GameUIInfo() {}

void GameUIInfo::SetGuid(const wchar_t* s) { WZStrCopy(Guid, 260, s); }
void GameUIInfo::SetName(const wchar_t* s) { WZStrCopy(Name, 260, s); }
void GameUIInfo::SetWebURL(const wchar_t* s) { WZStrCopy(WebURL, 2084, s); }
void GameUIInfo::SetInstallURL(const wchar_t* s) { WZStrCopy(InstallURL, 2084, s); }
void GameUIInfo::SetGameOptionDll(const wchar_t* s) { WZStrCopy(GameOptionDll, 260, s); }
