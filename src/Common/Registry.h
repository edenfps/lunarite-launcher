// Registry.h - registry helpers.
#pragma once
#include "WZCommon.h"

// "HKEY_CURRENT_USER" etc. -> predefined HKEY (unknown names -> HKEY_LOCAL_MACHINE).
HKEY WZRegRootFromName(const wchar_t* rootName);

// Returns TRUE on success. Reads 4 bytes (REG_DWORD).
BOOL WZRegReadDword(const wchar_t* rootName, const wchar_t* subKey, const wchar_t* valueName, DWORD* data);

// Queries with a 259-byte buffer; on failure the buffer is zeroed.
BOOL WZRegReadString(const wchar_t* rootName, const wchar_t* subKey, const wchar_t* valueName, BYTE* data);

// 
// Opens an existing key (no creation) and writes a REG_DWORD.
BOOL WZRegWriteDword(const wchar_t* rootName, const wchar_t* subKey, const wchar_t* valueName, DWORD value);

// Creates the key if needed and writes a REG_SZ.
BOOL WZRegWriteString(const wchar_t* rootName, const wchar_t* subKey, const wchar_t* valueName, const wchar_t* value);
