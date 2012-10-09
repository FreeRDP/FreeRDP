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

char* strtok_s(char* strToken, const char* strDelimit, char** context)
{
	return strtok_r(strToken, strDelimit, context);
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

/*
 * Advanced String Techniques in C++ - Part I: Unicode
 * http://www.flipcode.com/archives/Advanced_String_Techniques_in_C-Part_I_Unicode.shtml
 */

/*
 * Conversion *to* Unicode
 * MultiByteToWideChar: http://msdn.microsoft.com/en-us/library/windows/desktop/dd319072/
 */

int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
		int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar)
{
	size_t ibl;
	size_t obl;
	char* pin;
	char* pout;
	char* pout0;

	if (lpMultiByteStr == NULL)
		return 0;

	if (cbMultiByte < 0)
		cbMultiByte = strlen(lpMultiByteStr) + 1;

	ibl = cbMultiByte;
	obl = 2 * ibl;

	if (cchWideChar < 1)
		return (obl / 2);

	pin = (char*) lpMultiByteStr;
	pout0 = (char*) lpWideCharStr;
	pout = pout0;

#ifdef HAVE_ICONV
	{
		iconv_t* out_iconv_h;

		out_iconv_h = iconv_open(WINDOWS_CODEPAGE, DEFAULT_CODEPAGE);

		if (errno == EINVAL)
		{
			printf("Error opening iconv converter to %s from %s\n", WINDOWS_CODEPAGE, DEFAULT_CODEPAGE);
			return 0;
		}

		if (iconv(out_iconv_h, (ICONV_CONST char **) &pin, &ibl, &pout, &obl) == (size_t) - 1)
		{
			printf("MultiByteToWideChar: iconv() error\n");
			return NULL;
		}

		iconv_close(out_iconv_h);
	}
#else
	while ((ibl > 0) && (obl > 0))
	{
		unsigned int wc;

		wc = (unsigned int) (unsigned char) (*pin++);
		ibl--;

		if (wc >= 0xF0)
		{
			wc = (wc - 0xF0) << 18;
			wc += ((unsigned int) (unsigned char) (*pin++) - 0x80) << 12;
			wc += ((unsigned int) (unsigned char) (*pin++) - 0x80) << 6;
			wc += ((unsigned int) (unsigned char) (*pin++) - 0x80);
			ibl -= 3;
		}
		else if (wc >= 0xE0)
		{
			wc = (wc - 0xE0) << 12;
			wc += ((unsigned int) (unsigned char) (*pin++) - 0x80) << 6;
			wc += ((unsigned int) (unsigned char) (*pin++) - 0x80);
			ibl -= 2;
		}
		else if (wc >= 0xC0)
		{
			wc = (wc - 0xC0) << 6;
			wc += ((unsigned int) (unsigned char) (*pin++) - 0x80);
			ibl -= 1;
		}

		if (wc <= 0xFFFF)
		{
			*pout++ = (char) (wc & 0xFF);
			*pout++ = (char) (wc >> 8);
			obl -= 2;
		}
		else
		{
			wc -= 0x10000;
			*pout++ = (char) ((wc >> 10) & 0xFF);
			*pout++ = (char) ((wc >> 18) + 0xD8);
			*pout++ = (char) (wc & 0xFF);
			*pout++ = (char) (((wc >> 8) & 0x03) + 0xDC);
			obl -= 4;
		}
	}
#endif

	if (ibl > 0)
	{
		printf("MultiByteToWideChar: string not fully converted - %d chars left\n", (int) ibl);
		return 0;
	}

	return (pout - pout0) / 2;
}

/*
 * Conversion *from* Unicode
 * WideCharToMultiByte: http://msdn.microsoft.com/en-us/library/windows/desktop/dd374130/
 */

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
		LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar)
{
	char* pout;
	char* conv_pout;
	size_t conv_in_len;
	size_t conv_out_len;
	unsigned char* conv_pin;

	/*
	 * if cbMultiByte is set to 0, the function returns the required buffer size
	 * for lpMultiByteStr and makes no use of the output parameter itself.
	 */

	if (cbMultiByte == 0)
		return lstrlenW(lpWideCharStr);

	/* If cchWideChar is set to 0, the function fails */

	if (cchWideChar == 0)
		return 0;

	/* cchWideChar is set to -1 if the string is null-terminated */

	if (cchWideChar == -1)
		cchWideChar = lstrlenW(lpWideCharStr);

	conv_pin = (unsigned char*) lpWideCharStr;
	conv_in_len = cchWideChar * 2;
	pout = lpMultiByteStr;
	conv_pout = pout;
	conv_out_len = cchWideChar * 2;

#ifdef HAVE_ICONV
	{
		iconv_t* in_iconv_h;

		in_iconv_h = iconv_open(DEFAULT_CODEPAGE, WINDOWS_CODEPAGE);

		if (errno == EINVAL)
		{
			printf("Error opening iconv converter to %s from %s\n", DEFAULT_CODEPAGE, WINDOWS_CODEPAGE);
			return 0;
		}

		if (iconv(in_iconv_h, (ICONV_CONST char **) &conv_pin, &conv_in_len, &conv_pout, &conv_out_len) == (size_t) - 1)
		{
			printf("WideCharToMultiByte: iconv failure\n");
			return 0;
		}

		iconv_close(in_iconv_h);
	}
#else
	while (conv_in_len >= 2)
	{
		unsigned int wc;

		wc = (unsigned int) (unsigned char) (*conv_pin++);
		wc += ((unsigned int) (unsigned char) (*conv_pin++)) << 8;
		conv_in_len -= 2;

		if (wc >= 0xD800 && wc <= 0xDFFF && conv_in_len >= 2)
		{
			/* Code points U+10000 to U+10FFFF using surrogate pair */
			wc = ((wc - 0xD800) << 10) + 0x10000;
			wc += (unsigned int) (unsigned char) (*conv_pin++);
			wc += ((unsigned int) (unsigned char) (*conv_pin++) - 0xDC) << 8;
			conv_in_len -= 2;
		}

		if (wc <= 0x7F)
		{
			*conv_pout++ = (char) wc;
			conv_out_len--;
		}
		else if (wc <= 0x07FF)
		{
			*conv_pout++ = (char) (0xC0 + (wc >> 6));
			*conv_pout++ = (char) (0x80 + (wc & 0x3F));
			conv_out_len -= 2;
		}
		else if (wc <= 0xFFFF)
		{
			*conv_pout++ = (char) (0xE0 + (wc >> 12));
			*conv_pout++ = (char) (0x80 + ((wc >> 6) & 0x3F));
			*conv_pout++ = (char) (0x80 + (wc & 0x3F));
			conv_out_len -= 3;
		}
		else
		{
			*conv_pout++ = (char) (0xF0 + (wc >> 18));
			*conv_pout++ = (char) (0x80 + ((wc >> 12) & 0x3F));
			*conv_pout++ = (char) (0x80 + ((wc >> 6) & 0x3F));
			*conv_pout++ = (char) (0x80 + (wc & 0x3F));
			conv_out_len -= 4;
		}
	}
#endif

	if (conv_in_len > 0)
	{
		printf("WideCharToMultiByte: conversion failure - %d chars left\n", (int) conv_in_len);
		return 0;
	}

	*conv_pout = 0;

	return conv_out_len;
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
