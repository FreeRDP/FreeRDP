/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2020 Hardening <contact@hardening-consulting.com>
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

#include <stdio.h>
#include <string.h>

#include <winpr/stream.h>
#include <freerdp/utils/chunkstream.h>

int TestChunkStream(int argc, char* argv[])
{
	wStream* s;
	ChunkStream* cs;
	ChunkStreamSlot *slot, *slot3;
	int i;
	int errcode = 1;

	/* ============== first basic tests with static chunks ======= */
	cs = chunkstream_new(0);
	if (!cs)
	{
		fprintf(stderr, "unable to allocate a chunkstream");
		return errcode;
	}
	errcode++;

	slot = chunkstream_getStaticStringSlot(cs, "hello", FALSE);
	if (!slot || chunkstreamslot_size(slot) != 5)
	{
		fprintf(stderr, "error with first slot");
		return errcode;
	}
	errcode++;

	slot = chunkstream_getStaticStringSlot(cs, "hello", TRUE);
	if (!slot || chunkstreamslot_size(slot) != 6)
	{
		fprintf(stderr, "error with second slot");
		return errcode;
	}
	errcode++;

	slot = chunkstream_getPoolSlot(cs, 10);
	if (slot)
	{
		fprintf(stderr, "should not get a slot from an empty pool");
		return errcode;
	}
	errcode++;

	s = chunkstream_linearizeToStream(cs);
	if (!s || Stream_GetPosition(s) != 11 || memcmp("hellohello\x00", Stream_Buffer(s), 11))
	{
		fprintf(stderr, "error with linearized stream");
		return errcode;
	}
	Stream_Free(s, TRUE);
	errcode++;

	s = Stream_New(NULL, 3);
	if (!s)
	{
		fprintf(stderr, "error allocating stream");
		return errcode;
	}

	if (!chunkstream_linearizeInStream(cs, s) || Stream_GetPosition(s) != 11 ||
	    memcmp("hellohello\x00", Stream_Buffer(s), 11))
	{
		fprintf(stderr, "error with linearized stream");
		return errcode;
	}
	Stream_Free(s, TRUE);
	errcode++;

	chunkstream_destroy(&cs);
	if (cs)
	{
		fprintf(stderr, "expecting chunkstream to be cleaned");
		return errcode;
	}
	errcode++;

	/* ==================== let's test the pool =========== */

	cs = chunkstream_new(1024);
	if (!cs)
	{
		fprintf(stderr, "unable to allocate a chunkstream");
		return errcode;
	}
	errcode++;

	// get 8 blocks of 128 bytes, that should exhaust the pool
	for (i = 0; i < 8; i++)
	{
		size_t nbytes, j;
		BYTE* slotData;
		slot = chunkstream_getPoolSlot(cs, 128);
		if (!slot)
		{
			fprintf(stderr, "failed retrieving a 128 bytes pool slot");
			return errcode;
		}
		if (i == 3)
			slot3 = slot;

		slotData = chunkstreamslot_data(slot);
		nbytes = 1 + (i / 4);
		for (j = 0; j < nbytes; j++, slotData++)
			*slotData = (BYTE)i;

		if (!chunkstreamslot_update_used(slot, nbytes))
		{
			fprintf(stderr, "failed updating used size");
			return errcode;
		}
	}
	errcode++;

	// we should fail to update used size of last slot with more than 128
	if (chunkstreamslot_update_used(slot, 129))
	{
		fprintf(stderr, "last slot should not grow over 128");
		return errcode;
	}
	errcode++;

	slot = chunkstream_getPoolSlot(cs, 128);
	if (slot)
	{
		fprintf(stderr, "pool should be exhausted");
		return errcode;
	}
	errcode++;

	// check computation of sizeAfterSlot (after slot 3 so 2 * 4)
	if (chunkstream_sizeAfterSlot(cs, slot3) != 8)
	{
		fprintf(stderr, "invalid computation for chunkstream_sizeAfterSlot");
		return errcode;
	}
	errcode++;

	// let's check the content of the stream
	s = Stream_New(NULL, 3);
	if (!s)
	{
		fprintf(stderr, "error allocating stream");
		return errcode;
	}

	if (!chunkstream_linearizeInStream(cs, s) ||
	    memcmp("\x00\x01\x02\x03\x04\x04\x05\x05\x06\x06\x07\x07", Stream_Buffer(s), 12))
	{
		fprintf(stderr, "error with linearized stream");
		return errcode;
	}
	Stream_Free(s, TRUE);
	errcode++;

	chunkstream_destroy(&cs);
	if (cs)
	{
		fprintf(stderr, "expecting chunkstream to be cleaned");
		return errcode;
	}
	errcode++;

	return 0;
}
