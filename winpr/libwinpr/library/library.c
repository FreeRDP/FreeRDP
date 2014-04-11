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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __MACOSX__
#include <mach-o/dyld.h>
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
	HMODULE library;

	library = dlopen(lpLibFileName, RTLD_LOCAL | RTLD_LAZY);

	if (!library)
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

	if (!library)
	{
		fprintf(stderr, "LoadLibraryExA: failed to open %s: %s\n", lpLibFileName, dlerror());
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
			return 0;

		buffer[status] = '\0';

		length = strlen(buffer);

		if (length < nSize)
		{
			CopyMemory(lpFilename, buffer, length);
			lpFilename[length] = '\0';
		}
		else
		{
			CopyMemory(lpFilename, buffer, nSize - 1);
			lpFilename[nSize - 1] = '\0';
		}

		return 0;
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
		}
		else
		{
			CopyMemory(lpFilename, buffer, nSize - 1);
			lpFilename[nSize - 1] = '\0';
		}
		
		return 0;
	}
#endif

	return 0;
}

#endif

