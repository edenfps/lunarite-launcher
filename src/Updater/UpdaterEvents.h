// UpdaterEvents.h - FileInfo, IUpdaterStateEvent, IDownloaderStateEvent, UpdaterStateEventSender.
#pragma once
#include "Common/WZCommon.h"

// 0x1668 bytes, cleared by, filled by. One entry per file the updater handles.
struct FileInfo
{
    wchar_t name[260];          // display name: path relative to the patch/full directory
    wchar_t localPath[260];     // destination on disk
    wchar_t remotePath[2084];   // server path, e.g. /huxley/Patch/Korea/Service/City/<file>
    wchar_t reserved[260];
    unsigned __int64 size;      // expected / reported size in bytes

    FileInfo();
    void Set(const wchar_t* name, const wchar_t* local, const wchar_t* remote);
};

// Implemented by WZLauncherController (vtable). Slot numbers are vtable indices.
class IUpdaterStateEvent
{
public:
    virtual ~IUpdaterStateEvent() {}                                                    // [0]
    virtual void OnUpdateBegin() = 0;                                                   // [1]
    virtual void OnUpdateEnd(WZResult result) = 0;                                      // [2]
    virtual void OnCheckVersionBegin() = 0;                                             // [3]
    virtual void OnCheckVersionEnd(WZResult result, int fileCount,
                                   unsigned __int64 totalSize) = 0;                     // [4]
    virtual void OnDownloadStart(const FileInfo* file, unsigned __int64 size) = 0;      // [5]
    virtual void OnDownloadProgress(const FileInfo* file, unsigned __int64 done) = 0;   // [6]
    virtual void OnDownloadEnd(WZResult result) = 0;                                    // [7]
    virtual void OnPatchBegin() = 0;                                                    // [8]
    virtual void OnPatchFileStart(const FileInfo* file, unsigned __int64 size) = 0;     // [9]
    virtual void OnPatchFileProgress(const FileInfo* file, unsigned __int64 done) = 0;  // [10]
    virtual void OnPatchFileEnd(WZResult result) = 0;                                   // [11]
    virtual void OnCheckFilesBegin() = 0;                                               // [12]
    virtual void OnCheckFilesProgress(int total, int current) = 0;                      // [13]
    virtual void OnCheckFilesEnd(WZResult result, int fileCount,
                                 unsigned __int64 totalSize) = 0;                       // [14]
    // Lunarite addition: bytes downloaded so far across all files, finished and in flight, while
    // FileDownloadManager downloads several files at once.
    virtual void OnDownloadTotal(unsigned __int64 doneBytes) {}
};

// Implemented by FileDownloadManager (vtable).
class IDownloaderStateEvent
{
public:
    virtual ~IDownloaderStateEvent() {}                                                 // [0]
    virtual void OnStart(const FileInfo* file, unsigned __int64 size) = 0;              // [1]
    virtual void OnProgress(const FileInfo* file, unsigned __int64 done) = 0;           // [2]
    virtual void OnEnd(WZResult result) = 0;                                            // [3]
};

// Null-safe forwarder to the listener (vtable, listener). Methods mirror
// (UpdateEnd), (CheckVersionEnd), (DownloadEnd), (PatchFileEnd),
// (CheckFilesEnd).
class UpdaterStateEventSender
{
public:
    UpdaterStateEventSender() : listener(nullptr) {}
    virtual ~UpdaterStateEventSender() {}

    IUpdaterStateEvent* listener;

    void UpdateEnd(const WZResult& r)                                  { if (listener) listener->OnUpdateEnd(r); }
    void CheckVersionEnd(const WZResult& r, int n, unsigned __int64 s) { if (listener) listener->OnCheckVersionEnd(r, n, s); }
    void DownloadEnd(const WZResult& r)                                { if (listener) listener->OnDownloadEnd(r); }
    void PatchFileEnd(const WZResult& r)                               { if (listener) listener->OnPatchFileEnd(r); }
    void CheckFilesEnd(const WZResult& r, int n, unsigned __int64 s)   { if (listener) listener->OnCheckFilesEnd(r, n, s); }
};
