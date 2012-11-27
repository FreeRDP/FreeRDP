/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
{
	WINPR_EVENT* event;
	HANDLE handle = NULL;

	event = (WINPR_EVENT*) malloc(sizeof(WINPR_EVENT));

	if (event)
	{
		event->bAttached = FALSE;
		event->bManualReset = bManualReset;

		if (!event->bManualReset)
		{
			printf("CreateEventW: auto-reset events not yet implemented\n");
		}

		event->pipe_fd[0] = -1;
		event->pipe_fd[1] = -1;

		if (pipe(event->pipe_fd) < 0)
		{
			printf("CreateEventW: failed to create event\n");
			return NULL;
		}

		handle = winpr_Handle_Insert(HANDLE_TYPE_EVENT, event);
	}

	return handle;
}

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName)
{
	return CreateEventW(lpEventAttributes, bManualReset, bInitialState, NULL);
}

HANDLE CreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
{
	return NULL;
}

HANDLE CreateEventExA(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCSTR lpName, DWORD dwFlags, DWORD dwDesiredAccess)
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

BOOL SetEvent(HANDLE hEvent)
{
	ULONG Type;
	PVOID Object;
	int length;
	WINPR_EVENT* event;

	if (!winpr_Handle_GetInfo(hEvent, &Type, &Object))
		return FALSE;

	event = (WINPR_EVENT*) Object;

	if (WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0)
		return TRUE;

	length = write(event->pipe_fd[1], "-", 1);

	if (length != 1)
		return FALSE;

	return TRUE;
}

BOOL ResetEvent(HANDLE hEvent)
{
	ULONG Type;
	PVOID Object;
	int length;
	WINPR_EVENT* event;

	if (!winpr_Handle_GetInfo(hEvent, &Type, &Object))
		return FALSE;

	event = (WINPR_EVENT*) Object;

	while (WaitForSingleObject(hEvent, 0) == WAIT_OBJECT_0)
	{
		length = read(event->pipe_fd[0], &length, 1);

		if (length != 1)
			return FALSE;
	}

	return TRUE;
}

#endif

HANDLE CreateFileDescriptorEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, int FileDescriptor)
{
#ifndef _WIN32
	WINPR_EVENT* event;
	HANDLE handle = NULL;

	event = (WINPR_EVENT*) malloc(sizeof(WINPR_EVENT));

	if (event)
	{
		event->bAttached = TRUE;
		event->bManualReset = bManualReset;

		if (!event->bManualReset)
		{
			printf("CreateFileDescriptorEventW: auto-reset events not yet implemented\n");
		}

		event->pipe_fd[0] = FileDescriptor;
		event->pipe_fd[1] = -1;

		handle = winpr_Handle_Insert(HANDLE_TYPE_EVENT, event);
	}

	return handle;
#else
	return NULL;
#endif
}

HANDLE CreateFileDescriptorEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, int FileDescriptor)
{
	return CreateFileDescriptorEventW(lpEventAttributes, bManualReset, bInitialState, FileDescriptor);
}

int GetEventFileDescriptor(HANDLE hEvent)
{
#ifndef _WIN32
	ULONG Type;
	PVOID Object;
	WINPR_EVENT* event;

	if (!winpr_Handle_GetInfo(hEvent, &Type, &Object))
		return -1;

	event = (WINPR_EVENT*) Object;

	return event->pipe_fd[0];
#else
	return -1;
#endif
}
