/**
 * WinPR: Windows Portable Runtime
 * Shell Functions
 *
 * Copyright 2015 Dell Software <Mike.McDonald@software.dell.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#ifndef WINPR_SHELL_H
#define WINPR_SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifdef _WIN32

#include <shlobj.h>
#include <userenv.h>

#else

/* Shell clipboard formats */
typedef struct
{
	DWORD dwFlags;
	CLSID clsid;
	SIZEL sizel;
	POINTL pointl;
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	WCHAR cFileName[260];
} FILEDESCRIPTORW;

/* Legacy definition, some types do not match the windows equivalent. */
typedef struct
{
	DWORD dwFlags;
	BYTE clsid[16];
	BYTE sizel[8];
	BYTE pointl[8];
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	WCHAR cFileName[260];
} FILEDESCRIPTOR;

/* FILEDESCRIPTOR.dwFlags */
typedef enum
{
	FD_CLSID = 0x00000001,
	FD_SIZEPOINT = 0x00000002,
	FD_ATTRIBUTES = 0x00000004,
	FD_CREATETIME = 0x00000008,
	FD_ACCESSTIME = 0x00000010,
	FD_WRITESTIME = 0x00000020,
	FD_FILESIZE = 0x00000040,
	FD_PROGRESSUI = 0x00004000,
	FD_LINKUI = 0x00008000,
} FD_FLAGS;
#define FD_UNICODE 0x80000000

/* Deprecated, here for compatibility */
#define FD_SHOWPROGRESSUI FD_PROGRESSUI

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API BOOL GetUserProfileDirectoryA(HANDLE hToken, LPSTR lpProfileDir, LPDWORD lpcchSize);

	WINPR_API BOOL GetUserProfileDirectoryW(HANDLE hToken, LPWSTR lpProfileDir, LPDWORD lpcchSize);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define GetUserProfileDirectory GetUserProfileDirectoryW
#else
#define GetUserProfileDirectory GetUserProfileDirectoryA
#endif

#endif

#endif /* WINPR_SHELL_H */
