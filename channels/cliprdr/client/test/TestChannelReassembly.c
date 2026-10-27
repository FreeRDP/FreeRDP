/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Reassembly of chunked static virtual channel messages (channel_client_post_message).
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
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>
#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/svc.h>

#include "../../../../channels/client/addin.h"

typedef struct
{
	size_t expected;
	size_t delivered;
	BOOL ok;
} Received;

static BYTE pattern(size_t index)
{
	return (BYTE)((index * 31u) + 7u);
}

static UINT on_message(LPVOID userdata, wStream* s)
{
	Received* r = userdata;
	r->delivered++;
	r->ok = (Stream_Length(s) == r->expected);

	const BYTE* data = Stream_ConstBuffer(s);
	for (size_t x = 0; r->ok && (x < r->expected); x++)
	{
		if (data[x] != pattern(x))
			r->ok = FALSE;
	}

	/* the buffer grows ahead of the data, but never beyond twice the message */
	if (Stream_Capacity(s) > (2 * r->expected) + 128)
	{
		(void)fprintf(stderr, "capacity %" PRIuz " for a %" PRIuz " byte message\n",
		              Stream_Capacity(s), r->expected);
		r->ok = FALSE;
	}
	Stream_Free(s, TRUE);
	return CHANNEL_RC_OK;
}

static BOOL post_chunked(void* handler, Received* r, const BYTE* message, size_t length,
                         size_t chunk)
{
	r->expected = length;
	r->delivered = 0;
	r->ok = FALSE;

	for (size_t offset = 0; offset < length; offset += chunk)
	{
		const size_t size = MIN(chunk, length - offset);
		UINT32 flags = 0;
		if (offset == 0)
			flags |= CHANNEL_FLAG_FIRST;
		if (offset + size == length)
			flags |= CHANNEL_FLAG_LAST;

		const UINT rc = channel_client_post_message(handler, (LPVOID)&message[offset], (UINT32)size,
		                                            (UINT32)length, flags);
		if (rc != CHANNEL_RC_OK)
		{
			(void)fprintf(stderr, "post failed [%" PRIu32 "] at offset %" PRIuz " of %" PRIuz "\n",
			              rc, offset, length);
			return FALSE;
		}
	}

	if ((r->delivered != 1) || !r->ok)
	{
		(void)fprintf(stderr,
		              "a %" PRIuz " byte message in %" PRIuz " byte chunks was not reassembled\n",
		              length, chunk);
		return FALSE;
	}
	return TRUE;
}

int TestChannelReassembly(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;
	const size_t big = 32ull * 1024ull * 1024ull;
	BYTE* message = malloc(big);
	rdpContext context = WINPR_C_ARRAY_INIT;
	void* handler = nullptr;
	Received received = WINPR_C_ARRAY_INIT;

	context.settings = freerdp_settings_new(0);
	if (!message || !context.settings)
		goto fail;
	for (size_t x = 0; x < big; x++)
		message[x] = pattern(x);

	/* messages are delivered from this thread, as soon as their last chunk is posted */
	if (!freerdp_settings_set_uint32(context.settings, FreeRDP_ThreadingFlags,
	                                 THREADING_FLAGS_DISABLE_THREADS))
		goto fail;
	handler = channel_client_create_handler(&context, &received, on_message, "test");
	if (!handler)
		goto fail;

	{
		const size_t lengths[] = { 1, 127, 128, 1599, 1600, 1601, 3200, 4800, 65535, 1048577 };
		const size_t chunks[] = { 1600, 1599, 64, 7 };
		for (size_t x = 0; x < ARRAYSIZE(lengths); x++)
		{
			for (size_t y = 0; y < ARRAYSIZE(chunks); y++)
			{
				if ((chunks[y] < 64) && (lengths[x] > 65535))
					continue; /* too many chunks to be worth the time */
				if (!post_chunked(handler, &received, message, lengths[x], chunks[y]))
					goto fail;
			}
		}
	}

	{
		/* A large message in the usual chunk size: growing the buffer one chunk at a time made
		 * this quadratic in the message size. */
		const UINT64 start = GetTickCount64();
		if (!post_chunked(handler, &received, message, big, 1600))
			goto fail;
		(void)printf("%" PRIuz " MiB reassembled from 1600 byte chunks in %" PRIu64 " ms\n",
		             big / (1024 * 1024), GetTickCount64() - start);
	}

	rc = 0;
fail:
	if (handler)
		(void)channel_client_quit_handler(handler);
	freerdp_settings_free(context.settings);
	free(message);
	return rc;
}
