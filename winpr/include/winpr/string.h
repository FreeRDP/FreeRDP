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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32

#define CSTR_LESS_THAN			1
#define CSTR_EQUAL			2
#define CSTR_GREATER_THAN		3

#define CP_ACP				0
#define CP_OEMCP			1
#define CP_MACCP			2
#define CP_THREAD_ACP			3
#define CP_SYMBOL			42
#define CP_UTF7				65000
#define CP_UTF8				65001

#define MB_PRECOMPOSED			0x00000001
#define MB_COMPOSITE			0x00000002
#define MB_USEGLYPHCHARS		0x00000004
#define MB_ERR_INVALID_CHARS		0x00000008

WINPR_API char* _strdup(const char* strSource);
WINPR_API WCHAR* _wcsdup(const WCHAR* strSource);

WINPR_API int _stricmp(const char* string1, const char* string2);
WINPR_API int _strnicmp(const char* string1, const char* string2, size_t count);

WINPR_API int _wcscmp(const WCHAR* string1, const WCHAR* string2);

WINPR_API size_t _wcslen(const WCHAR* str);
WINPR_API WCHAR* _wcschr(const WCHAR* str, WCHAR c);

WINPR_API char* strtok_s(char* strToken, const char* strDelimit, char** context);
WINPR_API WCHAR* wcstok_s(WCHAR* strToken, const WCHAR* strDelimit, WCHAR** context);

#else

#define _wcscmp		wcscmp
#define _wcslen		wcslen
#define _wcschr		wcschr

#endif

#if !defined(_WIN32) || defined(_UWP)

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

WINPR_API int lstrlenA(LPCSTR lpString);
WINPR_API int lstrlenW(LPCWSTR lpString);

#ifdef UNICODE
#define lstrlen		lstrlenW
#else
#define lstrlen		lstrlenA
#endif

WINPR_API int lstrcmpA(LPCSTR lpString1, LPCSTR lpString2);
WINPR_API int lstrcmpW(LPCWSTR lpString1, LPCWSTR lpString2);

#ifdef UNICODE
#define lstrcmp		lstrcmpW
#else
#define lstrcmp		lstrcmpA
#endif

#endif

#ifndef _WIN32

#define	 sprintf_s	snprintf
#define	 _snprintf	snprintf
#define _scprintf(_fmt, ...) snprintf(NULL, 0, _fmt, ## __VA_ARGS__)

#define _scprintf(_fmt, ...)	snprintf(NULL, 0, _fmt, ## __VA_ARGS__)

/* Unicode Conversion */

WINPR_API int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
		int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);

WINPR_API int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
		LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);

#endif

/* Extended API */

WINPR_API int ConvertToUnicode(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
		int cbMultiByte, LPWSTR* lpWideCharStr, int cchWideChar);

WINPR_API int ConvertFromUnicode(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
		LPSTR* lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar);

WINPR_API void ByteSwapUnicode(WCHAR* wstr, int length);

WINPR_API int ConvertLineEndingToLF(char* str, int size);
WINPR_API char* ConvertLineEndingToCRLF(const char* str, int* size);

WINPR_API char* StrSep(char** stringp, const char* delim);

WINPR_API INT64 GetLine(char** lineptr, size_t* size, FILE* stream);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_CRT_STRING_H */
