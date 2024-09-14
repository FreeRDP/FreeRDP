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

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

struct format_option_recurse;

struct format_option
{
	const char* fmt;
	size_t fmtlen;
	const char* replace;
	size_t replacelen;
	const char* (*fkt)(void*);
	void* arg;
	const char* (*ext)(const struct format_option* opt, const char* str, size_t* preplacelen,
	                   size_t* pskiplen);
	struct format_option_recurse* recurse;
};

struct format_option_recurse
{
	struct format_option* options;
	size_t nroptions;
	wLog* log;
	wLogLayout* layout;
	wLogMessage* message;
	char buffer[WLOG_MAX_PREFIX_SIZE];
};

/**
 * Log Layout
 */
WINPR_ATTR_FORMAT_ARG(3, 0)
static void WLog_PrintMessagePrefixVA(wLog* log, wLogMessage* message,
                                      WINPR_FORMAT_ARG const char* format, va_list args)
{
	WINPR_ASSERT(message);
	(void)vsnprintf(message->PrefixString, WLOG_MAX_PREFIX_SIZE - 1, format, args);
}

WINPR_ATTR_FORMAT_ARG(3, 4)
static void WLog_PrintMessagePrefix(wLog* log, wLogMessage* message,
                                    WINPR_FORMAT_ARG const char* format, ...)
{
	va_list args;
	va_start(args, format);
	WLog_PrintMessagePrefixVA(log, message, format, args);
	va_end(args);
}

static const char* get_tid(void* arg)
{
	char* str = arg;
	size_t tid = 0;
#if defined __linux__ && !defined ANDROID
	/* On Linux we prefer to see the LWP id */
	tid = (size_t)syscall(SYS_gettid);
#else
	tid = (size_t)GetCurrentThreadId();
#endif
	(void)sprintf(str, "%08" PRIxz, tid);
	return str;
}

static BOOL log_invalid_fmt(const char* what)
{
	(void)fprintf(stderr, "Invalid format string '%s'\n", what);
	return FALSE;
}

static BOOL check_and_log_format_size(char* format, size_t size, size_t index, size_t add)
{
	/* format string must be '\0' terminated, so abort at size - 1 */
	if (index + add + 1 >= size)
	{
		(void)fprintf(stderr,
		              "Format string too long ['%s', max %" PRIuz ", used %" PRIuz
		              ", adding %" PRIuz "]\n",
		              format, size, index, add);
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

static BOOL replace_format_string(const char* FormatString, struct format_option_recurse* recurse,
                                  char* format, size_t formatlen);

static const char* skip_if_null(const struct format_option* opt, const char* fmt,
                                size_t* preplacelen, size_t* pskiplen)
{
	WINPR_ASSERT(opt);
	WINPR_ASSERT(fmt);
	WINPR_ASSERT(preplacelen);
	WINPR_ASSERT(pskiplen);

	*preplacelen = 0;
	*pskiplen = 0;

	const char* str = &fmt[opt->fmtlen]; /* Skip first %{ from string */
	const char* end = strstr(str, opt->replace);
	if (!end)
		return NULL;
	*pskiplen = end - fmt + opt->replacelen;

	if (!opt->arg)
		return NULL;

	const size_t replacelen = end - str;

	char buffer[WLOG_MAX_PREFIX_SIZE] = { 0 };
	memcpy(buffer, str, MIN(replacelen, ARRAYSIZE(buffer) - 1));

	if (!replace_format_string(buffer, opt->recurse, opt->recurse->buffer,
	                           ARRAYSIZE(opt->recurse->buffer)))
		return NULL;

	*preplacelen = strnlen(opt->recurse->buffer, ARRAYSIZE(opt->recurse->buffer));
	return opt->recurse->buffer;
}

static BOOL replace_format_string(const char* FormatString, struct format_option_recurse* recurse,
                                  char* format, size_t formatlen)
{
	WINPR_ASSERT(FormatString);
	WINPR_ASSERT(recurse);

	size_t index = 0;

	while (*FormatString)
	{
		const struct format_option* opt =
		    bsearch(FormatString, recurse->options, recurse->nroptions,
		            sizeof(struct format_option), opt_compare_fn);
		if (opt)
		{
			size_t replacelen = opt->replacelen;
			size_t fmtlen = opt->fmtlen;
			const char* replace = opt->replace;
			const void* arg = opt->arg;

			if (opt->ext)
				replace = opt->ext(opt, FormatString, &replacelen, &fmtlen);
			if (opt->fkt)
				arg = opt->fkt(opt->arg);

			if (replace && (replacelen > 0))
			{
				WINPR_PRAGMA_DIAG_PUSH
				WINPR_PRAGMA_DIAG_IGNORED_FORMAT_NONLITERAL
				const int rc = _snprintf(&format[index], formatlen - index, replace, arg);
				WINPR_PRAGMA_DIAG_POP
				if (rc < 0)
					return FALSE;
				if (!check_and_log_format_size(format, formatlen, index, rc))
					return FALSE;
				index += rc;
			}
			FormatString += fmtlen;
		}
		else
		{
			/* Unknown format string */
			if (*FormatString == '%')
				return log_invalid_fmt(FormatString);

			if (!check_and_log_format_size(format, formatlen, index, 1))
				return FALSE;
			format[index++] = *FormatString++;
		}
	}

	if (!check_and_log_format_size(format, formatlen, index, 0))
		return FALSE;
	return TRUE;
}

BOOL WLog_Layout_GetMessagePrefix(wLog* log, wLogLayout* layout, wLogMessage* message)
{
	char format[WLOG_MAX_PREFIX_SIZE] = { 0 };

	WINPR_ASSERT(layout);
	WINPR_ASSERT(message);

	char tid[32] = { 0 };
	SYSTEMTIME localTime = { 0 };
	GetLocalTime(&localTime);

	struct format_option_recurse recurse = {
		.options = NULL, .nroptions = 0, .log = log, .layout = layout, .message = message
	};

#define ENTRY(x) x, sizeof(x) - 1
	struct format_option options[] = {
		{ ENTRY("%ctx"), ENTRY("%s"), log->custom, log->context, NULL, &recurse }, /* log context */
		{ ENTRY("%dw"), ENTRY("%u"), NULL, (void*)(size_t)localTime.wDayOfWeek, NULL,
		  &recurse }, /* day of week */
		{ ENTRY("%dy"), ENTRY("%u"), NULL, (void*)(size_t)localTime.wDay, NULL,
		  &recurse }, /* day of year */
		{ ENTRY("%fl"), ENTRY("%s"), NULL, WINPR_CAST_CONST_PTR_AWAY(message->FileName, void*),
		  NULL, &recurse }, /* file */
		{ ENTRY("%fn"), ENTRY("%s"), NULL, WINPR_CAST_CONST_PTR_AWAY(message->FunctionName, void*),
		  NULL, &recurse }, /* function */
		{ ENTRY("%hr"), ENTRY("%02u"), NULL, (void*)(size_t)localTime.wHour, NULL,
		  &recurse }, /* hours */
		{ ENTRY("%ln"), ENTRY("%" PRIuz), NULL, (void*)message->LineNumber, NULL,
		  &recurse }, /* line number */
		{ ENTRY("%lv"), ENTRY("%s"), NULL,
		  WINPR_CAST_CONST_PTR_AWAY(WLOG_LEVELS[message->Level], void*), NULL,
		  &recurse }, /* log level */
		{ ENTRY("%mi"), ENTRY("%02u"), NULL, (void*)(size_t)localTime.wMinute, NULL,
		  &recurse }, /* minutes */
		{ ENTRY("%ml"), ENTRY("%03u"), NULL, (void*)(size_t)localTime.wMilliseconds, NULL,
		  &recurse },                                                   /* milliseconds */
		{ ENTRY("%mn"), ENTRY("%s"), NULL, log->Name, NULL, &recurse }, /* module name */
		{ ENTRY("%mo"), ENTRY("%u"), NULL, (void*)(size_t)localTime.wMonth, NULL,
		  &recurse }, /* month */
		{ ENTRY("%pid"), ENTRY("%u"), NULL, (void*)(size_t)GetCurrentProcessId(), NULL,
		  &recurse }, /* process id */
		{ ENTRY("%se"), ENTRY("%02u"), NULL, (void*)(size_t)localTime.wSecond, NULL,
		  &recurse },                                                 /* seconds */
		{ ENTRY("%tid"), ENTRY("%s"), get_tid, tid, NULL, &recurse }, /* thread id */
		{ ENTRY("%yr"), ENTRY("%u"), NULL, (void*)(size_t)localTime.wYear, NULL,
		  &recurse }, /* year */
		{ ENTRY("%{"), ENTRY("%}"), NULL, log->context, skip_if_null,
		  &recurse }, /* skip if no context */
	};

	recurse.options = options;
	recurse.nroptions = ARRAYSIZE(options);

	if (!replace_format_string(layout->FormatString, &recurse, format, ARRAYSIZE(format)))
		return FALSE;

	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_FORMAT_SECURITY

	WLog_PrintMessagePrefix(log, message, format);

	WINPR_PRAGMA_DIAG_POP

	return TRUE;
}

wLogLayout* WLog_GetLogLayout(wLog* log)
{
	wLogAppender* appender = NULL;
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
	DWORD nSize = 0;
	char* env = NULL;
	wLogLayout* layout = NULL;
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
		layout->FormatString = _strdup("[pid=%pid:tid=%tid] - [%fn]%{[%ctx]%}: ");
#else
		layout->FormatString =
		    _strdup("[%hr:%mi:%se:%ml] [%pid:%tid] [%lv][%mn] - [%fn]%{[%ctx]%}: ");
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
