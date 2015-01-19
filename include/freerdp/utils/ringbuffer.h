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

#ifndef __RINGBUFFER_H___
#define __RINGBUFFER_H___

#include <winpr/wtypes.h>

/** @brief ring buffer meta data */
struct _RingBuffer {
	size_t initialSize;
	size_t freeSize;
	size_t size;
	size_t readPtr;
	size_t writePtr;
	BYTE *buffer;
};
typedef struct _RingBuffer RingBuffer;


/** @brief a piece of data in the ring buffer, exactly like a glibc iovec */
struct _DataChunk {
	size_t size;
	const BYTE *data;
};
typedef struct _DataChunk DataChunk;

/** initialise a ringbuffer
 * @param initialSize the initial capacity of the ringBuffer
 * @return if the initialisation was successful
 */
BOOL ringbuffer_init(RingBuffer *rb, size_t initialSize);

/** destroys internal data used by this ringbuffer
 * @param ringbuffer
 */
void ringbuffer_destroy(RingBuffer *ringbuffer);

/** computes the space used in this ringbuffer
 * @param ringbuffer
 * @return the number of bytes stored in that ringbuffer
 */
size_t ringbuffer_used(const RingBuffer *ringbuffer);

/** returns the capacity of the ring buffer
 * @param ringbuffer
 * @return the capacity of this ring buffer
 */
size_t ringbuffer_capacity(const RingBuffer *ringbuffer);

/** writes some bytes in the ringbuffer, if the data doesn't fit, the ringbuffer
 * is resized automatically
 *
 * @param rb the ringbuffer
 * @param ptr a pointer on the data to add
 * @param sz the size of the data to add
 * @return if the operation was successful, it could fail in case of OOM during realloc()
 */
BOOL ringbuffer_write(RingBuffer *rb, const void *ptr, size_t sz);


/** ensures that we have sz bytes available at the write head, and return a pointer
 * on the write head
 *
 * @param rb the ring buffer
 * @param sz the size to ensure
 * @return a pointer on the write head, or NULL in case of OOM
 */
BYTE *ringbuffer_ensure_linear_write(RingBuffer *rb, size_t sz);

/** move ahead the write head in case some byte were written directly by using
 * a pointer retrieved via ringbuffer_ensure_linear_write(). This function is
 * used to commit the written bytes. The provided size should not exceed the
 * size ensured by ringbuffer_ensure_linear_write()
 *
 * @param rb the ring buffer
 * @param sz the number of bytes that have been written
 * @return if the operation was successful, FALSE is sz is too big
 */
BOOL ringbuffer_commit_written_bytes(RingBuffer *rb, size_t sz);


/** peeks the buffer chunks for sz bytes and returns how many chunks are filled.
 * Note that the sum of the resulting chunks may be smaller than sz.
 *
 * @param rb the ringbuffer
 * @param chunks an array of data chunks that will contain data / size of chunks
 * @param sz the requested size
 * @return the number of chunks used for reading sz bytes
 */
int ringbuffer_peek(const RingBuffer *rb, DataChunk chunks[2], size_t sz);

/** move ahead the read head in case some byte were read using ringbuffer_peek()
 * This function is used to commit the bytes that were effectively consumed.
 *
 * @param rb the ring buffer
 * @param sz the
 */
void ringbuffer_commit_read_bytes(RingBuffer *rb, size_t sz);


#endif /* __RINGBUFFER_H___ */
