/**
 * WinPR: Windows Portable Runtime
 * String Manipulation (CRT)
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

#ifndef WINPR_CRT_STRING_H
#define WINPR_CRT_STRING_H

#include <wchar.h>
#include <string.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

WINPR_API char* _strdup(const char* strSource);
WINPR_API wchar_t* _wcsdup(const wchar_t* strSource);

WINPR_API LPSTR CharUpperA(LPSTR lpsz);
WINPR_API LPWSTR CharUpperW(LPWSTR lpsz);

#ifdef UNICODE
#define CharUpper	CharUpperW
#else
#define CharUpper	CharUpperA
#endif

WINPR_API DWORD CharUpperBuffA(LPSTR lpsz, DWORD cchLength);
WINPR_API DWORD CharUpperBuffW(LPWSTR lpsz, DWORD cchLength);

#ifdef UNICODE
#define CharUpperBuff	CharUpperBuffW
#else
#define CharUpperBuff	CharUpperBuffA
#endif

WINPR_API LPSTR CharLowerA(LPSTR lpsz);
WINPR_API LPWSTR CharLowerW(LPWSTR lpsz);

#ifdef UNICODE
#define CharLower	CharLowerW
#else
#define CharLower	CharLowerA
#endif

WINPR_API DWORD CharLowerBuffA(LPSTR lpsz, DWORD cchLength);
WINPR_API DWORD CharLowerBuffW(LPWSTR lpsz, DWORD cchLength);

#ifdef UNICODE
#define CharLowerBuff	CharLowerBuffW
#else
#define CharLowerBuff	CharLowerBuffA
#endif

WINPR_API BOOL IsCharAlphaA(CHAR ch);
WINPR_API BOOL IsCharAlphaW(WCHAR ch);

#ifdef UNICODE
#define IsCharAlpha	IsCharAlphaW
#else
#define IsCharAlpha	IsCharAlphaA
#endif

WINPR_API BOOL IsCharAlphaNumericA(CHAR ch);
WINPR_API BOOL IsCharAlphaNumericW(WCHAR ch);

#ifdef UNICODE
#define IsCharAlphaNumeric	IsCharAlphaNumericW
#else
#define IsCharAlphaNumeric	IsCharAlphaNumericA
#endif

WINPR_API BOOL IsCharUpperA(CHAR ch);
WINPR_API BOOL IsCharUpperW(WCHAR ch);

#ifdef UNICODE
#define IsCharUpper	IsCharUpperW
#else
#define IsCharUpper	IsCharUpperA
#endif

WINPR_API BOOL IsCharLowerA(CHAR ch);
WINPR_API BOOL IsCharLowerW(WCHAR ch);

#ifdef UNICODE
#define IsCharLower	IsCharLowerW
#else
#define IsCharLower	IsCharLowerA
#endif

#endif

#endif /* WINPR_CRT_STRING_H */
