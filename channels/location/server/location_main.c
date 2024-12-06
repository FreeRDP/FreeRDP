/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Location Virtual Channel Extension
 *
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#include <freerdp/freerdp.h>
#include <freerdp/channels/log.h>
#include <freerdp/server/location.h>
#include <freerdp/utils/encoded_types.h>

#define TAG CHANNELS_TAG("location.server")

typedef enum
{
	LOCATION_INITIAL,
	LOCATION_OPENED,
} eLocationChannelState;

typedef struct
{
	LocationServerContext context;

	HANDLE stopEvent;

	HANDLE thread;
	void* location_channel;

	DWORD SessionId;

	BOOL isOpened;
	BOOL externalThread;

	/* Channel state */
	eLocationChannelState state;

	wStream* buffer;
} location_server;

static UINT location_server_initialize(LocationServerContext* context, BOOL externalThread)
{
	UINT error = CHANNEL_RC_OK;
	location_server* location = (location_server*)context;

	WINPR_ASSERT(location);

	if (location->isOpened)
	{
		WLog_WARN(TAG, "Application error: Location channel already initialized, "
		               "calling in this state is not possible!");
		return ERROR_INVALID_STATE;
	}

	location->externalThread = externalThread;

	return error;
}

static UINT location_server_open_channel(location_server* location)
{
	LocationServerContext* context = &location->context;
	DWORD Error = ERROR_SUCCESS;
	HANDLE hEvent = NULL;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	UINT32 channelId = 0;
	BOOL status = TRUE;

	WINPR_ASSERT(location);

	if (WTSQuerySessionInformationA(location->context.vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		return ERROR_INTERNAL_ERROR;
	}

	location->SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);
	hEvent = WTSVirtualChannelManagerGetEventHandle(location->context.vcm);

	if (WaitForSingleObject(hEvent, 1000) == WAIT_FAILED)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", Error);
		return Error;
	}

	location->location_channel = WTSVirtualChannelOpenEx(
	    location->SessionId, LOCATION_DVC_CHANNEL_NAME, WTS_CHANNEL_OPTION_DYNAMIC);
	if (!location->location_channel)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed with error %" PRIu32 "!", Error);
		return Error;
	}

	channelId = WTSChannelGetIdByHandle(location->location_channel);

	IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
	if (!status)
	{
		WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return Error;
}

static UINT location_server_recv_client_ready(LocationServerContext* context, wStream* s,
                                              const RDPLOCATION_HEADER* header)
{
	RDPLOCATION_CLIENT_READY_PDU pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	pdu.header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.protocolVersion);

	if (Stream_GetRemainingLength(s) >= 4)
		Stream_Read_UINT32(s, pdu.flags);

	IFCALLRET(context->ClientReady, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->ClientReady failed with error %" PRIu32 "", error);

	return error;
}

static UINT location_server_recv_base_location3d(LocationServerContext* context, wStream* s,
                                                 const RDPLOCATION_HEADER* header)
{
	RDPLOCATION_BASE_LOCATION3D_PDU pdu = { 0 };
	UINT error = CHANNEL_RC_OK;
	double speed = 0.0;
	double heading = 0.0;
	double horizontalAccuracy = 0.0;
	LOCATIONSOURCE source = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	pdu.header = *header;

	if (!freerdp_read_four_byte_float(s, &pdu.latitude) ||
	    !freerdp_read_four_byte_float(s, &pdu.longitude) ||
	    !freerdp_read_four_byte_signed_integer(s, &pdu.altitude))
		return FALSE;

	if (Stream_GetRemainingLength(s) >= 1)
	{
		if (!freerdp_read_four_byte_float(s, &speed) ||
		    !freerdp_read_four_byte_float(s, &heading) ||
		    !freerdp_read_four_byte_float(s, &horizontalAccuracy) ||
		    !Stream_CheckAndLogRequiredLength(TAG, s, 1))
			return FALSE;

		Stream_Read_UINT8(s, source);

		pdu.speed = &speed;
		pdu.heading = &heading;
		pdu.horizontalAccuracy = &horizontalAccuracy;
		pdu.source = &source;
	}

	IFCALLRET(context->BaseLocation3D, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->BaseLocation3D failed with error %" PRIu32 "", error);

	return error;
}

static UINT location_server_recv_location2d_delta(LocationServerContext* context, wStream* s,
                                                  const RDPLOCATION_HEADER* header)
{
	RDPLOCATION_LOCATION2D_DELTA_PDU pdu = { 0 };
	UINT error = CHANNEL_RC_OK;
	double speedDelta = 0.0;
	double headingDelta = 0.0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	pdu.header = *header;

	if (!freerdp_read_four_byte_float(s, &pdu.latitudeDelta) ||
	    !freerdp_read_four_byte_float(s, &pdu.longitudeDelta))
		return FALSE;

	if (Stream_GetRemainingLength(s) >= 1)
	{
		if (!freerdp_read_four_byte_float(s, &speedDelta) ||
		    !freerdp_read_four_byte_float(s, &headingDelta))
			return FALSE;

		pdu.speedDelta = &speedDelta;
		pdu.headingDelta = &headingDelta;
	}

	IFCALLRET(context->Location2DDelta, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->Location2DDelta failed with error %" PRIu32 "", error);

	return error;
}

static UINT location_server_recv_location3d_delta(LocationServerContext* context, wStream* s,
                                                  const RDPLOCATION_HEADER* header)
{
	RDPLOCATION_LOCATION3D_DELTA_PDU pdu = { 0 };
	UINT error = CHANNEL_RC_OK;
	double speedDelta = 0.0;
	double headingDelta = 0.0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	pdu.header = *header;

	if (!freerdp_read_four_byte_float(s, &pdu.latitudeDelta) ||
	    !freerdp_read_four_byte_float(s, &pdu.longitudeDelta) ||
	    !freerdp_read_four_byte_signed_integer(s, &pdu.altitudeDelta))
		return FALSE;

	if (Stream_GetRemainingLength(s) >= 1)
	{
		if (!freerdp_read_four_byte_float(s, &speedDelta) ||
		    !freerdp_read_four_byte_float(s, &headingDelta))
			return FALSE;

		pdu.speedDelta = &speedDelta;
		pdu.headingDelta = &headingDelta;
	}

	IFCALLRET(context->Location3DDelta, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->Location3DDelta failed with error %" PRIu32 "", error);

	return error;
}

static UINT location_process_message(location_server* location)
{
	BOOL rc = 0;
	UINT error = ERROR_INTERNAL_ERROR;
	ULONG BytesReturned = 0;
	RDPLOCATION_HEADER header = { 0 };
	wStream* s = NULL;

	WINPR_ASSERT(location);
	WINPR_ASSERT(location->location_channel);

	s = location->buffer;
	WINPR_ASSERT(s);

	Stream_SetPosition(s, 0);
	rc = WTSVirtualChannelRead(location->location_channel, 0, NULL, 0, &BytesReturned);
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

	if (WTSVirtualChannelRead(location->location_channel, 0, Stream_BufferAs(s, char),
	                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
		goto out;
	}

	Stream_SetLength(s, BytesReturned);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, LOCATION_HEADER_SIZE))
		return ERROR_NO_DATA;

	Stream_Read_UINT16(s, header.pduType);
	Stream_Read_UINT32(s, header.pduLength);

	switch (header.pduType)
	{
		case PDUTYPE_CLIENT_READY:
			error = location_server_recv_client_ready(&location->context, s, &header);
			break;
		case PDUTYPE_BASE_LOCATION3D:
			error = location_server_recv_base_location3d(&location->context, s, &header);
			break;
		case PDUTYPE_LOCATION2D_DELTA:
			error = location_server_recv_location2d_delta(&location->context, s, &header);
			break;
		case PDUTYPE_LOCATION3D_DELTA:
			error = location_server_recv_location3d_delta(&location->context, s, &header);
			break;
		default:
			WLog_ERR(TAG, "location_process_message: unknown or invalid pduType %" PRIu8 "",
			         header.pduType);
			break;
	}

out:
	if (error)
		WLog_ERR(TAG, "Response failed with error %" PRIu32 "!", error);

	return error;
}

static UINT location_server_context_poll_int(LocationServerContext* context)
{
	location_server* location = (location_server*)context;
	UINT error = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(location);

	switch (location->state)
	{
		case LOCATION_INITIAL:
			error = location_server_open_channel(location);
			if (error)
				WLog_ERR(TAG, "location_server_open_channel failed with error %" PRIu32 "!", error);
			else
				location->state = LOCATION_OPENED;
			break;
		case LOCATION_OPENED:
			error = location_process_message(location);
			break;
		default:
			break;
	}

	return error;
}

static HANDLE location_server_get_channel_handle(location_server* location)
{
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	HANDLE ChannelEvent = NULL;

	WINPR_ASSERT(location);

	if (WTSVirtualChannelQuery(location->location_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			ChannelEvent = *(HANDLE*)buffer;

		WTSFreeMemory(buffer);
	}

	return ChannelEvent;
}

static DWORD WINAPI location_server_thread_func(LPVOID arg)
{
	DWORD nCount = 0;
	HANDLE events[2] = { 0 };
	location_server* location = (location_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status = 0;

	WINPR_ASSERT(location);

	nCount = 0;
	events[nCount++] = location->stopEvent;

	while ((error == CHANNEL_RC_OK) && (WaitForSingleObject(events[0], 0) != WAIT_OBJECT_0))
	{
		switch (location->state)
		{
			case LOCATION_INITIAL:
				error = location_server_context_poll_int(&location->context);
				if (error == CHANNEL_RC_OK)
				{
					events[1] = location_server_get_channel_handle(location);
					nCount = 2;
				}
				break;
			case LOCATION_OPENED:
				status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);
				switch (status)
				{
					case WAIT_OBJECT_0:
						break;
					case WAIT_OBJECT_0 + 1:
					case WAIT_TIMEOUT:
						error = location_server_context_poll_int(&location->context);
						break;

					case WAIT_FAILED:
					default:
						error = ERROR_INTERNAL_ERROR;
						break;
				}
				break;
			default:
				break;
		}
	}

	(void)WTSVirtualChannelClose(location->location_channel);
	location->location_channel = NULL;

	if (error && location->context.rdpcontext)
		setChannelError(location->context.rdpcontext, error,
		                "location_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

static UINT location_server_open(LocationServerContext* context)
{
	location_server* location = (location_server*)context;

	WINPR_ASSERT(location);

	if (!location->externalThread && (location->thread == NULL))
	{
		location->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!location->stopEvent)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return ERROR_INTERNAL_ERROR;
		}

		location->thread = CreateThread(NULL, 0, location_server_thread_func, location, 0, NULL);
		if (!location->thread)
		{
			WLog_ERR(TAG, "CreateThread failed!");
			(void)CloseHandle(location->stopEvent);
			location->stopEvent = NULL;
			return ERROR_INTERNAL_ERROR;
		}
	}
	location->isOpened = TRUE;

	return CHANNEL_RC_OK;
}

static UINT location_server_close(LocationServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	location_server* location = (location_server*)context;

	WINPR_ASSERT(location);

	if (!location->externalThread && location->thread)
	{
		(void)SetEvent(location->stopEvent);

		if (WaitForSingleObject(location->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		(void)CloseHandle(location->thread);
		(void)CloseHandle(location->stopEvent);
		location->thread = NULL;
		location->stopEvent = NULL;
	}
	if (location->externalThread)
	{
		if (location->state != LOCATION_INITIAL)
		{
			(void)WTSVirtualChannelClose(location->location_channel);
			location->location_channel = NULL;
			location->state = LOCATION_INITIAL;
		}
	}
	location->isOpened = FALSE;

	return error;
}

static UINT location_server_context_poll(LocationServerContext* context)
{
	location_server* location = (location_server*)context;

	WINPR_ASSERT(location);

	if (!location->externalThread)
		return ERROR_INTERNAL_ERROR;

	return location_server_context_poll_int(context);
}

static BOOL location_server_context_handle(LocationServerContext* context, HANDLE* handle)
{
	location_server* location = (location_server*)context;

	WINPR_ASSERT(location);
	WINPR_ASSERT(handle);

	if (!location->externalThread)
		return FALSE;
	if (location->state == LOCATION_INITIAL)
		return FALSE;

	*handle = location_server_get_channel_handle(location);

	return TRUE;
}

static UINT location_server_packet_send(LocationServerContext* context, wStream* s)
{
	location_server* location = (location_server*)context;
	UINT error = CHANNEL_RC_OK;
	ULONG written = 0;

	WINPR_ASSERT(location);
	WINPR_ASSERT(s);

	const size_t pos = Stream_GetPosition(s);
	WINPR_ASSERT(pos <= UINT32_MAX);
	if (!WTSVirtualChannelWrite(location->location_channel, Stream_BufferAs(s, char), (ULONG)pos,
	                            &written))
	{
		WLog_ERR(TAG, "WTSVirtualChannelWrite failed!");
		error = ERROR_INTERNAL_ERROR;
		goto out;
	}

	if (written < Stream_GetPosition(s))
	{
		WLog_WARN(TAG, "Unexpected bytes written: %" PRIu32 "/%" PRIuz "", written,
		          Stream_GetPosition(s));
	}

out:
	Stream_Free(s, TRUE);
	return error;
}

static UINT location_server_send_server_ready(LocationServerContext* context,
                                              const RDPLOCATION_SERVER_READY_PDU* serverReady)
{
	wStream* s = NULL;
	UINT32 pduLength = 0;
	UINT32 protocolVersion = 0;

	WINPR_ASSERT(context);
	WINPR_ASSERT(serverReady);

	protocolVersion = serverReady->protocolVersion;

	pduLength = LOCATION_HEADER_SIZE + 4 + 4;

	s = Stream_New(NULL, pduLength);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	/* RDPLOCATION_HEADER */
	Stream_Write_UINT16(s, PDUTYPE_SERVER_READY);
	Stream_Write_UINT32(s, pduLength);

	Stream_Write_UINT32(s, protocolVersion);
	Stream_Write_UINT32(s, serverReady->flags);

	return location_server_packet_send(context, s);
}

LocationServerContext* location_server_context_new(HANDLE vcm)
{
	location_server* location = (location_server*)calloc(1, sizeof(location_server));

	if (!location)
		return NULL;

	location->context.vcm = vcm;
	location->context.Initialize = location_server_initialize;
	location->context.Open = location_server_open;
	location->context.Close = location_server_close;
	location->context.Poll = location_server_context_poll;
	location->context.ChannelHandle = location_server_context_handle;

	location->context.ServerReady = location_server_send_server_ready;

	location->buffer = Stream_New(NULL, 4096);
	if (!location->buffer)
		goto fail;

	return &location->context;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	location_server_context_free(&location->context);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void location_server_context_free(LocationServerContext* context)
{
	location_server* location = (location_server*)context;

	if (location)
	{
		location_server_close(context);
		Stream_Free(location->buffer, TRUE);
	}

	free(location);
}
