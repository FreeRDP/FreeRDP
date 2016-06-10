/**
 * WinPR: Windows Portable Runtime
 * Library Loader
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
#include <winpr/platform.h>

#include <winpr/library.h>

#include "../log.h"
#define TAG WINPR_TAG("library")

/**
 * api-ms-win-core-libraryloader-l1-1-1.dll:
 *
 * AddDllDirectory
 * RemoveDllDirectory
 * SetDefaultDllDirectories
 * DisableThreadLibraryCalls
 * EnumResourceLanguagesExA
 * EnumResourceLanguagesExW
 * EnumResourceNamesExA
 * EnumResourceNamesExW
 * EnumResourceTypesExA
 * EnumResourceTypesExW
 * FindResourceExW
 * FindStringOrdinal
 * FreeLibrary
 * FreeLibraryAndExitThread
 * FreeResource
 * GetModuleFileNameA
 * GetModuleFileNameW
 * GetModuleHandleA
 * GetModuleHandleExA
 * GetModuleHandleExW
 * GetModuleHandleW
 * GetProcAddress
 * LoadLibraryExA
 * LoadLibraryExW
 * LoadResource
 * LoadStringA
 * LoadStringW
 * LockResource
 * QueryOptionalDelayLoadedAPI
 * SizeofResource
 */

#if !defined(_WIN32) || defined(_UWP)

#ifndef _WIN32

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __MACOSX__
#include <mach-o/dyld.h>
#endif

#endif

DLL_DIRECTORY_COOKIE AddDllDirectory(PCWSTR NewDirectory)
{
	return NULL;
}

BOOL RemoveDllDirectory(DLL_DIRECTORY_COOKIE Cookie)
{
	return TRUE;
}

BOOL SetDefaultDllDirectories(DWORD DirectoryFlags)
{
	return TRUE;
}

HMODULE LoadLibraryA(LPCSTR lpLibFileName)
{
#if defined(_UWP)
	int status;
	HMODULE hModule = NULL;
	WCHAR* filenameW = NULL;

	if (!lpLibFileName)
		return NULL;

	status = ConvertToUnicode(CP_UTF8, 0, lpLibFileName, -1, &filenameW, 0);

	if (status < 1)
		return NULL;

	hModule = LoadPackagedLibrary(filenameW, 0);

	free(filenameW);

	return hModule;
#else
	HMODULE library;

	library = dlopen(lpLibFileName, RTLD_LOCAL | RTLD_LAZY);

	if (!library)
	{
		WLog_ERR(TAG, "LoadLibraryA: %s", dlerror());
		return NULL;
	}

	return library;
#endif
}

HMODULE LoadLibraryW(LPCWSTR lpLibFileName)
{
#if defined(_UWP)
	return LoadPackagedLibrary(lpLibFileName, 0);
#else
	return (HMODULE) NULL;
#endif
}

HMODULE LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
#if !defined(_UWP)
	HMODULE library;

	library = dlopen(lpLibFileName, RTLD_LOCAL | RTLD_LAZY);

	if (!library)
	{
		WLog_ERR(TAG, "LoadLibraryExA: failed to open %s: %s", lpLibFileName, dlerror());
		return NULL;
	}

	return library;
#else
	return (HMODULE)NULL;
#endif
}

HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	return (HMODULE) NULL;
}

#endif

#ifndef _WIN32

FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	FARPROC proc;
	proc = dlsym(hModule, lpProcName);

	if (proc == NULL)
	{
		WLog_ERR(TAG, "GetProcAddress: could not find procedure %s: %s", lpProcName, dlerror());
		return (FARPROC) NULL;
	}

	return proc;
}

BOOL FreeLibrary(HMODULE hLibModule)
{
	int status;
	status = dlclose(hLibModule);

	if (status != 0)
		return FALSE;

	return TRUE;
}

HMODULE GetModuleHandleA(LPCSTR lpModuleName)
{
	return NULL;
}

HMODULE GetModuleHandleW(LPCWSTR lpModuleName)
{
	return NULL;
}

/**
 * GetModuleFileName:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms683197/
 *
 * Finding current executable's path without /proc/self/exe:
 * http://stackoverflow.com/questions/1023306/finding-current-executables-path-without-proc-self-exe
 */

DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize)
{
	WLog_ERR(TAG, "%s is not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return 0;
}

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
#if defined(__linux__)
	int status;
	int length;
	char path[64];

	if (!hModule)
	{
		char buffer[4096];
		sprintf(path, "/proc/%d/exe", getpid());
		status = readlink(path, buffer, sizeof(buffer));

		if (status < 0)
		{
			SetLastError(ERROR_INTERNAL_ERROR);
			return 0;
		}

		buffer[status] = '\0';
		length = strlen(buffer);

		if (length < nSize)
		{
			CopyMemory(lpFilename, buffer, length);
			lpFilename[length] = '\0';
			return length;
		}

		CopyMemory(lpFilename, buffer, nSize - 1);
		lpFilename[nSize - 1] = '\0';
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return nSize;
	}

#elif defined(__MACOSX__)
	int status;
	int length;

	if (!hModule)
	{
		char path[4096];
		char buffer[4096];
		uint32_t size = sizeof(path);
		status = _NSGetExecutablePath(path, &size);

		if (status != 0)
		{
			/* path too small */
			SetLastError(ERROR_INTERNAL_ERROR);
			return 0;
		}

		/*
		 * _NSGetExecutablePath may not return the canonical path,
		 * so use realpath to find the absolute, canonical path.
		 */
		realpath(path, buffer);
		length = strlen(buffer);

		if (length < nSize)
		{
			CopyMemory(lpFilename, buffer, length);
			lpFilename[length] = '\0';
			return length;
		}

		CopyMemory(lpFilename, buffer, nSize - 1);
		lpFilename[nSize - 1] = '\0';
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return nSize;
	}

#endif
	WLog_ERR(TAG, "%s is not implemented", __FUNCTION__);
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return 0;
}

#endif

