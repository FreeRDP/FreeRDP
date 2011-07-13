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

#include <errno.h>
#include <freerdp/utils/memory.h>

#include <freerdp/utils/unicode.h>

/* Convert pin/in_len from WINDOWS_CODEPAGE - return like xstrdup, 0-terminated */

char* freerdp_uniconv_in(UNICONV *uniconv, unsigned char* pin, size_t in_len)
{
	unsigned char *conv_pin = pin;
	size_t conv_in_len = in_len;
	char *pout = xmalloc(in_len + 1), *conv_pout = pout;
	size_t conv_out_len = in_len;

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
		if ((signed char)(*conv_pin) < 0)
		{
			printf("freerdp_uniconv_in: wrong input conversion of char %d\n", *conv_pin);
		}
		*conv_pout++ = *conv_pin++;
		if ((*conv_pin) != 0)
		{
			printf("freerdp_uniconv_in: wrong input conversion skipping non-zero char %d\n", *conv_pin);
		}
		conv_pin++;
		conv_in_len -= 2;
		conv_out_len--;
	}
#endif

	if (conv_in_len > 0)
	{
		printf("freerdp_uniconv_in: conversion failure - %d chars left\n", (int) conv_in_len);
	}

	*conv_pout = 0;
	return pout;
}

/* Convert str from DEFAULT_CODEPAGE to WINDOWS_CODEPAGE and return buffer like xstrdup.
 * Buffer is 0-terminated but that is not included in the returned length. */

char* freerdp_uniconv_out(UNICONV *uniconv, char *str, size_t *pout_len)
{
	size_t ibl;
	size_t obl;
	char* pin;
	char* pout;
	char* pout0;

	if (str == NULL)
	{
		*pout_len = 0;
		return NULL;
	}

	ibl = strlen(str);
	obl = 2 * ibl;
	pin = str;
	pout0 = xmalloc(obl + 2);
	pout = pout0;

#ifdef HAVE_ICONV
	if (iconv(uniconv->out_iconv_h, (ICONV_CONST char **) &pin, &ibl, &pout, &obl) == (size_t) - 1)
	{
		printf("freerdp_uniconv_out: iconv() error\n");
		return NULL;
	}
#else
	while ((ibl > 0) && (obl > 0))
	{
		if ((signed char)(*pin) < 0)
		{
			return NULL;
		}
		*pout++ = *pin++;
		*pout++ = 0;
		ibl--;
		obl -= 2;
	}
#endif

	if (ibl > 0)
	{
		printf("freerdp_uniconv_out: string not fully converted - %d chars left\n", (int) ibl);
	}

	*pout_len = pout - pout0;
	*pout++ = 0; /* Add extra double zero termination */
	*pout = 0;

	return pout0;
}

/* Uppercase a unicode string */

void freerdp_uniconv_uppercase(UNICONV *uniconv, char *wstr, int length)
{
	int i;
	char* p;

	p = wstr;
	for (i = 0; i < length; i++)
	{
		if (p[i * 2] >= 'a' && p[i * 2] <= 'z')
			p[i * 2] = p[i * 2] - 32;
	}
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
