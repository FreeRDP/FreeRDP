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

STREAM *
stream_new(int capacity)
{
	STREAM * stream;

	stream = (STREAM *) xmalloc(sizeof(STREAM));
	if (stream != NULL)
	{
		capacity = capacity > 0 ? capacity : 0x400;
		stream->buffer = (uint8 *) xmalloc(capacity);
		stream->ptr = stream->buffer;
		stream->capacity = capacity;
	}

	return stream;
}

void
stream_free(STREAM * stream)
{
	if (stream != NULL)
	{
		xfree(stream->buffer);
		xfree(stream);
	}
}

void
stream_extend(STREAM * stream)
{
	int pos;

	pos = stream_get_pos(stream);
	stream->capacity <<= 1;
	stream->buffer = (uint8 *) realloc(stream->buffer, stream->capacity);
	stream_set_pos(stream, pos);
}
