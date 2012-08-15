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

#ifndef _WIN32

WINPR_API HMODULE LoadLibraryA(LPCSTR lpLibFileName);
WINPR_API HMODULE LoadLibraryW(LPCWSTR lpLibFileName);

WINPR_API HMODULE LoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
WINPR_API HMODULE LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

#ifdef UNICODE
#define LoadLibrary	LoadLibraryW
#define LoadLibraryEx	LoadLibraryExW
#else
#define LoadLibrary	LoadLibraryA
#define LoadLibraryEx	LoadLibraryExA
#endif

WINPR_API FARPROC GetProcAddress(HMODULE hModule, LPCSTR lpProcName);

WINPR_API BOOL FreeLibrary(HMODULE hLibModule);

#endif

#endif /* WINPR_LIBRARY_H */

