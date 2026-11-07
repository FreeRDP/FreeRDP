/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Fuzzer for the server side of RDP-UDP
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

/* What a client can send to the shared UDP port of the server, before and after the server accepts
 * it:
 * - the whole input as a SYN, for rdpudp_get_syn_cookie_hash() and rdpudp_accept()
 * - a valid version 3 SYN, then the input as steps. Each step starts with a 16 bit big endian
 *   value. A value below 0x8000 is the length of a datagram for rdpudp_input(), which follows it.
 *   0xFFFF tells the connection that the client proved its cookie. Other values move the clock
 *   forward by the low 15 bits in milliseconds and run the timers. The timers also run after each
 *   eighth datagram.
 *
 * A server connection has no socket, so what it sends goes nowhere. */

#include <stdint.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include "../rdpudp.h"

#define SYN_LENGTH 1232

typedef struct
{
	BOOL ready;
	wLog* log;
	UINT64 now; /* nanoseconds, the fuzzer moves this clock */
} FuzzCtx;

static FuzzCtx g_ctx;

static UINT64 fuzz_clock(WINPR_ATTR_UNUSED void* custom)
{
	return g_ctx.now;
}

static BOOL on_receive(WINPR_ATTR_UNUSED void* custom, WINPR_ATTR_UNUSED const BYTE* data,
                       WINPR_ATTR_UNUSED size_t length)
{
	return TRUE;
}

static BOOL on_send(WINPR_ATTR_UNUSED void* custom, WINPR_ATTR_UNUSED const BYTE* data,
                    WINPR_ATTR_UNUSED size_t length)
{
	return TRUE;
}

static BOOL fuzz_init(void)
{
	if (g_ctx.ready)
		return TRUE;

	WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
	g_ctx.log = WLog_Get("com.freerdp.core.test.fuzz.rdpudp");
	g_ctx.ready = TRUE;
	return TRUE;
}

/* [MS-RDPEUDP] 2.2.2.1, 2.2.2.5 and 2.2.2.6: a version 3 SYN, with zero padding to the MTU */
static void make_syn(BYTE syn[SYN_LENGTH])
{
	memset(syn, 0, SYN_LENGTH);
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, syn, SYN_LENGTH);
	Stream_Write_UINT32_BE(s, 0xFFFFFFFF);      /* snSourceAck */
	Stream_Write_UINT16_BE(s, 64);              /* uReceiveWindowSize */
	Stream_Write_UINT16_BE(s, 0x0001 | 0x1000); /* RDPUDP_FLAG_SYN | RDPUDP_FLAG_SYNEX */
	Stream_Write_UINT32_BE(s, 1000);            /* snInitialSequenceNumber */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);      /* uUpStreamMtu */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);      /* uDownStreamMtu */
	Stream_Write_UINT16_BE(s, 0x0001);          /* RDPUDP_VERSION_INFO_VALID */
	Stream_Write_UINT16_BE(s, RDPUDP_PROTOCOL_VERSION_3);
	/* cookieHash: zero */
}

static void fuzz_syn(const uint8_t* data, size_t size)
{
	BYTE hash[32] = WINPR_C_ARRAY_INIT;
	(void)rdpudp_get_syn_cookie_hash(data, size, hash);

	rdpUdp* udp = rdpudp_new(g_ctx.log, nullptr, on_receive, nullptr);
	if (!udp)
		return;
	(void)rdpudp_accept(udp, on_send, nullptr, data, size);
	rdpudp_free(udp);
}

static void fuzz_datagrams(const uint8_t* data, size_t size)
{
	rdpUdp* udp = rdpudp_new(g_ctx.log, nullptr, on_receive, nullptr);
	if (!udp)
		return;

	/* each input starts at the same time, thus a crash repeats */
	g_ctx.now = 1000000000000ULL;
	rdpudp_set_clock(udp, fuzz_clock, nullptr);

	BYTE syn[SYN_LENGTH];
	make_syn(syn);
	if (!rdpudp_accept(udp, on_send, nullptr, syn, sizeof(syn)))
		goto out;

	size_t offset = 0;
	size_t count = 0;
	while (size - offset >= 2)
	{
		const size_t length = ((size_t)data[offset] << 8) | data[offset + 1];
		offset += 2;
		if (length == 0xFFFF)
		{
			rdpudp_set_peer_proven(udp);
			continue;
		}
		if (length >= 0x8000)
		{
			g_ctx.now += (length & 0x7FFF) * 1000000ULL;
			if (!rdpudp_check(udp))
				break;
			continue;
		}
		if (length > size - offset)
			break;
		/* rdpudp_input works in place */
		BYTE datagram[0x8000];
		memcpy(datagram, &data[offset], length);
		if (!rdpudp_input(udp, datagram, length))
			break;
		offset += length;
		if ((++count % 8) == 0)
		{
			if (!rdpudp_check(udp))
				break;
		}
	}

out:
	rdpudp_free(udp);
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);
int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	if (!fuzz_init())
		return 0;
	fuzz_syn(Data, Size);
	fuzz_datagrams(Data, Size);
	return 0;
}
