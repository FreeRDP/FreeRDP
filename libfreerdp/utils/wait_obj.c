/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <winpr/windows.h>

#include <freerdp/utils/memory.h>
#include <freerdp/utils/wait_obj.h>

#ifndef _WIN32
#include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

struct wait_obj
{
#ifdef _WIN32
	HANDLE event;
#else
	int pipe_fd[2];
#endif
	int attached;
};

struct wait_obj*
wait_obj_new(void)
{
	struct wait_obj* obj;

	obj = xnew(struct wait_obj);

	obj->attached = 0;
#ifdef _WIN32
	obj->event = CreateEvent(NULL, TRUE, FALSE, NULL);
#else
	obj->pipe_fd[0] = -1;
	obj->pipe_fd[1] = -1;
	if (pipe(obj->pipe_fd) < 0)
	{
		printf("wait_obj_new: pipe failed\n");
		xfree(obj);
		return NULL;
	}
#endif

	return obj;
}

struct wait_obj* wait_obj_new_with_fd(void* fd)
{
	struct wait_obj* obj;

	obj = xnew(struct wait_obj);

	obj->attached = 1;
#ifdef _WIN32
	obj->event = fd;
#else
	obj->pipe_fd[0] = (int)(long)fd;
	obj->pipe_fd[1] = -1;
#endif

	return obj;
}

void
wait_obj_free(struct wait_obj* obj)
{
	if (obj)
	{

		if (obj->attached == 0)
		{
#ifdef _WIN32
			if (obj->event)
			{
				CloseHandle(obj->event);
				obj->event = NULL;
			}
#else
			if (obj->pipe_fd[0] != -1)
			{
				close(obj->pipe_fd[0]);
				obj->pipe_fd[0] = -1;
			}
			if (obj->pipe_fd[1] != -1)
			{
				close(obj->pipe_fd[1]);
				obj->pipe_fd[1] = -1;
			}
#endif
		}

		xfree(obj);
	}
}

int
wait_obj_is_set(struct wait_obj* obj)
{
#ifdef _WIN32
	return (WaitForSingleObject(obj->event, 0) == WAIT_OBJECT_0);
#else
	fd_set rfds;
	int num_set;
	struct timeval time;

	FD_ZERO(&rfds);
	FD_SET(obj->pipe_fd[0], &rfds);
	memset(&time, 0, sizeof(time));
	num_set = select(obj->pipe_fd[0] + 1, &rfds, 0, 0, &time);
	return (num_set == 1);
#endif
}

void
wait_obj_set(struct wait_obj* obj)
{
#ifdef _WIN32
	SetEvent(obj->event);
#else
	int len;

	if (wait_obj_is_set(obj))
		return;
	len = write(obj->pipe_fd[1], "sig", 4);
	if (len != 4)
		printf("wait_obj_set: error\n");
#endif
}

void
wait_obj_clear(struct wait_obj* obj)
{
#ifdef _WIN32
	ResetEvent(obj->event);
#else
	int len;

	while (wait_obj_is_set(obj))
	{
		len = read(obj->pipe_fd[0], &len, 4);
		if (len != 4)
			printf("wait_obj_clear: error\n");
	}
#endif
}

int
wait_obj_select(struct wait_obj** listobj, int numobj, int timeout)
{
	int index;
	int status;
#ifndef _WIN32
	int max;
	int sock;
	fd_set fds;
	struct timeval time;
	struct timeval* ptime;

	ptime = 0;
	if (timeout >= 0)
	{
		time.tv_sec = timeout / 1000;
		time.tv_usec = (timeout * 1000) % 1000000;
		ptime = &time;
	}

	max = 0;
	FD_ZERO(&fds);
	if (listobj)
	{
		for (index = 0; index < numobj; index++)
		{
			sock = listobj[index]->pipe_fd[0];
			FD_SET(sock, &fds);

			if (sock > max)
				max = sock;
		}
	}
	status = select(max + 1, &fds, 0, 0, ptime);
#else
	HANDLE *hnds;

	hnds = (HANDLE *) xzalloc(sizeof(HANDLE) * (numobj + 1));
	for (index = 0; index < numobj; index++)
	{
		hnds[index] = listobj[index]->event;
	}

	if (WaitForMultipleObjects(numobj, hnds, FALSE, timeout) == WAIT_FAILED)
		status = -1;
	else
		status = 0;
	xfree(hnds);
#endif

	return status;
}

void wait_obj_get_fds(struct wait_obj* obj, void** fds, int* count)
{
#ifdef _WIN32
	fds[*count] = (void*) obj->event;
#else
	if (obj->pipe_fd[0] == -1)
		return;

	fds[*count] = (void*)(long) obj->pipe_fd[0];
#endif
	(*count)++;
}
