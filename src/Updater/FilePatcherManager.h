// FilePatcherManager.h - applies downloaded patch files through the patcher adapter DLL.
#pragma once
#include <vector>
#include "UpdaterEvents.h"
#include "UpdaterInfo.h"

// Exports of ZlibAdapter.dll / RTPatchAdapter.dll (both identical in shape, cdecl):
//   bool UnPatchFile(const wchar_t* patchFile, wchar_t* targetDir, PatchProgressProc progress);
//   void SetBreak();   // aborts the running UnPatchFile
typedef BOOL(__stdcall* PatchProgressProc)(int unused, const unsigned __int64* progress);
typedef unsigned char(__cdecl* UnPatchFileProc)(const wchar_t* patchFile, wchar_t* targetDir, PatchProgressProc progress);
typedef void(__cdecl* SetBreakProc)();

// Object size 0x820.
class FilePatcherManager
{
public:
    explicit FilePatcherManager(IUpdaterStateEvent* listener);    // (inline ctor)
    ~FilePatcherManager();

    WZResult PatchFiles(std::vector<FileInfo*>& files, PatcherInfo* patcher);
    WZResult LoadPatcherLibrary(PatcherInfo* patcher);
    void Break();                                                 // see WZUpdater::Break

    volatile LONG breakFlag;

private:
    bool CompulsionFileCheck(PatcherInfo* patcher);
    bool Ok();

    WZResult                m_result;
    UpdaterStateEventSender m_sender;                             // (listener)
    HMODULE                 m_library;
    SetBreakProc            m_setBreak;
    UnPatchFileProc         m_unPatchFile;
};
