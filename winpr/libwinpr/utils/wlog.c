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

#include <winpr/crt.h>
#include <winpr/print.h>

#include <winpr/wlog.h>

#define WLOG_BUFFER_SIZE	8192

const char* WLOG_LEVELS[7] =
{
	"Trace",
	"Debug",
	"Info",
	"Warn",
	"Error",
	"Fatal",
	"Off"
};

void WLog_WriteA(wLog* log, DWORD logLevel, LPCSTR logMessage)
{
	char* logLevelStr;

	if (logLevel > log->Level)
		return;

	logLevelStr = (char*) WLOG_LEVELS[logLevel];

	printf("[%s] [%s]: %s\n", logLevelStr, log->Name, logMessage);
}

void WLog_LogVA(wLog* log, DWORD logLevel, LPCSTR logMessage, va_list args)
{
	if (!strchr(logMessage, '%'))
	{
		WLog_WriteA(log, logLevel, logMessage);
	}
	else
	{
		char formattedLogMessage[8192];
		wvsnprintfx(formattedLogMessage, WLOG_BUFFER_SIZE - 1, logMessage, args);
		WLog_WriteA(log, logLevel, formattedLogMessage);
	}
}

void WLog_Print(wLog* log, DWORD logLevel, LPCSTR logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(log, logLevel, logMessage, args);
	va_end(args);
}

DWORD WLog_GetLogLevel(wLog* log)
{
	return log->Level;
}

void WLog_SetLogLevel(wLog* log, DWORD logLevel)
{
	if (logLevel > WLOG_OFF)
		logLevel = WLOG_OFF;

	log->Level = logLevel;
}

wLog* WLog_New(LPCSTR name)
{
	wLog* log;

	log = (wLog*) malloc(sizeof(wLog));

	if (log)
	{
		ZeroMemory(log, sizeof(wLog));

		log->Name = _strdup(name);
		log->Level = WLOG_TRACE;
	}

	return log;
}

void WLog_Free(wLog* log)
{
	if (log)
	{
		free(log->Name);
		free(log);
	}
}
