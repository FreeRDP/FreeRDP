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
#include <freerdp/utils/memory.h>

#include "wtsvc.h"

struct send_item
{
	uint16 channel_id;
	uint8* buffer;
	uint32 length;
};

static void send_item_free(struct send_item* item)
{
	xfree(item->buffer);
	xfree(item);
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
	}

	return vcm;
}

void WTSDestroyVirtualChannelManager(WTSVirtualChannelManager* vcm)
{
	struct send_item* item;

	if (vcm != NULL)
	{
		wait_obj_free(vcm->send_event);
		while ((item = (struct send_item*) list_dequeue(vcm->send_queue)) != NULL)
		{
			send_item_free(item);
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
	struct send_item* item;

	wait_obj_clear(vcm->send_event);

	freerdp_mutex_lock(vcm->mutex);
	while ((item = (struct send_item*) list_dequeue(vcm->send_queue)) != NULL)
	{
		if (vcm->client->SendChannelData(vcm->client, item->channel_id, item->buffer, item->length) == false)
		{
			result = false;
		}
		send_item_free(item);
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

	channel = xnew(rdpPeerChannel);
	channel->vcm = vcm;
	channel->client = client;
	channel->channel_id = client->settings->channels[i].channel_id;
	if ((flags & WTS_CHANNEL_OPTION_DYNAMIC) != 0)
	{
		channel->channel_type = RDP_PEER_CHANNEL_TYPE_DVC;

		/* TODO: do DVC channel initialization here using pVirtualName */
	}
	else
	{
		channel->channel_type = RDP_PEER_CHANNEL_TYPE_SVC;
	}

	return channel;
}

boolean WTSVirtualChannelQuery(
	/* __in */  void* hChannelHandle,
	/* __in */  WTS_VIRTUAL_CLASS WtsVirtualClass,
	/* __out */ void** ppBuffer,
	/* __out */ uint32* pBytesReturned)
{
	return false;
}

void WTSFreeMemory(
	/* __in */ void* pMemory)
{
}

boolean WTSVirtualChannelRead(
	/* __in */  void* hChannelHandle,
	/* __in */  uint32 TimeOut,
	/* __out */ uint8* Buffer,
	/* __in */  uint32 BufferSize,
	/* __out */ uint32* pBytesRead)
{
	return false;
}

boolean WTSVirtualChannelWrite(
	/* __in */  void* hChannelHandle,
	/* __in */  uint8* Buffer,
	/* __in */  uint32 Length,
	/* __out */ uint32* pBytesWritten)
{
	uint32 written = 0;
	boolean result = false;
	struct send_item* item;
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;
	WTSVirtualChannelManager* vcm = channel->vcm;

	if (channel == NULL)
		return false;

	if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_SVC)
	{
		item = xnew(struct send_item);
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
	rdpPeerChannel* channel = (rdpPeerChannel*) hChannelHandle;

	if (channel != NULL)
		xfree(channel);

	return true;
}
