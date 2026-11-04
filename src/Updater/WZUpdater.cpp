#include "WZUpdater.h"
#include "PathUtil.h"
#include "UpdaterRegistry.h"

WZUpdater::WZUpdater(IUpdaterStateEvent* listener)
    : m_break(0), m_versionManager(nullptr), m_downloadManager(nullptr), m_patcher(nullptr), m_checker(nullptr),
      m_netTest(nullptr), m_updateInfo(nullptr), m_server(nullptr), m_version(nullptr), m_patcherInfo(nullptr),
      m_testNetwork(nullptr), m_path(nullptr)
{
    m_sender.listener = listener;
}

WZUpdater::~WZUpdater()
{
    ClearSubObjects();
}

bool WZUpdater::Ok()
{
    if (m_break)
        m_result.Set(1, 0, L"[WZUpdater] User Break");
    return m_result.IsSuccess();
}

void WZUpdater::ClearSubObjects()
{
    delete m_versionManager;
    m_versionManager = nullptr;
    delete m_downloadManager;
    m_downloadManager = nullptr;
    delete m_patcher;
    m_patcher = nullptr;
    delete m_checker;
    m_checker = nullptr;
    delete m_netTest;
    m_netTest = nullptr;
    ClearFileList();
}

void WZUpdater::ClearFileList()
{
    for (FileInfo* file : m_files)
        delete file;
    m_files.clear();
}

void WZUpdater::Break()
{
    m_break = 1;
    if (m_netTest)
        m_netTest->Stop();
    if (m_versionManager)
        m_versionManager->breakFlag = 1;
    if (m_downloadManager)
        m_downloadManager->Break();
    if (m_patcher)
        m_patcher->Break();
    if (m_checker)
        m_checker->breakFlag = 1;
}

WZResult WZUpdater::Init(UpdateInfo* info)
{
    ClearSubObjects();
    m_updateInfo = info;
    if (!info)
    {
        m_result.Set(0x20000002, 0, L"[WZUpdater::Init] Fail : Not Updaet Info");
        return m_result;
    }
    m_server = info->Server;
    if (!m_server)
    {
        m_result.Set(0x20000002, 0, L"[WZUpdater::Init] Fail : Not Server Info");
        return m_result;
    }
    m_version = info->Version;
    if (!m_version)
    {
        m_result.Set(0x10000002, 0, L"[WZUpdater::Init] Fail : Not Version Info");
        return m_result;
    }
    m_patcherInfo = info->Patcher;
    if (!m_patcherInfo)
    {
        m_result.Set(0x40000002, 0, L"[WZUpdater::Init] Fail : Not Patcher Info");
        return m_result;
    }
    m_testNetwork = info->TestNetwork;
    if (!m_testNetwork)
    {
        m_result.Set(0x40000002, 0, L"[WZUpdater::Init] Fail : Not TestNetwork Info");
        return m_result;
    }
    m_path = info->Path;
    if (!m_path)
    {
        m_result.Set(0x40000002, 0, L"[WZUpdater::Init] Fail : Not Path Info");
        return m_result;
    }
    m_result.SetSuccess();
    return m_result;
}

WZResult WZUpdater::CheckVersion()
{
    size_t len = 0;
    while (len < 260 && m_version->ServerVersion[len])
        ++len;
    if (len >= 2 && len < 260)
    {
        m_result.SetSuccess();
        return m_result;
    }

    m_result = DownloadVersionFile();
    if (!Ok())
        return m_result;

    delete m_versionManager;
    m_versionManager = new VersionManager(m_sender.listener);
    m_result = m_versionManager->GetUpdateState(m_version);
    return m_result;
}

WZResult WZUpdater::DownloadVersionFile()
{
    FileInfo* file = new FileInfo;
    file->Set(m_version->VersionFileUrl, m_version->TempVersionFile, m_version->VersionFileUrl);
    m_files.push_back(file);
    m_result = DownloadFiles();
    if (!m_files.empty())
    {
        delete m_files.back();
        m_files.pop_back();
    }
    return m_result;
}

WZResult WZUpdater::Update()
{
    ClearSubObjects();
    StartNetTest();
    if (m_sender.listener)
        m_sender.listener->OnUpdateBegin();

    m_result = CheckLastPatch();
    if (!Ok())
        return Recover();

    m_result = CheckVersionChain();
    if (!Ok())
        return Recover();

    m_result = DownloadFiles();
    if (!Ok())
        return Recover();

    m_result = PatchFiles();
    if (!Ok())
        return Recover();

    WaitNetTest();
    m_sender.UpdateEnd(m_result);
    return m_result;
}

void WZUpdater::StartNetTest()
{
    delete m_netTest;
    m_netTest = new NetTest(m_server, m_testNetwork);
    if (m_testNetwork)
        m_netTest->Start(m_path->DownloadTempDir);
    else
        m_netTest->MarkDone();
}

WZResult WZUpdater::CheckLastPatch()
{
    if (m_patcherInfo && m_patcherInfo->HasPatcher && m_patcherInfo->UseCheckFiles)
    {
        DWORD state = 0;
        UpdaterRegistry::GetDword(m_path->RegistrySubKey, m_path->RegistryRootKey, L"LastPatchState", &state);
        if (state == 1)
            m_result.Set(0xF0000002, 0, L"[WZUpdater::CheckLastPatch] Last Patch Error");
    }
    return m_result;
}

WZResult WZUpdater::CheckVersionChain()
{
    delete m_versionManager;
    m_versionManager = new VersionManager(m_sender.listener);
    return m_versionManager->CheckVersion(m_version, m_files, m_path->DownloadTempDir);
}

WZResult WZUpdater::DownloadFiles()
{
    if (m_files.empty())
    {
        m_result.Set(0xF0000001, 0, L"[WZUpdater::DownloadFiles] Fail : Empty File List");
        return m_result;
    }
    delete m_downloadManager;
    m_downloadManager = new FileDownloadManager(m_sender.listener);
    return m_downloadManager->DownloadFiles(m_files, m_server);
}

// LastPatchState = 1 while patch files are being applied.
WZResult WZUpdater::PatchFiles()
{
    delete m_patcher;
    m_patcher = new FilePatcherManager(m_sender.listener);
    if (m_patcherInfo->UseCheckFiles)
        UpdaterRegistry::SetDword(m_path->RegistrySubKey, m_path->RegistryRootKey, L"LastPatchState", 1);
    m_result = m_patcher->PatchFiles(m_files, m_patcherInfo);
    if (m_patcherInfo->UseCheckFiles)
        UpdaterRegistry::SetDword(m_path->RegistrySubKey, m_path->RegistryRootKey, L"LastPatchState", 0);
    return m_result;
}

void WZUpdater::WaitNetTest()
{
    if (m_netTest && !m_netTest->IsDone())
        while (!m_netTest->IsDone())
            Sleep(10);
}

// after a failed or interrupted run: compare the installation with FullFile.txt, re-download
// every mismatching file, then report the end of the update.
WZResult WZUpdater::Recover()
{
    WaitNetTest();

    bool failed = false;
    if (m_result.code != 1 && m_patcherInfo && m_patcherInfo->HasPatcher && m_patcherInfo->UseCheckFiles)
    {
        UpdaterRegistry::SetDword(m_path->RegistrySubKey, m_path->RegistryRootKey, L"LastPatchState", 1);
        m_result = CheckFullFiles();
        if (!Ok())
        {
            failed = true;
        }
        else if (!m_files.empty())
        {
            m_result = DownloadFiles();
            failed = !Ok();
        }
    }

    if (failed)
    {
        m_sender.UpdateEnd(m_result);
    }
    else
    {
        if (!m_result.code && !m_result.subCode && m_patcherInfo->UseCheckFiles)
            UpdaterRegistry::SetDword(m_path->RegistrySubKey, m_path->RegistryRootKey, L"LastPatchState", 0);
        m_sender.UpdateEnd(m_result);
    }
    return m_result;
}

WZResult WZUpdater::CheckFullFiles()
{
    ClearFileList();
    m_result = DownloadCheckList();
    if (m_break)
        m_result.Set(1, 0, L"[WZUpdater] User Break");
    if (!m_result.code && !m_result.subCode)
    {
        delete m_checker;
        m_checker = new CFileCheckerManager(m_sender.listener);
        m_result = m_checker->CheckFiles(m_patcherInfo, m_server, m_files);
        DeleteFileW(m_patcherInfo->CheckListTempPath);
    }
    return m_result;
}

WZResult WZUpdater::DownloadCheckList()
{
    FileInfo* file = new FileInfo;
    file->Set(m_patcherInfo->CheckListFileUrl, m_patcherInfo->CheckListTempPath, m_patcherInfo->CheckListFileUrl);
    m_files.push_back(file);
    m_result = DownloadFiles();
    if (!m_files.empty())
    {
        delete m_files.back();
        m_files.pop_back();
    }
    return m_result;
}

WZResult WZUpdater::UpdateVersion()
{
    delete m_versionManager;
    m_versionManager = new VersionManager(m_sender.listener);
    return m_versionManager->ChangeCurrentVersion(m_version);
}
