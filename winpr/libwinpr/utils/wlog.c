/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/print.h>

#include <winpr/wlog.h>

#define WLOG_BUFFER_SIZE	8192

void WLog_WriteA(int logLevel, LPCSTR loggerName, LPCSTR logMessage)
{
	switch (logLevel)
	{
		case WLOG_TRACE:
			break;

		case WLOG_DEBUG:
			break;

		case WLOG_INFO:
			break;

		case WLOG_WARN:
			break;

		case WLOG_ERROR:
			break;

		case WLOG_FATAL:
			break;
	}

	printf("[%s]: %s\n", loggerName, logMessage);
}

void WLog_LogVA(int logLevel, LPCSTR loggerName, LPCSTR logMessage, va_list args)
{
	if (!strchr(logMessage, '%'))
	{
		WLog_WriteA(logLevel, loggerName, logMessage);
	}
	else
	{
		char formattedLogMessage[8192];
		wvsnprintfx(formattedLogMessage, WLOG_BUFFER_SIZE - 1, logMessage, args);
		WLog_WriteA(logLevel, loggerName, formattedLogMessage);
	}
}

void WLog_LogA(int logLevel, LPCSTR loggerName, LPCSTR logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(logLevel, loggerName, logMessage, args);
	va_end(args);
}

void WLog_TraceA(LPCSTR loggerName, LPCSTR logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(WLOG_TRACE, loggerName, logMessage, args);
	va_end(args);
}

void WLog_DebugA(LPCSTR loggerName, LPCSTR logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(WLOG_DEBUG, loggerName, logMessage, args);
	va_end(args);
}

void WLog_InfoA(LPCSTR loggerName, LPCSTR logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(WLOG_INFO, loggerName, logMessage, args);
	va_end(args);
}

void WLog_WarnA(LPCSTR loggerName, LPCSTR logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(WLOG_WARN, loggerName, logMessage, args);
	va_end(args);
}

void WLog_ErrorA(LPCSTR loggerName, LPCSTR logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(WLOG_ERROR, loggerName, logMessage, args);
	va_end(args);
}

void WLog_FatalA(LPCSTR loggerName, LPCSTR logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(WLOG_FATAL, loggerName, logMessage, args);
	va_end(args);
}
