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

#include <stdlib.h>
#include <fcntl.h>

#include <execinfo.h>

#include "debug.h"

typedef struct
{
	void** buffer;
	size_t max;
	size_t used;
} t_execinfo;

void winpr_execinfo_backtrace_free(void* buffer)
{
	t_execinfo* data = (t_execinfo*)buffer;
	if (!data)
		return;

	free(data->buffer);
	free(data);
}

void* winpr_execinfo_backtrace(DWORD size)
{
	t_execinfo* data = calloc(1, sizeof(t_execinfo));

	if (!data)
		return NULL;

	data->buffer = calloc(size, sizeof(void*));

	if (!data->buffer)
	{
		free(data);
		return NULL;
	}

	const int rc = backtrace(data->buffer, size);
	if (rc < 0)
	{
		free(data);
		return NULL;
	}
	data->max = size;
	data->used = (size_t)rc;
	return data;
}

char** winpr_execinfo_backtrace_symbols(void* buffer, size_t* used)
{
	t_execinfo* data = (t_execinfo*)buffer;
	if (used)
		*used = 0;

	if (!data)
		return NULL;

	if (used)
		*used = data->used;

	return backtrace_symbols(data->buffer, data->used);
}

void winpr_execinfo_backtrace_symbols_fd(void* buffer, int fd)
{
	t_execinfo* data = (t_execinfo*)buffer;

	if (!data)
		return;

	backtrace_symbols_fd(data->buffer, data->used, fd);
}
