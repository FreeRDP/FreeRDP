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
		if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_DVC)
		{
			/* TODO: Receive DVC channel data */
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

		client->ReceiveChannelData = WTSReceiveChannelData;
	}

	return vcm;
}

void WTSDestroyVirtualChannelManager(WTSVirtualChannelManager* vcm)
{
	wts_data_item* item;

	if (vcm != NULL)
	{
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
}

boolean WTSVirtualChannelManagerCheckFileDescriptor(WTSVirtualChannelManager* vcm)
{
	boolean result = true;
	wts_data_item* item;

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
	const char* channel_name;
	freerdp_peer* client = vcm->client;

	channel_name = ((flags & WTS_CHANNEL_OPTION_DYNAMIC) != 0 ? "drdynvc" : pVirtualName);

	len = strlen(channel_name);
	if (len > 8)
		return NULL;

	for (i = 0; i < client->settings->num_channels; i++)
	{
		if (client->settings->channels[i].joined &&
			strncmp(client->settings->channels[i].name, channel_name, len) == 0)
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
		if ((flags & WTS_CHANNEL_OPTION_DYNAMIC) != 0)
		{
			channel->channel_type = RDP_PEER_CHANNEL_TYPE_DVC;
			vcm->drdynvc_channel = channel;
		}
		else
		{
			channel->channel_type = RDP_PEER_CHANNEL_TYPE_SVC;
			channel->receive_event = wait_obj_new();
			channel->receive_queue = list_new();
			channel->mutex = freerdp_mutex_new();
		}

		client->settings->channels[i].handle = channel;
	}
	if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_DVC)
	{
		/* TODO: do DVC channel initialization here using pVirtualName */
		/* A sub-channel should be created and returned, instead of using the main drdynvc channel */
		/* Set channel->index to num_channels */
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
	if (channel->receive_queue->head == NULL)
		wait_obj_clear(channel->receive_event);
	freerdp_mutex_unlock(channel->mutex);

	memcpy(Buffer, item->buffer, item->length);

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
	WTSVirtualChannelManager* vcm = channel->vcm;

	if (channel == NULL)
		return false;

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
	wts_data_item* item;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	if (channel != NULL)
	{
		if (channel->index < channel->client->settings->num_channels)
			channel->client->settings->channels[channel->index].handle = NULL;
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
