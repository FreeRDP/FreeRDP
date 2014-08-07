/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Debug Utils
 *
 * Copyright 2014 Armin Novak <armin.novak@thincast.com>
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

#include <stdio.h>
#include <stdarg.h>
#include <winpr/wlog.h>

#define FMT_MAX 1024

static wLog *sLog = NULL;

static void debug_init(void)
{
	WLog_Init();
	sLog = WLog_Get("com.freerdp.common");
}

void debug_print(int level, const char *file, const char *fkt, int line,
	const char *dbg_str, const char *fmt, ...)
{
	va_list ap;
	size_t len = strnlen(dbg_str, FMT_MAX) + strnlen(fmt, FMT_MAX) + 10;
	char *cfmt = alloca(len);
	wLogMessage msg;

	if (!sLog)
		debug_init();

	if (cfmt)
	{
		snprintf(cfmt, len, "[%s]: %s", dbg_str, fmt);
			
		va_start(ap, fmt);

		msg.Type = WLOG_MESSAGE_TEXT;
		msg.Level = level;
		msg.FormatString = cfmt;
		msg.LineNumber = line;
		msg.FileName = file;
		msg.FunctionName = fkt;
		WLog_PrintMessageVA(sLog, &msg, ap);
		va_end(ap);
	}
	else
		WLog_Print(sLog, WLOG_FATAL, "failed to allocate %zd bytes on stack!", len);
}

