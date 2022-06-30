/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Telemetry Virtual Channel Extension
 *
 * Copyright 2022 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#include <freerdp/channels/log.h>
#include <freerdp/server/telemetry.h>

#define TAG CHANNELS_TAG("telemetry.server")

typedef enum
{
	TELEMETRY_INITIAL,
	TELEMETRY_OPENED,
} eTelemetryChannelState;

typedef struct
{
	TelemetryServerContext context;

	HANDLE stopEvent;

	HANDLE thread;
	void* telemetry_channel;

	DWORD SessionId;

	BOOL isOpened;
	BOOL externalThread;

	/* Channel state */
	eTelemetryChannelState state;

	wStream* buffer;
} telemetry_server;

static UINT telemetry_server_initialize(TelemetryServerContext* context, BOOL externalThread)
{
	UINT error = CHANNEL_RC_OK;
	telemetry_server* telemetry = (telemetry_server*)context;

	WINPR_ASSERT(telemetry);

	if (telemetry->isOpened)
	{
		WLog_WARN(TAG, "Application error: TELEMETRY channel already initialized, "
		               "calling in this state is not possible!");
		return ERROR_INVALID_STATE;
	}

	telemetry->externalThread = externalThread;

	return error;
}

static UINT telemetry_server_open_channel(telemetry_server* telemetry)
{
	TelemetryServerContext* context = &telemetry->context;
	DWORD Error = ERROR_SUCCESS;
	HANDLE hEvent;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	UINT32 channelId;
	BOOL status = TRUE;

	WINPR_ASSERT(telemetry);

	if (WTSQuerySessionInformationA(telemetry->context.vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		return ERROR_INTERNAL_ERROR;
	}

	telemetry->SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);
	hEvent = WTSVirtualChannelManagerGetEventHandle(telemetry->context.vcm);

	if (WaitForSingleObject(hEvent, 1000) == WAIT_FAILED)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", Error);
		return Error;
	}

	telemetry->telemetry_channel = WTSVirtualChannelOpenEx(
	    telemetry->SessionId, TELEMETRY_DVC_CHANNEL_NAME, WTS_CHANNEL_OPTION_DYNAMIC);
	if (!telemetry->telemetry_channel)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed with error %" PRIu32 "!", Error);
		return Error;
	}

	channelId = WTSChannelGetIdByHandle(telemetry->telemetry_channel);

	IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
	if (!status)
	{
		WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return Error;
}

static UINT telemetry_server_recv_rdp_telemetry_pdu(TelemetryServerContext* context, wStream* s)
{
	TELEMETRY_RDP_TELEMETRY_PDU pdu;
	UINT error = CHANNEL_RC_OK;

	if (Stream_GetRemainingLength(s) < 16)
	{
		WLog_ERR(TAG, "telemetry_server_recv_rdp_telemetry_pdu: Not enough data!");
		return ERROR_NO_DATA;
	}

	Stream_Read_UINT32(s, pdu.PromptForCredentialsMillis);
	Stream_Read_UINT32(s, pdu.PromptForCredentialsDoneMillis);
	Stream_Read_UINT32(s, pdu.GraphicsChannelOpenedMillis);
	Stream_Read_UINT32(s, pdu.FirstGraphicsReceivedMillis);

	IFCALLRET(context->RdpTelemetry, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->RdpTelemetry failed with error %" PRIu32 "", error);

	return error;
}

static UINT telemetry_process_message(telemetry_server* telemetry)
{
	BOOL rc;
	UINT error = ERROR_INTERNAL_ERROR;
	ULONG BytesReturned;
	BYTE MessageId;
	BYTE Length;
	wStream* s;

	WINPR_ASSERT(telemetry);
	WINPR_ASSERT(telemetry->telemetry_channel);

	s = telemetry->buffer;
	WINPR_ASSERT(s);

	Stream_SetPosition(s, 0);
	rc = WTSVirtualChannelRead(telemetry->telemetry_channel, 0, NULL, 0, &BytesReturned);
	if (!rc)
		goto out;

	if (BytesReturned < 1)
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

	if (WTSVirtualChannelRead(telemetry->telemetry_channel, 0, (PCHAR)Stream_Buffer(s),
	                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
		goto out;
	}

	Stream_SetLength(s, BytesReturned);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(s, MessageId);
	Stream_Read_UINT8(s, Length);

	switch (MessageId)
	{
		case 0x01:
			error = telemetry_server_recv_rdp_telemetry_pdu(&telemetry->context, s);
			break;
		default:
			WLog_ERR(TAG, "telemetry_process_message: unknown MessageId %" PRIu8 "", MessageId);
			break;
	}

out:
	if (error)
		WLog_ERR(TAG, "Response failed with error %" PRIu32 "!", error);

	return error;
}

static UINT telemetry_server_context_poll_int(TelemetryServerContext* context)
{
	telemetry_server* telemetry = (telemetry_server*)context;
	UINT error = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(telemetry);

	switch (telemetry->state)
	{
		case TELEMETRY_INITIAL:
			error = telemetry_server_open_channel(telemetry);
			if (error)
				WLog_ERR(TAG, "telemetry_server_open_channel failed with error %" PRIu32 "!",
				         error);
			else
				telemetry->state = TELEMETRY_OPENED;
			break;
		case TELEMETRY_OPENED:
			error = telemetry_process_message(telemetry);
			break;
	}

	return error;
}

static HANDLE telemetry_server_get_channel_handle(telemetry_server* telemetry)
{
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	HANDLE ChannelEvent = NULL;

	WINPR_ASSERT(telemetry);

	if (WTSVirtualChannelQuery(telemetry->telemetry_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	return ChannelEvent;
}

static DWORD WINAPI telemetry_server_thread_func(LPVOID arg)
{
	DWORD nCount;
	HANDLE events[2] = { 0 };
	telemetry_server* telemetry = (telemetry_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status;

	WINPR_ASSERT(telemetry);

	nCount = 0;
	events[nCount++] = telemetry->stopEvent;

	while ((error == CHANNEL_RC_OK) && (WaitForSingleObject(events[0], 0) != WAIT_OBJECT_0))
	{
		switch (telemetry->state)
		{
			case TELEMETRY_INITIAL:
				error = telemetry_server_context_poll_int(&telemetry->context);
				if (error == CHANNEL_RC_OK)
				{
					events[1] = telemetry_server_get_channel_handle(telemetry);
					nCount = 2;
				}
				break;
			case TELEMETRY_OPENED:
				status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);
				switch (status)
				{
					case WAIT_OBJECT_0:
						break;
					case WAIT_OBJECT_0 + 1:
					case WAIT_TIMEOUT:
						error = telemetry_server_context_poll_int(&telemetry->context);
						break;

					case WAIT_FAILED:
					default:
						error = ERROR_INTERNAL_ERROR;
						break;
				}
				break;
		}
	}

	WTSVirtualChannelClose(telemetry->telemetry_channel);
	telemetry->telemetry_channel = NULL;

	if (error && telemetry->context.rdpcontext)
		setChannelError(telemetry->context.rdpcontext, error,
		                "telemetry_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

static UINT telemetry_server_open(TelemetryServerContext* context)
{
	telemetry_server* telemetry = (telemetry_server*)context;

	WINPR_ASSERT(telemetry);

	if (!telemetry->externalThread && (telemetry->thread == NULL))
	{
		telemetry->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!telemetry->stopEvent)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return ERROR_INTERNAL_ERROR;
		}

		telemetry->thread = CreateThread(NULL, 0, telemetry_server_thread_func, telemetry, 0, NULL);
		if (!telemetry->thread)
		{
			WLog_ERR(TAG, "CreateThread failed!");
			CloseHandle(telemetry->stopEvent);
			telemetry->stopEvent = NULL;
			return ERROR_INTERNAL_ERROR;
		}
	}
	telemetry->isOpened = TRUE;

	return CHANNEL_RC_OK;
}

static UINT telemetry_server_close(TelemetryServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	telemetry_server* telemetry = (telemetry_server*)context;

	WINPR_ASSERT(telemetry);

	if (!telemetry->externalThread && telemetry->thread)
	{
		SetEvent(telemetry->stopEvent);

		if (WaitForSingleObject(telemetry->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		CloseHandle(telemetry->thread);
		CloseHandle(telemetry->stopEvent);
		telemetry->thread = NULL;
		telemetry->stopEvent = NULL;
	}
	if (telemetry->externalThread)
	{
		if (telemetry->state != TELEMETRY_INITIAL)
		{
			WTSVirtualChannelClose(telemetry->telemetry_channel);
			telemetry->telemetry_channel = NULL;
			telemetry->state = TELEMETRY_INITIAL;
		}
	}
	telemetry->isOpened = FALSE;

	return error;
}

static UINT telemetry_server_context_poll(TelemetryServerContext* context)
{
	telemetry_server* telemetry = (telemetry_server*)context;

	WINPR_ASSERT(telemetry);

	if (!telemetry->externalThread)
		return ERROR_INTERNAL_ERROR;

	return telemetry_server_context_poll_int(context);
}

static BOOL telemetry_server_context_handle(TelemetryServerContext* context, HANDLE* handle)
{
	telemetry_server* telemetry = (telemetry_server*)context;

	WINPR_ASSERT(telemetry);
	WINPR_ASSERT(handle);

	if (!telemetry->externalThread)
		return FALSE;
	if (telemetry->state == TELEMETRY_INITIAL)
		return FALSE;

	*handle = telemetry_server_get_channel_handle(telemetry);

	return TRUE;
}

TelemetryServerContext* telemetry_server_context_new(HANDLE vcm)
{
	telemetry_server* telemetry = (telemetry_server*)calloc(1, sizeof(telemetry_server));

	if (!telemetry)
		return NULL;

	telemetry->context.vcm = vcm;
	telemetry->context.Initialize = telemetry_server_initialize;
	telemetry->context.Open = telemetry_server_open;
	telemetry->context.Close = telemetry_server_close;
	telemetry->context.Poll = telemetry_server_context_poll;
	telemetry->context.ChannelHandle = telemetry_server_context_handle;

	telemetry->buffer = Stream_New(NULL, 4096);
	if (!telemetry->buffer)
		goto fail;

	return &telemetry->context;
fail:
	telemetry_server_context_free(&telemetry->context);
	return NULL;
}

void telemetry_server_context_free(TelemetryServerContext* context)
{
	telemetry_server* telemetry = (telemetry_server*)context;

	if (telemetry)
	{
		telemetry_server_close(context);
		Stream_Free(telemetry->buffer, TRUE);
	}

	free(telemetry);
}
