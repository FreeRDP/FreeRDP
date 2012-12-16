/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Unicode Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <freerdp/types.h>
#include <winpr/print.h>

#include <freerdp/utils/unicode.h>

#include <winpr/crt.h>

/**
 * This is a temporary copy of the old buggy implementations of
 * MultiByteToWideChar and WideCharToMultiByte
 */

#if 1
#define _MultiByteToWideChar	old_MultiByteToWideChar
#define _WideCharToMultiByte	old_WideCharToMultiByte
#else
#define _MultiByteToWideChar	MultiByteToWideChar
#define _WideCharToMultiByte	WideCharToMultiByte
#endif

int old_MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr,
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

int old_WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
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

int freerdp_AsciiToUnicodeAlloc(const CHAR* str, WCHAR** wstr, int length)
{
	if (!str)
	{
		*wstr = NULL;
		return 0;
	}

	if (length < 1)
		length = strlen(str);

	length = _MultiByteToWideChar(CP_UTF8, 0, str, length, NULL, 0);
	*wstr = (WCHAR*) malloc((length + 1) * sizeof(WCHAR));

	_MultiByteToWideChar(CP_UTF8, 0, str, length, (LPWSTR) (*wstr), length * sizeof(WCHAR));
	(*wstr)[length] = 0;

	return length;
}

int freerdp_UnicodeToAsciiAlloc(const WCHAR* wstr, CHAR** str, int length)
{
	*str = malloc((length * 2) + 1);
	memset(*str, 0, (length * 2) + 1);

	_WideCharToMultiByte(CP_UTF8, 0, wstr, length, *str, length, NULL, NULL);
	(*str)[length] = 0;

	return length;
}
