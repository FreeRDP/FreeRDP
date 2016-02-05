/**
 * WinPR: Windows Portable Runtime
 * Process Environment Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef WINPR_ENVIRONMENT_H
#define WINPR_ENVIRONMENT_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
WINPR_API DWORD GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer);

WINPR_API BOOL SetCurrentDirectoryA(LPCSTR lpPathName);
WINPR_API BOOL SetCurrentDirectoryW(LPCWSTR lpPathName);

WINPR_API DWORD SearchPathA(LPCSTR lpPath, LPCSTR lpFileName, LPCSTR lpExtension, DWORD nBufferLength, LPSTR lpBuffer, LPSTR* lpFilePart);
WINPR_API DWORD SearchPathW(LPCWSTR lpPath, LPCWSTR lpFileName, LPCWSTR lpExtension, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR* lpFilePart);

WINPR_API LPSTR GetCommandLineA(VOID);
WINPR_API LPWSTR GetCommandLineW(VOID);

WINPR_API BOOL NeedCurrentDirectoryForExePathA(LPCSTR ExeName);
WINPR_API BOOL NeedCurrentDirectoryForExePathW(LPCWSTR ExeName);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define GetCurrentDirectory		GetCurrentDirectoryW
#define SetCurrentDirectory		SetCurrentDirectoryW
#define SearchPath			SearchPathW
#define GetCommandLine			GetCommandLineW
#define NeedCurrentDirectoryForExePath	NeedCurrentDirectoryForExePathW
#else
#define GetCurrentDirectory		GetCurrentDirectoryA
#define SetCurrentDirectory		SetCurrentDirectoryA
#define SearchPath			SearchPathA
#define GetCommandLine			GetCommandLineA
#define NeedCurrentDirectoryForExePath	NeedCurrentDirectoryForExePathA
#endif

#endif

#if !defined(_WIN32) || defined(_UWP)

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);
WINPR_API DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);

WINPR_API BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);
WINPR_API BOOL SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue);

/**
 * A brief history of the GetEnvironmentStrings functions:
 * http://blogs.msdn.com/b/oldnewthing/archive/2013/01/17/10385718.aspx
 */

WINPR_API LPCH GetEnvironmentStrings(VOID);
WINPR_API LPWCH GetEnvironmentStringsW(VOID);

WINPR_API BOOL SetEnvironmentStringsA(LPCH NewEnvironment);
WINPR_API BOOL SetEnvironmentStringsW(LPWCH NewEnvironment);

WINPR_API DWORD ExpandEnvironmentStringsA(LPCSTR lpSrc, LPSTR lpDst, DWORD nSize);
WINPR_API DWORD ExpandEnvironmentStringsW(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize);

WINPR_API BOOL FreeEnvironmentStringsA(LPCH lpszEnvironmentBlock);
WINPR_API BOOL FreeEnvironmentStringsW(LPWCH lpszEnvironmentBlock);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define GetEnvironmentVariable		GetEnvironmentVariableW
#define SetEnvironmentVariable		SetEnvironmentVariableW
#define GetEnvironmentStrings		GetEnvironmentStringsW
#define SetEnvironmentStrings		SetEnvironmentStringsW
#define ExpandEnvironmentStrings	ExpandEnvironmentStringsW
#define FreeEnvironmentStrings		FreeEnvironmentStringsW
#else
#define GetEnvironmentVariable		GetEnvironmentVariableA
#define SetEnvironmentVariable		SetEnvironmentVariableA
#define GetEnvironmentStringsA		GetEnvironmentStrings
#define SetEnvironmentStrings		SetEnvironmentStringsA
#define ExpandEnvironmentStrings	ExpandEnvironmentStringsA
#define FreeEnvironmentStrings		FreeEnvironmentStringsA
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

WINPR_API LPCH MergeEnvironmentStrings(PCSTR original, PCSTR merge);

WINPR_API DWORD GetEnvironmentVariableEBA(LPCSTR envBlock, LPCSTR lpName, LPSTR lpBuffer, DWORD nSize);
WINPR_API BOOL SetEnvironmentVariableEBA(LPSTR* envBlock, LPCSTR lpName, LPCSTR lpValue);

WINPR_API char** EnvironmentBlockToEnvpA(LPCH lpszEnvironmentBlock);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_ENVIRONMENT_H */

