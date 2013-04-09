/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Channel Addins
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

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/collections.h>

#include <freerdp/addin.h>
#include <freerdp/client/channels.h>

#include "tables.h"

#include "addin.h"

extern const STATIC_ENTRY_TABLE CLIENT_STATIC_ENTRY_TABLES[];

void* freerdp_channels_find_static_entry_in_table(const STATIC_ENTRY_TABLE* table, const char* identifier)
{
	int index = 0;
	STATIC_ENTRY* pEntry;

	pEntry = (STATIC_ENTRY*) &table->table[index++];

	while (pEntry->entry != NULL)
	{
		if (strcmp(pEntry->name, identifier) == 0)
		{
			return (void*) pEntry->entry;
		}

		pEntry = (STATIC_ENTRY*) &table->table[index++];
	}

	return NULL;
}

void* freerdp_channels_client_find_static_entry(const char* name, const char* identifier)
{
	int index = 0;
	STATIC_ENTRY_TABLE* pEntry;

	pEntry = (STATIC_ENTRY_TABLE*) &CLIENT_STATIC_ENTRY_TABLES[index++];

	while (pEntry->table != NULL)
	{
		if (strcmp(pEntry->name, name) == 0)
		{
			return freerdp_channels_find_static_entry_in_table(pEntry, identifier);
		}

		pEntry = (STATIC_ENTRY_TABLE*) &CLIENT_STATIC_ENTRY_TABLES[index++];
	}

	return NULL;
}

extern const STATIC_ADDIN_TABLE CLIENT_STATIC_ADDIN_TABLE[];

FREERDP_ADDIN** freerdp_channels_list_client_static_addins(LPSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	int i, j;
	DWORD nAddins;
	FREERDP_ADDIN* pAddin;
	FREERDP_ADDIN** ppAddins = NULL;
	STATIC_SUBSYSTEM_ENTRY* subsystems;

	nAddins = 0;
	ppAddins = (FREERDP_ADDIN**) malloc(sizeof(FREERDP_ADDIN*) * 128);
	ppAddins[nAddins] = NULL;

	for (i = 0; CLIENT_STATIC_ADDIN_TABLE[i].name != NULL; i++)
	{
		pAddin = (FREERDP_ADDIN*) malloc(sizeof(FREERDP_ADDIN));
		ZeroMemory(pAddin, sizeof(FREERDP_ADDIN));

		strcpy(pAddin->cName, CLIENT_STATIC_ADDIN_TABLE[i].name);

		pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
		pAddin->dwFlags |= FREERDP_ADDIN_STATIC;
		pAddin->dwFlags |= FREERDP_ADDIN_NAME;

		ppAddins[nAddins++] = pAddin;

		subsystems = (STATIC_SUBSYSTEM_ENTRY*) CLIENT_STATIC_ADDIN_TABLE[i].table;

		for (j = 0; subsystems[j].name != NULL; j++)
		{
			pAddin = (FREERDP_ADDIN*) malloc(sizeof(FREERDP_ADDIN));
			ZeroMemory(pAddin, sizeof(FREERDP_ADDIN));

			strcpy(pAddin->cName, CLIENT_STATIC_ADDIN_TABLE[i].name);
			strcpy(pAddin->cSubsystem, subsystems[j].name);

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_STATIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;

			ppAddins[nAddins++] = pAddin;
		}
	}

	ppAddins[nAddins] = NULL;

	return ppAddins;
}

FREERDP_ADDIN** freerdp_channels_list_dynamic_addins(LPSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	int index;
	int nDashes;
	HANDLE hFind;
	DWORD nAddins;
	LPSTR pszPattern;
	size_t cchPattern;
	LPCSTR pszAddinPath = FREERDP_ADDIN_PATH;
	LPCSTR pszInstallPrefix = FREERDP_INSTALL_PREFIX;
	LPCSTR pszExtension;
	LPSTR pszSearchPath;
	size_t cchSearchPath;
	size_t cchAddinPath;
	size_t cchInstallPrefix;
	FREERDP_ADDIN** ppAddins;
	WIN32_FIND_DATAA FindData;

	cchAddinPath = strlen(pszAddinPath);
	cchInstallPrefix = strlen(pszInstallPrefix);

	pszExtension = PathGetSharedLibraryExtensionA(0);

	cchPattern = 128 + strlen(pszExtension) + 2;
	pszPattern = (LPSTR) malloc(cchPattern + 1);

	if (pszName && pszSubsystem && pszType)
	{
		sprintf_s(pszPattern, cchPattern, "%s-client-%s-%s.%s", pszName, pszSubsystem, pszType, pszExtension);
	}
	else if (pszName && pszType)
	{
		sprintf_s(pszPattern, cchPattern, "%s-client-?-%s.%s", pszName, pszType, pszExtension);
	}
	else if (pszName)
	{
		sprintf_s(pszPattern, cchPattern, "%s-client*.%s", pszName, pszExtension);
	}
	else
	{
		sprintf_s(pszPattern, cchPattern, "?-client*.%s", pszExtension);
	}

	cchPattern = strlen(pszPattern);

	cchSearchPath = cchInstallPrefix + cchAddinPath + cchPattern + 3;
	pszSearchPath = (LPSTR) malloc(cchSearchPath + 1);

	CopyMemory(pszSearchPath, pszInstallPrefix, cchInstallPrefix);
	pszSearchPath[cchInstallPrefix] = '\0';

	NativePathCchAppendA(pszSearchPath, cchSearchPath + 1, pszAddinPath);
	NativePathCchAppendA(pszSearchPath, cchSearchPath + 1, pszPattern);

	cchSearchPath = strlen(pszSearchPath);

	hFind = FindFirstFileA(pszSearchPath, &FindData);

	nAddins = 0;
	ppAddins = (FREERDP_ADDIN**) malloc(sizeof(FREERDP_ADDIN*) * 128);
	ppAddins[nAddins] = NULL;

	if (hFind == INVALID_HANDLE_VALUE)
		return ppAddins;

	do
	{
		char* p[5];
		FREERDP_ADDIN* pAddin;

		nDashes = 0;
		pAddin = (FREERDP_ADDIN*) malloc(sizeof(FREERDP_ADDIN));
		ZeroMemory(pAddin, sizeof(FREERDP_ADDIN));

		for (index = 0; FindData.cFileName[index]; index++)
			nDashes += (FindData.cFileName[index] == '-') ? 1 : 0;

		if (nDashes == 1)
		{
			/* <name>-client.<extension> */

			p[0] = FindData.cFileName;
			p[1] = strchr(p[0], '-') + 1;

			strncpy(pAddin->cName, p[0], (p[1] - p[0]) - 1);

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;

			ppAddins[nAddins++] = pAddin;
		}
		else if (nDashes == 2)
		{
			/* <name>-client-<subsystem>.<extension> */

			p[0] = FindData.cFileName;
			p[1] = strchr(p[0], '-') + 1;
			p[2] = strchr(p[1], '-') + 1;
			p[3] = strchr(p[2], '.') + 1;

			strncpy(pAddin->cName, p[0], (p[1] - p[0]) - 1);
			strncpy(pAddin->cSubsystem, p[2], (p[3] - p[2]) - 1);

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;

			ppAddins[nAddins++] = pAddin;
		}
		else if (nDashes == 3)
		{
			/* <name>-client-<subsystem>-<type>.<extension> */

			p[0] = FindData.cFileName;
			p[1] = strchr(p[0], '-') + 1;
			p[2] = strchr(p[1], '-') + 1;
			p[3] = strchr(p[2], '-') + 1;
			p[4] = strchr(p[3], '.') + 1;

			strncpy(pAddin->cName, p[0], (p[1] - p[0]) - 1);
			strncpy(pAddin->cSubsystem, p[2], (p[3] - p[2]) - 1);
			strncpy(pAddin->cType, p[3], (p[4] - p[3]) - 1);

			pAddin->dwFlags = FREERDP_ADDIN_CLIENT;
			pAddin->dwFlags |= FREERDP_ADDIN_DYNAMIC;
			pAddin->dwFlags |= FREERDP_ADDIN_NAME;
			pAddin->dwFlags |= FREERDP_ADDIN_SUBSYSTEM;
			pAddin->dwFlags |= FREERDP_ADDIN_TYPE;

			ppAddins[nAddins++] = pAddin;
		}
		else
		{
			free(pAddin);
		}
	}
	while (FindNextFileA(hFind, &FindData));

	FindClose(hFind);

	ppAddins[nAddins] = NULL;

	return ppAddins;
}

FREERDP_ADDIN** freerdp_channels_list_addins(LPSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	if (dwFlags & FREERDP_ADDIN_STATIC)
		return freerdp_channels_list_client_static_addins(pszName, pszSubsystem, pszType, dwFlags);
	else if (dwFlags & FREERDP_ADDIN_DYNAMIC)
		return freerdp_channels_list_dynamic_addins(pszName, pszSubsystem, pszType, dwFlags);

	return NULL;
}

void freerdp_channels_addin_list_free(FREERDP_ADDIN** ppAddins)
{
	int index;

	for (index = 0; ppAddins[index] != NULL; index++)
		free(ppAddins[index]);

	free(ppAddins);
}

void* freerdp_channels_load_static_addin_entry(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	int i, j;
	STATIC_SUBSYSTEM_ENTRY* subsystems;

	for (i = 0; CLIENT_STATIC_ADDIN_TABLE[i].name != NULL; i++)
	{
		if (strcmp(CLIENT_STATIC_ADDIN_TABLE[i].name, pszName) == 0)
		{
			if (pszSubsystem != NULL)
			{
				subsystems = (STATIC_SUBSYSTEM_ENTRY*) CLIENT_STATIC_ADDIN_TABLE[i].table;

				for (j = 0; subsystems[j].name != NULL; j++)
				{
					if (strcmp(subsystems[j].name, pszSubsystem) == 0)
					{
						if (pszType)
						{
							if (strcmp(subsystems[j].type, pszType) == 0)
								return (void*) subsystems[j].entry;
						}
						else
						{
							return (void*) subsystems[j].entry;
						}
					}
				}
			}
			else
			{
				return (void*) CLIENT_STATIC_ADDIN_TABLE[i].entry;
			}
		}
	}

	return NULL;
}
