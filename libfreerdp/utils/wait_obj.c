/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Virtual Channel Manager
 *
 * Copyright 2009-2011 Jay Sorg
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
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/windows.h>

#include <freerdp/utils/wait_obj.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

HANDLE wait_obj_new(void)
{
	return CreateEvent(NULL, TRUE, FALSE, NULL);
}

HANDLE wait_obj_new_with_fd(void* fd)
{
	return CreateFileDescriptorEvent(NULL, TRUE, FALSE, ((int) (long) fd));
}

void wait_obj_free(HANDLE event)
{
	CloseHandle(event);
}

int wait_obj_is_set(HANDLE event)
{
	return (WaitForSingleObject(event, 0) == WAIT_OBJECT_0);
}

void wait_obj_set(HANDLE event)
{
	SetEvent(event);
}

void wait_obj_clear(HANDLE event)
{
	ResetEvent(event);
}

int wait_obj_select(HANDLE* events, int count, int timeout)
{
	if (WaitForMultipleObjects(count, events, FALSE, timeout) == WAIT_FAILED)
		return -1;

	return 0;
}

void wait_obj_get_fds(HANDLE event, void** fds, int* count)
{
	int fd;

	fd = GetEventFileDescriptor(event);

	if (fd == -1)
		return;

	fds[*count] = ((void*) (long) fd);

	(*count)++;
}
