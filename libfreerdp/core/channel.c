/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/freerdp.h>
#include <freerdp/peer.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>

#include "rdp.h"
#include "channel.h"

boolean freerdp_channel_send(rdpRdp* rdp, uint16 channel_id, uint8* data, int size)
{
	STREAM* s;
	uint32 flags;
	int i, left;
	int chunk_size;
	rdpChannel* channel = NULL;

	for (i = 0; i < rdp->settings->num_channels; i++)
	{
		if (rdp->settings->channels[i].channel_id == channel_id)
		{
			channel = &rdp->settings->channels[i];
			break;
		}
	}

	if (channel == NULL)
	{
		printf("freerdp_channel_send: unknown channel_id %d\n", channel_id);
		return false;
	}

	flags = CHANNEL_FLAG_FIRST;
	left = size;
	while (left > 0)
	{
		s = rdp_send_stream_init(rdp);

		if (left > (int) rdp->settings->vc_chunk_size)
		{
			chunk_size = rdp->settings->vc_chunk_size;
		}
		else
		{
			chunk_size = left;
			flags |= CHANNEL_FLAG_LAST;
		}
		if ((channel->options & CHANNEL_OPTION_SHOW_PROTOCOL))
		{
			flags |= CHANNEL_FLAG_SHOW_PROTOCOL;
		}

		stream_write_uint32(s, size);
		stream_write_uint32(s, flags);
		stream_check_size(s, chunk_size);
		stream_write(s, data, chunk_size);

		rdp_send(rdp, s, channel_id);

		data += chunk_size;
		left -= chunk_size;
		flags = 0;
	}

	return true;
}

void freerdp_channel_process(freerdp* instance, STREAM* s, uint16 channel_id)
{
	uint32 length;
	uint32 flags;
	int chunk_length;

	stream_read_uint32(s, length);
	stream_read_uint32(s, flags);
	chunk_length = stream_get_left(s);

	IFCALL(instance->ReceiveChannelData, instance,
		channel_id, stream_get_tail(s), chunk_length, flags, length);
}

void freerdp_channel_peer_process(freerdp_peer* client, STREAM* s, uint16 channel_id)
{
	uint32 length;
	uint32 flags;
	int chunk_length;

	stream_read_uint32(s, length);
	stream_read_uint32(s, flags);
	chunk_length = stream_get_left(s);

	IFCALL(client->ReceiveChannelData, client,
		channel_id, stream_get_tail(s), chunk_length, flags, length);
}
