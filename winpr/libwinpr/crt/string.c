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
#include <winpr/endian.h>

#if defined(WITH_URIPARSER)
#include <uriparser/Uri.h>
#endif

/* String Manipulation (CRT): http://msdn.microsoft.com/en-us/library/f0151s4x.aspx */

#include "../log.h"
#define TAG WINPR_TAG("crt")

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#if defined(WITH_URIPARSER)
char* winpr_str_url_decode(const char* str, size_t len)
{
	char* dst = strndup(str, len);
	if (!dst)
		return NULL;

	if (!uriUnescapeInPlaceExA(dst, URI_FALSE, URI_FALSE))
	{
		free(dst);
		return NULL;
	}

	return dst;
}

char* winpr_str_url_encode(const char* str, size_t len)
{
	char* dst = calloc(len + 1, sizeof(char) * 3);
	if (!dst)
		return NULL;

	if (!uriEscapeA(str, dst, URI_FALSE, URI_FALSE))
	{
		free(dst);
		return NULL;
	}
	return dst;
}

#else
static const char rfc3986[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x2e, 0x00,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x5f,
	0x00, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x00, 0x00, 0x00, 0x7e, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static char hex2bin(char what)
{
	if (what >= 'a')
		what -= 'a' - 'A';
	if (what >= 'A')
		what -= ('A' - 10);
	else
		what -= '0';
	return what;
}

static char unescape(const char* what, size_t* px)
{
	if ((*what == '%') && (isxdigit(what[1]) && isxdigit(what[2])))
	{
		*px += 2;
		return 16 * hex2bin(what[1]) + hex2bin(what[2]);
	}

	return *what;
}

char* winpr_str_url_decode(const char* str, size_t len)
{
	char* dst = calloc(len + 1, sizeof(char));
	if (!dst)
		return NULL;

	size_t pos = 0;
	for (size_t x = 0; x < strnlen(str, len); x++)
	{
		const char* cur = &str[x];
		dst[pos++] = unescape(cur, &x);
	}
	return dst;
}

static char* escape(char* dst, char what)
{
	if (rfc3986[what & 0xff])
	{
		*dst = what;
		return dst + 1;
	}

	sprintf(dst, "%%%02" PRIX8, (BYTE)(what & 0xff));
	return dst + 3;
}

char* winpr_str_url_encode(const char* str, size_t len)
{
	char* dst = calloc(len + 1, sizeof(char) * 3);
	if (!dst)
		return NULL;

	char* ptr = dst;
	for (size_t x = 0; x < strnlen(str, len); x++)
	{
		const char cur = str[x];
		ptr = escape(ptr, cur);
	}
	return dst;
}
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

WINPR_ATTR_FORMAT_ARG(3, 4)
int winpr_asprintf(char** s, size_t* slen, WINPR_FORMAT_ARG const char* templ, ...)
{
	va_list ap = { 0 };

	va_start(ap, templ);
	int rc = winpr_vasprintf(s, slen, templ, ap);
	va_end(ap);
	return rc;
}

WINPR_ATTR_FORMAT_ARG(3, 0)
int winpr_vasprintf(char** s, size_t* slen, WINPR_FORMAT_ARG const char* templ, va_list oap)
{
	va_list ap = { 0 };

	*s = NULL;
	*slen = 0;

	va_copy(ap, oap);
	const int length = vsnprintf(NULL, 0, templ, ap);
	va_end(ap);
	if (length < 0)
		return length;

	char* str = calloc((size_t)length + 1ul, sizeof(char));
	if (!str)
		return -1;

	va_copy(ap, oap);
	const int plen = vsprintf(str, templ, ap);
	va_end(ap);

	if (length != plen)
	{
		free(str);
		return -1;
	}
	*s = str;
	*slen = (size_t)length;
	return length;
}

#ifndef _WIN32

char* _strdup(const char* strSource)
{
	if (strSource == NULL)
		return NULL;

	char* strDestination = strdup(strSource);

	if (strDestination == NULL)
		WLog_ERR(TAG, "strdup");

	return strDestination;
}

WCHAR* _wcsdup(const WCHAR* strSource)
{
	if (!strSource)
		return NULL;

	size_t len = _wcslen(strSource);
	WCHAR* strDestination = calloc(len + 1, sizeof(WCHAR));

	if (strDestination != NULL)
		memcpy(strDestination, strSource, len * sizeof(WCHAR));

	if (strDestination == NULL)
		WLog_ERR(TAG, "wcsdup");

	return strDestination;
}

WCHAR* _wcsncat(WCHAR* dst, const WCHAR* src, size_t sz)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src || (sz == 0));

	const size_t dlen = _wcslen(dst);
	const size_t slen = _wcsnlen(src, sz);
	for (size_t x = 0; x < slen; x++)
		dst[dlen + x] = src[x];
	dst[dlen + slen] = '\0';
	return dst;
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
	const WCHAR* p = str;

	WINPR_ASSERT(p);

	while (*p)
		p++;

	return (size_t)(p - str);
}

/* _wcsnlen -> wcsnlen */

size_t _wcsnlen(const WCHAR* str, size_t max)
{
	WINPR_ASSERT(str);

	size_t x = 0;
	for (; x < max; x++)
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
		return WINPR_CAST_CONST_PTR_AWAY(str, WCHAR*);

	const size_t searchLen = _wcslen(strSearch);
	while (*str)
	{
		if (_wcsncmp(str, strSearch, searchLen) == 0)
			return WINPR_CAST_CONST_PTR_AWAY(str, WCHAR*);
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
	const WCHAR* p = str;

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
	WCHAR* nextToken = NULL;
	WCHAR value = 0;

	if (!strToken)
		strToken = *context;

	value = *strToken;

	while (*strToken && _wcschr(strDelimit, value))
	{
		strToken++;
		value = *strToken;
	}

	if (!*strToken)
		return NULL;

	nextToken = strToken++;
	value = *strToken;

	while (*strToken && !(_wcschr(strDelimit, value)))
	{
		strToken++;
		value = *strToken;
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

#include "casing.h"

LPSTR CharUpperA(LPSTR lpsz)
{
	size_t length = 0;

	if (!lpsz)
		return NULL;

	length = strlen(lpsz);

	if (length < 1)
		return (LPSTR)NULL;

	if (length == 1)
	{
		char c = *lpsz;

		if ((c >= 'a') && (c <= 'z'))
			c = (char)(c - 'a' + 'A');

		*lpsz = c;
		return lpsz;
	}

	for (size_t i = 0; i < length; i++)
	{
		if ((lpsz[i] >= 'a') && (lpsz[i] <= 'z'))
			lpsz[i] = (char)(lpsz[i] - 'a' + 'A');
	}

	return lpsz;
}

LPWSTR CharUpperW(LPWSTR lpsz)
{
	size_t length = 0;

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

	for (size_t i = 0; i < length; i++)
	{
		if ((lpsz[i] >= L'a') && (lpsz[i] <= L'z'))
			lpsz[i] = lpsz[i] - L'a' + L'A';
	}

	return lpsz;
}

DWORD CharUpperBuffA(LPSTR lpsz, DWORD cchLength)
{
	if (cchLength < 1)
		return 0;

	for (DWORD i = 0; i < cchLength; i++)
	{
		if ((lpsz[i] >= 'a') && (lpsz[i] <= 'z'))
			lpsz[i] = (char)(lpsz[i] - 'a' + 'A');
	}

	return cchLength;
}

DWORD CharUpperBuffW(LPWSTR lpsz, DWORD cchLength)
{
	WCHAR value = 0;

	for (DWORD i = 0; i < cchLength; i++)
	{
		Data_Read_UINT16(&lpsz[i], value);
		value = WINPR_TOUPPERW(value);
		Data_Write_UINT16(&lpsz[i], value);
	}

	return cchLength;
}

LPSTR CharLowerA(LPSTR lpsz)
{
	size_t length = 0;

	if (!lpsz)
		return (LPSTR)NULL;

	length = strlen(lpsz);

	if (length < 1)
		return (LPSTR)NULL;

	if (length == 1)
	{
		char c = *lpsz;

		if ((c >= 'A') && (c <= 'Z'))
			c = (char)(c - 'A' + 'a');

		*lpsz = c;
		return lpsz;
	}

	for (size_t i = 0; i < length; i++)
	{
		if ((lpsz[i] >= 'A') && (lpsz[i] <= 'Z'))
			lpsz[i] = (char)(lpsz[i] - 'A' + 'a');
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
	if (cchLength < 1)
		return 0;

	for (DWORD i = 0; i < cchLength; i++)
	{
		if ((lpsz[i] >= 'A') && (lpsz[i] <= 'Z'))
			lpsz[i] = (char)(lpsz[i] - 'A' + 'a');
	}

	return cchLength;
}

DWORD CharLowerBuffW(LPWSTR lpsz, DWORD cchLength)
{
	WCHAR value = 0;

	for (DWORD i = 0; i < cchLength; i++)
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
	char* p = NULL;
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

#if !defined(WINPR_HAVE_STRNDUP)
char* strndup(const char* src, size_t n)
{
	char* dst = calloc(n + 1, sizeof(char));
	if (dst)
		strncpy(dst, src, n);
	return dst;
}
#endif

const WCHAR* InitializeConstWCharFromUtf8(const char* str, WCHAR* buffer, size_t len)
{
	WINPR_ASSERT(str);
	WINPR_ASSERT(buffer || (len == 0));
	(void)ConvertUtf8ToWChar(str, buffer, len);
	return buffer;
}
