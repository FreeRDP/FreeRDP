/**
 * WinPR: Windows Portable Runtime
 * Debugging Utils
 *
 * Copyright 2014 Armin Novak <armin.novak@thincast.com>
 * Copyright 2014 Thincast Technologies GmbH
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
#include <winpr/buildflags.h>
#include <winpr/platform.h>

WINPR_PRAGMA_DIAG_PUSH
WINPR_PRAGMA_DIAG_IGNORED_RESERVED_ID_MACRO
WINPR_PRAGMA_DIAG_IGNORED_UNUSED_MACRO

#define __STDC_WANT_LIB_EXT1__ 1 // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)

WINPR_PRAGMA_DIAG_POP

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <winpr/crt.h>
#include <winpr/string.h>

#if defined(USE_EXECINFO)
#include <execinfo/debug.h>
#endif

#if defined(USE_UNWIND)
#include <unwind/debug.h>
#endif

#if defined(WINPR_HAVE_CORKSCREW)
#include <corkscrew/debug.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <windows/debug.h>
#endif

#include <winpr/wlog.h>
#include <winpr/debug.h>

#define TAG "com.winpr.utils.debug"

#define LINE_LENGTH_MAX 2048

static const char support_msg[] = "Invalid stacktrace buffer! check if platform is supported!";

void winpr_backtrace_free(void* buffer)
{
	if (!buffer)
		return;

#if defined(USE_UNWIND)
	winpr_unwind_backtrace_free(buffer);
#elif defined(USE_EXECINFO)
	winpr_execinfo_backtrace_free(buffer);
#elif defined(WINPR_HAVE_CORKSCREW)
	winpr_corkscrew_backtrace_free(buffer);
#elif defined(_WIN32) || defined(_WIN64)
	winpr_win_backtrace_free(buffer);
#else
	free(buffer);
	WLog_FATAL(TAG, "%s", support_msg);
#endif
}

void* winpr_backtrace(DWORD size)
{
#if defined(USE_UNWIND)
	return winpr_unwind_backtrace(size);
#elif defined(USE_EXECINFO)
	return winpr_execinfo_backtrace(size);
#elif defined(WINPR_HAVE_CORKSCREW)
	return winpr_corkscrew_backtrace(size);
#elif (defined(_WIN32) || defined(_WIN64)) && !defined(_UWP)
	return winpr_win_backtrace(size);
#else
	WLog_FATAL(TAG, "%s", support_msg);
	/* return a non nullptr buffer to allow the backtrace function family to succeed without failing
	 */
	return strndup(support_msg, sizeof(support_msg));
#endif
}

char** winpr_backtrace_symbols(void* buffer, size_t* used)
{
	if (used)
		*used = 0;

	if (!buffer)
	{
		WLog_FATAL(TAG, "%s", support_msg);
		return nullptr;
	}

#if defined(USE_UNWIND)
	return winpr_unwind_backtrace_symbols(buffer, used);
#elif defined(USE_EXECINFO)
	return winpr_execinfo_backtrace_symbols(buffer, used);
#elif defined(WINPR_HAVE_CORKSCREW)
	return winpr_corkscrew_backtrace_symbols(buffer, used);
#elif (defined(_WIN32) || defined(_WIN64)) && !defined(_UWP)
	return winpr_win_backtrace_symbols(buffer, used);
#else
	WLog_FATAL(TAG, "%s", support_msg);

	/* We return a char** on heap that is compatible with free:
	 *
	 * 1. We allocate sizeof(char*) + strlen + 1 bytes.
	 * 2. The first sizeof(char*) bytes contain the pointer to the string following the pointer.
	 * 3. The at data + sizeof(char*) contains the actual string
	 */
	size_t len = strnlen(support_msg, sizeof(support_msg));
	char* ppmsg = calloc(sizeof(char*) + len + 1, sizeof(char));
	if (!ppmsg)
		return nullptr;
	char** msgptr = (char**)ppmsg;
	char* msg = &ppmsg[sizeof(char*)];

	*msgptr = msg;
	strncpy(msg, support_msg, len);
	*used = 1;
	return msgptr;
#endif
}

void winpr_backtrace_symbols_fd(void* buffer, int fd)
{
	if (!buffer)
	{
		WLog_FATAL(TAG, "%s", support_msg);
		return;
	}

#if defined(USE_EXECINFO) && !defined(USE_UNWIND)
	winpr_execinfo_backtrace_symbols_fd(buffer, fd);
#elif !defined(ANDROID)
	{
		size_t used = 0;
		char** lines = winpr_backtrace_symbols(buffer, &used);

		if (!lines)
			return;

		for (size_t i = 0; i < used; i++)
			(void)_write(fd, lines[i], (unsigned)strnlen(lines[i], LINE_LENGTH_MAX));
		free((void*)lines);
	}
#else
	WLog_FATAL(TAG, "%s", support_msg);
#endif
}

void winpr_log_backtrace(const char* tag, DWORD level, DWORD size)
{
	winpr_log_backtrace_ex(WLog_Get(tag), level, size);
}

void winpr_log_backtrace_ex(wLog* log, DWORD level, WINPR_ATTR_UNUSED DWORD size)
{
	size_t used = 0;
	char** msg = nullptr;
	void* stack = winpr_backtrace(20);

	if (!stack)
	{
		WLog_Print(log, WLOG_ERROR, "winpr_backtrace failed!\n");
		goto fail;
	}

	msg = winpr_backtrace_symbols(stack, &used);

	if (msg)
	{
		for (size_t x = 0; x < used; x++)
			WLog_Print(log, level, "%" PRIuz ": %s", x, msg[x]);
	}
	free((void*)msg);

fail:
	winpr_backtrace_free(stack);
}

char* winpr_strerror(INT32 dw, char* dmsg, size_t size)
{
#ifdef __STDC_LIB_EXT1__
	(void)strerror_s(dw, dmsg, size);
#elif defined(WINPR_HAVE_STRERROR_R)
	(void)strerror_r(dw, dmsg, size);
#else
	(void)_snprintf(dmsg, size, "%s", strerror(dw));
#endif
	return dmsg;
}

WINPR_ATTR_NODISCARD
static BOOL starts_with(const char* tok, const char* val)
{
	const size_t len = strlen(val);
	if (strncmp(tok, val, len) != 0)
		return FALSE;

	if (!strchr(tok, '='))
		return FALSE;
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL option_equals(const char* what, const char* val)
{
	return _stricmp(what, val) == 0;
}

WINPR_ATTR_NODISCARD
static BOOL parse_on_off_option(const char* value)
{
	WINPR_ASSERT(value);
	const char* sep = strchr(value, '=');
	if (!sep)
		return TRUE;
	if (option_equals("on", &sep[1]))
		return TRUE;
	if (option_equals("true", &sep[1]))
		return TRUE;
	if (option_equals("off", &sep[1]))
		return FALSE;
	if (option_equals("false", &sep[1]))
		return FALSE;

	errno = 0;
	long val = strtol(value, nullptr, 0);
	if (errno == 0)
		return (val != 0);

	return FALSE;
}

WINPR_ATTR_NODISCARD
static BOOL option_is_debug(wLog* log, DWORD level, const char* tok)
{
	WINPR_ASSERT(log);
	WINPR_UNUSED(log);
	WINPR_UNUSED(level);

	if (starts_with(tok, "WITH_DEBUG_"))
		return parse_on_off_option(tok);

	return FALSE;
}

static void log_build_warn(wLog* log, DWORD level, const char* what, const char* msg,
                           BOOL (*cmp)(wLog* log, DWORD level, const char* tok), const char* input,
                           size_t ilen)
{
	WINPR_ASSERT(log);

	if (!input || (ilen == 0))
		return;

	char* list = calloc(ilen, sizeof(char));
	char* config = _strdup(input);

	if (config && list)
	{
		char* saveptr = nullptr;
		char* tok = strtok_s(config, " ", &saveptr);
		while (tok)
		{
			if (!cmp || cmp(log, level, tok))
				winpr_str_append(tok, list, ilen, " ");

			tok = strtok_s(nullptr, " ", &saveptr);
		}
	}
	free(config);

	if (list)
	{
		if (strlen(list) > 0)
		{
			WLog_Print(log, level, "*************************************************");
			WLog_Print(log, level, "This WinPR build is using [%s] build options:", what);

			char* saveptr = nullptr;
			char* tok = strtok_s(list, " ", &saveptr);
			while (tok)
			{
				WLog_Print(log, level, "* '%s'", tok);
				tok = strtok_s(nullptr, " ", &saveptr);
			}
			WLog_Print(log, level, "*");
			WLog_Print(log, level, "[%s] build options %s", what, msg);
			WLog_Print(log, level, "*************************************************");
		}
	}
	free(list);
}

void winpr_log_build_warn(wLog* log, DWORD level)
{
#if !defined(WINPR_ARCH_SUPPORTED) || (WINPR_ARCH_SUPPORTED == 0) || \
    defined(DISABLE_SUPPORTED_ARCH_CHECKS)
#define STR(x) #x
#endif

	const char configurations[] = {
#if !defined(WINPR_ARCH_SUPPORTED) || (WINPR_ARCH_SUPPORTED == 0)
		STR(WINPR_ARCH_SUPPORTED) "==0 "
#endif
#if defined(DISABLE_SUPPORTED_ARCH_CHECKS)
		STR(DISABLE_SUPPORTED_ARCH_CHECKS) " "
#endif
		                                   ""
	};
	WINPR_ASSERT(log);
	log_build_warn(log, level, "experimental",
	               "might crash, overwrite data or steal your kitten, you have been warned!",
	               nullptr, configurations, sizeof(configurations));

	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_OVERLENGTH_STRINGS
	log_build_warn(log, level, "debug",
	               "might leak sensitive information (credentials, ...), slow down runtime, "
	               "increase memory usage",
	               option_is_debug, WINPR_BUILD_CONFIG, sizeof(WINPR_BUILD_CONFIG));
	WINPR_PRAGMA_DIAG_POP
}
