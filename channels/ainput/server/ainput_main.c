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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <winpr/stream.h>

#include <freerdp/server/ainput.h>
#include <freerdp/channels/ainput.h>
#include <freerdp/channels/log.h>

#include "../common/ainput_common.h"

#define TAG CHANNELS_TAG("ainput.server")

typedef enum
{
	AINPUT_INITIAL,
	AINPUT_OPENED,
	AINPUT_VERSION_SENT,
} eAInputChannelState;

typedef struct
{
	ainput_server_context context;

	BOOL opened;

	HANDLE stopEvent;

	HANDLE thread;
	void* ainput_channel;

	DWORD SessionId;

	BOOL isOpened;
	BOOL externalThread;

	/* Channel state */
	eAInputChannelState state;

	wStream* buffer;
} ainput_server;

static UINT ainput_server_context_poll(ainput_server_context* context);
static BOOL ainput_server_context_handle(ainput_server_context* context, HANDLE* handle);
static UINT ainput_server_context_poll_int(ainput_server_context* context);

static BOOL ainput_server_is_open(ainput_server_context* context)
{
	ainput_server* ainput = (ainput_server*)context;

	WINPR_ASSERT(ainput);
	return ainput->isOpened;
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

		Error = GetLastError();

		if (Error == ERROR_NOT_FOUND)
		{
			WLog_DBG(TAG, "Channel %s not found", AINPUT_DVC_CHANNEL_NAME);
			break;
		}

		if (ainput->ainput_channel)
		{
			UINT32 channelId;
			BOOL status = TRUE;

			channelId = WTSChannelGetIdByHandle(ainput->ainput_channel);

			IFCALLRET(ainput->context.ChannelIdAssigned, status, &ainput->context, channelId);
			if (!status)
			{
				WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
				return ERROR_INTERNAL_ERROR;
			}

			break;
		}

		if (GetTickCount() - StartTick > 5000)
		{
			WLog_WARN(TAG, "Timeout opening channel %s", AINPUT_DVC_CHANNEL_NAME);
			break;
		}
	}

	return ainput->ainput_channel ? CHANNEL_RC_OK : ERROR_INTERNAL_ERROR;
}

static UINT ainput_server_send_version(ainput_server* ainput)
{
	ULONG written;
	wStream* s;

	WINPR_ASSERT(ainput);

	s = ainput->buffer;
	WINPR_ASSERT(s);

	Stream_SetPosition(s, 0);
	if (!Stream_EnsureCapacity(s, 10))
	{
		WLog_WARN(TAG, "[%s] out of memory", AINPUT_DVC_CHANNEL_NAME);
		return ERROR_OUTOFMEMORY;
	}

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
	UINT64 flags, time;
	INT32 x, y;
	char buffer[128] = { 0 };

	WINPR_ASSERT(ainput);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
		return ERROR_NO_DATA;

	Stream_Read_UINT64(s, time);
	Stream_Read_UINT64(s, flags);
	Stream_Read_INT32(s, x);
	Stream_Read_INT32(s, y);

	WLog_VRB(TAG, "[%s] received: time=0x%08" PRIx64 ", flags=%s, %" PRId32 "x%" PRId32,
	         __FUNCTION__, time, ainput_flags_to_string(flags, buffer, sizeof(buffer)), x, y);
	IFCALLRET(ainput->context.MouseEvent, error, &ainput->context, time, flags, x, y);

	return error;
}

static HANDLE ainput_server_get_channel_handle(ainput_server* ainput)
{
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	HANDLE ChannelEvent = NULL;

	WINPR_ASSERT(ainput);

	if (WTSVirtualChannelQuery(ainput->ainput_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	return ChannelEvent;
}

static DWORD WINAPI ainput_server_thread_func(LPVOID arg)
{
	DWORD nCount;
	HANDLE events[2] = { 0 };
	ainput_server* ainput = (ainput_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status;

	WINPR_ASSERT(ainput);

	nCount = 0;
	events[nCount++] = ainput->stopEvent;

	while ((error == CHANNEL_RC_OK) && (WaitForSingleObject(events[0], 0) != WAIT_OBJECT_0))
	{
		switch (ainput->state)
		{
			case AINPUT_OPENED:
				events[1] = ainput_server_get_channel_handle(ainput);
				nCount = 2;
				status = WaitForMultipleObjects(nCount, events, FALSE, 100);
				switch (status)
				{
					case WAIT_TIMEOUT:
					case WAIT_OBJECT_0 + 1:
					case WAIT_OBJECT_0:
						error = ainput_server_context_poll_int(&ainput->context);
						break;
					case WAIT_FAILED:
					default:
						WLog_WARN(TAG, "[%s] Wait for open failed", AINPUT_DVC_CHANNEL_NAME);
						error = ERROR_INTERNAL_ERROR;
						break;
				}
				break;
			case AINPUT_VERSION_SENT:
				status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);
				switch (status)
				{
					case WAIT_TIMEOUT:
					case WAIT_OBJECT_0 + 1:
					case WAIT_OBJECT_0:
						error = ainput_server_context_poll_int(&ainput->context);
						break;

					case WAIT_FAILED:
					default:
						WLog_WARN(TAG, "[%s] Wait for version failed", AINPUT_DVC_CHANNEL_NAME);
						error = ERROR_INTERNAL_ERROR;
						break;
				}
				break;
			default:
				error = ainput_server_context_poll_int(&ainput->context);
				break;
		}
	}

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

	if (!ainput->externalThread && (ainput->thread == NULL))
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
	ainput->isOpened = TRUE;

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

	if (!ainput->externalThread && ainput->thread)
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
	if (ainput->externalThread)
	{
		if (ainput->state != AINPUT_INITIAL)
		{
			WTSVirtualChannelClose(ainput->ainput_channel);
			ainput->ainput_channel = NULL;
			ainput->state = AINPUT_INITIAL;
		}
	}
	ainput->isOpened = FALSE;

	return error;
}

static UINT ainput_server_initialize(ainput_server_context* context, BOOL externalThread)
{
	UINT error = CHANNEL_RC_OK;
	ainput_server* ainput = (ainput_server*)context;

	WINPR_ASSERT(ainput);

	if (ainput->isOpened)
	{
		WLog_WARN(TAG, "Application error: AINPUT channel already initialized, calling in this "
		               "state is not possible!");
		return ERROR_INVALID_STATE;
	}
	ainput->externalThread = externalThread;
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
	ainput->context.Initialize = ainput_server_initialize;
	ainput->context.Poll = ainput_server_context_poll;
	ainput->context.ChannelHandle = ainput_server_context_handle;

	ainput->buffer = Stream_New(NULL, 4096);
	if (!ainput->buffer)
		goto fail;
	return &ainput->context;
fail:
	ainput_server_context_free(&ainput->context);
	return NULL;
}

void ainput_server_context_free(ainput_server_context* context)
{
	ainput_server* ainput = (ainput_server*)context;
	if (ainput)
	{
		ainput_server_close(context);
		Stream_Free(ainput->buffer, TRUE);
	}
	free(ainput);
}

static UINT ainput_process_message(ainput_server* ainput)
{
	BOOL rc;
	UINT error = ERROR_INTERNAL_ERROR;
	ULONG BytesReturned, ActualBytesReturned;
	UINT16 MessageId;
	wStream* s;

	WINPR_ASSERT(ainput);
	WINPR_ASSERT(ainput->ainput_channel);

	s = ainput->buffer;
	WINPR_ASSERT(s);

	Stream_SetPosition(s, 0);
	rc = WTSVirtualChannelRead(ainput->ainput_channel, 0, NULL, 0, &BytesReturned);
	if (!rc)
		goto out;

	if (BytesReturned < 2)
	{
		error = CHANNEL_RC_OK;
		goto out;
	}

	if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
	{
		WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	if (WTSVirtualChannelRead(ainput->ainput_channel, 0, (PCHAR)Stream_Buffer(s),
	                          (ULONG)Stream_Capacity(s), &ActualBytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
		goto out;
	}

	if (BytesReturned != ActualBytesReturned)
	{
		WLog_ERR(TAG, "WTSVirtualChannelRead size mismatch %" PRId32 ", expected %" PRId32,
		         ActualBytesReturned, BytesReturned);
		goto out;
	}

	Stream_SetLength(s, ActualBytesReturned);
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

out:
	if (error)
		WLog_ERR(TAG, "Response failed with error %" PRIu32 "!", error);

	return error;
}

BOOL ainput_server_context_handle(ainput_server_context* context, HANDLE* handle)
{
	ainput_server* ainput = (ainput_server*)context;
	WINPR_ASSERT(ainput);
	WINPR_ASSERT(handle);

	if (!ainput->externalThread)
	{
		WLog_WARN(TAG, "[%s] externalThread fail!", AINPUT_DVC_CHANNEL_NAME);
		return FALSE;
	}
	if (ainput->state == AINPUT_INITIAL)
	{
		WLog_WARN(TAG, "[%s] state fail!", AINPUT_DVC_CHANNEL_NAME);
		return FALSE;
	}
	*handle = ainput_server_get_channel_handle(ainput);
	return TRUE;
}

UINT ainput_server_context_poll_int(ainput_server_context* context)
{
	ainput_server* ainput = (ainput_server*)context;
	UINT error = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(ainput);

	switch (ainput->state)
	{
		case AINPUT_INITIAL:
			error = ainput_server_open_channel(ainput);
			if (error)
				WLog_ERR(TAG, "ainput_server_open_channel failed with error %" PRIu32 "!", error);
			else
				ainput->state = AINPUT_OPENED;
			break;
		case AINPUT_OPENED:
		{
			union
			{
				BYTE* pb;
				void* pv;
			} buffer;
			DWORD BytesReturned = 0;

			buffer.pv = NULL;

			if (WTSVirtualChannelQuery(ainput->ainput_channel, WTSVirtualChannelReady, &buffer.pv,
			                           &BytesReturned) != TRUE)
			{
				WLog_ERR(TAG, "WTSVirtualChannelReady failed,");
			}
			else
			{
				if (*buffer.pb != 0)
				{
					error = ainput_server_send_version(ainput);
					if (error)
						WLog_ERR(TAG, "audin_server_send_version failed with error %" PRIu32 "!",
						         error);
					else
						ainput->state = AINPUT_VERSION_SENT;
				}
				else
					error = CHANNEL_RC_OK;
			}
			WTSFreeMemory(buffer.pv);
		}
		break;
		case AINPUT_VERSION_SENT:
			error = ainput_process_message(ainput);
			break;

		default:
			WLog_ERR(TAG, "AINPUT chanel is in invalid state %d", ainput->state);
			break;
	}

	return error;
}

UINT ainput_server_context_poll(ainput_server_context* context)
{
	ainput_server* ainput = (ainput_server*)context;

	WINPR_ASSERT(ainput);
	if (!ainput->externalThread)
	{
		WLog_WARN(TAG, "[%s] externalThread fail!", AINPUT_DVC_CHANNEL_NAME);
		return ERROR_INTERNAL_ERROR;
	}
	return ainput_server_context_poll_int(context);
}
