// WZThread.h - thin _beginthreadex wrapper ( dtor, Start, thread proc).
// Holds the thread handle (-1 when none), thread id, parameter and function.
#pragma once
#include "WZCommon.h"

class WZThread
{
public:
    typedef void (__cdecl *ThreadFunc)(void*);

    WZThread(ThreadFunc func, void* param) : m_handle((HANDLE)-1), m_threadId(0), m_param(param), m_func(func) {}
    virtual ~WZThread();

    BOOL Start();
    HANDLE Handle() const { return m_handle; }
    void ClearHandle() { m_handle = (HANDLE)-1; }   // after TerminateThread

private:
    static unsigned __stdcall ThreadProc(void* self);

    HANDLE     m_handle;
    DWORD      m_threadId;
    void*      m_param;
    ThreadFunc m_func;
};
