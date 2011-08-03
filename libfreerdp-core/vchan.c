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
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>

#include "vchan.h"

int vchan_send(rdpVchan* vchan, uint16 channel_id, uint8* data, int size)
{
	printf("vchan_send\n");
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
