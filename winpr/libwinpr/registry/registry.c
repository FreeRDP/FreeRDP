/**
 * WinPR: Windows Portable Runtime
 * Windows Registry
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/config.h>

#include <winpr/registry.h>

/*
 * Windows registry MSDN pages:
 * Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/ms724880/
 * Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/ms724875/
 */

#if !defined(_WIN32) || defined(_UWP)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/string.h>

#include "registry_reg.h"

#include "../log.h"
#define TAG WINPR_TAG("registry")

static Reg* instance = NULL;

static size_t regsz_length(const char* key, const void* value, bool unicode)
{
	/* https://learn.microsoft.com/en-us/windows/win32/sysinfo/registry-element-size-limits
	 *
	 * while not strictly limited to this size larger values should be stored to a
	 * file.
	 */
	const size_t limit = 16383;
	size_t length = 0;
	if (unicode)
		length = _wcsnlen((const WCHAR*)value, limit);
	else
		length = strnlen((const char*)value, limit);
	if (length >= limit)
		WLog_WARN(TAG, "REG_SZ[%s] truncated to size %" PRIuz, key, length);
	return length;
}

static Reg* RegGetInstance(void)
{
	if (!instance)
		instance = reg_open(1);

	return instance;
}

LONG RegCloseKey(HKEY hKey)
{
	WINPR_UNUSED(hKey);
	return 0;
}

LONG RegCopyTreeW(WINPR_ATTR_UNUSED HKEY hKeySrc, WINPR_ATTR_UNUSED LPCWSTR lpSubKey,
                  WINPR_ATTR_UNUSED HKEY hKeyDest)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegCopyTreeA(WINPR_ATTR_UNUSED HKEY hKeySrc, WINPR_ATTR_UNUSED LPCSTR lpSubKey,
                  WINPR_ATTR_UNUSED HKEY hKeyDest)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegCreateKeyExW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpSubKey,
                     WINPR_ATTR_UNUSED DWORD Reserved, WINPR_ATTR_UNUSED LPWSTR lpClass,
                     WINPR_ATTR_UNUSED DWORD dwOptions, WINPR_ATTR_UNUSED REGSAM samDesired,
                     WINPR_ATTR_UNUSED LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                     WINPR_ATTR_UNUSED PHKEY phkResult, WINPR_ATTR_UNUSED LPDWORD lpdwDisposition)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegCreateKeyExA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpSubKey,
                     WINPR_ATTR_UNUSED DWORD Reserved, WINPR_ATTR_UNUSED LPSTR lpClass,
                     WINPR_ATTR_UNUSED DWORD dwOptions, WINPR_ATTR_UNUSED REGSAM samDesired,
                     WINPR_ATTR_UNUSED LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                     WINPR_ATTR_UNUSED PHKEY phkResult, WINPR_ATTR_UNUSED LPDWORD lpdwDisposition)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegDeleteKeyExW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpSubKey,
                     WINPR_ATTR_UNUSED REGSAM samDesired, WINPR_ATTR_UNUSED DWORD Reserved)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegDeleteKeyExA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpSubKey,
                     WINPR_ATTR_UNUSED REGSAM samDesired, WINPR_ATTR_UNUSED DWORD Reserved)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegDeleteTreeW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpSubKey)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegDeleteTreeA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpSubKey)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegDeleteValueW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpValueName)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegDeleteValueA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpValueName)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegDisablePredefinedCacheEx(void)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegEnumKeyExW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED DWORD dwIndex,
                   WINPR_ATTR_UNUSED LPWSTR lpName, WINPR_ATTR_UNUSED LPDWORD lpcName,
                   WINPR_ATTR_UNUSED LPDWORD lpReserved, WINPR_ATTR_UNUSED LPWSTR lpClass,
                   WINPR_ATTR_UNUSED LPDWORD lpcClass,
                   WINPR_ATTR_UNUSED PFILETIME lpftLastWriteTime)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegEnumKeyExA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED DWORD dwIndex,
                   WINPR_ATTR_UNUSED LPSTR lpName, WINPR_ATTR_UNUSED LPDWORD lpcName,
                   WINPR_ATTR_UNUSED LPDWORD lpReserved, WINPR_ATTR_UNUSED LPSTR lpClass,
                   WINPR_ATTR_UNUSED LPDWORD lpcClass,
                   WINPR_ATTR_UNUSED PFILETIME lpftLastWriteTime)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegEnumValueW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED DWORD dwIndex,
                   WINPR_ATTR_UNUSED LPWSTR lpValueName, WINPR_ATTR_UNUSED LPDWORD lpcchValueName,
                   WINPR_ATTR_UNUSED LPDWORD lpReserved, WINPR_ATTR_UNUSED LPDWORD lpType,
                   WINPR_ATTR_UNUSED LPBYTE lpData, WINPR_ATTR_UNUSED LPDWORD lpcbData)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegEnumValueA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED DWORD dwIndex,
                   WINPR_ATTR_UNUSED LPSTR lpValueName, WINPR_ATTR_UNUSED LPDWORD lpcchValueName,
                   WINPR_ATTR_UNUSED LPDWORD lpReserved, WINPR_ATTR_UNUSED LPDWORD lpType,
                   WINPR_ATTR_UNUSED LPBYTE lpData, WINPR_ATTR_UNUSED LPDWORD lpcbData)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegFlushKey(WINPR_ATTR_UNUSED HKEY hKey)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegGetKeySecurity(WINPR_ATTR_UNUSED HKEY hKey,
                       WINPR_ATTR_UNUSED SECURITY_INFORMATION SecurityInformation,
                       WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor,
                       WINPR_ATTR_UNUSED LPDWORD lpcbSecurityDescriptor)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegGetValueW(WINPR_ATTR_UNUSED HKEY hkey, WINPR_ATTR_UNUSED LPCWSTR lpSubKey,
                  WINPR_ATTR_UNUSED LPCWSTR lpValue, WINPR_ATTR_UNUSED DWORD dwFlags,
                  WINPR_ATTR_UNUSED LPDWORD pdwType, WINPR_ATTR_UNUSED PVOID pvData,
                  WINPR_ATTR_UNUSED LPDWORD pcbData)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegGetValueA(WINPR_ATTR_UNUSED HKEY hkey, WINPR_ATTR_UNUSED LPCSTR lpSubKey,
                  WINPR_ATTR_UNUSED LPCSTR lpValue, WINPR_ATTR_UNUSED DWORD dwFlags,
                  WINPR_ATTR_UNUSED LPDWORD pdwType, WINPR_ATTR_UNUSED PVOID pvData,
                  WINPR_ATTR_UNUSED LPDWORD pcbData)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegLoadAppKeyW(WINPR_ATTR_UNUSED LPCWSTR lpFile, WINPR_ATTR_UNUSED PHKEY phkResult,
                    WINPR_ATTR_UNUSED REGSAM samDesired, WINPR_ATTR_UNUSED DWORD dwOptions,
                    WINPR_ATTR_UNUSED DWORD Reserved)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegLoadAppKeyA(WINPR_ATTR_UNUSED LPCSTR lpFile, WINPR_ATTR_UNUSED PHKEY phkResult,
                    WINPR_ATTR_UNUSED REGSAM samDesired, WINPR_ATTR_UNUSED DWORD dwOptions,
                    WINPR_ATTR_UNUSED DWORD Reserved)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegLoadKeyW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpSubKey,
                 WINPR_ATTR_UNUSED LPCWSTR lpFile)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegLoadKeyA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpSubKey,
                 WINPR_ATTR_UNUSED LPCSTR lpFile)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegLoadMUIStringW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR pszValue,
                       WINPR_ATTR_UNUSED LPWSTR pszOutBuf, WINPR_ATTR_UNUSED DWORD cbOutBuf,
                       WINPR_ATTR_UNUSED LPDWORD pcbData, WINPR_ATTR_UNUSED DWORD Flags,
                       WINPR_ATTR_UNUSED LPCWSTR pszDirectory)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegLoadMUIStringA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR pszValue,
                       WINPR_ATTR_UNUSED LPSTR pszOutBuf, WINPR_ATTR_UNUSED DWORD cbOutBuf,
                       WINPR_ATTR_UNUSED LPDWORD pcbData, WINPR_ATTR_UNUSED DWORD Flags,
                       WINPR_ATTR_UNUSED LPCSTR pszDirectory)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegNotifyChangeKeyValue(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED BOOL bWatchSubtree,
                             WINPR_ATTR_UNUSED DWORD dwNotifyFilter,
                             WINPR_ATTR_UNUSED HANDLE hEvent, WINPR_ATTR_UNUSED BOOL fAsynchronous)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegOpenCurrentUser(WINPR_ATTR_UNUSED REGSAM samDesired, WINPR_ATTR_UNUSED PHKEY phkResult)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	LONG rc = 0;
	char* str = ConvertWCharToUtf8Alloc(lpSubKey, NULL);
	if (!str)
		return ERROR_FILE_NOT_FOUND;

	rc = RegOpenKeyExA(hKey, str, ulOptions, samDesired, phkResult);
	free(str);
	return rc;
}

LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, WINPR_ATTR_UNUSED DWORD ulOptions,
                   WINPR_ATTR_UNUSED REGSAM samDesired, PHKEY phkResult)
{
	Reg* reg = RegGetInstance();

	if (!reg)
		return -1;

	if (hKey != HKEY_LOCAL_MACHINE)
	{
		WLog_WARN(TAG, "Registry emulation only supports HKEY_LOCAL_MACHINE");
		return ERROR_FILE_NOT_FOUND;
	}

	WINPR_ASSERT(reg->root_key);
	RegKey* pKey = reg->root_key->subkeys;

	while (pKey != NULL)
	{
		WINPR_ASSERT(lpSubKey);

		if (pKey->subname && (_stricmp(pKey->subname, lpSubKey) == 0))
		{
			*phkResult = (HKEY)pKey;
			return ERROR_SUCCESS;
		}

		pKey = pKey->next;
	}

	*phkResult = NULL;

	return ERROR_FILE_NOT_FOUND;
}

LONG RegOpenUserClassesRoot(WINPR_ATTR_UNUSED HANDLE hToken, WINPR_ATTR_UNUSED DWORD dwOptions,
                            WINPR_ATTR_UNUSED REGSAM samDesired, WINPR_ATTR_UNUSED PHKEY phkResult)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegQueryInfoKeyW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPWSTR lpClass,
                      WINPR_ATTR_UNUSED LPDWORD lpcClass, WINPR_ATTR_UNUSED LPDWORD lpReserved,
                      WINPR_ATTR_UNUSED LPDWORD lpcSubKeys,
                      WINPR_ATTR_UNUSED LPDWORD lpcMaxSubKeyLen,
                      WINPR_ATTR_UNUSED LPDWORD lpcMaxClassLen, WINPR_ATTR_UNUSED LPDWORD lpcValues,
                      WINPR_ATTR_UNUSED LPDWORD lpcMaxValueNameLen,
                      WINPR_ATTR_UNUSED LPDWORD lpcMaxValueLen,
                      WINPR_ATTR_UNUSED LPDWORD lpcbSecurityDescriptor,
                      WINPR_ATTR_UNUSED PFILETIME lpftLastWriteTime)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegQueryInfoKeyA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPSTR lpClass,
                      WINPR_ATTR_UNUSED LPDWORD lpcClass, WINPR_ATTR_UNUSED LPDWORD lpReserved,
                      WINPR_ATTR_UNUSED LPDWORD lpcSubKeys,
                      WINPR_ATTR_UNUSED LPDWORD lpcMaxSubKeyLen,
                      WINPR_ATTR_UNUSED LPDWORD lpcMaxClassLen, WINPR_ATTR_UNUSED LPDWORD lpcValues,
                      WINPR_ATTR_UNUSED LPDWORD lpcMaxValueNameLen,
                      WINPR_ATTR_UNUSED LPDWORD lpcMaxValueLen,
                      WINPR_ATTR_UNUSED LPDWORD lpcbSecurityDescriptor,
                      WINPR_ATTR_UNUSED PFILETIME lpftLastWriteTime)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

static LONG reg_read_int(const RegVal* pValue, LPBYTE lpData, LPDWORD lpcbData)
{
	const BYTE* ptr = NULL;
	DWORD required = 0;

	WINPR_ASSERT(pValue);

	switch (pValue->type)
	{
		case REG_DWORD:
		case REG_DWORD_BIG_ENDIAN:
			required = sizeof(DWORD);
			ptr = (const BYTE*)&pValue->data.dword;
			break;
		case REG_QWORD:
			required = sizeof(UINT64);
			ptr = (const BYTE*)&pValue->data.qword;
			break;
		default:
			return ERROR_INTERNAL_ERROR;
	}

	if (lpcbData)
	{
		DWORD size = *lpcbData;
		*lpcbData = required;
		if (lpData)
		{
			if (size < *lpcbData)
				return ERROR_MORE_DATA;
		}
	}

	if (lpData != NULL)
	{
		DWORD size = 0;
		WINPR_ASSERT(lpcbData);

		size = *lpcbData;
		*lpcbData = required;
		if (size < required)
			return ERROR_MORE_DATA;
		memcpy(lpData, ptr, required);
	}
	else if (lpcbData != NULL)
		*lpcbData = required;
	return ERROR_SUCCESS;
}

// NOLINTBEGIN(readability-non-const-parameter)
LONG RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                      LPBYTE lpData, LPDWORD lpcbData)
// NOLINTEND(readability-non-const-parameter)
{
	LONG status = ERROR_FILE_NOT_FOUND;
	RegKey* key = NULL;
	RegVal* pValue = NULL;
	char* valueName = NULL;

	WINPR_UNUSED(lpReserved);

	key = (RegKey*)hKey;
	WINPR_ASSERT(key);

	valueName = ConvertWCharToUtf8Alloc(lpValueName, NULL);
	if (!valueName)
		goto end;

	pValue = key->values;

	while (pValue != NULL)
	{
		if (strcmp(pValue->name, valueName) == 0)
		{
			if (lpType)
				*lpType = pValue->type;

			switch (pValue->type)
			{
				case REG_DWORD_BIG_ENDIAN:
				case REG_QWORD:
				case REG_DWORD:
					status = reg_read_int(pValue, lpData, lpcbData);
					goto end;
				case REG_SZ:
				{
					const size_t length =
					    regsz_length(pValue->name, pValue->data.string, TRUE) * sizeof(WCHAR);

					status = ERROR_SUCCESS;
					if (lpData != NULL)
					{
						DWORD size = 0;
						union
						{
							WCHAR* wc;
							BYTE* b;
						} cnv;
						WINPR_ASSERT(lpcbData);

						cnv.b = lpData;
						size = *lpcbData;
						*lpcbData = (DWORD)length;
						if (size < length)
							status = ERROR_MORE_DATA;
						if (ConvertUtf8NToWChar(pValue->data.string, length, cnv.wc, length) < 0)
							status = ERROR_OUTOFMEMORY;
					}
					else if (lpcbData)
						*lpcbData = (UINT32)length;

					goto end;
				}
				default:
					WLog_WARN(TAG,
					          "Registry emulation does not support value type %s [0x%08" PRIx32 "]",
					          reg_type_string(pValue->type), pValue->type);
					break;
			}
		}

		pValue = pValue->next;
	}

end:
	free(valueName);
	return status;
}

// NOLINTBEGIN(readability-non-const-parameter)
LONG RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                      LPBYTE lpData, LPDWORD lpcbData)
// NOLINTEND(readability-non-const-parameter)
{
	RegKey* key = NULL;
	RegVal* pValue = NULL;

	WINPR_UNUSED(lpReserved);

	key = (RegKey*)hKey;
	WINPR_ASSERT(key);

	pValue = key->values;

	while (pValue != NULL)
	{
		if (strcmp(pValue->name, lpValueName) == 0)
		{
			if (lpType)
				*lpType = pValue->type;

			switch (pValue->type)
			{
				case REG_DWORD_BIG_ENDIAN:
				case REG_QWORD:
				case REG_DWORD:
					return reg_read_int(pValue, lpData, lpcbData);
				case REG_SZ:
				{
					const size_t length = regsz_length(pValue->name, pValue->data.string, FALSE);

					char* pData = (char*)lpData;

					if (pData != NULL)
					{
						DWORD size = 0;
						WINPR_ASSERT(lpcbData);

						size = *lpcbData;
						*lpcbData = (DWORD)length;
						if (size < length)
							return ERROR_MORE_DATA;
						memcpy(pData, pValue->data.string, length);
						pData[length] = '\0';
					}
					else if (lpcbData)
						*lpcbData = (UINT32)length;

					return ERROR_SUCCESS;
				}
				default:
					WLog_WARN(TAG,
					          "Registry emulation does not support value type %s [0x%08" PRIx32 "]",
					          reg_type_string(pValue->type), pValue->type);
					break;
			}
		}

		pValue = pValue->next;
	}

	return ERROR_FILE_NOT_FOUND;
}

LONG RegRestoreKeyW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpFile,
                    WINPR_ATTR_UNUSED DWORD dwFlags)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegRestoreKeyA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpFile,
                    WINPR_ATTR_UNUSED DWORD dwFlags)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegSaveKeyExW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpFile,
                   WINPR_ATTR_UNUSED LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   WINPR_ATTR_UNUSED DWORD Flags)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegSaveKeyExA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpFile,
                   WINPR_ATTR_UNUSED LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   WINPR_ATTR_UNUSED DWORD Flags)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegSetKeySecurity(WINPR_ATTR_UNUSED HKEY hKey,
                       WINPR_ATTR_UNUSED SECURITY_INFORMATION SecurityInformation,
                       WINPR_ATTR_UNUSED PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegSetValueExW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpValueName,
                    WINPR_ATTR_UNUSED DWORD Reserved, WINPR_ATTR_UNUSED DWORD dwType,
                    WINPR_ATTR_UNUSED const BYTE* lpData, WINPR_ATTR_UNUSED DWORD cbData)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegSetValueExA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpValueName,
                    WINPR_ATTR_UNUSED DWORD Reserved, WINPR_ATTR_UNUSED DWORD dwType,
                    WINPR_ATTR_UNUSED const BYTE* lpData, WINPR_ATTR_UNUSED DWORD cbData)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegUnLoadKeyW(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCWSTR lpSubKey)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

LONG RegUnLoadKeyA(WINPR_ATTR_UNUSED HKEY hKey, WINPR_ATTR_UNUSED LPCSTR lpSubKey)
{
	WLog_ERR(TAG, "TODO: Implement");
	return -1;
}

#endif
