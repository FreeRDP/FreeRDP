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

#include <stdio.h>
#include <fcntl.h>

#include <io.h>
#include <windows.h>
#include <dbghelp.h>

#include "debug.h"

#ifndef MIN
#define MIN(a, b) (a) < (b) ? (a) : (b)
#endif

typedef struct
{
	PVOID* stack;
	ULONG used;
	ULONG max;
} t_win_stack;

void winpr_win_backtrace_free(void* buffer)
{
	t_win_stack* data = (t_win_stack*)buffer;
	if (!data)
		return;

	free(data->stack);
	free(data);
}

void* winpr_win_backtrace(DWORD size)
{
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
}

char** winpr_win_backtrace_symbols(void* buffer, size_t* used)
{
	if (used)
		*used = 0;

	if (!buffer)
		return NULL;

	{
		size_t i;
		size_t line_len = 1024;
		HANDLE process = GetCurrentProcess();
		t_win_stack* data = (t_win_stack*)buffer;
		size_t array_size = data->used * sizeof(char*);
		size_t lines_size = data->used * line_len;
		char** vlines = calloc(1, array_size + lines_size);
		SYMBOL_INFO* symbol = calloc(1, sizeof(SYMBOL_INFO) + line_len * sizeof(char));
		IMAGEHLP_LINE64* line = (IMAGEHLP_LINE64*)calloc(1, sizeof(IMAGEHLP_LINE64));

		if (!vlines || !symbol || !line)
		{
			free(vlines);
			free(symbol);
			free(line);
			return NULL;
		}

		line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
		symbol->MaxNameLen = (ULONG)line_len;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

		/* Set the pointers in the allocated buffer's initial array section */
		for (i = 0; i < data->used; i++)
			vlines[i] = (char*)vlines + array_size + i * line_len;

		for (i = 0; i < data->used; i++)
		{
			DWORD64 address = (DWORD64)(data->stack[i]);
			DWORD displacement;
			SymFromAddr(process, address, 0, symbol);

			if (SymGetLineFromAddr64(process, address, &displacement, line))
			{
				sprintf_s(vlines[i], line_len, "%016" PRIx64 ": %s in %s:%" PRIu32, symbol->Address,
				          symbol->Name, line->FileName, line->LineNumber);
			}
			else
				sprintf_s(vlines[i], line_len, "%016" PRIx64 ": %s", symbol->Address, symbol->Name);
		}

		if (used)
			*used = data->used;

		free(symbol);
		free(line);
		return vlines;
	}
}

char* winpr_win_strerror(DWORD dw, char* dmsg, size_t size)
{
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
	nSize = (DWORD)(size * 2);
	msg = (LPTSTR)calloc(nSize, sizeof(TCHAR));
#endif
	rc = FormatMessage(dwFlags, NULL, dw, 0, alloc ? (LPTSTR)&msg : msg, nSize, NULL);

	if (rc)
	{
#if defined(UNICODE)
		WideCharToMultiByte(CP_ACP, 0, msg, rc, dmsg, (int)MIN(size - 1, INT_MAX), NULL, NULL);
#else  /* defined(UNICODE) */
		memcpy(dmsg, msg, MIN(rc, size - 1));
#endif /* defined(UNICODE) */
		dmsg[MIN(rc, size - 1)] = 0;
#ifdef FORMAT_MESSAGE_ALLOCATE_BUFFER
		LocalFree(msg);
#else
		free(msg);
#endif
	}
	else
	{
		_snprintf(dmsg, size, "FAILURE: 0x%08" PRIX32 "", GetLastError());
	}

	return dmsg;
}
