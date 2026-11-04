#include "UpdaterManager.h"

UpdaterManager::~UpdaterManager()
{
    delete m_updater;
    m_updater = nullptr;
}

void UpdaterManager::Break()
{
    m_break = 1;
    if (m_updater)
        m_updater->Break();
}

WZResult UpdaterManager::Init(UpdateInfo* info, IUpdaterStateEvent* listener)
{
    if (m_updater)
    {
        m_updater->Break();
        delete m_updater;
        m_updater = nullptr;
    }
    m_break = 0;
    m_updater = new WZUpdater(listener);
    return m_updater->Init(info);
}

// fetches the version file and sets VersionInfo::status (4 on a network error).
WZResult UpdaterManager::CheckVersion(UpdateInfo* info, IUpdaterStateEvent* listener)
{
    m_result = Init(info, listener);
    if (m_break)
        m_result.Set(1, 0, L"User Break");
    if (!m_result.code && !m_result.subCode)
    {
        m_result = m_updater->CheckVersion();
        if (m_result.code & 0x20000000)
            info->Version->State = 4;
    }
    return m_result;
}

WZResult UpdaterManager::Update(UpdateInfo* info, IUpdaterStateEvent* listener)
{
    m_result = Init(info, listener);
    if (m_break)
        m_result.Set(1, 0, L"User Break");
    if (!m_result.code && !m_result.subCode)
        m_result = m_updater->Update();
    return m_result;
}

WZResult UpdaterManager::StartNetTest(UpdateInfo* info, IUpdaterStateEvent* listener)
{
    m_result = Init(info, listener);
    if (m_break)
        m_result.Set(1, 0, L"User Break");
    if (!m_result.code && !m_result.subCode)
        m_updater->StartNetTest();
    return m_result;
}

WZResult UpdaterManager::UpdateVersion(UpdateInfo* info, IUpdaterStateEvent* listener)
{
    m_result = Init(info, listener);
    if (m_break)
        m_result.Set(1, 0, L"User Break");
    if (!m_result.code && !m_result.subCode)
        m_result = m_updater->UpdateVersion();
    return m_result;
}
