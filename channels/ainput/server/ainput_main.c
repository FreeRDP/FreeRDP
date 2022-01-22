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

#include "../common/ainput_common.h"

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

static BOOL ainput_server_is_open(ainput_server_context* context)
{
	ainput_server* ainput = (ainput_server*)context;

	WINPR_ASSERT(ainput);
	return ainput->thread != NULL;
}

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

static UINT ainput_server_send_version(ainput_server* ainput, wStream* s)
{
	ULONG written;
	WINPR_ASSERT(ainput);
	WINPR_ASSERT(s);

	Stream_SetPosition(s, 0);
	if (!Stream_EnsureCapacity(s, 10))
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT16(s, MSG_AINPUT_VERSION);
	Stream_Write_UINT32(s, AINPUT_VERSION_MAJOR); /* Version (4 bytes) */
	Stream_Write_UINT32(s, AINPUT_VERSION_MINOR); /* Version (4 bytes) */

	WINPR_ASSERT(Stream_GetPosition(s) <= ULONG_MAX);
	if (!WTSVirtualChannelWrite(ainput->ainput_channel, (PCHAR)Stream_Buffer(s),
	                            (ULONG)Stream_GetPosition(s), &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

static UINT ainput_server_recv_mouse_event(ainput_server* ainput, wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	UINT64 flags;
	INT32 x, y;
	char buffer[128] = { 0 };

	WINPR_ASSERT(ainput);
	WINPR_ASSERT(s);

	if (Stream_GetRemainingLength(s) < 16)
		return ERROR_NO_DATA;

	Stream_Read_UINT64(s, flags);
	Stream_Read_INT32(s, x);
	Stream_Read_INT32(s, y);

	WLog_VRB(TAG, "[%s] received: flags=%s, %" PRId32 "x%" PRId32, __FUNCTION__,
	         ainput_flags_to_string(flags, buffer, sizeof(buffer)), x, y);
	IFCALLRET(ainput->context.MouseEvent, error, &ainput->context, flags, x, y);

	return error;
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
		WLog_ERR(TAG, "ainput_server_open_channel failed with error %" PRIu32 "!", error);
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
			if (error)
				WLog_ERR(TAG, "OpenResult failed with error %" PRIu32 "!", error);

			break;
		}

		if (WTSVirtualChannelQuery(ainput->ainput_channel, WTSVirtualChannelReady, &buffer,
		                           &BytesReturned) == FALSE)
		{
			if (error)
				WLog_ERR(TAG, "OpenResult failed with error %" PRIu32 "!", error);

			break;
		}

		ready = *((BOOL*)buffer);
		WTSFreeMemory(buffer);

		if (ready)
		{
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

	if (ready)
	{
		if ((error = ainput_server_send_version(ainput, s)))
		{
			WLog_ERR(TAG, "audin_server_send_version failed with error %" PRIu32 "!", error);
			goto out_capacity;
		}
	}

	while (ready)
	{
		UINT16 MessageId;
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

		if (BytesReturned < 2)
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

		Stream_SetLength(s, BytesReturned);
		Stream_Read_UINT16(s, MessageId);

		switch (MessageId)
		{
			case MSG_AINPUT_MOUSE:
				error = ainput_server_recv_mouse_event(ainput, s);
				break;

			default:
				WLog_ERR(TAG, "audin_server_thread_func: unknown MessageId %" PRIu8 "", MessageId);
				break;
		}

		if (error)
		{
			WLog_ERR(TAG, "Response failed with error %" PRIu32 "!", error);
			break;
		}
	}

out_capacity:
	Stream_Free(s, TRUE);

out:
	WTSVirtualChannelClose(ainput->ainput_channel);
	ainput->ainput_channel = NULL;

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
	ainput->context.IsOpen = ainput_server_is_open;
	ainput->context.Close = ainput_server_close;

	return &ainput->context;
}

void ainput_server_context_free(ainput_server_context* context)
{
	ainput_server* ainput = (ainput_server*)context;
	if (ainput)
		ainput_server_close(context);
	free(ainput);
}
