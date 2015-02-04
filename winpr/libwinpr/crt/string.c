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

#include "casing.c"

#include "../log.h"
#define TAG WINPR_TAG("crt")

char* _strdup(const char* strSource)
{
	char* strDestination;

	if (strSource == NULL)
		return NULL;

	strDestination = strdup(strSource);

	if (strDestination == NULL)
		WLog_ERR(TAG,"strdup");

	return strDestination;
}

WCHAR* _wcsdup(const WCHAR* strSource)
{
	WCHAR* strDestination;

	if (strSource == NULL)
		return NULL;

#if defined(__APPLE__) && defined(__MACH__) || defined(ANDROID) || defined(sun)
	strDestination = malloc(wcslen((wchar_t*)strSource));

	if (strDestination != NULL)
		wcscpy((wchar_t*)strDestination, (const wchar_t*)strSource);

#else
	strDestination = (WCHAR*) wcsdup((wchar_t*) strSource);
#endif

	if (strDestination == NULL)
		WLog_ERR(TAG,"wcsdup");

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

	if (!lpsz)
		return NULL;

	length = strlen(lpsz);

	if (length < 1)
		return (LPSTR) NULL;

	if (length == 1)
	{
		char c = *lpsz;

		if ((c >= 'a') && (c <= 'z'))
			c = c - 32;

		*lpsz = c;
		return lpsz;
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
	WLog_ERR(TAG, "CharUpperW unimplemented!");
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

	for (i = 0; i < cchLength; i++)
	{
		lpsz[i] = WINPR_TOUPPERW(lpsz[i]);
	}

	return cchLength;
}

LPSTR CharLowerA(LPSTR lpsz)
{
	int i;
	int length;

	if (!lpsz)
		return (LPSTR) NULL;

	length = strlen(lpsz);

	if (length < 1)
		return (LPSTR) NULL;

	if (length == 1)
	{
		char c = *lpsz;

		if ((c >= 'A') && (c <= 'Z'))
			c = c + 32;

		*lpsz = c;
		return lpsz;
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
	WLog_ERR(TAG, "CharLowerW unimplemented!");
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

	for (i = 0; i < cchLength; i++)
	{
		lpsz[i] = WINPR_TOLOWERW(lpsz[i]);
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
	WLog_ERR(TAG, "IsCharAlphaW unimplemented!");
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
	WLog_ERR(TAG, "IsCharAlphaNumericW unimplemented!");
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
	WLog_ERR(TAG, "IsCharUpperW unimplemented!");
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
	WLog_ERR(TAG, "IsCharLowerW unimplemented!");
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

int ConvertLineEndingToLF(char* str, int size)
{
	int status;
	char* end;
	char* pInput;
	char* pOutput;

	end = &str[size];
	pInput = pOutput = str;

	while (pInput < end)
	{
		if ((pInput[0] == '\r') && (pInput[1] == '\n'))
		{
			*pOutput++ = '\n';
			pInput += 2;
		}
		else
		{
			*pOutput++ = *pInput++;
		}
	}

	status = pOutput - str;

	return status;
}

char* ConvertLineEndingToCRLF(const char* str, int* size)
{
	int count;
	char* newStr;
	char* pOutput;
	const char* end;
	const char* pInput;

	end = &str[*size];

	count = 0;
	pInput = str;

	while (pInput < end)
	{
		if (*pInput == '\n')
			count++;

		pInput++;
	}

	newStr = (char*) malloc(*size + (count * 2) + 1);

	if (!newStr)
		return NULL;

	pInput = str;
	pOutput = newStr;

	while (pInput < end)
	{
		if ((*pInput == '\n') && ((pInput > str) && (pInput[-1] != '\r')))
		{
			*pOutput++ = '\r';
			*pOutput++ = '\n';
		}
		else
		{
			*pOutput++ = *pInput;
		}

		pInput++;
	}

	*size = pOutput - newStr;

	return newStr;
}

