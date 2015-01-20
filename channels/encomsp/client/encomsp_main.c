/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <freerdp/channels/log.h>
#include <freerdp/client/encomsp.h>

#include "encomsp_main.h"

static int encomsp_read_header(wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	if (Stream_GetRemainingLength(s) < ENCOMSP_ORDER_HEADER_SIZE)
		return -1;

	Stream_Read_UINT16(s, header->Type); /* Type (2 bytes) */
	Stream_Read_UINT16(s, header->Length); /* Length (2 bytes) */

	return 1;
}

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

	if (Stream_GetRemainingLength(s) < (size_t) (str->cchString * 2))
		return -1;

	Stream_Read(s, &(str->wString), (str->cchString * 2)); /* String (variable) */

	return 1;
}

EncomspClientContext* encomsp_get_client_interface(encomspPlugin* encomsp)
{
	EncomspClientContext* pInterface;
	pInterface = (EncomspClientContext*) encomsp->channelEntryPoints.pInterface;
	return pInterface;
}

int encomsp_virtual_channel_write(encomspPlugin* encomsp, wStream* s)
{
	UINT32 status = 0;

	if (!encomsp)
		return -1;

#if 0
	WLog_INFO(TAG, "EncomspWrite (%d)", Stream_Length(s));
	winpr_HexDump(Stream_Buffer(s), Stream_Length(s));
#endif

	status = encomsp->channelEntryPoints.pVirtualChannelWrite(encomsp->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_Length(s), s);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return -1;
	}

	return 1;
}

static int encomsp_recv_filter_updated_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_FILTER_UPDATED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 1)
		return -1;

	Stream_Read_UINT8(s, pdu.Flags); /* Flags (1 byte) */

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->FilterUpdated)
	{
		return context->FilterUpdated(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_application_created_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_APPLICATION_CREATED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 6)
		return -1;

	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */
	Stream_Read_UINT32(s, pdu.AppId); /* AppId (4 bytes) */

	if (encomsp_read_unicode_string(s, &(pdu.Name)) < 0)
		return -1;

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->ApplicationCreated)
	{
		return context->ApplicationCreated(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_application_removed_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_APPLICATION_REMOVED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, pdu.AppId); /* AppId (4 bytes) */

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->ApplicationRemoved)
	{
		return context->ApplicationRemoved(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_window_created_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_WINDOW_CREATED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 10)
		return -1;

	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */
	Stream_Read_UINT32(s, pdu.AppId); /* AppId (4 bytes) */
	Stream_Read_UINT32(s, pdu.WndId); /* WndId (4 bytes) */

	if (encomsp_read_unicode_string(s, &(pdu.Name)) < 0)
		return -1;

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->WindowCreated)
	{
		return context->WindowCreated(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_window_removed_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_WINDOW_REMOVED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, pdu.WndId); /* WndId (4 bytes) */

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->WindowRemoved)
	{
		return context->WindowRemoved(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_show_window_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_SHOW_WINDOW_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT32(s, pdu.WndId); /* WndId (4 bytes) */

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->ShowWindow)
	{
		return context->ShowWindow(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_participant_created_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_PARTICIPANT_CREATED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 10)
		return -1;

	Stream_Read_UINT32(s, pdu.ParticipantId); /* ParticipantId (4 bytes) */
	Stream_Read_UINT32(s, pdu.GroupId); /* GroupId (4 bytes) */
	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */

	if (encomsp_read_unicode_string(s, &(pdu.FriendlyName)) < 0)
		return -1;

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->ParticipantCreated)
	{
		return context->ParticipantCreated(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_participant_removed_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_PARTICIPANT_REMOVED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 12)
		return -1;

	Stream_Read_UINT32(s, pdu.ParticipantId); /* ParticipantId (4 bytes) */
	Stream_Read_UINT32(s, pdu.DiscType); /* DiscType (4 bytes) */
	Stream_Read_UINT32(s, pdu.DiscCode); /* DiscCode (4 bytes) */

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->ParticipantRemoved)
	{
		return context->ParticipantRemoved(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_change_participant_control_level_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	if (Stream_GetRemainingLength(s) < 6)
		return -1;

	Stream_Read_UINT16(s, pdu.Flags); /* Flags (2 bytes) */
	Stream_Read_UINT32(s, pdu.ParticipantId); /* ParticipantId (4 bytes) */

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->ChangeParticipantControlLevel)
	{
		return context->ChangeParticipantControlLevel(context, &pdu);
	}

	return 1;
}

static int encomsp_send_change_participant_control_level_pdu(EncomspClientContext* context, ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU* pdu)
{
	wStream* s;
	encomspPlugin* encomsp;

	encomsp = (encomspPlugin*) context->handle;

	pdu->Type = ODTYPE_PARTICIPANT_CTRL_CHANGED;
	pdu->Length = ENCOMSP_ORDER_HEADER_SIZE + 6;

	s = Stream_New(NULL, pdu->Length);

	encomsp_write_header(s, (ENCOMSP_ORDER_HEADER*) pdu);

	Stream_Write_UINT16(s, pdu->Flags); /* Flags (2 bytes) */
	Stream_Write_UINT32(s, pdu->ParticipantId); /* ParticipantId (4 bytes) */

	Stream_SealLength(s);

	encomsp_virtual_channel_write(encomsp, s);

	return 1;
}

static int encomsp_recv_graphics_stream_paused_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_GRAPHICS_STREAM_PAUSED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->GraphicsStreamPaused)
	{
		return context->GraphicsStreamPaused(context, &pdu);
	}

	return 1;
}

static int encomsp_recv_graphics_stream_resumed_pdu(encomspPlugin* encomsp, wStream* s, ENCOMSP_ORDER_HEADER* header)
{
	int beg, end;
	EncomspClientContext* context;
	ENCOMSP_GRAPHICS_STREAM_RESUMED_PDU pdu;

	context = encomsp_get_client_interface(encomsp);

	if (!context)
		return -1;

	beg = ((int) Stream_GetPosition(s)) - ENCOMSP_ORDER_HEADER_SIZE;

	CopyMemory(&pdu, header, sizeof(ENCOMSP_ORDER_HEADER));

	end = (int) Stream_GetPosition(s);

	if ((beg + header->Length) < end)
		return -1;

	if ((beg + header->Length) > end)
	{
		if (Stream_GetRemainingLength(s) < (size_t) ((beg + header->Length) - end))
			return -1;

		Stream_SetPosition(s, (beg + header->Length));
	}

	if (context->GraphicsStreamResumed)
	{
		return context->GraphicsStreamResumed(context, &pdu);
	}

	return 1;
}

static int encomsp_process_receive(encomspPlugin* encomsp, wStream* s)
{
	int status = 1;
	ENCOMSP_ORDER_HEADER header;

	while (Stream_GetRemainingLength(s) > 0)
	{
		if (encomsp_read_header(s, &header) < 0)
			return -1;

		//WLog_DBG(TAG, "EncomspReceive: Type: %d Length: %d", header.Type, header.Length);

		switch (header.Type)
		{
			case ODTYPE_FILTER_STATE_UPDATED:
				status = encomsp_recv_filter_updated_pdu(encomsp, s, &header);
				break;

			case ODTYPE_APP_REMOVED:
				status = encomsp_recv_application_removed_pdu(encomsp, s, &header);
				break;

			case ODTYPE_APP_CREATED:
				status = encomsp_recv_application_created_pdu(encomsp, s, &header);
				break;

			case ODTYPE_WND_REMOVED:
				status = encomsp_recv_window_removed_pdu(encomsp, s, &header);
				break;

			case ODTYPE_WND_CREATED:
				status = encomsp_recv_window_created_pdu(encomsp, s, &header);
				break;

			case ODTYPE_WND_SHOW:
				status = encomsp_recv_show_window_pdu(encomsp, s, &header);
				break;

			case ODTYPE_PARTICIPANT_REMOVED:
				status = encomsp_recv_participant_removed_pdu(encomsp, s, &header);
				break;

			case ODTYPE_PARTICIPANT_CREATED:
				status = encomsp_recv_participant_created_pdu(encomsp, s, &header);
				break;

			case ODTYPE_PARTICIPANT_CTRL_CHANGED:
				status = encomsp_recv_change_participant_control_level_pdu(encomsp, s, &header);
				break;

			case ODTYPE_GRAPHICS_STREAM_PAUSED:
				status = encomsp_recv_graphics_stream_paused_pdu(encomsp, s, &header);
				break;

			case ODTYPE_GRAPHICS_STREAM_RESUMED:
				status = encomsp_recv_graphics_stream_resumed_pdu(encomsp, s, &header);
				break;

			default:
				status = -1;
				break;
		}

		if (status < 0)
			return -1;
	}

	return status;
}

static void encomsp_process_connect(encomspPlugin* encomsp)
{

}

/****************************************************************************************/

static wListDictionary* g_InitHandles = NULL;
static wListDictionary* g_OpenHandles = NULL;

void encomsp_add_init_handle_data(void* pInitHandle, void* pUserData)
{
	if (!g_InitHandles)
		g_InitHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_InitHandles, pInitHandle, pUserData);
}

void* encomsp_get_init_handle_data(void* pInitHandle)
{
	void* pUserData = NULL;
	pUserData = ListDictionary_GetItemValue(g_InitHandles, pInitHandle);
	return pUserData;
}

void encomsp_remove_init_handle_data(void* pInitHandle)
{
	ListDictionary_Remove(g_InitHandles, pInitHandle);
	if (ListDictionary_Count(g_InitHandles) < 1)
	{
		ListDictionary_Free(g_InitHandles);
		g_InitHandles = NULL;
	}
}

void encomsp_add_open_handle_data(DWORD openHandle, void* pUserData)
{
	void* pOpenHandle = (void*) (size_t) openHandle;

	if (!g_OpenHandles)
		g_OpenHandles = ListDictionary_New(TRUE);

	ListDictionary_Add(g_OpenHandles, pOpenHandle, pUserData);
}

void* encomsp_get_open_handle_data(DWORD openHandle)
{
	void* pUserData = NULL;
	void* pOpenHandle = (void*) (size_t) openHandle;
	pUserData = ListDictionary_GetItemValue(g_OpenHandles, pOpenHandle);
	return pUserData;
}

void encomsp_remove_open_handle_data(DWORD openHandle)
{
	void* pOpenHandle = (void*) (size_t) openHandle;
	ListDictionary_Remove(g_OpenHandles, pOpenHandle);
	if (ListDictionary_Count(g_OpenHandles) < 1)
	{
		ListDictionary_Free(g_OpenHandles);
		g_OpenHandles = NULL;
	}
}

int encomsp_send(encomspPlugin* encomsp, wStream* s)
{
	UINT32 status = 0;
	encomspPlugin* plugin = (encomspPlugin*) encomsp;

	if (!plugin)
	{
		status = CHANNEL_RC_BAD_INIT_HANDLE;
	}
	else
	{
		status = plugin->channelEntryPoints.pVirtualChannelWrite(plugin->OpenHandle,
			Stream_Buffer(s), (UINT32) Stream_GetPosition(s), s);
	}

	if (status != CHANNEL_RC_OK)
	{
		Stream_Free(s, TRUE);
		WLog_ERR(TAG,  "VirtualChannelWrite failed with %s [%08X]",
				 WTSErrorToString(status), status);
	}

	return status;
}

static void encomsp_virtual_channel_event_data_received(encomspPlugin* encomsp,
		void* pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	wStream* data_in;

	if ((dataFlags & CHANNEL_FLAG_SUSPEND) || (dataFlags & CHANNEL_FLAG_RESUME))
	{
		return;
	}

	if (dataFlags & CHANNEL_FLAG_FIRST)
	{
		if (encomsp->data_in)
			Stream_Free(encomsp->data_in, TRUE);

		encomsp->data_in = Stream_New(NULL, totalLength);
	}

	data_in = encomsp->data_in;
	Stream_EnsureRemainingCapacity(data_in, (int) dataLength);
	Stream_Write(data_in, pData, dataLength);

	if (dataFlags & CHANNEL_FLAG_LAST)
	{
		if (Stream_Capacity(data_in) != Stream_GetPosition(data_in))
		{
			WLog_ERR(TAG,  "encomsp_plugin_process_received: read error");
		}

		encomsp->data_in = NULL;
		Stream_SealLength(data_in);
		Stream_SetPosition(data_in, 0);

		MessageQueue_Post(encomsp->queue, NULL, 0, (void*) data_in, NULL);
	}
}

static VOID VCAPITYPE encomsp_virtual_channel_open_event(DWORD openHandle, UINT event,
		LPVOID pData, UINT32 dataLength, UINT32 totalLength, UINT32 dataFlags)
{
	encomspPlugin* encomsp;

	encomsp = (encomspPlugin*) encomsp_get_open_handle_data(openHandle);

	if (!encomsp)
	{
		WLog_ERR(TAG,  "encomsp_virtual_channel_open_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_DATA_RECEIVED:
			encomsp_virtual_channel_event_data_received(encomsp, pData, dataLength, totalLength, dataFlags);
			break;

		case CHANNEL_EVENT_WRITE_COMPLETE:
			Stream_Free((wStream*) pData, TRUE);
			break;

		case CHANNEL_EVENT_USER:
			break;
	}
}

static void* encomsp_virtual_channel_client_thread(void* arg)
{
	wStream* data;
	wMessage message;
	encomspPlugin* encomsp = (encomspPlugin*) arg;

	encomsp_process_connect(encomsp);

	while (1)
	{
		if (!MessageQueue_Wait(encomsp->queue))
			break;

		if (MessageQueue_Peek(encomsp->queue, &message, TRUE))
		{
			if (message.id == WMQ_QUIT)
				break;

			if (message.id == 0)
			{
				data = (wStream*) message.wParam;
				encomsp_process_receive(encomsp, data);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void encomsp_virtual_channel_event_connected(encomspPlugin* encomsp, LPVOID pData, UINT32 dataLength)
{
	UINT32 status;

	status = encomsp->channelEntryPoints.pVirtualChannelOpen(encomsp->InitHandle,
		&encomsp->OpenHandle, encomsp->channelDef.name, encomsp_virtual_channel_open_event);

	encomsp_add_open_handle_data(encomsp->OpenHandle, encomsp);

	if (status != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG,  "pVirtualChannelOpen failed with %s [%08X]",
				 WTSErrorToString(status), status);
		return;
	}

	encomsp->queue = MessageQueue_New(NULL);

	encomsp->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) encomsp_virtual_channel_client_thread, (void*) encomsp, 0, NULL);
}

static void encomsp_virtual_channel_event_disconnected(encomspPlugin* encomsp)
{
	UINT rc;
	MessageQueue_PostQuit(encomsp->queue, 0);
	WaitForSingleObject(encomsp->thread, INFINITE);

	MessageQueue_Free(encomsp->queue);
	CloseHandle(encomsp->thread);

	encomsp->queue = NULL;
	encomsp->thread = NULL;

	rc = encomsp->channelEntryPoints.pVirtualChannelClose(encomsp->OpenHandle);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelClose failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
	}

	if (encomsp->data_in)
	{
		Stream_Free(encomsp->data_in, TRUE);
		encomsp->data_in = NULL;
	}

	encomsp_remove_open_handle_data(encomsp->OpenHandle);
}


static void encomsp_virtual_channel_event_terminated(encomspPlugin* encomsp)
{
	encomsp_remove_init_handle_data(encomsp->InitHandle);
	free(encomsp);
}

static VOID VCAPITYPE encomsp_virtual_channel_init_event(LPVOID pInitHandle, UINT event, LPVOID pData, UINT dataLength)
{
	encomspPlugin* encomsp;

	encomsp = (encomspPlugin*) encomsp_get_init_handle_data(pInitHandle);

	if (!encomsp)
	{
		WLog_ERR(TAG,  "encomsp_virtual_channel_init_event: error no match");
		return;
	}

	switch (event)
	{
		case CHANNEL_EVENT_CONNECTED:
			encomsp_virtual_channel_event_connected(encomsp, pData, dataLength);
			break;

		case CHANNEL_EVENT_DISCONNECTED:
			encomsp_virtual_channel_event_disconnected(encomsp);
			break;

		case CHANNEL_EVENT_TERMINATED:
			encomsp_virtual_channel_event_terminated(encomsp);
			break;
	}
}

/* encomsp is always built-in */
#define VirtualChannelEntry	encomsp_VirtualChannelEntry

BOOL VCAPITYPE VirtualChannelEntry(PCHANNEL_ENTRY_POINTS pEntryPoints)
{
	UINT rc;
	encomspPlugin* encomsp;
	EncomspClientContext* context;
	CHANNEL_ENTRY_POINTS_FREERDP* pEntryPointsEx;

	encomsp = (encomspPlugin*) calloc(1, sizeof(encomspPlugin));

	encomsp->channelDef.options =
			CHANNEL_OPTION_INITIALIZED |
			CHANNEL_OPTION_ENCRYPT_RDP |
			CHANNEL_OPTION_COMPRESS_RDP |
			CHANNEL_OPTION_SHOW_PROTOCOL;

	strcpy(encomsp->channelDef.name, "encomsp");

	pEntryPointsEx = (CHANNEL_ENTRY_POINTS_FREERDP*) pEntryPoints;

	if ((pEntryPointsEx->cbSize >= sizeof(CHANNEL_ENTRY_POINTS_FREERDP)) &&
			(pEntryPointsEx->MagicNumber == FREERDP_CHANNEL_MAGIC_NUMBER))
	{
		context = (EncomspClientContext*) calloc(1, sizeof(EncomspClientContext));

		context->handle = (void*) encomsp;

		context->FilterUpdated = NULL;
		context->ApplicationCreated = NULL;
		context->ApplicationRemoved = NULL;
		context->WindowCreated = NULL;
		context->WindowRemoved = NULL;
		context->ShowWindow = NULL;
		context->ParticipantCreated = NULL;
		context->ParticipantRemoved = NULL;
		context->ChangeParticipantControlLevel = encomsp_send_change_participant_control_level_pdu;
		context->GraphicsStreamPaused = NULL;
		context->GraphicsStreamResumed = NULL;

		*(pEntryPointsEx->ppInterface) = (void*) context;
		encomsp->context = context;
	}

	CopyMemory(&(encomsp->channelEntryPoints), pEntryPoints, sizeof(CHANNEL_ENTRY_POINTS_FREERDP));

	rc = encomsp->channelEntryPoints.pVirtualChannelInit(&encomsp->InitHandle,
		&encomsp->channelDef, 1, VIRTUAL_CHANNEL_VERSION_WIN2000, encomsp_virtual_channel_init_event);
	if (CHANNEL_RC_OK != rc)
	{
		WLog_ERR(TAG, "pVirtualChannelInit failed with %s [%08X]",
				 WTSErrorToString(rc), rc);
		free(encomsp);
		return -1;
	}
	encomsp->channelEntryPoints.pInterface = *(encomsp->channelEntryPoints.ppInterface);
	encomsp->channelEntryPoints.ppInterface = &(encomsp->channelEntryPoints.pInterface);

	encomsp_add_init_handle_data(encomsp->InitHandle, (void*) encomsp);

	return 1;
}
