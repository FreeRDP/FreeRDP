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

#include <winpr/config.h>

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>
#include <winpr/environment.h>

#include "wlog.h"

#include "Layout.h"

#if defined __linux__ && !defined ANDROID
#include <unistd.h>
#include <sys/syscall.h>
#endif

struct format_option
{
	const char* fmt;
	size_t fmtlen;
	const char* replace;
	size_t replacelen;
	const char* (*fkt)(void*);
	void* arg;
};

/**
 * Log Layout
 */

static void WLog_PrintMessagePrefixVA(wLog* log, wLogMessage* message, const char* format,
                                      va_list args)
{
	WINPR_ASSERT(message);
	vsnprintf(message->PrefixString, WLOG_MAX_PREFIX_SIZE - 1, format, args);
}

static void WLog_PrintMessagePrefix(wLog* log, wLogMessage* message, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	WLog_PrintMessagePrefixVA(log, message, format, args);
	va_end(args);
}

static const char* get_tid(void* arg)
{
	char* str = arg;
	size_t tid;
#if defined __linux__ && !defined ANDROID
	/* On Linux we prefer to see the LWP id */
	tid = (size_t)syscall(SYS_gettid);
#else
	tid = (size_t)GetCurrentThreadId();
#endif
	sprintf(str, "%08" PRIxz, tid);
	return str;
}

static BOOL log_invalid_fmt(const char* what)
{
	fprintf(stderr, "Invalid format string '%s'\n", what);
	return FALSE;
}

static BOOL check_and_log_format_size(char* format, size_t size, size_t index, size_t add)
{
	/* format string must be '\0' terminated, so abort at size - 1 */
	if (index + add + 1 >= size)
	{
		fprintf(stderr,
		        "Format string too long ['%s', max %" PRIuz ", used %" PRIuz ", adding %" PRIuz
		        "]\n",
		        format, size, index, add);
		return FALSE;
	}
	return TRUE;
}

static BOOL check_and_log_format_args(size_t size, size_t index, size_t add)
{
	if (index + add > size)
	{
		fprintf(stderr,
		        "Too many format string arguments [max %" PRIuz ", used %" PRIuz ", adding %" PRIuz
		        "]\n",
		        size, index, add);
		return FALSE;
	}
	return TRUE;
}

static int opt_compare_fn(const void* a, const void* b)
{
	const char* what = a;
	const struct format_option* opt = b;
	if (!opt)
		return -1;
	return strncmp(what, opt->fmt, opt->fmtlen);
}

BOOL WLog_Layout_GetMessagePrefix(wLog* log, wLogLayout* layout, wLogMessage* message)
{
	size_t index = 0;
	unsigned argc = 0;
	const void* args[32] = { 0 };
	char format[WLOG_MAX_PREFIX_SIZE] = { 0 };
	char tid[32] = { 0 };
	SYSTEMTIME localTime = { 0 };

	WINPR_ASSERT(layout);
	WINPR_ASSERT(message);

	GetLocalTime(&localTime);

#define ENTRY(x) x, sizeof(x) - 1
	const struct format_option options[] = {
		{ ENTRY("%ctx"), ENTRY("%s"), log->custom, log->context },                /* log context */
		{ ENTRY("%dw"), ENTRY("%u"), NULL, (void*)(size_t)localTime.wDayOfWeek }, /* day of week */
		{ ENTRY("%dy"), ENTRY("%u"), NULL, (void*)(size_t)localTime.wDay },       /* day of year */
		{ ENTRY("%fl"), ENTRY("%s"), NULL, (void*)message->FileName },            /* file */
		{ ENTRY("%fn"), ENTRY("%s"), NULL, (void*)message->FunctionName },        /* function */
		{ ENTRY("%hr"), ENTRY("%02u"), NULL, (void*)(size_t)localTime.wHour },    /* hours */
		{ ENTRY("%ln"), ENTRY("%s"), NULL, (void*)(size_t)message->LineNumber },  /* line number */
		{ ENTRY("%lv"), ENTRY("%s"), NULL, (void*)WLOG_LEVELS[message->Level] },  /* log level */
		{ ENTRY("%mi"), ENTRY("%02u"), NULL, (void*)(size_t)localTime.wMinute },  /* minutes */
		{ ENTRY("%ml"), ENTRY("%02u"), NULL,
		  (void*)(size_t)localTime.wMilliseconds },                           /* milliseconds */
		{ ENTRY("%mn"), ENTRY("%s"), NULL, log->Name },                       /* module name */
		{ ENTRY("%mo"), ENTRY("%u"), NULL, (void*)(size_t)localTime.wMonth }, /* month */
		{ ENTRY("%pid"), ENTRY("%u"), NULL, (void*)(size_t)GetCurrentProcessId() }, /* process id */
		{ ENTRY("%se"), ENTRY("%02u"), NULL, (void*)(size_t)localTime.wSecond },    /* seconds */
		{ ENTRY("%tid"), ENTRY("%s"), get_tid, tid },                               /* thread id */
		{ ENTRY("%yr"), ENTRY("%u"), NULL, (void*)(size_t)localTime.wYear },        /* year */
	};

	const char* p = layout->FormatString;

	while (*p)
	{
		const struct format_option* opt =
		    bsearch(p, options, ARRAYSIZE(options), sizeof(struct format_option), opt_compare_fn);
		if (opt)
		{
			if (!check_and_log_format_size(format, ARRAYSIZE(format), index, opt->replacelen))
				return FALSE;

			strncpy(&format[index], opt->replace, opt->replacelen);
			index += opt->replacelen;

			if (!check_and_log_format_args(ARRAYSIZE(args), argc, 1))
				return FALSE;
			if (opt->fkt)
				args[argc++] = opt->fkt(opt->arg);
			else
				args[argc++] = opt->arg;

			p += opt->fmtlen;
		}
		else
		{
			/* Unknown format string */
			if (*p == '%')
				return log_invalid_fmt(p);

			if (!check_and_log_format_size(format, ARRAYSIZE(format), index, 1))
				return FALSE;
			format[index++] = *p++;
		}
	}

	if (!check_and_log_format_size(format, ARRAYSIZE(format), index, 0))
		return FALSE;

	switch (argc)
	{
		case 0:
			WLog_PrintMessagePrefix(log, message, format);
			break;

		case 1:
			WLog_PrintMessagePrefix(log, message, format, args[0]);
			break;

		case 2:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1]);
			break;

		case 3:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2]);
			break;

		case 4:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3]);
			break;

		case 5:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4]);
			break;

		case 6:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5]);
			break;

		case 7:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6]);
			break;

		case 8:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7]);
			break;

		case 9:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7], args[8]);
			break;

		case 10:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7], args[8], args[9]);
			break;

		case 11:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7], args[8], args[9], args[10]);
			break;

		case 12:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7], args[8], args[9], args[10],
			                        args[11]);
			break;

		case 13:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7], args[8], args[9], args[10],
			                        args[11], args[12]);
			break;

		case 14:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7], args[8], args[9], args[10],
			                        args[11], args[12], args[13]);
			break;

		case 15:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7], args[8], args[9], args[10],
			                        args[11], args[12], args[13], args[14]);
			break;

		case 16:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
			                        args[4], args[5], args[6], args[7], args[8], args[9], args[10],
			                        args[11], args[12], args[13], args[14], args[15]);
			break;
	}

	return TRUE;
}

wLogLayout* WLog_GetLogLayout(wLog* log)
{
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);
	return appender->Layout;
}

BOOL WLog_Layout_SetPrefixFormat(wLog* log, wLogLayout* layout, const char* format)
{
	free(layout->FormatString);
	layout->FormatString = NULL;

	if (format)
	{
		layout->FormatString = _strdup(format);

		if (!layout->FormatString)
			return FALSE;
	}

	return TRUE;
}

wLogLayout* WLog_Layout_New(wLog* log)
{
	LPCSTR prefix = "WLOG_PREFIX";
	DWORD nSize;
	char* env = NULL;
	wLogLayout* layout;
	layout = (wLogLayout*)calloc(1, sizeof(wLogLayout));

	if (!layout)
		return NULL;

	nSize = GetEnvironmentVariableA(prefix, NULL, 0);

	if (nSize)
	{
		env = (LPSTR)malloc(nSize);

		if (!env)
		{
			free(layout);
			return NULL;
		}

		if (GetEnvironmentVariableA(prefix, env, nSize) != nSize - 1)
		{
			free(env);
			free(layout);
			return NULL;
		}
	}

	if (env)
		layout->FormatString = env;
	else
	{
#ifdef ANDROID
		layout->FormatString = _strdup("[pid=%pid:tid=%tid] - [%fn][%ctx]: ");
#else
		layout->FormatString = _strdup("[%hr:%mi:%se:%ml] [%pid:%tid] [%lv][%mn] - [%fn][%ctx]: ");
#endif

		if (!layout->FormatString)
		{
			free(layout);
			return NULL;
		}
	}

	return layout;
}

void WLog_Layout_Free(wLog* log, wLogLayout* layout)
{
	if (layout)
	{
		if (layout->FormatString)
		{
			free(layout->FormatString);
			layout->FormatString = NULL;
		}

		free(layout);
	}
}
