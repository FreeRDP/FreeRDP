/**
 * FreeRDP: A Remote Desktop Protocol Client
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
#include <freerdp/utils/memory.h>

#include <freerdp/utils/unicode.h>

#include <winpr/crt.h>

/* Convert pin/in_len from WINDOWS_CODEPAGE - return like xstrdup, 0-terminated */

char* freerdp_uniconv_in(UNICONV* uniconv, unsigned char* pin, size_t in_len)
{
	unsigned char *conv_pin = pin;
	size_t conv_in_len = in_len;
	char *pout = xmalloc(in_len * 2 + 1), *conv_pout = pout;
	size_t conv_out_len = in_len * 2;

#ifdef HAVE_ICONV
	if (iconv(uniconv->in_iconv_h, (ICONV_CONST char **) &conv_pin, &conv_in_len, &conv_pout, &conv_out_len) == (size_t) - 1)
	{
		/* TODO: xrealloc if conv_out_len == 0 - it shouldn't be needed, but would allow a smaller initial alloc ... */
		printf("freerdp_uniconv_in: iconv failure\n");
		return 0;
	}
#else
	while (conv_in_len >= 2)
	{
		unsigned int wc;

		wc = (unsigned int)(unsigned char)(*conv_pin++);
		wc += ((unsigned int)(unsigned char)(*conv_pin++)) << 8;
		conv_in_len -= 2;

		if (wc >= 0xD800 && wc <= 0xDFFF && conv_in_len >= 2)
		{
			/* Code points U+10000 to U+10FFFF using surrogate pair */
			wc = ((wc - 0xD800) << 10) + 0x10000;
			wc += (unsigned int)(unsigned char)(*conv_pin++);
			wc += ((unsigned int)(unsigned char)(*conv_pin++) - 0xDC) << 8;
			conv_in_len -= 2;
		}

		if (wc <= 0x7F)
		{
			*conv_pout++ = (char)wc;
			conv_out_len--;
		}
		else if (wc <= 0x07FF)
		{
			*conv_pout++ = (char)(0xC0 + (wc >> 6));
			*conv_pout++ = (char)(0x80 + (wc & 0x3F));
			conv_out_len -= 2;
		}
		else if (wc <= 0xFFFF)
		{
			*conv_pout++ = (char)(0xE0 + (wc >> 12));
			*conv_pout++ = (char)(0x80 + ((wc >> 6) & 0x3F));
			*conv_pout++ = (char)(0x80 + (wc & 0x3F));
			conv_out_len -= 3;
		}
		else
		{
			*conv_pout++ = (char)(0xF0 + (wc >> 18));
			*conv_pout++ = (char)(0x80 + ((wc >> 12) & 0x3F));
			*conv_pout++ = (char)(0x80 + ((wc >> 6) & 0x3F));
			*conv_pout++ = (char)(0x80 + (wc & 0x3F));
			conv_out_len -= 4;
		}
	}
#endif

	if (conv_in_len > 0)
	{
		printf("freerdp_uniconv_in: conversion failure - %d chars left\n", (int) conv_in_len);
	}

	*conv_pout = 0;
	return pout;
}

UNICONV* freerdp_uniconv_new()
{
	UNICONV *uniconv = xnew(UNICONV);

#ifdef HAVE_ICONV
	uniconv->iconv = 1;
	uniconv->in_iconv_h = iconv_open(DEFAULT_CODEPAGE, WINDOWS_CODEPAGE);
	if (errno == EINVAL)
	{
		printf("Error opening iconv converter to %s from %s\n", DEFAULT_CODEPAGE, WINDOWS_CODEPAGE);
	}
	uniconv->out_iconv_h = iconv_open(WINDOWS_CODEPAGE, DEFAULT_CODEPAGE);
	if (errno == EINVAL)
	{
		printf("Error opening iconv converter to %s from %s\n", WINDOWS_CODEPAGE, DEFAULT_CODEPAGE);
	}
#endif

	return uniconv;
}

void freerdp_uniconv_free(UNICONV *uniconv)
{
	if (uniconv != NULL)
	{
#ifdef HAVE_ICONV
		iconv_close(uniconv->in_iconv_h);
		iconv_close(uniconv->out_iconv_h);
#endif
		xfree(uniconv);
	}
}

char* freerdp_uniconv_out(UNICONV* uniconv, const char *str, size_t* pout_len)
{
	WCHAR* wstr;
	int length;

	wstr = freerdp_AsciiToUnicode(str, &length);
	*pout_len = (size_t) length;

	return (char*) wstr;
}

WCHAR* freerdp_AsciiToUnicode(const char* str, int* length)
{
	WCHAR* wstr;

	if (!str)
	{
		*length = 0;
		return NULL;
	}

	*length = MultiByteToWideChar(CP_UTF8, 0, str, strlen(str), NULL, 0);
	wstr = (WCHAR*) malloc((*length + 1) * sizeof(WCHAR));

	MultiByteToWideChar(CP_UTF8, 0, str, *length, (LPWSTR) wstr, *length * sizeof(WCHAR));
	wstr[*length] = 0;

	*length *= 2;

	return wstr;
}

CHAR* freerdp_UnicodeToAscii(const WCHAR* wstr, int* length)
{
	CHAR* str;

	*length = WideCharToMultiByte(CP_UTF8, 0, wstr, lstrlenW(wstr), NULL, 0, NULL, NULL);
	str = (char*) malloc(*length + 1);

	WideCharToMultiByte(CP_UTF8, 0, wstr, *length, str, *length, NULL, NULL);
	str[*length] = '\0';

	return str;
}
