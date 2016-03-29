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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#include "registry_reg.h"

static Reg* instance = NULL;

static Reg* RegGetInstance()
{
	if (!instance)
	{
		instance = reg_open(1);
	}

	return instance;
}

LONG RegCloseKey(HKEY hKey)
{
	return 0;
}

LONG RegCopyTreeW(HKEY hKeySrc, LPCWSTR lpSubKey, HKEY hKeyDest)
{
	return 0;
}

LONG RegCopyTreeA(HKEY hKeySrc, LPCSTR lpSubKey, HKEY hKeyDest)
{
	return 0;
}

LONG RegCreateKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions,
		REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition)
{
	return 0;
}

LONG RegCreateKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD Reserved, LPSTR lpClass, DWORD dwOptions,
		REGSAM samDesired, LPSECURITY_ATTRIBUTES lpSecurityAttributes, PHKEY phkResult, LPDWORD lpdwDisposition)
{
	return 0;
}

LONG RegDeleteKeyExW(HKEY hKey, LPCWSTR lpSubKey, REGSAM samDesired, DWORD Reserved)
{
	return 0;
}

LONG RegDeleteKeyExA(HKEY hKey, LPCSTR lpSubKey, REGSAM samDesired, DWORD Reserved)
{
	return 0;
}

LONG RegDeleteTreeW(HKEY hKey, LPCWSTR lpSubKey)
{
	return 0;
}

LONG RegDeleteTreeA(HKEY hKey, LPCSTR lpSubKey)
{
	return 0;
}

LONG RegDeleteValueW(HKEY hKey, LPCWSTR lpValueName)
{
	return 0;
}

LONG RegDeleteValueA(HKEY hKey, LPCSTR lpValueName)
{
	return 0;
}

LONG RegDisablePredefinedCacheEx(void)
{
	return 0;
}

LONG RegEnumKeyExW(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcName,
		LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcClass, PFILETIME lpftLastWriteTime)
{
	return 0;
}

LONG RegEnumKeyExA(HKEY hKey, DWORD dwIndex, LPSTR lpName, LPDWORD lpcName,
		LPDWORD lpReserved, LPSTR lpClass, LPDWORD lpcClass, PFILETIME lpftLastWriteTime)
{
	return 0;
}

LONG RegEnumValueW(HKEY hKey, DWORD dwIndex, LPWSTR lpValueName,
		LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	return 0;
}

LONG RegEnumValueA(HKEY hKey, DWORD dwIndex, LPSTR lpValueName,
		LPDWORD lpcchValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	return 0;
}

LONG RegFlushKey(HKEY hKey)
{
	return 0;
}

LONG RegGetKeySecurity(HKEY hKey, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, LPDWORD lpcbSecurityDescriptor)
{
	return 0;
}

LONG RegGetValueW(HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpValue,
		DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData)
{
	return 0;
}

LONG RegGetValueA(HKEY hkey, LPCSTR lpSubKey, LPCSTR lpValue,
		DWORD dwFlags, LPDWORD pdwType, PVOID pvData, LPDWORD pcbData)
{
	return 0;
}

LONG RegLoadAppKeyW(LPCWSTR lpFile, PHKEY phkResult,
		REGSAM samDesired, DWORD dwOptions, DWORD Reserved)
{
	return 0;
}

LONG RegLoadAppKeyA(LPCSTR lpFile, PHKEY phkResult,
		REGSAM samDesired, DWORD dwOptions, DWORD Reserved)
{
	return 0;
}

LONG RegLoadKeyW(HKEY hKey, LPCWSTR lpSubKey, LPCWSTR lpFile)
{
	return 0;
}

LONG RegLoadKeyA(HKEY hKey, LPCSTR lpSubKey, LPCSTR lpFile)
{
	return 0;
}

LONG RegLoadMUIStringW(HKEY hKey, LPCWSTR pszValue, LPWSTR pszOutBuf,
		DWORD cbOutBuf, LPDWORD pcbData, DWORD Flags, LPCWSTR pszDirectory)
{
	return 0;
}

LONG RegLoadMUIStringA(HKEY hKey, LPCSTR pszValue, LPSTR pszOutBuf,
		DWORD cbOutBuf, LPDWORD pcbData, DWORD Flags, LPCSTR pszDirectory)
{
	return 0;
}

LONG RegNotifyChangeKeyValue(HKEY hKey, BOOL bWatchSubtree, DWORD dwNotifyFilter, HANDLE hEvent, BOOL fAsynchronous)
{
	return 0;
}

LONG RegOpenCurrentUser(REGSAM samDesired, PHKEY phkResult)
{
	return 0;
}

LONG RegOpenKeyExW(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	return 0;
}

LONG RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	Reg* reg;
	RegKey* pKey;

	reg = RegGetInstance();

	if (!reg)
		return -1;

	pKey = reg->root_key->subkeys;

	while (pKey != NULL)
	{
		if (_stricmp(pKey->subname, lpSubKey) == 0)
		{
			*phkResult = (HKEY) pKey;
			return ERROR_SUCCESS;
		}

		pKey = pKey->next;
	}

	*phkResult = NULL;

	return ERROR_FILE_NOT_FOUND;
}

LONG RegOpenUserClassesRoot(HANDLE hToken, DWORD dwOptions, REGSAM samDesired, PHKEY phkResult)
{
	return 0;
}

LONG RegQueryInfoKeyW(HKEY hKey, LPWSTR lpClass, LPDWORD lpcClass, LPDWORD lpReserved,
		LPDWORD lpcSubKeys, LPDWORD lpcMaxSubKeyLen, LPDWORD lpcMaxClassLen,
		LPDWORD lpcValues, LPDWORD lpcMaxValueNameLen, LPDWORD lpcMaxValueLen,
		LPDWORD lpcbSecurityDescriptor, PFILETIME lpftLastWriteTime)
{
	return 0;
}

LONG RegQueryInfoKeyA(HKEY hKey, LPSTR lpClass, LPDWORD lpcClass, LPDWORD lpReserved,
		LPDWORD lpcSubKeys, LPDWORD lpcMaxSubKeyLen, LPDWORD lpcMaxClassLen,
		LPDWORD lpcValues, LPDWORD lpcMaxValueNameLen, LPDWORD lpcMaxValueLen,
		LPDWORD lpcbSecurityDescriptor, PFILETIME lpftLastWriteTime)
{
	return 0;
}

LONG RegQueryValueExW(HKEY hKey, LPCWSTR lpValueName,
		LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	return ERROR_FILE_NOT_FOUND;
}

LONG RegQueryValueExA(HKEY hKey, LPCSTR lpValueName,
		LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	RegKey* key;
	RegVal* pValue;

	key = (RegKey*) hKey;
	pValue = key->values;

	while (pValue != NULL)
	{
		if (strcmp(pValue->name, lpValueName) == 0)
		{
			if (pValue->type == REG_DWORD)
			{
				DWORD* pData = (DWORD*) lpData;

				if (pData != NULL)
				{
					*pData = pValue->data.dword;
				}

				*lpcbData = sizeof(DWORD);

				return ERROR_SUCCESS;
			}
			else if (pValue->type == REG_SZ)
			{
				int length;
				char* pData = (char*) lpData;

				length = (int) strlen(pValue->data.string);

				if (pData != NULL)
				{
					memcpy(pData, pValue->data.string, length);
					pData[length] = '\0';
				}

				*lpcbData = length;

				return ERROR_SUCCESS;
			}
		}

		pValue = pValue->next;
	}

	return ERROR_FILE_NOT_FOUND;
}

LONG RegRestoreKeyW(HKEY hKey, LPCWSTR lpFile, DWORD dwFlags)
{
	return 0;
}

LONG RegRestoreKeyA(HKEY hKey, LPCSTR lpFile, DWORD dwFlags)
{
	return 0;
}

LONG RegSaveKeyExW(HKEY hKey, LPCWSTR lpFile, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD Flags)
{
	return 0;
}

LONG RegSaveKeyExA(HKEY hKey, LPCSTR lpFile, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD Flags)
{
	return 0;
}

LONG RegSetKeySecurity(HKEY hKey, SECURITY_INFORMATION SecurityInformation, PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	return 0;
}

LONG RegSetValueExW(HKEY hKey, LPCWSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData)
{
	return 0;
}

LONG RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData)
{
	return 0;
}

LONG RegUnLoadKeyW(HKEY hKey, LPCWSTR lpSubKey)
{
	return 0;
}

LONG RegUnLoadKeyA(HKEY hKey, LPCSTR lpSubKey)
{
	return 0;
}

#endif
