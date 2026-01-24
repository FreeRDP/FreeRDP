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
extern "C"
{
#endif

	WINPR_API WINPR_ATTR_NODISCARD DWORD GetCurrentDirectoryA(DWORD nBufferLength, LPSTR lpBuffer);
	WINPR_API WINPR_ATTR_NODISCARD DWORD GetCurrentDirectoryW(DWORD nBufferLength, LPWSTR lpBuffer);

	WINPR_API WINPR_ATTR_NODISCARD BOOL SetCurrentDirectoryA(LPCSTR lpPathName);
	WINPR_API WINPR_ATTR_NODISCARD BOOL SetCurrentDirectoryW(LPCWSTR lpPathName);

	WINPR_API WINPR_ATTR_NODISCARD DWORD SearchPathA(LPCSTR lpPath, LPCSTR lpFileName,
	                                                 LPCSTR lpExtension, DWORD nBufferLength,
	                                                 LPSTR lpBuffer, LPSTR* lpFilePart);
	WINPR_API WINPR_ATTR_NODISCARD DWORD SearchPathW(LPCWSTR lpPath, LPCWSTR lpFileName,
	                                                 LPCWSTR lpExtension, DWORD nBufferLength,
	                                                 LPWSTR lpBuffer, LPWSTR* lpFilePart);

	WINPR_API WINPR_ATTR_NODISCARD LPSTR GetCommandLineA(VOID);
	WINPR_API WINPR_ATTR_NODISCARD LPWSTR GetCommandLineW(VOID);

	WINPR_API WINPR_ATTR_NODISCARD BOOL NeedCurrentDirectoryForExePathA(LPCSTR ExeName);
	WINPR_API WINPR_ATTR_NODISCARD BOOL NeedCurrentDirectoryForExePathW(LPCWSTR ExeName);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define GetCurrentDirectory GetCurrentDirectoryW
#define SetCurrentDirectory SetCurrentDirectoryW
#define SearchPath SearchPathW
#define GetCommandLine GetCommandLineW
#define NeedCurrentDirectoryForExePath NeedCurrentDirectoryForExePathW
#else
#define GetCurrentDirectory GetCurrentDirectoryA
#define SetCurrentDirectory SetCurrentDirectoryA
#define SearchPath SearchPathA
#define GetCommandLine GetCommandLineA
#define NeedCurrentDirectoryForExePath NeedCurrentDirectoryForExePathA
#endif

#endif

#if !defined(_WIN32) || defined(_UWP)

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API WINPR_ATTR_NODISCARD DWORD GetEnvironmentVariableA(LPCSTR lpName, LPSTR lpBuffer,
	                                                             DWORD nSize);
	WINPR_API WINPR_ATTR_NODISCARD DWORD GetEnvironmentVariableW(LPCWSTR lpName, LPWSTR lpBuffer,
	                                                             DWORD nSize);

	WINPR_API WINPR_ATTR_NODISCARD BOOL SetEnvironmentVariableA(LPCSTR lpName, LPCSTR lpValue);
	WINPR_API WINPR_ATTR_NODISCARD BOOL SetEnvironmentVariableW(LPCWSTR lpName, LPCWSTR lpValue);

	/**
	 * A brief history of the GetEnvironmentStrings functions:
	 * http://blogs.msdn.com/b/oldnewthing/archive/2013/01/17/10385718.aspx
	 */

	WINPR_API WINPR_ATTR_NODISCARD BOOL FreeEnvironmentStringsA(LPCH lpszEnvironmentBlock);
	WINPR_API WINPR_ATTR_NODISCARD BOOL FreeEnvironmentStringsW(LPWCH lpszEnvironmentBlock);

	WINPR_ATTR_MALLOC(FreeEnvironmentStringsA, 1)
	WINPR_API WINPR_ATTR_NODISCARD LPCH GetEnvironmentStrings(VOID);

	WINPR_ATTR_MALLOC(FreeEnvironmentStringsW, 1)
	WINPR_API WINPR_ATTR_NODISCARD LPWCH GetEnvironmentStringsW(VOID);

	WINPR_API WINPR_ATTR_NODISCARD BOOL SetEnvironmentStringsA(LPCH NewEnvironment);
	WINPR_API WINPR_ATTR_NODISCARD BOOL SetEnvironmentStringsW(LPWCH NewEnvironment);

	WINPR_API WINPR_ATTR_NODISCARD DWORD ExpandEnvironmentStringsA(LPCSTR lpSrc, LPSTR lpDst,
	                                                               DWORD nSize);
	WINPR_API WINPR_ATTR_NODISCARD DWORD ExpandEnvironmentStringsW(LPCWSTR lpSrc, LPWSTR lpDst,
	                                                               DWORD nSize);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define GetEnvironmentVariable GetEnvironmentVariableW
#define SetEnvironmentVariable SetEnvironmentVariableW
#define GetEnvironmentStrings GetEnvironmentStringsW
#define SetEnvironmentStrings SetEnvironmentStringsW
#define ExpandEnvironmentStrings ExpandEnvironmentStringsW
#define FreeEnvironmentStrings FreeEnvironmentStringsW
#else
#define GetEnvironmentVariable GetEnvironmentVariableA
#define SetEnvironmentVariable SetEnvironmentVariableA
#define GetEnvironmentStringsA GetEnvironmentStrings
#define SetEnvironmentStrings SetEnvironmentStringsA
#define ExpandEnvironmentStrings ExpandEnvironmentStringsA
#define FreeEnvironmentStrings FreeEnvironmentStringsA
#endif

#endif

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD LPCH MergeEnvironmentStrings(PCSTR original, PCSTR merge);

	WINPR_API WINPR_ATTR_NODISCARD DWORD GetEnvironmentVariableEBA(LPCSTR envBlock, LPCSTR lpName,
	                                                               LPSTR lpBuffer, DWORD nSize);
	WINPR_API WINPR_ATTR_NODISCARD BOOL SetEnvironmentVariableEBA(LPSTR* envBlock, LPCSTR lpName,
	                                                              LPCSTR lpValue);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD char** EnvironmentBlockToEnvpA(LPCH lpszEnvironmentBlock);

	WINPR_API WINPR_ATTR_NODISCARD DWORD GetEnvironmentVariableX(const char* lpName, char* lpBuffer,
	                                                             DWORD nSize);

	WINPR_ATTR_MALLOC(free, 1)
	WINPR_API WINPR_ATTR_NODISCARD char* GetEnvAlloc(LPCSTR lpName);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_ENVIRONMENT_H */
