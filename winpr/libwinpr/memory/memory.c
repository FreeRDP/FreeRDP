/**
 * WinPR: Windows Portable Runtime
 * Memory Functions
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/memory.h>

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

/**
 * api-ms-win-core-memory-l1-1-2.dll:
 *
 * AllocateUserPhysicalPages
 * AllocateUserPhysicalPagesNuma
 * CreateFileMappingFromApp
 * CreateFileMappingNumaW
 * CreateFileMappingW
 * CreateMemoryResourceNotification
 * FlushViewOfFile
 * FreeUserPhysicalPages
 * GetLargePageMinimum
 * GetMemoryErrorHandlingCapabilities
 * GetProcessWorkingSetSizeEx
 * GetSystemFileCacheSize
 * GetWriteWatch
 * MapUserPhysicalPages
 * MapViewOfFile
 * MapViewOfFileEx
 * MapViewOfFileFromApp
 * OpenFileMappingW
 * PrefetchVirtualMemory
 * QueryMemoryResourceNotification
 * ReadProcessMemory
 * RegisterBadMemoryNotification
 * ResetWriteWatch
 * SetProcessWorkingSetSizeEx
 * SetSystemFileCacheSize
 * UnmapViewOfFile
 * UnmapViewOfFileEx
 * UnregisterBadMemoryNotification
 * VirtualAlloc
 * VirtualAllocEx
 * VirtualAllocExNuma
 * VirtualFree
 * VirtualFreeEx
 * VirtualLock
 * VirtualProtect
 * VirtualProtectEx
 * VirtualQuery
 * VirtualQueryEx
 * VirtualUnlock
 * WriteProcessMemory
 */

#ifndef _WIN32

#include "memory.h"

HANDLE CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttributes, DWORD flProtect,
                          DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCSTR lpName)
{
	if (hFile != INVALID_HANDLE_VALUE)
	{
		return NULL; /* not yet implemented */
	}

	return NULL;
}

HANDLE CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttributes, DWORD flProtect,
                          DWORD dwMaximumSizeHigh, DWORD dwMaximumSizeLow, LPCWSTR lpName)
{
	return NULL;
}

HANDLE OpenFileMappingA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
{
	return NULL;
}

HANDLE OpenFileMappingW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	return NULL;
}

LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh,
                     DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap)
{
	return NULL;
}

LPVOID MapViewOfFileEx(HANDLE hFileMappingObject, DWORD dwDesiredAccess, DWORD dwFileOffsetHigh,
                       DWORD dwFileOffsetLow, SIZE_T dwNumberOfBytesToMap, LPVOID lpBaseAddress)
{
	return NULL;
}

BOOL FlushViewOfFile(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush)
{
	return TRUE;
}

BOOL UnmapViewOfFile(LPCVOID lpBaseAddress)
{
	return TRUE;
}

#endif
