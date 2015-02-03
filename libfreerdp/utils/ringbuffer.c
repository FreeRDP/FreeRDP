/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Hardening <contact@hardening-consulting.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
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

#include <freerdp/utils/ringbuffer.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <winpr/crt.h>
#include <freerdp/log.h>

#define TAG FREERDP_TAG("utils.ringbuffer")

#ifdef WITH_DEBUG_RINGBUFFER
#define DEBUG_RINGBUFFER(fmt, ...) WLog_DBG(TAG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_RINGBUFFER(fmt, ...) do { } while (0)
#endif

BOOL ringbuffer_init(RingBuffer* rb, size_t initialSize)
{
	rb->buffer = malloc(initialSize);
	
	if (!rb->buffer)
		return FALSE;

	rb->readPtr = rb->writePtr = 0;
	rb->initialSize = rb->size = rb->freeSize = initialSize;

	DEBUG_RINGBUFFER("ringbuffer_init(%p)", rb);

	return TRUE;
}

size_t ringbuffer_used(const RingBuffer* rb)
{
	return rb->size - rb->freeSize;
}

size_t ringbuffer_capacity(const RingBuffer* rb)
{
	return rb->size;
}

void ringbuffer_destroy(RingBuffer* rb)
{
	DEBUG_RINGBUFFER("ringbuffer_destroy(%p)", rb);
	
	free(rb->buffer);
	rb->buffer = NULL;
}

static BOOL ringbuffer_realloc(RingBuffer* rb, size_t targetSize)
{
	BYTE* newData;
	
	DEBUG_RINGBUFFER("ringbuffer_realloc(%p): targetSize: %d", rb, targetSize);

	if (rb->writePtr == rb->readPtr)
	{
		/* when no size is used we can realloc() and set the heads at the
		 * beginning of the buffer
		 */
		newData = (BYTE*) realloc(rb->buffer, targetSize);
		
		if (!newData)
			return FALSE;
		
		rb->readPtr = rb->writePtr = 0;
		rb->buffer = newData;
	}
	else if ((rb->writePtr >= rb->readPtr) && (rb->writePtr < targetSize))
	{
		/* we reallocate only if we're in that case, realloc don't touch read
		 * and write heads
		 *
		 *        readPtr              writePtr
		 *              |              |
		 *              v              v
		 * [............|XXXXXXXXXXXXXX|..........]
		 */
		newData = (BYTE*) realloc(rb->buffer, targetSize);
		
		if (!newData)
			return FALSE;

		rb->buffer = newData;
	}
	else
	{
		/* in case of malloc the read head is moved at the beginning of the new buffer
		 * and the write head is set accordingly
		 */
		newData = (BYTE*) malloc(targetSize);
		
		if (!newData)
			return FALSE;
		
		if (rb->readPtr < rb->writePtr)
		{
			/*        readPtr              writePtr
			 *              |              |
			 *              v              v
			 * [............|XXXXXXXXXXXXXX|..........]
			 */
			memcpy(newData, rb->buffer + rb->readPtr, ringbuffer_used(rb));
		}
		else
		{
			/*        writePtr             readPtr
			 *              |              |
			 *              v              v
			 * [XXXXXXXXXXXX|..............|XXXXXXXXXX]
			 */
			BYTE* dst = newData;
			
			memcpy(dst, rb->buffer + rb->readPtr, rb->size - rb->readPtr);
			dst += (rb->size - rb->readPtr);
			
			if (rb->writePtr)
				memcpy(dst, rb->buffer, rb->writePtr);
		}
		
		rb->writePtr = rb->size - rb->freeSize;
		rb->readPtr = 0;
		free(rb->buffer);
		rb->buffer = newData;
	}

	rb->freeSize += (targetSize - rb->size);
	rb->size = targetSize;
	
	return TRUE;
}

/**
 *
 * @param rb
 * @param ptr
 * @param sz
 * @return
 */
BOOL ringbuffer_write(RingBuffer* rb, const BYTE* ptr, size_t sz)
{
	size_t toWrite;
	size_t remaining;

	DEBUG_RINGBUFFER("ringbuffer_write(%p): sz: %d", rb, sz);
	
	if ((rb->freeSize <= sz) && !ringbuffer_realloc(rb, rb->size + sz))
		return FALSE;

	/*  the write could be split in two
	 *    readHead        writeHead
	 *      |               |
	 *      v               v
	 * [    ################        ]
	 */
	toWrite = sz;
	remaining = sz;
	
	if (rb->size - rb->writePtr < sz)
		toWrite = rb->size - rb->writePtr;

	if (toWrite)
	{
		memcpy(rb->buffer + rb->writePtr, ptr, toWrite);
		remaining -= toWrite;
		ptr += toWrite;
	}

	if (remaining)
		memcpy(rb->buffer, ptr, remaining);

	rb->writePtr = (rb->writePtr + sz) % rb->size;

	rb->freeSize -= sz;
	return TRUE;
}

BYTE* ringbuffer_ensure_linear_write(RingBuffer* rb, size_t sz)
{
	DEBUG_RINGBUFFER("ringbuffer_ensure_linear_write(%p): sz: %d", rb, sz);

	if (rb->freeSize < sz)
	{
		if (!ringbuffer_realloc(rb, rb->size + sz - rb->freeSize + 32))
			return NULL;
	}

	if (rb->writePtr == rb->readPtr)
	{
		rb->writePtr = rb->readPtr = 0;
	}

	if (rb->writePtr + sz < rb->size)
		return rb->buffer + rb->writePtr;

	/*
	 * to add:             .......
	 * [          XXXXXXXXX  ]
	 *
	 * result:
	 * [XXXXXXXXX.......     ]
	 */
	memmove(rb->buffer, rb->buffer + rb->readPtr, rb->writePtr - rb->readPtr);
	rb->readPtr = 0;
	rb->writePtr = rb->size - rb->freeSize;
	return rb->buffer + rb->writePtr;
}

BOOL ringbuffer_commit_written_bytes(RingBuffer* rb, size_t sz)
{
	DEBUG_RINGBUFFER("ringbuffer_commit_written_bytes(%p): sz: %d", rb, sz);
	
	if (sz < 1)
		return TRUE;
	
	if (rb->writePtr + sz > rb->size)
		return FALSE;
	
	rb->writePtr = (rb->writePtr + sz) % rb->size;
	rb->freeSize -= sz;
	
	return TRUE;
}

int ringbuffer_peek(const RingBuffer* rb, DataChunk chunks[2], size_t sz)
{
	size_t remaining = sz;
	size_t toRead;
	int chunkIndex = 0;
	int status = 0;
	
	DEBUG_RINGBUFFER("ringbuffer_peek(%p): sz: %d", rb, sz);

	if (sz < 1)
		return 0;
	
	if ((rb->size - rb->freeSize) < sz)
		remaining = rb->size - rb->freeSize;

	toRead = remaining;

	if ((rb->readPtr + remaining) > rb->size)
		toRead = rb->size - rb->readPtr;

	if (toRead)
	{
		chunks[0].data = rb->buffer + rb->readPtr;
		chunks[0].size = toRead;
		remaining -= toRead;
		chunkIndex++;
		status++;
	}

	if (remaining)
	{
		chunks[chunkIndex].data = rb->buffer;
		chunks[chunkIndex].size = remaining;
		status++;
	}
	
	return status;
}

void ringbuffer_commit_read_bytes(RingBuffer* rb, size_t sz)
{
	DEBUG_RINGBUFFER("ringbuffer_commit_read_bytes(%p): sz: %d", rb, sz);
	
	if (sz < 1)
		return;
	
	assert(rb->size - rb->freeSize >= sz);

	rb->readPtr = (rb->readPtr + sz) % rb->size;
	rb->freeSize += sz;

	/* when we reach a reasonable free size, we can go back to the original size */
	if ((rb->size != rb->initialSize) && (ringbuffer_used(rb) < rb->initialSize / 2))
		ringbuffer_realloc(rb, rb->initialSize);
}
