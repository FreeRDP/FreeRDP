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

static STREAM* stream_new_inner(int size, boolean dirty)
{
	STREAM* stream;

	stream = xnew(STREAM);

	if (stream != NULL)
	{
		stream->dirty = dirty;
		if (size != 0)
		{
			size = size > 0 ? size : 0x400;
			stream->p = stream->data = stream->allocated = 
				dirty ? (uint8*) xmalloc(size) : (uint8*) xzalloc(size);
			stream->size = size;
		}
	}

	return stream;
}


STREAM* stream_new(int size)
{
	return stream_new_inner(size, false);
}

FREERDP_API STREAM* stream_new_dirty(int size)
{
	return stream_new_inner(size, true);
}

void stream_free(STREAM* stream)
{
	if (stream != NULL)
	{
		if (stream->allocated != NULL)
			xfree(stream->allocated);

		xfree(stream);
	}
}

void stream_extend(STREAM* stream, int request_size)
{
	int pos;
	int original_size;
	int increased_size;
	int offset;

	pos = stream_get_pos(stream);
	original_size = stream->size;
	increased_size = (request_size > original_size ? request_size : original_size);
	stream->size += increased_size;

	if (stream->allocated == NULL)
	{  //stram has or shadowed or really not allocated data 
		stream->allocated = (uint8*) xmalloc(stream->size);

		//if stream is shadowed - copy original data to new storage
		//original allocated pointer will be free'd by unshadowing
		if (original_size!=0)
			memcpy(stream->allocated, stream->data, original_size);

		stream->data = stream->allocated;
	}
	else if (stream->allocated != stream->data)
	{   //stream is not shadowed but shifted
		//realloc whole block opnly if itssize less than we want
		//otherwise just unshift pointer and move content to beginning
		offset = stream->data - stream->allocated;
		if (offset>=increased_size)
		{
			stream->size += offset - increased_size;
			increased_size = offset;
		}
		else
			stream->allocated = (uint8*) xrealloc(stream->allocated, stream->size);

		memmove(stream->allocated, stream->allocated  + offset, original_size);
		stream->data = stream->allocated;
	}
	else
	{
		stream->data = stream->allocated = 
			(uint8*) xrealloc(stream->allocated, stream->size);
	}

	if (!stream->dirty)
		memset(stream->data + original_size, 0, increased_size);
	stream_set_pos(stream, pos);
}
