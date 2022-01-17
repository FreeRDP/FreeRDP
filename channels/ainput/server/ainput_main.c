/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Advanced Input Virtual Channel Extension
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#include <freerdp/server/ainput.h>
#include <freerdp/channels/ainput.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("ainput.server")

typedef struct ainput_server_
{
	ainput_server_context context;

	BOOL opened;

	HANDLE stopEvent;

	HANDLE thread;
	void* ainput_channel;

	DWORD SessionId;

} ainput_server;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ainput_server_open_channel(ainput_server* ainput)
{
	DWORD Error;
	HANDLE hEvent;
	DWORD StartTick;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;

	WINPR_ASSERT(ainput);

	if (WTSQuerySessionInformationA(ainput->context.vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		return ERROR_INTERNAL_ERROR;
	}

	ainput->SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);
	hEvent = WTSVirtualChannelManagerGetEventHandle(ainput->context.vcm);
	StartTick = GetTickCount();

	while (ainput->ainput_channel == NULL)
	{
		if (WaitForSingleObject(hEvent, 1000) == WAIT_FAILED)
		{
			Error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", Error);
			return Error;
		}

		ainput->ainput_channel = WTSVirtualChannelOpenEx(ainput->SessionId, AINPUT_DVC_CHANNEL_NAME,
		                                                 WTS_CHANNEL_OPTION_DYNAMIC);

		if (ainput->ainput_channel)
			break;

		Error = GetLastError();

		if (Error == ERROR_NOT_FOUND)
			break;

		if (GetTickCount() - StartTick > 5000)
			break;
	}

	return ainput->ainput_channel ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static DWORD WINAPI ainput_server_thread_func(LPVOID arg)
{
	wStream* s;
	void* buffer;
	DWORD nCount;
	HANDLE events[8] = { 0 };
	BOOL ready = FALSE;
	HANDLE ChannelEvent;
	DWORD BytesReturned = 0;
	ainput_server* ainput = (ainput_server*)arg;
	UINT error;
	DWORD status;

	WINPR_ASSERT(ainput);

	if ((error = ainput_server_open_channel(ainput)))
	{
		UINT error2 = 0;
		WLog_ERR(TAG, "ainput_server_open_channel failed with error %" PRIu32 "!", error);
		IFCALLRET(ainput->context.OpenResult, error2, &ainput->context,
		          AINPUT_SERVER_OPEN_RESULT_NOTSUPPORTED);

		if (error2)
			WLog_ERR(TAG, "ainput server's OpenResult callback failed with error %" PRIu32 "",
			         error2);

		goto out;
	}

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;

	if (WTSVirtualChannelQuery(ainput->ainput_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = ainput->stopEvent;
	events[nCount++] = ChannelEvent;

	/* Wait for the client to confirm that the Graphics Pipeline dynamic channel is ready */

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, 100);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
		{
			IFCALLRET(ainput->context.OpenResult, error, &ainput->context,
			          AINPUT_SERVER_OPEN_RESULT_CLOSED);

			if (error)
				WLog_ERR(TAG, "OpenResult failed with error %" PRIu32 "!", error);

			break;
		}

		if (WTSVirtualChannelQuery(ainput->ainput_channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			IFCALLRET(ainput->context.OpenResult, error, &ainput->context,
			          AINPUT_SERVER_OPEN_RESULT_ERROR);

			if (error)
				WLog_ERR(TAG, "OpenResult failed with error %" PRIu32 "!", error);

			break;
		}

		ready = *((BOOL*)buffer);
		WTSFreeMemory(buffer);

		if (ready)
		{
			IFCALLRET(ainput->context.OpenResult, error, &ainput->context,
			          AINPUT_SERVER_OPEN_RESULT_OK);

			if (error)
				WLog_ERR(TAG, "OpenResult failed with error %" PRIu32 "!", error);

			break;
		}
	}

	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		WTSVirtualChannelClose(ainput->ainput_channel);
		ExitThread(ERROR_NOT_ENOUGH_MEMORY);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	while (ready)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
			break;

		Stream_SetPosition(s, 0);
		WTSVirtualChannelRead(ainput->ainput_channel, 0, NULL, 0, &BytesReturned);

		if (BytesReturned < 1)
			continue;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			error = CHANNEL_RC_NO_MEMORY;
			break;
		}

		if (WTSVirtualChannelRead(ainput->ainput_channel, 0, (PCHAR)Stream_Buffer(s),
		                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		IFCALLRET(ainput->context.Receive, error, &ainput->context, (BYTE*)Stream_Buffer(s),
		          BytesReturned);

		if (error)
		{
			WLog_ERR(TAG, "Response failed with error %" PRIu32 "!", error);
			break;
		}
	}

	Stream_Free(s, TRUE);
	WTSVirtualChannelClose(ainput->ainput_channel);
	ainput->ainput_channel = NULL;
out:

	if (error && ainput->context.rdpcontext)
		setChannelError(ainput->context.rdpcontext, error,
		                "ainput_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ainput_server_open(ainput_server_context* context)
{
	ainput_server* ainput = (ainput_server*)context;

	WINPR_ASSERT(ainput);

	if (ainput->thread == NULL)
	{
		ainput->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!ainput->stopEvent)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return ERROR_INTERNAL_ERROR;
		}

		ainput->thread = CreateThread(NULL, 0, ainput_server_thread_func, ainput, 0, NULL);
		if (!ainput->thread)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			CloseHandle(ainput->stopEvent);
			ainput->stopEvent = NULL;
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
static UINT ainput_server_close(ainput_server_context* context)
{
	UINT error = CHANNEL_RC_OK;
	ainput_server* ainput = (ainput_server*)context;

	WINPR_ASSERT(ainput);

	if (ainput->thread)
	{
		SetEvent(ainput->stopEvent);

		if (WaitForSingleObject(ainput->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		CloseHandle(ainput->thread);
		CloseHandle(ainput->stopEvent);
		ainput->thread = NULL;
		ainput->stopEvent = NULL;
	}

	return error;
}

ainput_server_context* ainput_server_context_new(HANDLE vcm)
{
	ainput_server* ainput = (ainput_server*)calloc(1, sizeof(ainput_server));

	if (!ainput)
		return NULL;

	ainput->context.vcm = vcm;
	ainput->context.Open = ainput_server_open;
	ainput->context.Close = ainput_server_close;

	return &ainput->context;
}

void ainput_server_context_free(ainput_server_context* context)
{
	ainput_server* ainput = (ainput_server*)context;
	ainput_server_close(context);
	free(ainput);
}
