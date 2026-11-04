// SettingManager.h - loads WZLauncher.wzc (AES encrypted XML) and the command line.
#pragma once

#include "../Common/WZCommon.h"
#include "../Common/XmlParser.h"
#include "CommonInfo.h"
#include "GameUIInfo.h"
#include "UpdateInfo.h"
#include <string>
#include <vector>

class SettingManager
{
public:
    SettingManager();
    virtual ~SettingManager();                              // /

    // decrypt + parse the configuration, then collect the command line.
    WZResult Load();

    CommonInfo*  GetCommonInfo() const { return m_pCommonInfo; }
    CStringInfo* GetStringInfo() const { return m_pStringInfo; }
    UpdateInfo*  GetLauncherUpdateInfo() const { return m_pLauncherUpdate; }
    const std::vector<GameEntry*>& GetGames() const { return m_Games; }
    const wchar_t* GetCommandLineText() const { return m_CommandLine; }
    const std::vector<std::wstring>& GetArguments() const { return m_Args; }

    // "" selects the first game; otherwise exact (case sensitive) GUID match.
    GameEntry* FindGame(const wchar_t* guid) const;
    // argument following "/d" or "/D" (game GUID) copied into out[260]; untouched if absent.
    void GetGameGuidArgument(wchar_t* out) const;
    // argument following "/g" or "/G" (skipping a "-name" token) copied into out[4096].
    void GetGameArguments(wchar_t* out) const;

private:
    void ClearAll();
    WZResult GetDecodeXmlPath(wchar_t* outXmlPath);
    void ParseCommandLine();
    WZResult ParseConfiguration(XmlNode* doc, const wchar_t* exeDir, const wchar_t* exePath);
    WZResult ParseLauncherInfo(XmlNode* launcherUI, const wchar_t* exePath, const wchar_t* exeDir);
    WZResult ParseLauncherUI(XmlNode* launcherUI);
    WZResult ParseDescriptions(XmlNode* node);
    WZResult ParseGames(XmlNode* node);
    WZResult ParseGame(XmlNode* game);
    WZResult ParseGameRegistry(XmlNode* node, UpdateInfo* update);
    WZResult ParseVersionInfo(XmlNode* node, VersionInfo* version, const wchar_t* rootDir, const wchar_t* tempDir);
    WZResult ParseUpdateServer(XmlNode* node, UpdateServerInfo* server);
    WZResult ParsePatcherInfo(XmlNode* node, PatcherInfo* patcher, const wchar_t* launcherDir,
                              const wchar_t* targetDir, const wchar_t* tempDir);
    WZResult ParseMainWindow(XmlNode* node);
    WZResult ParseButtons(XmlNode* node);
    WZResult ParseControls(XmlNode* node);
    WZResult ParseUIInfo(XmlNode* node, GameUIInfo* ui, const wchar_t* guid, const wchar_t* name);

    WZResult        m_LastResult;
    std::vector<std::wstring> m_Args;                                       // argv[1..]
    wchar_t         m_CommandLine[4096];                                    // text from the first '/'
    CommonInfo*     m_pCommonInfo;
    CStringInfo*    m_pStringInfo;
    UpdateInfo*     m_pLauncherUpdate;
    std::vector<GameEntry*> m_Games;
};
