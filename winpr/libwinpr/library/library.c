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

#include <winpr/library.h>

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

#ifndef _WIN32

#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>

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
	HMODULE library;

	library = dlopen(lpLibFileName, RTLD_LOCAL | RTLD_LAZY);

	if (library == NULL)
	{
		fprintf(stderr, "LoadLibraryA: %s\n", dlerror());
		return NULL;
	}

	return library;
}

HMODULE LoadLibraryW(LPCWSTR lpLibFileName)
{
	return (HMODULE) NULL;
}

HMODULE LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE library;

	library = dlopen(lpLibFileName, RTLD_LOCAL | RTLD_LAZY);

	if (library == NULL)
	{
		fprintf(stderr, "LoadLibraryA: failed to open %s: %s\n", lpLibFileName, dlerror());
		return NULL;
	}

	return library;
}

HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	return (HMODULE) NULL;
}

FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	FARPROC proc;

	proc = dlsym(hModule, lpProcName);

	if (proc == NULL)
	{
		fprintf(stderr, "GetProcAddress: could not find procedure %s: %s\n", lpProcName, dlerror());
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

#endif

