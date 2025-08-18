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

#include <winpr/config.h>

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

#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#endif

DLL_DIRECTORY_COOKIE AddDllDirectory(WINPR_ATTR_UNUSED PCWSTR NewDirectory)
{
	/* TODO: Implement */
	WLog_ERR(TAG, "not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return NULL;
}

BOOL RemoveDllDirectory(WINPR_ATTR_UNUSED DLL_DIRECTORY_COOKIE Cookie)
{
	/* TODO: Implement */
	WLog_ERR(TAG, "not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

BOOL SetDefaultDllDirectories(WINPR_ATTR_UNUSED DWORD DirectoryFlags)
{
	/* TODO: Implement */
	WLog_ERR(TAG, "not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return FALSE;
}

HMODULE LoadLibraryA(LPCSTR lpLibFileName)
{
	if (!lpLibFileName)
		return NULL;

#if defined(_UWP)
	int status;
	HMODULE hModule = NULL;
	WCHAR* filenameW = NULL;

	filenameW = ConvertUtf8ToWCharAlloc(lpLibFileName, NULL);
	if (filenameW)
		return NULL;

	hModule = LoadLibraryW(filenameW);
	free(filenameW);
	return hModule;
#else
	HMODULE library = dlopen(lpLibFileName, RTLD_LOCAL | RTLD_LAZY);

	if (!library)
	{
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		const char* err = dlerror();
		WLog_ERR(TAG, "failed with %s", err);
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
	char* name = NULL;

	if (lpLibFileName)
		name = ConvertWCharToUtf8Alloc(lpLibFileName, NULL);

	HMODULE module = LoadLibraryA(name);
	free(name);
	return module;
#endif
}

HMODULE LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	if (dwFlags != 0)
		WLog_WARN(TAG, "does not support dwFlags 0x%08" PRIx32, dwFlags);

	if (hFile)
		WLog_WARN(TAG, "does not support hFile != NULL");

	return LoadLibraryA(lpLibFileName);
}

HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	if (dwFlags != 0)
		WLog_WARN(TAG, "does not support dwFlags 0x%08" PRIx32, dwFlags);

	if (hFile)
		WLog_WARN(TAG, "does not support hFile != NULL");

	return LoadLibraryW(lpLibFileName);
}

#endif

#if !defined(_WIN32) && !defined(__CYGWIN__)

FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	FARPROC proc = NULL;
	proc = dlsym(hModule, lpProcName);

	if (proc == NULL)
	{
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		WLog_ERR(TAG, "GetProcAddress: could not find procedure %s: %s", lpProcName, dlerror());
		return (FARPROC)NULL;
	}

	return proc;
}

BOOL FreeLibrary(HMODULE hLibModule)
{
	int status = 0;
	status = dlclose(hLibModule);

	if (status != 0)
		return FALSE;

	return TRUE;
}

HMODULE GetModuleHandleA(LPCSTR lpModuleName)
{
	return dlopen(lpModuleName, RTLD_NOLOAD | RTLD_LOCAL | RTLD_LAZY);
}

HMODULE GetModuleHandleW(LPCWSTR lpModuleName)
{
	char* name = NULL;
	if (lpModuleName)
		name = ConvertWCharToUtf8Alloc(lpModuleName, NULL);
	HANDLE hdl = GetModuleHandleA(name);
	free(name);
	return hdl;
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
	DWORD status = 0;
	if (!lpFilename)
	{
		SetLastError(ERROR_INTERNAL_ERROR);
		return 0;
	}

	char* name = calloc(nSize, sizeof(char));
	if (!name)
	{
		SetLastError(ERROR_INTERNAL_ERROR);
		return 0;
	}
	status = GetModuleFileNameA(hModule, name, nSize);

	if ((status > INT_MAX) || (nSize > INT_MAX))
	{
		SetLastError(ERROR_INTERNAL_ERROR);
		status = 0;
	}

	if (status > 0)
	{
		if (ConvertUtf8NToWChar(name, status, lpFilename, nSize) < 0)
		{
			free(name);
			SetLastError(ERROR_INTERNAL_ERROR);
			return 0;
		}
	}

	free(name);
	return status;
}

#if defined(__linux__) || defined(__NetBSD__) || defined(__DragonFly__)
static DWORD module_from_proc(const char* proc, LPSTR lpFilename, DWORD nSize)
{
	char buffer[8192] = { 0 };
	ssize_t status = readlink(proc, buffer, ARRAYSIZE(buffer) - 1);

	if ((status < 0) || ((size_t)status >= ARRAYSIZE(buffer)))
	{
		SetLastError(ERROR_INTERNAL_ERROR);
		return 0;
	}

	const size_t length = strnlen(buffer, ARRAYSIZE(buffer));

	if (length < nSize)
	{
		CopyMemory(lpFilename, buffer, length);
		lpFilename[length] = '\0';
		return (DWORD)length;
	}

	CopyMemory(lpFilename, buffer, nSize - 1);
	lpFilename[nSize - 1] = '\0';
	SetLastError(ERROR_INSUFFICIENT_BUFFER);
	return nSize;
}
#endif

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize)
{
	if (hModule)
	{
		WLog_ERR(TAG, "is not implemented");
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return 0;
	}

#if defined(__linux__)
	return module_from_proc("/proc/self/exe", lpFilename, nSize);
#elif defined(__FreeBSD__)
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	size_t cb = nSize;

	{
		const int rc = sysctl(mib, ARRAYSIZE(mib), NULL, &cb, NULL, 0);
		if (rc != 0)
		{
			SetLastError(ERROR_INTERNAL_ERROR);
			return 0;
		}
	}

	char* fullname = calloc(cb + 1, sizeof(char));
	if (!fullname)
	{
		SetLastError(ERROR_INTERNAL_ERROR);
		return 0;
	}

	{
		size_t cb2 = cb;
		const int rc = sysctl(mib, ARRAYSIZE(mib), fullname, &cb2, NULL, 0);
		if ((rc != 0) || (cb2 != cb))
		{
			SetLastError(ERROR_INTERNAL_ERROR);
			free(fullname);
			return 0;
		}
	}

	if (nSize > 0)
	{
		strncpy(lpFilename, fullname, nSize - 1);
		lpFilename[nSize - 1] = '\0';
	}
	free(fullname);

	if (nSize < cb)
		SetLastError(ERROR_INSUFFICIENT_BUFFER);

	return (DWORD)MIN(nSize, cb);
#elif defined(__NetBSD__)
	return module_from_proc("/proc/curproc/exe", lpFilename, nSize);
#elif defined(__DragonFly__)
	return module_from_proc("/proc/curproc/file", lpFilename, nSize);
#elif defined(__MACOSX__)
	char path[4096] = { 0 };
	char buffer[4096] = { 0 };
	uint32_t size = sizeof(path);
	const int status = _NSGetExecutablePath(path, &size);

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
	const size_t length = strnlen(buffer, sizeof(buffer));

	if (length < nSize)
	{
		CopyMemory(lpFilename, buffer, length);
		lpFilename[length] = '\0';
		return (DWORD)length;
	}

	CopyMemory(lpFilename, buffer, nSize - 1);
	lpFilename[nSize - 1] = '\0';
	SetLastError(ERROR_INSUFFICIENT_BUFFER);
	return nSize;
#else
	WLog_ERR(TAG, "is not implemented");
	SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
	return 0;
#endif
}

#endif

HMODULE LoadLibraryX(LPCSTR lpLibFileName)
{
#if defined(_WIN32)
	HMODULE hm = NULL;
	WCHAR* wstr = NULL;

	if (lpLibFileName)
		wstr = ConvertUtf8ToWCharAlloc(lpLibFileName, NULL);

	hm = LoadLibraryW(wstr);
	free(wstr);
	return hm;
#else
	return LoadLibraryA(lpLibFileName);
#endif
}

HMODULE LoadLibraryExX(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	if (!lpLibFileName)
		return NULL;
#if defined(_WIN32)
	HMODULE hm = NULL;
	WCHAR* wstr = ConvertUtf8ToWCharAlloc(lpLibFileName, NULL);
	if (wstr)
		hm = LoadLibraryExW(wstr, hFile, dwFlags);
	free(wstr);
	return hm;
#else
	return LoadLibraryExA(lpLibFileName, hFile, dwFlags);
#endif
}
