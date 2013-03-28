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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <wctype.h>

#include <winpr/crt.h>

/* String Manipulation (CRT): http://msdn.microsoft.com/en-us/library/f0151s4x.aspx */

#ifndef _WIN32

char* _strdup(const char* strSource)
{
	char* strDestination;

	if (strSource == NULL)
		return NULL;

	strDestination = strdup(strSource);

	if (strDestination == NULL)
		perror("strdup");

	return strDestination;
}

WCHAR* _wcsdup(const WCHAR* strSource)
{
	WCHAR* strDestination;

	if (strSource == NULL)
		return NULL;

#if sun
	strDestination = wsdup(strSource);
#elif defined(__APPLE__) && defined(__MACH__) || defined(ANDROID)
	strDestination = malloc(wcslen((wchar_t*)strSource));

	if (strDestination != NULL)
		wcscpy((wchar_t*)strDestination, (const wchar_t*)strSource);
#else
	strDestination = (WCHAR*) wcsdup((wchar_t*) strSource);
#endif

	if (strDestination == NULL)
		perror("wcsdup");

	return strDestination;
}

int _stricmp(const char* string1, const char* string2)
{
	return strcasecmp(string1, string2);
}

int _strnicmp(const char* string1, const char* string2, size_t count)
{
	return strncasecmp(string1, string2, count);
}

/* _wcscmp -> wcscmp */

int _wcscmp(const WCHAR* string1, const WCHAR* string2)
{
	while (*string1 && (*string1 == *string2))
	{
		string1++;
		string2++;
	}

	return *string1 - *string2;
}

/* _wcslen -> wcslen */

size_t _wcslen(const WCHAR* str)
{
	WCHAR* p = (WCHAR*) str;

	if (!p)
		return 0;

	while (*p)
		p++;

	return (p - str);
}

/* _wcschr -> wcschr */

WCHAR* _wcschr(const WCHAR* str, WCHAR c)
{
	WCHAR* p = (WCHAR*) str;

	while (*p && (*p != c))
		p++;

	return ((*p == c) ? p : NULL);
}

char* strtok_s(char* strToken, const char* strDelimit, char** context)
{
	return strtok_r(strToken, strDelimit, context);
}

WCHAR* wcstok_s(WCHAR* strToken, const WCHAR* strDelimit, WCHAR** context)
{
	WCHAR* nextToken;

	if (!strToken)
		strToken = *context;

	while (*strToken && _wcschr(strDelimit, *strToken))
		strToken++;

	if (!*strToken)
		return NULL;

	nextToken = strToken++;

	while (*strToken && !(_wcschr(strDelimit, *strToken)))
		strToken++;

	if (*strToken)
		*strToken++ = 0;

	*context = strToken;

	return nextToken;
}

/* Windows API Sets - api-ms-win-core-string-l2-1-0.dll
 * http://msdn.microsoft.com/en-us/library/hh802935/
 */

LPSTR CharUpperA(LPSTR lpsz)
{
	int i;
	int length;

	length = strlen(lpsz);

	if (length < 1)
		return (LPSTR) NULL;

	if (length == 1)
	{
		LPSTR pc = NULL;
		char c = *lpsz;

		if ((c >= 'a') && (c <= 'z'))
			c = c - 32;

		*pc = c;

		return pc;
	}

	for (i = 0; i < length; i++)
	{
		if ((lpsz[i] >= 'a') && (lpsz[i] <= 'z'))
			lpsz[i] = lpsz[i] - 32;
	}

	return lpsz;
}

LPWSTR CharUpperW(LPWSTR lpsz)
{
	fprintf(stderr, "CharUpperW unimplemented!\n");

	return (LPWSTR) NULL;
}

DWORD CharUpperBuffA(LPSTR lpsz, DWORD cchLength)
{
	int i;

	if (cchLength < 1)
		return 0;

	for (i = 0; i < cchLength; i++)
	{
		if ((lpsz[i] >= 'a') && (lpsz[i] <= 'z'))
			lpsz[i] = lpsz[i] - 32;
	}

	return cchLength;
}

DWORD CharUpperBuffW(LPWSTR lpsz, DWORD cchLength)
{
	DWORD i;
	unsigned char* p;
	unsigned int wc, uwc;

	p = (unsigned char*) lpsz;

	for (i = 0; i < cchLength; i++)
	{
		wc = (unsigned int) (*p);
		wc += (unsigned int) (*(p + 1)) << 8;

		uwc = towupper(wc);

		if (uwc != wc)
		{
			*p = uwc & 0xFF;
			*(p + 1) = (uwc >> 8) & 0xFF;
		}

		p += 2;
	}

	return cchLength;
}

LPSTR CharLowerA(LPSTR lpsz)
{
	int i;
	int length;

	length = strlen(lpsz);

	if (length < 1)
		return (LPSTR) NULL;

	if (length == 1)
	{
		LPSTR pc = NULL;
		char c = *lpsz;

		if ((c >= 'A') && (c <= 'Z'))
			c = c + 32;

		*pc = c;

		return pc;
	}

	for (i = 0; i < length; i++)
	{
		if ((lpsz[i] >= 'A') && (lpsz[i] <= 'Z'))
			lpsz[i] = lpsz[i] + 32;
	}

	return lpsz;
}

LPWSTR CharLowerW(LPWSTR lpsz)
{
	fprintf(stderr, "CharLowerW unimplemented!\n");

	return (LPWSTR) NULL;
}

DWORD CharLowerBuffA(LPSTR lpsz, DWORD cchLength)
{
	int i;

	if (cchLength < 1)
		return 0;

	for (i = 0; i < cchLength; i++)
	{
		if ((lpsz[i] >= 'A') && (lpsz[i] <= 'Z'))
			lpsz[i] = lpsz[i] + 32;
	}

	return cchLength;
}

DWORD CharLowerBuffW(LPWSTR lpsz, DWORD cchLength)
{
	DWORD i;
	unsigned char* p;
	unsigned int wc, uwc;

	p = (unsigned char*) lpsz;

	for (i = 0; i < cchLength; i++)
	{
		wc = (unsigned int) (*p);
		wc += (unsigned int) (*(p + 1)) << 8;

		uwc = towlower(wc);

		if (uwc != wc)
		{
			*p = uwc & 0xFF;
			*(p + 1) = (uwc >> 8) & 0xFF;
		}

		p += 2;
	}

	return cchLength;
}

BOOL IsCharAlphaA(CHAR ch)
{
	if (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')))
		return 1;
	else
		return 0;
}

BOOL IsCharAlphaW(WCHAR ch)
{
	fprintf(stderr, "IsCharAlphaW unimplemented!\n");
	return 0;
}

BOOL IsCharAlphaNumericA(CHAR ch)
{
	if (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) ||
			((ch >= '0') && (ch <= '9')))
		return 1;
	else
		return 0;
}

BOOL IsCharAlphaNumericW(WCHAR ch)
{
	fprintf(stderr, "IsCharAlphaNumericW unimplemented!\n");
	return 0;
}

BOOL IsCharUpperA(CHAR ch)
{
	if ((ch >= 'A') && (ch <= 'Z'))
		return 1;
	else
		return 0;
}

BOOL IsCharUpperW(WCHAR ch)
{
	fprintf(stderr, "IsCharUpperW unimplemented!\n");
	return 0;
}

BOOL IsCharLowerA(CHAR ch)
{
	if ((ch >= 'a') && (ch <= 'z'))
		return 1;
	else
		return 0;
}

BOOL IsCharLowerW(WCHAR ch)
{
	fprintf(stderr, "IsCharLowerW unimplemented!\n");
	return 0;
}

int lstrlenA(LPCSTR lpString)
{
	return strlen(lpString);
}

int lstrlenW(LPCWSTR lpString)
{
	LPWSTR p;

	if (!lpString)
		return 0;

	p = (LPWSTR) lpString;

	while (*p)
		p++;

	return p - lpString;
}

int lstrcmpA(LPCSTR lpString1, LPCSTR lpString2)
{
	return strcmp(lpString1, lpString2);
}

int lstrcmpW(LPCWSTR lpString1, LPCWSTR lpString2)
{
	while (*lpString1 && (*lpString1 == *lpString2))
	{
		lpString1++;
		lpString2++;
	}

	return *lpString1 - *lpString2;
}

#endif
