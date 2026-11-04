#include "FilePatcherManager.h"
#include "PathUtil.h"
#include <fstream>

// dword_475FB4 / dword_475FB8: sender and file the adapter progress callback reports to.
static UpdaterStateEventSender* g_patchSender = nullptr;
static FileInfo* g_currentPatchFile = nullptr;

static BOOL __stdcall PatchProgress(int /*unused*/, const unsigned __int64* progress)
{
    if (g_patchSender && g_patchSender->listener)
        g_patchSender->listener->OnPatchFileProgress(g_currentPatchFile, *progress);
    return TRUE;
}

FilePatcherManager::FilePatcherManager(IUpdaterStateEvent* listener)
    : breakFlag(0), m_library(nullptr), m_setBreak(nullptr), m_unPatchFile(nullptr)
{
    m_sender.listener = listener;
    g_patchSender = &m_sender;
}

FilePatcherManager::~FilePatcherManager()
{
    if (m_library)
    {
        FreeLibrary(m_library);
        m_library = nullptr;
        m_setBreak = nullptr;
        m_unPatchFile = nullptr;
    }
    if (g_patchSender == &m_sender)
        g_patchSender = nullptr;
}

void FilePatcherManager::Break()
{
    breakFlag = 1;
    if (m_setBreak)
        m_setBreak();
}

bool FilePatcherManager::Ok()
{
    if (breakFlag)
        m_result.Set(1, 0, L"[FilePatcherManager] User Break");
    return m_result.IsSuccess();
}

WZResult FilePatcherManager::LoadPatcherLibrary(PatcherInfo* patcher)
{
    if (m_library)
    {
        FreeLibrary(m_library);
        m_library = nullptr;
        m_setBreak = nullptr;
        m_unPatchFile = nullptr;
    }
    if (!patcher || !patcher->PatcherDllPath[0])
    {
        m_result.Set(0x40000003, GetLastError(), L"[FilePatcherManager::LoadPatcherLibrary] Fail : Not Patcher Dll Info");
        return m_result;
    }
    m_library = LoadLibraryW(patcher->PatcherDllPath);
    if (!m_library)
    {
        m_result.Set(0x40000004, GetLastError(), L"[FilePatcherManager::LoadPatcherLibrary] Fail : LoadLibrary");
        return m_result;
    }
    m_setBreak = reinterpret_cast<SetBreakProc>(GetProcAddress(m_library, "SetBreak"));
    m_unPatchFile = reinterpret_cast<UnPatchFileProc>(GetProcAddress(m_library, "UnPatchFile"));
    if (!m_setBreak || !m_unPatchFile)
        m_result.Set(0x40000005, 0, L"[FilePatcherManager::LoadPatcherLibrary] Fail : GetProcAddress");
    else
        m_result.SetSuccess();
    return m_result;
}

// CompulsionFileCheck.txt in the game root whose first line starts with '1' requests a
// full file check; the marker file is consumed.
bool FilePatcherManager::CompulsionFileCheck(PatcherInfo* patcher)
{
    wchar_t file[260] = {};
    PathUtil::Copy(file, 260, patcher->TargetDir);
    PathUtil::AddBackslash(file);
    size_t len = wcslen(file);
    if (len < 260)
        PathUtil::Copy(file + len, 260 - len, L"CompulsionFileCheck.txt");

    std::wifstream in(file, std::ios::in | std::ios::binary);
    if (!in.is_open())
        return false;
    wchar_t line[1024] = {};
    in.getline(line, 1024);
    in.close();
    DeleteFileW(file);
    return line[0] == L'1';
}

WZResult FilePatcherManager::PatchFiles(std::vector<FileInfo*>& files, PatcherInfo* patcher)
{
    if (!patcher)
    {
        m_result.Set(0x40000002, 0, L"[FilePatcherManager::PatchFiles] Fail : Not Patcher Info");
        return m_result;
    }
    if (!patcher->HasPatcher)
    {
        m_result.SetSuccess();
        return m_result;
    }

    m_result = LoadPatcherLibrary(patcher);
    if (!Ok())
        return m_result;

    if (m_sender.listener)
        m_sender.listener->OnPatchBegin();

    for (FileInfo* file : files)
    {
        g_currentPatchFile = file;
        if (file)
        {
            if (m_unPatchFile)
            {
                if (m_sender.listener)
                    m_sender.listener->OnPatchFileStart(file, file->size);
                wchar_t target[260] = {};
                PathUtil::Copy(target, 260, patcher->TargetDir);
                PathUtil::RemoveBackslash(target);
                if (m_unPatchFile(file->localPath, target, PatchProgress))
                    m_result.SetSuccess();
                else
                    m_result.Set(0x40000006, 0, L"[FilePatcherManager::PatchFiles] Fail : m_PatchFileMethod");
                m_sender.PatchFileEnd(m_result);
                g_currentPatchFile = nullptr;
            }
            if (!Ok())
                return m_result;
            if (CompulsionFileCheck(patcher))
            {
                m_result.Set(0x40000009, 0, L"[FilePatcherManager::PatchFiles] -- Set CompulsionFileCheck.txt --");
                return m_result;
            }
        }
    }
    m_result.SetSuccess();
    return m_result;
}
