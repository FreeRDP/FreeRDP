/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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
#include <string.h>
#include <stdlib.h>

#include <winpr/synch.h>

#ifndef _WIN32

#include "synch.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_SYS_EVENTFD_H
#include <sys/eventfd.h>
#endif

#include <errno.h>

#include "../handle/handle.h"
#include "../pipe/pipe.h"

#include "../log.h"
#define TAG WINPR_TAG("synch.event")

static BOOL EventCloseHandle(HANDLE handle);

static BOOL EventIsHandled(HANDLE handle)
{
	WINPR_TIMER* pEvent = (WINPR_TIMER*) handle;

	if (!pEvent || (pEvent->Type != HANDLE_TYPE_EVENT))
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

static int EventGetFd(HANDLE handle)
{
	WINPR_EVENT* event = (WINPR_EVENT*)handle;

	if (!EventIsHandled(handle))
		return -1;

	return event->pipe_fd[0];
}

static BOOL EventCloseHandle(HANDLE handle)
{
	WINPR_EVENT* event = (WINPR_EVENT*) handle;

	if (!EventIsHandled(handle))
		return FALSE;

	if (!event->bAttached)
	{
		if (event->pipe_fd[0] != -1)
		{
			close(event->pipe_fd[0]);
			event->pipe_fd[0] = -1;
		}

		if (event->pipe_fd[1] != -1)
		{
			close(event->pipe_fd[1]);
			event->pipe_fd[1] = -1;
		}
	}

	free(event);
	return TRUE;
}

static HANDLE_OPS ops =
{
	EventIsHandled,
	EventCloseHandle,
	EventGetFd,
	NULL /* CleanupHandle */
};

HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState,
                    LPCWSTR lpName)
{
	WINPR_EVENT* event = (WINPR_EVENT*) calloc(1, sizeof(WINPR_EVENT));

	if (!event)
		return NULL;

	event->bAttached = FALSE;
	event->bManualReset = bManualReset;
	event->ops = &ops;
	WINPR_HANDLE_SET_TYPE_AND_MODE(event, HANDLE_TYPE_EVENT, FD_READ);

	if (!event->bManualReset)
		WLog_ERR(TAG, "auto-reset events not yet implemented");

	event->pipe_fd[0] = -1;
	event->pipe_fd[1] = -1;
#ifdef HAVE_SYS_EVENTFD_H
	event->pipe_fd[0] = eventfd(0, EFD_NONBLOCK);

	if (event->pipe_fd[0] < 0)
		goto fail;

#else

	if (pipe(event->pipe_fd) < 0)
		goto fail;

#endif

	if (bInitialState)
		SetEvent(event);

	return (HANDLE)event;
fail:
	free(event);
	return NULL;
}

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState,
                    LPCSTR lpName)
{
	return CreateEventW(lpEventAttributes, bManualReset, bInitialState, NULL);
}

HANDLE CreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName, DWORD dwFlags,
                      DWORD dwDesiredAccess)
{
	return NULL;
}

HANDLE CreateEventExA(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCSTR lpName, DWORD dwFlags,
                      DWORD dwDesiredAccess)
{
	return NULL;
}

HANDLE OpenEventW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	return NULL;
}

HANDLE OpenEventA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
{
	return NULL;
}

#ifdef HAVE_SYS_EVENTFD_H
#if !defined(WITH_EVENTFD_READ_WRITE)
static int eventfd_read(int fd, eventfd_t* value)
{
	return (read(fd, value, sizeof(*value)) == sizeof(*value)) ? 0 : -1;
}

static int eventfd_write(int fd, eventfd_t value)
{
	return (write(fd, &value, sizeof(value)) == sizeof(value)) ? 0 : -1;
}
#endif
#endif

BOOL SetEvent(HANDLE hEvent)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	int length;
	BOOL status;
	WINPR_EVENT* event;
	status = FALSE;

	if (winpr_Handle_GetInfo(hEvent, &Type, &Object))
	{
		event = (WINPR_EVENT*) Object;
#ifdef HAVE_SYS_EVENTFD_H
		eventfd_t val = 1;

		do
		{
			length = eventfd_write(event->pipe_fd[0], val);
		}
		while ((length < 0) && (errno == EINTR));

		status = (length == 0) ? TRUE : FALSE;
#else

		if (WaitForSingleObject(hEvent, 0) != WAIT_OBJECT_0)
		{
			length = write(event->pipe_fd[1], "-", 1);

			if (length == 1)
				status = TRUE;
		}
		else
		{
			status = TRUE;
		}

#endif
	}

	return status;
}

BOOL ResetEvent(HANDLE hEvent)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	int length;
	BOOL status = TRUE;
	WINPR_EVENT* event;

	if (!winpr_Handle_GetInfo(hEvent, &Type, &Object))
		return FALSE;

	event = (WINPR_EVENT*) Object;

	while (status && WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0)
	{
		do
		{
#ifdef HAVE_SYS_EVENTFD_H
			eventfd_t value;
			length = eventfd_read(event->pipe_fd[0], &value);
#else
			length = read(event->pipe_fd[0], &length, 1);
#endif
		}
		while ((length < 0) && (errno == EINTR));

		if (length < 0)
			status = FALSE;
	}

	return status;
}

#endif


HANDLE CreateFileDescriptorEventW(LPSECURITY_ATTRIBUTES lpEventAttributes,
                                  BOOL bManualReset, BOOL bInitialState,
                                  int FileDescriptor, ULONG mode)
{
#ifndef _WIN32
	WINPR_EVENT* event;
	HANDLE handle = NULL;
	event = (WINPR_EVENT*) calloc(1, sizeof(WINPR_EVENT));

	if (event)
	{
		event->bAttached = TRUE;
		event->bManualReset = bManualReset;
		event->pipe_fd[0] = FileDescriptor;
		event->pipe_fd[1] = -1;
		event->ops = &ops;
		WINPR_HANDLE_SET_TYPE_AND_MODE(event, HANDLE_TYPE_EVENT, mode);
		handle = (HANDLE) event;
	}

	return handle;
#else
	return NULL;
#endif
}

HANDLE CreateFileDescriptorEventA(LPSECURITY_ATTRIBUTES lpEventAttributes,
                                  BOOL bManualReset, BOOL bInitialState,
                                  int FileDescriptor, ULONG mode)
{
	return CreateFileDescriptorEventW(lpEventAttributes, bManualReset,
	                                  bInitialState, FileDescriptor, mode);
}

/**
 * Returns an event based on the handle returned by GetEventWaitObject()
 */
HANDLE CreateWaitObjectEvent(LPSECURITY_ATTRIBUTES lpEventAttributes,
                             BOOL bManualReset, BOOL bInitialState, void* pObject)
{
#ifndef _WIN32
	return CreateFileDescriptorEventW(lpEventAttributes, bManualReset,
	                                  bInitialState, (int)(ULONG_PTR) pObject, WINPR_FD_READ);
#else
	HANDLE hEvent = NULL;
	DuplicateHandle(GetCurrentProcess(), pObject, GetCurrentProcess(), &hEvent, 0, FALSE,
	                DUPLICATE_SAME_ACCESS);
	return hEvent;
#endif
}

/*
 * Returns inner file descriptor for usage with select()
 * This file descriptor is not usable on Windows
 */

int GetEventFileDescriptor(HANDLE hEvent)
{
#ifndef _WIN32
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_EVENT* event;

	if (!winpr_Handle_GetInfo(hEvent, &Type, &Object))
		return -1;

	event = (WINPR_EVENT*) Object;

	if (Type == HANDLE_TYPE_NAMED_PIPE)
	{
		WINPR_NAMED_PIPE* named = (WINPR_NAMED_PIPE*)hEvent;

		if (named->ServerMode)
		{
			return named->serverfd;
		}
		else
		{
			return named->clientfd;
		}
	}

	return event->pipe_fd[0];
#else
	return -1;
#endif
}

/*
 * Set inner file descriptor for usage with select()
 * This file descriptor is not usable on Windows
 */

int SetEventFileDescriptor(HANDLE hEvent, int FileDescriptor, ULONG mode)
{
#ifndef _WIN32
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_EVENT* event;

	if (!winpr_Handle_GetInfo(hEvent, &Type, &Object))
		return -1;

	event = (WINPR_EVENT*) Object;

	if (!event->bAttached && event->pipe_fd[0] >= 0)
		close(event->pipe_fd[0]);

	event->bAttached = TRUE;
	event->Mode = mode;
	event->pipe_fd[0] = FileDescriptor;
	return 0;
#else
	return -1;
#endif
}

/**
 * Returns platform-specific wait object as a void pointer
 *
 * On Windows, the returned object is the same as the hEvent
 * argument and is an event HANDLE usable in WaitForMultipleObjects
 *
 * On other platforms, the returned object can be cast to an int
 * to obtain a file descriptor usable in select()
 */

void* GetEventWaitObject(HANDLE hEvent)
{
#ifndef _WIN32
	int fd;
	void* obj;
	fd = GetEventFileDescriptor(hEvent);
	obj = ((void*)(long) fd);
	return obj;
#else
	return hEvent;
#endif
}

