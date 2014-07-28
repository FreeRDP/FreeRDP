/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Echo Virtual Channel Extension
 *
 * Copyright 2014 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
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
#include <winpr/thread.h>
#include <winpr/stream.h>

#include <freerdp/server/echo.h>

typedef struct _echo_server
{
	echo_server_context context;

	BOOL opened;

	HANDLE stopEvent;

	HANDLE thread;
	void* echo_channel;

	DWORD SessionId;

} echo_server;

static BOOL echo_server_open_channel(echo_server* echo)
{
	DWORD Error;
	HANDLE hEvent;
	DWORD StartTick;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;

	if (WTSQuerySessionInformationA(echo->context.vcm, WTS_CURRENT_SESSION,
		WTSSessionId, (LPSTR*) &pSessionId, &BytesReturned) == FALSE)
	{
		return FALSE;
	}
	echo->SessionId = (DWORD) *pSessionId;
	WTSFreeMemory(pSessionId);

	hEvent = WTSVirtualChannelManagerGetEventHandle(echo->context.vcm);
	StartTick = GetTickCount();

	while (echo->echo_channel == NULL)
	{
		WaitForSingleObject(hEvent, 1000);

		echo->echo_channel = WTSVirtualChannelOpenEx(echo->SessionId,
			"ECHO", WTS_CHANNEL_OPTION_DYNAMIC);

		if (echo->echo_channel)
			break;

		Error = GetLastError();
		if (Error == ERROR_NOT_FOUND)
			break;

		if (GetTickCount() - StartTick > 5000)
			break;
	}

	return echo->echo_channel ? TRUE : FALSE;
}

static void* echo_server_thread_func(void* arg)
{
	wStream* s;
	void* buffer;
	DWORD nCount;
	HANDLE events[8];
	BOOL ready = FALSE;
	HANDLE ChannelEvent;
	DWORD BytesReturned = 0;
	echo_server* echo = (echo_server*) arg;

	if (echo_server_open_channel(echo) == FALSE)
	{
		IFCALL(echo->context.OpenResult, &echo->context, ECHO_SERVER_OPEN_RESULT_NOTSUPPORTED);
		return NULL;
	}

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	if (WTSVirtualChannelQuery(echo->echo_channel, WTSVirtualEventHandle, &buffer, &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = echo->stopEvent;
	events[nCount++] = ChannelEvent;

	/* Wait for the client to confirm that the Graphics Pipeline dynamic channel is ready */

	while (1)
	{
		if (WaitForMultipleObjects(nCount, events, FALSE, 100) == WAIT_OBJECT_0)
		{
			IFCALL(echo->context.OpenResult, &echo->context, ECHO_SERVER_OPEN_RESULT_CLOSED);
			break;
		}

		if (WTSVirtualChannelQuery(echo->echo_channel, WTSVirtualChannelReady, &buffer, &BytesReturned) == FALSE)
		{
			IFCALL(echo->context.OpenResult, &echo->context, ECHO_SERVER_OPEN_RESULT_ERROR);
			break;
		}

		ready = *((BOOL*) buffer);

		WTSFreeMemory(buffer);

		if (ready)
		{
			IFCALL(echo->context.OpenResult, &echo->context, ECHO_SERVER_OPEN_RESULT_OK);
			break;
		}
	}

	s = Stream_New(NULL, 4096);

	while (ready)
	{
		if (WaitForMultipleObjects(nCount, events, FALSE, INFINITE) == WAIT_OBJECT_0)
			break;

		Stream_SetPosition(s, 0);

		WTSVirtualChannelRead(echo->echo_channel, 0, NULL, 0, &BytesReturned);
		if (BytesReturned < 1)
			continue;
		Stream_EnsureRemainingCapacity(s, BytesReturned);
		if (WTSVirtualChannelRead(echo->echo_channel, 0, (PCHAR) Stream_Buffer(s),
			(ULONG) Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			break;
		}

		IFCALL(echo->context.Response, &echo->context, (PCHAR) Stream_Buffer(s), BytesReturned);
	}

	Stream_Free(s, TRUE);
	WTSVirtualChannelClose(echo->echo_channel);
	echo->echo_channel = NULL;

	return NULL;
}

static void echo_server_open(echo_server_context* context)
{
	echo_server* echo = (echo_server*) context;

	if (echo->thread == NULL)
	{
		echo->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		echo->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) echo_server_thread_func, (void*) echo, 0, NULL);
	}
}

static void echo_server_close(echo_server_context* context)
{
	echo_server* echo = (echo_server*) context;

	if (echo->thread)
	{
		SetEvent(echo->stopEvent);
		WaitForSingleObject(echo->thread, INFINITE);
		CloseHandle(echo->thread);
		CloseHandle(echo->stopEvent);
		echo->thread = NULL;
		echo->stopEvent = NULL;
	}
}

static BOOL echo_server_request(echo_server_context* context, const BYTE* buffer, UINT32 length)
{
	echo_server* echo = (echo_server*) context;

	return WTSVirtualChannelWrite(echo->echo_channel, (PCHAR) buffer, length, NULL);
}

echo_server_context* echo_server_context_new(HANDLE vcm)
{
	echo_server* echo;

	echo = (echo_server*) calloc(1, sizeof(echo_server));

	echo->context.vcm = vcm;
	echo->context.Open = echo_server_open;
	echo->context.Close = echo_server_close;
	echo->context.Request = echo_server_request;

	return (echo_server_context*) echo;
}

void echo_server_context_free(echo_server_context* context)
{
	echo_server* echo = (echo_server*) context;

	echo_server_close(context);

	free(echo);
}
