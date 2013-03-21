/*
 * WinPR: Windows Portable Runtime
 * Stream Utils
 *
 * Copyright 2011 Vic Lee
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/stream.h>

void Stream_EnsureCapacity(wStream* s, size_t size)
{
	if (s->capacity < size)
	{
		s->capacity = size;
		s->buffer = (BYTE*) realloc(s->buffer, size);
		s->pointer = s->buffer;
	}
}

wStream* Stream_New(BYTE* buffer, size_t size)
{
	wStream* s;

	s = malloc(sizeof(wStream));

	if (s != NULL)
	{
		if (buffer)
			s->buffer = buffer;
		else
			s->buffer = (BYTE*) malloc(size);

		s->pointer = s->buffer;
		s->capacity = size;
		s->length = size;
	}

	return s;
}

void Stream_Free(wStream* s, BOOL bFreeBuffer)
{
	if (s != NULL)
	{
		if (bFreeBuffer)
		{
			if (s->buffer != NULL)
				free(s->buffer);
		}

		free(s);
	}
}

/* Deprecated STREAM API */

/**
 * Allocates and initializes a STREAM structure.
 * STREAM are used to ease data access in read and write operations.
 * They consist of a buffer containing the data we want to access, and an offset associated to it, and keeping
 * track of the 'current' position in the stream. A list of functions can then be used to read/write different
 * type of data to/from it.
 * @see stream.h for the list of data access functions.
 *
 * @param size [in]		- size of the buffer that will ba allocated to the stream.
 * 						  If 0, there will be no buffer attached to the stream. The caller
 * 						  then needs to call stream_attach() to link an existing buffer to this stream.
 * 						  Caution: calling stream_attach() on a stream with an existing buffer will result
 * 						  in this buffer being lost, and possible memory leak.
 *
 * @return A pointer to an allocated and initialized STREAM structure.
 * This pointer need to be deallocated using the stream_free() function.
 */
wStream* stream_new(int size)
{
	wStream* stream;

	stream = malloc(sizeof(wStream));
	ZeroMemory(stream, sizeof(wStream));

	if (stream != NULL)
	{
		if (size != 0)
		{
			size = size > 0 ? size : 0x400;
			stream->buffer = (BYTE*) malloc(size);
			ZeroMemory(stream->buffer, size);
			stream->pointer = stream->buffer;
			stream->capacity = size;
		}
	}

	return stream;
}

/**
 * This function is used to deallocate a stream that was allocated using stream_new().
 * Caution: the buffer linked to the stream will be deallocated in the process. If this buffer was attached
 * using the stream_attach() function, the stream_detach() function needs to be called before calling stream_free()
 * otherwise it will be freed in the process.
 *
 * @param stream [in]	- Pointer to the STREAM structure that needs to be deallocated.
 * 						  This pointer is invalid on return.
 */
void stream_free(wStream* stream)
{
	if (stream != NULL)
	{
		if (stream->buffer != NULL)
			free(stream->buffer);

		free(stream);
	}
}

/**
 * This function is used to extend the size of an existing stream.
 * It will infact extend the attached buffer, fill the newly allocated region with 0, and reset the current
 * stream position.
 * If the stream did not have a buffer attached, a new one will be allocated and attached.
 *
 * @param stream [in/out]	pointer to the STREAM structure that needs to be extended.
 * @param request_size [in]	Number of bytes to add to the existing stream.
 * 							If the value is < the existing size, then the existing size is doubled.
 */
void stream_extend(wStream* stream, int request_size)
{
	int pos;
	int original_size;
	int increased_size;

	pos = stream_get_pos(stream);
	original_size = stream->capacity;
	increased_size = (request_size > original_size ? request_size : original_size);
	stream->capacity += increased_size;

	if (original_size == 0)
		stream->buffer = (BYTE*) malloc(stream->capacity);
	else
		stream->buffer = (BYTE*) realloc(stream->buffer, stream->capacity);

	memset(stream->buffer + original_size, 0, increased_size);
	stream_set_pos(stream, pos);
}
