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
#include <freerdp/server/rdpecam.h>

#define TAG CHANNELS_TAG("rdpecam.server")

typedef enum
{
	CAMERA_DEVICE_INITIAL,
	CAMERA_DEVICE_OPENED,
} eCameraDeviceChannelState;

typedef struct
{
	CameraDeviceServerContext context;

	HANDLE stopEvent;

	HANDLE thread;
	void* device_channel;

	DWORD SessionId;

	BOOL isOpened;
	BOOL externalThread;

	/* Channel state */
	eCameraDeviceChannelState state;

	wStream* buffer;
} device_server;

static UINT device_server_initialize(CameraDeviceServerContext* context, BOOL externalThread)
{
	UINT error = CHANNEL_RC_OK;
	device_server* device = (device_server*)context;

	WINPR_ASSERT(device);

	if (device->isOpened)
	{
		WLog_WARN(TAG, "Application error: Camera channel already initialized, "
		               "calling in this state is not possible!");
		return ERROR_INVALID_STATE;
	}

	device->externalThread = externalThread;

	return error;
}

static UINT device_server_open_channel(device_server* device)
{
	CameraDeviceServerContext* context = &device->context;
	DWORD Error = ERROR_SUCCESS;
	HANDLE hEvent;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	UINT32 channelId;
	BOOL status = TRUE;

	WINPR_ASSERT(device);

	if (WTSQuerySessionInformationA(device->context.vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		return ERROR_INTERNAL_ERROR;
	}

	device->SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);
	hEvent = WTSVirtualChannelManagerGetEventHandle(device->context.vcm);

	if (WaitForSingleObject(hEvent, 1000) == WAIT_FAILED)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "!", Error);
		return Error;
	}

	device->device_channel = WTSVirtualChannelOpenEx(device->SessionId, context->virtualChannelName,
	                                                 WTS_CHANNEL_OPTION_DYNAMIC);
	if (!device->device_channel)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed with error %" PRIu32 "!", Error);
		return Error;
	}

	channelId = WTSChannelGetIdByHandle(device->device_channel);

	IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
	if (!status)
	{
		WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return Error;
}

static UINT device_server_handle_success_response(CameraDeviceServerContext* context, wStream* s,
                                                  const CAM_SHARED_MSG_HEADER* header)
{
	CAM_SUCCESS_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	IFCALLRET(context->SuccessResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->SuccessResponse failed with error %" PRIu32 "", error);

	return error;
}

static UINT device_server_recv_error_response(CameraDeviceServerContext* context, wStream* s,
                                              const CAM_SHARED_MSG_HEADER* header)
{
	CAM_ERROR_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_NO_DATA;

	Stream_Read_UINT32(s, pdu.ErrorCode);

	IFCALLRET(context->ErrorResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->ErrorResponse failed with error %" PRIu32 "", error);

	return error;
}

static UINT device_server_recv_stream_list_response(CameraDeviceServerContext* context, wStream* s,
                                                    const CAM_SHARED_MSG_HEADER* header)
{
	CAM_STREAM_LIST_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;
	BYTE i;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 5))
		return ERROR_NO_DATA;

	pdu.N_Descriptions = MIN(Stream_GetRemainingLength(s) / 5, 255);

	for (i = 0; i < pdu.N_Descriptions; ++i)
	{
		CAM_STREAM_DESCRIPTION* StreamDescription = &pdu.StreamDescriptions[i];

		Stream_Read_UINT16(s, StreamDescription->FrameSourceTypes);
		Stream_Read_UINT8(s, StreamDescription->StreamCategory);
		Stream_Read_UINT8(s, StreamDescription->Selected);
		Stream_Read_UINT8(s, StreamDescription->CanBeShared);
	}

	IFCALLRET(context->StreamListResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->StreamListResponse failed with error %" PRIu32 "", error);

	return error;
}

static UINT device_server_recv_media_type_list_response(CameraDeviceServerContext* context,
                                                        wStream* s,
                                                        const CAM_SHARED_MSG_HEADER* header)
{
	CAM_MEDIA_TYPE_LIST_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;
	BYTE i;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 26))
		return ERROR_NO_DATA;

	pdu.N_Descriptions = Stream_GetRemainingLength(s) / 26;

	pdu.MediaTypeDescriptions = calloc(pdu.N_Descriptions, sizeof(CAM_MEDIA_TYPE_DESCRIPTION));
	if (!pdu.MediaTypeDescriptions)
	{
		WLog_ERR(TAG, "Failed to allocate %zu CAM_MEDIA_TYPE_DESCRIPTION structs",
		         pdu.N_Descriptions);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	for (i = 0; i < pdu.N_Descriptions; ++i)
	{
		CAM_MEDIA_TYPE_DESCRIPTION* MediaTypeDescriptions = &pdu.MediaTypeDescriptions[i];

		Stream_Read_UINT8(s, MediaTypeDescriptions->Format);
		Stream_Read_UINT32(s, MediaTypeDescriptions->Width);
		Stream_Read_UINT32(s, MediaTypeDescriptions->Height);
		Stream_Read_UINT32(s, MediaTypeDescriptions->FrameRateNumerator);
		Stream_Read_UINT32(s, MediaTypeDescriptions->FrameRateDenominator);
		Stream_Read_UINT32(s, MediaTypeDescriptions->PixelAspectRatioNumerator);
		Stream_Read_UINT32(s, MediaTypeDescriptions->PixelAspectRatioDenominator);
		Stream_Read_UINT8(s, MediaTypeDescriptions->Flags);
	}

	IFCALLRET(context->MediaTypeListResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->MediaTypeListResponse failed with error %" PRIu32 "", error);

	free(pdu.MediaTypeDescriptions);

	return error;
}

static UINT device_server_recv_current_media_type_response(CameraDeviceServerContext* context,
                                                           wStream* s,
                                                           const CAM_SHARED_MSG_HEADER* header)
{
	CAM_CURRENT_MEDIA_TYPE_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 26))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(s, pdu.MediaTypeDescription.Format);
	Stream_Read_UINT32(s, pdu.MediaTypeDescription.Width);
	Stream_Read_UINT32(s, pdu.MediaTypeDescription.Height);
	Stream_Read_UINT32(s, pdu.MediaTypeDescription.FrameRateNumerator);
	Stream_Read_UINT32(s, pdu.MediaTypeDescription.FrameRateDenominator);
	Stream_Read_UINT32(s, pdu.MediaTypeDescription.PixelAspectRatioNumerator);
	Stream_Read_UINT32(s, pdu.MediaTypeDescription.PixelAspectRatioDenominator);
	Stream_Read_UINT8(s, pdu.MediaTypeDescription.Flags);

	IFCALLRET(context->CurrentMediaTypeResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->CurrentMediaTypeResponse failed with error %" PRIu32 "", error);

	return error;
}

static UINT device_server_recv_sample_response(CameraDeviceServerContext* context, wStream* s,
                                               const CAM_SHARED_MSG_HEADER* header)
{
	CAM_SAMPLE_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(s, pdu.StreamIndex);

	pdu.SampleSize = Stream_GetRemainingLength(s);
	pdu.Sample = Stream_Pointer(s);

	IFCALLRET(context->SampleResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->SampleResponse failed with error %" PRIu32 "", error);

	return error;
}

static UINT device_server_recv_sample_error_response(CameraDeviceServerContext* context, wStream* s,
                                                     const CAM_SHARED_MSG_HEADER* header)
{
	CAM_SAMPLE_ERROR_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 5))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(s, pdu.StreamIndex);
	Stream_Read_UINT32(s, pdu.ErrorCode);

	IFCALLRET(context->SampleErrorResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->SampleErrorResponse failed with error %" PRIu32 "", error);

	return error;
}

static UINT device_server_recv_property_list_response(CameraDeviceServerContext* context,
                                                      wStream* s,
                                                      const CAM_SHARED_MSG_HEADER* header)
{
	CAM_PROPERTY_LIST_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	pdu.N_Properties = Stream_GetRemainingLength(s) / 19;

	if (pdu.N_Properties > 0)
	{
		size_t i;

		pdu.Properties = calloc(pdu.N_Properties, sizeof(CAM_PROPERTY_DESCRIPTION));
		if (!pdu.Properties)
		{
			WLog_ERR(TAG, "Failed to allocate %zu CAM_PROPERTY_DESCRIPTION structs",
			         pdu.N_Properties);
			return ERROR_NOT_ENOUGH_MEMORY;
		}

		for (i = 0; i < pdu.N_Properties; ++i)
		{
			Stream_Read_UINT8(s, pdu.Properties[i].PropertySet);
			Stream_Read_UINT8(s, pdu.Properties[i].PropertyId);
			Stream_Read_UINT8(s, pdu.Properties[i].Capabilities);
			Stream_Read_INT32(s, pdu.Properties[i].MinValue);
			Stream_Read_INT32(s, pdu.Properties[i].MaxValue);
			Stream_Read_INT32(s, pdu.Properties[i].Step);
			Stream_Read_INT32(s, pdu.Properties[i].DefaultValue);
		}
	}

	IFCALLRET(context->PropertyListResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->PropertyListResponse failed with error %" PRIu32 "", error);

	free(pdu.Properties);

	return error;
}

static UINT device_server_recv_property_value_response(CameraDeviceServerContext* context,
                                                       wStream* s,
                                                       const CAM_SHARED_MSG_HEADER* header)
{
	CAM_PROPERTY_VALUE_RESPONSE pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(header);

	pdu.Header = *header;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 5))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(s, pdu.PropertyValue.Mode);
	Stream_Read_INT32(s, pdu.PropertyValue.Value);

	IFCALLRET(context->PropertyValueResponse, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->PropertyValueResponse failed with error %" PRIu32 "", error);

	return error;
}

static UINT device_process_message(device_server* device)
{
	BOOL rc;
	UINT error = ERROR_INTERNAL_ERROR;
	ULONG BytesReturned;
	CAM_SHARED_MSG_HEADER header = { 0 };
	wStream* s;

	WINPR_ASSERT(device);
	WINPR_ASSERT(device->device_channel);

	s = device->buffer;
	WINPR_ASSERT(s);

	Stream_SetPosition(s, 0);
	rc = WTSVirtualChannelRead(device->device_channel, 0, NULL, 0, &BytesReturned);
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

	if (WTSVirtualChannelRead(device->device_channel, 0, (PCHAR)Stream_Buffer(s),
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
		case CAM_MSG_ID_SuccessResponse:
			error = device_server_handle_success_response(&device->context, s, &header);
			break;
		case CAM_MSG_ID_ErrorResponse:
			error = device_server_recv_error_response(&device->context, s, &header);
			break;
		case CAM_MSG_ID_StreamListResponse:
			error = device_server_recv_stream_list_response(&device->context, s, &header);
			break;
		case CAM_MSG_ID_MediaTypeListResponse:
			error = device_server_recv_media_type_list_response(&device->context, s, &header);
			break;
		case CAM_MSG_ID_CurrentMediaTypeResponse:
			error = device_server_recv_current_media_type_response(&device->context, s, &header);
			break;
		case CAM_MSG_ID_SampleResponse:
			error = device_server_recv_sample_response(&device->context, s, &header);
			break;
		case CAM_MSG_ID_SampleErrorResponse:
			error = device_server_recv_sample_error_response(&device->context, s, &header);
			break;
		case CAM_MSG_ID_PropertyListResponse:
			error = device_server_recv_property_list_response(&device->context, s, &header);
			break;
		case CAM_MSG_ID_PropertyValueResponse:
			error = device_server_recv_property_value_response(&device->context, s, &header);
			break;
		default:
			WLog_ERR(TAG, "device_process_message: unknown or invalid MessageId %" PRIu8 "",
			         header.MessageId);
			break;
	}

out:
	if (error)
		WLog_ERR(TAG, "Response failed with error %" PRIu32 "!", error);

	return error;
}

static UINT device_server_context_poll_int(CameraDeviceServerContext* context)
{
	device_server* device = (device_server*)context;
	UINT error = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(device);

	switch (device->state)
	{
		case CAMERA_DEVICE_INITIAL:
			error = device_server_open_channel(device);
			if (error)
				WLog_ERR(TAG, "device_server_open_channel failed with error %" PRIu32 "!", error);
			else
				device->state = CAMERA_DEVICE_OPENED;
			break;
		case CAMERA_DEVICE_OPENED:
			error = device_process_message(device);
			break;
	}

	return error;
}

static HANDLE device_server_get_channel_handle(device_server* device)
{
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	HANDLE ChannelEvent = NULL;

	WINPR_ASSERT(device);

	if (WTSVirtualChannelQuery(device->device_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			CopyMemory(&ChannelEvent, buffer, sizeof(HANDLE));

		WTSFreeMemory(buffer);
	}

	return ChannelEvent;
}

static DWORD WINAPI device_server_thread_func(LPVOID arg)
{
	DWORD nCount;
	HANDLE events[2] = { 0 };
	device_server* device = (device_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status;

	WINPR_ASSERT(device);

	nCount = 0;
	events[nCount++] = device->stopEvent;

	while ((error == CHANNEL_RC_OK) && (WaitForSingleObject(events[0], 0) != WAIT_OBJECT_0))
	{
		switch (device->state)
		{
			case CAMERA_DEVICE_INITIAL:
				error = device_server_context_poll_int(&device->context);
				if (error == CHANNEL_RC_OK)
				{
					events[1] = device_server_get_channel_handle(device);
					nCount = 2;
				}
				break;
			case CAMERA_DEVICE_OPENED:
				status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);
				switch (status)
				{
					case WAIT_OBJECT_0:
						break;
					case WAIT_OBJECT_0 + 1:
					case WAIT_TIMEOUT:
						error = device_server_context_poll_int(&device->context);
						break;

					case WAIT_FAILED:
					default:
						error = ERROR_INTERNAL_ERROR;
						break;
				}
				break;
		}
	}

	WTSVirtualChannelClose(device->device_channel);
	device->device_channel = NULL;

	if (error && device->context.rdpcontext)
		setChannelError(device->context.rdpcontext, error,
		                "device_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

static UINT device_server_open(CameraDeviceServerContext* context)
{
	device_server* device = (device_server*)context;

	WINPR_ASSERT(device);

	if (!device->externalThread && (device->thread == NULL))
	{
		device->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!device->stopEvent)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return ERROR_INTERNAL_ERROR;
		}

		device->thread = CreateThread(NULL, 0, device_server_thread_func, device, 0, NULL);
		if (!device->thread)
		{
			WLog_ERR(TAG, "CreateThread failed!");
			CloseHandle(device->stopEvent);
			device->stopEvent = NULL;
			return ERROR_INTERNAL_ERROR;
		}
	}
	device->isOpened = TRUE;

	return CHANNEL_RC_OK;
}

static UINT device_server_close(CameraDeviceServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	device_server* device = (device_server*)context;

	WINPR_ASSERT(device);

	if (!device->externalThread && device->thread)
	{
		SetEvent(device->stopEvent);

		if (WaitForSingleObject(device->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		CloseHandle(device->thread);
		CloseHandle(device->stopEvent);
		device->thread = NULL;
		device->stopEvent = NULL;
	}
	if (device->externalThread)
	{
		if (device->state != CAMERA_DEVICE_INITIAL)
		{
			WTSVirtualChannelClose(device->device_channel);
			device->device_channel = NULL;
			device->state = CAMERA_DEVICE_INITIAL;
		}
	}
	device->isOpened = FALSE;

	return error;
}

static UINT device_server_context_poll(CameraDeviceServerContext* context)
{
	device_server* device = (device_server*)context;

	WINPR_ASSERT(device);

	if (!device->externalThread)
		return ERROR_INTERNAL_ERROR;

	return device_server_context_poll_int(context);
}

static BOOL device_server_context_handle(CameraDeviceServerContext* context, HANDLE* handle)
{
	device_server* device = (device_server*)context;

	WINPR_ASSERT(device);
	WINPR_ASSERT(handle);

	if (!device->externalThread)
		return FALSE;
	if (device->state == CAMERA_DEVICE_INITIAL)
		return FALSE;

	*handle = device_server_get_channel_handle(device);

	return TRUE;
}

static wStream* device_server_packet_new(size_t size, BYTE version, BYTE messageId)
{
	wStream* s;

	WINPR_ASSERT(size > 0);

	/* Allocate what we need plus header bytes */
	s = Stream_New(NULL, size + CAM_HEADER_SIZE);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return NULL;
	}

	Stream_Write_UINT8(s, version);
	Stream_Write_UINT8(s, messageId);

	return s;
}

static UINT device_server_packet_send(CameraDeviceServerContext* context, wStream* s)
{
	device_server* device = (device_server*)context;
	UINT error = CHANNEL_RC_OK;
	ULONG written;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	if (!WTSVirtualChannelWrite(device->device_channel, (PCHAR)Stream_Buffer(s),
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

static UINT device_server_write_and_send_header(CameraDeviceServerContext* context, BYTE messageId)
{
	wStream* s;

	WINPR_ASSERT(context);

	s = device_server_packet_new(0, context->protocolVersion, messageId);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	return device_server_packet_send(context, s);
}

static UINT
device_send_activate_device_request_pdu(CameraDeviceServerContext* context,
                                        const CAM_ACTIVATE_DEVICE_REQUEST* activateDeviceRequest)
{
	WINPR_ASSERT(context);

	return device_server_write_and_send_header(context, CAM_MSG_ID_ActivateDeviceRequest);
}

static UINT device_send_deactivate_device_request_pdu(
    CameraDeviceServerContext* context,
    const CAM_DEACTIVATE_DEVICE_REQUEST* deactivateDeviceRequest)
{
	WINPR_ASSERT(context);

	return device_server_write_and_send_header(context, CAM_MSG_ID_DeactivateDeviceRequest);
}

static UINT device_send_stream_list_request_pdu(CameraDeviceServerContext* context,
                                                const CAM_STREAM_LIST_REQUEST* streamListRequest)
{
	WINPR_ASSERT(context);

	return device_server_write_and_send_header(context, CAM_MSG_ID_StreamListRequest);
}

static UINT
device_send_media_type_list_request_pdu(CameraDeviceServerContext* context,
                                        const CAM_MEDIA_TYPE_LIST_REQUEST* mediaTypeListRequest)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(mediaTypeListRequest);

	s = device_server_packet_new(1, context->protocolVersion, CAM_MSG_ID_MediaTypeListRequest);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT8(s, mediaTypeListRequest->StreamIndex);

	return device_server_packet_send(context, s);
}

static UINT device_send_current_media_type_request_pdu(
    CameraDeviceServerContext* context,
    const CAM_CURRENT_MEDIA_TYPE_REQUEST* currentMediaTypeRequest)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(currentMediaTypeRequest);

	s = device_server_packet_new(1, context->protocolVersion, CAM_MSG_ID_CurrentMediaTypeRequest);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT8(s, currentMediaTypeRequest->StreamIndex);

	return device_server_packet_send(context, s);
}

static UINT
device_send_start_streams_request_pdu(CameraDeviceServerContext* context,
                                      const CAM_START_STREAMS_REQUEST* startStreamsRequest)
{
	wStream* s;
	size_t i;

	WINPR_ASSERT(context);
	WINPR_ASSERT(startStreamsRequest);

	s = device_server_packet_new(startStreamsRequest->N_Infos * 27ul, context->protocolVersion,
	                             CAM_MSG_ID_StartStreamsRequest);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	for (i = 0; i < startStreamsRequest->N_Infos; ++i)
	{
		const CAM_START_STREAM_INFO* info = &startStreamsRequest->StartStreamsInfo[i];
		const CAM_MEDIA_TYPE_DESCRIPTION* description = &info->MediaTypeDescription;

		Stream_Write_UINT8(s, info->StreamIndex);

		Stream_Write_UINT8(s, description->Format);
		Stream_Write_UINT32(s, description->Width);
		Stream_Write_UINT32(s, description->Height);
		Stream_Write_UINT32(s, description->FrameRateNumerator);
		Stream_Write_UINT32(s, description->FrameRateDenominator);
		Stream_Write_UINT32(s, description->PixelAspectRatioNumerator);
		Stream_Write_UINT32(s, description->PixelAspectRatioDenominator);
		Stream_Write_UINT8(s, description->Flags);
	}

	return device_server_packet_send(context, s);
}

static UINT device_send_stop_streams_request_pdu(CameraDeviceServerContext* context,
                                                 const CAM_STOP_STREAMS_REQUEST* stopStreamsRequest)
{
	WINPR_ASSERT(context);

	return device_server_write_and_send_header(context, CAM_MSG_ID_StopStreamsRequest);
}

static UINT device_send_sample_request_pdu(CameraDeviceServerContext* context,
                                           const CAM_SAMPLE_REQUEST* sampleRequest)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(sampleRequest);

	s = device_server_packet_new(1, context->protocolVersion, CAM_MSG_ID_SampleRequest);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT8(s, sampleRequest->StreamIndex);

	return device_server_packet_send(context, s);
}

static UINT
device_send_property_list_request_pdu(CameraDeviceServerContext* context,
                                      const CAM_PROPERTY_LIST_REQUEST* propertyListRequest)
{
	WINPR_ASSERT(context);

	return device_server_write_and_send_header(context, CAM_MSG_ID_PropertyListRequest);
}

static UINT
device_send_property_value_request_pdu(CameraDeviceServerContext* context,
                                       const CAM_PROPERTY_VALUE_REQUEST* propertyValueRequest)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(propertyValueRequest);

	s = device_server_packet_new(2, context->protocolVersion, CAM_MSG_ID_PropertyValueRequest);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT8(s, propertyValueRequest->PropertySet);
	Stream_Write_UINT8(s, propertyValueRequest->PropertyId);

	return device_server_packet_send(context, s);
}

static UINT device_send_set_property_value_request_pdu(
    CameraDeviceServerContext* context,
    const CAM_SET_PROPERTY_VALUE_REQUEST* setPropertyValueRequest)
{
	wStream* s;

	WINPR_ASSERT(context);
	WINPR_ASSERT(setPropertyValueRequest);

	s = device_server_packet_new(2 + 5, context->protocolVersion,
	                             CAM_MSG_ID_SetPropertyValueRequest);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT8(s, setPropertyValueRequest->PropertySet);
	Stream_Write_UINT8(s, setPropertyValueRequest->PropertyId);

	Stream_Write_UINT8(s, setPropertyValueRequest->PropertyValue.Mode);
	Stream_Write_INT32(s, setPropertyValueRequest->PropertyValue.Value);

	return device_server_packet_send(context, s);
}

CameraDeviceServerContext* camera_device_server_context_new(HANDLE vcm)
{
	device_server* device = (device_server*)calloc(1, sizeof(device_server));

	if (!device)
		return NULL;

	device->context.vcm = vcm;
	device->context.Initialize = device_server_initialize;
	device->context.Open = device_server_open;
	device->context.Close = device_server_close;
	device->context.Poll = device_server_context_poll;
	device->context.ChannelHandle = device_server_context_handle;

	device->context.ActivateDeviceRequest = device_send_activate_device_request_pdu;
	device->context.DeactivateDeviceRequest = device_send_deactivate_device_request_pdu;

	device->context.StreamListRequest = device_send_stream_list_request_pdu;
	device->context.MediaTypeListRequest = device_send_media_type_list_request_pdu;
	device->context.CurrentMediaTypeRequest = device_send_current_media_type_request_pdu;

	device->context.StartStreamsRequest = device_send_start_streams_request_pdu;
	device->context.StopStreamsRequest = device_send_stop_streams_request_pdu;
	device->context.SampleRequest = device_send_sample_request_pdu;

	device->context.PropertyListRequest = device_send_property_list_request_pdu;
	device->context.PropertyValueRequest = device_send_property_value_request_pdu;
	device->context.SetPropertyValueRequest = device_send_set_property_value_request_pdu;

	device->buffer = Stream_New(NULL, 4096);
	if (!device->buffer)
		goto fail;

	return &device->context;
fail:
	camera_device_server_context_free(&device->context);
	return NULL;
}

void camera_device_server_context_free(CameraDeviceServerContext* context)
{
	device_server* device = (device_server*)context;

	if (device)
	{
		device_server_close(context);
		Stream_Free(device->buffer, TRUE);
	}

	free(context->virtualChannelName);

	free(device);
}
