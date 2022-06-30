/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Capture Virtual Channel Extension
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
#include <freerdp/server/rdpecam-enumerator.h>

#define TAG CHANNELS_TAG("rdpecam-enumerator.server")

typedef enum
{
	ENUMERATOR_INITIAL,
	ENUMERATOR_OPENED,
} eEnumeratorChannelState;

typedef struct
{
	CamDevEnumServerContext context;

	HANDLE stopEvent;

	HANDLE thread;
	void* enumerator_channel;

	DWORD SessionId;

	BOOL isOpened;
	BOOL externalThread;

	/* Channel state */
	eEnumeratorChannelState state;

	wStream* buffer;
} enumerator_server;

static UINT enumerator_server_initialize(CamDevEnumServerContext* context, BOOL externalThread)
{
	UINT error = CHANNEL_RC_OK;
	enumerator_server* enumerator = (enumerator_server*)context;

	WINPR_ASSERT(enumerator);

	if (enumerator->isOpened)
	{
		WLog_WARN(TAG, "Application error: Camera Device Enumerator channel already initialized, "
		               "calling in this state is not possible!");
		return ERROR_INVALID_STATE;
	}

	enumerator->externalThread = externalThread;

	return error;
}

static UINT enumerator_server_open_channel(enumerator_server* enumerator)
{
	CamDevEnumServerContext* context = &enumerator->context;
	DWORD Error = ERROR_SUCCESS;
	HANDLE hEvent;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	UINT32 channelId;
	BOOL status = TRUE;

	WINPR_ASSERT(enumerator);

	if (WTSQuerySessionInformationA(enumerator->context.vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		return ERROR_INTERNAL_ERROR;
	}

	enumerator->SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);
	hEvent = WTSVirtualChannelManagerGetEventHandle(enumerator->context.vcm);

	if (WaitForSingleObject(hEvent, 1000) == WAIT_FAILED)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", Error);
		return Error;
	}

	enumerator->enumerator_channel = WTSVirtualChannelOpenEx(
	    enumerator->SessionId, RDPECAM_CONTROL_DVC_CHANNEL_NAME, WTS_CHANNEL_OPTION_DYNAMIC);
	if (!enumerator->enumerator_channel)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed with error %" PRIu32 "!", Error);
		return Error;
	}

	channelId = WTSChannelGetIdByHandle(enumerator->enumerator_channel);

	IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
	if (!status)
	{
		WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return Error;
}

static UINT enumerator_server_handle_select_version_request(CamDevEnumServerContext* context,
                                                            wStream* s,
                                                            const CAM_SHARED_MSG_HEADER* header)
{
	CAM_SELECT_VERSION_REQUEST pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	IFCALLRET(context->SelectVersionRequest, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->SelectVersionRequest failed with error %" PRIu32 "", error);

	return error;
}

static UINT enumerator_server_recv_device_added_notification(CamDevEnumServerContext* context,
                                                             wStream* s,
                                                             const CAM_SHARED_MSG_HEADER* header)
{
	CAM_DEVICE_ADDED_NOTIFICATION pdu;
	UINT error = CHANNEL_RC_OK;
	size_t remaining_length;
	WCHAR* channel_name_start;
	char* tmp;
	size_t i;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	/*
	 * RequiredLength 4:
	 *
	 * Nullterminator DeviceName (2),
	 * VirtualChannelName (>= 1),
	 * Nullterminator VirtualChannelName (1)
	 */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_NO_DATA;

	pdu.DeviceName = (WCHAR*)Stream_Pointer(s);

	remaining_length = Stream_GetRemainingLength(s);
	channel_name_start = (WCHAR*)Stream_Pointer(s);

	/* Search for null terminator of DeviceName */
	for (i = 0; i < remaining_length; i += sizeof(WCHAR), ++channel_name_start)
	{
		if (*channel_name_start == L'\0')
			break;
	}

	if (*channel_name_start != L'\0')
	{
		WLog_ERR(TAG, "enumerator_server_recv_device_added_notification: Invalid DeviceName!");
		return ERROR_INVALID_DATA;
	}

	pdu.VirtualChannelName = (char*)++channel_name_start;
	++i;

	if (i >= remaining_length || *pdu.VirtualChannelName == '\0')
	{
		WLog_ERR(TAG,
		         "enumerator_server_recv_device_added_notification: Invalid VirtualChannelName!");
		return ERROR_INVALID_DATA;
	}

	tmp = pdu.VirtualChannelName;
	for (; i < remaining_length; ++i, ++tmp)
	{
		if (*tmp == '\0')
			break;
	}

	if (*tmp != '\0')
	{
		WLog_ERR(TAG,
		         "enumerator_server_recv_device_added_notification: Invalid VirtualChannelName!");
		return ERROR_INVALID_DATA;
	}

	IFCALLRET(context->DeviceAddedNotification, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->DeviceAddedNotification failed with error %" PRIu32 "", error);

	return error;
}

static UINT enumerator_server_recv_device_removed_notification(CamDevEnumServerContext* context,
                                                               wStream* s,
                                                               const CAM_SHARED_MSG_HEADER* header)
{
	CAM_DEVICE_REMOVED_NOTIFICATION pdu;
	UINT error = CHANNEL_RC_OK;
	size_t remaining_length;
	char* tmp;
	size_t i;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return ERROR_NO_DATA;

	pdu.VirtualChannelName = (char*)Stream_Pointer(s);

	remaining_length = Stream_GetRemainingLength(s);
	tmp = (char*)(Stream_Pointer(s) + 1);

	for (i = 1; i < remaining_length; ++i, ++tmp)
	{
		if (*tmp == '\0')
			break;
	}

	if (*tmp != '\0')
	{
		WLog_ERR(TAG,
		         "enumerator_server_recv_device_removed_notification: Invalid VirtualChannelName!");
		return ERROR_INVALID_DATA;
	}

	IFCALLRET(context->DeviceRemovedNotification, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->DeviceRemovedNotification failed with error %" PRIu32 "", error);

	return error;
}

static UINT enumerator_process_message(enumerator_server* enumerator)
{
	BOOL rc;
	UINT error = ERROR_INTERNAL_ERROR;
	ULONG BytesReturned;
	CAM_SHARED_MSG_HEADER header = { 0 };
	wStream* s;

	WINPR_ASSERT(enumerator);
	WINPR_ASSERT(enumerator->enumerator_channel);

	s = enumerator->buffer;
	WINPR_ASSERT(s);

	Stream_SetPosition(s, 0);
	rc = WTSVirtualChannelRead(enumerator->enumerator_channel, 0, NULL, 0, &BytesReturned);
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

	if (WTSVirtualChannelRead(enumerator->enumerator_channel, 0, (PCHAR)Stream_Buffer(s),
	                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
		goto out;
	}

	Stream_SetLength(s, BytesReturned);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, CAM_HEADER_SIZE))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(s, header.Version);
	Stream_Read_UINT8(s, header.MessageId);

	switch (header.MessageId)
	{
		case CAM_MSG_ID_SelectVersionRequest:
			error =
			    enumerator_server_handle_select_version_request(&enumerator->context, s, &header);
			break;
		case CAM_MSG_ID_DeviceAddedNotification:
			error =
			    enumerator_server_recv_device_added_notification(&enumerator->context, s, &header);
			break;
		case CAM_MSG_ID_DeviceRemovedNotification:
			error = enumerator_server_recv_device_removed_notification(&enumerator->context, s,
			                                                           &header);
			break;
		default:
			WLog_ERR(TAG, "enumerator_process_message: unknown or invalid MessageId %" PRIu8 "",
			         header.MessageId);
			break;
	}

out:
	if (error)
		WLog_ERR(TAG, "Response failed with error %" PRIu32 "!", error);

	return error;
}

static UINT enumerator_server_context_poll_int(CamDevEnumServerContext* context)
{
	enumerator_server* enumerator = (enumerator_server*)context;
	UINT error = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(enumerator);

	switch (enumerator->state)
	{
		case ENUMERATOR_INITIAL:
			error = enumerator_server_open_channel(enumerator);
			if (error)
				WLog_ERR(TAG, "enumerator_server_open_channel failed with error %" PRIu32 "!",
				         error);
			else
				enumerator->state = ENUMERATOR_OPENED;
			break;
		case ENUMERATOR_OPENED:
			error = enumerator_process_message(enumerator);
			break;
	}

	return error;
}

static HANDLE enumerator_server_get_channel_handle(enumerator_server* enumerator)
{
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	HANDLE ChannelEvent = NULL;

	WINPR_ASSERT(enumerator);

	if (WTSVirtualChannelQuery(enumerator->enumerator_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	return ChannelEvent;
}

static DWORD WINAPI enumerator_server_thread_func(LPVOID arg)
{
	DWORD nCount;
	HANDLE events[2] = { 0 };
	enumerator_server* enumerator = (enumerator_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status;

	WINPR_ASSERT(enumerator);

	nCount = 0;
	events[nCount++] = enumerator->stopEvent;

	while ((error == CHANNEL_RC_OK) && (WaitForSingleObject(events[0], 0) != WAIT_OBJECT_0))
	{
		switch (enumerator->state)
		{
			case ENUMERATOR_INITIAL:
				error = enumerator_server_context_poll_int(&enumerator->context);
				if (error == CHANNEL_RC_OK)
				{
					events[1] = enumerator_server_get_channel_handle(enumerator);
					nCount = 2;
				}
				break;
			case ENUMERATOR_OPENED:
				status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);
				switch (status)
				{
					case WAIT_OBJECT_0:
						break;
					case WAIT_OBJECT_0 + 1:
					case WAIT_TIMEOUT:
						error = enumerator_server_context_poll_int(&enumerator->context);
						break;

					case WAIT_FAILED:
					default:
						error = ERROR_INTERNAL_ERROR;
						break;
				}
				break;
		}
	}

	WTSVirtualChannelClose(enumerator->enumerator_channel);
	enumerator->enumerator_channel = NULL;

	if (error && enumerator->context.rdpcontext)
		setChannelError(enumerator->context.rdpcontext, error,
		                "enumerator_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

static UINT enumerator_server_open(CamDevEnumServerContext* context)
{
	enumerator_server* enumerator = (enumerator_server*)context;

	WINPR_ASSERT(enumerator);

	if (!enumerator->externalThread && (enumerator->thread == NULL))
	{
		enumerator->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!enumerator->stopEvent)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return ERROR_INTERNAL_ERROR;
		}

		enumerator->thread =
		    CreateThread(NULL, 0, enumerator_server_thread_func, enumerator, 0, NULL);
		if (!enumerator->thread)
		{
			WLog_ERR(TAG, "CreateThread failed!");
			CloseHandle(enumerator->stopEvent);
			enumerator->stopEvent = NULL;
			return ERROR_INTERNAL_ERROR;
		}
	}
	enumerator->isOpened = TRUE;

	return CHANNEL_RC_OK;
}

static UINT enumerator_server_close(CamDevEnumServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	enumerator_server* enumerator = (enumerator_server*)context;

	WINPR_ASSERT(enumerator);

	if (!enumerator->externalThread && enumerator->thread)
	{
		SetEvent(enumerator->stopEvent);

		if (WaitForSingleObject(enumerator->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		CloseHandle(enumerator->thread);
		CloseHandle(enumerator->stopEvent);
		enumerator->thread = NULL;
		enumerator->stopEvent = NULL;
	}
	if (enumerator->externalThread)
	{
		if (enumerator->state != ENUMERATOR_INITIAL)
		{
			WTSVirtualChannelClose(enumerator->enumerator_channel);
			enumerator->enumerator_channel = NULL;
			enumerator->state = ENUMERATOR_INITIAL;
		}
	}
	enumerator->isOpened = FALSE;

	return error;
}

static UINT enumerator_server_context_poll(CamDevEnumServerContext* context)
{
	enumerator_server* enumerator = (enumerator_server*)context;

	WINPR_ASSERT(enumerator);

	if (!enumerator->externalThread)
		return ERROR_INTERNAL_ERROR;

	return enumerator_server_context_poll_int(context);
}

static BOOL enumerator_server_context_handle(CamDevEnumServerContext* context, HANDLE* handle)
{
	enumerator_server* enumerator = (enumerator_server*)context;

	WINPR_ASSERT(enumerator);
	WINPR_ASSERT(handle);

	if (!enumerator->externalThread)
		return FALSE;
	if (enumerator->state == ENUMERATOR_INITIAL)
		return FALSE;

	*handle = enumerator_server_get_channel_handle(enumerator);

	return TRUE;
}

static UINT enumerator_server_packet_send(CamDevEnumServerContext* context, wStream* s)
{
	enumerator_server* enumerator = (enumerator_server*)context;
	UINT error = CHANNEL_RC_OK;
	ULONG written;

	if (!WTSVirtualChannelWrite(enumerator->enumerator_channel, (PCHAR)Stream_Buffer(s),
	                            Stream_GetPosition(s), &written))
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

static UINT enumerator_send_select_version_response_pdu(
    CamDevEnumServerContext* context, const CAM_SELECT_VERSION_RESPONSE* selectVersionResponse)
{
	wStream* s;

	s = Stream_New(NULL, CAM_HEADER_SIZE);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	Stream_Write_UINT8(s, selectVersionResponse->Header.Version);
	Stream_Write_UINT8(s, selectVersionResponse->Header.MessageId);

	return enumerator_server_packet_send(context, s);
}

CamDevEnumServerContext* cam_dev_enum_server_context_new(HANDLE vcm)
{
	enumerator_server* enumerator = (enumerator_server*)calloc(1, sizeof(enumerator_server));

	if (!enumerator)
		return NULL;

	enumerator->context.vcm = vcm;
	enumerator->context.Initialize = enumerator_server_initialize;
	enumerator->context.Open = enumerator_server_open;
	enumerator->context.Close = enumerator_server_close;
	enumerator->context.Poll = enumerator_server_context_poll;
	enumerator->context.ChannelHandle = enumerator_server_context_handle;

	enumerator->context.SelectVersionResponse = enumerator_send_select_version_response_pdu;

	enumerator->buffer = Stream_New(NULL, 4096);
	if (!enumerator->buffer)
		goto fail;

	return &enumerator->context;
fail:
	cam_dev_enum_server_context_free(&enumerator->context);
	return NULL;
}

void cam_dev_enum_server_context_free(CamDevEnumServerContext* context)
{
	enumerator_server* enumerator = (enumerator_server*)context;

	if (enumerator)
	{
		enumerator_server_close(context);
		Stream_Free(enumerator->buffer, TRUE);
	}

	free(enumerator);
}
