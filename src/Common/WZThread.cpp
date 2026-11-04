#include "WZThread.h"
#include <process.h>

WZThread::~WZThread()
{
    if (m_handle != (HANDLE)-1)
        CloseHandle(m_handle);
}

BOOL WZThread::Start()
{
    if (m_handle != (HANDLE)-1)
    {
        CloseHandle(m_handle);
        m_handle = (HANDLE)-1;
    }
    unsigned threadId = 0;
    uintptr_t h = _beginthreadex(nullptr, 0, ThreadProc, this, 0, &threadId);
    m_handle = (HANDLE)h;
    m_threadId = threadId;
    return h != (uintptr_t)-1;
}

unsigned __stdcall WZThread::ThreadProc(void* self)
{
    WZThread* t = static_cast<WZThread*>(self);
    if (t->m_func)
        t->m_func(t->m_param);
    _endthreadex(0);
    return 0;
}
