// UpdaterManager.h - facade used by WZLauncherController.
#pragma once
#include "WZUpdater.h"

class UpdaterManager
{
public:
    UpdaterManager() : m_break(0), m_updater(nullptr) {}
    virtual ~UpdaterManager();

    // Each call recreates the WZUpdater for the given info and runs one operation.
    WZResult CheckVersion(UpdateInfo* info, IUpdaterStateEvent* listener);
    WZResult Update(UpdateInfo* info, IUpdaterStateEvent* listener);
    WZResult StartNetTest(UpdateInfo* info, IUpdaterStateEvent* listener);
    WZResult UpdateVersion(UpdateInfo* info, IUpdaterStateEvent* listener);
    void Break();                                                          // sets the break flag and breaks the running updater

    const WZResult& Result() const { return m_result; }
    WZUpdater* Updater() const { return m_updater; }

private:
    WZResult Init(UpdateInfo* info, IUpdaterStateEvent* listener);

    volatile LONG m_break;
    WZResult      m_result;
    WZUpdater*    m_updater;
};
