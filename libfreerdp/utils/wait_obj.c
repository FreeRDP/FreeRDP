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

struct wait_obj
{
	HANDLE event;
};

struct wait_obj* wait_obj_new(void)
{
	struct wait_obj* obj;

	obj = (struct wait_obj*) malloc(sizeof(struct wait_obj));
	ZeroMemory(obj, sizeof(struct wait_obj));

	obj->event = CreateEvent(NULL, TRUE, FALSE, NULL);

	return obj;
}

struct wait_obj* wait_obj_new_with_fd(void* fd)
{
	struct wait_obj* obj;

	obj = (struct wait_obj*) malloc(sizeof(struct wait_obj));
	ZeroMemory(obj, sizeof(struct wait_obj));

	obj->event = CreateFileDescriptorEvent(NULL, TRUE, FALSE, ((int) (long) fd));

	return obj;
}

void wait_obj_free(struct wait_obj* obj)
{
	CloseHandle(obj->event);
}

int wait_obj_is_set(struct wait_obj* obj)
{
	return (WaitForSingleObject(obj->event, 0) == WAIT_OBJECT_0);
}

void wait_obj_set(struct wait_obj* obj)
{
	SetEvent(obj->event);
}

void wait_obj_clear(struct wait_obj* obj)
{
	ResetEvent(obj->event);
}

int wait_obj_select(struct wait_obj** listobj, int numobj, int timeout)
{
	int index;
	int status;
	HANDLE* handles;

	handles = (HANDLE*) malloc(sizeof(HANDLE) * (numobj + 1));
	ZeroMemory(handles, sizeof(HANDLE) * (numobj + 1));

	for (index = 0; index < numobj; index++)
		handles[index] = listobj[index]->event;

	if (WaitForMultipleObjects(numobj, handles, FALSE, timeout) == WAIT_FAILED)
		status = -1;
	else
		status = 0;

	free(handles);

	return status;
}

void wait_obj_get_fds(struct wait_obj* obj, void** fds, int* count)
{
	int fd;

	fd = GetEventFileDescriptor(obj->event);

	if (fd == -1)
		return;

	fds[*count] = ((void*) (long) fd);

	(*count)++;
}
