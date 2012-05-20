/*
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
STREAM* stream_new(int size)
{
	STREAM* stream;

	stream = xnew(STREAM);

	if (stream != NULL)
	{
		if (size != 0)
		{
			size = size > 0 ? size : 0x400;
			stream->data = (uint8*) xzalloc(size);
			stream->p = stream->data;
			stream->size = size;
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
void stream_free(STREAM* stream)
{
	if (stream != NULL)
	{
		if (stream->data != NULL)
			xfree(stream->data);

		xfree(stream);
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
void stream_extend(STREAM* stream, int request_size)
{
	int pos;
	int original_size;
	int increased_size;

	pos = stream_get_pos(stream);
	original_size = stream->size;
	increased_size = (request_size > original_size ? request_size : original_size);
	stream->size += increased_size;

	if (original_size == 0)
		stream->data = (uint8*) xmalloc(stream->size);
	else
		stream->data = (uint8*) xrealloc(stream->data, stream->size);

	memset(stream->data + original_size, 0, increased_size);
	stream_set_pos(stream, pos);
}
