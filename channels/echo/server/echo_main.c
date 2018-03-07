/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Echo Virtual Channel Extension
 *
 * Copyright 2014 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/sysinfo.h>

#include <freerdp/server/echo.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("echo.server")

typedef struct _echo_server
{
	echo_server_context context;

	BOOL opened;

	HANDLE stopEvent;

	HANDLE thread;
	void* echo_channel;

	DWORD SessionId;

} echo_server;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_server_open_channel(echo_server* echo)
{
	DWORD Error;
	HANDLE hEvent;
	DWORD StartTick;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;

	if (WTSQuerySessionInformationA(echo->context.vcm, WTS_CURRENT_SESSION,
	                                WTSSessionId, (LPSTR*) &pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		return ERROR_INTERNAL_ERROR;
	}

	echo->SessionId = (DWORD) * pSessionId;
	WTSFreeMemory(pSessionId);
	hEvent = WTSVirtualChannelManagerGetEventHandle(echo->context.vcm);
	StartTick = GetTickCount();

	while (echo->echo_channel == NULL)
	{
		if (WaitForSingleObject(hEvent, 1000) == WAIT_FAILED)
		{
			Error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"!", Error);
			return Error;
		}

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

	return echo->echo_channel ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static DWORD WINAPI echo_server_thread_func(LPVOID arg)
{
	wStream* s;
	void* buffer;
	DWORD nCount;
	HANDLE events[8];
	BOOL ready = FALSE;
	HANDLE ChannelEvent;
	DWORD BytesReturned = 0;
	echo_server* echo = (echo_server*) arg;
	UINT error;
	DWORD status;

	if ((error = echo_server_open_channel(echo)))
	{
		UINT error2 = 0;
		WLog_ERR(TAG, "echo_server_open_channel failed with error %"PRIu32"!", error);
		IFCALLRET(echo->context.OpenResult, error2, &echo->context,
		          ECHO_SERVER_OPEN_RESULT_NOTSUPPORTED);

		if (error2)
			WLog_ERR(TAG, "echo server's OpenResult callback failed with error %"PRIu32"",
			         error2);

		goto out;
	}

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	if (WTSVirtualChannelQuery(echo->echo_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
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
		status = WaitForMultipleObjects(nCount, events, FALSE, 100);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
		{
			IFCALLRET(echo->context.OpenResult, error, &echo->context,
			          ECHO_SERVER_OPEN_RESULT_CLOSED);

			if (error)
				WLog_ERR(TAG, "OpenResult failed with error %"PRIu32"!", error);

			break;
		}

		if (WTSVirtualChannelQuery(echo->echo_channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			IFCALLRET(echo->context.OpenResult, error, &echo->context,
			          ECHO_SERVER_OPEN_RESULT_ERROR);

			if (error)
				WLog_ERR(TAG, "OpenResult failed with error %"PRIu32"!", error);

			break;
		}

		ready = *((BOOL*) buffer);
		WTSFreeMemory(buffer);

		if (ready)
		{
			IFCALLRET(echo->context.OpenResult, error, &echo->context,
			          ECHO_SERVER_OPEN_RESULT_OK);

			if (error)
				WLog_ERR(TAG, "OpenResult failed with error %"PRIu32"!", error);

			break;
		}
	}

	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		WTSVirtualChannelClose(echo->echo_channel);
		ExitThread(ERROR_NOT_ENOUGH_MEMORY);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	while (ready)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

		Stream_SetPosition(s, 0);
		WTSVirtualChannelRead(echo->echo_channel, 0, NULL, 0, &BytesReturned);

		if (BytesReturned < 1)
			continue;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			error = CHANNEL_RC_NO_MEMORY;
			break;
		}

		if (WTSVirtualChannelRead(echo->echo_channel, 0, (PCHAR) Stream_Buffer(s),
		                          (ULONG) Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		IFCALLRET(echo->context.Response, error, &echo->context,
		          (BYTE*) Stream_Buffer(s), BytesReturned);

		if (error)
		{
			WLog_ERR(TAG, "Response failed with error %"PRIu32"!", error);
			break;
		}
	}

	Stream_Free(s, TRUE);
	WTSVirtualChannelClose(echo->echo_channel);
	echo->echo_channel = NULL;
out:

	if (error && echo->context.rdpcontext)
		setChannelError(echo->context.rdpcontext, error,
		                "echo_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_server_open(echo_server_context* context)
{
	echo_server* echo = (echo_server*) context;

	if (echo->thread == NULL)
	{
		if (!(echo->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return ERROR_INTERNAL_ERROR;
		}

		if (!(echo->thread = CreateThread(NULL, 0, echo_server_thread_func, (void*) echo, 0, NULL)))
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			CloseHandle(echo->stopEvent);
			echo->stopEvent = NULL;
			return ERROR_INTERNAL_ERROR;
		}
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT echo_server_close(echo_server_context* context)
{
	UINT error = CHANNEL_RC_OK;
	echo_server* echo = (echo_server*) context;

	if (echo->thread)
	{
		SetEvent(echo->stopEvent);

		if (WaitForSingleObject(echo->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
			return error;
		}

		CloseHandle(echo->thread);
		CloseHandle(echo->stopEvent);
		echo->thread = NULL;
		echo->stopEvent = NULL;
	}

	return error;
}

static BOOL echo_server_request(echo_server_context* context,
                                const BYTE* buffer, UINT32 length)
{
	echo_server* echo = (echo_server*) context;
	return WTSVirtualChannelWrite(echo->echo_channel, (PCHAR) buffer, length, NULL);
}

echo_server_context* echo_server_context_new(HANDLE vcm)
{
	echo_server* echo;
	echo = (echo_server*) calloc(1, sizeof(echo_server));

	if (echo)
	{
		echo->context.vcm = vcm;
		echo->context.Open = echo_server_open;
		echo->context.Close = echo_server_close;
		echo->context.Request = echo_server_request;
	}
	else
		WLog_ERR(TAG, "calloc failed!");

	return (echo_server_context*) echo;
}

void echo_server_context_free(echo_server_context* context)
{
	echo_server* echo = (echo_server*) context;
	echo_server_close(context);
	free(echo);
}
