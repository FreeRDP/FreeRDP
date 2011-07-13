/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Stream Utils
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

#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>

STREAM* stream_new(int size)
{
	STREAM* stream;

	stream = xnew(STREAM);

	if (stream != NULL)
	{
		if (size != 0)
		{
			size = size > 0 ? size : 0x400;
			stream->data = (uint8*)xmalloc(size);
			stream->p = stream->data;
			stream->size = size;
		}
	}

	return stream;
}

void stream_free(STREAM* stream)
{
	if (stream != NULL)
	{
		xfree(stream->data);
		xfree(stream);
	}
}

void stream_extend(STREAM* stream)
{
	int pos;

	pos = stream_get_pos(stream);
	stream->size <<= 1;
	stream->data = (uint8*)xrealloc(stream->data, stream->size);
	stream_set_pos(stream, pos);
}
