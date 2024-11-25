/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Mouse Cursor Virtual Channel Extension
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
#include <freerdp/server/rdpemsc.h>

#define TAG CHANNELS_TAG("rdpemsc.server")

typedef enum
{
	MOUSE_CURSOR_INITIAL,
	MOUSE_CURSOR_OPENED,
} eMouseCursorChannelState;

typedef struct
{
	MouseCursorServerContext context;

	HANDLE stopEvent;

	HANDLE thread;
	void* mouse_cursor_channel;

	DWORD SessionId;

	BOOL isOpened;
	BOOL externalThread;

	/* Channel state */
	eMouseCursorChannelState state;

	wStream* buffer;
} mouse_cursor_server;

static UINT mouse_cursor_server_initialize(MouseCursorServerContext* context, BOOL externalThread)
{
	UINT error = CHANNEL_RC_OK;
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)context;

	WINPR_ASSERT(mouse_cursor);

	if (mouse_cursor->isOpened)
	{
		WLog_WARN(TAG, "Application error: Mouse Cursor channel already initialized, "
		               "calling in this state is not possible!");
		return ERROR_INVALID_STATE;
	}

	mouse_cursor->externalThread = externalThread;

	return error;
}

static UINT mouse_cursor_server_open_channel(mouse_cursor_server* mouse_cursor)
{
	MouseCursorServerContext* context = NULL;
	DWORD Error = ERROR_SUCCESS;
	DWORD BytesReturned = 0;
	PULONG pSessionId = NULL;
	UINT32 channelId = 0;
	BOOL status = TRUE;

	WINPR_ASSERT(mouse_cursor);
	context = &mouse_cursor->context;
	WINPR_ASSERT(context);

	if (WTSQuerySessionInformationA(mouse_cursor->context.vcm, WTS_CURRENT_SESSION, WTSSessionId,
	                                (LPSTR*)&pSessionId, &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSQuerySessionInformationA failed!");
		return ERROR_INTERNAL_ERROR;
	}

	mouse_cursor->SessionId = (DWORD)*pSessionId;
	WTSFreeMemory(pSessionId);

	mouse_cursor->mouse_cursor_channel = WTSVirtualChannelOpenEx(
	    mouse_cursor->SessionId, RDPEMSC_DVC_CHANNEL_NAME, WTS_CHANNEL_OPTION_DYNAMIC);
	if (!mouse_cursor->mouse_cursor_channel)
	{
		Error = GetLastError();
		WLog_ERR(TAG, "WTSVirtualChannelOpenEx failed with error %" PRIu32 "!", Error);
		return Error;
	}

	channelId = WTSChannelGetIdByHandle(mouse_cursor->mouse_cursor_channel);

	IFCALLRET(context->ChannelIdAssigned, status, context, channelId);
	if (!status)
	{
		WLog_ERR(TAG, "context->ChannelIdAssigned failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return Error;
}

static BOOL read_cap_set(wStream* s, wArrayList* capsSets)
{
	RDP_MOUSE_CURSOR_CAPSET* capsSet = NULL;
	UINT32 signature = 0;
	RDP_MOUSE_CURSOR_CAPVERSION version = RDP_MOUSE_CURSOR_CAPVERSION_INVALID;
	UINT32 size = 0;
	size_t capsDataSize = 0;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return FALSE;

	Stream_Read_UINT32(s, signature);
	Stream_Read_UINT32(s, version);
	Stream_Read_UINT32(s, size);

	if (size < 12)
	{
		WLog_ERR(TAG, "Size of caps set is invalid: %u", size);
		return FALSE;
	}

	capsDataSize = size - 12;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, capsDataSize))
		return FALSE;

	switch (version)
	{
		case RDP_MOUSE_CURSOR_CAPVERSION_1:
		{
			RDP_MOUSE_CURSOR_CAPSET_VERSION1* capsSetV1 = NULL;

			capsSetV1 = calloc(1, sizeof(RDP_MOUSE_CURSOR_CAPSET_VERSION1));
			if (!capsSetV1)
				return FALSE;

			capsSet = (RDP_MOUSE_CURSOR_CAPSET*)capsSetV1;
			break;
		}
		default:
			WLog_WARN(TAG, "Received caps set with unknown version %u", version);
			Stream_Seek(s, capsDataSize);
			return TRUE;
	}
	WINPR_ASSERT(capsSet);

	capsSet->signature = signature;
	capsSet->version = version;
	capsSet->size = size;

	if (!ArrayList_Append(capsSets, capsSet))
	{
		WLog_ERR(TAG, "Failed to append caps set to arraylist");
		free(capsSet);
		return FALSE;
	}

	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): ArrayList_Append owns capsSet
	return TRUE;
}

static UINT mouse_cursor_server_recv_cs_caps_advertise(MouseCursorServerContext* context,
                                                       wStream* s,
                                                       const RDP_MOUSE_CURSOR_HEADER* header)
{
	RDP_MOUSE_CURSOR_CAPS_ADVERTISE_PDU pdu = { 0 };
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);
	WINPR_ASSERT(header);

	pdu.header = *header;

	/* There must be at least one capability set present */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_NO_DATA;

	pdu.capsSets = ArrayList_New(FALSE);
	if (!pdu.capsSets)
	{
		WLog_ERR(TAG, "Failed to allocate arraylist");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	wObject* aobj = ArrayList_Object(pdu.capsSets);
	WINPR_ASSERT(aobj);
	aobj->fnObjectFree = free;

	while (Stream_GetRemainingLength(s) > 0)
	{
		if (!read_cap_set(s, pdu.capsSets))
		{
			ArrayList_Free(pdu.capsSets);
			return ERROR_INVALID_DATA;
		}
	}

	IFCALLRET(context->CapsAdvertise, error, context, &pdu);
	if (error)
		WLog_ERR(TAG, "context->CapsAdvertise failed with error %" PRIu32 "", error);

	ArrayList_Free(pdu.capsSets);

	return error;
}

static UINT mouse_cursor_process_message(mouse_cursor_server* mouse_cursor)
{
	BOOL rc = 0;
	UINT error = ERROR_INTERNAL_ERROR;
	ULONG BytesReturned = 0;
	RDP_MOUSE_CURSOR_HEADER header = { 0 };
	wStream* s = NULL;

	WINPR_ASSERT(mouse_cursor);
	WINPR_ASSERT(mouse_cursor->mouse_cursor_channel);

	s = mouse_cursor->buffer;
	WINPR_ASSERT(s);

	Stream_SetPosition(s, 0);
	rc = WTSVirtualChannelRead(mouse_cursor->mouse_cursor_channel, 0, NULL, 0, &BytesReturned);
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

	if (WTSVirtualChannelRead(mouse_cursor->mouse_cursor_channel, 0, Stream_BufferAs(s, char),
	                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
	{
		WLog_ERR(TAG, "WTSVirtualChannelRead failed!");
		goto out;
	}

	Stream_SetLength(s, BytesReturned);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, RDPEMSC_HEADER_SIZE))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(s, header.pduType);
	Stream_Read_UINT8(s, header.updateType);
	Stream_Read_UINT16(s, header.reserved);

	switch (header.pduType)
	{
		case PDUTYPE_CS_CAPS_ADVERTISE:
			error = mouse_cursor_server_recv_cs_caps_advertise(&mouse_cursor->context, s, &header);
			break;
		default:
			WLog_ERR(TAG, "mouse_cursor_process_message: unknown or invalid pduType %" PRIu8 "",
			         header.pduType);
			break;
	}

out:
	if (error)
		WLog_ERR(TAG, "Response failed with error %" PRIu32 "!", error);

	return error;
}

static UINT mouse_cursor_server_context_poll_int(MouseCursorServerContext* context)
{
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)context;
	UINT error = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(mouse_cursor);

	switch (mouse_cursor->state)
	{
		case MOUSE_CURSOR_INITIAL:
			error = mouse_cursor_server_open_channel(mouse_cursor);
			if (error)
				WLog_ERR(TAG, "mouse_cursor_server_open_channel failed with error %" PRIu32 "!",
				         error);
			else
				mouse_cursor->state = MOUSE_CURSOR_OPENED;
			break;
		case MOUSE_CURSOR_OPENED:
			error = mouse_cursor_process_message(mouse_cursor);
			break;
		default:
			break;
	}

	return error;
}

static HANDLE mouse_cursor_server_get_channel_handle(mouse_cursor_server* mouse_cursor)
{
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	HANDLE ChannelEvent = NULL;

	WINPR_ASSERT(mouse_cursor);

	if (WTSVirtualChannelQuery(mouse_cursor->mouse_cursor_channel, WTSVirtualEventHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		if (BytesReturned == sizeof(HANDLE))
			ChannelEvent = *(HANDLE*)buffer;

		WTSFreeMemory(buffer);
	}

	return ChannelEvent;
}

static DWORD WINAPI mouse_cursor_server_thread_func(LPVOID arg)
{
	DWORD nCount = 0;
	HANDLE events[2] = { 0 };
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)arg;
	UINT error = CHANNEL_RC_OK;
	DWORD status = 0;

	WINPR_ASSERT(mouse_cursor);

	nCount = 0;
	events[nCount++] = mouse_cursor->stopEvent;

	while ((error == CHANNEL_RC_OK) && (WaitForSingleObject(events[0], 0) != WAIT_OBJECT_0))
	{
		switch (mouse_cursor->state)
		{
			case MOUSE_CURSOR_INITIAL:
				error = mouse_cursor_server_context_poll_int(&mouse_cursor->context);
				if (error == CHANNEL_RC_OK)
				{
					events[1] = mouse_cursor_server_get_channel_handle(mouse_cursor);
					nCount = 2;
				}
				break;
			case MOUSE_CURSOR_OPENED:
				status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);
				switch (status)
				{
					case WAIT_OBJECT_0:
						break;
					case WAIT_OBJECT_0 + 1:
					case WAIT_TIMEOUT:
						error = mouse_cursor_server_context_poll_int(&mouse_cursor->context);
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

	(void)WTSVirtualChannelClose(mouse_cursor->mouse_cursor_channel);
	mouse_cursor->mouse_cursor_channel = NULL;

	if (error && mouse_cursor->context.rdpcontext)
		setChannelError(mouse_cursor->context.rdpcontext, error,
		                "mouse_cursor_server_thread_func reported an error");

	ExitThread(error);
	return error;
}

static UINT mouse_cursor_server_open(MouseCursorServerContext* context)
{
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)context;

	WINPR_ASSERT(mouse_cursor);

	if (!mouse_cursor->externalThread && (mouse_cursor->thread == NULL))
	{
		mouse_cursor->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (!mouse_cursor->stopEvent)
		{
			WLog_ERR(TAG, "CreateEvent failed!");
			return ERROR_INTERNAL_ERROR;
		}

		mouse_cursor->thread =
		    CreateThread(NULL, 0, mouse_cursor_server_thread_func, mouse_cursor, 0, NULL);
		if (!mouse_cursor->thread)
		{
			WLog_ERR(TAG, "CreateThread failed!");
			(void)CloseHandle(mouse_cursor->stopEvent);
			mouse_cursor->stopEvent = NULL;
			return ERROR_INTERNAL_ERROR;
		}
	}
	mouse_cursor->isOpened = TRUE;

	return CHANNEL_RC_OK;
}

static UINT mouse_cursor_server_close(MouseCursorServerContext* context)
{
	UINT error = CHANNEL_RC_OK;
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)context;

	WINPR_ASSERT(mouse_cursor);

	if (!mouse_cursor->externalThread && mouse_cursor->thread)
	{
		(void)SetEvent(mouse_cursor->stopEvent);

		if (WaitForSingleObject(mouse_cursor->thread, INFINITE) == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);
			return error;
		}

		(void)CloseHandle(mouse_cursor->thread);
		(void)CloseHandle(mouse_cursor->stopEvent);
		mouse_cursor->thread = NULL;
		mouse_cursor->stopEvent = NULL;
	}
	if (mouse_cursor->externalThread)
	{
		if (mouse_cursor->state != MOUSE_CURSOR_INITIAL)
		{
			(void)WTSVirtualChannelClose(mouse_cursor->mouse_cursor_channel);
			mouse_cursor->mouse_cursor_channel = NULL;
			mouse_cursor->state = MOUSE_CURSOR_INITIAL;
		}
	}
	mouse_cursor->isOpened = FALSE;

	return error;
}

static UINT mouse_cursor_server_context_poll(MouseCursorServerContext* context)
{
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)context;

	WINPR_ASSERT(mouse_cursor);

	if (!mouse_cursor->externalThread)
		return ERROR_INTERNAL_ERROR;

	return mouse_cursor_server_context_poll_int(context);
}

static BOOL mouse_cursor_server_context_handle(MouseCursorServerContext* context, HANDLE* handle)
{
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)context;

	WINPR_ASSERT(mouse_cursor);
	WINPR_ASSERT(handle);

	if (!mouse_cursor->externalThread)
		return FALSE;
	if (mouse_cursor->state == MOUSE_CURSOR_INITIAL)
		return FALSE;

	*handle = mouse_cursor_server_get_channel_handle(mouse_cursor);

	return TRUE;
}

static wStream* mouse_cursor_server_packet_new(size_t size, RDP_MOUSE_CURSOR_PDUTYPE pduType,
                                               const RDP_MOUSE_CURSOR_HEADER* header)
{
	wStream* s = NULL;

	/* Allocate what we need plus header bytes */
	s = Stream_New(NULL, size + RDPEMSC_HEADER_SIZE);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return NULL;
	}

	WINPR_ASSERT(pduType <= UINT8_MAX);
	Stream_Write_UINT8(s, (BYTE)pduType);

	WINPR_ASSERT(header->updateType <= UINT8_MAX);
	Stream_Write_UINT8(s, (BYTE)header->updateType);
	Stream_Write_UINT16(s, header->reserved);

	return s;
}

static UINT mouse_cursor_server_packet_send(MouseCursorServerContext* context, wStream* s)
{
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)context;
	UINT error = CHANNEL_RC_OK;
	ULONG written = 0;

	WINPR_ASSERT(mouse_cursor);
	WINPR_ASSERT(s);

	const size_t pos = Stream_GetPosition(s);

	WINPR_ASSERT(pos <= UINT32_MAX);
	if (!WTSVirtualChannelWrite(mouse_cursor->mouse_cursor_channel, Stream_BufferAs(s, char),
	                            (ULONG)pos, &written))
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

static UINT
mouse_cursor_server_send_sc_caps_confirm(MouseCursorServerContext* context,
                                         const RDP_MOUSE_CURSOR_CAPS_CONFIRM_PDU* capsConfirm)
{
	RDP_MOUSE_CURSOR_CAPSET* capsetHeader = NULL;
	RDP_MOUSE_CURSOR_PDUTYPE pduType = PDUTYPE_EMSC_RESERVED;
	size_t caps_size = 0;
	wStream* s = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(capsConfirm);

	capsetHeader = capsConfirm->capsSet;
	WINPR_ASSERT(capsetHeader);

	caps_size = 12;
	switch (capsetHeader->version)
	{
		case RDP_MOUSE_CURSOR_CAPVERSION_1:
			break;
		default:
			WINPR_ASSERT(FALSE);
			break;
	}

	pduType = PDUTYPE_SC_CAPS_CONFIRM;
	s = mouse_cursor_server_packet_new(caps_size, pduType, &capsConfirm->header);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	Stream_Write_UINT32(s, capsetHeader->signature);
	Stream_Write_UINT32(s, capsetHeader->version);
	Stream_Write_UINT32(s, capsetHeader->size);

	/* Write capsData */
	switch (capsetHeader->version)
	{
		case RDP_MOUSE_CURSOR_CAPVERSION_1:
			break;
		default:
			WINPR_ASSERT(FALSE);
			break;
	}

	return mouse_cursor_server_packet_send(context, s);
}

static void write_point16(wStream* s, const TS_POINT16* point16)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(point16);

	Stream_Write_UINT16(s, point16->xPos);
	Stream_Write_UINT16(s, point16->yPos);
}

static UINT mouse_cursor_server_send_sc_mouseptr_update(
    MouseCursorServerContext* context, const RDP_MOUSE_CURSOR_MOUSEPTR_UPDATE_PDU* mouseptrUpdate)
{
	TS_POINT16* position = NULL;
	TS_POINTERATTRIBUTE* pointerAttribute = NULL;
	TS_LARGEPOINTERATTRIBUTE* largePointerAttribute = NULL;
	RDP_MOUSE_CURSOR_PDUTYPE pduType = PDUTYPE_EMSC_RESERVED;
	size_t update_size = 0;
	wStream* s = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(mouseptrUpdate);

	position = mouseptrUpdate->position;
	pointerAttribute = mouseptrUpdate->pointerAttribute;
	largePointerAttribute = mouseptrUpdate->largePointerAttribute;

	switch (mouseptrUpdate->header.updateType)
	{
		case TS_UPDATETYPE_MOUSEPTR_SYSTEM_NULL:
		case TS_UPDATETYPE_MOUSEPTR_SYSTEM_DEFAULT:
			update_size = 0;
			break;
		case TS_UPDATETYPE_MOUSEPTR_POSITION:
			WINPR_ASSERT(position);
			update_size = 4;
			break;
		case TS_UPDATETYPE_MOUSEPTR_CACHED:
			WINPR_ASSERT(mouseptrUpdate->cachedPointerIndex);
			update_size = 2;
			break;
		case TS_UPDATETYPE_MOUSEPTR_POINTER:
			WINPR_ASSERT(pointerAttribute);
			update_size = 2 + 2 + 4 + 2 + 2 + 2 + 2;
			update_size += pointerAttribute->lengthAndMask;
			update_size += pointerAttribute->lengthXorMask;
			break;
		case TS_UPDATETYPE_MOUSEPTR_LARGE_POINTER:
			WINPR_ASSERT(largePointerAttribute);
			update_size = 2 + 2 + 4 + 2 + 2 + 4 + 4;
			update_size += largePointerAttribute->lengthAndMask;
			update_size += largePointerAttribute->lengthXorMask;
			break;
		default:
			WINPR_ASSERT(FALSE);
			break;
	}

	pduType = PDUTYPE_SC_MOUSEPTR_UPDATE;
	s = mouse_cursor_server_packet_new(update_size, pduType, &mouseptrUpdate->header);
	if (!s)
		return ERROR_NOT_ENOUGH_MEMORY;

	switch (mouseptrUpdate->header.updateType)
	{
		case TS_UPDATETYPE_MOUSEPTR_SYSTEM_NULL:
		case TS_UPDATETYPE_MOUSEPTR_SYSTEM_DEFAULT:
			break;
		case TS_UPDATETYPE_MOUSEPTR_POSITION:
			write_point16(s, position);
			break;
		case TS_UPDATETYPE_MOUSEPTR_CACHED:
			Stream_Write_UINT16(s, *mouseptrUpdate->cachedPointerIndex);
			break;
		case TS_UPDATETYPE_MOUSEPTR_POINTER:
			Stream_Write_UINT16(s, pointerAttribute->xorBpp);
			Stream_Write_UINT16(s, pointerAttribute->cacheIndex);
			write_point16(s, &pointerAttribute->hotSpot);
			Stream_Write_UINT16(s, pointerAttribute->width);
			Stream_Write_UINT16(s, pointerAttribute->height);
			Stream_Write_UINT16(s, pointerAttribute->lengthAndMask);
			Stream_Write_UINT16(s, pointerAttribute->lengthXorMask);
			Stream_Write(s, pointerAttribute->xorMaskData, pointerAttribute->lengthXorMask);
			Stream_Write(s, pointerAttribute->andMaskData, pointerAttribute->lengthAndMask);
			break;
		case TS_UPDATETYPE_MOUSEPTR_LARGE_POINTER:
			Stream_Write_UINT16(s, largePointerAttribute->xorBpp);
			Stream_Write_UINT16(s, largePointerAttribute->cacheIndex);
			write_point16(s, &largePointerAttribute->hotSpot);
			Stream_Write_UINT16(s, largePointerAttribute->width);
			Stream_Write_UINT16(s, largePointerAttribute->height);
			Stream_Write_UINT32(s, largePointerAttribute->lengthAndMask);
			Stream_Write_UINT32(s, largePointerAttribute->lengthXorMask);
			Stream_Write(s, largePointerAttribute->xorMaskData,
			             largePointerAttribute->lengthXorMask);
			Stream_Write(s, largePointerAttribute->andMaskData,
			             largePointerAttribute->lengthAndMask);
			break;
		default:
			WINPR_ASSERT(FALSE);
			break;
	}

	return mouse_cursor_server_packet_send(context, s);
}

MouseCursorServerContext* mouse_cursor_server_context_new(HANDLE vcm)
{
	mouse_cursor_server* mouse_cursor =
	    (mouse_cursor_server*)calloc(1, sizeof(mouse_cursor_server));

	if (!mouse_cursor)
		return NULL;

	mouse_cursor->context.vcm = vcm;
	mouse_cursor->context.Initialize = mouse_cursor_server_initialize;
	mouse_cursor->context.Open = mouse_cursor_server_open;
	mouse_cursor->context.Close = mouse_cursor_server_close;
	mouse_cursor->context.Poll = mouse_cursor_server_context_poll;
	mouse_cursor->context.ChannelHandle = mouse_cursor_server_context_handle;

	mouse_cursor->context.CapsConfirm = mouse_cursor_server_send_sc_caps_confirm;
	mouse_cursor->context.MouseptrUpdate = mouse_cursor_server_send_sc_mouseptr_update;

	mouse_cursor->buffer = Stream_New(NULL, 4096);
	if (!mouse_cursor->buffer)
		goto fail;

	return &mouse_cursor->context;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	mouse_cursor_server_context_free(&mouse_cursor->context);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void mouse_cursor_server_context_free(MouseCursorServerContext* context)
{
	mouse_cursor_server* mouse_cursor = (mouse_cursor_server*)context;

	if (mouse_cursor)
	{
		mouse_cursor_server_close(context);
		Stream_Free(mouse_cursor->buffer, TRUE);
	}

	free(mouse_cursor);
}
