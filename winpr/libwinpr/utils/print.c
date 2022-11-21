/**
 * WinPR: Windows Portable Runtime
 * Print Utils
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include "../log.h"

#ifndef MIN
#define MIN(a, b) (a) < (b) ? (a) : (b)
#endif

void winpr_HexDump(const char* tag, UINT32 level, const BYTE* data, size_t length)
{
	wLog* log = WLog_Get(tag);
	winpr_HexLogDump(log, level, data, length);
}

void winpr_HexLogDump(wLog* log, UINT32 lvl, const BYTE* data, size_t length)
{
	const BYTE* p = data;
	size_t i, line, offset = 0;
	const size_t maxlen = 20; /* 64bit SIZE_MAX as decimal */
	/* String line length:
	 * prefix          '[1234] '
	 * hexdump         '01 02 03 04'
	 * separator       '   '
	 * ASIC line       'ab..cd'
	 * zero terminator '\0'
	 */
	const size_t blen =
	    (maxlen + 3) + (WINPR_HEXDUMP_LINE_LENGTH * 3) + 3 + WINPR_HEXDUMP_LINE_LENGTH + 1;
	size_t pos = 0;

	char* buffer;

	if (!WLog_IsLevelActive(log, lvl))
		return;

	if (!log)
		return;

	buffer = malloc(blen);

	if (!buffer)
	{
		WLog_Print(log, WLOG_ERROR, "malloc(%" PRIuz ") failed with [%" PRIuz "] %s", blen, errno,
		           strerror(errno));
		return;
	}

	while (offset < length)
	{
		int rc = _snprintf(&buffer[pos], blen - pos, "%04" PRIuz " ", offset);

		if (rc < 0)
			goto fail;

		pos += (size_t)rc;
		line = length - offset;

		if (line > WINPR_HEXDUMP_LINE_LENGTH)
			line = WINPR_HEXDUMP_LINE_LENGTH;

		for (i = 0; i < line; i++)
		{
			rc = _snprintf(&buffer[pos], blen - pos, "%02" PRIx8 " ", p[i]);

			if (rc < 0)
				goto fail;

			pos += (size_t)rc;
		}

		for (; i < WINPR_HEXDUMP_LINE_LENGTH; i++)
		{
			rc = _snprintf(&buffer[pos], blen - pos, "   ");

			if (rc < 0)
				goto fail;

			pos += (size_t)rc;
		}

		for (i = 0; i < line; i++)
		{
			rc = _snprintf(&buffer[pos], blen - pos, "%c",
			               (p[i] >= 0x20 && p[i] < 0x7F) ? (char)p[i] : '.');

			if (rc < 0)
				goto fail;

			pos += (size_t)rc;
		}

		WLog_Print(log, lvl, "%s", buffer);
		offset += line;
		p += line;
		pos = 0;
	}

	WLog_Print(log, lvl, "[length=%" PRIuz "] ", length);
fail:
	free(buffer);
}

void winpr_CArrayDump(const char* tag, UINT32 level, const BYTE* data, size_t length, size_t width)
{
	const BYTE* p = data;
	size_t i, offset = 0;
	const size_t llen = ((length > width) ? width : length) * 4ull + 1ull;
	size_t pos;
	char* buffer = malloc(llen);

	if (!buffer)
	{
		WLog_ERR(tag, "malloc(%" PRIuz ") failed with [%d] %s", llen, errno, strerror(errno));
		return;
	}

	while (offset < length)
	{
		size_t line = length - offset;

		if (line > width)
			line = width;

		pos = 0;

		for (i = 0; i < line; i++)
		{
			const int rc = _snprintf(&buffer[pos], llen - pos, "\\x%02" PRIX8 "", p[i]);
			if (rc < 0)
				goto fail;
			pos += (size_t)rc;
		}

		WLog_LVL(tag, level, "%s", buffer);
		offset += line;
		p += line;
	}

fail:
	free(buffer);
}

static BYTE value(char c)
{
	if ((c >= '0') && (c <= '9'))
		return c - '0';
	if ((c >= 'A') && (c <= 'F'))
		return 10 + c - 'A';
	if ((c >= 'a') && (c <= 'f'))
		return 10 + c - 'a';
	return 0;
}

size_t winpr_HexStringToBinBuffer(const char* str, size_t strLength, BYTE* data, size_t dataLength)
{
	size_t x, y = 0;
	size_t maxStrLen;
	if (!str || !data || (strLength == 0) || (dataLength == 0))
		return 0;

	maxStrLen = strnlen(str, strLength);
	for (x = 0; x < maxStrLen;)
	{
		BYTE val = value(str[x++]);
		if (x < maxStrLen)
			val = (BYTE)(val << 4) | (value(str[x++]));
		if (x < maxStrLen)
		{
			if (str[x] == ' ')
				x++;
		}
		data[y++] = val;
		if (y >= dataLength)
			return y;
	}
	return y;
}

size_t winpr_BinToHexStringBuffer(const BYTE* data, size_t length, char* dstStr, size_t dstSize,
                                  BOOL space)
{
	const size_t n = space ? 3 : 2;
	const char bin2hex[] = "0123456789ABCDEF";
	const size_t maxLength = MIN(length, dstSize / n);
	size_t i;

	if (!data || !dstStr || (length == 0) || (dstSize == 0))
		return 0;

	for (i = 0; i < maxLength; i++)
	{
		const int ln = data[i] & 0xF;
		const int hn = (data[i] >> 4) & 0xF;
		char* dst = &dstStr[i * n];

		dst[0] = bin2hex[hn];
		dst[1] = bin2hex[ln];

		if (space)
			dst[2] = ' ';
	}

	if (space && (maxLength > 0))
	{
		dstStr[maxLength * n - 1] = '\0';
		return maxLength * n - 1;
	}
	dstStr[maxLength * n] = '\0';
	return maxLength * n;
}

char* winpr_BinToHexString(const BYTE* data, size_t length, BOOL space)
{
	size_t rc;
	const size_t n = space ? 3 : 2;
	const size_t size = (length + 1ULL) * n;
	char* p = (char*)malloc(size);

	if (!p)
		return NULL;

	rc = winpr_BinToHexStringBuffer(data, length, p, size, space);
	if (rc == 0)
	{
		free(p);
		return NULL;
	}

	return p;
}
