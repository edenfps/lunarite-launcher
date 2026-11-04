// SettingManager.cpp - SettingManager.
#include "SettingManager.h"
#include "SettingsUtil.h"
#include "../Common/Registry.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <shlwapi.h>

using namespace settings_detail;

namespace
{
    // WZResult codes used by the configuration reader.
    enum : DWORD
    {
        ERR_XML_OPEN        = 0x80000002,   // "An error occurred reading configuration file." (XML Open Fail)
        ERR_LOAD_LIBRARY    = 0x80000003,   // AesCrypt.dll LoadLibrary / GetProcAddress
        ERR_DECRYPT         = 0x80000004,
        ERR_NO_GAMES        = 0x80000005,   // missing root or no valid <Game>
        ERR_NO_UPDATE_INFO  = 0x80000006,
        ERR_NO_NODE         = 0x80000007,   // MainWindow / Buttons / Controls missing or empty
        ERR_NO_LAUNCHERUI   = 0x80000008,
        ERR_BAD_GAME        = 0x80000009,   // missing/duplicate GUID
        ERR_NO_UIINFO       = 0x8000000A,
        ERR_NO_VERSIONINFO  = 0x8000000B,
        ERR_NO_SERVERINFO   = 0x8000000C,
    };

    const wchar_t kConfigError[] = L"An error occurred reading configuration file.";

    struct DescriptionDef
    {
        const wchar_t* Tag;
        const wchar_t* Default;
    };

    // Order = DescriptionId.
    const DescriptionDef kDescriptions[] =
    {
        { L"UpdateRequired",        L"Update Required." },
        { L"UpdateCompleted",       L"Update has completed successfully." },
        { L"UnknownVersion",        L"Unable to validate game version." },
        { L"DownloadingFiles",      L"Downloading Files..." },
        { L"ErrorDownloadingFiles", L"An error occurred downloading files." },
        { L"PatchingFiles",         L"Patching Files..." },
        { L"ErrorPatchingFiles",    L"An error occurred applying patch to files." },
        { L"CheckingFiles",         L"Checking Files..." },
        { L"ErrorCheckingFiles",    L"An error occurred checking files." },
        { L"InstallRequired",       L"Required to install the game." },
        { L"ErrorNetwork",          L"A network error occurred." },
        { L"RunOnTheHomePage",      L"Please launch the game from our website." },
    };

    WZResult Fail(DWORD code)
    {
        return WZResult(code, 0, L"%s", kConfigError);
    }

    // Reads the text of <name> under 'parent' into out[260]; returns whether the element exists.
    bool ReadText(const XmlNode* parent, const wchar_t* name, wchar_t* out, size_t cch = 260)
    {
        const wchar_t* t = ChildText(parent, name);
        if (!t)
            return false;
        WZStrCopy(out, cch, t);
        return true;
    }

    // Reads <Position x= y=> and <Size width= height=> style attribute pairs.
    void ReadPosition(const XmlNode* ctl, int* x, int* y)
    {
        const XmlNode* p = FindChild(ctl, L"Position");
        if (p)
        {
            *x = _wtoi(Attr(p, L"x"));
            *y = _wtoi(Attr(p, L"y"));
        }
    }

    void ReadSize(const XmlNode* ctl, int* w, int* h)
    {
        const XmlNode* s = FindChild(ctl, L"Size");
        if (s)
        {
            *w = _wtoi(Attr(s, L"width"));
            *h = _wtoi(Attr(s, L"height"));
        }
    }

    // <BackgroundImage*> text turned into <dir><relative path>; empty result when the element is absent.
    void ReadImagePath(const XmlNode* ctl, const wchar_t* tag, const wchar_t* dir, wchar_t* out)
    {
        out[0] = 0;
        const wchar_t* t = ChildText(ctl, tag);
        if (t)
        {
            wchar_t rel[260] = {};
            WZStrCopy(rel, 260, t);
            BuildImagePath(out, dir, rel);
        }
    }

    HBITMAP LoadBmpFile(const wchar_t* path)
    {
        return path[0] ? (HBITMAP)LoadImageW(nullptr, path, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE) : nullptr;
    }
}

SettingManager::SettingManager()
    : m_pCommonInfo(nullptr), m_pStringInfo(nullptr), m_pLauncherUpdate(nullptr)
{
    memset(m_CommandLine, 0, sizeof(m_CommandLine));
}

SettingManager::~SettingManager()
{
    ClearAll();
}

void SettingManager::ClearAll()
{
    delete m_pCommonInfo;
    m_pCommonInfo = nullptr;
    delete m_pLauncherUpdate;
    m_pLauncherUpdate = nullptr;
    delete m_pStringInfo;
    m_pStringInfo = nullptr;

    for (GameEntry* e : m_Games)
    {
        if (!e)
            continue;
        if (e->update)
        {
            const wchar_t* temp = e->update->Path->DownloadTempDir;
            if (!IsBadStringPtrW(temp, 260) && PathFileExistsW(temp))
                DeleteDirectoryTree(temp);
        }
        delete e->ui;
        delete e->update;
        delete e;
    }
    m_Games.clear();
    m_Args.clear();
}

WZResult SettingManager::Load()
{
    ClearAll();

    m_pLauncherUpdate = new UpdateInfo;
    m_pCommonInfo = new CommonInfo;
    m_pStringInfo = new CStringInfo;

    wchar_t exeDir[260] = {};
    wchar_t exePath[260] = {};
    wchar_t xmlPath[260] = {};

    GetModuleFileNameW(nullptr, exeDir, 260);
    if (wchar_t* slash = wcsrchr(exeDir, L'\\'))
        *slash = 0;
    TrimPath(exeDir);
    GetModuleFileNameW(nullptr, exePath, 260);

    WZResult decrypt = GetDecodeXmlPath(xmlPath);
    if (!decrypt.IsSuccess())
    {
        OutputDebugStringW(xmlPath);
        OutputDebugStringW(L"GetDecodeXmlPath() Fail");
        OutputDebugStringW(decrypt.message);
        return decrypt;
    }

    XmlDocument doc;
    std::wstring text;
    if (!XmlLoadTextFile(xmlPath, text))
    {
        OutputDebugStringW(xmlPath);
        OutputDebugStringW(L"XML Open Fail");
        DeleteFileW(xmlPath);                       // temp XML removed on the failure path ...
        return WZResult(ERR_XML_OPEN, 0, L"%s", kConfigError);
    }

    doc.Parse(text.c_str());
    DeleteFileW(xmlPath);                           // ... and right after reading it on success

    WZResult result = ParseConfiguration(&doc, exeDir, exePath);
    ParseCommandLine();

    if (m_pLauncherUpdate)
    {
        const wchar_t* temp = m_pLauncherUpdate->Path->DownloadTempDir;
        if (!IsBadStringPtrW(temp, 260) && PathFileExistsW(temp))
            DeleteDirectoryTree(temp);
    }
    return result;
}

WZResult SettingManager::GetDecodeXmlPath(wchar_t* outXmlPath)
{
    wchar_t dir[260] = {};
    wchar_t wzcPath[260] = {};
    wchar_t dllPath[260] = {};

    GetModuleFileNameW(nullptr, dir, 260);
    if (wchar_t* slash = wcsrchr(dir, L'\\'))
        *slash = 0;

    wchar_t trimmed[260] = {};
    WZStrCopy(trimmed, 260, dir);
    TrimPath(trimmed);
    // A Korean Windows reads the Korean config when the bundle has one.
    WZStrPrintf(wzcPath, 260, L"%s\\%s", trimmed, L"WZLauncher.kor.wzc");
    if (!WZUserIsKorean() || GetFileAttributesW(wzcPath) == INVALID_FILE_ATTRIBUTES)
        WZStrPrintf(wzcPath, 260, L"%s\\%s", trimmed, L"WZLauncher.wzc");
    WZStrPrintf(outXmlPath, 260, L"%s\\%s", dir, L"WZLauncherTmp.xml");
    WZStrPrintf(dllPath, 260, L"%s\\%s", dir, L"AesCrypt.dll");
    OutputDebugStringW(wzcPath);
    OutputDebugStringW(outXmlPath);
    OutputDebugStringW(dllPath);

    HMODULE lib = LoadLibraryW(dllPath);
    if (!lib)
        return WZResult(ERR_LOAD_LIBRARY, 0,
                        L"[SettingManager::GetDecodeXmlPath] Fail : LoadLibrary Crypt Dll, Error Code : %d",
                        (int)GetLastError());

    typedef int (__cdecl *DecryptAESFn)(const wchar_t*, const wchar_t*);
    DecryptAESFn decrypt = (DecryptAESFn)GetProcAddress(lib, "DecryptAES");
    if (!decrypt)
    {
        FreeLibrary(lib);
        return WZResult(ERR_LOAD_LIBRARY, 0, L"[SettingManager::GetDecodeXmlPath] Fail : GetProcAddress");
    }

    int ok = decrypt(wzcPath, outXmlPath);
    FreeLibrary(lib);
    if (!ok)
        return WZResult(ERR_DECRYPT, 0, L"[SettingManager::GetDecodeXmlPath] Fail : Decrypt");
    return WZResult();
}

void SettingManager::ParseCommandLine()
{
    // There is no web login to pass a /g ticket, so a plain start
    // (shortcut or double-click) runs the game with the language switch for the Windows language.
    if (__argc < 2)
    {
        const wchar_t* language = WZUserIsKorean() ? L"-LANGUAGE=KOR" : L"-LANGUAGE=INT";
        WZStrPrintf(m_CommandLine, 260, L"/g %s", language);
        m_Args = { L"/g", language };
        return;
    }

    wchar_t cmd[260] = {};
    WZStrCopy(cmd, 260, GetCommandLineW());

    if (m_pCommonInfo->UsePurpleLauncher == 1)
    {
        // Purple launcher: "<exe> <hwnd>"
        wchar_t module[260] = {};
        GetModuleFileNameW(GetModuleHandleW(nullptr), module, 260);
        HWND wnd = nullptr;
        size_t skip = wcslen(module) + 2;
        if (skip <= wcslen(cmd))
        {
            wchar_t* ctx = nullptr;
            if (wchar_t* tok = wcstok_s(cmd + skip, L" ", &ctx))
                wnd = (HWND)(LONG_PTR)_wtol(tok);
        }
        m_pCommonInfo->hPurpleWnd = IsWindow(wnd) ? wnd : nullptr;
    }
    else
    {
        if (wchar_t* slash = wcschr(cmd, L'/'))
            WZStrCopy(m_CommandLine, 260, slash);
        for (int i = 1; i < __argc; ++i)
            m_Args.push_back(std::wstring(__wargv[i]));
    }
}

WZResult SettingManager::ParseConfiguration(XmlNode* doc, const wchar_t* exeDir, const wchar_t* exePath)
{
    if (!doc || !doc->firstChild)
        return Fail(ERR_NO_GAMES);

    XmlNode* config = doc->firstChild;
    XmlNode* launcherUI = FindChild(config, L"LauncherUI");

    WZResult r = ParseLauncherInfo(launcherUI, exePath, exeDir);
    if (!r.IsSuccess())
        return r;
    r = ParseLauncherUI(launcherUI);
    if (!r.IsSuccess())
        return r;
    r = ParseGames(FindChild(config, L"Games"));
    if (!r.IsSuccess())
        return r;

    if (m_Games.empty())
        return Fail(ERR_NO_GAMES);
    if (m_Games.size() > 1)
    {
        std::stable_sort(m_Games.begin(), m_Games.end(),
                         [](const GameEntry* a, const GameEntry* b) { return a->ui->ListIndex < b->ui->ListIndex; });
    }
    return WZResult();
}

WZResult SettingManager::ParseLauncherInfo(XmlNode* launcherUI, const wchar_t* exePath, const wchar_t* exeDir)
{
    if (!launcherUI || !launcherUI->firstChild)
        return Fail(ERR_NO_LAUNCHERUI);

    UpdateInfo* upd = m_pLauncherUpdate;
    PathInfo* path = upd->Path;
    VersionInfo* version = upd->Version;
    if (!version)
        return Fail(ERR_NO_UPDATE_INFO);

    path->SetPaths(exeDir, exePath, L"LauncherTemp");
    version->IsLauncher = 1;

    XmlNode* vr = FindChild(launcherUI, L"VersionAndRegistryInfo");
    WZResult r = ParseVersionInfo(vr, version, path->RootDir, path->DownloadTempDir);
    if (!r.IsSuccess())
        return r;

    if (upd->Patcher)
    {
        r = ParsePatcherInfo(vr, upd->Patcher, path->RootDir, path->DownloadTempDir, path->DownloadTempDir);
        if (!r.IsSuccess())
            return r;
        if (upd->Server)
            return ParseUpdateServer(FindChild(launcherUI, L"UpdateServerInfo"), upd->Server);
    }
    return Fail(ERR_NO_UPDATE_INFO);
}

WZResult SettingManager::ParseLauncherUI(XmlNode* launcherUI)
{
    if (!launcherUI || !launcherUI->firstChild)
        return Fail(ERR_NO_NODE);

    WZResult r = ParseMainWindow(FindChild(launcherUI, L"MainWindow"));
    if (!r.IsSuccess())
        return r;
    r = ParseButtons(FindChild(launcherUI, L"Buttons"));
    if (!r.IsSuccess())
        return r;
    r = ParseControls(FindChild(launcherUI, L"Controls"));
    if (!r.IsSuccess())
        return r;
    r = ParseDescriptions(FindChild(launcherUI, L"Descriptions"));
    if (!r.IsSuccess())
        return r;
    return WZResult();
}

WZResult SettingManager::ParseDescriptions(XmlNode* node)
{
    const bool haveNode = node && node->firstChild;
    for (const DescriptionDef& d : kDescriptions)
    {
        std::wstring s;
        if (haveNode)
        {
            const wchar_t* t = ChildText(node, d.Tag);
            if (t)
                s = t;
        }
        if (s.empty())
            s = d.Default;
        m_pStringInfo->Add(s);
    }
    return WZResult();
}

// a bad <Game> is skipped, it does not fail the load.
WZResult SettingManager::ParseGames(XmlNode* node)
{
    if (!node || !node->firstChild)
        return Fail(ERR_BAD_GAME);

    XmlNode* game = node->firstChild;
    do
    {
        ParseGame(game);
        game = (node->firstChild != game->next) ? game->next : nullptr;
    } while (game);
    return WZResult();
}

WZResult SettingManager::ParseGame(XmlNode* game)
{
    if (!game || !game->firstChild)
        return Fail(ERR_BAD_GAME);

    wchar_t guid[260] = {};
    WZStrCopy(guid, 260, Attr(game, L"GUID"));
    if (!guid[0] || FindGame(guid))
        return Fail(ERR_BAD_GAME);

    wchar_t name[260] = {};
    WZStrCopy(name, 260, Attr(game, L"Name"));
    if (!name[0])
        WZStrCopy(name, 260, guid);

    GameEntry* entry = new GameEntry;
    entry->ui = new GameUIInfo;
    entry->update = new UpdateInfo;

    WZResult r;
    UpdateInfo* upd = entry->update;
    XmlNode* vr = FindChild(game, L"VersionAndRegistryInfo");

    r = ParseGameRegistry(vr, upd);
    wchar_t removeFiles[1024] = {};
    if (ReadText(vr, L"RemoveFiles", removeFiles, 1024))
        entry->removeFiles = removeFiles;
    if (r.IsSuccess())
    {
        if (!upd->Version)
            r = Fail(ERR_NO_UPDATE_INFO);
        else
            r = ParseVersionInfo(vr, upd->Version, upd->Path->RootDir, upd->Path->DownloadTempDir);
    }
    if (r.IsSuccess())
    {
        if (!upd->Patcher)
            r = Fail(ERR_NO_UPDATE_INFO);
        else
            r = ParsePatcherInfo(vr, upd->Patcher, m_pLauncherUpdate->Path->RootDir,
                                 upd->Path->RootDir, upd->Path->DownloadTempDir);
    }
    if (r.IsSuccess())
    {
        if (!upd->Server)
            r = Fail(ERR_NO_UPDATE_INFO);
        else
            r = ParseUpdateServer(FindChild(game, L"UpdateServerInfo"), upd->Server);
    }
    if (r.IsSuccess())
        r = ParseUIInfo(FindChild(game, L"UIInfo"), entry->ui, guid, name);

    if (r.IsSuccess())
    {
        m_Games.push_back(entry);
        return WZResult();
    }

    delete entry->ui;
    delete entry->update;
    delete entry;
    return r;
}

// registry-driven paths (EXE / PATH values) plus the optional net-test block.
WZResult SettingManager::ParseGameRegistry(XmlNode* node, UpdateInfo* update)
{
    if (!node || !node->firstChild)
        return Fail(ERR_NO_VERSIONINFO);

    wchar_t rootKey[260] = {};
    wchar_t subKey[260] = {};
    wchar_t exeValueName[260] = {};
    wchar_t rootValueName[260] = {};
    wchar_t tempFolder[260] = {};
    wchar_t executionFile[260] = {};
    wchar_t exeValue[260] = {};
    wchar_t pathValue[260] = {};

    ReadText(node, L"RegistryKey", rootKey);
    ReadText(node, L"RegistryPath", subKey);
    ReadText(node, L"RegistryValue", exeValueName);
    ReadText(node, L"GameRootValue", rootValueName);
    ReadText(node, L"DownloadTempForder", tempFolder);       // sic: the config uses this spelling
    if (!tempFolder[0])
        WZStrCopy(tempFolder, 260, L"DownloadTemp");
    ReadText(node, L"ExecutionFile", executionFile);          // read but unused

    WZRegReadString(rootKey, subKey, exeValueName, (BYTE*)exeValue);
    TrimPath(exeValue);
    WZRegReadString(rootKey, subKey, rootValueName, (BYTE*)pathValue);
    TrimPath(pathValue);

    // The game lives in the launcher's own folder. In an empty folder the update's full-file check
    // finds every file missing and downloads the whole client, so no existing install is needed.
    // The registry values are rewritten when they point elsewhere: the updater opens the key for
    // LastPatchState / NPT, and other tools read the game path from it.
    {
        const wchar_t* launcherDir = m_pLauncherUpdate->Path->RootDir;
        wchar_t exe[260] = {};
        WZStrCopy(exe, 260, launcherDir);
        AddBackslash(exe);
        Append(exe, L"Binaries\\");
        Append(exe, executionFile[0] ? executionFile : L"HuxleyMMOGame.exe");
        if (_wcsicmp(pathValue, launcherDir) != 0 || _wcsicmp(exeValue, exe) != 0)
        {
            WZRegWriteString(rootKey, subKey, rootValueName, launcherDir);
            WZRegWriteString(rootKey, subKey, exeValueName, exe);
        }
        WZStrCopy(pathValue, 260, launcherDir);
        WZStrCopy(exeValue, 260, exe);
    }

    update->Path->SetPaths(pathValue, exeValue, tempFolder);
    update->Path->SetRegistry(rootKey, subKey, rootValueName);

    wchar_t useNetTest[260] = {};
    if (ReadText(node, L"UseNetTest", useNetTest) && IsTrue(useNetTest))
    {
        wchar_t saveValue[260] = {};
        wchar_t downloadFile[260] = {};
        ReadText(node, L"NetTestResultSaveValue", saveValue);
        ReadText(node, L"NetTestDownloadFile", downloadFile);
        update->TestNetwork->SetRegistry(rootKey, subKey, saveValue);
        update->TestNetwork->SetDownloadFile(downloadFile);
    }
    return WZResult();
}

WZResult SettingManager::ParseVersionInfo(XmlNode* node, VersionInfo* version, const wchar_t* rootDir, const wchar_t* tempDir)
{
    wchar_t localFile[260] = {};
    wchar_t tempFile[260] = {};
    wchar_t versionFile[260] = {};
    wchar_t fileName[260] = {};

    WZStrCopy(localFile, 260, rootDir);
    AddBackslash(localFile);
    WZStrCopy(tempFile, 260, tempDir);
    AddBackslash(tempFile);

    if (node && node->firstChild)
    {
        if (!ReadText(node, L"VersionFile", versionFile))
            WZStrCopy(versionFile, 260, L"Version.txt");
    }
    else
    {
        WZStrCopy(versionFile, 260, L"Version.txt");
    }

    WZStrCopy(fileName, 260, versionFile);
    ExtractFileName(fileName);
    Append(localFile, fileName);
    Append(tempFile, fileName);

    version->SetFiles(localFile, tempFile, versionFile);
    return WZResult();
}

WZResult SettingManager::ParseUpdateServer(XmlNode* node, UpdateServerInfo* server)
{
    if (!node || !node->firstChild)
        return Fail(ERR_NO_SERVERINFO);

    wchar_t buf[260] = {};

    int passive = 0;
    if (ReadText(node, L"IsPassive", buf) && IsTrue(buf))
        passive = 1;
    server->IsPassive = passive;

    int noResume = 1;
    buf[0] = 0;
    if (ReadText(node, L"ResumeDownloadedFiles", buf) && IsTrue(buf))
        noResume = 0;
    server->NoResume = noResume;

    int isHttp = 0;
    buf[0] = 0;
    if (ReadText(node, L"Protocol", buf) && !_wcsicmp(L"http", buf))
        isHttp = 1;
    server->IsHttp = isHttp;

    buf[0] = 0;
    if (ReadText(node, L"PacketSize", buf))
    {
        int size = _wtoi(buf);
        if (size)
            server->PacketSize = size;
    }

    wchar_t url[260] = {};
    wchar_t account[260] = {};
    wchar_t password[260] = {};
    ReadText(node, L"ServerURL", url);

    WORD port;
    buf[0] = 0;
    if (ReadText(node, L"Port", buf))
        port = (WORD)_wtoi(buf);
    else
        port = (server->IsHttp != 1) ? 21 : 80;

    ReadText(node, L"Account", account);
    ReadText(node, L"Password", password);

    if (url[0])
        server->SetServer(url, port, account, password);
    return WZResult();
}

WZResult SettingManager::ParsePatcherInfo(XmlNode* node, PatcherInfo* patcher, const wchar_t* launcherDir,
                                          const wchar_t* targetDir, const wchar_t* tempDir)
{
    if (!node || !node->firstChild)
        return WZResult();

    wchar_t dllFile[260] = {};
    if (!ReadText(node, L"PatcherDllFile", dllFile))
        return WZResult();

    if (dllFile[0])
    {
        wchar_t dllPath[260] = {};
        WZStrCopy(dllPath, 260, launcherDir);
        AddBackslash(dllPath);
        Append(dllPath, dllFile);
        patcher->SetPatcherDll(dllPath);
        patcher->SetTargetDir(targetDir);
    }

    wchar_t useCheck[260] = {};
    if (ReadText(node, L"UseCheckFiles", useCheck) && IsTrue(useCheck))
    {
        wchar_t listUrl[260] = {};
        if (ReadText(node, L"CheckListFileName", listUrl) && listUrl[0])
        {
            wchar_t listName[260] = {};
            wchar_t listTemp[260] = {};
            WZStrCopy(listName, 260, listUrl);
            ExtractFileName(listName);
            WZStrCopy(listTemp, 260, tempDir);
            AddBackslash(listTemp);
            Append(listTemp, listName);
            patcher->SetCheckList(listUrl, listTemp);
        }
    }
    return WZResult();
}

WZResult SettingManager::ParseMainWindow(XmlNode* node)
{
    if (!node || !node->firstChild)
        return Fail(ERR_NO_NODE);

    CommonInfo* ci = m_pCommonInfo;
    wchar_t buf[260] = {};

    if (XmlNode* purple = FindChild(node, L"UsePurpleLauncher"))
    {
        if (IsTrue(Attr(purple, L"enable")))
            ci->UsePurpleLauncher = 1;
    }

    wchar_t dir[260] = {};
    WZStrCopy(dir, 260, m_pLauncherUpdate->Path->RootDir);
    AddBackslash(dir);

    if (ReadText(node, L"BackgroundImage", buf))
    {
        wchar_t path[260] = {};
        WZStrCopy(path, 260, dir);
        Append(path, buf + (buf[0] == L'\\' ? 1 : 0));
        ci->LoadBackground(path);
    }

    if (XmlNode* mask = FindChild(node, L"BackgroundMask"))
    {
        if (IsTrue(Attr(mask, L"enable")))
        {
            ci->BackgroundMaskEnabled = 1;
            ci->BackgroundMaskColor = _wtoi(Attr(mask, L"color"));
        }
    }

    if (ReadText(node, L"Icon", buf))
    {
        wchar_t path[260] = {};
        BuildImagePath(path, dir, buf);
        ci->hIcon = path[0] ? (HICON)LoadImageW(nullptr, path, IMAGE_ICON, 16, 16, LR_LOADFROMFILE) : nullptr;
    }

    wchar_t titleText[260] = {};
    wchar_t titleFace[260] = {};
    wchar_t staticFace[260] = {};
    int titleColor = 0;
    int staticColor = 0;

    if (XmlNode* title = FindChild(node, L"Title"))
    {
        WZStrCopy(titleText, 260, title->value.c_str());
        WZStrCopy(titleFace, 260, Attr(title, L"font"));
        titleColor = _wtoi(Attr(title, L"color"));
    }
    if (!titleFace[0])
        WZStrCopy(titleFace, 260, L"MS Sans Serif");

    if (XmlNode* st = FindChild(node, L"Static"))
    {
        WZStrCopy(staticFace, 260, Attr(st, L"font"));
        staticColor = _wtoi(Attr(st, L"color"));
    }
    if (!staticFace[0])
        WZStrCopy(staticFace, 260, L"MS Sans Serif");

    ci->CreateFonts(titleText, titleFace, titleColor, staticFace, staticColor);
    return WZResult();
}

WZResult SettingManager::ParseButtons(XmlNode* node)
{
    if (!node || !node->firstChild)
        return Fail(ERR_NO_NODE);

    CommonInfo* ci = m_pCommonInfo;
    wchar_t dir[260] = {};
    WZStrCopy(dir, 260, m_pLauncherUpdate->Path->RootDir);
    AddBackslash(dir);

    wchar_t updatePath[260] = {}, executePath[260] = {}, stopPath[260] = {}, installPath[260] = {};
    int lastX = 0, lastY = 0;

    XmlNode* btn = node->firstChild;
    do
    {
        if (IsTrue(Attr(btn, L"enable")))
        {
            wchar_t name[260] = {};
            WZStrCopy(name, 260, Attr(btn, L"Name"));
            int x = 0, y = 0;
            wchar_t image[260] = {};
            if (btn->firstChild)
            {
                ReadPosition(btn, &x, &y);
                ReadImagePath(btn, L"BackgroundImage", dir, image);
            }

            if (!_wcsicmp(name, L"minimize"))
                ci->SetMinimizeButton(image, x, y);
            else if (!_wcsicmp(name, L"exit"))
                ci->SetExitButton(image, x, y);
            else if (!_wcsicmp(name, L"Configuration"))
                ci->SetConfigurationButton(image, x, y);
            else
            {
                wchar_t* dst = nullptr;
                if (!_wcsicmp(name, L"execute"))
                    dst = executePath;
                else if (!_wcsicmp(name, L"stop"))
                    dst = stopPath;
                else if (!_wcsicmp(name, L"install"))
                    dst = installPath;
                else if (!_wcsicmp(name, L"update"))
                    dst = updatePath;
                if (dst)
                {
                    lastY = y;
                    lastX = x;
                    WZStrCopy(dst, 260, image);
                }
            }
        }
        btn = (node->firstChild != btn->next) ? btn->next : nullptr;
    } while (btn);

    ci->SetActionButtons(updatePath, executePath, stopPath, installPath, lastX, lastY);
    return WZResult();
}

WZResult SettingManager::ParseControls(XmlNode* node)
{
    if (!node || !node->firstChild)
        return Fail(ERR_NO_NODE);

    CommonInfo* ci = m_pCommonInfo;
    wchar_t dir[260] = {};
    WZStrCopy(dir, 260, m_pLauncherUpdate->Path->RootDir);
    AddBackslash(dir);

    XmlNode* ctl = node->firstChild;
    do
    {
        if (IsTrue(Attr(ctl, L"enable")))
        {
            wchar_t name[260] = {};
            WZStrCopy(name, 260, Attr(ctl, L"Name"));
            int x = 0, y = 0, w = 0, h = 0;
            wchar_t image[260] = {};
            if (ctl->firstChild)
            {
                ReadPosition(ctl, &x, &y);
                ReadSize(ctl, &w, &h);
                ReadImagePath(ctl, L"BackgroundImage", dir, image);
            }

            if (!_wcsicmp(name, L"titlestatic"))
                SetRect(&ci->TitleStaticRect, x, y, x + w, y + h);
            else if (!_wcsicmp(name, L"filestatic"))
                SetRect(&ci->FileStaticRect, x, y, x + w, y + h);
            else if (!_wcsicmp(name, L"totalstatic"))
                SetRect(&ci->TotalStaticRect, x, y, x + w, y + h);
            else if (!_wcsicmp(name, L"fileprogressbar"))
            {
                ci->hFileProgressBar = LoadBmpFile(image);
                SetRect(&ci->FileProgressRect, x, y, x + w, y + h);
            }
            else if (!_wcsicmp(name, L"totalprogressbar"))
            {
                ci->hTotalProgressBar = LoadBmpFile(image);
                SetRect(&ci->TotalProgressRect, x, y, x + w, y + h);
            }
            else if (!_wcsicmp(name, L"web"))
                SetRect(&ci->WebRect, x, y, x + w, y + h);
            else if (!_wcsicmp(name, L"gamelist"))
            {
                int lineHeight = 0;
                wchar_t lh[260] = {};
                if (ctl->firstChild && ReadText(ctl, L"LineHeight", lh))
                    lineHeight = _wtoi(lh);
                ci->GameListEnabled = 1;
                SetRect(&ci->GameListRect, x, y, x + w, y + h);
                ci->GameListLineHeight = lineHeight;
            }
            else if (!_wcsicmp(name, L"AutoExecutableCheckBox"))
            {
                wchar_t image2[260] = {};
                ReadImagePath(ctl, L"BackgroundImage2", dir, image2);
                ci->SetCheckBox(image, image2, x, y);
            }
        }
        ctl = (node->firstChild != ctl->next) ? ctl->next : nullptr;
    } while (ctl);
    return WZResult();
}

WZResult SettingManager::ParseUIInfo(XmlNode* node, GameUIInfo* ui, const wchar_t* guid, const wchar_t* name)
{
    if (!node || !node->firstChild)
        return Fail(ERR_NO_UIINFO);

    ui->SetGuid(guid);
    ui->SetName(name);

    wchar_t dir[260] = {};
    WZStrCopy(dir, 260, m_pLauncherUpdate->Path->RootDir);
    AddBackslash(dir);

    if (const wchar_t* t = ChildText(node, L"ListIndex"))
        ui->ListIndex = _wtoi(t);
    if (const wchar_t* t = ChildText(node, L"WebURL"))
        ui->SetWebURL(t);
    if (const wchar_t* t = ChildText(node, L"InstallURL"))
        ui->SetInstallURL(t);

    if (m_pCommonInfo->GameListEnabled)
    {
        wchar_t path[260] = {};
        if (const wchar_t* t = ChildText(node, L"ListImage"))
        {
            BuildImagePath(path, dir, t);
            ui->hListImage = LoadBmpFile(path);
        }
        if (const wchar_t* t = ChildText(node, L"ListIcon"))
        {
            BuildImagePath(path, dir, t);
            ui->hListIcon = path[0] ? (HICON)LoadImageW(nullptr, path, IMAGE_ICON, 16, 16, LR_LOADFROMFILE) : nullptr;
        }
    }

    if (const wchar_t* t = ChildText(node, L"GameOptionDll"))
        ui->SetGameOptionDll(t);
    return WZResult();
}

GameEntry* SettingManager::FindGame(const wchar_t* guid) const
{
    if (!wcscmp(guid, L""))
        return m_Games.empty() ? nullptr : m_Games.front();
    for (GameEntry* e : m_Games)
    {
        if (e && !wcscmp(guid, e->ui->Guid))
            return e;
    }
    return nullptr;
}

void SettingManager::GetGameGuidArgument(wchar_t* out) const
{
    for (size_t i = 0; i < m_Args.size(); ++i)
    {
        if (m_Args[i] == L"/d" || m_Args[i] == L"/D")
        {
            if (i + 1 < m_Args.size())
                WZStrCopy(out, 260, m_Args[i + 1].c_str());
            return;
        }
    }
}

void SettingManager::GetGameArguments(wchar_t* out) const
{
    for (size_t i = 0; i < m_Args.size(); ++i)
    {
        if (m_Args[i] == L"/g" || m_Args[i] == L"/G")
        {
            size_t next = i + 1;
            if (next >= m_Args.size())
                return;
            if (m_Args[next] == L"-name")
            {
                ++next;
                if (next >= m_Args.size())
                    return;
            }
            WZStrCopy(out, 4096, m_Args[next].c_str());
            return;
        }
    }
}
