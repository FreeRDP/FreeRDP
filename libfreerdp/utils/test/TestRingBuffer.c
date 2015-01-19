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

#include <stdio.h>
#include <string.h>

#include <freerdp/utils/ringbuffer.h>

BOOL test_overlaps(void)
{
	RingBuffer rb;
	DataChunk chunks[2];
	BYTE bytes[200];
	int nchunks, i, j, k, counter = 0;

	for (i = 0; i < sizeof(bytes); i++)
		bytes[i] = (BYTE)i;

	ringbuffer_init(&rb, 5);
	if (!ringbuffer_write(&rb, bytes, 4)) /* [0123.] */
		goto error;
	counter += 4;
	ringbuffer_commit_read_bytes(&rb, 2); /* [..23.] */

	if (!ringbuffer_write(&rb, &bytes[counter], 2)) /* [5.234] */
		goto error;
	counter += 2;

	nchunks = ringbuffer_peek(&rb, chunks, 4);
	if (nchunks != 2 || chunks[0].size != 3 || chunks[1].size != 1)
		goto error;

	for (i = 0, j = 2; i < nchunks; i++)
	{
		for (k = 0; k < chunks[i].size; k++, j++)
		{
			if (chunks[i].data[k] != (BYTE)j)
				goto error;
		}
	}

	ringbuffer_commit_read_bytes(&rb, 3); /* [5....] */
	if (ringbuffer_used(&rb) != 1)
		goto error;

	if (!ringbuffer_write(&rb, &bytes[counter], 6)) /* [56789ab....] */
		goto error;
	counter += 6;

	ringbuffer_commit_read_bytes(&rb, 6); /* [......b....] */
	nchunks = ringbuffer_peek(&rb, chunks, 10);
	if (nchunks != 1 || chunks[0].size != 1 || (*chunks[0].data != 0xb))
		goto error;

	if (ringbuffer_capacity(&rb) != 5)
		goto error;

	return TRUE;
error:
	ringbuffer_destroy(&rb);
	return FALSE;
}


int TestRingBuffer(int argc, char* argv[])
{
	RingBuffer ringBuffer;
	int testNo = 0;
	BYTE *tmpBuf;
	BYTE *rb_ptr;
	int i/*, chunkNb, counter*/;
	DataChunk chunks[2];

	if (!ringbuffer_init(&ringBuffer, 10))
	{
		fprintf(stderr, "unable to initialize ringbuffer\n");
		return -1;
	}

	tmpBuf = (BYTE *)malloc(50);
	if (!tmpBuf)
		return -1;

	for (i = 0; i < 50; i++)
		tmpBuf[i] = (char)i;

	fprintf(stderr, "%d: basic tests...", ++testNo);
	if (!ringbuffer_write(&ringBuffer, tmpBuf, 5) || !ringbuffer_write(&ringBuffer, tmpBuf, 5) ||
			!ringbuffer_write(&ringBuffer, tmpBuf, 5))
	{
		fprintf(stderr, "error when writing bytes\n");
		return -1;
	}

	if (ringbuffer_used(&ringBuffer) != 15)
	{
		fprintf(stderr, "invalid used size got %d when i would expect 15\n", ringbuffer_used(&ringBuffer));
		return -1;
	}

	if (ringbuffer_peek(&ringBuffer, chunks, 10) != 1 || chunks[0].size != 10)
	{
		fprintf(stderr, "error when reading bytes\n");
		return -1;
	}
	ringbuffer_commit_read_bytes(&ringBuffer, chunks[0].size);

	/* check retrieved bytes */
	for (i = 0; i < chunks[0].size; i++)
	{
		if (chunks[0].data[i] != i % 5)
		{
			fprintf(stderr, "invalid byte at %d, got %d instead of %d\n", i, chunks[0].data[i], i % 5);
			return -1;
		}
	}

	if (ringbuffer_used(&ringBuffer) != 5)
	{
		fprintf(stderr, "invalid used size after read got %d when i would expect 5\n", ringbuffer_used(&ringBuffer));
		return -1;
	}

	/* write some more bytes to have writePtr < readPtr and data splitted in 2 chunks */
	if (!ringbuffer_write(&ringBuffer, tmpBuf, 6) ||
			ringbuffer_peek(&ringBuffer, chunks, 11) != 2 ||
			chunks[0].size != 10 ||
			chunks[1].size != 1)
	{
		fprintf(stderr, "invalid read of splitted data\n");
		return -1;
	}

	ringbuffer_commit_read_bytes(&ringBuffer, 11);
	fprintf(stderr, "ok\n");

	fprintf(stderr, "%d: peek with nothing to read...", ++testNo);
	if (ringbuffer_peek(&ringBuffer, chunks, 10))
	{
		fprintf(stderr, "peek returns some chunks\n");
		return -1;
	}
	fprintf(stderr, "ok\n");

	fprintf(stderr, "%d: ensure_linear_write / read() shouldn't grow...", ++testNo);
	for (i = 0; i < 1000; i++)
	{
		rb_ptr = ringbuffer_ensure_linear_write(&ringBuffer, 50);
		if (!rb_ptr)
		{
			fprintf(stderr, "ringbuffer_ensure_linear_write() error\n");
			return -1;
		}

		memcpy(rb_ptr, tmpBuf, 50);

		if (!ringbuffer_commit_written_bytes(&ringBuffer, 50))
		{
			fprintf(stderr, "ringbuffer_commit_written_bytes() error, i=%d\n", i);
			return -1;
		}

		//ringbuffer_commit_read_bytes(&ringBuffer, 25);
	}

	for (i = 0; i < 1000; i++)
		ringbuffer_commit_read_bytes(&ringBuffer, 25);

	for (i = 0; i < 1000; i++)
		ringbuffer_commit_read_bytes(&ringBuffer, 25);


	if (ringbuffer_capacity(&ringBuffer) != 10)
	{
		fprintf(stderr, "not the expected capacity, have %d and expects 10\n", ringbuffer_capacity(&ringBuffer));
		return -1;
	}
	fprintf(stderr, "ok\n");


	fprintf(stderr, "%d: free size is correctly computed...", ++testNo);
	for (i = 0; i < 1000; i++)
	{
		ringbuffer_ensure_linear_write(&ringBuffer, 50);
		if (!ringbuffer_commit_written_bytes(&ringBuffer, 50))
		{
			fprintf(stderr, "ringbuffer_commit_written_bytes() error, i=%d\n", i);
			return -1;
		}
	}
	ringbuffer_commit_read_bytes(&ringBuffer, 50 * 1000);
	fprintf(stderr, "ok\n");

	ringbuffer_destroy(&ringBuffer);

	fprintf(stderr, "%d: specific overlaps test...", ++testNo);
	if (!test_overlaps())
	{
		fprintf(stderr, "ko\n", i);
		return -1;
	}
	fprintf(stderr, "ok\n");
	return 0;
}




