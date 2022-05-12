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

#include <stdio.h>
#include <fcntl.h>

#include <winpr/crt.h>
#include <winpr/string.h>

#if defined(HAVE_EXECINFO_H)
#include <execinfo/debug.h>
#endif

#if defined(USE_UNWIND)
#include <unwind/debug.h>
#endif

#if defined(HAVE_CORKSCREW)
#include <corkscrew/debug.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <windows/debug.h>
#endif

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/debug.h>

#ifndef MIN
#define MIN(a, b) (a) < (b) ? (a) : (b)
#endif

#define TAG "com.winpr.utils.debug"
#define LOGT(...)                                           \
	do                                                      \
	{                                                       \
		WLog_Print(WLog_Get(TAG), WLOG_TRACE, __VA_ARGS__); \
	} while (0)
#define LOGD(...)                                           \
	do                                                      \
	{                                                       \
		WLog_Print(WLog_Get(TAG), WLOG_DEBUG, __VA_ARGS__); \
	} while (0)
#define LOGI(...)                                          \
	do                                                     \
	{                                                      \
		WLog_Print(WLog_Get(TAG), WLOG_INFO, __VA_ARGS__); \
	} while (0)
#define LOGW(...)                                          \
	do                                                     \
	{                                                      \
		WLog_Print(WLog_Get(TAG), WLOG_WARN, __VA_ARGS__); \
	} while (0)
#define LOGE(...)                                           \
	do                                                      \
	{                                                       \
		WLog_Print(WLog_Get(TAG), WLOG_ERROR, __VA_ARGS__); \
	} while (0)
#define LOGF(...)                                           \
	do                                                      \
	{                                                       \
		WLog_Print(WLog_Get(TAG), WLOG_FATAL, __VA_ARGS__); \
	} while (0)

static const char* support_msg = "Invalid stacktrace buffer! check if platform is supported!";

void winpr_backtrace_free(void* buffer)
{
	if (!buffer)
		return;

#if defined(USE_UNWIND)
	winpr_unwind_backtrace_free(buffer);
#elif defined(HAVE_EXECINFO_H)
	winpr_execinfo_backtrace_free(buffer);
#elif defined(HAVE_CORKSCREW)
	winpr_corkscrew_backtrace_free(buffer);
#elif defined(_WIN32) || defined(_WIN64)
	winpr_win_backtrace_free(buffer);
#else
	LOGF(support_msg);
#endif
}

void* winpr_backtrace(DWORD size)
{
#if defined(USE_UNWIND)
	return winpr_unwind_backtrace(size);
#elif defined(HAVE_EXECINFO_H)
	return winpr_execinfo_backtrace(size);
#elif defined(HAVE_CORKSCREW)
	return winpr_corkscrew_backtrace(size);
#elif (defined(_WIN32) || defined(_WIN64)) && !defined(_UWP)
	return winpr_win_backtrace(size);
#else
	LOGF(support_msg);
	return NULL;
#endif
}

char** winpr_backtrace_symbols(void* buffer, size_t* used)
{
	if (used)
		*used = 0;

	if (!buffer)
	{
		LOGF(support_msg);
		return NULL;
	}

#if defined(USE_UNWIND)
	return winpr_unwind_backtrace_symbols(buffer, used);
#elif defined(HAVE_EXECINFO_H)
	return winpr_execinfo_backtrace_symbols(buffer, used);
#elif defined(HAVE_CORKSCREW)
	return winpr_corkscrew_backtrace_symbols(buffer, used);
#elif (defined(_WIN32) || defined(_WIN64)) && !defined(_UWP)
	return winpr_win_backtrace_symbols(buffer, used);
#else
	LOGF(support_msg);
	return NULL;
#endif
}

void winpr_backtrace_symbols_fd(void* buffer, int fd)
{
	if (!buffer)
	{
		LOGF(support_msg);
		return;
	}

#if defined(HAVE_EXECINFO_H) && !defined(USE_UNWIND)
	winpr_execinfo_backtrace_symbols_fd(buffer, fd);
#elif !defined(ANDROID)
	{
		size_t i;
		size_t used = 0;
		char** lines = winpr_backtrace_symbols(buffer, &used);

		if (!lines)
			return;

		for (i = 0; i < used; i++)
			_write(fd, lines[i], (unsigned)strnlen(lines[i], UINT32_MAX));
	}
#else
	LOGF(support_msg);
#endif
}

void winpr_log_backtrace(const char* tag, DWORD level, DWORD size)
{
	winpr_log_backtrace_ex(WLog_Get(tag), level, size);
}

void winpr_log_backtrace_ex(wLog* log, DWORD level, DWORD size)
{
	size_t used, x;
	char** msg;
	void* stack = winpr_backtrace(20);

	if (!stack)
	{
		WLog_Print(log, WLOG_ERROR, "winpr_backtrace failed!\n");
		goto fail;
	}

	msg = winpr_backtrace_symbols(stack, &used);

	if (msg)
	{
		for (x = 0; x < used; x++)
			WLog_Print(log, level, "%" PRIuz ": %s\n", x, msg[x]);
	}
	free(msg);

fail:
	winpr_backtrace_free(stack);
}

char* winpr_strerror(DWORD dw, char* dmsg, size_t size)
{
#if defined(_WIN32)
	return winpr_win_strerror(dw, dmsg, size);
#else  /* defined(_WIN32) */
	_snprintf(dmsg, size, "%s", strerror(dw));
#endif /* defined(_WIN32) */
	return dmsg;
}
