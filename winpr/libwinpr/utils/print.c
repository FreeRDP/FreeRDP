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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include "trio.h"

#include "../log.h"

void winpr_HexDump(const char* tag, UINT32 level, const BYTE* data, int length)
{
	const BYTE* p = data;
	int i, line, offset = 0;
	size_t blen = 7 + WINPR_HEXDUMP_LINE_LENGTH * 5;
	size_t pos = 0;
	char* buffer = malloc(blen);

	if (!buffer)
	{
		WLog_ERR(tag, "malloc(%zd) failed with [%d] %s", blen, errno, strerror(errno));
		return;
	}

	while (offset < length)
	{
		pos += trio_snprintf(&buffer[pos], blen - pos, "%04x ", offset);
		line = length - offset;

		if (line > WINPR_HEXDUMP_LINE_LENGTH)
			line = WINPR_HEXDUMP_LINE_LENGTH;

		for (i = 0; i < line; i++)
			pos += trio_snprintf(&buffer[pos], blen - pos, "%02x ", p[i]);

		for (; i < WINPR_HEXDUMP_LINE_LENGTH; i++)
			pos += trio_snprintf(&buffer[pos], blen - pos, "   ");

		for (i = 0; i < line; i++)
			pos += trio_snprintf(&buffer[pos], blen - pos, "%c",
							(p[i] >= 0x20 && p[i] < 0x7F) ? p[i] : '.');

		WLog_LVL(tag, level, "%s", buffer);
		offset += line;
		p += line;
		pos = 0;
	}

	free(buffer);
}

void winpr_CArrayDump(const char* tag, UINT32 level, const BYTE* data, int length, int width)
{
	const BYTE* p = data;
	int i, line, offset = 0;
	const size_t llen = ((length > width) ? width : length) * 4 + 1;
	size_t pos;
	char* buffer = malloc(llen);

	if (!buffer)
	{
		WLog_ERR(tag, "malloc(%zd) failed with [%d] %s", llen, errno, strerror(errno));
		return;
	}

	while (offset < length)
	{
		line = length - offset;

		if (line > width)
			line = width;

		pos = 0;

		for (i = 0; i < line; i++)
			pos += trio_snprintf(&buffer[pos], llen - pos, "\\x%02X", p[i]);

		WLog_LVL(tag, level, "%s", buffer);
		offset += line;
		p += line;
	}

	free(buffer);
}

char* winpr_BinToHexString(const BYTE* data, int length, BOOL space)
{
	int i;
	int n;
	char* p;
	int ln, hn;
	char bin2hex[] = "0123456789ABCDEF";
	n = space ? 3 : 2;
	p = (char*) malloc((length + 1) * n);
	if (!p)
		return NULL;

	for (i = 0; i < length; i++)
	{
		ln = data[i] & 0xF;
		hn = (data[i] >> 4) & 0xF;
		p[i * n] = bin2hex[hn];
		p[(i * n) + 1] = bin2hex[ln];

		if (space)
			p[(i * n) + 2] = ' ';
	}

	p[length * n] = '\0';
	return p;
}

int wvprintfx(const char* fmt, va_list args)
{
	return trio_vprintf(fmt, args);
}

int wprintfx(const char* fmt, ...)
{
	va_list args;
	int status;
	va_start(args, fmt);
	status = trio_vprintf(fmt, args);
	va_end(args);
	return status;
}

int wvsnprintfx(char* buffer, size_t bufferSize, const char* fmt, va_list args)
{
	return trio_vsnprintf(buffer, bufferSize, fmt, args);
}
