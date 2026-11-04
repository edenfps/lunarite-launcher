#include "Registry.h"
#include <wchar.h>

HKEY WZRegRootFromName(const wchar_t* name)
{
    if (!_wcsicmp(name, L"HKEY_CLASSES_ROOT"))        return HKEY_CLASSES_ROOT;
    if (!_wcsicmp(name, L"HKEY_CURRENT_USER"))        return HKEY_CURRENT_USER;
    if (!_wcsicmp(name, L"HKEY_LOCAL_MACHINE"))       return HKEY_LOCAL_MACHINE;
    if (!_wcsicmp(name, L"HKEY_USERS"))               return HKEY_USERS;
    if (!_wcsicmp(name, L"HKEY_PERFORMANCE_DATA"))    return HKEY_PERFORMANCE_DATA;
    if (!_wcsicmp(name, L"HKEY_PERFORMANCE_TEXT"))    return HKEY_PERFORMANCE_TEXT;
    if (!_wcsicmp(name, L"HKEY_PERFORMANCE_NLSTEXT")) return HKEY_PERFORMANCE_NLSTEXT;
    if (!_wcsicmp(name, L"HKEY_CURRENT_CONFIG"))      return HKEY_CURRENT_CONFIG;
    if (!_wcsicmp(name, L"HKEY_DYN_DATA"))            return HKEY_DYN_DATA;
    return HKEY_LOCAL_MACHINE;
}

BOOL WZRegReadDword(const wchar_t* rootName, const wchar_t* subKey, const wchar_t* valueName, DWORD* data)
{
    DWORD cbData = 4;
    DWORD type;
    HKEY key = nullptr;
    if (RegOpenKeyExW(WZRegRootFromName(rootName), subKey, 0, KEY_READ, &key) != ERROR_SUCCESS ||
        RegQueryValueExW(key, valueName, nullptr, &type, (LPBYTE)data, &cbData) != ERROR_SUCCESS)
    {
        RegCloseKey(key);
        return FALSE;
    }
    RegCloseKey(key);
    return TRUE;
}

BOOL WZRegReadString(const wchar_t* rootName, const wchar_t* subKey, const wchar_t* valueName, BYTE* data)
{
    DWORD cbData = 259;
    DWORD type;
    HKEY key = nullptr;
    if (RegOpenKeyExW(WZRegRootFromName(rootName), subKey, 0, KEY_READ, &key) != ERROR_SUCCESS ||
        RegQueryValueExW(key, valueName, nullptr, &type, data, &cbData) != ERROR_SUCCESS)
    {
        for (DWORD i = 0; i < cbData; ++i)
            data[i] = 0;
        RegCloseKey(key);
        return FALSE;
    }
    RegCloseKey(key);
    return TRUE;
}

BOOL WZRegWriteDword(const wchar_t* rootName, const wchar_t* subKey, const wchar_t* valueName, DWORD value)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(WZRegRootFromName(rootName), subKey, 0, KEY_WRITE, &key) != ERROR_SUCCESS ||
        RegSetValueExW(key, valueName, 0, REG_DWORD, (const BYTE*)&value, 4) != ERROR_SUCCESS)
    {
        RegCloseKey(key);
        return FALSE;
    }
    RegCloseKey(key);
    return TRUE;
}

BOOL WZRegWriteString(const wchar_t* rootName, const wchar_t* subKey, const wchar_t* valueName, const wchar_t* value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(WZRegRootFromName(rootName), subKey, 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return FALSE;
    BOOL ok = RegSetValueExW(key, valueName, 0, REG_SZ, (const BYTE*)value,
                             (DWORD)((wcslen(value) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    RegCloseKey(key);
    return ok;
}
