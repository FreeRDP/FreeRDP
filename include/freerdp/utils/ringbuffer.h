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

#ifndef FREERDP_UTILS_RINGBUFFER_H
#define FREERDP_UTILS_RINGBUFFER_H

#include <winpr/wtypes.h>
#include <freerdp/api.h>

/** @brief ring buffer meta data */
struct _RingBuffer
{
	size_t initialSize;
	size_t freeSize;
	size_t size;
	size_t readPtr;
	size_t writePtr;
	BYTE* buffer;
};
typedef struct _RingBuffer RingBuffer;

/** @brief a piece of data in the ring buffer, exactly like a glibc iovec */
struct _DataChunk
{
	size_t size;
	const BYTE* data;
};
typedef struct _DataChunk DataChunk;

#ifdef __cplusplus
extern "C"
{
#endif

	/** initialise a ringbuffer
	 * @param initialSize the initial capacity of the ringBuffer
	 * @return if the initialisation was successful
	 */
	FREERDP_API BOOL ringbuffer_init(RingBuffer* rb, size_t initialSize);

	/** destroys internal data used by this ringbuffer
	 * @param ringbuffer
	 */
	FREERDP_API void ringbuffer_destroy(RingBuffer* ringbuffer);

	/** computes the space used in this ringbuffer
	 * @param ringbuffer
	 * @return the number of bytes stored in that ringbuffer
	 */
	FREERDP_API size_t ringbuffer_used(const RingBuffer* ringbuffer);

	/** returns the capacity of the ring buffer
	 * @param ringbuffer
	 * @return the capacity of this ring buffer
	 */
	FREERDP_API size_t ringbuffer_capacity(const RingBuffer* ringbuffer);

	/** writes some bytes in the ringbuffer, if the data doesn't fit, the ringbuffer
	 * is resized automatically
	 *
	 * @param rb the ringbuffer
	 * @param ptr a pointer on the data to add
	 * @param sz the size of the data to add
	 * @return if the operation was successful, it could fail in case of OOM during realloc()
	 */
	FREERDP_API BOOL ringbuffer_write(RingBuffer* rb, const BYTE* ptr, size_t sz);

	/** ensures that we have sz bytes available at the write head, and return a pointer
	 * on the write head
	 *
	 * @param rb the ring buffer
	 * @param sz the size to ensure
	 * @return a pointer on the write head, or NULL in case of OOM
	 */
	FREERDP_API BYTE* ringbuffer_ensure_linear_write(RingBuffer* rb, size_t sz);

	/** move ahead the write head in case some byte were written directly by using
	 * a pointer retrieved via ringbuffer_ensure_linear_write(). This function is
	 * used to commit the written bytes. The provided size should not exceed the
	 * size ensured by ringbuffer_ensure_linear_write()
	 *
	 * @param rb the ring buffer
	 * @param sz the number of bytes that have been written
	 * @return if the operation was successful, FALSE is sz is too big
	 */
	FREERDP_API BOOL ringbuffer_commit_written_bytes(RingBuffer* rb, size_t sz);

	/** peeks the buffer chunks for sz bytes and returns how many chunks are filled.
	 * Note that the sum of the resulting chunks may be smaller than sz.
	 *
	 * @param rb the ringbuffer
	 * @param chunks an array of data chunks that will contain data / size of chunks
	 * @param sz the requested size
	 * @return the number of chunks used for reading sz bytes
	 */
	FREERDP_API int ringbuffer_peek(const RingBuffer* rb, DataChunk chunks[2], size_t sz);

	/** move ahead the read head in case some byte were read using ringbuffer_peek()
	 * This function is used to commit the bytes that were effectively consumed.
	 *
	 * @param rb the ring buffer
	 * @param sz the
	 */
	FREERDP_API void ringbuffer_commit_read_bytes(RingBuffer* rb, size_t sz);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_RINGBUFFER_H */
