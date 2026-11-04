#include "MsgSender.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
    // COPYDATASTRUCT payload: 160 ANSI bytes followed by one int (164 bytes).
    struct PurpleData
    {
        char text[160];
        int  param;
    };

    int DebugPrintf(wchar_t* dst, size_t cch, const wchar_t* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        int n = _vsnwprintf_s(dst, cch, _TRUNCATE, fmt, args);
        va_end(args);
        return n;
    }
}

MsgSender::MsgSender() : m_bOutbound(FALSE), m_hPurpleWnd(nullptr)
{
}

MsgSender::~MsgSender()
{
}

void MsgSender::PopErrorMessage(const wchar_t* text, DWORD code, LPARAM lParam)
{
    OutputDebugStringW(L"MsgSender::PopErrorMessage");
    if (m_bOutbound)
    {
        wchar_t buf[260] = {};
        DebugPrintf(buf, 260, L"PURPLE_OUTBOUND_ENDING parameter is %s, %d", text, code);
        OutputDebugStringW(buf);
        SendPurple(PURPLE_ENDING, text, (int)code);
        if (lParam)
            PostMessageW(GetLauncherMainWnd(), WM_WZ_EXITLAUNCHER, 0, 0);
    }
    else
    {
        SendMessageW(GetLauncherMainWnd(), WM_WZ_POPERROR, (WPARAM)text, lParam);
    }
}

void MsgSender::StartLauncher(WPARAM gameName)
{
    OutputDebugStringW(L"MsgSender::StartLauncher");
    SendMessageW(GetLauncherMainWnd(), WM_WZ_STARTLAUNCHER, gameName, 0);
    if (m_bOutbound)
    {
        OutputDebugStringW(L"Set Clear Outbound Param");
        SendPurple(PURPLE_FILE_PROGRESS, L"", 0);
        SendPurple(PURPLE_UPDATE_PROGRESS, L"", 0);
        SendPurple(PURPLE_FILE_STATIC, L"Game Patch Start..", 0);
        SendPurple(PURPLE_UNKNOWN_1F, L"", 0);
        SendPurple(PURPLE_UNKNOWN_1B, L"", 0);
        SendPurple(PURPLE_FILE_COUNT, L"", 0);
    }
}

void MsgSender::ExitLauncher()
{
    OutputDebugStringW(L"MsgSender::ExitLauncher");
    PostMessageW(GetLauncherMainWnd(), WM_WZ_EXITLAUNCHER, 0, 0);
    if (m_bOutbound)
    {
        SendPurple(PURPLE_FILE_STATIC, L"Complete..", 0);
        SendPurple(PURPLE_ENDING, L"", 0);
    }
}

void MsgSender::SetGameListStateText(const wchar_t* gameName, const wchar_t* text)
{
    OutputDebugStringW(L"MsgSender::SetGameListStateText");
    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETGAMELISTSTATE, (WPARAM)gameName, (LPARAM)text);
    if (m_bOutbound)
        SendPurple(PURPLE_FILE_STATIC, text, 0);
}

void MsgSender::SetUpdateProgress(int percent)
{
    wchar_t buf[100];
    OutputDebugStringW(L"MsgSender::SetUpdateProgress");
    memset(buf, 0, sizeof(buf));
    DebugPrintf(buf, 100, L"Update Progress : %d", percent);
    OutputDebugStringW(buf);
    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETUPDATEPROGRESS, (WPARAM)percent, 0);
    if (m_bOutbound)
        SendPurple(PURPLE_UPDATE_PROGRESS, L"", percent);
}

void MsgSender::SetFileProgress(int percent)
{
    wchar_t buf[100];
    OutputDebugStringW(L"MsgSender::SetFileProgress");
    memset(buf, 0, sizeof(buf));
    DebugPrintf(buf, 100, L"File Progress : %d", percent);
    OutputDebugStringW(buf);
    if (m_bOutbound)
    {
        OutputDebugStringW(L"Outbound is use!!");
        SendPurple(PURPLE_FILE_PROGRESS, L"", percent);
    }
    else
    {
        OutputDebugStringW(L"Outbound is not use");
        SendMessageW(GetLauncherMainWnd(), WM_WZ_SETFILEPROGRESS, (WPARAM)percent, 0);
    }
}

void MsgSender::SetTotalStatic(const wchar_t* text)
{
    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETTOTALSTATIC, (WPARAM)text, 0);
}

void MsgSender::SetFileStatic(const wchar_t* text)
{
    OutputDebugStringW(L"MsgSender::SetFileStatic");
    SendMessageW(GetLauncherMainWnd(), WM_WZ_SETFILESTATIC, (WPARAM)text, 0);
    if (m_bOutbound)
        SendPurple(PURPLE_FILE_STATIC, text, 0);
}

void MsgSender::ClearOutboundParam()
{
    SendPurple(PURPLE_FILE_PROGRESS, L"", 0);
    SendPurple(PURPLE_UPDATE_PROGRESS, L"", 0);
    SendPurple(PURPLE_FILE_STATIC, L"", 0);
    SendPurple(PURPLE_UNKNOWN_1F, L"", 0);
    SendPurple(PURPLE_UNKNOWN_1B, L"", 0);
    SendPurple(PURPLE_FILE_COUNT, L"", 0);
}

// Inlined,,,, ...
void MsgSender::SetFileCount(unsigned current, unsigned total)
{
    OutputDebugStringW(L"MsgSender::SetFileCount");
    if (m_bOutbound)
        SendPurple(PURPLE_FILE_COUNT, L"", (int)((current & 0xFFFF) | ((total & 0xFFFF) << 16)));
}

void MsgSender::SendPurple(int type, const wchar_t* text, int param)
{
    if (m_hPurpleWnd)
    {
        char ansi[260];
        PurpleData data;
        COPYDATASTRUCT cds;
        wchar_t dbg[100];

        WideCharToMultiByte(CP_ACP, 0, text, -1, ansi, sizeof(ansi), nullptr, nullptr);
        DebugPrintf(dbg, 100, L"MSG Type : %d", type);
        OutputDebugStringW(dbg);

        memset(&data, 0, sizeof(data));
        strncpy_s(data.text, sizeof(data.text), ansi, _TRUNCATE);
        data.param = param;

        cds.dwData = (ULONG_PTR)type;
        cds.cbData = sizeof(data);
        cds.lpData = &data;
        if (SendMessageW(m_hPurpleWnd, WM_COPYDATA, (WPARAM)GetLauncherMainWnd(), (LPARAM)&cds))
            return;
    }

    wchar_t msg[260] = {};
    DebugPrintf(msg, 260, L"Not Exist Target Window(Purple Launcher) Handle");
    SendMessageW(GetLauncherMainWnd(), WM_WZ_POPERROR, (WPARAM)msg, 1);
}
