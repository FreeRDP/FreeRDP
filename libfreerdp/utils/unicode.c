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
#include <freerdp/types.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/memory.h>

#include <freerdp/utils/unicode.h>

#include <winpr/crt.h>

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

	length = freerdp_AsciiToUnicodeAlloc(str, &wstr, 0);
	*pout_len = (size_t) length;

	return (char*) wstr;
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

	length = MultiByteToWideChar(CP_UTF8, 0, str, length, NULL, 0);
	*wstr = (WCHAR*) malloc((length + 1) * sizeof(WCHAR));

	MultiByteToWideChar(CP_UTF8, 0, str, length, (LPWSTR) (*wstr), length * sizeof(WCHAR));
	(*wstr)[length] = 0;

	length *= 2;

	return length;
}

char* freerdp_uniconv_in(UNICONV* uniconv, unsigned char* pin, size_t in_len)
{
	CHAR* str;
	int length;

	length = freerdp_UnicodeToAsciiAlloc((WCHAR*) pin, &str, (int) (in_len / 2));

	return (char*) str;
}

int freerdp_UnicodeToAsciiAlloc(const WCHAR* wstr, CHAR** str, int length)
{
	*str = malloc((length * 2) + 1);

	WideCharToMultiByte(CP_UTF8, 0, wstr, length, *str, length, NULL, NULL);

	return length;
}
