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

#include <freerdp/config.h>

#include <freerdp/utils/ringbuffer.h>

#include <stdlib.h>
#include <string.h>
#include <winpr/assert.h>

#include <winpr/crt.h>
#include <freerdp/log.h>

#ifdef WITH_DEBUG_RINGBUFFER
#define TAG FREERDP_TAG("utils.ringbuffer")

#define DEBUG_RINGBUFFER(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_RINGBUFFER(...) \
	do                        \
	{                         \
	} while (0)
#endif

BOOL ringbuffer_init(RingBuffer* ringbuffer, size_t initialSize)
{
	WINPR_ASSERT(ringbuffer);
	ringbuffer->buffer = malloc(initialSize);

	if (!ringbuffer->buffer)
		return FALSE;

	ringbuffer->readPtr = ringbuffer->writePtr = 0;
	ringbuffer->initialSize = ringbuffer->size = ringbuffer->freeSize = initialSize;
	DEBUG_RINGBUFFER("ringbuffer_init(%p)", (void*)rb);
	return TRUE;
}

size_t ringbuffer_used(const RingBuffer* ringbuffer)
{
	WINPR_ASSERT(ringbuffer);
	return ringbuffer->size - ringbuffer->freeSize;
}

size_t ringbuffer_capacity(const RingBuffer* ringbuffer)
{
	WINPR_ASSERT(ringbuffer);
	return ringbuffer->size;
}

void ringbuffer_destroy(RingBuffer* ringbuffer)
{
	DEBUG_RINGBUFFER("ringbuffer_destroy(%p)", (void*)ringbuffer);
	if (!ringbuffer)
		return;

	free(ringbuffer->buffer);
	ringbuffer->buffer = NULL;
}

static BOOL ringbuffer_realloc(RingBuffer* ringbuffer, size_t targetSize)
{
	WINPR_ASSERT(ringbuffer);
	BYTE* newData = NULL;
	DEBUG_RINGBUFFER("ringbuffer_realloc(%p): targetSize: %" PRIdz "", (void*)rb, targetSize);

	if (ringbuffer->writePtr == ringbuffer->readPtr)
	{
		/* when no size is used we can realloc() and set the heads at the
		 * beginning of the buffer
		 */
		newData = (BYTE*)realloc(ringbuffer->buffer, targetSize);

		if (!newData)
			return FALSE;

		ringbuffer->readPtr = ringbuffer->writePtr = 0;
		ringbuffer->buffer = newData;
	}
	else if ((ringbuffer->writePtr >= ringbuffer->readPtr) && (ringbuffer->writePtr < targetSize))
	{
		/* we reallocate only if we're in that case, realloc don't touch read
		 * and write heads
		 *
		 *        readPtr              writePtr
		 *              |              |
		 *              v              v
		 * [............|XXXXXXXXXXXXXX|..........]
		 */
		newData = (BYTE*)realloc(ringbuffer->buffer, targetSize);

		if (!newData)
			return FALSE;

		ringbuffer->buffer = newData;
	}
	else
	{
		/* in case of malloc the read head is moved at the beginning of the new buffer
		 * and the write head is set accordingly
		 */
		newData = (BYTE*)malloc(targetSize);

		if (!newData)
			return FALSE;

		if (ringbuffer->readPtr < ringbuffer->writePtr)
		{
			/*        readPtr              writePtr
			 *              |              |
			 *              v              v
			 * [............|XXXXXXXXXXXXXX|..........]
			 */
			memcpy(newData, ringbuffer->buffer + ringbuffer->readPtr, ringbuffer_used(ringbuffer));
		}
		else
		{
			/*        writePtr             readPtr
			 *              |              |
			 *              v              v
			 * [XXXXXXXXXXXX|..............|XXXXXXXXXX]
			 */
			BYTE* dst = newData;
			memcpy(dst, ringbuffer->buffer + ringbuffer->readPtr,
			       ringbuffer->size - ringbuffer->readPtr);
			dst += (ringbuffer->size - ringbuffer->readPtr);

			if (ringbuffer->writePtr)
				memcpy(dst, ringbuffer->buffer, ringbuffer->writePtr);
		}

		ringbuffer->writePtr = ringbuffer->size - ringbuffer->freeSize;
		ringbuffer->readPtr = 0;
		free(ringbuffer->buffer);
		ringbuffer->buffer = newData;
	}

	ringbuffer->freeSize += (targetSize - ringbuffer->size);
	ringbuffer->size = targetSize;
	return TRUE;
}

/**
 * Write to a ringbuffer
 *
 * @param ringbuffer A pointer to the ringbuffer
 * @param ptr A pointer to the data to write
 * @param sz The number of bytes to write
 *
 * @return \b TRUE for success, \b FALSE for failure
 */
BOOL ringbuffer_write(RingBuffer* ringbuffer, const BYTE* ptr, size_t sz)
{
	size_t toWrite = 0;
	size_t remaining = 0;

	WINPR_ASSERT(ringbuffer);
	DEBUG_RINGBUFFER("ringbuffer_write(%p): sz: %" PRIdz "", (void*)rb, sz);

	if ((ringbuffer->freeSize <= sz) && !ringbuffer_realloc(ringbuffer, ringbuffer->size + sz))
		return FALSE;

	/*  the write could be split in two
	 *    readHead        writeHead
	 *      |               |
	 *      v               v
	 * [    ################        ]
	 */
	toWrite = sz;
	remaining = sz;

	if (ringbuffer->size - ringbuffer->writePtr < sz)
		toWrite = ringbuffer->size - ringbuffer->writePtr;

	if (toWrite)
	{
		memcpy(ringbuffer->buffer + ringbuffer->writePtr, ptr, toWrite);
		remaining -= toWrite;
		ptr += toWrite;
	}

	if (remaining)
		memcpy(ringbuffer->buffer, ptr, remaining);

	ringbuffer->writePtr = (ringbuffer->writePtr + sz) % ringbuffer->size;
	ringbuffer->freeSize -= sz;
	return TRUE;
}

BYTE* ringbuffer_ensure_linear_write(RingBuffer* ringbuffer, size_t sz)
{
	DEBUG_RINGBUFFER("ringbuffer_ensure_linear_write(%p): sz: %" PRIdz "", (void*)rb, sz);

	WINPR_ASSERT(ringbuffer);
	if (ringbuffer->freeSize < sz)
	{
		if (!ringbuffer_realloc(ringbuffer, ringbuffer->size + sz - ringbuffer->freeSize + 32))
			return NULL;
	}

	if (ringbuffer->writePtr == ringbuffer->readPtr)
	{
		ringbuffer->writePtr = ringbuffer->readPtr = 0;
	}

	if (ringbuffer->writePtr + sz < ringbuffer->size)
		return ringbuffer->buffer + ringbuffer->writePtr;

	/*
	 * to add:             .......
	 * [          XXXXXXXXX  ]
	 *
	 * result:
	 * [XXXXXXXXX.......     ]
	 */
	memmove(ringbuffer->buffer, ringbuffer->buffer + ringbuffer->readPtr,
	        ringbuffer->writePtr - ringbuffer->readPtr);
	ringbuffer->readPtr = 0;
	ringbuffer->writePtr = ringbuffer->size - ringbuffer->freeSize;
	return ringbuffer->buffer + ringbuffer->writePtr;
}

BOOL ringbuffer_commit_written_bytes(RingBuffer* ringbuffer, size_t sz)
{
	DEBUG_RINGBUFFER("ringbuffer_commit_written_bytes(%p): sz: %" PRIdz "", (void*)rb, sz);

	WINPR_ASSERT(ringbuffer);
	if (sz < 1)
		return TRUE;

	if (ringbuffer->writePtr + sz > ringbuffer->size)
		return FALSE;

	ringbuffer->writePtr = (ringbuffer->writePtr + sz) % ringbuffer->size;
	ringbuffer->freeSize -= sz;
	return TRUE;
}

int ringbuffer_peek(const RingBuffer* ringbuffer, DataChunk chunks[2], size_t sz)
{
	size_t remaining = sz;
	size_t toRead = 0;
	int chunkIndex = 0;
	int status = 0;
	DEBUG_RINGBUFFER("ringbuffer_peek(%p): sz: %" PRIdz "", (const void*)rb, sz);

	WINPR_ASSERT(ringbuffer);
	if (sz < 1)
		return 0;

	if ((ringbuffer->size - ringbuffer->freeSize) < sz)
		remaining = ringbuffer->size - ringbuffer->freeSize;

	toRead = remaining;

	if ((ringbuffer->readPtr + remaining) > ringbuffer->size)
		toRead = ringbuffer->size - ringbuffer->readPtr;

	if (toRead)
	{
		chunks[0].data = ringbuffer->buffer + ringbuffer->readPtr;
		chunks[0].size = toRead;
		remaining -= toRead;
		chunkIndex++;
		status++;
	}

	if (remaining)
	{
		chunks[chunkIndex].data = ringbuffer->buffer;
		chunks[chunkIndex].size = remaining;
		status++;
	}

	return status;
}

void ringbuffer_commit_read_bytes(RingBuffer* ringbuffer, size_t sz)
{
	DEBUG_RINGBUFFER("ringbuffer_commit_read_bytes(%p): sz: %" PRIdz "", (void*)ringbuffer, sz);

	WINPR_ASSERT(ringbuffer);
	if (sz < 1)
		return;

	WINPR_ASSERT(ringbuffer->size - ringbuffer->freeSize >= sz);
	ringbuffer->readPtr = (ringbuffer->readPtr + sz) % ringbuffer->size;
	ringbuffer->freeSize += sz;

	/* when we reach a reasonable free size, we can go back to the original size */
	if ((ringbuffer->size != ringbuffer->initialSize) &&
	    (ringbuffer_used(ringbuffer) < ringbuffer->initialSize / 2))
		ringbuffer_realloc(ringbuffer, ringbuffer->initialSize);
}
