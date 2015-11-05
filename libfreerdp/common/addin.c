/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Addin Loader
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/library.h>

#include <freerdp/addin.h>
#include <freerdp/build-config.h>


LPSTR freerdp_get_library_install_path()
{
	LPSTR pszPath;
	size_t cchPath;
	size_t cchLibraryPath;
	size_t cchInstallPrefix;
	LPCSTR pszLibraryPath = FREERDP_LIBRARY_PATH;
	LPCSTR pszInstallPrefix = FREERDP_INSTALL_PREFIX;

	cchLibraryPath = strlen(pszLibraryPath);
	cchInstallPrefix = strlen(pszInstallPrefix);

	cchPath = cchInstallPrefix + cchLibraryPath + 2;
	pszPath = (LPSTR) malloc(cchPath + 1);
	if (!pszPath)
		return NULL;

	CopyMemory(pszPath, pszInstallPrefix, cchInstallPrefix);
	pszPath[cchInstallPrefix] = '\0';

	if (FAILED(NativePathCchAppendA(pszPath, cchPath + 1, pszLibraryPath)))
	{
		free(pszPath);
		return NULL;
	}

	return pszPath;
}

LPSTR freerdp_get_dynamic_addin_install_path()
{
	LPSTR pszPath;
	size_t cchPath;
	size_t cchAddinPath;
	size_t cchInstallPrefix;
	LPCSTR pszAddinPath = FREERDP_ADDIN_PATH;
	LPCSTR pszInstallPrefix = FREERDP_INSTALL_PREFIX;

	cchAddinPath = strlen(pszAddinPath);
	cchInstallPrefix = strlen(pszInstallPrefix);

	cchPath = cchInstallPrefix + cchAddinPath + 2;
	pszPath = (LPSTR) malloc(cchPath + 1);
	if (!pszPath)
		return NULL;

	CopyMemory(pszPath, pszInstallPrefix, cchInstallPrefix);
	pszPath[cchInstallPrefix] = '\0';

	if (FAILED(NativePathCchAppendA(pszPath, cchPath + 1, pszAddinPath)))
	{
		free(pszPath);
		return NULL;
	}

	return pszPath;
}

void* freerdp_load_dynamic_addin(LPCSTR pszFileName, LPCSTR pszPath, LPCSTR pszEntryName)
{
	void* entry;
	BOOL bHasExt;
	PCSTR pszExt;
	size_t cchExt;
	HINSTANCE library;
	size_t cchFileName;
	LPSTR pszFilePath;
	size_t cchFilePath;
	LPSTR pszAddinFile;
	size_t cchAddinFile;
	LPSTR pszAddinInstallPath;
	size_t cchAddinInstallPath;

	entry = NULL;
	cchExt = 0;
	bHasExt = TRUE;
	cchFileName = strlen(pszFileName);

	if (FAILED(PathCchFindExtensionA(pszFileName, cchFileName + 1, &pszExt)))
	{
		pszExt = PathGetSharedLibraryExtensionA(PATH_SHARED_LIB_EXT_WITH_DOT);
		cchExt = strlen(pszExt);
		bHasExt = FALSE;
	}

	pszAddinInstallPath = freerdp_get_dynamic_addin_install_path();
	if (!pszAddinInstallPath)
		return NULL;
	cchAddinInstallPath = strlen(pszAddinInstallPath);

	cchFilePath = cchAddinInstallPath + cchFileName + 32;
	pszFilePath = (LPSTR) malloc(cchFilePath + 1);
	if (!pszFilePath)
	{
		free(pszAddinInstallPath);
		return NULL;
	}

	if (bHasExt)
	{
		pszAddinFile = _strdup(pszFileName);
		if (!pszAddinFile)
		{
			free(pszAddinInstallPath);
			free(pszFilePath);
			return NULL;
		}
		cchAddinFile = strlen(pszAddinFile);
	}
	else
	{
		cchAddinFile = cchFileName + cchExt + 2 + sizeof(FREERDP_SHARED_LIBRARY_PREFIX);
		pszAddinFile = (LPSTR) malloc(cchAddinFile + 1);
		if (!pszAddinFile)
		{
			free(pszAddinInstallPath);
			free(pszFilePath);
			return NULL;
		}
		sprintf_s(pszAddinFile, cchAddinFile, FREERDP_SHARED_LIBRARY_PREFIX"%s%s", pszFileName, pszExt);
		cchAddinFile = strlen(pszAddinFile);
	}

	CopyMemory(pszFilePath, pszAddinInstallPath, cchAddinInstallPath);
	pszFilePath[cchAddinInstallPath] = '\0';

	NativePathCchAppendA((LPSTR) pszFilePath, cchFilePath + 1, pszAddinFile);

	library = LoadLibraryA(pszFilePath);

	free(pszAddinInstallPath);
	free(pszAddinFile);
	free(pszFilePath);

	if (!library)
		return NULL;

	entry = GetProcAddress(library, pszEntryName);

	if (entry)
		return entry;

	FreeLibrary(library);
	return entry;
}

void* freerdp_load_dynamic_channel_addin_entry(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	void* entry;
	LPSTR pszFileName;
	size_t cchFileName = sizeof(FREERDP_SHARED_LIBRARY_PREFIX) + 32;
	LPCSTR pszExtension;
	LPCSTR pszPrefix = FREERDP_SHARED_LIBRARY_PREFIX;

	pszExtension = PathGetSharedLibraryExtensionA(0);

	if (pszName && pszSubsystem && pszType)
	{
		cchFileName += strlen(pszName) + strlen(pszSubsystem) + strlen(pszType) + strlen(pszExtension);
		pszFileName = (LPSTR) malloc(cchFileName);
		if (!pszFileName)
			return NULL;
		sprintf_s(pszFileName, cchFileName, "%s%s-client-%s-%s.%s", pszPrefix, pszName, pszSubsystem, pszType, pszExtension);
		cchFileName = strlen(pszFileName);
	}
	else if (pszName && pszSubsystem)
	{
		cchFileName += strlen(pszName) + strlen(pszSubsystem) + strlen(pszExtension);
		pszFileName = (LPSTR) malloc(cchFileName);
		if (!pszFileName)
			return NULL;

		sprintf_s(pszFileName, cchFileName, "%s%s-client-%s.%s", pszPrefix, pszName, pszSubsystem, pszExtension);
		cchFileName = strlen(pszFileName);
	}
	else if (pszName)
	{
		cchFileName += strlen(pszName) + strlen(pszExtension);
		pszFileName = (LPSTR) malloc(cchFileName);
		if (!pszFileName)
			return NULL;

		sprintf_s(pszFileName, cchFileName, "%s%s-client.%s", pszPrefix, pszName, pszExtension);
		cchFileName = strlen(pszFileName);
	}
	else
	{
		return NULL;
	}

	if (pszSubsystem)
	{
		LPSTR pszEntryName;
		size_t cchEntryName;

		/* subsystem add-in */

		cchEntryName = 64 + strlen(pszName);
		pszEntryName = (LPSTR) malloc(cchEntryName + 1);
		if (!pszEntryName)
		{
			free(pszFileName);
			return NULL;
		}
		sprintf_s(pszEntryName, cchEntryName + 1, "freerdp_%s_client_subsystem_entry", pszName);

		entry = freerdp_load_dynamic_addin(pszFileName, NULL, pszEntryName);

		free(pszEntryName);
		free(pszFileName);

		return entry;
	}

	/* channel add-in */

	if (dwFlags & FREERDP_ADDIN_CHANNEL_STATIC)
		entry = freerdp_load_dynamic_addin(pszFileName, NULL, "VirtualChannelEntry");
	else if (dwFlags & FREERDP_ADDIN_CHANNEL_DYNAMIC)
		entry = freerdp_load_dynamic_addin(pszFileName, NULL, "DVCPluginEntry");
	else if (dwFlags & FREERDP_ADDIN_CHANNEL_DEVICE)
		entry = freerdp_load_dynamic_addin(pszFileName, NULL, "DeviceServiceEntry");
	else
		entry = freerdp_load_dynamic_addin(pszFileName, NULL, pszType);

	free(pszFileName);
	return entry;
}

static FREERDP_LOAD_CHANNEL_ADDIN_ENTRY_FN freerdp_load_static_channel_addin_entry = NULL;

int freerdp_register_addin_provider(FREERDP_LOAD_CHANNEL_ADDIN_ENTRY_FN provider, DWORD dwFlags)
{
	freerdp_load_static_channel_addin_entry = provider;
	return 0;
}

void* freerdp_load_channel_addin_entry(LPCSTR pszName, LPSTR pszSubsystem, LPSTR pszType, DWORD dwFlags)
{
	void* entry = NULL;

	if (freerdp_load_static_channel_addin_entry)
		entry = freerdp_load_static_channel_addin_entry(pszName, pszSubsystem, pszType, dwFlags);

	if (!entry)
		entry = freerdp_load_dynamic_channel_addin_entry(pszName, pszSubsystem, pszType, dwFlags);

	return entry;
}
