/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Server Virtual Channel Interface
 *
 * Copyright 2011-2012 Vic Lee
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>

#include "wtsvc.h"

#define CREATE_REQUEST_PDU     0x01
#define DATA_FIRST_PDU         0x02
#define DATA_PDU               0x03
#define CLOSE_REQUEST_PDU      0x04
#define CAPABILITY_REQUEST_PDU 0x05

typedef struct wts_data_item
{
	uint16 channel_id;
	uint8* buffer;
	uint32 length;
} wts_data_item;

static void wts_data_item_free(wts_data_item* item)
{
	xfree(item->buffer);
	xfree(item);
}

static rdpPeerChannel* wts_get_dvc_channel_by_id(WTSVirtualChannelManager* vcm, uint32 ChannelId)
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

static void wts_read_drdynvc_capabilities_response(rdpPeerChannel* channel, uint32 length)
{
	uint16 Version;

	if (length < 3)
		return;
	stream_seek_uint8(channel->receive_data); /* Pad (1 byte) */
	stream_read_uint16(channel->receive_data, Version);

	DEBUG_DVC("Version: %d", Version);

	channel->vcm->drdynvc_state = DRDYNVC_STATE_READY;
}

static void wts_read_drdynvc_create_response(rdpPeerChannel* channel, STREAM* s, uint32 length)
{
	uint32 CreationStatus;

	if (length < 4)
		return;
	stream_read_uint32(s, CreationStatus);
	if ((sint32)CreationStatus < 0)
	{
		DEBUG_DVC("ChannelId %d creation failed (%d)", channel->channel_id, (sint32)CreationStatus);
		channel->dvc_open_state = DVC_OPEN_STATE_FAILED;
	}
	else
	{
		DEBUG_DVC("ChannelId %d creation succeeded", channel->channel_id);
		channel->dvc_open_state = DVC_OPEN_STATE_SUCCEEDED;
	}
}

static void wts_read_drdynvc_close_response(rdpPeerChannel* channel)
{
	DEBUG_DVC("ChannelId %d close response", channel->channel_id);
	channel->dvc_open_state = DVC_OPEN_STATE_CLOSED;
}

static void wts_read_drdynvc_pdu(rdpPeerChannel* channel)
{
	uint32 length;
	int value;
	int Cmd;
	int Sp;
	int cbChId;
	uint32 ChannelId;
	rdpPeerChannel* dvc;

	length = stream_get_pos(channel->receive_data);
	if (length < 1)
		return;
	stream_set_pos(channel->receive_data, 0);
	stream_read_uint8(channel->receive_data, value);
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
		switch (cbChId)
		{
			case 0:
				if (length < 1)
					return;
				stream_read_uint8(channel->receive_data, ChannelId);
				length--;
				break;

			case 1:
				if (length < 2)
					return;
				stream_read_uint16(channel->receive_data, ChannelId);
				length -= 2;
				break;

			default:
				if (length < 4)
					return;
				stream_read_uint32(channel->receive_data, ChannelId);
				length -= 4;
				break;
		}

		DEBUG_DVC("wts_read_drdynvc_pdu: Cmd %d ChannelId %d length %d", Cmd, ChannelId, length);
		dvc = wts_get_dvc_channel_by_id(channel->vcm, ChannelId);
		if (dvc)
		{
			switch (Cmd)
			{
				case CREATE_REQUEST_PDU:
					wts_read_drdynvc_create_response(dvc, channel->receive_data, length);
					break;

				case DATA_FIRST_PDU:
					break;

				case DATA_PDU:
					break;

				case CLOSE_REQUEST_PDU:
					wts_read_drdynvc_close_response(dvc);
					break;

				default:
					printf("wts_read_drdynvc_pdu: Cmd %d not recognized.\n", Cmd);
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
		printf("wts_read_drdynvc_pdu: received Cmd %d but channel is not ready.\n", Cmd);
	}
}

static void wts_write_drdynvc_header(STREAM *s, uint8 Cmd, uint32 ChannelId)
{
	if (ChannelId <= 0xFF)
	{
		stream_write_uint8(s, ((Cmd & 0x0F) << 4) | 0);
		stream_write_uint8(s, ChannelId);
	}
	else if (ChannelId <= 0xFFFF)
	{
		stream_write_uint8(s, ((Cmd & 0x0F) << 4) | 1);
		stream_write_uint16(s, ChannelId);
	}
	else
	{
		stream_write_uint8(s, ((Cmd & 0x0F) << 4) | 2);
		stream_write_uint32(s, ChannelId);
	}
}

static void wts_write_drdynvc_create_request(STREAM *s, uint32 ChannelId, const char *ChannelName)
{
	uint32 len;

	wts_write_drdynvc_header(s, CREATE_REQUEST_PDU, ChannelId);
	len = strlen(ChannelName) + 1;
	stream_check_size(s, len);
	stream_write(s, ChannelName, len);
}

static void WTSProcessChannelData(rdpPeerChannel* channel, int channelId, uint8* data, int size, int flags, int total_size)
{
	wts_data_item* item;

	if (flags & CHANNEL_FLAG_FIRST)
	{
		stream_set_pos(channel->receive_data, 0);
	}

	stream_check_size(channel->receive_data, size);
	stream_write(channel->receive_data, data, size);

	if (flags & CHANNEL_FLAG_LAST)
	{
		if (stream_get_length(channel->receive_data) != total_size)
		{
			printf("WTSProcessChannelData: read error\n");
		}
		if (channel == channel->vcm->drdynvc_channel)
		{
			wts_read_drdynvc_pdu(channel);
		}
		else
		{
			item = xnew(wts_data_item);
			item->length = stream_get_length(channel->receive_data);
			item->buffer = xmalloc(item->length);
			memcpy(item->buffer, stream_get_head(channel->receive_data), item->length);

			freerdp_mutex_lock(channel->mutex);
			list_enqueue(channel->receive_queue, item);
			freerdp_mutex_unlock(channel->mutex);

			wait_obj_set(channel->receive_event);
		}
		stream_set_pos(channel->receive_data, 0);
	}
}

static int WTSReceiveChannelData(freerdp_peer* client, int channelId, uint8* data, int size, int flags, int total_size)
{
	int i;
	boolean result = false;
	rdpPeerChannel* channel;

	for (i = 0; i < client->settings->num_channels; i++)
	{
		if (client->settings->channels[i].channel_id == channelId)
			break;
	}
	if (i < client->settings->num_channels)
	{
		channel = (rdpPeerChannel*) client->settings->channels[i].handle;
		if (channel != NULL)
		{
			WTSProcessChannelData(channel, channelId, data, size, flags, total_size);
			result = true;
		}
	}

	return result;
}

WTSVirtualChannelManager* WTSCreateVirtualChannelManager(freerdp_peer* client)
{
	WTSVirtualChannelManager* vcm;

	vcm = xnew(WTSVirtualChannelManager);
	if (vcm != NULL)
	{
		vcm->client = client;
		vcm->send_event = wait_obj_new();
		vcm->send_queue = list_new();
		vcm->mutex = freerdp_mutex_new();
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

		wait_obj_free(vcm->send_event);
		while ((item = (wts_data_item*) list_dequeue(vcm->send_queue)) != NULL)
		{
			wts_data_item_free(item);
		}
		list_free(vcm->send_queue);
		freerdp_mutex_free(vcm->mutex);
		xfree(vcm);
	}
}

void WTSVirtualChannelManagerGetFileDescriptor(WTSVirtualChannelManager* vcm,
	void** fds, int* fds_count)
{
	wait_obj_get_fds(vcm->send_event, fds, fds_count);
	if (vcm->drdynvc_channel)
	{
		wait_obj_get_fds(vcm->drdynvc_channel->receive_event, fds, fds_count);
	}
}

boolean WTSVirtualChannelManagerCheckFileDescriptor(WTSVirtualChannelManager* vcm)
{
	boolean result = true;
	wts_data_item* item;
	rdpPeerChannel* channel;
	uint32 dynvc_caps;

	if (vcm->drdynvc_state == DRDYNVC_STATE_NONE && vcm->client->activated)
	{
		/* Initialize drdynvc channel once and only once. */
		vcm->drdynvc_state = DRDYNVC_STATE_INITIALIZED;

		channel = WTSVirtualChannelOpenEx(vcm, "drdynvc", 0);
		if (channel)
		{
			vcm->drdynvc_channel = channel;
			dynvc_caps = 0x00010050; /* DYNVC_CAPS_VERSION1 (4 bytes) */
			WTSVirtualChannelWrite(channel, (uint8*) &dynvc_caps, sizeof(dynvc_caps), NULL);
		}
	}

	wait_obj_clear(vcm->send_event);

	freerdp_mutex_lock(vcm->mutex);
	while ((item = (wts_data_item*) list_dequeue(vcm->send_queue)) != NULL)
	{
		if (vcm->client->SendChannelData(vcm->client, item->channel_id, item->buffer, item->length) == false)
		{
			result = false;
		}
		wts_data_item_free(item);
		if (result == false)
			break;
	}
	freerdp_mutex_unlock(vcm->mutex);

	return result;
}

void* WTSVirtualChannelOpenEx(
	/* __in */ WTSVirtualChannelManager* vcm,
	/* __in */ const char* pVirtualName,
	/* __in */ uint32 flags)
{
	int i;
	int len;
	rdpPeerChannel* channel;
	freerdp_peer* client = vcm->client;
	STREAM* s;

	if ((flags & WTS_CHANNEL_OPTION_DYNAMIC) != 0)
	{
		if (vcm->drdynvc_channel == NULL || vcm->drdynvc_state != DRDYNVC_STATE_READY)
		{
			DEBUG_DVC("Dynamic virtual channel not ready.");
			return NULL;
		}

		channel = xnew(rdpPeerChannel);
		channel->vcm = vcm;
		channel->client = client;
		channel->channel_type = RDP_PEER_CHANNEL_TYPE_DVC;

		freerdp_mutex_lock(vcm->mutex);
		channel->channel_id = vcm->dvc_channel_id_seq++;
		list_enqueue(vcm->dvc_channel_list, channel);
		freerdp_mutex_unlock(vcm->mutex);

		s = stream_new(64);
		wts_write_drdynvc_create_request(s, channel->channel_id, pVirtualName);
		WTSVirtualChannelWrite(vcm->drdynvc_channel, stream_get_head(s), stream_get_length(s), NULL);
		stream_free(s);

		DEBUG_DVC("ChannelId %d.%s (total %d)", channel->channel_id, pVirtualName, list_size(vcm->dvc_channel_list));
	}
	else
	{
		len = strlen(pVirtualName);
		if (len > 8)
			return NULL;

		for (i = 0; i < client->settings->num_channels; i++)
		{
			if (client->settings->channels[i].joined &&
				strncmp(client->settings->channels[i].name, pVirtualName, len) == 0)
			{
				break;
			}
		}
		if (i >= client->settings->num_channels)
			return NULL;

		channel = (rdpPeerChannel*) client->settings->channels[i].handle;
		if (channel == NULL)
		{
			channel = xnew(rdpPeerChannel);
			channel->vcm = vcm;
			channel->client = client;
			channel->channel_id = client->settings->channels[i].channel_id;
			channel->index = i;
			channel->receive_data = stream_new(client->settings->vc_chunk_size);
			channel->channel_type = RDP_PEER_CHANNEL_TYPE_SVC;
			channel->receive_event = wait_obj_new();
			channel->receive_queue = list_new();
			channel->mutex = freerdp_mutex_new();

			client->settings->channels[i].handle = channel;
		}
	}

	return channel;
}

boolean WTSVirtualChannelQuery(
	/* __in */  void* hChannelHandle,
	/* __in */  WTS_VIRTUAL_CLASS WtsVirtualClass,
	/* __out */ void** ppBuffer,
	/* __out */ uint32* pBytesReturned)
{
	void* fds[10];
	int fds_count = 0;
	boolean result = false;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	switch (WtsVirtualClass)
	{
		case WTSVirtualFileHandle:
			wait_obj_get_fds(channel->receive_event, fds, &fds_count);
			*ppBuffer = xmalloc(sizeof(void*));
			memcpy(*ppBuffer, &fds[0], sizeof(void*));
			*pBytesReturned = sizeof(void*);
			result = true;
			break;

		default:
			break;
	}
	return result;
}

void WTSFreeMemory(
	/* __in */ void* pMemory)
{
	xfree(pMemory);
}

boolean WTSVirtualChannelRead(
	/* __in */  void* hChannelHandle,
	/* __in */  uint32 TimeOut,
	/* __out */ uint8* Buffer,
	/* __in */  uint32 BufferSize,
	/* __out */ uint32* pBytesRead)
{
	wts_data_item* item;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	item = (wts_data_item*) list_peek(channel->receive_queue);
	if (item == NULL)
	{
		wait_obj_clear(channel->receive_event);
		*pBytesRead = 0;
		return true;
	}
	*pBytesRead = item->length;
	if (item->length > BufferSize)
		return false;

	/* remove the first element (same as what we just peek) */
	freerdp_mutex_lock(channel->mutex);
	list_dequeue(channel->receive_queue);
	if (list_size(channel->receive_queue) == 0)
		wait_obj_clear(channel->receive_event);
	freerdp_mutex_unlock(channel->mutex);

	memcpy(Buffer, item->buffer, item->length);
	wts_data_item_free(item) ;

	return true;
}

boolean WTSVirtualChannelWrite(
	/* __in */  void* hChannelHandle,
	/* __in */  uint8* Buffer,
	/* __in */  uint32 Length,
	/* __out */ uint32* pBytesWritten)
{
	uint32 written = 0;
	wts_data_item* item;
	boolean result = false;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;
	WTSVirtualChannelManager* vcm;

	if (channel == NULL)
		return false;

	vcm = channel->vcm ;

	if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_SVC)
	{
		item = xnew(wts_data_item);
		item->channel_id = channel->channel_id;
		item->buffer = xmalloc(Length);
		item->length = Length;
		memcpy(item->buffer, Buffer, Length);

		freerdp_mutex_lock(vcm->mutex);
		list_enqueue(vcm->send_queue, item);
		freerdp_mutex_unlock(vcm->mutex);

		wait_obj_set(vcm->send_event);

		written = Length;
		result = true;
	}
	else
	{
		/* TODO: Send to DVC channel */
	}

	if (pBytesWritten != NULL)
		*pBytesWritten = written;
	return result;
}

boolean WTSVirtualChannelClose(
	/* __in */ void* hChannelHandle)
{
	STREAM* s;
	wts_data_item* item;
	WTSVirtualChannelManager* vcm;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	if (channel)
	{
		vcm = channel->vcm;

		if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_SVC)
		{
			if (channel->index < channel->client->settings->num_channels)
				channel->client->settings->channels[channel->index].handle = NULL;
		}
		else
		{
			freerdp_mutex_lock(vcm->mutex);
			list_remove(vcm->dvc_channel_list, channel);
			freerdp_mutex_unlock(vcm->mutex);

			if (channel->dvc_open_state == DVC_OPEN_STATE_SUCCEEDED)
			{
				s = stream_new(8);
				wts_write_drdynvc_header(s, CLOSE_REQUEST_PDU, channel->channel_id);
				WTSVirtualChannelWrite(vcm->drdynvc_channel, stream_get_head(s), stream_get_length(s), NULL);
				stream_free(s);
			}
		}
		if (channel->receive_data)
			stream_free(channel->receive_data);
		if (channel->receive_event)
			wait_obj_free(channel->receive_event);
		if (channel->receive_queue)
		{
			while ((item = (wts_data_item*) list_dequeue(channel->receive_queue)) != NULL)
			{
				wts_data_item_free(item);
			}
			list_free(channel->receive_queue);
		}
		if (channel->mutex)
			freerdp_mutex_free(channel->mutex);
		xfree(channel);
	}
	return true;
}
