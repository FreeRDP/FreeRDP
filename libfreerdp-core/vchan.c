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
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>

#include "rdp.h"
#include "vchan.h"

boolean vchan_send(rdpVchan* vchan, uint16 channel_id, uint8* data, int size)
{
	STREAM* s;
	uint32 flags;
	rdpChan* channel = NULL;
	int i;
	int chunk_size;
	int left;

	for (i = 0; i < vchan->instance->settings->num_channels; i++)
	{
		if (vchan->instance->settings->channels[i].chan_id == channel_id)
		{
			channel = &vchan->instance->settings->channels[i];
			break;
		}
	}
	if (channel == NULL)
	{
		printf("vchan_send: unknown channel_id %d\n", channel_id);
		return False;
	}

	flags = CHANNEL_FLAG_FIRST;
	left = size;
	while (left > 0)
	{
		s = rdp_send_stream_init(vchan->instance->rdp);

		if (left > (int) vchan->instance->settings->vc_chunk_size)
		{
			chunk_size = vchan->instance->settings->vc_chunk_size;
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

		rdp_send(vchan->instance->rdp, s, channel_id);

		data += chunk_size;
		left -= chunk_size;
		flags = 0;
	}

	return True;
}

void vchan_process(rdpVchan* vchan, STREAM* s, uint16 channel_id)
{
	uint32 length;
	uint32 flags;
	int chunk_length;

	stream_read_uint32(s, length);
	stream_read_uint32(s, flags);
	chunk_length = stream_get_left(s);

	IFCALL(vchan->instance->ReceiveChannelData, vchan->instance,
		channel_id, stream_get_tail(s), chunk_length, flags, length);
}

rdpVchan* vchan_new(freerdp* instance)
{
	rdpVchan* vchan;

	vchan = xnew(rdpVchan);
	vchan->instance = instance;

	return vchan;
}

void vchan_free(rdpVchan* vchan)
{
	xfree(vchan);
}
