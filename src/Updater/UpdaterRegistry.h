// UpdaterRegistry.h - DWORD registry helpers the updater uses.
#pragma once
#include "Common/WZCommon.h"
#include <cwchar>

namespace UpdaterRegistry
{
    // root key name to HKEY; unknown names map to HKEY_LOCAL_MACHINE.
    inline HKEY ParseRootKey(const wchar_t* name)
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

    // reads a 4-byte value; false when the key or value is missing.
    inline bool GetDword(const wchar_t* subKey, const wchar_t* rootName, const wchar_t* valueName, DWORD* out)
    {
        HKEY key = nullptr;
        DWORD size = 4, type = 0;
        if (RegOpenKeyExW(ParseRootKey(rootName), subKey, 0, KEY_READ, &key) != ERROR_SUCCESS ||
            RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(out), &size) != ERROR_SUCCESS)
        {
            RegCloseKey(key);
            return false;
        }
        RegCloseKey(key);
        return true;
    }

    // writes a REG_DWORD; the key must already exist.
    inline bool SetDword(const wchar_t* subKey, const wchar_t* rootName, const wchar_t* valueName, DWORD value)
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(ParseRootKey(rootName), subKey, 0, KEY_SET_VALUE | KEY_CREATE_SUB_KEY, &key) != ERROR_SUCCESS ||
            RegSetValueExW(key, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), 4) != ERROR_SUCCESS)
        {
            RegCloseKey(key);
            return false;
        }
        RegCloseKey(key);
        return true;
    }
}
