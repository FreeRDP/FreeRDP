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

#include <winpr/config.h>

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

#include <fcntl.h>
#include <errno.h>

#include "../handle/handle.h"
#include "../pipe/pipe.h"

#include "../log.h"
#include "event.h"
#define TAG WINPR_TAG("synch.event")

#if defined(WITH_DEBUG_EVENTS)
static wArrayList* global_event_list = NULL;
#endif

static void dump_event(WINPR_EVENT* event, size_t index)
{
#if defined(WITH_DEBUG_EVENTS)
	char** msg;
	size_t used, i;
#if 0
	void* stack = winpr_backtrace(20);
	WLog_DBG(TAG, "Called from:");
	msg = winpr_backtrace_symbols(stack, &used);

	for (i = 0; i < used; i++)
		WLog_DBG(TAG, "[%" PRIdz "]: %s", i, msg[i]);

	free(msg);
	winpr_backtrace_free(stack);
#endif
	WLog_DBG(TAG, "Event handle created still not closed! [%" PRIuz ", %p]", index, event);
	msg = winpr_backtrace_symbols(event->create_stack, &used);

	for (i = 2; i < used; i++)
		WLog_DBG(TAG, "[%" PRIdz "]: %s", i, msg[i]);

	free(msg);
#endif
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

BOOL winpr_event_init(WINPR_EVENT_IMPL* event)
{
#ifdef HAVE_SYS_EVENTFD_H
	event->fds[1] = -1;
	event->fds[0] = eventfd(0, EFD_NONBLOCK);

	return event->fds[0] >= 0;
#else
	int flags;

	if (pipe(event->fds) < 0)
		return FALSE;

	flags = fcntl(event->fds[0], F_GETFL);
	if (flags < 0)
		goto out_error;

	if (fcntl(event->fds[0], F_SETFL, flags | O_NONBLOCK) < 0)
		goto out_error;

	return TRUE;

out_error:
	winpr_event_uninit(event);
	return FALSE;
#endif
}

void winpr_event_init_from_fd(WINPR_EVENT_IMPL* event, int fd)
{
	event->fds[0] = fd;
#ifndef HAVE_SYS_EVENTFD_H
	event->fds[1] = fd;
#endif
}

BOOL winpr_event_set(WINPR_EVENT_IMPL* event)
{
	int ret;
	do
	{
#ifdef HAVE_SYS_EVENTFD_H
		eventfd_t value = 1;
		ret = eventfd_write(event->fds[0], value);
#else
		ret = write(event->fds[1], "-", 1);
#endif
	} while (ret < 0 && errno == EINTR);

	return ret >= 0;
}

BOOL winpr_event_reset(WINPR_EVENT_IMPL* event)
{
	int ret;
	do
	{
		do
		{
#ifdef HAVE_SYS_EVENTFD_H
			eventfd_t value = 1;
			ret = eventfd_read(event->fds[0], &value);
#else
			char value;
			ret = read(event->fds[0], &value, 1);
#endif
		} while (ret < 0 && errno == EINTR);
	} while (ret >= 0);

	return (errno == EAGAIN);
}

void winpr_event_uninit(WINPR_EVENT_IMPL* event)
{
	if (event->fds[0] != -1)
	{
		close(event->fds[0]);
		event->fds[0] = -1;
	}

	if (event->fds[1] != -1)
	{
		close(event->fds[1]);
		event->fds[1] = -1;
	}
}

static BOOL EventCloseHandle(HANDLE handle);

static BOOL EventIsHandled(HANDLE handle)
{
	return WINPR_HANDLE_IS_HANDLED(handle, HANDLE_TYPE_EVENT, FALSE);
}

static int EventGetFd(HANDLE handle)
{
	WINPR_EVENT* event = (WINPR_EVENT*)handle;

	if (!EventIsHandled(handle))
		return -1;

	return event->impl.fds[0];
}

static BOOL EventCloseHandle_(WINPR_EVENT* event)
{
	if (!event)
		return FALSE;

	if (event->bAttached)
	{
		// don't close attached file descriptor
		event->impl.fds[0] = -1; // mark as invalid
	}

	winpr_event_uninit(&event->impl);

#if defined(WITH_DEBUG_EVENTS)
	if (global_event_list)
	{
		ArrayList_Remove(global_event_list, event);
		if (ArrayList_Count(global_event_list) < 1)
		{
			ArrayList_Free(global_event_list);
			global_event_list = NULL;
		}
	}

	winpr_backtrace_free(event->create_stack);
#endif
	free(event->name);
	free(event);
	return TRUE;
}

static BOOL EventCloseHandle(HANDLE handle)
{
	WINPR_EVENT* event = (WINPR_EVENT*)handle;

	if (!EventIsHandled(handle))
		return FALSE;

	return EventCloseHandle_(event);
}

static HANDLE_OPS ops = { EventIsHandled,
	                      EventCloseHandle,
	                      EventGetFd,
	                      NULL, /* CleanupHandle */
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL };

HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState,
                    LPCWSTR lpName)
{
	HANDLE handle;
	char* name = NULL;

	if (lpName)
	{
		int rc = ConvertFromUnicode(CP_UTF8, 0, lpName, -1, &name, 0, NULL, NULL);

		if (rc < 0)
			return NULL;
	}

	handle = CreateEventA(lpEventAttributes, bManualReset, bInitialState, name);
	free(name);
	return handle;
}

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState,
                    LPCSTR lpName)
{
	WINPR_EVENT* event = (WINPR_EVENT*)calloc(1, sizeof(WINPR_EVENT));

	if (lpEventAttributes)
		WLog_WARN(TAG, "%s [%s] does not support lpEventAttributes", __FUNCTION__, lpName);

	if (!event)
		return NULL;

	if (lpName)
		event->name = strdup(lpName);

	event->impl.fds[0] = -1;
	event->impl.fds[1] = -1;
	event->bAttached = FALSE;
	event->bManualReset = bManualReset;
	event->common.ops = &ops;
	WINPR_HANDLE_SET_TYPE_AND_MODE(event, HANDLE_TYPE_EVENT, FD_READ);

	if (!event->bManualReset)
		WLog_ERR(TAG, "auto-reset events not yet implemented");

	if (!winpr_event_init(&event->impl))
		goto fail;

	if (bInitialState)
	{
		if (!SetEvent(event))
			goto fail;
	}

#if defined(WITH_DEBUG_EVENTS)
	event->create_stack = winpr_backtrace(20);
	if (!global_event_list)
		global_event_list = ArrayList_New(TRUE);

	if (global_event_list)
		ArrayList_Append(global_event_list, event);
#endif
	return (HANDLE)event;
fail:
	EventCloseHandle_(event);
	return NULL;
}

HANDLE CreateEventExW(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCWSTR lpName, DWORD dwFlags,
                      DWORD dwDesiredAccess)
{
	BOOL initial = FALSE;
	BOOL manual = FALSE;

	if (dwFlags & CREATE_EVENT_INITIAL_SET)
		initial = TRUE;

	if (dwFlags & CREATE_EVENT_MANUAL_RESET)
		manual = TRUE;

	if (dwDesiredAccess != 0)
		WLog_WARN(TAG, "%s [%s] does not support dwDesiredAccess 0x%08" PRIx32, __FUNCTION__,
		          lpName, dwDesiredAccess);

	return CreateEventW(lpEventAttributes, manual, initial, lpName);
}

HANDLE CreateEventExA(LPSECURITY_ATTRIBUTES lpEventAttributes, LPCSTR lpName, DWORD dwFlags,
                      DWORD dwDesiredAccess)
{
	BOOL initial = FALSE;
	BOOL manual = FALSE;

	if (dwFlags & CREATE_EVENT_INITIAL_SET)
		initial = TRUE;

	if (dwFlags & CREATE_EVENT_MANUAL_RESET)
		manual = TRUE;

	if (dwDesiredAccess != 0)
		WLog_WARN(TAG, "%s [%s] does not support dwDesiredAccess 0x%08" PRIx32, __FUNCTION__,
		          lpName, dwDesiredAccess);

	return CreateEventA(lpEventAttributes, manual, initial, lpName);
}

HANDLE OpenEventW(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	/* TODO: Implement */
	WINPR_UNUSED(dwDesiredAccess);
	WINPR_UNUSED(bInheritHandle);
	WINPR_UNUSED(lpName);
	WLog_ERR(TAG, "%s not implemented", __FUNCTION__);
	return NULL;
}

HANDLE OpenEventA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName)
{
	/* TODO: Implement */
	WINPR_UNUSED(dwDesiredAccess);
	WINPR_UNUSED(bInheritHandle);
	WINPR_UNUSED(lpName);
	WLog_ERR(TAG, "%s not implemented", __FUNCTION__);
	return NULL;
}

BOOL SetEvent(HANDLE hEvent)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	BOOL status;
	WINPR_EVENT* event;
	status = FALSE;

	if (winpr_Handle_GetInfo(hEvent, &Type, &Object))
	{
		event = (WINPR_EVENT*)Object;

		status = winpr_event_set(&event->impl);
	}

	return status;
}

BOOL ResetEvent(HANDLE hEvent)
{
	ULONG Type;
	WINPR_HANDLE* Object;
	WINPR_EVENT* event;

	if (!winpr_Handle_GetInfo(hEvent, &Type, &Object))
		return FALSE;

	event = (WINPR_EVENT*)Object;

	return winpr_event_reset(&event->impl);
}

#endif

HANDLE CreateFileDescriptorEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
                                  BOOL bInitialState, int FileDescriptor, ULONG mode)
{
#ifndef _WIN32
	WINPR_EVENT* event;
	HANDLE handle = NULL;
	event = (WINPR_EVENT*)calloc(1, sizeof(WINPR_EVENT));

	if (event)
	{
		event->impl.fds[0] = -1;
		event->impl.fds[1] = -1;
		event->bAttached = TRUE;
		event->bManualReset = bManualReset;
		winpr_event_init_from_fd(&event->impl, FileDescriptor);
		event->common.ops = &ops;
		WINPR_HANDLE_SET_TYPE_AND_MODE(event, HANDLE_TYPE_EVENT, mode);
		handle = (HANDLE)event;
	}

	return handle;
#else
	return NULL;
#endif
}

HANDLE CreateFileDescriptorEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
                                  BOOL bInitialState, int FileDescriptor, ULONG mode)
{
	return CreateFileDescriptorEventW(lpEventAttributes, bManualReset, bInitialState,
	                                  FileDescriptor, mode);
}

/**
 * Returns an event based on the handle returned by GetEventWaitObject()
 */
HANDLE CreateWaitObjectEvent(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset,
                             BOOL bInitialState, void* pObject)
{
#ifndef _WIN32
	return CreateFileDescriptorEventW(lpEventAttributes, bManualReset, bInitialState,
	                                  (int)(ULONG_PTR)pObject, WINPR_FD_READ);
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
	return winpr_Handle_getFd(hEvent);
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

	event = (WINPR_EVENT*)Object;

	if (!event->bAttached && event->impl.fds[0] >= 0 && event->impl.fds[0] != FileDescriptor)
		close(event->impl.fds[0]);

	event->bAttached = TRUE;
	event->common.Mode = mode;
	event->impl.fds[0] = FileDescriptor;
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
	obj = ((void*)(long)fd);
	return obj;
#else
	return hEvent;
#endif
}
#if defined(WITH_DEBUG_EVENTS)
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>

static BOOL dump_handle_list(void* data, size_t index, va_list ap)
{
	WINPR_EVENT* event = data;
	dump_event(event, index);
	return TRUE;
}

void DumpEventHandles_(const char* fkt, const char* file, size_t line)
{
	struct rlimit r = { 0 };
	int rc = getrlimit(RLIMIT_NOFILE, &r);
	if (rc >= 0)
	{
		rlim_t x;
		size_t count = 0;
		for (x = 0; x < r.rlim_cur; x++)
		{
			int flags = fcntl(x, F_GETFD);
			if (flags >= 0)
				count++;
		}
		WLog_INFO(TAG, "------- limits [%d/%d] open files %" PRIuz, r.rlim_cur, r.rlim_max, count);
	}
	WLog_DBG(TAG, "--------- Start dump [%s %s:%" PRIuz "]", fkt, file, line);
	if (global_event_list)
	{
		ArrayList_Lock(global_event_list);
		ArrayList_ForEach(global_event_list, dump_handle_list);
		ArrayList_Unlock(global_event_list);
	}
	WLog_DBG(TAG, "--------- End dump   [%s %s:%" PRIuz "]", fkt, file, line);
}
#endif
