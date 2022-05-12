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

#include "registry_reg.h"

#include "../log.h"
#define TAG WINPR_TAG("registry")

static Reg* instance = NULL;

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

LONG RegCopyTreeW(HKEY hKeySrc, LPCWSTR lpSubKey, HKEY hKeyDest)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegCopyTreeA(HKEY hKeySrc, LPCSTR lpSubKey, HKEY hKeyDest)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions,
                     REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult,
                     LPDWORD lpdwDisposition)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions,
                     REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult,
                     LPDWORD lpdwDisposition)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegDeleteKeyExW(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegDeleteKeyExA(HKEY hKey, LPCSTR lpSubKey, REGSAM samDesired, DWORD Reserved)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegDeleteTreeW(HKEY hKey, LPCWSTR lpSubKey)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegDeleteTreeA(HKEY hKey, LPCSTR lpSubKey)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegDeleteValueW(HKEY hKey, LPCWSTR lpValueName)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegDeleteValueA(HKEY hKey, LPCSTR lpValueName)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegDisablePredefinedCacheEx(void)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcName, LPDWORD lpReserved,
                   LPWSTR lpClass, LPDWORD lpcClass, PFILETIME lpftLastWriteTime)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcName, LPDWORD lpReserved,
                   LPSTR lpClass, LPDWORD lpcClass, PFILETIME lpftLastWriteTime)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName, LPDWORD lpcchValueName,
                   LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegEnumValueA(HKEY hKey, DWORD dwIndex, LPSTR lpValueName, LPDWORD lpcchValueName,
                   LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegFlushKey(HKEY hKey)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegGetKeySecurity(HKEY hKey, SECURITY_INFORMATION SecurityInformation,
                       PSECURITY_DESCRIPTOR pSecurityDescriptor, LPDWORD lpcbSecurityDescriptor)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue, DWORD dwFlags, LPDWORD pdwType,
                  PVOID pvData, LPDWORD pcbData)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegGetValueA(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue, DWORD dwFlags, LPDWORD pdwType,
                  PVOID pvData, LPDWORD pcbData)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegLoadAppKeyW(LPCWSTR lpFile, PHKEY phkResult, REGSAM samDesired, DWORD dwOptions,
                    DWORD Reserved)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegLoadAppKeyA(LPCSTR lpFile, PHKEY phkResult, REGSAM samDesired, DWORD dwOptions,
                    DWORD Reserved)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegLoadKeyW(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpFile)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegLoadKeyA(HKEY hKey, LPCSTR lpSubKey, LPCSTR lpFile)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegLoadMUIStringW(HKEY hKey, LPCWSTR pszValue, LPWSTR pszOutBuf, DWORD cbOutBuf,
                       LPDWORD pcbData, DWORD Flags, LPCWSTR pszDirectory)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegLoadMUIStringA(HKEY hKey, LPCSTR pszValue, LPSTR pszOutBuf, DWORD cbOutBuf, LPDWORD pcbData,
                       DWORD Flags, LPCSTR pszDirectory)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegNotifyChangeKeyValue(HKEY hKey, BOOL bWatchSubtree, DWORD dwNotifyFilter, HANDLE hEvent,
                             BOOL fAsynchronous)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegOpenCurrentUser(REGSAM samDesired, PHKEY phkResult)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	LONG rc;
	char* str = NULL;
	int res = ConvertFromUnicode(CP_UTF8, 0, lpSubKey, -1, &str, 0, NULL, NULL);
	if (res < 0)
		return ERROR_FILE_NOT_FOUND;

	rc = RegOpenKeyExA(hKey, str, ulOptions, samDesired, phkResult);
	free(str);
	return rc;
}

LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	RegKey* pKey;
	Reg* reg = RegGetInstance();

	if (!reg)
		return -1;

	if (hKey != HKEY_LOCAL_MACHINE)
	{
		WLog_WARN(TAG, "Registry emulation only supports HKEY_LOCAL_MACHINE");
		return ERROR_FILE_NOT_FOUND;
	}

	WINPR_ASSERT(reg->root_key);
	pKey = reg->root_key->subkeys;

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

LONG RegOpenUserClassesRoot(HANDLE hToken, DWORD dwOptions, REGSAM samDesired, PHKEY phkResult)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegQueryInfoKeyW(HKEY hKey, LPWSTR lpClass, LPDWORD lpcClass, LPDWORD lpReserved,
                      LPDWORD lpcSubKeys, LPDWORD lpcMaxSubKeyLen, LPDWORD lpcMaxClassLen,
                      LPDWORD lpcValues, LPDWORD lpcMaxValueNameLen, LPDWORD lpcMaxValueLen,
                      LPDWORD lpcbSecurityDescriptor, PFILETIME lpftLastWriteTime)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegQueryInfoKeyA(HKEY hKey, LPSTR lpClass, LPDWORD lpcClass, LPDWORD lpReserved,
                      LPDWORD lpcSubKeys, LPDWORD lpcMaxSubKeyLen, LPDWORD lpcMaxClassLen,
                      LPDWORD lpcValues, LPDWORD lpcMaxValueNameLen, LPDWORD lpcMaxValueLen,
                      LPDWORD lpcbSecurityDescriptor, PFILETIME lpftLastWriteTime)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

static LONG reg_read_int(const RegVal* pValue, LPBYTE lpData, LPDWORD lpcbData)
{
	const BYTE* ptr = NULL;
	DWORD required;

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
		DWORD size;
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

LONG RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                      LPBYTE lpData, LPDWORD lpcbData)
{
	LONG status = ERROR_FILE_NOT_FOUND;
	RegKey* key;
	RegVal* pValue;
	char* valueName = NULL;

	WINPR_UNUSED(lpReserved);

	key = (RegKey*)hKey;
	WINPR_ASSERT(key);

	if (ConvertFromUnicode(CP_UTF8, 0, lpValueName, -1, &valueName, 0, NULL, NULL) <= 0)
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
					return reg_read_int(pValue, lpData, lpcbData);
				case REG_SZ:
				{
					const size_t length = strnlen(pValue->data.string, UINT32_MAX) * sizeof(WCHAR);

					if (lpData != NULL)
					{
						DWORD size;
						WINPR_ASSERT(lpcbData);

						size = *lpcbData;
						*lpcbData = (DWORD)length;
						if (size < length)
							return ERROR_MORE_DATA;
						ConvertToUnicode(CP_UTF8, 0, pValue->data.string, length, (WCHAR**)&lpData,
						                 length);
					}
					else if (lpcbData)
						*lpcbData = (UINT32)length;

					status = ERROR_SUCCESS;
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

LONG RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                      LPBYTE lpData, LPDWORD lpcbData)
{
	RegKey* key;
	RegVal* pValue;

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
					const size_t length = strnlen(pValue->data.string, UINT32_MAX);
					char* pData = (char*)lpData;

					if (pData != NULL)
					{
						DWORD size;
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

LONG RegRestoreKeyW(HKEY hKey, LPCWSTR lpFile, DWORD dwFlags)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegRestoreKeyA(HKEY hKey, LPCSTR lpFile, DWORD dwFlags)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegSaveKeyExW(HKEY hKey, LPCWSTR lpFile, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   DWORD Flags)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegSaveKeyExA(HKEY hKey, LPCSTR lpFile, LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                   DWORD Flags)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegSetKeySecurity(HKEY hKey, SECURITY_INFORMATION SecurityInformation,
                       PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType,
                    const BYTE* lpData, DWORD cbData)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData,
                    DWORD cbData)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegUnLoadKeyW(HKEY hKey, LPCWSTR lpSubKey)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

LONG RegUnLoadKeyA(HKEY hKey, LPCSTR lpSubKey)
{
	WLog_ERR(TAG, "TODO: Implement %s", __FUNCTION__);
	return -1;
}

#endif
