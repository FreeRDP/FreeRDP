/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP-UDP reliable transport ([MS-RDPEUDP] and [MS-RDPEUDP2])
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

#include <freerdp/config.h>

#include <errno.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "rdpudp.h"

/* winpr's SOCKET is wider than the int a POSIX socket call takes */
#if defined(_WIN32)
#define RDPUDP_FD(s) (s)
#else
#define RDPUDP_FD(s) ((int)(s))
#endif

/* [MS-RDPEUDP] 2.2.2.1 RDPUDP_FEC_HEADER uFlags */
#define RDPUDP_FLAG_SYN 0x0001
#define RDPUDP_FLAG_ACK 0x0004
#define RDPUDP_FLAG_DATA 0x0008
#define RDPUDP_FLAG_FEC 0x0010
#define RDPUDP_FLAG_CN 0x0020
#define RDPUDP_FLAG_CWR 0x0040
#define RDPUDP_FLAG_ACK_OF_ACKS 0x0100
#define RDPUDP_FLAG_CORRELATION_ID 0x0800
#define RDPUDP_FLAG_SYNEX 0x1000

/* [MS-RDPEUDP] 2.2.2.9 RDPUDP_SYNDATAEX_PAYLOAD uSynExFlags */
#define RDPUDP_VERSION_INFO_VALID 0x0001

/* [MS-RDPEUDP] 2.2.1.1 VECTOR_ELEMENT_STATE */
#define DATAGRAM_RECEIVED 0
#define DATAGRAM_NOT_YET_RECEIVED 3

/* [MS-RDPEUDP2] 2.2.1.1 RDPUDP2_PACKET_HEADER Flags */
#define RDPUDP2_ACK 0x001
#define RDPUDP2_DATA 0x004
#define RDPUDP2_ACKVEC 0x008
#define RDPUDP2_AOA 0x010
#define RDPUDP2_OVERHEADSIZE 0x040
#define RDPUDP2_DELAYACKINFO 0x100
#define RDPUDP2_NOACK 0x200

/* [MS-RDPEUDP2] 2.2.1 packet type index of the prefix byte */
#define RDPUDP2_PACKET_STANDARD 0
#define RDPUDP2_PACKET_DUMMY 8

#define RDPUDP_MTU 1232
#define RDPUDP_MIN_MTU 1132

/* What this side advertises in uReceiveWindowSize (versions 1 and 2). */
#define RDPUDP_RECEIVE_WINDOW 64

/* Out of order data held while waiting for a gap to fill. Must stay below the 4096 slots. */
#define RDPUDP_RECEIVE_SLOTS 4096
#define RDPUDP_RECEIVE_REACH 4000

/* Datagrams of ours that may be unacknowledged at once. */
#define RDPUDP_MAX_IN_FLIGHT 64

/* [MS-RDPEUDP2] LogWindowSize. Windows always advertises the largest window (2^15 * MTU), a
 * smaller value throttles the server's sender for no benefit. */
#define RDPUDP2_LOG_WINDOW_SIZE 15

/* Both Windows endpoints start the RDP-UDP2 data sequence space at 100. */
#define RDPUDP2_INITIAL_SEQUENCE 100

/* RDPUDP2 ACK vectors hold at most 127 state map bytes of 7 packets each. */
#define RDPUDP2_MAX_ACKVEC 127

/* Room left in every version 1/2 data datagram for the ACK vector and AckOfAcks. */
#define RDPUDP_ACK_ROOM 64

#define RDPUDP_SYN_TIMEOUT_MS 1000
#define RDPUDP_SYN_ATTEMPTS 4
#define RDPUDP_MAX_RETRANSMITS 10
#define RDPUDP_MAX_RTO_MS 5000
#define RDPUDP_KEEPALIVE_MS 5000
#define RDPUDP_PEER_TIMEOUT_MS 65000

/* A stream chunk that stays missing while later ones pile up behind it for this long will not
 * come back: the peer only resends a lost chunk while it still has it outstanding. */
#define RDPUDP_STALL_TIMEOUT_MS 10000

typedef struct
{
	BOOL used;
	UINT32 seq; /* source sequence (v1/v2) or data sequence (v3) */
	UINT16 channelSeq;
	BYTE* payload;
	size_t length;
	UINT64 sent;
	UINT64 due;
	UINT32 rto;
	UINT32 retransmits;
} rdpudp_pending;

typedef struct
{
	BOOL present;
	UINT32 seq;
	BYTE* data;
	size_t length;
} rdpudp_slot;

typedef struct
{
	BYTE* data;
	size_t length;
} rdpudp_chunk;

struct rdp_udp
{
	wLog* log;
	SOCKET sockfd;
	RDPUDP_STATE state;
	UINT16 version;
	BOOL offerV3;
	BYTE cookieHash[32];

	rdpUdpReceiveFn receive;
	void* custom;

	UINT16 mtu;
	UINT32 synAttempts;
	UINT64 synSent;
	UINT64 lastSent;
	UINT64 lastReceived;
	UINT32 srtt;

	/* sending side */
	rdpudp_pending pending[RDPUDP_MAX_IN_FLIGHT];
	size_t inFlight;
	UINT32 peerWindow;
	wQueue* sendQueue;
	UINT32 initialSequence;
	UINT32 nextSource;
	UINT32 nextCoded;
	BOOL cwrPending;
	UINT32 announcedAoA;
	BOOL aoaAnnounced;
	UINT16 nextChannelSeq;

	/* receiving side */
	BOOL ackPending;
	BOOL receivedData;
	UINT64 lastDataArrival;
	rdpudp_slot slots[RDPUDP_RECEIVE_SLOTS];
	size_t buffered;
	UINT64 lastDelivery;
	UINT64 bufferingSince;

	/* receiving, versions 1 and 2 */
	UINT32 peerInitialSequence;
	UINT32 recvNext;
	UINT32 recvHighest;
	UINT32 ackVectorStart;

	/* receiving, version 3 */
	BOOL v3BaseKnown;
	UINT16 v3Expected;
	UINT16 v3Highest;
	BOOL v3Received[RDPUDP_RECEIVE_SLOTS];
	BOOL v3ChannelKnown;
	UINT16 v3NextChannel;
};

static BOOL seq16_after(UINT16 a, UINT16 b)
{
	return (a != b) && ((UINT16)(a - b) < 0x8000);
}

static BOOL seq32_after(UINT32 a, UINT32 b)
{
	return (a != b) && ((a - b) < 0x80000000u);
}

/* Channel sequence numbers are 1-based: the peer wraps 65535 to 1 and never uses 0. */
static UINT16 channel_seq_next(UINT16 seq)
{
	return (seq == UINT16_MAX) ? 1 : (UINT16)(seq + 1);
}

static UINT32 channel_seq_distance(UINT16 to, UINT16 from)
{
	if (to >= from)
		return (UINT32)to - from;
	return (UINT32)(UINT16_MAX - from) + to;
}

static void write_u16_be(BYTE* p, UINT16 v)
{
	p[0] = (BYTE)(v >> 8);
	p[1] = (BYTE)v;
}

static void write_u32_be(BYTE* p, UINT32 v)
{
	p[0] = (BYTE)(v >> 24);
	p[1] = (BYTE)(v >> 16);
	p[2] = (BYTE)(v >> 8);
	p[3] = (BYTE)v;
}

static UINT16 read_u16_be(const BYTE* p)
{
	return (UINT16)((p[0] << 8) | p[1]);
}

static UINT32 read_u32_be(const BYTE* p)
{
	return ((UINT32)p[0] << 24) | ((UINT32)p[1] << 16) | ((UINT32)p[2] << 8) | p[3];
}

static void write_u16_le(BYTE* p, UINT16 v)
{
	p[0] = (BYTE)v;
	p[1] = (BYTE)(v >> 8);
}

static UINT16 read_u16_le(const BYTE* p)
{
	return (UINT16)(p[0] | (p[1] << 8));
}

static UINT64 now_ms(void)
{
	return GetTickCount64();
}

static UINT32 rto_floor(const rdpUdp* udp)
{
	return (udp->version == RDPUDP_PROTOCOL_VERSION_1) ? 500 : 300;
}

static UINT32 rto_initial(const rdpUdp* udp)
{
	const UINT32 floor = rto_floor(udp);
	const UINT32 rtt2 = udp->srtt * 2;
	return (rtt2 > floor) ? rtt2 : floor;
}

static void chunk_free(void* obj)
{
	rdpudp_chunk* chunk = obj;
	if (!chunk)
		return;
	free(chunk->data);
	free(chunk);
}

static void slot_clear(rdpudp_slot* slot)
{
	free(slot->data);
	slot->data = nullptr;
	slot->length = 0;
	slot->present = FALSE;
}

static void pending_clear(rdpudp_pending* p)
{
	free(p->payload);
	memset(p, 0, sizeof(*p));
}

static void rdpudp_fail(rdpUdp* udp, const char* why)
{
	if (udp->state != RDPUDP_STATE_FAILED)
		WLog_Print(udp->log, WLOG_WARN, "RDP-UDP connection failed: %s", why);
	udp->state = RDPUDP_STATE_FAILED;
}

static BOOL rdpudp_send_raw(rdpUdp* udp, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	const SSIZE_T rc = send(RDPUDP_FD(udp->sockfd), (const char*)data, (int)length, 0);
	if (rc < 0)
	{
#if defined(_WIN32)
		const int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			return TRUE; /* dropped, the retransmit timer recovers it */
#else
		const int err = errno;
		if ((err == EAGAIN) || (err == EWOULDBLOCK) || (err == ECONNREFUSED))
			return TRUE;
#endif
		WLog_Print(udp->log, WLOG_DEBUG, "send failed with %d", err);
		return TRUE;
	}
	udp->lastSent = now_ms();
	return TRUE;
}

/* ------------------------------------------------------------------------------------------ */
/* [MS-RDPEUDP2] encoding                                                                      */
/* ------------------------------------------------------------------------------------------ */

static UINT32 v3_timestamp(void)
{
	/* [MS-RDPEUDP2] 3.1.1.1.4: 24 bits in units of 4 microseconds */
	return (UINT32)((winpr_GetTickCount64NS() / 4000ULL) & 0x00FFFFFFULL);
}

static BYTE v3_ack_gap(const rdpUdp* udp)
{
	if (!udp->receivedData)
		return 0;
	const UINT64 gap = now_ms() - udp->lastDataArrival;
	return (gap > 254) ? 254 : (BYTE)gap; /* 255 means invalid */
}

static BOOL v3_has_gap(const rdpUdp* udp)
{
	return udp->v3BaseKnown && seq16_after(udp->v3Highest, (UINT16)(udp->v3Expected - 1));
}

static size_t v3_write_ack(const rdpUdp* udp, BYTE* p)
{
	write_u16_le(p, (UINT16)(udp->v3Expected - 1));
	const UINT32 ts = v3_timestamp();
	p[2] = (BYTE)ts;
	p[3] = (BYTE)(ts >> 8);
	p[4] = (BYTE)(ts >> 16);
	p[5] = v3_ack_gap(udp);
	p[6] = 0; /* numDelayedAcks / delayAckTimeScale */
	return 7;
}

/* Turns a packet layout (header and payloads) into its on-wire form: the prefix byte is put in
 * front, short packets are padded and the first and eighth byte are swapped
 * ([MS-RDPEUDP2] 2.2.1). */
static BOOL v3_send_layout(rdpUdp* udp, BYTE packetType, const BYTE* layout, size_t length)
{
	BYTE wire[RDPUDP_MTU + 16] = WINPR_C_ARRAY_INIT;
	if (length + 1 > sizeof(wire))
		return FALSE;

	BYTE shortLength = 7;
	size_t padded = length;
	if (length < 7)
	{
		shortLength = (BYTE)length;
		padded = 7;
	}

	wire[0] = (BYTE)((shortLength << 5) | ((packetType & 0x0F) << 1));
	memcpy(&wire[1], layout, length);
	size_t total = padded + 1;
	if (total < 8)
		total = 8;

	const BYTE tmp = wire[0];
	wire[0] = wire[7];
	wire[7] = tmp;
	return rdpudp_send_raw(udp, wire, total);
}

static BOOL v3_send_ack(rdpUdp* udp)
{
	BYTE layout[16 + RDPUDP2_MAX_ACKVEC] = WINPR_C_ARRAY_INIT;
	size_t pos = 2;
	UINT16 flags = RDPUDP2_OVERHEADSIZE;

	if (!udp->v3BaseKnown)
		return TRUE;

	udp->ackPending = FALSE;

	if (v3_has_gap(udp))
	{
		/* A cumulative ACK cannot describe a hole, the ACK vector reports the exact window so
		 * only what is missing gets resent. ACK and ACKVEC are mutually exclusive. */
		flags |= RDPUDP2_ACKVEC;
		layout[pos++] = 10; /* OverheadSize */

		const UINT16 base = udp->v3Expected;
		const size_t span = (size_t)(UINT16)(udp->v3Highest - base) + 1;
		size_t count = (span + 6) / 7;
		if (count > RDPUDP2_MAX_ACKVEC)
			count = RDPUDP2_MAX_ACKVEC;

		write_u16_le(&layout[pos], base);
		pos += 2;
		layout[pos++] = (BYTE)(0x80 | count); /* TimeStampPresent | codedAckVecSize */
		const UINT32 ts = v3_timestamp();
		layout[pos++] = (BYTE)ts;
		layout[pos++] = (BYTE)(ts >> 8);
		layout[pos++] = (BYTE)(ts >> 16);
		layout[pos++] = v3_ack_gap(udp);
		for (size_t x = 0; x < count; x++)
		{
			BYTE bits = 0;
			for (size_t bit = 0; bit < 7; bit++)
			{
				const UINT16 seq = (UINT16)(base + x * 7 + bit);
				if (udp->v3Received[seq % RDPUDP_RECEIVE_SLOTS] &&
				    (UINT16)(seq - base) < RDPUDP_RECEIVE_REACH)
					bits |= (BYTE)(1 << bit);
			}
			layout[pos++] = bits;
		}
	}
	else
	{
		flags |= RDPUDP2_ACK;
		pos += v3_write_ack(udp, &layout[pos]);
		layout[pos++] = 10; /* OverheadSize */
	}

	write_u16_le(layout, (UINT16)(flags | (RDPUDP2_LOG_WINDOW_SIZE << 12)));
	return v3_send_layout(udp, RDPUDP2_PACKET_STANDARD, layout, pos);
}

/* The lowest data sequence number of ours still awaiting acknowledgement, which is what
 * AckOfAcks tells the peer ([MS-RDPEUDP2] 2.2.1.2.4). */
static UINT32 sender_base(const rdpUdp* udp)
{
	UINT32 base = (udp->version == RDPUDP_PROTOCOL_VERSION_3) ? udp->nextCoded : udp->nextSource;
	BOOL found = FALSE;
	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		const rdpudp_pending* p = &udp->pending[x];
		if (!p->used)
			continue;
		if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		{
			if (!found || seq16_after((UINT16)base, (UINT16)p->seq))
				base = p->seq;
		}
		else if (!found || seq32_after(base, p->seq))
		{
			base = p->seq;
		}
		found = TRUE;
	}
	return base;
}

static BOOL v3_send_data(rdpUdp* udp, const rdpudp_pending* p)
{
	BYTE layout[RDPUDP_MTU + 32] = WINPR_C_ARRAY_INIT;
	size_t pos = 2;
	UINT16 flags = RDPUDP2_DATA | RDPUDP2_DELAYACKINFO;

	if (udp->ackPending && udp->v3BaseKnown && !v3_has_gap(udp))
	{
		flags |= RDPUDP2_ACK;
		pos += v3_write_ack(udp, &layout[pos]);
		udp->ackPending = FALSE;
	}
	else
		flags |= RDPUDP2_NOACK; /* Windows sets this on every DATA packet without an ACK */

	layout[pos++] = 1;              /* MaxDelayedAcks */
	write_u16_le(&layout[pos], 20); /* DelayedAckTimeoutInMs */
	pos += 2;

	const UINT16 base = (UINT16)sender_base(udp);
	/* repeated on a resend, the first announcement may have been lost with it */
	if (!udp->aoaAnnounced || (udp->announcedAoA != base) || (p->retransmits > 0))
	{
		flags |= RDPUDP2_AOA;
		write_u16_le(&layout[pos], base);
		pos += 2;
		udp->announcedAoA = base;
		udp->aoaAnnounced = TRUE;
	}

	write_u16_le(&layout[pos], (UINT16)p->seq);
	pos += 2;
	write_u16_le(&layout[pos], p->channelSeq);
	pos += 2;

	if (pos + p->length > sizeof(layout))
		return FALSE;
	memcpy(&layout[pos], p->payload, p->length);
	pos += p->length;

	write_u16_le(layout, (UINT16)(flags | (RDPUDP2_LOG_WINDOW_SIZE << 12)));
	return v3_send_layout(udp, RDPUDP2_PACKET_STANDARD, layout, pos);
}

/* ------------------------------------------------------------------------------------------ */
/* [MS-RDPEUDP] version 1 and 2 encoding                                                      */
/* ------------------------------------------------------------------------------------------ */

/* Writes an RDPUDP_ACK_VECTOR_HEADER covering the peer's packets up to the highest one seen.
 * @return bytes written */
static size_t v1_write_ack_vector(const rdpUdp* udp, BYTE* p, size_t room)
{
	const UINT32 highest = udp->recvHighest;
	UINT32 low = udp->ackVectorStart;
	if (!udp->receivedData || seq32_after(low, highest))
		low = highest;
	if ((highest - low) >= RDPUDP_RECEIVE_REACH)
		low = highest - (RDPUDP_RECEIVE_REACH - 1);

	BYTE elements[2048] = WINPR_C_ARRAY_INIT;
	size_t count = 0;
	BYTE state = 0xFF;
	BYTE run = 0;

	UINT32 seq = low;
	while (TRUE)
	{
		BYTE current = DATAGRAM_RECEIVED;
		if (udp->receivedData && !seq32_after(udp->recvNext, seq))
		{
			const rdpudp_slot* slot = &udp->slots[seq % RDPUDP_RECEIVE_SLOTS];
			if (!slot->present || (slot->seq != seq))
				current = DATAGRAM_NOT_YET_RECEIVED;
		}

		if ((current != state) || (run == 0x3F))
		{
			if (run > 0)
			{
				if (count >= ARRAYSIZE(elements))
					break;
				elements[count++] = (BYTE)((state << 6) | run);
			}
			state = current;
			run = 0;
		}
		run++;

		if (seq == highest)
			break;
		seq++;
	}
	if ((run > 0) && (count < ARRAYSIZE(elements)))
		elements[count++] = (BYTE)((state << 6) | run);

	/* Too long to fit: keep the newest part, the older part has been reported before. */
	size_t skip = 0;
	while ((2 + (count - skip) + 3) > room && (skip < count))
		skip++;
	count -= skip;

	write_u16_be(p, (UINT16)count);
	memcpy(&p[2], &elements[skip], count);
	size_t pos = 2 + count;
	while (pos % 4)
		p[pos++] = 0;
	return pos;
}

static UINT32 v1_cumulative_ack(const rdpUdp* udp)
{
	return sender_base(udp) - 1;
}

static size_t v1_write_header(rdpUdp* udp, BYTE* p, UINT16 flags)
{
	const UINT32 ack = udp->receivedData ? udp->recvHighest : udp->peerInitialSequence;
	write_u32_be(p, ack);
	write_u16_be(&p[4], RDPUDP_RECEIVE_WINDOW);
	write_u16_be(&p[6], flags);
	return 8;
}

static BOOL v1_send_packet(rdpUdp* udp, const rdpudp_pending* p)
{
	BYTE buffer[RDPUDP_MTU + 2048] = WINPR_C_ARRAY_INIT;
	UINT16 flags = RDPUDP_FLAG_ACK;
	size_t pos = 8;

	/* an ACK-only datagram must stay within the MTU too: header, vector, AckOfAcks */
	pos += v1_write_ack_vector(udp, &buffer[pos], p ? RDPUDP_ACK_ROOM - 4 : udp->mtu - 12u);
	udp->ackPending = FALSE;

	const UINT32 cumulative = v1_cumulative_ack(udp);
	if (!udp->aoaAnnounced || (udp->announcedAoA != cumulative))
	{
		flags |= RDPUDP_FLAG_ACK_OF_ACKS;
		write_u32_be(&buffer[pos], cumulative);
		pos += 4;
		udp->announcedAoA = cumulative;
		udp->aoaAnnounced = TRUE;
	}

	if (p)
	{
		flags |= RDPUDP_FLAG_DATA;
		if (udp->cwrPending)
		{
			flags |= RDPUDP_FLAG_CWR;
			udp->cwrPending = FALSE;
		}
		/* A retransmitted source packet gets a new coded sequence number. */
		write_u32_be(&buffer[pos], udp->nextCoded++);
		write_u32_be(&buffer[pos + 4], p->seq);
		pos += 8;
		if (pos + p->length > sizeof(buffer))
			return FALSE;
		memcpy(&buffer[pos], p->payload, p->length);
		pos += p->length;
	}

	(void)v1_write_header(udp, buffer, flags);
	return rdpudp_send_raw(udp, buffer, pos);
}

static BOOL rdpudp_send_ack(rdpUdp* udp)
{
	if (udp->state != RDPUDP_STATE_ESTABLISHED)
		return TRUE;
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		return v3_send_ack(udp);
	return v1_send_packet(udp, nullptr);
}

static BOOL rdpudp_transmit(rdpUdp* udp, const rdpudp_pending* p)
{
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		return v3_send_data(udp, p);
	return v1_send_packet(udp, p);
}

/* ------------------------------------------------------------------------------------------ */
/* Sending                                                                                    */
/* ------------------------------------------------------------------------------------------ */

static size_t window_limit(const rdpUdp* udp)
{
	size_t limit = RDPUDP_MAX_IN_FLIGHT;
	if ((udp->version != RDPUDP_PROTOCOL_VERSION_3) && (udp->peerWindow > 0) &&
	    (udp->peerWindow < limit))
		limit = udp->peerWindow;
	return limit;
}

static BOOL send_chunk_now(rdpUdp* udp, BYTE* data, size_t length)
{
	rdpudp_pending* p = nullptr;
	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		if (!udp->pending[x].used)
		{
			p = &udp->pending[x];
			break;
		}
	}
	if (!p)
		return FALSE;

	p->used = TRUE;
	p->payload = data;
	p->length = length;
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
	{
		p->seq = (UINT16)udp->nextCoded;
		udp->nextCoded = (UINT16)(udp->nextCoded + 1);
		p->channelSeq = udp->nextChannelSeq;
		udp->nextChannelSeq = channel_seq_next(udp->nextChannelSeq);
	}
	else
	{
		p->seq = udp->nextSource++;
	}

	const UINT64 now = now_ms();
	p->sent = now;
	p->rto = rto_initial(udp);
	p->due = now + p->rto;
	p->retransmits = 0;
	udp->inFlight++;

	return rdpudp_transmit(udp, p);
}

static BOOL flush_send_queue(rdpUdp* udp)
{
	while ((udp->state == RDPUDP_STATE_ESTABLISHED) && (udp->inFlight < window_limit(udp)) &&
	       (Queue_Count(udp->sendQueue) > 0))
	{
		rdpudp_chunk* chunk = Queue_Dequeue(udp->sendQueue);
		if (!chunk)
			break;
		BYTE* data = chunk->data;
		const size_t length = chunk->length;
		free(chunk);
		if (!send_chunk_now(udp, data, length))
		{
			free(data);
			return FALSE;
		}
	}
	return TRUE;
}

BOOL rdpudp_send(rdpUdp* udp, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);

	if ((length == 0) || (length > rdpudp_get_max_payload(udp)))
		return FALSE;
	if ((udp->state != RDPUDP_STATE_ESTABLISHED) && (udp->state != RDPUDP_STATE_SYN_SENT))
		return FALSE;

	rdpudp_chunk* chunk = calloc(1, sizeof(rdpudp_chunk));
	if (!chunk)
		return FALSE;
	chunk->data = malloc(length);
	if (!chunk->data)
	{
		free(chunk);
		return FALSE;
	}
	memcpy(chunk->data, data, length);
	chunk->length = length;

	if (!Queue_Enqueue(udp->sendQueue, chunk))
	{
		chunk_free(chunk);
		return FALSE;
	}
	return flush_send_queue(udp);
}

static void sample_rtt(rdpUdp* udp, const rdpudp_pending* p)
{
	/* Karn: a retransmitted packet says nothing reliable about the round trip */
	if (p->retransmits > 0)
		return;
	const UINT64 sample64 = now_ms() - p->sent;
	const UINT32 sample = (sample64 > UINT32_MAX) ? UINT32_MAX : (UINT32)sample64;
	udp->srtt = (udp->srtt == 0) ? sample : (udp->srtt * 7 + sample) / 8;
}

static void acknowledge(rdpUdp* udp, rdpudp_pending* p)
{
	sample_rtt(udp, p);
	pending_clear(p);
	if (udp->inFlight > 0)
		udp->inFlight--;
}

static void acknowledge_seq(rdpUdp* udp, UINT32 seq)
{
	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		rdpudp_pending* p = &udp->pending[x];
		if (p->used && (p->seq == seq))
			acknowledge(udp, p);
	}
}

/* ------------------------------------------------------------------------------------------ */
/* Receiving                                                                                  */
/* ------------------------------------------------------------------------------------------ */

static BOOL deliver(rdpUdp* udp, const BYTE* data, size_t length)
{
	udp->lastDelivery = now_ms();
	if (length == 0)
		return TRUE;
	WINPR_ASSERT(udp->receive);
	if (!udp->receive(udp->custom, data, length))
	{
		rdpudp_fail(udp, "receiver rejected data");
		return FALSE;
	}
	return TRUE;
}

static BOOL store_slot(rdpUdp* udp, UINT32 seq, const BYTE* data, size_t length)
{
	rdpudp_slot* slot = &udp->slots[seq % RDPUDP_RECEIVE_SLOTS];
	if (slot->present)
	{
		if (slot->seq == seq)
			return TRUE; /* duplicate */
		slot_clear(slot);
		udp->buffered--;
	}

	slot->data = nullptr;
	if (length > 0)
	{
		slot->data = malloc(length);
		if (!slot->data)
			return FALSE;
		memcpy(slot->data, data, length);
	}
	slot->length = length;
	slot->seq = seq;
	slot->present = TRUE;
	if (udp->buffered++ == 0)
		udp->bufferingSince = now_ms();
	return TRUE;
}

static BOOL v1_receive_source(rdpUdp* udp, UINT32 seq, const BYTE* data, size_t length)
{
	if (!udp->receivedData)
	{
		/* [MS-RDPEUDP] puts the first source packet at the initial sequence number plus one,
		 * an implementation that uses the initial sequence number itself is accepted too. */
		if (seq == udp->peerInitialSequence)
		{
			udp->recvNext = seq;
			udp->ackVectorStart = seq;
		}
		udp->recvHighest = udp->recvNext - 1;
	}

	udp->ackPending = TRUE;
	udp->lastDataArrival = now_ms();

	if (seq32_after(udp->recvNext, seq))
		return TRUE; /* already delivered, the ACK above tells the peer again */

	const UINT32 ahead = seq - udp->recvNext;
	if (ahead >= RDPUDP_RECEIVE_REACH)
		return TRUE;

	udp->receivedData = TRUE;
	if (seq32_after(seq, udp->recvHighest))
		udp->recvHighest = seq;

	if (!store_slot(udp, seq, data, length))
		return FALSE;

	while (TRUE)
	{
		rdpudp_slot* slot = &udp->slots[udp->recvNext % RDPUDP_RECEIVE_SLOTS];
		if (!slot->present || (slot->seq != udp->recvNext))
			break;
		BYTE* chunk = slot->data;
		const size_t chunkLength = slot->length;
		slot->data = nullptr;
		slot->present = FALSE;
		udp->buffered--;
		udp->recvNext++;
		const BOOL rc = deliver(udp, chunk, chunkLength);
		free(chunk);
		if (!rc)
			return FALSE;
	}
	return TRUE;
}

static void v1_process_ack_vector(rdpUdp* udp, UINT32 snSourceAck, const BYTE* elements,
                                  size_t count)
{
	UINT32 total = 0;
	for (size_t x = 0; x < count; x++)
		total += elements[x] & 0x3F;
	if (total == 0)
		return;

	UINT32 seq = snSourceAck - total + 1;
	for (size_t x = 0; x < count; x++)
	{
		const BYTE state = (BYTE)(elements[x] >> 6);
		const BYTE run = elements[x] & 0x3F;
		for (BYTE y = 0; y < run; y++)
		{
			if (state == DATAGRAM_RECEIVED)
				acknowledge_seq(udp, seq);
			seq++;
		}
	}
}

static BOOL v1_process_datagram(rdpUdp* udp, const BYTE* data, size_t length)
{
	if (length < 8)
		return TRUE;

	const UINT32 snSourceAck = read_u32_be(data);
	const UINT16 window = read_u16_be(&data[4]);
	const UINT16 flags = read_u16_be(&data[6]);
	size_t pos = 8;

	if (flags & RDPUDP_FLAG_SYN)
	{
		/* a repeated SYN+ACK: our acknowledgement of it got lost */
		udp->ackPending = TRUE;
		return TRUE;
	}

	udp->peerWindow = window;
	if (flags & RDPUDP_FLAG_CN)
		udp->cwrPending = TRUE;

	if (flags & RDPUDP_FLAG_ACK)
	{
		if (length < pos + 2)
			return TRUE;
		const size_t size = read_u16_be(&data[pos]);
		size_t vectorLength = 2 + size;
		while (vectorLength % 4)
			vectorLength++;
		if (length < pos + vectorLength)
			return TRUE;
		v1_process_ack_vector(udp, snSourceAck, &data[pos + 2], size);
		pos += vectorLength;
	}

	if (flags & RDPUDP_FLAG_ACK_OF_ACKS)
	{
		if (length < pos + 4)
			return TRUE;
		const UINT32 reset = read_u32_be(&data[pos]);
		pos += 4;
		/* the peer has seen our acknowledgements up to here, the ACK vector can start later */
		const UINT32 start = reset + 1;
		if (seq32_after(start, udp->ackVectorStart) && !seq32_after(start, udp->recvNext))
			udp->ackVectorStart = start;
	}

	if (flags & RDPUDP_FLAG_CORRELATION_ID)
		pos += 32;

	if (!(flags & RDPUDP_FLAG_DATA) || (pos > length))
		return TRUE;

	if (flags & RDPUDP_FLAG_FEC)
		return TRUE; /* FEC packets are not acknowledged, losses are recovered by resending */

	if (length < pos + 8)
		return TRUE;
	const UINT32 snSourceStart = read_u32_be(&data[pos + 4]);
	pos += 8;
	return v1_receive_source(udp, snSourceStart, &data[pos], length - pos);
}

static void v3_advance_window(rdpUdp* udp)
{
	while (udp->v3Received[udp->v3Expected % RDPUDP_RECEIVE_SLOTS])
	{
		udp->v3Received[udp->v3Expected % RDPUDP_RECEIVE_SLOTS] = FALSE;
		udp->v3Expected++;
	}
}

static void v3_advance_base(rdpUdp* udp, UINT16 base)
{
	if (!udp->v3BaseKnown || !seq16_after(base, udp->v3Expected))
		return;
	if ((UINT16)(base - udp->v3Expected) > RDPUDP_RECEIVE_REACH)
		return;

	/* The peer stopped resending everything below base. This only gives up transport sequence
	 * numbers: a chunk of the stream sent there comes back under a later one, with its channel
	 * sequence number unchanged. */
	while (udp->v3Expected != base)
	{
		udp->v3Received[udp->v3Expected % RDPUDP_RECEIVE_SLOTS] = FALSE;
		udp->v3Expected++;
	}
	if (seq16_after((UINT16)(base - 1), udp->v3Highest))
		udp->v3Highest = (UINT16)(base - 1);
	v3_advance_window(udp);
	udp->ackPending = TRUE;
}

static void v3_process_ack_vector(rdpUdp* udp, UINT16 base, const BYTE* entries, size_t count)
{
	UINT16 seq = base;
	for (size_t x = 0; x < count; x++)
	{
		const BYTE entry = entries[x];
		if (entry & 0x80)
		{
			const BOOL received = (entry & 0x40) != 0;
			const BYTE run = entry & 0x3F;
			for (BYTE y = 0; y < run; y++)
			{
				if (received)
					acknowledge_seq(udp, seq);
				seq++;
			}
		}
		else
		{
			for (BYTE bit = 0; bit < 7; bit++)
			{
				if (entry & (1 << bit))
					acknowledge_seq(udp, seq);
				seq++;
			}
		}
	}

	/* everything below the base has been received */
	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		rdpudp_pending* p = &udp->pending[x];
		if (p->used && seq16_after(base, (UINT16)p->seq))
			acknowledge(udp, p);
	}
}

static void v3_process_ack(rdpUdp* udp, UINT16 ack)
{
	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		rdpudp_pending* p = &udp->pending[x];
		if (p->used && !seq16_after((UINT16)p->seq, ack))
			acknowledge(udp, p);
	}
}

static BOOL v3_receive_channel_data(rdpUdp* udp, UINT16 channelSeq, const BYTE* data, size_t length)
{
	if (!udp->v3ChannelKnown)
	{
		udp->v3ChannelKnown = TRUE;
		udp->v3NextChannel = channelSeq;
	}

	/* A retransmission travels under a new data sequence number with its original channel
	 * sequence, so the channel sequence alone says where a chunk belongs and whether it has
	 * been handed up already. */
	const UINT32 ahead = channel_seq_distance(channelSeq, udp->v3NextChannel);
	if (ahead >= RDPUDP_RECEIVE_REACH)
		return TRUE;

	if (!store_slot(udp, channelSeq, data, length))
		return FALSE;

	while (TRUE)
	{
		rdpudp_slot* slot = &udp->slots[udp->v3NextChannel % RDPUDP_RECEIVE_SLOTS];
		if (!slot->present || (slot->seq != udp->v3NextChannel))
			break;
		BYTE* chunk = slot->data;
		const size_t chunkLength = slot->length;
		slot->data = nullptr;
		slot->present = FALSE;
		udp->buffered--;
		udp->v3NextChannel = channel_seq_next(udp->v3NextChannel);
		const BOOL rc = deliver(udp, chunk, chunkLength);
		free(chunk);
		if (!rc)
			return FALSE;
	}
	return TRUE;
}

static BOOL v3_process_datagram(rdpUdp* udp, const BYTE* wire, size_t length)
{
	BYTE buffer[RDPUDP_MTU + 1024] = WINPR_C_ARRAY_INIT;
	if ((length < 8) || (length > sizeof(buffer)))
		return TRUE;

	memcpy(buffer, wire, length);
	const BYTE tmp = buffer[0];
	buffer[0] = buffer[7];
	buffer[7] = tmp;

	const BYTE prefix = buffer[0];
	if (prefix & 0x01)
		return TRUE;
	const BYTE shortLength = (BYTE)(prefix >> 5);
	const BYTE packetType = (BYTE)((prefix >> 1) & 0x0F);
	if ((packetType != RDPUDP2_PACKET_STANDARD) && (packetType != RDPUDP2_PACKET_DUMMY))
		return TRUE;

	const BYTE* p = &buffer[1];
	size_t avail = length - 1;
	if ((shortLength > 0) && (shortLength < 7))
		avail = shortLength;

#define NEED(n)          \
	do                   \
	{                    \
		if (avail < (n)) \
			return TRUE; \
	} while (0)

	NEED(2);
	const UINT16 flags = read_u16_le(p) & 0x0FFF;
	p += 2;
	avail -= 2;

	if ((flags & RDPUDP2_ACK) && (flags & RDPUDP2_ACKVEC))
		return TRUE;

	if (flags & RDPUDP2_ACK)
	{
		NEED(7);
		const UINT16 ack = read_u16_le(p);
		const size_t delayed = p[6] & 0x0F;
		p += 7;
		avail -= 7;
		NEED(delayed);
		p += delayed;
		avail -= delayed;
		v3_process_ack(udp, ack);
	}

	if (flags & RDPUDP2_OVERHEADSIZE)
	{
		NEED(1);
		p++;
		avail--;
	}

	if (flags & RDPUDP2_DELAYACKINFO)
	{
		NEED(3);
		p += 3;
		avail -= 3;
	}

	BOOL haveAoA = FALSE;
	UINT16 aoa = 0;
	if (flags & RDPUDP2_AOA)
	{
		NEED(2);
		aoa = read_u16_le(p);
		haveAoA = TRUE;
		p += 2;
		avail -= 2;
	}

	BOOL haveData = FALSE;
	UINT16 dataSeq = 0;
	if (flags & RDPUDP2_DATA)
	{
		NEED(2);
		dataSeq = read_u16_le(p);
		haveData = TRUE;
		p += 2;
		avail -= 2;
	}

	if (flags & RDPUDP2_ACKVEC)
	{
		NEED(3);
		const UINT16 base = read_u16_le(p);
		const BYTE control = p[2];
		const size_t size = control & 0x7F;
		p += 3;
		avail -= 3;
		if (control & 0x80)
		{
			NEED(4);
			p += 4;
			avail -= 4;
		}
		NEED(size);
		v3_process_ack_vector(udp, base, p, size);
		p += size;
		avail -= size;
	}
#undef NEED

	/* AckOfAcks moves the lower edge of our receive window. It is the only way past a lost
	 * dummy packet, those are never resent. */
	if (haveAoA)
		v3_advance_base(udp, aoa);

	if (!haveData)
		return TRUE;

	udp->lastDataArrival = now_ms();
	udp->receivedData = TRUE;
	if (!udp->v3BaseKnown)
	{
		/* Windows announces where its sequence space starts in the AckOfAcks of its first
		 * DATA packets. Taking the base from that, rather than from whichever packet arrives
		 * first, keeps a lost or late first packet from being acknowledged unseen. */
		UINT16 base = dataSeq;
		if (haveAoA && !seq16_after(aoa, dataSeq) &&
		    ((UINT16)(dataSeq - aoa) < RDPUDP_RECEIVE_REACH))
			base = aoa;
		udp->v3BaseKnown = TRUE;
		udp->v3Expected = base;
		udp->v3Highest = (UINT16)(base - 1);
	}

	udp->ackPending = TRUE;
	const UINT16 ahead = (UINT16)(dataSeq - udp->v3Expected);
	if (ahead >= 0x8000)
		return TRUE; /* resent because our ACK got lost, acknowledged again above */
	if (ahead >= RDPUDP_RECEIVE_REACH)
		return TRUE;

	if (seq16_after(dataSeq, udp->v3Highest))
		udp->v3Highest = dataSeq;
	udp->v3Received[dataSeq % RDPUDP_RECEIVE_SLOTS] = TRUE;
	v3_advance_window(udp);

	/* A dummy packet occupies a sequence number but its body means nothing. */
	if (packetType == RDPUDP2_PACKET_DUMMY)
		return TRUE;
	if (avail < 2)
		return TRUE;

	const UINT16 channelSeq = read_u16_le(p);
	return v3_receive_channel_data(udp, channelSeq, p + 2, avail - 2);
}

/* ------------------------------------------------------------------------------------------ */
/* Connection setup                                                                           */
/* ------------------------------------------------------------------------------------------ */

static BOOL send_syn(rdpUdp* udp)
{
	BYTE syn[RDPUDP_MTU] = WINPR_C_ARRAY_INIT;
	size_t pos = 0;
	const UINT16 version = udp->offerV3 ? RDPUDP_PROTOCOL_VERSION_3 : RDPUDP_PROTOCOL_VERSION_2;

	write_u32_be(&syn[pos], UINT32_MAX); /* snSourceAck: -1 */
	write_u16_be(&syn[pos + 4], RDPUDP_RECEIVE_WINDOW);
	write_u16_be(&syn[pos + 6], RDPUDP_FLAG_SYN | RDPUDP_FLAG_SYNEX);
	pos += 8;

	write_u32_be(&syn[pos], udp->initialSequence);
	write_u16_be(&syn[pos + 4], RDPUDP_MTU); /* uUpStreamMtu */
	write_u16_be(&syn[pos + 6], RDPUDP_MTU); /* uDownStreamMtu */
	pos += 8;

	write_u16_be(&syn[pos], RDPUDP_VERSION_INFO_VALID);
	write_u16_be(&syn[pos + 2], version);
	pos += 4;
	if (version == RDPUDP_PROTOCOL_VERSION_3)
		memcpy(&syn[pos], udp->cookieHash, sizeof(udp->cookieHash));

	/* [MS-RDPEUDP] 3.1.5.1.1: zero padded to the MTU, which validates the path MTU */
	udp->synSent = now_ms();
	udp->synAttempts++;
	WLog_Print(udp->log, WLOG_DEBUG, "sending SYN #%" PRIu32 " offering version 0x%04" PRIx16,
	           udp->synAttempts, version);
	return rdpudp_send_raw(udp, syn, sizeof(syn));
}

static BOOL process_syn_ack(rdpUdp* udp, const BYTE* data, size_t length)
{
	if (length < 16)
		return TRUE;

	const UINT32 snSourceAck = read_u32_be(data);
	const UINT16 window = read_u16_be(&data[4]);
	const UINT16 flags = read_u16_be(&data[6]);
	if ((flags & (RDPUDP_FLAG_SYN | RDPUDP_FLAG_ACK)) != (RDPUDP_FLAG_SYN | RDPUDP_FLAG_ACK))
		return TRUE;
	if (snSourceAck != udp->initialSequence)
		WLog_Print(udp->log, WLOG_DEBUG, "SYN+ACK acknowledges 0x%08" PRIx32 ", sent 0x%08" PRIx32,
		           snSourceAck, udp->initialSequence);

	size_t pos = 8;
	const UINT32 peerInitial = read_u32_be(&data[pos]);
	UINT16 upMtu = read_u16_be(&data[pos + 4]);
	pos += 8;

	if (flags & RDPUDP_FLAG_CORRELATION_ID)
		pos += 32;

	UINT16 version = RDPUDP_PROTOCOL_VERSION_1;
	if ((flags & RDPUDP_FLAG_SYNEX) && (length >= pos + 4))
	{
		const UINT16 synExFlags = read_u16_be(&data[pos]);
		if (synExFlags & RDPUDP_VERSION_INFO_VALID)
			version = read_u16_be(&data[pos + 2]);
	}

	switch (version)
	{
		case RDPUDP_PROTOCOL_VERSION_1:
		case RDPUDP_PROTOCOL_VERSION_2:
			break;
		case RDPUDP_PROTOCOL_VERSION_3:
			/* a server that rejected the cookie hash answers with version 2 */
			if (!udp->offerV3)
				version = RDPUDP_PROTOCOL_VERSION_2;
			break;
		default:
			version = RDPUDP_PROTOCOL_VERSION_1;
			break;
	}

	if ((upMtu < RDPUDP_MIN_MTU) || (upMtu > RDPUDP_MTU))
		upMtu = RDPUDP_MIN_MTU;

	udp->mtu = upMtu;
	udp->version = version;
	udp->peerWindow = window;
	udp->peerInitialSequence = peerInitial;
	udp->recvNext = peerInitial + 1;
	udp->recvHighest = peerInitial;
	udp->ackVectorStart = peerInitial + 1;
	udp->lastDelivery = now_ms();
	udp->state = RDPUDP_STATE_ESTABLISHED;

	if (version == RDPUDP_PROTOCOL_VERSION_3)
	{
		/* The 32 bit sequence numbers of the SYN exchange only exist for versions 1 and 2. The
		 * peer's RDP-UDP2 sequence space is learned from its first DATA packet. */
		udp->nextCoded = RDPUDP2_INITIAL_SEQUENCE;
		udp->nextChannelSeq = 1;
	}
	else
	{
		udp->nextSource = udp->initialSequence + 1;
		udp->nextCoded = udp->initialSequence + 1;
		/* the client completes the three way handshake by acknowledging the SYN+ACK */
		udp->ackPending = TRUE;
	}

	WLog_Print(udp->log, WLOG_INFO,
	           "RDP-UDP connected, protocol version 0x%04" PRIx16 ", MTU %" PRIu16, version, upMtu);
	return flush_send_queue(udp);
}

static BOOL process_datagram(rdpUdp* udp, const BYTE* data, size_t length)
{
	udp->lastReceived = now_ms();

	switch (udp->state)
	{
		case RDPUDP_STATE_SYN_SENT:
			return process_syn_ack(udp, data, length);
		case RDPUDP_STATE_ESTABLISHED:
			if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
				return v3_process_datagram(udp, data, length);
			return v1_process_datagram(udp, data, length);
		default:
			return TRUE;
	}
}

/* ------------------------------------------------------------------------------------------ */
/* Timers                                                                                     */
/* ------------------------------------------------------------------------------------------ */

static BOOL run_timers(rdpUdp* udp)
{
	const UINT64 now = now_ms();

	if (udp->state == RDPUDP_STATE_SYN_SENT)
	{
		if (now - udp->synSent < RDPUDP_SYN_TIMEOUT_MS)
			return TRUE;
		if (udp->synAttempts >= RDPUDP_SYN_ATTEMPTS)
		{
			rdpudp_fail(udp, "no SYN+ACK from the server");
			return FALSE;
		}
		return send_syn(udp);
	}

	if (udp->state != RDPUDP_STATE_ESTABLISHED)
		return FALSE;

	if (now - udp->lastReceived > RDPUDP_PEER_TIMEOUT_MS)
	{
		rdpudp_fail(udp, "nothing heard from the server for 65 seconds");
		return FALSE;
	}

	/* stuck: chunks have been waiting behind a hole, and nothing moved, for the whole timeout */
	const UINT64 progress =
	    (udp->bufferingSince > udp->lastDelivery) ? udp->bufferingSince : udp->lastDelivery;
	if ((udp->buffered > 0) && (now - progress > RDPUDP_STALL_TIMEOUT_MS))
	{
		rdpudp_fail(udp, "the stream is stuck behind a chunk the server no longer resends");
		return FALSE;
	}

	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		rdpudp_pending* p = &udp->pending[x];
		if (!p->used || (now < p->due))
			continue;
		if (p->retransmits >= RDPUDP_MAX_RETRANSMITS)
		{
			rdpudp_fail(udp, "a datagram went unacknowledged too many times");
			return FALSE;
		}
		p->retransmits++;
		p->rto = (p->rto * 2 > RDPUDP_MAX_RTO_MS) ? RDPUDP_MAX_RTO_MS : p->rto * 2;
		p->due = now + p->rto;
		if (udp->version != RDPUDP_PROTOCOL_VERSION_3)
			udp->cwrPending = TRUE;
		WLog_Print(udp->log, WLOG_TRACE, "retransmitting 0x%08" PRIx32 " (#%" PRIu32 ")", p->seq,
		           p->retransmits);
		if (!rdpudp_transmit(udp, p))
			return FALSE;
	}

	if (now - udp->lastSent >= RDPUDP_KEEPALIVE_MS)
	{
		if (!rdpudp_send_ack(udp))
			return FALSE;
		udp->lastSent = now;
	}

	return TRUE;
}

/* ------------------------------------------------------------------------------------------ */
/* Public interface                                                                           */
/* ------------------------------------------------------------------------------------------ */

rdpUdp* rdpudp_new(wLog* log, const BYTE* cookieHash, rdpUdpReceiveFn receive, void* custom)
{
	WINPR_ASSERT(log);
	WINPR_ASSERT(receive);

	rdpUdp* udp = calloc(1, sizeof(rdpUdp));
	if (!udp)
		return nullptr;

	udp->log = log;
	udp->sockfd = INVALID_SOCKET;
	udp->state = RDPUDP_STATE_CLOSED;
	udp->receive = receive;
	udp->custom = custom;
	udp->mtu = RDPUDP_MTU;
	udp->offerV3 = (cookieHash != nullptr);
	udp->version = udp->offerV3 ? RDPUDP_PROTOCOL_VERSION_3 : RDPUDP_PROTOCOL_VERSION_2;
	if (cookieHash)
		memcpy(udp->cookieHash, cookieHash, sizeof(udp->cookieHash));

	udp->sendQueue = Queue_New(FALSE, -1, -1);
	if (!udp->sendQueue)
		goto fail;
	wObject* obj = Queue_Object(udp->sendQueue);
	obj->fnObjectFree = chunk_free;

	if (winpr_RAND(&udp->initialSequence, sizeof(udp->initialSequence)) < 0)
		goto fail;

	return udp;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	rdpudp_free(udp);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

void rdpudp_free(rdpUdp* udp)
{
	if (!udp)
		return;

	if (udp->sockfd != INVALID_SOCKET)
		closesocket(udp->sockfd);

	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
		pending_clear(&udp->pending[x]);
	for (size_t x = 0; x < RDPUDP_RECEIVE_SLOTS; x++)
		slot_clear(&udp->slots[x]);
	Queue_Free(udp->sendQueue);
	free(udp);
}

static BOOL set_non_blocking(SOCKET sockfd)
{
#if defined(_WIN32)
	u_long arg = 1;
	return ioctlsocket(sockfd, FIONBIO, &arg) == 0;
#else
	const int flags = fcntl((int)sockfd, F_GETFL);
	if (flags < 0)
		return FALSE;
	return fcntl((int)sockfd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

BOOL rdpudp_connect(rdpUdp* udp, const struct sockaddr* addr, size_t addrlen)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(addr);

	if (udp->state != RDPUDP_STATE_CLOSED)
		return FALSE;

	udp->sockfd = socket(addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (udp->sockfd == INVALID_SOCKET)
	{
		WLog_Print(udp->log, WLOG_ERROR, "failed to create the UDP socket");
		return FALSE;
	}

	if (connect(RDPUDP_FD(udp->sockfd), addr, (int)addrlen) != 0)
	{
		WLog_Print(udp->log, WLOG_ERROR, "failed to connect the UDP socket");
		return FALSE;
	}

	if (!set_non_blocking(udp->sockfd))
		return FALSE;

	udp->state = RDPUDP_STATE_SYN_SENT;
	udp->lastReceived = now_ms();
	return send_syn(udp);
}

SOCKET rdpudp_get_socket(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return udp->sockfd;
}

RDPUDP_STATE rdpudp_get_state(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return udp->state;
}

UINT16 rdpudp_get_version(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return udp->version;
}

size_t rdpudp_get_max_payload(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	/* v3: prefix, header, ACK, DelayAckInfo, AckOfAcks, DataHeader and channel sequence.
	 * v1/v2: header, room for the ACK vector and AckOfAcks, source payload header. */
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		return udp->mtu - 24u;
	return udp->mtu - (8u + RDPUDP_ACK_ROOM + 8u);
}

UINT32 rdpudp_get_timeout(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	const UINT64 now = now_ms();
	UINT64 timeout = 50;

	if (udp->state == RDPUDP_STATE_SYN_SENT)
	{
		const UINT64 due = udp->synSent + RDPUDP_SYN_TIMEOUT_MS;
		if (due <= now)
			return 0;
		if (due - now < timeout)
			timeout = due - now;
		return (UINT32)timeout;
	}

	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		const rdpudp_pending* p = &udp->pending[x];
		if (!p->used)
			continue;
		if (p->due <= now)
			return 0;
		if (p->due - now < timeout)
			timeout = p->due - now;
	}
	return (UINT32)timeout;
}

BOOL rdpudp_check(rdpUdp* udp)
{
	WINPR_ASSERT(udp);

	if ((udp->state == RDPUDP_STATE_FAILED) || (udp->state == RDPUDP_STATE_CLOSED))
		return FALSE;

	BYTE buffer[0x10000];
	while (TRUE)
	{
		const SSIZE_T rc = recv(RDPUDP_FD(udp->sockfd), (char*)buffer, sizeof(buffer), 0);
		if (rc < 0)
		{
#if defined(_WIN32)
			const int err = WSAGetLastError();
			if ((err == WSAEWOULDBLOCK) || (err == WSAECONNRESET))
				break;
#else
			const int err = errno;
			if ((err == EAGAIN) || (err == EWOULDBLOCK) || (err == EINTR) || (err == ECONNREFUSED))
				break;
#endif
			WLog_Print(udp->log, WLOG_DEBUG, "recv failed with %d", err);
			break;
		}

		if (!process_datagram(udp, buffer, (size_t)rc))
		{
			rdpudp_fail(udp, "failed to process a datagram");
			return FALSE;
		}
		if (udp->state == RDPUDP_STATE_FAILED)
			return FALSE;
	}

	/* Acknowledge everything that arrived in one go, unless a data packet carried it already. */
	if (udp->ackPending && !rdpudp_send_ack(udp))
		return FALSE;

	if (!flush_send_queue(udp))
		return FALSE;

	return run_timers(udp);
}
