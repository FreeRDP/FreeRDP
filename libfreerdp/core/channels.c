/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Virtual Channels
 *
 * Copyright 2011 Vic Lee
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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>

#include <freerdp/svc.h>
#include <freerdp/peer.h>
#include <freerdp/addin.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/debug.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/drdynvc.h>
#include <freerdp/channels/channels.h>

#include "rdp.h"
#include "client.h"
#include "channels.h"

BOOL freerdp_channel_send(rdpRdp* rdp, UINT16 channelId, BYTE* data, int size)
{
	DWORD i;
	int left;
	wStream* s;
	UINT32 flags;
	int chunkSize;
	rdpMcs* mcs = rdp->mcs;
	rdpMcsChannel* channel = NULL;

	for (i = 0; i < mcs->channelCount; i++)
	{
		if (mcs->channels[i].ChannelId == channelId)
		{
			channel = &mcs->channels[i];
			break;
		}
	}

	if (!channel)
	{
		fprintf(stderr, "freerdp_channel_send: unknown channelId %d\n", channelId);
		return FALSE;
	}

	flags = CHANNEL_FLAG_FIRST;
	left = size;

	while (left > 0)
	{
		s = rdp_send_stream_init(rdp);

		if (left > (int) rdp->settings->VirtualChannelChunkSize)
		{
			chunkSize = rdp->settings->VirtualChannelChunkSize;
		}
		else
		{
			chunkSize = left;
			flags |= CHANNEL_FLAG_LAST;
		}

		if ((channel->options & CHANNEL_OPTION_SHOW_PROTOCOL))
		{
			flags |= CHANNEL_FLAG_SHOW_PROTOCOL;
		}

		Stream_Write_UINT32(s, size);
		Stream_Write_UINT32(s, flags);
		Stream_EnsureCapacity(s, chunkSize);
		Stream_Write(s, data, chunkSize);

		rdp_send(rdp, s, channelId);

		data += chunkSize;
		left -= chunkSize;
		flags = 0;
	}

	return TRUE;
}

BOOL freerdp_channel_process(freerdp* instance, wStream* s, UINT16 channelId)
{
	UINT32 length;
	UINT32 flags;
	int chunkLength;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, length);
	Stream_Read_UINT32(s, flags);
	chunkLength = Stream_GetRemainingLength(s);

	IFCALL(instance->ReceiveChannelData, instance,
			channelId, Stream_Pointer(s), chunkLength, flags, length);

	return TRUE;
}

BOOL freerdp_channel_peer_process(freerdp_peer* client, wStream* s, UINT16 channelId)
{
	UINT32 length;
	UINT32 flags;
	int chunkLength;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT32(s, length);
	Stream_Read_UINT32(s, flags);
	chunkLength = Stream_GetRemainingLength(s);

	IFCALL(client->ReceiveChannelData, client,
		channelId, Stream_Pointer(s), chunkLength, flags, length);

	return TRUE;
}
