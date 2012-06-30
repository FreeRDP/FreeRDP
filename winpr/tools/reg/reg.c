/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Registry API Tool
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/tchar.h>
#include <winpr/wtypes.h>

#include <winpr/print.h>
#include <winpr/registry.h>

int main(int argc, char* argv[])
{
	HKEY hKey;
	LONG status;
	DWORD dwType;
	DWORD dwSize;
	DWORD dwValue;
	DWORD RemoteFX;
	char* ComputerName;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\FreeRDP"), 0, KEY_READ, &hKey);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("RegOpenKeyEx error: 0x%08lX\n"), status);
		return 0;
	}

	dwValue = 0;
	status = RegQueryValueEx(hKey, _T("RemoteFX"), NULL, &dwType, (BYTE*) &dwValue, &dwSize);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("RegQueryValueEx error: 0x%08lX\n"), status);
		return 0;
	}

	RemoteFX = dwValue;

	status = RegQueryValueEx(hKey, _T("ComputerName"), NULL, &dwType, NULL, &dwSize);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("RegQueryValueEx error: 0x%08lX\n"), status);
		return 0;
	}

	ComputerName = (char*) malloc(dwSize + 1);

	status = RegQueryValueEx(hKey, _T("ComputerName"), NULL, &dwType, (BYTE*) ComputerName, &dwSize);

	if (status != ERROR_SUCCESS)
	{
		_tprintf(_T("RegQueryValueEx error: 0x%08lX\n"), status);
		return 0;
	}

	printf("RemoteFX: %08lX\n", RemoteFX);
	printf("ComputerName: %s\n", ComputerName);

	RegCloseKey(hKey);

	free(ComputerName);

	return 0;
}

/* Modeline for vim. Don't delete */
/* vim: cindent:noet:sw=8:ts=8 */
