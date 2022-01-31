/**
 * WinPR: Windows Portable Runtime
 * Memory Allocation
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

#ifndef WINPR_MEMORY_H
#define WINPR_MEMORY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/crt.h>
#include <winpr/file.h>

#ifndef _WIN32

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API HANDLE CreateFileMappingA(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttributes,
	                                    DWORD flProtect, DWORD dwMaximumSizeHigh,
	                                    DWORD dwMaximumSizeLow, LPCSTR lpName);
	WINPR_API HANDLE CreateFileMappingW(HANDLE hFile, LPSECURITY_ATTRIBUTES lpAttributes,
	                                    DWORD flProtect, DWORD dwMaximumSizeHigh,
	                                    DWORD dwMaximumSizeLow, LPCWSTR lpName);

	WINPR_API HANDLE OpenFileMappingA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
	WINPR_API HANDLE OpenFileMappingW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);

	WINPR_API LPVOID MapViewOfFile(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
	                               DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow,
	                               SIZE_T dwNumberOfBytesToMap);

	WINPR_API LPVOID MapViewOfFileEx(HANDLE hFileMappingObject, DWORD dwDesiredAccess,
	                                 DWORD dwFileOffsetHigh, DWORD dwFileOffsetLow,
	                                 SIZE_T dwNumberOfBytesToMap, LPVOID lpBaseAddress);

	WINPR_API BOOL FlushViewOfFile(LPCVOID lpBaseAddress, SIZE_T dwNumberOfBytesToFlush);

	WINPR_API BOOL UnmapViewOfFile(LPCVOID lpBaseAddress);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define CreateFileMapping CreateFileMappingW
#define OpenFileMapping OpenFileMappingW
#else
#define CreateFileMapping CreateFileMappingA
#define OpenFileMapping OpenFileMappingA
#endif

#endif

#endif /* WINPR_MEMORY_H */
