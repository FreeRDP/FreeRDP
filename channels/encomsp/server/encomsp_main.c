/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/stream.h>

#include <freerdp/channels/log.h>

#include "encomsp_main.h"

#define TAG CHANNELS_TAG("encomsp.server")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_read_header(wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	if (Stream_GetRemainingLength(s) < ENCOMSP_ORDER_HEADER_SIZE)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, header->Type); /* Type (2 bytes) */
	Stream_Read_UINT16(s, header->Length); /* Length (2 bytes) */
	return CHANNEL_RC_OK;
}

#if 0

static int encomsp_write_header(wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	Stream_Write_UINT16(s, header->Type); /* Type (2 bytes) */
	Stream_Write_UINT16(s, header->Length); /* Length (2 bytes) */
	return 1;
}

static int encomsp_read_unicode_string(wStream* s, ENCOMSP_UNICODE_STRING* str)
{
	ZeroMemory(str, sizeof(ENCOMSP_UNICODE_STRING));

	if (Stream_GetRemainingLength(s) < 2)
		return -1;

	Stream_Read_UINT16(s, str->cchString); /* cchString (2 bytes) */

	if (str->cchString > 1024)
		return -1;

	if (Stream_GetRemainingLength(s) < (str->cchString * 2))
		return -1;

	Stream_Read(s, &(str->wString), (str->cchString * 2)); /* String (variable) */
	return 1;
}

#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_recv_change_participant_control_level_pdu(
    EncomspServerContext* context, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU pdu;
	UINT error = CHANNEL_RC_OK;
	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;
	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 6)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */
	Stream_Read_UINT32(s, pdu.ParticipantId); /* ParticipantId (4 bytes) */
	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
	{
		WLog_ERR(TAG, "Not enough data!");
		return ERROR_INVALID_DATA;
	}

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < ((beg + header->Length) - end))
		{
			WLog_ERR(TAG, "Not enough data!");
			return ERROR_INVALID_DATA;
		}

		Stream_SetPosition(s, (beg + header->Length));
	}

	IFCALLRET(context->ChangeParticipantControlLevel, error, context, &pdu);

	if (error)
		WLog_ERR(TAG, "context->ChangeParticipantControlLevel failed with error %"PRIu32"",
		         error);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_server_receive_pdu(EncomspServerContext* context,
                                       wStream* s)
{
	UINT error = CHANNEL_RC_OK;
	ENCOMSP_ORDER_HEADER header;

	while (Stream_GetRemainingLength(s) > 0)
	{
		if ((error = encomsp_read_header(s, &header)))
		{
			WLog_ERR(TAG, "encomsp_read_header failed with error %"PRIu32"!", error);
			return error;
		}

		WLog_INFO(TAG, "EncomspReceive: Type: %"PRIu16" Length: %"PRIu16"", header.Type,
		          header.Length);

		switch (header.Type)
		{
			case ODTYPE_PARTICIPANT_CTRL_CHANGED:
				if ((error = encomsp_recv_change_participant_control_level_pdu(context, s,
				             &header)))
				{
					WLog_ERR(TAG,
					         "encomsp_recv_change_participant_control_level_pdu failed with error %"PRIu32"!",
					         error);
					return error;
				}

				break;

			default:
				WLog_ERR(TAG, "header.Type unknown %"PRIu16"!", header.Type);
				return ERROR_INVALID_DATA;
				break;
		}
	}

	return error;
}

static DWORD WINAPI encomsp_server_thread(LPVOID arg)
{
	wStream* s;
	DWORD nCount;
	void* buffer;
	HANDLE events[8];
	HANDLE ChannelEvent;
	DWORD BytesReturned;
	ENCOMSP_ORDER_HEADER* header;
	EncomspServerContext* context;
	UINT error = CHANNEL_RC_OK;
	DWORD status;
	context = (EncomspServerContext*) arg;

	buffer = NULL;
	BytesReturned = 0;
	ChannelEvent = NULL;
	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto out;
	}

	if (WTSVirtualChannelQuery(context->priv->ChannelHandle, WTSVirtualEventHandle,
	                           &buffer, &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	nCount = 0;
	events[nCount++] = ChannelEvent;
	events[nCount++] = context->priv->StopEvent;

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %"PRIu32"", error);
			break;
		}

		status = WaitForSingleObject(context->priv->StopEvent, 0);

		if (status == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
			break;
		}

		if (status == WAIT_OBJECT_0)
		{
			break;
		}

		WTSVirtualChannelRead(context->priv->ChannelHandle, 0, NULL, 0, &BytesReturned);

		if (BytesReturned < 1)
			continue;

		if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			error = CHANNEL_RC_NO_MEMORY;
			break;
		}

		if (!WTSVirtualChannelRead(context->priv->ChannelHandle, 0,
		                           (PCHAR) Stream_Buffer(s), Stream_Capacity(s), &BytesReturned))
		{
			WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (Stream_GetPosition(s) >= ENCOMSP_ORDER_HEADER_SIZE)
		{
			header = (ENCOMSP_ORDER_HEADER*) Stream_Buffer(s);

			if (header->Length >= Stream_GetPosition(s))
			{
				Stream_SealLength(s);
				Stream_SetPosition(s, 0);

				if ((error = encomsp_server_receive_pdu(context, s)))
				{
					WLog_ERR(TAG, "encomsp_server_receive_pdu failed with error %"PRIu32"!", error);
					break;
				}

				Stream_SetPosition(s, 0);
			}
		}
	}

	Stream_Free(s, TRUE);
out:

	if (error && context->rdpcontext)
		setChannelError(context->rdpcontext, error,
		                "encomsp_server_thread reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_server_start(EncomspServerContext* context)
{
	context->priv->ChannelHandle = WTSVirtualChannelOpen(context->vcm,
	                               WTS_CURRENT_SESSION, "encomsp");

	if (!context->priv->ChannelHandle)
		return CHANNEL_RC_BAD_CHANNEL;

	if (!(context->priv->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		return ERROR_INTERNAL_ERROR;
	}

	if (!(context->priv->Thread = CreateThread(NULL, 0,
								  encomsp_server_thread, (void*) context, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		CloseHandle(context->priv->StopEvent);
		context->priv->StopEvent = NULL;
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT encomsp_server_stop(EncomspServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	SetEvent(context->priv->StopEvent);

	if (WaitForSingleObject(context->priv->Thread, INFINITE) == WAIT_FAILED)
	{
		error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %"PRIu32"", error);
		return error;
	}

	CloseHandle(context->priv->Thread);
	CloseHandle(context->priv->StopEvent);
	return error;
}

EncomspServerContext* encomsp_server_context_new(HANDLE vcm)
{
	EncomspServerContext* context;
	context = (EncomspServerContext*) calloc(1, sizeof(EncomspServerContext));

	if (context)
	{
		context->vcm = vcm;
		context->Start = encomsp_server_start;
		context->Stop = encomsp_server_stop;
		context->priv = (EncomspServerPrivate*) calloc(1, sizeof(EncomspServerPrivate));

		if (!context->priv)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(context);
			return NULL;
		}
	}

	return context;
}

void encomsp_server_context_free(EncomspServerContext* context)
{
	if (context)
	{
		if (context->priv->ChannelHandle != INVALID_HANDLE_VALUE)
			WTSVirtualChannelClose(context->priv->ChannelHandle);
		
		free(context->priv);
		free(context);
	}
}
