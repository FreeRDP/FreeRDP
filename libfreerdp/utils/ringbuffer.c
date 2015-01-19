/**
 * Copyright © 2014 Thincast Technologies GmbH
 * Copyright © 2014 Hardening <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  The copyright holders make
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <freerdp/utils/ringbuffer.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>


BOOL ringbuffer_init(RingBuffer *rb, size_t initialSize)
{
	rb->buffer = malloc(initialSize);
	if (!rb->buffer)
		return FALSE;

	rb->readPtr = rb->writePtr = 0;
	rb->initialSize = rb->size = rb->freeSize = initialSize;
	return TRUE;
}


size_t ringbuffer_used(const RingBuffer *ringbuffer)
{
	return ringbuffer->size - ringbuffer->freeSize;
}

size_t ringbuffer_capacity(const RingBuffer *ringbuffer)
{
	return ringbuffer->size;
}

void ringbuffer_destroy(RingBuffer *ringbuffer)
{
	free(ringbuffer->buffer);
	ringbuffer->buffer = 0;
}

static BOOL ringbuffer_realloc(RingBuffer *rb, size_t targetSize)
{
	BYTE *newData;

	if (rb->writePtr == rb->readPtr)
	{
		/* when no size is used we can realloc() and set the heads at the
		 * beginning of the buffer
		 */
		newData = (BYTE *)realloc(rb->buffer, targetSize);
		if (!newData)
			return FALSE;
		rb->readPtr = rb->writePtr = 0;
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
		newData = (BYTE *)realloc(rb->buffer, targetSize);
		if (!newData)
			return FALSE;

		rb->buffer = newData;
	}
	else
	{
		/* in case of malloc the read head is moved at the beginning of the new buffer
		 * and the write head is set accordingly
		 */
		newData = (BYTE *)malloc(targetSize);
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
			BYTE *dst = newData;
			memcpy(dst, rb->buffer + rb->readPtr, rb->size - rb->readPtr);
			dst += (rb->size - rb->readPtr);
			if (rb->writePtr)
				memcpy(dst, rb->buffer, rb->writePtr);
		}
		rb->writePtr = rb->size - rb->freeSize;
		rb->readPtr = 0;
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
BOOL ringbuffer_write(RingBuffer *rb, const void *ptr, size_t sz)
{
	if ((rb->freeSize <= sz) && !ringbuffer_realloc(rb, rb->size + sz))
		return FALSE;

	/*  the write could be split in two
	 *    readHead        writeHead
	 *      |               |
	 *      v               v
	 * [    ################        ]
	 */
	size_t toWrite = sz;
	size_t remaining = sz;
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


BYTE *ringbuffer_ensure_linear_write(RingBuffer *rb, size_t sz)
{
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

BOOL ringbuffer_commit_written_bytes(RingBuffer *rb, size_t sz)
{
	if (rb->writePtr + sz > rb->size)
		return FALSE;
	rb->writePtr = (rb->writePtr + sz) % rb->size;
	rb->freeSize -= sz;
	return TRUE;
}

int ringbuffer_peek(const RingBuffer *rb, DataChunk chunks[2], size_t sz)
{
	size_t remaining = sz;
	size_t toRead;
	int chunkIndex = 0;
	int ret = 0;

	if (rb->size - rb->freeSize < sz)
		remaining = rb->size - rb->freeSize;

	toRead = remaining;

	if (rb->readPtr + remaining > rb->size)
		toRead = rb->size - rb->readPtr;

	if (toRead)
	{
		chunks[0].data = rb->buffer + rb->readPtr;
		chunks[0].size = toRead;
		remaining -= toRead;
		chunkIndex++;
		ret++;
	}

	if (remaining)
	{
		chunks[chunkIndex].data = rb->buffer;
		chunks[chunkIndex].size = remaining;
		ret++;
	}
	return ret;
}

void ringbuffer_commit_read_bytes(RingBuffer *rb, size_t sz)
{
	assert(rb->size - rb->freeSize >= sz);

	rb->readPtr = (rb->readPtr + sz) % rb->size;
	rb->freeSize += sz;

	/* when we reach a reasonable free size, we can go back to the original size */
	if ((rb->size != rb->initialSize) && (ringbuffer_used(rb) < rb->initialSize / 2))
		ringbuffer_realloc(rb, rb->initialSize);
}
