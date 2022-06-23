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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/string.h>
#include <winpr/library.h>

#include <freerdp/addin.h>
#include <freerdp/build-config.h>

#include <freerdp/log.h>
#define TAG FREERDP_TAG("addin")

static INLINE BOOL is_path_required(LPCSTR path, size_t len)
{
	if (!path || (len <= 1))
		return FALSE;

	if (strcmp(path, ".") == 0)
		return FALSE;

	return TRUE;
}

LPSTR freerdp_get_library_install_path(void)
{
	LPSTR pszPath;
	size_t cchPath;
	size_t cchLibraryPath;
	size_t cchInstallPrefix;
	BOOL needLibPath, needInstallPath;
	LPCSTR pszLibraryPath = FREERDP_LIBRARY_PATH;
	LPCSTR pszInstallPrefix = FREERDP_INSTALL_PREFIX;
	cchLibraryPath = strlen(pszLibraryPath) + 1;
	cchInstallPrefix = strlen(pszInstallPrefix) + 1;
	cchPath = cchInstallPrefix + cchLibraryPath;
	needInstallPath = is_path_required(pszInstallPrefix, cchInstallPrefix);
	needLibPath = is_path_required(pszLibraryPath, cchLibraryPath);

	if (!needInstallPath && !needLibPath)
		return NULL;

	pszPath = (LPSTR)malloc(cchPath + 1);

	if (!pszPath)
		return NULL;

	if (needInstallPath)
	{
		CopyMemory(pszPath, pszInstallPrefix, cchInstallPrefix);
		pszPath[cchInstallPrefix] = '\0';
	}

	if (needLibPath)
	{
		if (FAILED(NativePathCchAppendA(pszPath, cchPath + 1, pszLibraryPath)))
		{
			free(pszPath);
			return NULL;
		}
	}

	return pszPath;
}

LPSTR freerdp_get_dynamic_addin_install_path(void)
{
#if defined(WITH_ADD_PLUGIN_TO_RPATH)
	return NULL;
#else
	LPSTR pszPath;
	size_t cchPath;
	size_t cchAddinPath;
	size_t cchInstallPrefix;
	BOOL needLibPath, needInstallPath;
	LPCSTR pszAddinPath = FREERDP_ADDIN_PATH;
	LPCSTR pszInstallPrefix = FREERDP_INSTALL_PREFIX;
	cchAddinPath = strlen(pszAddinPath) + 1;
	cchInstallPrefix = strlen(pszInstallPrefix) + 1;
	cchPath = cchInstallPrefix + cchAddinPath;
	needInstallPath = is_path_required(pszInstallPrefix, cchInstallPrefix);
	needLibPath = is_path_required(pszAddinPath, cchAddinPath);

	WLog_DBG(TAG,
	         "freerdp_get_dynamic_addin_install_path <- pszInstallPrefix: %s, pszAddinPath: %s",
	         pszInstallPrefix, pszAddinPath);

	if (!needInstallPath && !needLibPath)
		return NULL;

	pszPath = (LPSTR)calloc(cchPath + 1, sizeof(CHAR));

	if (!pszPath)
		return NULL;

	if (needInstallPath)
	{
		CopyMemory(pszPath, pszInstallPrefix, cchInstallPrefix);
		pszPath[cchInstallPrefix] = '\0';
	}

	if (needLibPath)
	{
		if (FAILED(NativePathCchAppendA(pszPath, cchPath + 1, pszAddinPath)))
		{
			free(pszPath);
			return NULL;
		}
	}

	WLog_DBG(TAG, "freerdp_get_dynamic_addin_install_path -> pszPath: %s", pszPath);

	return pszPath;
#endif
}

PVIRTUALCHANNELENTRY freerdp_load_dynamic_addin(LPCSTR pszFileName, LPCSTR pszPath,
                                                LPCSTR pszEntryName)
{
	LPSTR pszAddinInstallPath = freerdp_get_dynamic_addin_install_path();
	PVIRTUALCHANNELENTRY entry = NULL;
	BOOL bHasExt = TRUE;
	PCSTR pszExt;
	size_t cchExt = 0;
	HINSTANCE library = NULL;
	size_t cchFileName;
	size_t cchFilePath;
	LPSTR pszAddinFile = NULL;
	LPSTR pszFilePath = NULL;
	LPSTR pszRelativeFilePath = NULL;
	size_t cchAddinFile;
	size_t cchAddinInstallPath;

	if (!pszFileName || !pszEntryName)
		goto fail;

	WLog_DBG(TAG, "freerdp_load_dynamic_addin <- pszFileName: %s, pszPath: %s, pszEntryName: %s",
	         pszFileName, pszPath, pszEntryName);

	cchFileName = strlen(pszFileName);

	/* Get file name with prefix and extension */
	if (FAILED(PathCchFindExtensionA(pszFileName, cchFileName + 1, &pszExt)))
	{
		pszExt = PathGetSharedLibraryExtensionA(PATH_SHARED_LIB_EXT_WITH_DOT);
		cchExt = strlen(pszExt);
		bHasExt = FALSE;
	}

	if (bHasExt)
	{
		pszAddinFile = _strdup(pszFileName);

		if (!pszAddinFile)
			goto fail;
	}
	else
	{
		cchAddinFile = cchFileName + cchExt + 2 + sizeof(FREERDP_SHARED_LIBRARY_PREFIX);
		pszAddinFile = (LPSTR)malloc(cchAddinFile + 1);

		if (!pszAddinFile)
			goto fail;

		sprintf_s(pszAddinFile, cchAddinFile, FREERDP_SHARED_LIBRARY_PREFIX "%s%s", pszFileName,
		          pszExt);
	}

	cchAddinFile = strlen(pszAddinFile);

	/* If a path is provided prefix the library name with it. */
	if (pszPath)
	{
		size_t relPathLen = strlen(pszPath) + cchAddinFile + 1;
		pszRelativeFilePath = calloc(relPathLen, sizeof(CHAR));

		if (!pszRelativeFilePath)
			goto fail;

		sprintf_s(pszRelativeFilePath, relPathLen, "%s", pszPath);
		NativePathCchAppendA(pszRelativeFilePath, relPathLen, pszAddinFile);
	}
	else
		pszRelativeFilePath = _strdup(pszAddinFile);

	if (!pszRelativeFilePath)
		goto fail;

	/* If a system prefix path is provided try these locations too. */
	if (pszAddinInstallPath)
	{
		cchAddinInstallPath = strlen(pszAddinInstallPath);
		cchFilePath = cchAddinInstallPath + cchFileName + 32;
		pszFilePath = (LPSTR)malloc(cchFilePath + 1);

		if (!pszFilePath)
			goto fail;

		CopyMemory(pszFilePath, pszAddinInstallPath, cchAddinInstallPath);
		pszFilePath[cchAddinInstallPath] = '\0';
		NativePathCchAppendA((LPSTR)pszFilePath, cchFilePath + 1, pszRelativeFilePath);
	}
	else
		pszFilePath = _strdup(pszRelativeFilePath);

	library = LoadLibraryX(pszFilePath);

	if (!library)
		goto fail;

	entry = (PVIRTUALCHANNELENTRY)GetProcAddress(library, pszEntryName);
fail:
	free(pszRelativeFilePath);
	free(pszAddinFile);
	free(pszFilePath);
	free(pszAddinInstallPath);

	if (!entry && library)
		FreeLibrary(library);

	return entry;
}

PVIRTUALCHANNELENTRY freerdp_load_dynamic_channel_addin_entry(LPCSTR pszName, LPCSTR pszSubsystem,
                                                              LPCSTR pszType, DWORD dwFlags)
{
	PVIRTUALCHANNELENTRY entry;
	LPSTR pszFileName;
	const size_t cchBaseFileName = sizeof(FREERDP_SHARED_LIBRARY_PREFIX) + 32;
	size_t nameLen = 0;
	size_t subsystemLen = 0;
	size_t typeLen = 0;
	size_t cchFileName = 0;

	if (pszName)
		nameLen = strnlen(pszName, MAX_PATH);
	if (pszSubsystem)
		subsystemLen = strnlen(pszSubsystem, MAX_PATH);
	if (pszType)
		typeLen = strnlen(pszType, MAX_PATH);

	if (pszName && pszSubsystem && pszType)
	{
		cchFileName = cchBaseFileName + nameLen + subsystemLen + typeLen;
		pszFileName = (LPSTR)malloc(cchFileName);

		if (!pszFileName)
			return NULL;

		sprintf_s(pszFileName, cchFileName, "%s-client-%s-%s", pszName, pszSubsystem, pszType);
	}
	else if (pszName && pszSubsystem)
	{
		cchFileName = cchBaseFileName + nameLen + subsystemLen;
		pszFileName = (LPSTR)malloc(cchFileName);

		if (!pszFileName)
			return NULL;

		sprintf_s(pszFileName, cchFileName, "%s-client-%s", pszName, pszSubsystem);
	}
	else if (pszName)
	{
		cchFileName = cchBaseFileName + nameLen;
		pszFileName = (LPSTR)malloc(cchFileName);

		if (!pszFileName)
			return NULL;

		sprintf_s(pszFileName, cchFileName, "%s-client", pszName);
	}
	else
	{
		return NULL;
	}

	{
		LPCSTR pszExtension = PathGetSharedLibraryExtensionA(0);
		LPCSTR pszPrefix = FREERDP_SHARED_LIBRARY_PREFIX;
		LPSTR tmp;
		int rc = 0;

		if (pszPrefix)
			cchFileName += strnlen(pszPrefix, MAX_PATH);
		if (pszExtension)
			cchFileName += strnlen(pszExtension, MAX_PATH) + 1;
		tmp = calloc(cchFileName, sizeof(CHAR));
		if (tmp)
			rc = sprintf_s(tmp, cchFileName, "%s%s.%s", pszPrefix, pszFileName, pszExtension);

		free(pszFileName);
		pszFileName = tmp;
		if (!pszFileName || (rc < 0))
		{
			free(pszFileName);
			return NULL;
		}
	}

	if (pszSubsystem)
	{
		LPSTR pszEntryName;
		size_t cchEntryName;
		/* subsystem add-in */
		cchEntryName = 64 + nameLen;
		pszEntryName = (LPSTR)malloc(cchEntryName + 1);

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
	{
		if (dwFlags & FREERDP_ADDIN_CHANNEL_ENTRYEX)
			entry = freerdp_load_dynamic_addin(pszFileName, NULL, "VirtualChannelEntryEx");
		else
			entry = freerdp_load_dynamic_addin(pszFileName, NULL, "VirtualChannelEntry");
	}
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

FREERDP_LOAD_CHANNEL_ADDIN_ENTRY_FN freerdp_get_current_addin_provider(void)
{
	return freerdp_load_static_channel_addin_entry;
}

PVIRTUALCHANNELENTRY freerdp_load_channel_addin_entry(LPCSTR pszName, LPCSTR pszSubsystem,
                                                      LPCSTR pszType, DWORD dwFlags)
{
	PVIRTUALCHANNELENTRY entry = NULL;

	if (freerdp_load_static_channel_addin_entry)
		entry = freerdp_load_static_channel_addin_entry(pszName, pszSubsystem, pszType, dwFlags);

	if (!entry)
		entry = freerdp_load_dynamic_channel_addin_entry(pszName, pszSubsystem, pszType, dwFlags);

	if (!entry)
		WLog_WARN(TAG, "Failed to load channel %s [%s]", pszName, pszSubsystem);

	return entry;
}
