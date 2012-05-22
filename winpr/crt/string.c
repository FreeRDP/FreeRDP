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

wchar_t* _wcsdup(const wchar_t* strSource)
{
	wchar_t* strDestination;

	if (strSource == NULL)
		return NULL;

#if sun
	strDestination = wsdup(strSource);
#else
	strDestination = wcsdup(strSource);
#endif

	if (strDestination == NULL)
		perror("wcsdup");

	return strDestination;
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
	printf("CharUpperW unimplemented!\n");

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
	printf("CharLowerW unimplemented!\n");

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
	printf("IsCharAlphaW unimplemented!\n");
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
	printf("IsCharAlphaNumericW unimplemented!\n");
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
	printf("IsCharUpperW unimplemented!\n");
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
	printf("IsCharLowerW unimplemented!\n");
	return 0;
}

#endif
