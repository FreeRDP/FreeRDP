/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Channels
 *
 * Copyright 2011-2012 Vic Lee
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/constants.h>
#include <freerdp/server/channels.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#include "channels.h"

/**
 * this is a workaround to force importing symbols
 * will need to fix that later on cleanly
 */

#include <freerdp/server/audin.h>
#include <freerdp/server/rdpsnd.h>
#include <freerdp/server/cliprdr.h>
#include <freerdp/server/rdpdr.h>
#include <freerdp/server/drdynvc.h>

void freerdp_channels_dummy()
{
	audin_server_context_new(NULL);
	audin_server_context_free(NULL);

	rdpsnd_server_context_new(NULL);
	rdpsnd_server_context_free(NULL);

	cliprdr_server_context_new(NULL);
	cliprdr_server_context_free(NULL);

	rdpdr_server_context_new(NULL);
	rdpdr_server_context_free(NULL);

	drdynvc_server_context_new(NULL);
	drdynvc_server_context_free(NULL);
}

/**
 * end of ugly symbols import workaround
 */

#define CREATE_REQUEST_PDU			0x01
#define DATA_FIRST_PDU				0x02
#define DATA_PDU				0x03
#define CLOSE_REQUEST_PDU			0x04
#define CAPABILITY_REQUEST_PDU			0x05

typedef struct wts_data_item
{
	UINT16 channel_id;
	BYTE* buffer;
	UINT32 length;
} wts_data_item;

static void wts_data_item_free(wts_data_item* item)
{
	free(item->buffer);
	free(item);
}

void* freerdp_channels_server_find_static_entry(const char* name, const char* entry)
{
	return NULL;
}

static rdpPeerChannel* wts_get_dvc_channel_by_id(WTSVirtualChannelManager* vcm, UINT32 ChannelId)
{
	LIST_ITEM* item;
	rdpPeerChannel* channel = NULL;

	for (item = vcm->dvc_channel_list->head; item; item = item->next)
	{
		channel = (rdpPeerChannel*) item->data;

		if (channel->channel_id == ChannelId)
			break;
	}

	return channel;
}

static void wts_queue_receive_data(rdpPeerChannel* channel, const BYTE* buffer, UINT32 length)
{
	wts_data_item* item;

	item = (wts_data_item*) malloc(sizeof(wts_data_item));
	ZeroMemory(item, sizeof(wts_data_item));

	item->length = length;
	item->buffer = malloc(length);
	CopyMemory(item->buffer, buffer, length);

	WaitForSingleObject(channel->mutex, INFINITE);
	list_enqueue(channel->receive_queue, item);
	ReleaseMutex(channel->mutex);

	SetEvent(channel->receive_event);
}

static void wts_queue_send_item(rdpPeerChannel* channel, wts_data_item* item)
{
	WTSVirtualChannelManager* vcm;

	vcm = channel->vcm;

	item->channel_id = channel->channel_id;

	WaitForSingleObject(vcm->mutex, INFINITE);
	list_enqueue(vcm->send_queue, item);
	ReleaseMutex(vcm->mutex);

	SetEvent(vcm->send_event);
}

static int wts_read_variable_uint(wStream* s, int cbLen, UINT32* val)
{
	switch (cbLen)
	{
		case 0:
			if (Stream_GetRemainingLength(s) < 1)
				return 0;
			Stream_Read_UINT8(s, *val);
			return 1;

		case 1:
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			Stream_Read_UINT16(s, *val);
			return 2;

		default:
			if (Stream_GetRemainingLength(s) < 4)
				return 0;
			Stream_Read_UINT32(s, *val);
			return 4;
	}
}

static void wts_read_drdynvc_capabilities_response(rdpPeerChannel* channel, UINT32 length)
{
	UINT16 Version;

	if (length < 3)
		return;

	Stream_Seek_UINT8(channel->receive_data); /* Pad (1 byte) */
	Stream_Read_UINT16(channel->receive_data, Version);

	DEBUG_DVC("Version: %d", Version);

	channel->vcm->drdynvc_state = DRDYNVC_STATE_READY;
}

static void wts_read_drdynvc_create_response(rdpPeerChannel* channel, wStream* s, UINT32 length)
{
	UINT32 CreationStatus;

	if (length < 4)
		return;

	Stream_Read_UINT32(s, CreationStatus);

	if ((INT32) CreationStatus < 0)
	{
		DEBUG_DVC("ChannelId %d creation failed (%d)", channel->channel_id, (INT32) CreationStatus);
		channel->dvc_open_state = DVC_OPEN_STATE_FAILED;
	}
	else
	{
		DEBUG_DVC("ChannelId %d creation succeeded", channel->channel_id);
		channel->dvc_open_state = DVC_OPEN_STATE_SUCCEEDED;
	}

	SetEvent(channel->receive_event);
}

static void wts_read_drdynvc_data_first(rdpPeerChannel* channel, wStream* s, int cbLen, UINT32 length)
{
	int value;

	value = wts_read_variable_uint(s, cbLen, &channel->dvc_total_length);

	if (value == 0)
		return;

	length -= value;

	if (length > channel->dvc_total_length)
		return;

	Stream_SetPosition(channel->receive_data, 0);
	Stream_EnsureRemainingCapacity(channel->receive_data, (int) channel->dvc_total_length);
	Stream_Write(channel->receive_data, Stream_Pointer(s), length);
}

static void wts_read_drdynvc_data(rdpPeerChannel* channel, wStream* s, UINT32 length)
{
	if (channel->dvc_total_length > 0)
	{
		if (Stream_GetPosition(channel->receive_data) + length > channel->dvc_total_length)
		{
			channel->dvc_total_length = 0;
			fprintf(stderr, "wts_read_drdynvc_data: incorrect fragment data, discarded.\n");
			return;
		}

		Stream_Write(channel->receive_data, Stream_Pointer(s), length);

		if (Stream_GetPosition(channel->receive_data) >= (int) channel->dvc_total_length)
		{
			wts_queue_receive_data(channel, Stream_Buffer(channel->receive_data), channel->dvc_total_length);
			channel->dvc_total_length = 0;
		}
	}
	else
	{
		wts_queue_receive_data(channel, Stream_Pointer(s), length);
	}
}

static void wts_read_drdynvc_close_response(rdpPeerChannel* channel)
{
	DEBUG_DVC("ChannelId %d close response", channel->channel_id);
	channel->dvc_open_state = DVC_OPEN_STATE_CLOSED;
}

static void wts_read_drdynvc_pdu(rdpPeerChannel* channel)
{
	UINT32 length;
	int value;
	int Cmd;
	int Sp;
	int cbChId;
	UINT32 ChannelId;
	rdpPeerChannel* dvc;

	length = Stream_GetPosition(channel->receive_data);

	if (length < 1)
		return;

	Stream_SetPosition(channel->receive_data, 0);
	Stream_Read_UINT8(channel->receive_data, value);

	length--;
	Cmd = (value & 0xf0) >> 4;
	Sp = (value & 0x0c) >> 2;
	cbChId = (value & 0x03) >> 0;

	if (Cmd == CAPABILITY_REQUEST_PDU)
	{
		wts_read_drdynvc_capabilities_response(channel, length);
	}
	else if (channel->vcm->drdynvc_state == DRDYNVC_STATE_READY)
	{
		value = wts_read_variable_uint(channel->receive_data, cbChId, &ChannelId);

		if (value == 0)
			return;

		length -= value;

		DEBUG_DVC("Cmd %d ChannelId %d length %d", Cmd, ChannelId, length);
		dvc = wts_get_dvc_channel_by_id(channel->vcm, ChannelId);

		if (dvc)
		{
			switch (Cmd)
			{
				case CREATE_REQUEST_PDU:
					wts_read_drdynvc_create_response(dvc, channel->receive_data, length);
					break;

				case DATA_FIRST_PDU:
					wts_read_drdynvc_data_first(dvc, channel->receive_data, Sp, length);
					break;

				case DATA_PDU:
					wts_read_drdynvc_data(dvc, channel->receive_data, length);
					break;

				case CLOSE_REQUEST_PDU:
					wts_read_drdynvc_close_response(dvc);
					break;

				default:
					fprintf(stderr, "wts_read_drdynvc_pdu: Cmd %d not recognized.\n", Cmd);
					break;
			}
		}
		else
		{
			DEBUG_DVC("ChannelId %d not exists.", ChannelId);
		}
	}
	else
	{
		fprintf(stderr, "wts_read_drdynvc_pdu: received Cmd %d but channel is not ready.\n", Cmd);
	}
}

static int wts_write_variable_uint(wStream* stream, UINT32 val)
{
	int cb;

	if (val <= 0xFF)
	{
		cb = 0;
		Stream_Write_UINT8(stream, val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		Stream_Write_UINT16(stream, val);
	}
	else
	{
		cb = 3;
		Stream_Write_UINT32(stream, val);
	}

	return cb;
}

static void wts_write_drdynvc_header(wStream *s, BYTE Cmd, UINT32 ChannelId)
{
	BYTE* bm;
	int cbChId;

	Stream_GetPointer(s, bm);
	Stream_Seek_UINT8(s);
	cbChId = wts_write_variable_uint(s, ChannelId);
	*bm = ((Cmd & 0x0F) << 4) | cbChId;
}

static void wts_write_drdynvc_create_request(wStream *s, UINT32 ChannelId, const char *ChannelName)
{
	UINT32 len;

	wts_write_drdynvc_header(s, CREATE_REQUEST_PDU, ChannelId);
	len = strlen(ChannelName) + 1;
	Stream_EnsureRemainingCapacity(s, (int) len);
	Stream_Write(s, ChannelName, len);
}

static void WTSProcessChannelData(rdpPeerChannel* channel, int channelId, BYTE* data, int size, int flags, int total_size)
{
	if (flags & CHANNEL_FLAG_FIRST)
	{
		Stream_SetPosition(channel->receive_data, 0);
	}

	Stream_EnsureRemainingCapacity(channel->receive_data, size);
	Stream_Write(channel->receive_data, data, size);

	if (flags & CHANNEL_FLAG_LAST)
	{
		if (Stream_GetPosition(channel->receive_data) != total_size)
		{
			fprintf(stderr, "WTSProcessChannelData: read error\n");
		}
		if (channel == channel->vcm->drdynvc_channel)
		{
			wts_read_drdynvc_pdu(channel);
		}
		else
		{
			wts_queue_receive_data(channel, Stream_Buffer(channel->receive_data), Stream_GetPosition(channel->receive_data));
		}
		Stream_SetPosition(channel->receive_data, 0);
	}
}

static int WTSReceiveChannelData(freerdp_peer* client, int channelId, BYTE* data, int size, int flags, int total_size)
{
	int i;
	BOOL result = FALSE;
	rdpPeerChannel* channel;

	for (i = 0; i < client->settings->ChannelCount; i++)
	{
		if (client->settings->ChannelDefArray[i].ChannelId == channelId)
			break;
	}

	if (i < client->settings->ChannelCount)
	{
		channel = (rdpPeerChannel*) client->settings->ChannelDefArray[i].handle;

		if (channel != NULL)
		{
			WTSProcessChannelData(channel, channelId, data, size, flags, total_size);
			result = TRUE;
		}
	}

	return result;
}

WTSVirtualChannelManager* WTSCreateVirtualChannelManager(freerdp_peer* client)
{
	WTSVirtualChannelManager* vcm;

	vcm = (WTSVirtualChannelManager*) malloc(sizeof(WTSVirtualChannelManager));

	if (vcm)
	{
		ZeroMemory(vcm, sizeof(WTSVirtualChannelManager));

		vcm->client = client;
		vcm->send_event = CreateEvent(NULL, TRUE, FALSE, NULL);
		vcm->send_queue = list_new();
		vcm->mutex = CreateMutex(NULL, FALSE, NULL);
		vcm->dvc_channel_id_seq = 1;
		vcm->dvc_channel_list = list_new();

		client->ReceiveChannelData = WTSReceiveChannelData;
	}

	return vcm;
}

void WTSDestroyVirtualChannelManager(WTSVirtualChannelManager* vcm)
{
	wts_data_item* item;
	rdpPeerChannel* channel;

	if (vcm != NULL)
	{
		while ((channel = (rdpPeerChannel*) list_dequeue(vcm->dvc_channel_list)) != NULL)
		{
			WTSVirtualChannelClose(channel);
		}

		list_free(vcm->dvc_channel_list);

		if (vcm->drdynvc_channel != NULL)
		{
			WTSVirtualChannelClose(vcm->drdynvc_channel);
			vcm->drdynvc_channel = NULL;
		}

		CloseHandle(vcm->send_event);

		while ((item = (wts_data_item*) list_dequeue(vcm->send_queue)) != NULL)
		{
			wts_data_item_free(item);
		}

		list_free(vcm->send_queue);
		CloseHandle(vcm->mutex);
		free(vcm);
	}
}

void WTSVirtualChannelManagerGetFileDescriptor(WTSVirtualChannelManager* vcm, void** fds, int* fds_count)
{
	void* fd;

	fd = GetEventWaitObject(vcm->send_event);

	if (fd)
	{
		fds[*fds_count] = fd;
		(*fds_count)++;
	}

	if (vcm->drdynvc_channel)
	{
		fd = GetEventWaitObject(vcm->drdynvc_channel->receive_event);

		if (fd)
		{
			fds[*fds_count] = fd;
			(*fds_count)++;
		}
	}
}

BOOL WTSVirtualChannelManagerCheckFileDescriptor(WTSVirtualChannelManager* vcm)
{
	BOOL result = TRUE;
	wts_data_item* item;
	rdpPeerChannel* channel;
	UINT32 dynvc_caps;

	if (vcm->drdynvc_state == DRDYNVC_STATE_NONE && vcm->client->activated)
	{
		/* Initialize drdynvc channel once and only once. */
		vcm->drdynvc_state = DRDYNVC_STATE_INITIALIZED;

		channel = WTSVirtualChannelManagerOpenEx(vcm, "drdynvc", 0);

		if (channel)
		{
			vcm->drdynvc_channel = channel;
			dynvc_caps = 0x00010050; /* DYNVC_CAPS_VERSION1 (4 bytes) */
			WTSVirtualChannelWrite(channel, (PCHAR) &dynvc_caps, sizeof(dynvc_caps), NULL);
		}
	}

	ResetEvent(vcm->send_event);

	WaitForSingleObject(vcm->mutex, INFINITE);

	while ((item = (wts_data_item*) list_dequeue(vcm->send_queue)) != NULL)
	{
		if (vcm->client->SendChannelData(vcm->client, item->channel_id, item->buffer, item->length) == FALSE)
		{
			result = FALSE;
		}

		wts_data_item_free(item);

		if (result == FALSE)
			break;
	}

	ReleaseMutex(vcm->mutex);

	return result;
}

HANDLE WTSVirtualChannelManagerGetEventHandle(WTSVirtualChannelManager* vcm)
{
	return vcm->send_event;
}

HANDLE WTSVirtualChannelManagerOpenEx(WTSVirtualChannelManager* vcm, LPSTR pVirtualName, DWORD flags)
{
	int i;
	int len;
	wStream* s;
	rdpPeerChannel* channel;
	freerdp_peer* client = vcm->client;

	if ((flags & WTS_CHANNEL_OPTION_DYNAMIC) != 0)
	{
		for (i = 0; i < client->settings->ChannelCount; i++)
		{
			if (client->settings->ChannelDefArray[i].joined &&
				strncmp(client->settings->ChannelDefArray[i].Name, "drdynvc", 7) == 0)
			{
				break;
			}
		}
		if (i >= client->settings->ChannelCount)
		{
			DEBUG_DVC("Dynamic virtual channel not registered.");
			SetLastError(ERROR_NOT_FOUND);
			return NULL;
		}

		if (vcm->drdynvc_channel == NULL || vcm->drdynvc_state != DRDYNVC_STATE_READY)
		{
			DEBUG_DVC("Dynamic virtual channel not ready.");
			SetLastError(ERROR_NOT_READY);
			return NULL;
		}

		channel = (rdpPeerChannel*) malloc(sizeof(rdpPeerChannel));
		ZeroMemory(channel, sizeof(rdpPeerChannel));

		channel->vcm = vcm;
		channel->client = client;
		channel->channel_type = RDP_PEER_CHANNEL_TYPE_DVC;
		channel->receive_data = Stream_New(NULL, client->settings->VirtualChannelChunkSize);
		channel->receive_event = CreateEvent(NULL, TRUE, FALSE, NULL);
		channel->receive_queue = list_new();
		channel->mutex = CreateMutex(NULL, FALSE, NULL);

		WaitForSingleObject(vcm->mutex, INFINITE);
		channel->channel_id = vcm->dvc_channel_id_seq++;
		list_enqueue(vcm->dvc_channel_list, channel);
		ReleaseMutex(vcm->mutex);

		s = Stream_New(NULL, 64);
		wts_write_drdynvc_create_request(s, channel->channel_id, pVirtualName);
		WTSVirtualChannelWrite(vcm->drdynvc_channel, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);
		Stream_Free(s, TRUE);

		DEBUG_DVC("ChannelId %d.%s (total %d)", channel->channel_id, pVirtualName, list_size(vcm->dvc_channel_list));
	}
	else
	{
		len = strlen(pVirtualName);

		if (len > 8)
		{
			SetLastError(ERROR_NOT_FOUND);
			return NULL;
		}

		for (i = 0; i < client->settings->ChannelCount; i++)
		{
			if (client->settings->ChannelDefArray[i].joined &&
				strncmp(client->settings->ChannelDefArray[i].Name, pVirtualName, len) == 0)
			{
				break;
			}
		}

		if (i >= client->settings->ChannelCount)
		{
			SetLastError(ERROR_NOT_FOUND);
			return NULL;
		}

		channel = (rdpPeerChannel*) client->settings->ChannelDefArray[i].handle;

		if (channel == NULL)
		{
			channel = (rdpPeerChannel*) malloc(sizeof(rdpPeerChannel));
			ZeroMemory(channel, sizeof(rdpPeerChannel));

			channel->vcm = vcm;
			channel->client = client;
			channel->channel_id = client->settings->ChannelDefArray[i].ChannelId;
			channel->index = i;
			channel->channel_type = RDP_PEER_CHANNEL_TYPE_SVC;
			channel->receive_data = Stream_New(NULL, client->settings->VirtualChannelChunkSize);
			channel->receive_event = CreateEvent(NULL, TRUE, FALSE, NULL);
			channel->receive_queue = list_new();
			channel->mutex = CreateMutex(NULL, FALSE, NULL);

			client->settings->ChannelDefArray[i].handle = channel;
		}
	}

	return channel;
}

BOOL WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned)
{
	void* pfd;
	BOOL bval;
	void* fds[10];
	int fds_count = 0;
	BOOL result = FALSE;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;
	ZeroMemory(fds, sizeof(fds));

	switch (WtsVirtualClass)
	{
		case WTSVirtualFileHandle:

			pfd = GetEventWaitObject(channel->receive_event);

			if (pfd)
			{
				fds[fds_count] = pfd;
				(fds_count)++;
			}

			*ppBuffer = malloc(sizeof(void*));
			CopyMemory(*ppBuffer, &fds[0], sizeof(void*));
			*pBytesReturned = sizeof(void*);
			result = TRUE;
			break;

		case WTSVirtualEventHandle:
			*ppBuffer = malloc(sizeof(HANDLE));
			CopyMemory(*ppBuffer, &(channel->receive_event), sizeof(HANDLE));
			*pBytesReturned = sizeof(void*);
			result = TRUE;
			break;

		case WTSVirtualChannelReady:
			if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_SVC)
			{
				bval = TRUE;
				result = TRUE;
			}
			else
			{
				switch (channel->dvc_open_state)
				{
					case DVC_OPEN_STATE_NONE:
						bval = FALSE;
						result = TRUE;
						break;

					case DVC_OPEN_STATE_SUCCEEDED:
						bval = TRUE;
						result = TRUE;
						break;

					default:
						bval = FALSE;
						result = FALSE;
						break;
				}
			}

			*ppBuffer = malloc(sizeof(BOOL));
			CopyMemory(*ppBuffer, &bval, sizeof(BOOL));
			*pBytesReturned = sizeof(BOOL);
			break;

		default:
			break;
	}
	return result;
}

VOID WTSFreeMemory(PVOID pMemory)
{
	free(pMemory);
}

BOOL WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead)
{
	wts_data_item* item;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	item = (wts_data_item*) list_peek(channel->receive_queue);

	if (!item)
	{
		ResetEvent(channel->receive_event);
		*pBytesRead = 0;
		return TRUE;
	}

	*pBytesRead = item->length;

	if (item->length > BufferSize)
		return FALSE;

	/* remove the first element (same as what we just peek) */
	WaitForSingleObject(channel->mutex, INFINITE);
	list_dequeue(channel->receive_queue);

	if (list_size(channel->receive_queue) == 0)
		ResetEvent(channel->receive_event);

	ReleaseMutex(channel->mutex);

	CopyMemory(Buffer, item->buffer, item->length);
	wts_data_item_free(item);

	return TRUE;
}

BOOL WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten)
{
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;
	wts_data_item* item;
	wStream* s;
	int cbLen;
	int cbChId;
	int first;
	UINT32 written;

	if (!channel)
		return FALSE;

	if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_SVC)
	{
		item = (wts_data_item*) malloc(sizeof(wts_data_item));
		ZeroMemory(item, sizeof(wts_data_item));

		item->buffer = malloc(Length);
		item->length = Length;
		CopyMemory(item->buffer, Buffer, Length);

		wts_queue_send_item(channel, item);
	}
	else if (channel->vcm->drdynvc_channel == NULL || channel->vcm->drdynvc_state != DRDYNVC_STATE_READY)
	{
		DEBUG_DVC("drdynvc not ready");
		return FALSE;
	}
	else
	{
		first = TRUE;

		while (Length > 0)
		{
			item = (wts_data_item*) malloc(sizeof(wts_data_item));
			ZeroMemory(item, sizeof(wts_data_item));

			s = Stream_New(NULL, channel->client->settings->VirtualChannelChunkSize);
			item->buffer = Stream_Buffer(s);

			Stream_Seek_UINT8(s);
			cbChId = wts_write_variable_uint(s, channel->channel_id);

			if (first && (Length > (UINT32) Stream_GetRemainingLength(s)))
			{
				cbLen = wts_write_variable_uint(s, Length);
				item->buffer[0] = (DATA_FIRST_PDU << 4) | (cbLen << 2) | cbChId;
			}
			else
			{
				item->buffer[0] = (DATA_PDU << 4) | cbChId;
			}

			first = FALSE;
			written = Stream_GetRemainingLength(s);

			if (written > Length)
				written = Length;

			Stream_Write(s, Buffer, written);
			item->length = Stream_GetPosition(s);
			Stream_Free(s, FALSE);

			Length -= written;
			Buffer += written;

			wts_queue_send_item(channel->vcm->drdynvc_channel, item);
		}
	}

	if (pBytesWritten != NULL)
		*pBytesWritten = Length;

	return TRUE;
}

BOOL WTSVirtualChannelClose(HANDLE hChannelHandle)
{
	wStream* s;
	wts_data_item* item;
	WTSVirtualChannelManager* vcm;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	if (channel)
	{
		vcm = channel->vcm;

		if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_SVC)
		{
			if (channel->index < channel->client->settings->ChannelCount)
				channel->client->settings->ChannelDefArray[channel->index].handle = NULL;
		}
		else
		{
			WaitForSingleObject(vcm->mutex, INFINITE);
			list_remove(vcm->dvc_channel_list, channel);
			ReleaseMutex(vcm->mutex);

			if (channel->dvc_open_state == DVC_OPEN_STATE_SUCCEEDED)
			{
				s = Stream_New(NULL, 8);
				wts_write_drdynvc_header(s, CLOSE_REQUEST_PDU, channel->channel_id);
				WTSVirtualChannelWrite(vcm->drdynvc_channel, (PCHAR) Stream_Buffer(s), Stream_GetPosition(s), NULL);
				Stream_Free(s, TRUE);
			}
		}

		if (channel->receive_data)
			Stream_Free(channel->receive_data, TRUE);

		if (channel->receive_event)
			CloseHandle(channel->receive_event);

		if (channel->receive_queue)
		{
			while ((item = (wts_data_item*) list_dequeue(channel->receive_queue)) != NULL)
			{
				wts_data_item_free(item);
			}

			list_free(channel->receive_queue);
		}

		if (channel->mutex)
			CloseHandle(channel->mutex);

		free(channel);
	}

	return TRUE;
}

