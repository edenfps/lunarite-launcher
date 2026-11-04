// UpdateInfo.h - PathInfo, VersionInfo, PatcherInfo, UpdateServerInfo, TestNetworkInfo, UpdateInfo.
#pragma once

#include "../Common/WZCommon.h"

class PathInfo
{
public:
    PathInfo();
    virtual ~PathInfo();

    // root dir and exe path; the second then gets "\" + tempFolderName.
    void SetPaths(const wchar_t* rootDir, const wchar_t* exePath, const wchar_t* tempFolderName);
    void SetRegistry(const wchar_t* rootKey, const wchar_t* subKey, const wchar_t* gameRootValue);

    wchar_t RootDir[260];               // game (or launcher) directory
    wchar_t DownloadTempDir[260];       // <RootDir>\DownloadTemp (launcher: \LauncherTemp)
    wchar_t ExecutablePath[260];        // registry EXE value (launcher: its own exe path)
    wchar_t RegistryRootKey[260];       // e.g. "HKEY_CURRENT_USER"
    wchar_t RegistrySubKey[260];        // e.g. "Software\Webzen\HuxleyTheDystopia"
    wchar_t GameRootValueName[260];     // "PATH"
};

// ctor, dtor. 2612 bytes.
class VersionInfo
{
public:
    VersionInfo();
    virtual ~VersionInfo();

    void SetFiles(const wchar_t* localVersionFile, const wchar_t* tempVersionFile, const wchar_t* versionFileUrl);
    void SetLocalVersion(const wchar_t* s);     //  (shared by Updater)
    void SetServerVersion(const wchar_t* s);    // (shared by Updater)

    int     IsLauncher;                 // set to 1 for the launcher's own VersionInfo
    int     State;                      // initial 255; Updater sets it to 1
    wchar_t LocalVersion[260];          // version string read from the local version file
    wchar_t ServerVersion[260];         // version string read from the downloaded version file
    wchar_t LocalVersionFile[260];      // <RootDir>\<version file name>
    wchar_t VersionFileUrl[260];        // <VersionFile> from the XML (server relative path)
    wchar_t TempVersionFile[260];       // <DownloadTempDir>\<version file name>
};

// Anonymous object.
class PatcherInfo
{
public:
    PatcherInfo();

    void SetPatcherDll(const wchar_t* dllPath);
    void SetTargetDir(const wchar_t* dir);
    void SetCheckList(const wchar_t* fileUrl, const wchar_t* tempPath);

    int     HasPatcher;
    int     UseCheckFiles;
    int     CheckFilesDone;             // set by the controller after a full-file check
    wchar_t PatcherDllPath[260];        // <launcher dir>\<PatcherDllFile>
    wchar_t TargetDir[260];
    wchar_t CheckListFileUrl[260];      // <CheckListFileName>
    wchar_t CheckListTempPath[260];     // <DownloadTempDir>\<check list file name>
};

// Object.
struct UpdateServerInfo
{
    UpdateServerInfo();
    void SetServer(const wchar_t* url, WORD port, const wchar_t* account, const wchar_t* password);

    wchar_t ServerURL[2084];            // host (text after "://")
    wchar_t Account[128];
    wchar_t Password[128];
    WORD    Port;                       // (default 21)
    int     IsHttp;                     // 1 = HTTP, 0 = FTP
    int     PacketSize;                 // default 4096
    int     NoResume;                   // default 1; 0 when ResumeDownloadedFiles=true
    int     IsPassive;
    int     Reserved;
};

// ctor, dtor. 2092 bytes.
class TestNetworkInfo
{
public:
    TestNetworkInfo();
    virtual ~TestNetworkInfo();

    // = 1, registry root/sub key/result value name
    void SetRegistry(const wchar_t* rootKey, const wchar_t* subKey, const wchar_t* resultValueName);
    void SetDownloadFile(const wchar_t* file);

    int     Enabled;
    wchar_t RegistryRootKey[260];
    wchar_t RegistrySubKey[260];
    wchar_t ResultValueName[260];       // "NPT"
    wchar_t DownloadFile[260];
    int     Testing;                    // 1 while the net-test download runs
};

// ctor, / dtor. 24 bytes.
class UpdateInfo
{
public:
    UpdateInfo();
    virtual ~UpdateInfo();

    PathInfo*         Path;
    VersionInfo*      Version;
    PatcherInfo*      Patcher;
    UpdateServerInfo* Server;
    TestNetworkInfo*  TestNetwork;

    UpdateInfo(const UpdateInfo&) = delete;
    UpdateInfo& operator=(const UpdateInfo&) = delete;
};

#ifdef _M_IX86
#include <cstddef>
static_assert(sizeof(PathInfo) == 3124 && offsetof(PathInfo, GameRootValueName) == 2604, "layout");
static_assert(sizeof(VersionInfo) == 2612 && offsetof(VersionInfo, TempVersionFile) == 2092, "layout");
static_assert(sizeof(PatcherInfo) == 2092 && offsetof(PatcherInfo, CheckListTempPath) == 1572, "layout");
static_assert(sizeof(UpdateServerInfo) == 4704 && offsetof(UpdateServerInfo, Port) == 4680, "layout");
static_assert(sizeof(TestNetworkInfo) == 2092 && offsetof(TestNetworkInfo, Testing) == 2088, "layout");
static_assert(sizeof(UpdateInfo) == 24, "layout");
#endif
