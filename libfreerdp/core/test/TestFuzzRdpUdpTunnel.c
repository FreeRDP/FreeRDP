/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Fuzzer for what a UDP client can send past RDP-UDP on a server
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

/* The first byte selects what gets the rest of the input:
 * - tunnel PDUs, as TLS gives them to the tunnel of a server. This test does not use TLS, because a
 * fuzzer cannot get through it. The bits above tell if the tunnel is already ready.
 * - the listener, as datagrams of up to 3 clients. A client number and a 16 bit big endian length
 * come before each datagram. A SYN can claim a flow.
 * - a Soft-Sync Response, and tunnel data before and after it */

/* udplistener.c needs these before the first system header */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif
#if defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)
#define __APPLE_USE_RFC_3542 // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif

#include <stdint.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/winsock.h>
#include <winpr/wlog.h>

#include "../rdpemt.c"
#undef TAG
#include "../udplistener.c"
#undef TAG
#include "../server_tunnel.c"

static wLog* g_log;
static SSL_CTX* g_ctx;
static rdpUdp* g_udp;

static BOOL on_send(WINPR_ATTR_UNUSED void* custom, WINPR_ATTR_UNUSED const BYTE* data,
                    WINPR_ATTR_UNUSED size_t length)
{
	return TRUE;
}

static BOOL on_receive(WINPR_ATTR_UNUSED void* custom, WINPR_ATTR_UNUSED const BYTE* data,
                       WINPR_ATTR_UNUSED size_t length)
{
	return TRUE;
}

static void on_event(WINPR_ATTR_UNUSED void* custom, WINPR_ATTR_UNUSED rdpEmt* emt,
                     WINPR_ATTR_UNUSED RDPEMT_EVENT event, const BYTE* data, size_t length)
{
	/* the receiver reads all of it */
	volatile BYTE sum = 0;
	for (size_t x = 0; x < length; x++)
		sum ^= data[x];
}

static BOOL on_read_pdu(WINPR_ATTR_UNUSED rdpPeerChannel* channel, wStream* s,
                        WINPR_ATTR_UNUSED UINT8 Cmd, WINPR_ATTR_UNUSED UINT8 Sp,
                        WINPR_ATTR_UNUSED UINT8 cbChId)
{
	return Stream_GetRemainingLength(s) < 1000;
}

/* [MS-RDPEUDP] 2.2.2.1, 2.2.2.5 and 2.2.2.6: a version 3 SYN, with zero padding to the MTU */
static void make_syn(BYTE syn[1232])
{
	memset(syn, 0, 1232);
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, syn, 1232);
	Stream_Write_UINT32_BE(s, 0xFFFFFFFF);      /* snSourceAck */
	Stream_Write_UINT16_BE(s, 64);              /* uReceiveWindowSize */
	Stream_Write_UINT16_BE(s, 0x0001 | 0x1000); /* RDPUDP_FLAG_SYN | RDPUDP_FLAG_SYNEX */
	Stream_Write_UINT32_BE(s, 1000);            /* snInitialSequenceNumber */
	Stream_Write_UINT16_BE(s, 1232);            /* uUpStreamMtu */
	Stream_Write_UINT16_BE(s, 1232);            /* uDownStreamMtu */
	Stream_Write_UINT16_BE(s, 0x0001);          /* RDPUDP_VERSION_INFO_VALID */
	Stream_Write_UINT16_BE(s, RDPUDP_PROTOCOL_VERSION_3);
}

static BOOL fuzz_init(void)
{
	if (g_log)
		return TRUE;

	WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
	g_ctx = SSL_CTX_new(TLS_server_method());
	g_udp = rdpudp_new(WLog_Get("com.freerdp.core.test.fuzz.rdpemt"), nullptr, on_receive, nullptr);
	if (!g_ctx || !g_udp)
		return FALSE;

	BYTE syn[1232];
	make_syn(syn);
	if (!rdpudp_accept(g_udp, on_send, nullptr, syn, sizeof(syn)))
		return FALSE;
	g_log = WLog_Get("com.freerdp.core.test.fuzz.rdpemt");
	return TRUE;
}

static void fuzz_tunnel(BYTE flags, const uint8_t* data, size_t size)
{
	rdpEmt emt = WINPR_C_ARRAY_INIT;
	emt.log = g_log;
	emt.server = TRUE;
	emt.requestId = 0x01020304;
	emt.callback = on_event;
	emt.udp = g_udp;
	emt.ready = (flags & 0x10) != 0;
	emt.tlsStarted = TRUE;
	emt.handshakeDone = TRUE;
	emt.sendLimit = 1024 * 1024;
	emt.ssl = SSL_new(g_ctx);
	emt.rbio = BIO_new(BIO_s_mem());
	emt.wbio = BIO_new(BIO_s_mem());
	emt.in = Stream_New(nullptr, size + 1);
	emt.out = Stream_New(nullptr, 4096);
	emt.hold = Queue_New(FALSE, -1, -1);
	if (!emt.ssl || !emt.rbio || !emt.wbio || !emt.in || !emt.out || !emt.hold ||
	    !InitializeCriticalSectionAndSpinCount(&emt.lock, 4000))
		return;
	SSL_set_bio(emt.ssl, emt.rbio, emt.wbio);
	SSL_set_accept_state(emt.ssl);

	Stream_Write(emt.in, data, size);
	(void)emt_process_pdus(&emt);

	wStream* s = nullptr;
	while ((s = Queue_Dequeue(emt.hold)))
		Stream_Free(s, TRUE);
	Queue_Free(emt.hold);
	SSL_free(emt.ssl);
	Stream_Free(emt.in, TRUE);
	Stream_Free(emt.out, TRUE);
	Stream_Free(emt.batch, TRUE);
	DeleteCriticalSection(&emt.lock);
}

static void fuzz_listener(const uint8_t* data, size_t size)
{
	static const BYTE hash[32] = WINPR_C_ARRAY_INIT;
	static const BYTE other[32] = { 1 };

	freerdp_udp_listener* listener = freerdp_udp_listener_new();
	rdpUdpFlow* flow = freerdp_udp_flow_new(hash);
	rdpUdpFlow* second = freerdp_udp_flow_new(other);
	if (!listener || !flow || !second || !freerdp_udp_listener_expect(listener, flow) ||
	    !freerdp_udp_listener_expect(listener, second))
		goto out;

	const udpListenerSocket sock = { .sockfd = INVALID_SOCKET };
	size_t offset = 0;
	while (size - offset >= 3)
	{
		struct sockaddr_storage addr = WINPR_C_ARRAY_INIT;
		struct sockaddr_in* in = (struct sockaddr_in*)&addr;
		in->sin_family = AF_INET;
		in->sin_port = (u_short)(data[offset] % 3);
		const udpLocalAddress local = WINPR_C_ARRAY_INIT;
		const size_t length = ((size_t)data[offset + 1] << 8) | data[offset + 2];
		offset += 3;
		if (length > size - offset)
			break;
		udplistener_dispatch(listener, &sock, &addr, sizeof(*in), &local, &data[offset], length);
		offset += length;

		BYTE datagram[UDPFLOW_MAX_DATAGRAM];
		size_t got = 0;
		while (udpflow_read(flow, datagram, &got) || udpflow_read(second, datagram, &got))
		{
		}
	}

out:
	freerdp_udp_flow_free(flow);
	freerdp_udp_flow_free(second);
	freerdp_udp_listener_free(listener);
}

static void fuzz_soft_sync(BYTE flags, const uint8_t* data, size_t size)
{
	struct s_wts_tunnel tunnel = WINPR_C_ARRAY_INIT;
	rdpPeerChannel channel = WINPR_C_ARRAY_INIT;
	WTSVirtualChannelManager vcm = WINPR_C_ARRAY_INIT;
	vcm.tunnel = &tunnel;
	vcm.drdynvc_channel = &channel;
	vcm.drdynvc_state = DRDYNVC_STATE_READY;
	tunnel.readPdu = on_read_pdu;
	tunnel.softSyncState = (flags & 0x10) ? SOFT_SYNC_STATE_SENT : SOFT_SYNC_STATE_ANSWERED;
	tunnel.clientOnTunnel = (flags & 0x20) != 0;
	tunnel.tunnelBacklog = Queue_New(FALSE, -1, -1);
	if (!tunnel.tunnelBacklog)
		return;

	/* the first half arrives on the tunnel, the rest answers the Soft-Sync */
	const size_t half = size / 2;
	wStream* early = Stream_New(nullptr, half + 1);
	if (early)
	{
		Stream_Write(early, data, half);
		Stream_SealLength(early);
		Stream_ResetPosition(early);
		(void)wts_tunnel_data_received(&vcm, TUNNELTYPE_UDPFECR, early);
	}

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, &data[half], size - half);
	if (s)
		(void)wts_tunnel_read_soft_sync_response(&vcm, s);

	wStream* held = nullptr;
	while ((held = Queue_Dequeue(tunnel.tunnelBacklog)))
		Stream_Release(held);
	Queue_Free(tunnel.tunnelBacklog);
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size);
int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	if ((Size < 1) || !fuzz_init())
		return 0;

	switch (Data[0] & 0x03)
	{
		case 0:
			fuzz_tunnel(Data[0], &Data[1], Size - 1);
			break;
		case 1:
			fuzz_listener(&Data[1], Size - 1);
			break;
		default:
			fuzz_soft_sync(Data[0], &Data[1], Size - 1);
			break;
	}
	return 0;
}
