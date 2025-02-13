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
#include <winpr/wlog.h>

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

HANDLE CreateFileMappingA(WINPR_ATTR_UNUSED HANDLE hFile,
                          WINPR_ATTR_UNUSED LPSECURITY_ATTRIBUTES lpAttributes,
                          WINPR_ATTR_UNUSED DWORD flProtect,
                          WINPR_ATTR_UNUSED DWORD dwMaximumSizeHigh,
                          WINPR_ATTR_UNUSED DWORD dwMaximumSizeLow, WINPR_ATTR_UNUSED LPCSTR lpName)
{
	WLog_ERR("TODO", "TODO: Implement");
	if (hFile != INVALID_HANDLE_VALUE)
	{
		return NULL; /* not yet implemented */
	}

	return NULL;
}

HANDLE CreateFileMappingW(WINPR_ATTR_UNUSED HANDLE hFile,
                          WINPR_ATTR_UNUSED LPSECURITY_ATTRIBUTES lpAttributes,
                          WINPR_ATTR_UNUSED DWORD flProtect,
                          WINPR_ATTR_UNUSED DWORD dwMaximumSizeHigh,
                          WINPR_ATTR_UNUSED DWORD dwMaximumSizeLow,
                          WINPR_ATTR_UNUSED LPCWSTR lpName)
{
	WLog_ERR("TODO", "TODO: Implement");
	return NULL;
}

HANDLE OpenFileMappingA(WINPR_ATTR_UNUSED DWORD dwDesiredAccess,
                        WINPR_ATTR_UNUSED BOOL bInheritHandle, WINPR_ATTR_UNUSED LPCSTR lpName)
{
	WLog_ERR("TODO", "TODO: Implement");
	return NULL;
}

HANDLE OpenFileMappingW(WINPR_ATTR_UNUSED DWORD dwDesiredAccess,
                        WINPR_ATTR_UNUSED BOOL bInheritHandle, WINPR_ATTR_UNUSED LPCWSTR lpName)
{
	WLog_ERR("TODO", "TODO: Implement");
	return NULL;
}

LPVOID MapViewOfFile(WINPR_ATTR_UNUSED HANDLE hFileMappingObject,
                     WINPR_ATTR_UNUSED DWORD dwDesiredAccess,
                     WINPR_ATTR_UNUSED DWORD dwFileOffsetHigh,
                     WINPR_ATTR_UNUSED DWORD dwFileOffsetLow,
                     WINPR_ATTR_UNUSED size_t dwNumberOfBytesToMap)
{
	WLog_ERR("TODO", "TODO: Implement");
	return NULL;
}

LPVOID MapViewOfFileEx(WINPR_ATTR_UNUSED HANDLE hFileMappingObject,
                       WINPR_ATTR_UNUSED DWORD dwDesiredAccess,
                       WINPR_ATTR_UNUSED DWORD dwFileOffsetHigh,
                       WINPR_ATTR_UNUSED DWORD dwFileOffsetLow,
                       WINPR_ATTR_UNUSED size_t dwNumberOfBytesToMap,
                       WINPR_ATTR_UNUSED LPVOID lpBaseAddress)
{
	WLog_ERR("TODO", "TODO: Implement");
	return NULL;
}

BOOL FlushViewOfFile(WINPR_ATTR_UNUSED LPCVOID lpBaseAddress,
                     WINPR_ATTR_UNUSED size_t dwNumberOfBytesToFlush)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

BOOL UnmapViewOfFile(WINPR_ATTR_UNUSED LPCVOID lpBaseAddress)
{
	WLog_ERR("TODO", "TODO: Implement");
	return TRUE;
}

#endif
