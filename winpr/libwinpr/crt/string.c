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

#include <winpr/config.h>
#include <winpr/assert.h>

#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <wctype.h>
#include <wchar.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/endian.h>

/* String Manipulation (CRT): http://msdn.microsoft.com/en-us/library/f0151s4x.aspx */

#include "../log.h"
#define TAG WINPR_TAG("crt")

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

BOOL winpr_str_append(const char* what, char* buffer, size_t size, const char* separator)
{
	const size_t used = strnlen(buffer, size);
	const size_t add = strnlen(what, size);
	const size_t sep_len = separator ? strnlen(separator, size) : 0;
	const size_t sep = (used > 0) ? sep_len : 0;

	if (used + add + sep >= size)
		return FALSE;

	if ((used > 0) && (sep_len > 0))
		strncat(buffer, separator, sep_len);

	strncat(buffer, what, add);
	return TRUE;
}

#ifndef _WIN32

char* _strdup(const char* strSource)
{
	char* strDestination;

	if (strSource == NULL)
		return NULL;

	strDestination = strdup(strSource);

	if (strDestination == NULL)
		WLog_ERR(TAG, "strdup");

	return strDestination;
}

WCHAR* _wcsdup(const WCHAR* strSource)
{
	size_t len = _wcslen(strSource);
	WCHAR* strDestination;

	strDestination = calloc(len + 1, sizeof(WCHAR));

	if (strDestination != NULL)
		memcpy(strDestination, strSource, len * sizeof(WCHAR));

	if (strDestination == NULL)
		WLog_ERR(TAG, "wcsdup");

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
	WINPR_ASSERT(string1);
	WINPR_ASSERT(string2);

	while (TRUE)
	{
		const WCHAR w1 = *string1++;
		const WCHAR w2 = *string2++;

		if (w1 != w2)
			return (int)w1 - w2;
		else if ((w1 == '\0') || (w2 == '\0'))
			return (int)w1 - w2;
	}
}

int _wcsncmp(const WCHAR* string1, const WCHAR* string2, size_t count)
{
	WINPR_ASSERT(string1);
	WINPR_ASSERT(string2);

	for (size_t x = 0; x < count; x++)
	{
		const WCHAR a = string1[x];
		const WCHAR b = string2[x];

		if (a != b)
			return (int)a - b;
		else if ((a == '\0') || (b == '\0'))
			return (int)a - b;
	}
	return 0;
}

/* _wcslen -> wcslen */

size_t _wcslen(const WCHAR* str)
{
	const WCHAR* p = (const WCHAR*)str;

	WINPR_ASSERT(p);

	while (*p)
		p++;

	return (size_t)(p - str);
}

/* _wcsnlen -> wcsnlen */

size_t _wcsnlen(const WCHAR* str, size_t max)
{
	size_t x;

	WINPR_ASSERT(str);

	for (x = 0; x < max; x++)
	{
		if (str[x] == 0)
			return x;
	}

	return x;
}

/* _wcsstr -> wcsstr */

WCHAR* _wcsstr(const WCHAR* str, const WCHAR* strSearch)
{
	WINPR_ASSERT(str);
	WINPR_ASSERT(strSearch);

	if (strSearch[0] == '\0')
		return (WCHAR*)str;

	const size_t searchLen = _wcslen(strSearch);
	while (*str)
	{
		if (_wcsncmp(str, strSearch, searchLen) == 0)
			return (WCHAR*)str;
		str++;
	}
	return NULL;
}

/* _wcschr -> wcschr */

WCHAR* _wcschr(const WCHAR* str, WCHAR value)
{
	union
	{
		const WCHAR* cc;
		WCHAR* c;
	} cnv;
	const WCHAR* p = (const WCHAR*)str;

	while (*p && (*p != value))
		p++;

	cnv.cc = (*p == value) ? p : NULL;
	return cnv.c;
}

/* _wcsrchr -> wcsrchr */

WCHAR* _wcsrchr(const WCHAR* str, WCHAR c)
{
	union
	{
		const WCHAR* cc;
		WCHAR* c;
	} cnv;
	const WCHAR* p = NULL;

	if (!str)
		return NULL;

	for (; *str != '\0'; str++)
	{
		const WCHAR ch = *str;
		if (ch == c)
			p = str;
	}

	cnv.cc = p;
	return cnv.c;
}

char* strtok_s(char* strToken, const char* strDelimit, char** context)
{
	return strtok_r(strToken, strDelimit, context);
}

WCHAR* wcstok_s(WCHAR* strToken, const WCHAR* strDelimit, WCHAR** context)
{
	WCHAR* nextToken;
	WCHAR value;

	if (!strToken)
		strToken = *context;

	Data_Read_UINT16(strToken, value);

	while (*strToken && _wcschr(strDelimit, value))
	{
		strToken++;
		Data_Read_UINT16(strToken, value);
	}

	if (!*strToken)
		return NULL;

	nextToken = strToken++;
	Data_Read_UINT16(strToken, value);

	while (*strToken && !(_wcschr(strDelimit, value)))
	{
		strToken++;
		Data_Read_UINT16(strToken, value);
	}

	if (*strToken)
		*strToken++ = 0;

	*context = strToken;
	return nextToken;
}

#endif

#if !defined(_WIN32) || defined(_UWP)

/* Windows API Sets - api-ms-win-core-string-l2-1-0.dll
 * http://msdn.microsoft.com/en-us/library/hh802935/
 */

#include "casing.c"

LPSTR CharUpperA(LPSTR lpsz)
{
	size_t i;
	size_t length;

	if (!lpsz)
		return NULL;

	length = strlen(lpsz);

	if (length < 1)
		return (LPSTR)NULL;

	if (length == 1)
	{
		char c = *lpsz;

		if ((c >= 'a') && (c <= 'z'))
			c = c - 'a' + 'A';

		*lpsz = c;
		return lpsz;
	}

	for (i = 0; i < length; i++)
	{
		if ((lpsz[i] >= 'a') && (lpsz[i] <= 'z'))
			lpsz[i] = lpsz[i] - 'a' + 'A';
	}

	return lpsz;
}

LPWSTR CharUpperW(LPWSTR lpsz)
{
	size_t i;
	size_t length;

	if (!lpsz)
		return NULL;

	length = _wcslen(lpsz);

	if (length < 1)
		return (LPWSTR)NULL;

	if (length == 1)
	{
		WCHAR c = *lpsz;

		if ((c >= L'a') && (c <= L'z'))
			c = c - L'a' + L'A';

		*lpsz = c;
		return lpsz;
	}

	for (i = 0; i < length; i++)
	{
		if ((lpsz[i] >= L'a') && (lpsz[i] <= L'z'))
			lpsz[i] = lpsz[i] - L'a' + L'A';
	}

	return lpsz;
}

DWORD CharUpperBuffA(LPSTR lpsz, DWORD cchLength)
{
	DWORD i;

	if (cchLength < 1)
		return 0;

	for (i = 0; i < cchLength; i++)
	{
		if ((lpsz[i] >= 'a') && (lpsz[i] <= 'z'))
			lpsz[i] = lpsz[i] - 'a' + 'A';
	}

	return cchLength;
}

DWORD CharUpperBuffW(LPWSTR lpsz, DWORD cchLength)
{
	DWORD i;
	WCHAR value;

	for (i = 0; i < cchLength; i++)
	{
		Data_Read_UINT16(&lpsz[i], value);
		value = WINPR_TOUPPERW(value);
		Data_Write_UINT16(&lpsz[i], value);
	}

	return cchLength;
}

LPSTR CharLowerA(LPSTR lpsz)
{
	size_t i;
	size_t length;

	if (!lpsz)
		return (LPSTR)NULL;

	length = strlen(lpsz);

	if (length < 1)
		return (LPSTR)NULL;

	if (length == 1)
	{
		char c = *lpsz;

		if ((c >= 'A') && (c <= 'Z'))
			c = c - 'A' + 'a';

		*lpsz = c;
		return lpsz;
	}

	for (i = 0; i < length; i++)
	{
		if ((lpsz[i] >= 'A') && (lpsz[i] <= 'Z'))
			lpsz[i] = lpsz[i] - 'A' + 'a';
	}

	return lpsz;
}

LPWSTR CharLowerW(LPWSTR lpsz)
{
	CharLowerBuffW(lpsz, _wcslen(lpsz));
	return lpsz;
}

DWORD CharLowerBuffA(LPSTR lpsz, DWORD cchLength)
{
	DWORD i;

	if (cchLength < 1)
		return 0;

	for (i = 0; i < cchLength; i++)
	{
		if ((lpsz[i] >= 'A') && (lpsz[i] <= 'Z'))
			lpsz[i] = lpsz[i] - 'A' + 'a';
	}

	return cchLength;
}

DWORD CharLowerBuffW(LPWSTR lpsz, DWORD cchLength)
{
	DWORD i;
	WCHAR value;

	for (i = 0; i < cchLength; i++)
	{
		Data_Read_UINT16(&lpsz[i], value);
		value = WINPR_TOLOWERW(value);
		Data_Write_UINT16(&lpsz[i], value);
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
	if (((ch >= L'a') && (ch <= L'z')) || ((ch >= L'A') && (ch <= L'Z')))
		return 1;
	else
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
	if (((ch >= L'a') && (ch <= L'z')) || ((ch >= L'A') && (ch <= L'Z')) ||
	    ((ch >= L'0') && (ch <= L'9')))
		return 1;
	else
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
	if ((ch >= L'A') && (ch <= L'Z'))
		return 1;
	else
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
	if ((ch >= L'a') && (ch <= L'z'))
		return 1;
	else
		return 0;
}

int lstrlenA(LPCSTR lpString)
{
	return (int)strlen(lpString);
}

int lstrlenW(LPCWSTR lpString)
{
	LPCWSTR p;

	if (!lpString)
		return 0;

	p = (LPCWSTR)lpString;

	while (*p)
		p++;

	return (int)(p - lpString);
}

int lstrcmpA(LPCSTR lpString1, LPCSTR lpString2)
{
	return strcmp(lpString1, lpString2);
}

int lstrcmpW(LPCWSTR lpString1, LPCWSTR lpString2)
{
	WCHAR value1, value2;

	while (*lpString1 && (*lpString1 == *lpString2))
	{
		lpString1++;
		lpString2++;
	}

	Data_Read_UINT16(lpString1, value1);
	Data_Read_UINT16(lpString2, value2);
	return value1 - value2;
}

#endif

size_t ConvertLineEndingToLF(char* str, size_t size)
{
	size_t skip = 0;

	WINPR_ASSERT(str || (size == 0));
	for (size_t x = 0; x < size; x++)
	{
		char c = str[x];
		switch (c)
		{
			case '\r':
				str[x - skip] = '\n';
				if ((x + 1 < size) && (str[x + 1] == '\n'))
					skip++;
				break;
			default:
				str[x - skip] = c;
				break;
		}
	}
	return size - skip;
}

char* ConvertLineEndingToCRLF(const char* str, size_t* size)
{
	WINPR_ASSERT(size);
	const size_t s = *size;
	WINPR_ASSERT(str || (s == 0));

	*size = 0;
	if (s == 0)
		return NULL;

	size_t linebreaks = 0;
	for (size_t x = 0; x < s - 1; x++)
	{
		char c = str[x];
		switch (c)
		{
			case '\r':
			case '\n':
				linebreaks++;
				break;
			default:
				break;
		}
	}
	char* cnv = calloc(s + linebreaks * 2ull + 1ull, sizeof(char));
	if (!cnv)
		return NULL;

	size_t pos = 0;
	for (size_t x = 0; x < s; x++)
	{
		const char c = str[x];
		switch (c)
		{
			case '\r':
				cnv[pos++] = '\r';
				cnv[pos++] = '\n';
				break;
			case '\n':
				/* Do not duplicate existing \r\n sequences */
				if ((x > 0) && (str[x - 1] != '\r'))
				{
					cnv[pos++] = '\r';
					cnv[pos++] = '\n';
				}
				break;
			default:
				cnv[pos++] = c;
				break;
		}
	}
	*size = pos;
	return cnv;
}

char* StrSep(char** stringp, const char* delim)
{
	char* start = *stringp;
	char* p;
	p = (start != NULL) ? strpbrk(start, delim) : NULL;

	if (!p)
		*stringp = NULL;
	else
	{
		*p = '\0';
		*stringp = p + 1;
	}

	return start;
}

INT64 GetLine(char** lineptr, size_t* size, FILE* stream)
{
#if defined(_WIN32)
	char c;
	char* n;
	size_t step = 32;
	size_t used = 0;

	if (!lineptr || !size)
	{
		errno = EINVAL;
		return -1;
	}

	do
	{
		if (used + 2 >= *size)
		{
			*size += step;
			n = realloc(*lineptr, *size);

			if (!n)
			{
				return -1;
			}

			*lineptr = n;
		}

		c = fgetc(stream);

		if (c != EOF)
			(*lineptr)[used++] = c;
	} while ((c != '\n') && (c != '\r') && (c != EOF));

	(*lineptr)[used] = '\0';
	return used;
#elif !defined(ANDROID) && !defined(IOS)
	return getline(lineptr, size, stream);
#else
	return -1;
#endif
}

#if !defined(HAVE_STRNDUP)
char* strndup(const char* src, size_t n)
{
	char* dst = calloc(n + 1, sizeof(char));
	if (dst)
		strncpy(dst, src, n);
	return dst;
}
#endif
