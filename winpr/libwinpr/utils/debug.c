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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <winpr/string.h>

#if defined(HAVE_EXECINFO_H)
#include <execinfo.h>
#endif

#if defined(ANDROID)
#include <corkscrew/backtrace.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#include <Windows.h>
#include <Dbghelp.h>
#define write _write
#endif

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/debug.h>

#define TAG "com.winpr.utils.debug"
#define LOGT(...) do { WLog_Print(WLog_Get(TAG), WLOG_TRACE, __VA_ARGS__); } while(0)
#define LOGD(...) do { WLog_Print(WLog_Get(TAG), WLOG_DEBUG, __VA_ARGS__); } while(0)
#define LOGI(...) do { WLog_Print(WLog_Get(TAG), WLOG_INFO, __VA_ARGS__); } while(0)
#define LOGW(...) do { WLog_Print(WLog_Get(TAG), WLOG_WARN, __VA_ARGS__); } while(0)
#define LOGE(...) do { WLog_Print(WLog_Get(TAG), WLOG_ERROR, __VA_ARGS__); } while(0)
#define LOGF(...) do { WLog_Print(WLog_Get(TAG), WLOG_FATAL, __VA_ARGS__); } while(0)

static const char *support_msg = "Invalid stacktrace buffer! check if platform is supported!";

#if defined(HAVE_EXECINFO_H)
typedef struct
{
	void **buffer;
	size_t max;
	size_t used;
} t_execinfo;
#endif

#if defined(_WIN32) || defined(_WIN64)
typedef struct
{
		PVOID* stack;
		ULONG used;
		ULONG max;
} t_win_stack;
#endif

#if defined(ANDROID)
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>

typedef struct
{
	backtrace_frame_t *buffer;
	size_t max;
	size_t used;
} t_corkscrew_data;

typedef struct
{
	void *hdl;
	ssize_t (*unwind_backtrace)(backtrace_frame_t *backtrace, size_t ignore_depth, size_t max_depth);
	ssize_t (*unwind_backtrace_thread)(pid_t tid, backtrace_frame_t *backtrace,
									   size_t ignore_depth, size_t max_depth);
	ssize_t (*unwind_backtrace_ptrace)(pid_t tid, const ptrace_context_t *context,
									   backtrace_frame_t *backtrace, size_t ignore_depth, size_t max_depth);
	void (*get_backtrace_symbols)(const backtrace_frame_t *backtrace, size_t frames,
								  backtrace_symbol_t *backtrace_symbols);
	void (*get_backtrace_symbols_ptrace)(const ptrace_context_t *context,
										 const backtrace_frame_t *backtrace, size_t frames,
										 backtrace_symbol_t *backtrace_symbols);
	void (*free_backtrace_symbols)(backtrace_symbol_t *backtrace_symbols, size_t frames);
	void (*format_backtrace_line)(unsigned frameNumber, const backtrace_frame_t *frame,
								  const backtrace_symbol_t *symbol, char *buffer, size_t bufferSize);
} t_corkscrew;

static pthread_once_t initialized = PTHREAD_ONCE_INIT;
static t_corkscrew *fkt = NULL;

void load_library(void)
{
	static t_corkscrew lib;
	{
		lib.hdl = dlopen("libcorkscrew.so", RTLD_LAZY);

		if (!lib.hdl)
		{
			LOGF("dlopen error %s", dlerror());
			goto fail;
		}

		lib.unwind_backtrace = dlsym(lib.hdl, "unwind_backtrace");

		if (!lib.unwind_backtrace)
		{
			LOGF("dlsym error %s", dlerror());
			goto fail;
		}

		lib.unwind_backtrace_thread = dlsym(lib.hdl, "unwind_backtrace_thread");

		if (!lib.unwind_backtrace_thread)
		{
			LOGF("dlsym error %s", dlerror());
			goto fail;
		}

		lib.unwind_backtrace_ptrace = dlsym(lib.hdl, "unwind_backtrace_ptrace");

		if (!lib.unwind_backtrace_ptrace)
		{
			LOGF("dlsym error %s", dlerror());
			goto fail;
		}

		lib.get_backtrace_symbols = dlsym(lib.hdl, "get_backtrace_symbols");

		if (!lib.get_backtrace_symbols)
		{
			LOGF("dlsym error %s", dlerror());
			goto fail;
		}

		lib.get_backtrace_symbols_ptrace = dlsym(lib.hdl, "get_backtrace_symbols_ptrace");

		if (!lib.get_backtrace_symbols_ptrace)
		{
			LOGF("dlsym error %s", dlerror());
			goto fail;
		}

		lib.free_backtrace_symbols = dlsym(lib.hdl, "free_backtrace_symbols");

		if (!lib.free_backtrace_symbols)
		{
			LOGF("dlsym error %s", dlerror());
			goto fail;
		}

		lib.format_backtrace_line = dlsym(lib.hdl, "format_backtrace_line");

		if (!lib.format_backtrace_line)
		{
			LOGF("dlsym error %s", dlerror());
			goto fail;
		}

		fkt = &lib;
		return;
	}
fail:
	{
		if (lib.hdl)
			dlclose(lib.hdl);

		fkt = NULL;
	}
}
#endif

#if defined(_WIN32) && (NTDDI_VERSION <= NTDDI_WINXP)

typedef USHORT (WINAPI * PRTL_CAPTURE_STACK_BACK_TRACE_FN)(ULONG FramesToSkip, ULONG FramesToCapture, PVOID* BackTrace, PULONG BackTraceHash);

static HMODULE g_NTDLL_Library = NULL;
static BOOL g_RtlCaptureStackBackTrace_Detected = FALSE;
static BOOL g_RtlCaptureStackBackTrace_Available = FALSE;
static PRTL_CAPTURE_STACK_BACK_TRACE_FN g_pRtlCaptureStackBackTrace = NULL;

USHORT RtlCaptureStackBackTrace(ULONG FramesToSkip, ULONG FramesToCapture, PVOID* BackTrace, PULONG BackTraceHash)
{
	if (!g_RtlCaptureStackBackTrace_Detected)
	{
		g_NTDLL_Library = LoadLibraryA("kernel32.dll");

		if (g_NTDLL_Library)
		{
			g_pRtlCaptureStackBackTrace = (PRTL_CAPTURE_STACK_BACK_TRACE_FN) GetProcAddress(g_NTDLL_Library, "RtlCaptureStackBackTrace");
			g_RtlCaptureStackBackTrace_Available = (g_pRtlCaptureStackBackTrace) ? TRUE : FALSE;
		}
		else
		{
			g_RtlCaptureStackBackTrace_Available = FALSE;
		}

		g_RtlCaptureStackBackTrace_Detected = TRUE;
	}

	if (g_RtlCaptureStackBackTrace_Available)
	{
		return (*g_pRtlCaptureStackBackTrace)(FramesToSkip, FramesToCapture, BackTrace, BackTraceHash);
	}

	return 0;
}

#endif

void winpr_backtrace_free(void* buffer)
{
	if (!buffer)
	{
		LOGF(support_msg);
		return;
	}

#if defined(HAVE_EXECINFO_H)
	t_execinfo* data = (t_execinfo*) buffer;

	free(data->buffer);

	free(data);
#elif defined(ANDROID)
	t_corkscrew_data *data = (t_corkscrew_data *)buffer;

	free(data->buffer);

	free(data);
#elif defined(_WIN32) || defined(_WIN64)
	{
		t_win_stack *data = (t_win_stack*)buffer;
		free(data->stack);
		free(data);
	}
#else
	LOGF(support_msg);
#endif
}

void* winpr_backtrace(DWORD size)
{
#if defined(HAVE_EXECINFO_H)
	t_execinfo* data = calloc(1, sizeof(t_execinfo));

	if (!data)
		return NULL;

	data->buffer = calloc(size, sizeof(void*));

	if (!data->buffer)
	{
		free(data);
		return NULL;
	}

	data->max = size;
	data->used = backtrace(data->buffer, size);
	return data;
#elif defined(ANDROID)
	t_corkscrew_data* data = calloc(1, sizeof(t_corkscrew_data));

	if (!data)
		return NULL;

	data->buffer = calloc(size, sizeof(backtrace_frame_t));

	if (!data->buffer)
	{
		free(data);
		return NULL;
	}

	pthread_once(&initialized, load_library);
	data->max = size;
	data->used = fkt->unwind_backtrace(data->buffer, 0, size);
	return data;
#elif (defined(_WIN32) || defined(_WIN64)) && !defined(_UWP)
	HANDLE process = GetCurrentProcess();
	t_win_stack* data = calloc(1, sizeof(t_win_stack));

	if (!data)
		return NULL;

	data->max = size;
	data->stack = calloc(data->max, sizeof(PVOID));

	if (!data->stack)
	{
		free(data);
		return NULL;
	}

	SymInitialize(process, NULL, TRUE);
	data->used = RtlCaptureStackBackTrace(2, size, data->stack, NULL);

	return data;
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

#if defined(HAVE_EXECINFO_H)
	t_execinfo* data = (t_execinfo*) buffer;

	if (!data)
		return NULL;

	if (used)
		*used = data->used;

	return backtrace_symbols(data->buffer, data->used);
#elif defined(ANDROID)
	t_corkscrew_data* data = (t_corkscrew_data*) buffer;

	if (!data)
		return NULL;

	pthread_once(&initialized, load_library);

	if (!fkt)
	{
		LOGF(support_msg);
		return NULL;
	}
	else
	{
		size_t line_len = (data->max > 1024) ? data->max : 1024;
		size_t i;
		char* lines = calloc(data->used + 1, sizeof(char *) * line_len);
		char** vlines = (char**) lines;
		backtrace_symbol_t* symbols = calloc(data->used, sizeof(backtrace_symbol_t));

		if (!lines || !symbols)
		{
			if (lines)
				free(lines);

			if (symbols)
				free(symbols);

			return NULL;
		}

		/* To allow a char** malloced array to be returned, allocate n+1 lines
		* and fill in the first lines[i] char with the address of lines[(i+1) * 1024] */
		for (i = 0; i < data->used; i++)
			vlines[i] = &lines[(i + 1) * line_len];

		fkt->get_backtrace_symbols(data->buffer, data->used, symbols);

		for (i = 0; i <data->used; i++)
			fkt->format_backtrace_line(i, &data->buffer[i], &symbols[i], vlines[i], line_len);

		fkt->free_backtrace_symbols(symbols, data->used);
		free(symbols);

		if (used)
			*used = data->used;

		return (char**) lines;
	}
#elif (defined(_WIN32) || defined(_WIN64)) && !defined(_UWP)
	{
		size_t i;
		size_t line_len = 1024;
		HANDLE process = GetCurrentProcess();
		t_win_stack* data = (t_win_stack*) buffer;
		char *lines = calloc(data->used + 1, sizeof(char*) * line_len);
		char **vlines = (char**) lines;
		SYMBOL_INFO* symbol = calloc(sizeof(SYMBOL_INFO) + line_len * sizeof(char), 1);
		IMAGEHLP_LINE64* line = (IMAGEHLP_LINE64*) calloc(1, sizeof(IMAGEHLP_LINE64));

		if (!lines || !symbol || !line)
		{
				if (lines)
					free(lines);

				if (symbol)
					free(symbol);

				if (line)
					free(line);

				return NULL;
		}

		line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		symbol->MaxNameLen = line_len;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

		/* To allow a char** malloced array to be returned, allocate n+1 lines
		* and fill in the first lines[i] char with the address of lines[(i+1) * 1024] */
		for (i = 0; i < data->used; i++)
			vlines[i] = &lines[(i + 1) * line_len];

		for (i = 0; i < data->used; i++)
		{
			DWORD64 address = (DWORD64)(data->stack[i]);
			DWORD displacement;

			SymFromAddr(process, address, 0, symbol);

			if (SymGetLineFromAddr64(process, address, &displacement, line))
			{
				sprintf_s(vlines[i], line_len, "%08lX: %s in %s:%lu", symbol->Address, symbol->Name, line->FileName, line->LineNumber);
			}
			else
				sprintf_s(vlines[i], line_len, "%08lX: %s", symbol->Address, symbol->Name);
			}

			if (used)
				*used = data->used;

			free(symbol);
			free(line);

			return (char**) lines;
	}
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

#if defined(HAVE_EXECINFO_H)
	t_execinfo* data = (t_execinfo*) buffer;

	if (!data)
		return;

	backtrace_symbols_fd(data->buffer, data->used, fd);
#elif defined(_WIN32) || defined(_WIN64) || defined(ANDROID)
	{
		DWORD i;
		size_t used;
		char** lines;

		lines = winpr_backtrace_symbols(buffer, &used);

		if (lines)
		{
			for (i = 0; i < used; i++)
				write(fd, lines[i], strlen(lines[i]));
		}
	}
#else
	LOGF(support_msg);
#endif
}

void winpr_log_backtrace(const char* tag, DWORD level, DWORD size)
{
	size_t used, x;
	char **msg;
	void *stack = winpr_backtrace(20);

	if (!stack)
	{
		WLog_ERR(tag, "winpr_backtrace failed!\n");
		winpr_backtrace_free(stack);
		return;
	}

	msg = winpr_backtrace_symbols(stack, &used);
	if (msg)
	{
		for (x=0; x<used; x++)
			WLog_LVL(tag, level, "%zd: %s\n", x, msg[x]);
	}
	winpr_backtrace_free(stack);
}

char* winpr_strerror(DWORD dw, char* dmsg, size_t size)
{
#if defined(_WIN32)
	DWORD rc;
	DWORD nSize = 0;
	DWORD dwFlags = 0;
	LPTSTR msg = NULL;
	BOOL alloc = FALSE;

	dwFlags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

#ifdef FORMAT_MESSAGE_ALLOCATE_BUFFER
	alloc = TRUE;
	dwFlags |= FORMAT_MESSAGE_ALLOCATE_BUFFER;
#else
	nSize = (DWORD) (size * 2);
	msg = (LPTSTR) calloc(nSize, sizeof(TCHAR));
#endif

	rc = FormatMessage(dwFlags, NULL, dw, 0, alloc ? (LPTSTR) &msg : msg, nSize, NULL);

	if (rc) {
#if defined(UNICODE)
		WideCharToMultiByte(CP_ACP, 0, msg, rc, dmsg, size - 1, NULL, NULL);
#else /* defined(UNICODE) */
		memcpy(dmsg, msg, min(rc, size - 1));
#endif /* defined(UNICODE) */
		dmsg[min(rc, size - 1)] = 0;

#ifdef FORMAT_MESSAGE_ALLOCATE_BUFFER
		LocalFree(msg);
#else
		free(msg);
#endif
	} else {
		_snprintf(dmsg, size, "FAILURE: %08X", GetLastError());
	}
#else /* defined(_WIN32) */
	_snprintf(dmsg, size, "%s", strerror(dw));
#endif /* defined(_WIN32) */

	return dmsg;
}
