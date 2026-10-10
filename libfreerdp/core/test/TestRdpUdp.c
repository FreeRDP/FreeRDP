/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP-UDP transport unit test
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

/* The client side of RDP-UDP against a server played by the test over a loopback socket.
 *
 * Nothing in rdpudp.c is opened up for this: the test answers the SYN, sends the datagrams a
 * Windows server would, and reads what the client delivers through its receive callback and
 * sends back on the wire. */

#include <stdio.h>
#include <string.h>

#include <winpr/wtypes.h>
#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>
#include <winpr/winsock.h>
#include <winpr/wlog.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "../rdpudp.h"

#define TAG "com.freerdp.core.test.rdpudp"

/* [MS-RDPEUDP] 2.2.2.1 uFlags */
#define FLAG_SYN 0x0001
#define FLAG_ACK 0x0004
#define FLAG_SYNEX 0x1000

/* [MS-RDPEUDP2] 2.2.1.1 Flags */
#define V3_ACK 0x001
#define V3_DATA 0x004
#define V3_ACKVEC 0x008
#define V3_AOA 0x010
#define V3_OVERHEADSIZE 0x040
#define V3_DELAYACKINFO 0x100

#define V3_TYPE_STANDARD 0
#define V3_TYPE_DUMMY 8

#define SYN_LENGTH 1232
#define FIRST_SEQUENCE 100

#define CHECK(cond)                                                                 \
	do                                                                              \
	{                                                                               \
		if (!(cond))                                                                \
		{                                                                           \
			(void)fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			              #cond);                                                   \
			goto fail;                                                              \
		}                                                                           \
	} while (0)

/* A datagram the client sent, decoded. */
typedef struct
{
	UINT16 flags;
	BYTE logWindow;
	UINT16 ack;
	BOOL hasAoA;
	UINT16 aoa;
	BOOL hasData;
	UINT16 dataSeq;
	UINT16 channelSeq;
	BYTE payload[64];
	size_t payloadLength;
	UINT16 vecBase;
	BYTE vec[127];
	size_t vecLength;
} V3Packet;

#define MAX_SEEN 256

typedef struct
{
	SOCKET server;
	struct sockaddr_in serverAddr;
	rdpUdp* udp;
	wStream* delivered;

	/* what the client sent since the last drain, the newest MAX_SEEN of it */
	V3Packet seen[MAX_SEEN];
	size_t seenCount;
} TestCtx;

/* ---------------------------------------------------------------------------------------- */
/* The server's socket                                                                       */
/* ---------------------------------------------------------------------------------------- */

static BOOL on_receive(void* custom, const BYTE* data, size_t length)
{
	TestCtx* ctx = custom;
	if (!Stream_EnsureRemainingCapacity(ctx->delivered, length))
		return FALSE;
	Stream_Write(ctx->delivered, data, length);
	return TRUE;
}

static BOOL server_open(TestCtx* ctx)
{
	ctx->server = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ctx->server == INVALID_SOCKET)
		return FALSE;

	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;
	if (_bind(ctx->server, (const struct sockaddr*)&addr, sizeof(addr)) != 0)
		return FALSE;

	socklen_t length = sizeof(ctx->serverAddr);
	if (_getsockname(ctx->server, (struct sockaddr*)&ctx->serverAddr, &length) != 0)
		return FALSE;

	u_long arg = 1;
	return _ioctlsocket(ctx->server, FIONBIO, &arg) == 0;
}

/* One datagram from the client, waiting up to timeoutMs for it. 0 when none came. */
static size_t server_recv(TestCtx* ctx, BYTE* buffer, size_t size, UINT32 timeoutMs)
{
	const UINT64 deadline = GetTickCount64() + timeoutMs;
	while (TRUE)
	{
		const int rc = _recv(ctx->server, (char*)buffer, (int)size, 0);
		if (rc > 0)
			return (size_t)rc;
		if (GetTickCount64() >= deadline)
			return 0;
		Sleep(1);
	}
}

static BOOL server_send_raw(TestCtx* ctx, const BYTE* data, size_t length)
{
	return _send(ctx->server, (const char*)data, (int)length, 0) == (int)length;
}

/* Lets the client read what arrived and run its timers. */
static BOOL pump(TestCtx* ctx)
{
	Sleep(1);
	return rdpudp_check(ctx->udp);
}

/* ---------------------------------------------------------------------------------------- */
/* RDP-UDP2 packets ([MS-RDPEUDP2] 2.2.1)                                                    */
/* ---------------------------------------------------------------------------------------- */

static void write_uint24(wStream* s, UINT32 value)
{
	Stream_Write_UINT8(s, (BYTE)(value & 0xFF));
	Stream_Write_UINT8(s, (BYTE)((value >> 8) & 0xFF));
	Stream_Write_UINT8(s, (BYTE)((value >> 16) & 0xFF));
}

/* What the test server puts in one packet. */
typedef struct
{
	BOOL dummy;
	BOOL hasAck;
	UINT16 ack;
	BOOL hasAoA;
	UINT16 aoa;
	BOOL hasData;
	UINT16 dataSeq;
	UINT16 channelSeq;
	const BYTE* payload;
	size_t payloadLength;
} V3Out;

/* Encodes the packet the way Windows puts it on the wire: prefix byte in front, a short layout
 * padded to 7 bytes, then the first and eighth byte swapped. */
static BOOL server_send_v3(TestCtx* ctx, const V3Out* out)
{
	BYTE wire[1500] = { 0 };
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, wire, sizeof(wire));

	UINT16 flags = 0;
	if (out->hasAck)
		flags |= V3_ACK;
	if (out->hasAoA)
		flags |= V3_AOA;
	if (out->hasData)
		flags |= V3_DATA;

	Stream_Seek(s, 1); /* the prefix byte */
	Stream_Write_UINT16(s, (UINT16)(flags | (15u << 12)));
	if (out->hasAck)
	{
		Stream_Write_UINT16(s, out->ack); /* SeqNum */
		write_uint24(s, 0);               /* receivedTS */
		Stream_Write_UINT8(s, 0);         /* sendAckTimeGapInMs */
		Stream_Write_UINT8(s, 0);         /* numDelayedAcks */
	}
	if (out->hasAoA)
		Stream_Write_UINT16(s, out->aoa);
	if (out->hasData)
	{
		Stream_Write_UINT16(s, out->dataSeq);
		Stream_Write_UINT16(s, out->channelSeq);
		Stream_Write(s, out->payload, out->payloadLength);
	}

	const size_t layout = Stream_GetPosition(s) - 1;
	BYTE shortLength = 7;
	if (layout < 7)
	{
		shortLength = (BYTE)layout;
		Stream_Zero(s, 7 - layout);
	}
	const size_t length = Stream_GetPosition(s);
	const BYTE type = out->dummy ? V3_TYPE_DUMMY : V3_TYPE_STANDARD;
	wire[0] = (BYTE)((shortLength << 5) | (type << 1));
	const BYTE eighth = wire[7];
	wire[7] = wire[0];
	wire[0] = eighth;
	return server_send_raw(ctx, wire, length);
}

static BOOL send_data(TestCtx* ctx, UINT16 dataSeq, UINT16 channelSeq, const char* text)
{
	const V3Out out = { .hasData = TRUE,
		                .dataSeq = dataSeq,
		                .channelSeq = channelSeq,
		                .payload = (const BYTE*)text,
		                .payloadLength = strlen(text) };
	return server_send_v3(ctx, &out);
}

static BOOL send_aoa(TestCtx* ctx, UINT16 aoa)
{
	const V3Out out = { .hasAoA = TRUE, .aoa = aoa };
	return server_send_v3(ctx, &out);
}

static BOOL send_ack(TestCtx* ctx, UINT16 ack)
{
	const V3Out out = { .hasAck = TRUE, .ack = ack };
	return server_send_v3(ctx, &out);
}

static BOOL parse_v3(const BYTE* wire, size_t length, V3Packet* packet)
{
	BYTE buffer[1500] = { 0 };
	if ((length < 8) || (length > sizeof(buffer)))
		return FALSE;
	memcpy(buffer, wire, length);
	const BYTE eighth = buffer[7];
	buffer[7] = buffer[0];
	buffer[0] = eighth;

	memset(packet, 0, sizeof(*packet));
	const BYTE prefix = buffer[0];
	const BYTE shortLength = prefix >> 5;
	if (shortLength < 7)
		length = 1ull + shortLength;

	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, buffer + 1, length - 1);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;
	const UINT16 header = Stream_Get_UINT16(s);
	packet->flags = header & 0x0FFF;
	packet->logWindow = (BYTE)(header >> 12);

	if (packet->flags & V3_ACK)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 7))
			return FALSE;
		packet->ack = Stream_Get_UINT16(s);
		Stream_Seek(s, 4); /* receivedTS, sendAckTimeGapInMs */
		const size_t delayed = Stream_Get_UINT8(s) & 0x0F;
		if (!Stream_SafeSeek(s, delayed))
			return FALSE;
	}
	if ((packet->flags & V3_OVERHEADSIZE) && !Stream_SafeSeek(s, 1))
		return FALSE;
	if ((packet->flags & V3_DELAYACKINFO) && !Stream_SafeSeek(s, 3))
		return FALSE;
	if (packet->flags & V3_AOA)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
			return FALSE;
		packet->hasAoA = TRUE;
		packet->aoa = Stream_Get_UINT16(s);
	}
	if (packet->flags & V3_DATA)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
			return FALSE;
		packet->dataSeq = Stream_Get_UINT16(s);
	}
	if (packet->flags & V3_ACKVEC)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 3))
			return FALSE;
		packet->vecBase = Stream_Get_UINT16(s);
		const BYTE control = Stream_Get_UINT8(s);
		if ((control & 0x80) && !Stream_SafeSeek(s, 4)) /* TimeStamp, SendAckTimeGapInMs */
			return FALSE;
		packet->vecLength = control & 0x7F;
		if (!Stream_CheckAndLogRequiredLength(TAG, s, packet->vecLength))
			return FALSE;
		Stream_Read(s, packet->vec, packet->vecLength);
	}
	if (packet->flags & V3_DATA)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
			return FALSE;
		packet->hasData = TRUE;
		packet->channelSeq = Stream_Get_UINT16(s);
		packet->payloadLength = Stream_GetRemainingLength(s);
		if (packet->payloadLength > sizeof(packet->payload))
			packet->payloadLength = sizeof(packet->payload);
		Stream_Read(s, packet->payload, packet->payloadLength);
	}
	return TRUE;
}

/* Reads everything the client sent, until nothing more arrives for waitMs. */
static BOOL drain(TestCtx* ctx, UINT32 waitMs)
{
	BYTE buffer[1500] = { 0 };
	ctx->seenCount = 0;
	size_t length = 0;
	while ((length = server_recv(ctx, buffer, sizeof(buffer), waitMs)) > 0)
	{
		V3Packet packet = { 0 };
		if (!parse_v3(buffer, length, &packet))
			return FALSE;
		if (ctx->seenCount == MAX_SEEN)
		{
			memmove(&ctx->seen[0], &ctx->seen[1], sizeof(ctx->seen[0]) * (MAX_SEEN - 1));
			ctx->seenCount--;
		}
		ctx->seen[ctx->seenCount++] = packet;
	}
	return TRUE;
}

/* Reads what the client sent until want DATA packets are in, or timeoutMs passed. */
static BOOL collect_data(TestCtx* ctx, size_t want, UINT32 timeoutMs)
{
	BYTE buffer[1500] = { 0 };
	ctx->seenCount = 0;
	size_t count = 0;
	while (count < want)
	{
		const size_t length = server_recv(ctx, buffer, sizeof(buffer), timeoutMs);
		if (length == 0)
			return FALSE;
		if (ctx->seenCount == MAX_SEEN)
			return FALSE;
		V3Packet* packet = &ctx->seen[ctx->seenCount++];
		if (!parse_v3(buffer, length, packet))
			return FALSE;
		if (packet->hasData)
			count++;
	}
	return TRUE;
}

/* The newest acknowledgement the client sent, ACK or ACK vector. */
static const V3Packet* last_ack(const TestCtx* ctx)
{
	for (size_t x = ctx->seenCount; x > 0; x--)
	{
		const V3Packet* packet = &ctx->seen[x - 1];
		if (packet->flags & (V3_ACK | V3_ACKVEC))
			return packet;
	}
	return nullptr;
}

static size_t count_data(const TestCtx* ctx)
{
	size_t count = 0;
	for (size_t x = 0; x < ctx->seenCount; x++)
	{
		if (ctx->seen[x].hasData)
			count++;
	}
	return count;
}

static const V3Packet* first_data(const TestCtx* ctx)
{
	for (size_t x = 0; x < ctx->seenCount; x++)
	{
		if (ctx->seen[x].hasData)
			return &ctx->seen[x];
	}
	return nullptr;
}

static BOOL delivered_is(const TestCtx* ctx, const char* expected)
{
	const size_t length = strlen(expected);
	return (Stream_GetPosition(ctx->delivered) == length) &&
	       (memcmp(Stream_Buffer(ctx->delivered), expected, length) == 0);
}

/* What was delivered after the first offset bytes. */
static BOOL delivered_tail_is(const TestCtx* ctx, size_t offset, const char* expected)
{
	const size_t length = strlen(expected);
	return (Stream_GetPosition(ctx->delivered) == offset + length) &&
	       (memcmp(Stream_Buffer(ctx->delivered) + offset, expected, length) == 0);
}

/* ---------------------------------------------------------------------------------------- */
/* Connection setup                                                                          */
/* ---------------------------------------------------------------------------------------- */

static const BYTE cookieHash[32] = { 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	                                 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	                                 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	                                 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA };

typedef struct
{
	size_t length;
	UINT16 flags;
	UINT32 initialSequence;
	UINT16 version;
	BYTE cookie[32];
} SynInfo;

static void ctx_free(TestCtx* ctx)
{
	rdpudp_free(ctx->udp);
	if (ctx->server != INVALID_SOCKET)
		closesocket(ctx->server);
	Stream_Free(ctx->delivered, TRUE);
	memset(ctx, 0, sizeof(*ctx));
	ctx->server = INVALID_SOCKET;
}

/* Starts the client and reads its SYN, the connection is left in SYN_SENT. */
static BOOL ctx_start(TestCtx* ctx, BOOL offerV3, SynInfo* syn)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->server = INVALID_SOCKET;
	ctx->delivered = Stream_New(nullptr, 1024);
	if (!ctx->delivered || !server_open(ctx))
		return FALSE;

	ctx->udp = rdpudp_new(WLog_Get(TAG), offerV3 ? cookieHash : nullptr, on_receive, ctx);
	if (!ctx->udp)
		return FALSE;
	if (!rdpudp_connect(ctx->udp, (const struct sockaddr*)&ctx->serverAddr,
	                    sizeof(ctx->serverAddr)))
		return FALSE;

	BYTE buffer[1500] = { 0 };
	struct sockaddr_in from = { 0 };
	socklen_t fromLength = sizeof(from);
	const UINT64 deadline = GetTickCount64() + 1000;
	int rc = -1;
	while ((rc = _recvfrom(ctx->server, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&from,
	                       &fromLength)) <= 0)
	{
		if (GetTickCount64() >= deadline)
			return FALSE;
		Sleep(1);
	}
	if (_connect(ctx->server, (const struct sockaddr*)&from, fromLength) != 0)
		return FALSE;

	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, buffer, (size_t)rc);
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return FALSE;
	syn->length = (size_t)rc;
	Stream_Seek(s, 6); /* snSourceAck, uReceiveWindowSize */
	syn->flags = Stream_Get_UINT16_BE(s);
	syn->initialSequence = Stream_Get_UINT32_BE(s);
	Stream_Seek(s, 6); /* MTUs, uSynExFlags */
	syn->version = Stream_Get_UINT16_BE(s);
	if (Stream_GetRemainingLength(s) >= sizeof(syn->cookie))
		Stream_Read(s, syn->cookie, sizeof(syn->cookie));
	return TRUE;
}

static BOOL send_syn_ack(TestCtx* ctx, UINT32 acknowledged, UINT16 version)
{
	BYTE wire[20] = { 0 };
	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticInit(&sbuffer, wire, sizeof(wire));
	Stream_Write_UINT32_BE(s, acknowledged); /* snSourceAck */
	Stream_Write_UINT16_BE(s, 64);           /* uReceiveWindowSize */
	Stream_Write_UINT16_BE(s, FLAG_SYN | FLAG_ACK | FLAG_SYNEX);
	Stream_Write_UINT32_BE(s, 0x1000); /* snInitialSequenceNumber */
	Stream_Write_UINT16_BE(s, SYN_LENGTH);
	Stream_Write_UINT16_BE(s, SYN_LENGTH);
	Stream_Write_UINT16_BE(s, 0x0001); /* RDPUDP_VERSION_INFO_VALID */
	Stream_Write_UINT16_BE(s, version);
	return server_send_raw(ctx, wire, sizeof(wire));
}

/* A client connected with RDP-UDP version 3, the way a Windows server answers. */
static BOOL ctx_connect(TestCtx* ctx)
{
	SynInfo syn = { 0 };
	if (!ctx_start(ctx, TRUE, &syn))
		return FALSE;
	if (!send_syn_ack(ctx, syn.initialSequence, RDPUDP_PROTOCOL_VERSION_3))
		return FALSE;
	for (size_t x = 0; x < 100; x++)
	{
		if (!pump(ctx))
			return FALSE;
		if (rdpudp_get_state(ctx->udp) == RDPUDP_STATE_ESTABLISHED)
			return rdpudp_get_version(ctx->udp) == RDPUDP_PROTOCOL_VERSION_3;
	}
	return FALSE;
}

/* Delivers count chunks of one byte from the server, the stream moving on from channel
 * sequence *channel and data sequence *dataSeq. A batch stays well inside the socket buffer of
 * the client, which loses datagrams beyond it like any other. */
static BOOL stream_ahead(TestCtx* ctx, UINT16* dataSeq, UINT16* channel, size_t count)
{
	while (count > 0)
	{
		const size_t batch = (count > 64) ? 64 : count;
		for (size_t x = 0; x < batch; x++)
		{
			if (!send_data(ctx, *dataSeq, *channel, "."))
				return FALSE;
			(*dataSeq)++;
			(*channel)++;
		}
		if (!pump(ctx) || !drain(ctx, 0))
			return FALSE;
		count -= batch;
	}
	return TRUE;
}

/* ---------------------------------------------------------------------------------------- */
/* The SYN exchange                                                                          */
/* ---------------------------------------------------------------------------------------- */

static BOOL syn_offers_version_3_with_the_cookie_hash(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	SynInfo syn = { 0 };
	CHECK(ctx_start(&ctx, TRUE, &syn));
	/* [MS-RDPEUDP] 3.1.5.1.1: zero padded to the MTU, which validates the path MTU */
	CHECK(syn.length == SYN_LENGTH);
	CHECK((syn.flags & (FLAG_SYN | FLAG_SYNEX)) == (FLAG_SYN | FLAG_SYNEX));
	CHECK(syn.version == RDPUDP_PROTOCOL_VERSION_3);
	CHECK(memcmp(syn.cookie, cookieHash, sizeof(cookieHash)) == 0);
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

static BOOL syn_offers_version_2_without_a_cookie_hash(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	SynInfo syn = { 0 };
	CHECK(ctx_start(&ctx, FALSE, &syn));
	CHECK(syn.length == SYN_LENGTH);
	CHECK(syn.version == RDPUDP_PROTOCOL_VERSION_2);
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* ---------------------------------------------------------------------------------------- */
/* Receiving                                                                                 */
/* ---------------------------------------------------------------------------------------- */

static BOOL dummy_packets_are_acknowledged_but_not_delivered(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));

	const V3Out dummy = { .dummy = TRUE,
		                  .hasData = TRUE,
		                  .dataSeq = FIRST_SEQUENCE,
		                  .channelSeq = 1,
		                  .payload = (const BYTE*)"junk",
		                  .payloadLength = 4 };
	CHECK(server_send_v3(&ctx, &dummy));
	CHECK(pump(&ctx));
	CHECK(delivered_is(&ctx, ""));

	/* the dummy took no channel sequence, this is the first chunk of the stream */
	CHECK(send_data(&ctx, FIRST_SEQUENCE + 1, 1, "payload"));
	CHECK(pump(&ctx));
	CHECK(delivered_is(&ctx, "payload"));

	CHECK(drain(&ctx, 20));
	const V3Packet* ack = last_ack(&ctx);
	CHECK(ack && (ack->flags & V3_ACK));
	CHECK(ack->ack == FIRST_SEQUENCE + 1);
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* A lost chunk comes back under a new data sequence number with its original channel sequence,
 * so it arrives after the chunks that follow it. Handing the stream up in arrival order
 * corrupts the TLS record layer above. */
static BOOL retransmission_is_reordered_back_into_the_stream(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	CHECK(send_data(&ctx, 100, 1, "a"));
	CHECK(send_data(&ctx, 101, 3, "c"));
	CHECK(send_data(&ctx, 102, 4, "d"));
	CHECK(pump(&ctx));
	CHECK(delivered_is(&ctx, "a"));

	CHECK(send_data(&ctx, 103, 2, "b"));
	CHECK(pump(&ctx));
	CHECK(delivered_is(&ctx, "abcd"));
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* An AckOfAcks writes off a data sequence number the peer stopped resending (a dummy). It moves
 * the window, the stream is ordered by channel sequence and stays as it is. */
static BOOL ack_of_acks_advances_the_window_without_touching_the_stream(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	CHECK(send_data(&ctx, 100, 1, "a"));
	/* 101 is a dummy that never arrives */
	CHECK(send_data(&ctx, 102, 2, "b"));
	CHECK(pump(&ctx));
	CHECK(delivered_is(&ctx, "ab"));
	CHECK(drain(&ctx, 20));
	const V3Packet* ack = last_ack(&ctx);
	CHECK(ack && (ack->flags & V3_ACKVEC) && (ack->vecBase == 101));

	CHECK(send_aoa(&ctx, 102));
	CHECK(pump(&ctx));
	CHECK(drain(&ctx, 20));
	ack = last_ack(&ctx);
	CHECK(ack && (ack->flags & V3_ACK));
	CHECK(ack->ack == 102);
	CHECK(delivered_is(&ctx, "ab"));
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* The peer resends a packet when our acknowledgement got lost. Staying silent would make it
 * resend until it gives up. */
static BOOL retransmitted_packets_are_acknowledged_again(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	CHECK(send_data(&ctx, 100, 1, "first"));
	CHECK(pump(&ctx));
	CHECK(drain(&ctx, 20));
	CHECK(last_ack(&ctx));

	CHECK(send_data(&ctx, 100, 1, "first"));
	CHECK(pump(&ctx));
	CHECK(delivered_is(&ctx, "first"));
	CHECK(drain(&ctx, 20));
	const V3Packet* ack = last_ack(&ctx);
	CHECK(ack && (ack->flags & V3_ACK) && (ack->ack == 100));
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* A resend under a new data sequence number repeats a channel sequence already handed up.
 * Delivering it again duplicates bytes in the stream. */
static BOOL retransmitted_channel_data_is_not_delivered_twice(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	CHECK(send_data(&ctx, 100, 1, "a"));
	CHECK(send_data(&ctx, 101, 2, "b"));
	CHECK(send_data(&ctx, 102, 2, "b"));
	CHECK(send_data(&ctx, 103, 3, "c"));
	CHECK(pump(&ctx));
	CHECK(delivered_is(&ctx, "abc"));
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

static BOOL gap_produces_an_ack_vector_rather_than_a_cumulative_ack(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	CHECK(send_data(&ctx, 100, 1, "a"));
	CHECK(send_data(&ctx, 102, 3, "c"));
	CHECK(pump(&ctx));
	CHECK(drain(&ctx, 20));
	const V3Packet* ack = last_ack(&ctx);
	CHECK(ack);
	CHECK((ack->flags & V3_ACKVEC) && !(ack->flags & V3_ACK));
	CHECK(ack->vecBase == 101);
	/* bit 0 is the base (101, missing), bit 1 is 102 (received) */
	CHECK((ack->vecLength == 1) && (ack->vec[0] == 0x02));
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

static BOOL ack_advertises_the_largest_window(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	CHECK(send_data(&ctx, 100, 1, "a"));
	CHECK(pump(&ctx));
	CHECK(drain(&ctx, 20));
	const V3Packet* ack = last_ack(&ctx);
	CHECK(ack && (ack->flags & V3_ACK) && (ack->ack == 100));
	/* Windows always advertises 2^15 * MTU, a smaller window throttles its sender */
	CHECK(ack->logWindow == 15);
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* Windows wraps the channel sequence 65535 -> 1, skipping 0. Waiting for a 0 the peer never sends
 * stalls the stream for good while the transport stays healthy. */
static BOOL channel_sequence_wraps_past_zero(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	UINT16 dataSeq = FIRST_SEQUENCE;
	UINT16 channel = 1;
	CHECK(stream_ahead(&ctx, &dataSeq, &channel, 65534));
	CHECK(channel == 65535);
	const size_t before = Stream_GetPosition(ctx.delivered);
	CHECK(before == 65534);

	CHECK(send_data(&ctx, dataSeq, 65535, "f"));
	CHECK(send_data(&ctx, (UINT16)(dataSeq + 1), 1, "a"));
	CHECK(pump(&ctx));
	CHECK(delivered_tail_is(&ctx, before, "fa"));

	CHECK(send_data(&ctx, (UINT16)(dataSeq + 2), 2, "b"));
	CHECK(pump(&ctx));
	CHECK(delivered_tail_is(&ctx, before, "fab"));
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* A packet captured from Windows: an ACK with one delayed acknowledgement ahead of the data.
 * Reading numDelayedAcks from the wrong nibble shifts DataHeader and loses the payload. */
static BOOL real_server_ack_with_delayed_acks_and_data(void)
{
	static const BYTE raw[] = { 0x00, 0x45, 0xf0, 0x66, 0x00, 0x03, 0x22, 0xe0,
		                        0x00, 0x01, 0x54, 0x08, 0xe0, 0x00, 0x02, 0x00,
		                        0x17, 0x03, 0x03, 0x00, 0x62, 0x56, 0x2c, 0x9f };
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	/* the captured packet is data sequence 224 carrying channel sequence 2 */
	CHECK(send_data(&ctx, 223, 1, "a"));
	CHECK(server_send_raw(&ctx, raw, sizeof(raw)));
	CHECK(pump(&ctx));
	CHECK(Stream_GetPosition(ctx.delivered) > 6);
	CHECK(memcmp(Stream_Buffer(ctx.delivered), "a\x17\x03\x03\x00\x62", 6) == 0);
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* ---------------------------------------------------------------------------------------- */
/* Sending                                                                                   */
/* ---------------------------------------------------------------------------------------- */

/* AckOfAcks is the lowest of our data sequence numbers still awaiting acknowledgement. It goes
 * out when it changes, sending the sequence being transmitted would make the peer abandon every
 * packet still in flight. */
static BOOL ack_of_acks_reports_the_oldest_unacknowledged_packet(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));
	CHECK(rdpudp_send(ctx.udp, (const BYTE*)"1", 1));
	CHECK(drain(&ctx, 20));
	const V3Packet* sent = first_data(&ctx);
	CHECK(sent && sent->hasAoA && (sent->aoa == FIRST_SEQUENCE));

	CHECK(rdpudp_send(ctx.udp, (const BYTE*)"2", 1));
	CHECK(drain(&ctx, 20));
	sent = first_data(&ctx);
	CHECK(sent && !sent->hasAoA);

	CHECK(send_ack(&ctx, FIRST_SEQUENCE));
	CHECK(pump(&ctx));
	CHECK(rdpudp_send(ctx.udp, (const BYTE*)"3", 1));
	CHECK(drain(&ctx, 20));
	sent = first_data(&ctx);
	CHECK(sent && sent->hasAoA && (sent->aoa == FIRST_SEQUENCE + 1));
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* Once the 16 bit data sequence wraps, a cumulative ACK has to be compared with serial
 * arithmetic: a plain comparison keeps every outstanding packet and resends them all. */
static BOOL acknowledgements_survive_sequence_wraparound(void)
{
	BOOL rc = FALSE;
	TestCtx ctx = { 0 };
	CHECK(ctx_connect(&ctx));

	/* send and acknowledge until the next data sequence is 0xFFFE */
	UINT16 next = FIRST_SEQUENCE;
	while (next != 0xFFFE)
	{
		const size_t batch = ((size_t)(0xFFFE - next) > 64) ? 64 : (size_t)(0xFFFE - next);
		for (size_t x = 0; x < batch; x++)
			CHECK(rdpudp_send(ctx.udp, (const BYTE*)".", 1));
		CHECK(collect_data(&ctx, batch, 1000));
		next = (UINT16)(next + batch);
		CHECK(send_ack(&ctx, (UINT16)(next - 1)));
		CHECK(pump(&ctx));
	}

	/* 0xFFFE, 0xFFFF, 0x0000 and 0x0001 go out, the peer acknowledges up to 0x0000 */
	for (size_t x = 0; x < 4; x++)
		CHECK(rdpudp_send(ctx.udp, (const BYTE*)"w", 1));
	CHECK(collect_data(&ctx, 4, 1000));
	CHECK(send_ack(&ctx, 0x0000));
	CHECK(pump(&ctx));

	/* only 0x0001 is left to resend */
	Sleep(400);
	CHECK(pump(&ctx));
	CHECK(drain(&ctx, 20));
	CHECK(count_data(&ctx) == 1);
	const V3Packet* resent = first_data(&ctx);
	CHECK(resent && (resent->channelSeq == 0xFFFE - FIRST_SEQUENCE + 4));
	rc = TRUE;
fail:
	ctx_free(&ctx);
	return rc;
}

/* ---------------------------------------------------------------------------------------- */

typedef struct
{
	const char* name;
	BOOL (*fn)(void);
} TestCase;

static const TestCase cases[] = {
	{ "syn_offers_version_3_with_the_cookie_hash", syn_offers_version_3_with_the_cookie_hash },
	{ "syn_offers_version_2_without_a_cookie_hash", syn_offers_version_2_without_a_cookie_hash },
	{ "dummy_packets_are_acknowledged_but_not_delivered",
	  dummy_packets_are_acknowledged_but_not_delivered },
	{ "retransmission_is_reordered_back_into_the_stream",
	  retransmission_is_reordered_back_into_the_stream },
	{ "ack_of_acks_advances_the_window_without_touching_the_stream",
	  ack_of_acks_advances_the_window_without_touching_the_stream },
	{ "retransmitted_packets_are_acknowledged_again", retransmitted_packets_are_acknowledged_again },
	{ "retransmitted_channel_data_is_not_delivered_twice",
	  retransmitted_channel_data_is_not_delivered_twice },
	{ "gap_produces_an_ack_vector_rather_than_a_cumulative_ack",
	  gap_produces_an_ack_vector_rather_than_a_cumulative_ack },
	{ "ack_advertises_the_largest_window", ack_advertises_the_largest_window },
	{ "channel_sequence_wraps_past_zero", channel_sequence_wraps_past_zero },
	{ "real_server_ack_with_delayed_acks_and_data", real_server_ack_with_delayed_acks_and_data },
	{ "ack_of_acks_reports_the_oldest_unacknowledged_packet",
	  ack_of_acks_reports_the_oldest_unacknowledged_packet },
	{ "acknowledgements_survive_sequence_wraparound", acknowledgements_survive_sequence_wraparound },
};

int TestRdpUdp(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	WSADATA wsaData = { 0 };
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		return -1;

	int rc = 0;
	for (size_t x = 0; x < ARRAYSIZE(cases); x++)
	{
		const BOOL ok = cases[x].fn();
		(void)printf("%s %s\n", ok ? "ok    " : "FAILED", cases[x].name);
		if (!ok)
			rc = -1;
	}

	WSACleanup();
	return rc;
}
