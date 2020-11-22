/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server
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

#include <winpr/tchar.h>
#include <winpr/windows.h>

#include "wf_settings.h"

BOOL wf_settings_read_dword(HKEY key, LPCSTR subkey, LPTSTR name, UINT32* value)
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;

	if (!value)
		return FALSE;

	status = RegOpenKeyExA(key, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status == ERROR_SUCCESS)
	{
		dwSize = sizeof(DWORD);

		status = RegQueryValueEx(hKey, name, NULL, &dwType, (BYTE*)&dwValue, &dwSize);

		RegCloseKey(hKey);

		if (status != ERROR_SUCCESS)
			return FALSE;
		*value = dwValue;
		return TRUE;
	}

	return FALSE;
}

BOOL wf_settings_read_string_ascii(HKEY key, LPCSTR subkey, LPTSTR name, rdpSettings* settings,
                                   UINT32 id)
{
	BOOL rc = FALSE;
	HKEY hKey;
	int length;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	char* strA = NULL;
	TCHAR* strX = NULL;

	status = RegOpenKeyExA(key, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

	if (status != ERROR_SUCCESS)
		return FALSE;

	status = RegQueryValueEx(hKey, name, NULL, &dwType, NULL, &dwSize);

	if (status == ERROR_SUCCESS)
	{
		strX = (LPTSTR)malloc(dwSize + sizeof(TCHAR));
		if (!strX)
			return FALSE;
		status = RegQueryValueEx(hKey, name, NULL, &dwType, (BYTE*)strX, &dwSize);

		if (status != ERROR_SUCCESS)
		{
			free(strX);
			RegCloseKey(hKey);
			return FALSE;
		}
	}

	if (strX)
	{
#ifdef UNICODE
		length = WideCharToMultiByte(CP_UTF8, 0, strX, lstrlenW(strX), NULL, 0, NULL, NULL);
		strA = (char*)malloc(length + 1);
		WideCharToMultiByte(CP_UTF8, 0, strX, lstrlenW(strX), strA, length, NULL, NULL);
		strA[length] = '\0';
		rc = freerdp_settings_set_string(settings, id, strA);
#else
		rc = freerdp_settings_set_string(settings, id, strX);
#endif
	}

	free(strX);
	free(strA);
	return rc;
}
