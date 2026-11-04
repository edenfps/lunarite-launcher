#include "UpdateInfo.h"
#include "SettingsUtil.h"
#include <cstddef>
#include <cstring>

// ---- PathInfo -------------------------------------------------------------------------------
PathInfo::PathInfo() { memset(RootDir, 0, sizeof(PathInfo) - offsetof(PathInfo, RootDir)); }
PathInfo::~PathInfo() {}

void PathInfo::SetPaths(const wchar_t* rootDir, const wchar_t* exePath, const wchar_t* tempFolderName)
{
    WZStrCopy(RootDir, 260, rootDir);
    WZStrCopy(DownloadTempDir, 260, rootDir);
    WZStrCopy(ExecutablePath, 260, exePath);
    settings_detail::AddBackslash(DownloadTempDir);
    settings_detail::Append(DownloadTempDir, tempFolderName);
}

void PathInfo::SetRegistry(const wchar_t* rootKey, const wchar_t* subKey, const wchar_t* gameRootValue)
{
    WZStrCopy(RegistryRootKey, 260, rootKey);
    WZStrCopy(RegistrySubKey, 260, subKey);
    WZStrCopy(GameRootValueName, 260, gameRootValue);
}

// ---- VersionInfo ----------------------------------------------------------------------------
VersionInfo::VersionInfo()
{
    memset(&IsLauncher, 0, sizeof(VersionInfo) - offsetof(VersionInfo, IsLauncher));
    State = 255;
}
VersionInfo::~VersionInfo() {}

void VersionInfo::SetFiles(const wchar_t* localVersionFile, const wchar_t* tempVersionFile, const wchar_t* versionFileUrl)
{
    WZStrCopy(LocalVersionFile, 260, localVersionFile);
    WZStrCopy(TempVersionFile, 260, tempVersionFile);
    WZStrCopy(VersionFileUrl, 260, versionFileUrl);
}
void VersionInfo::SetLocalVersion(const wchar_t* s) { WZStrCopy(LocalVersion, 260, s); }
void VersionInfo::SetServerVersion(const wchar_t* s) { WZStrCopy(ServerVersion, 260, s); }

// ---- PatcherInfo ----------------------------------------------------------------------------
PatcherInfo::PatcherInfo() { memset(&HasPatcher, 0, sizeof(PatcherInfo)); }

void PatcherInfo::SetPatcherDll(const wchar_t* dllPath)
{
    HasPatcher = 1;
    WZStrCopy(PatcherDllPath, 260, dllPath);
}
void PatcherInfo::SetTargetDir(const wchar_t* dir) { WZStrCopy(TargetDir, 260, dir); }
void PatcherInfo::SetCheckList(const wchar_t* fileUrl, const wchar_t* tempPath)
{
    UseCheckFiles = 1;
    WZStrCopy(CheckListFileUrl, 260, fileUrl);
    WZStrCopy(CheckListTempPath, 260, tempPath);
}

// ---- UpdateServerInfo -----------------------------------------------------------------------
UpdateServerInfo::UpdateServerInfo()
{
    memset(this, 0, sizeof(*this));
    Port = 21;
    PacketSize = 4096;
    NoResume = 1;
}

void UpdateServerInfo::SetServer(const wchar_t* url, WORD port, const wchar_t* acct, const wchar_t* pass)
{
    const wchar_t* colon = wcschr(url, L':');
    if (colon && colon[1] == L'/' && colon[2] == L'/')
        WZStrCopy(ServerURL, 2084, colon + 3);
    else
        WZStrCopy(ServerURL, 2084, url);
    Port = port;
    WZStrCopy(Account, 128, acct);
    WZStrCopy(Password, 128, pass);
}

// ---- TestNetworkInfo ------------------------------------------------------------------------
TestNetworkInfo::TestNetworkInfo() { memset(&Enabled, 0, sizeof(TestNetworkInfo) - offsetof(TestNetworkInfo, Enabled)); }
TestNetworkInfo::~TestNetworkInfo() {}

void TestNetworkInfo::SetRegistry(const wchar_t* rootKey, const wchar_t* subKey, const wchar_t* resultValueName)
{
    Enabled = 1;
    WZStrCopy(RegistryRootKey, 260, rootKey);
    WZStrCopy(RegistrySubKey, 260, subKey);
    WZStrCopy(ResultValueName, 260, resultValueName);
}
void TestNetworkInfo::SetDownloadFile(const wchar_t* file) { WZStrCopy(DownloadFile, 260, file); }

// ---- UpdateInfo -----------------------------------------------------------------------------
UpdateInfo::UpdateInfo()
    : Path(new PathInfo), Version(new VersionInfo), Patcher(new PatcherInfo),
      Server(new UpdateServerInfo), TestNetwork(new TestNetworkInfo)
{
}

UpdateInfo::~UpdateInfo()
{
    delete Server;
    delete Version;
    delete Patcher;
    delete TestNetwork;
    delete Path;
}
