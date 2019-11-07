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

#ifndef WINPR_LIBRARY_H
#define WINPR_LIBRARY_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#if !defined(_WIN32) || defined(_UWP)

typedef HANDLE DLL_DIRECTORY_COOKIE;

#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x00000200
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#define LOAD_LIBRARY_SEARCH_USER_DIRS 0x00000400

#define DONT_RESOLVE_DLL_REFERENCES 0x00000001
#define LOAD_LIBRARY_AS_DATAFILE 0x00000002
#define LOAD_WITH_ALTERED_SEARCH_PATH 0x00000008
#define LOAD_IGNORE_CODE_AUTHZ_LEVEL 0x00000010
#define LOAD_LIBRARY_AS_IMAGE_RESOURCE 0x00000020
#define LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE 0x00000040
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0x00000100
#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x00000200
#define LOAD_LIBRARY_SEARCH_USER_DIRS 0x00000400
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x00001000

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API DLL_DIRECTORY_COOKIE AddDllDirectory(PCWSTR NewDirectory);
	WINPR_API BOOL RemoveDllDirectory(DLL_DIRECTORY_COOKIE Cookie);
	WINPR_API BOOL SetDefaultDllDirectories(DWORD DirectoryFlags);

	WINPR_API HMODULE LoadLibraryA(LPCSTR lpLibFileName);
	WINPR_API HMODULE LoadLibraryW(LPCWSTR lpLibFileName);

	WINPR_API HMODULE LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
	WINPR_API HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define LoadLibrary LoadLibraryW
#define LoadLibraryEx LoadLibraryExW
#else
#define LoadLibrary LoadLibraryA
#define LoadLibraryEx LoadLibraryExA
#endif

#endif

#if !defined(_WIN32) && !defined(__CYGWIN__)

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API HMODULE GetModuleHandleA(LPCSTR lpModuleName);
	WINPR_API HMODULE GetModuleHandleW(LPCWSTR lpModuleName);

	WINPR_API DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize);
	WINPR_API DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);

	WINPR_API FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);

	WINPR_API BOOL FreeLibrary(HMODULE hLibModule);

#ifdef __cplusplus
}
#endif

#ifdef UNICODE
#define GetModuleHandle GetModuleHandleW
#define GetModuleFileName GetModuleFileNameW
#else
#define GetModuleHandle GetModuleHandleA
#define GetModuleFileName GetModuleFileNameA
#endif

#endif

#endif /* WINPR_LIBRARY_H */
