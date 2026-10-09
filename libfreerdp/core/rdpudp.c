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

#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/sysinfo.h>
#include <winpr/collections.h>
#include <winpr/stream.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "rdpudp.h"

/* [MS-RDPEUDP] 2.2.2.1 RDPUDP_FEC_HEADER uFlags */
typedef enum WINPR_C23_ENUM_TYPE(uint16_t)
{
	RDPUDP_FLAG_SYN = 0x0001,
	RDPUDP_FLAG_ACK = 0x0004,
	RDPUDP_FLAG_DATA = 0x0008,
	RDPUDP_FLAG_FEC = 0x0010,
	RDPUDP_FLAG_CN = 0x0020,
	RDPUDP_FLAG_CWR = 0x0040,
	RDPUDP_FLAG_ACK_OF_ACKS = 0x0100,
	RDPUDP_FLAG_CORRELATION_ID = 0x0800,
	RDPUDP_FLAG_SYNEX = 0x1000
} RDPUDP_FLAGS;

/* [MS-RDPEUDP] 2.2.2.9 RDPUDP_SYNDATAEX_PAYLOAD uSynExFlags */
typedef enum WINPR_C23_ENUM_TYPE(uint16_t)
{
	RDPUDP_VERSION_INFO_VALID = 0x0001
} RDPUDP_SYNEX_FLAGS;

/* [MS-RDPEUDP] 2.2.1.1 VECTOR_ELEMENT_STATE */
typedef enum WINPR_C23_ENUM_TYPE(uint8_t)
{
	DATAGRAM_RECEIVED = 0,
	DATAGRAM_NOT_YET_RECEIVED = 3
} RDPUDP_VECTOR_ELEMENT_STATE;

/* [MS-RDPEUDP2] 2.2.1.1 RDPUDP2_PACKET_HEADER Flags */
typedef enum WINPR_C23_ENUM_TYPE(uint16_t)
{
	RDPUDP2_ACK = 0x001,
	RDPUDP2_DATA = 0x004,
	RDPUDP2_ACKVEC = 0x008,
	RDPUDP2_AOA = 0x010,
	RDPUDP2_OVERHEADSIZE = 0x040,
	RDPUDP2_DELAYACKINFO = 0x100,
	RDPUDP2_NOACK = 0x200
} RDPUDP2_FLAGS;

/* [MS-RDPEUDP2] 2.2.1 packet type index of the prefix byte */
typedef enum WINPR_C23_ENUM_TYPE(uint8_t)
{
	RDPUDP2_PACKET_STANDARD = 0,
	RDPUDP2_PACKET_DUMMY = 8
} RDPUDP2_PACKET_TYPE;

/* [MS-RDPEUDP] 2.2.2.1: the ACK vector holds at most 2048 elements */
#define RDPUDP_MAX_ACK_VECTOR 2048

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

/* [MS-RDPEUDP2] 3.1.5.2: a receiver acknowledges at least every 8 data packets until the peer's
 * DelayAckInfo says otherwise, which allows at most 15. */
#define RDPUDP2_DEFAULT_DELAYED_ACKS 8
#define RDPUDP2_MAX_DELAYED_ACKS 15

/* Asked for as the socket receive buffer. A redraw arrives as a burst of hundreds of datagrams,
 * more than the default buffer holds, and a datagram that does not fit waits for a resend. */
#define RDPUDP_RECEIVE_BUFFER (2 * 1024 * 1024)

/* RDPUDP2 ACK vectors hold at most 127 state map bytes of 7 packets each. */
#define RDPUDP2_MAX_ACKVEC 127

/* Room left in every version 1/2 data datagram for the ACK vector and AckOfAcks. */
#define RDPUDP_ACK_ROOM 64

#define RDPUDP_SYN_TIMEOUT_MS 1000
#define RDPUDP_SYN_ATTEMPTS 4
#define RDPUDP_MAX_RTO_MS 5000

/* [MS-RDPEUDP] 3.1.5.4: a datagram retransmitted three to five times without a response ends
 * the connection, and so does 65 seconds without any datagram from the peer (3.1.1.9). */
#define RDPUDP_MAX_RETRANSMITS 5
#define RDPUDP_KEEPALIVE_MS 5000
#define RDPUDP_PEER_TIMEOUT_MS 65000

/* [MS-RDPEUDP2] 3.1.1.3: both ends send at least every 16 seconds, Windows every 4, and 16
 * seconds without a datagram mean the peer is gone. The retransmit limit is left to the sender,
 * the peer timeout already bounds how long a dead connection goes unnoticed. */
#define RDPUDP2_MAX_RETRANSMITS 10
#define RDPUDP2_KEEPALIVE_MS 4000
#define RDPUDP2_PEER_TIMEOUT_MS 16000

/* A stream chunk that stays missing while later ones pile up behind it for this long will not
 * come back: the peer only resends a lost chunk while it still has it outstanding. */
#define RDPUDP_STALL_TIMEOUT_MS 10000

/* How long the stream waits at channel sequence 0 for a chunk that may never have been sent,
 * see v3_skip_channel_zero. */
#define RDPUDP_CHANNEL_ZERO_GRACE_MS 1000

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
	UINT64 v3Arrival[RDPUDP_RECEIVE_SLOTS]; /* when each data sequence arrived, in ns */
	UINT64 v3InOrderArrival;                /* when data sequence v3Expected - 1 arrived */
	BOOL v3InOrderReceived; /* FALSE while v3Expected - 1 is one AckOfAcks gave up on */
	UINT64 v3LatestArrival; /* when the latest data packet arrived */
	size_t v3MaxDelayedAcks;
	size_t v3UnackedPackets; /* data packets received since our last acknowledgement */
	BOOL v3AckedGap;         /* whether our last acknowledgement reported a missing packet */
	UINT16 v3NextChannel;
	UINT16 v3LastChannelDataSeq;  /* data sequence that carried channel sequence 0xFFFF */
	UINT16 v3FirstChannelDataSeq; /* data sequence that carried channel sequence 1 */
	UINT64 v3ZeroWaitSince;

	/* One received datagram. Kept here rather than on the stack, rdpudp_check is not reentrant. */
	BYTE recvBuffer[0x10000];
};

/* RDP-UDP sequence numbers are 16 (RDPUDP2) or 32 (RDPUDP) bit counters that wrap around, they
 * are compared and advanced with RFC 1982 serial number arithmetic. The wrap in the helpers
 * below is intentional, they mask to the counter width before the checked conversion. */
WINPR_ATTR_NODISCARD
static inline UINT16 seq16_add(UINT16 a, size_t n)
{
	return WINPR_ASSERTING_INT_CAST(UINT16, (a + n) & 0xFFFFu);
}

/* a - b modulo 2^16 */
WINPR_ATTR_NODISCARD
static inline UINT16 seq16_diff(UINT16 a, UINT16 b)
{
	return WINPR_ASSERTING_INT_CAST(UINT16, ((UINT32)a - (UINT32)b) & 0xFFFFu);
}

WINPR_ATTR_NODISCARD
static inline UINT32 seq32_add(UINT32 a, UINT32 n)
{
	return WINPR_ASSERTING_INT_CAST(UINT32, ((UINT64)a + n) & 0xFFFFFFFFull);
}

/* a - b modulo 2^32 */
WINPR_ATTR_NODISCARD
static inline UINT32 seq32_diff(UINT32 a, UINT32 b)
{
	return WINPR_ASSERTING_INT_CAST(UINT32, ((UINT64)a - (UINT64)b) & 0xFFFFFFFFull);
}

WINPR_ATTR_NODISCARD
static BOOL seq16_after(UINT16 a, UINT16 b)
{
	return (a != b) && (seq16_diff(a, b) < 0x8000);
}

WINPR_ATTR_NODISCARD
static BOOL seq32_after(UINT32 a, UINT32 b)
{
	return (a != b) && (seq32_diff(a, b) < 0x80000000u);
}

/* Channel sequence numbers start at 1 and Windows wraps 65535 to 1, never using 0. Ours do the
 * same; on receiving, 0 is accepted as well, see v3_skip_channel_zero. */
WINPR_ATTR_NODISCARD
static UINT16 channel_seq_next(UINT16 seq)
{
	return (seq == UINT16_MAX) ? 1 : seq16_add(seq, 1);
}

/* [MS-RDPEUDP2] has 24 bit little endian fields, wStream has no helper for those */
static void stream_write_uint24(wStream* s, UINT32 value)
{
	WINPR_ASSERT(s);
	const UINT16 low = WINPR_ASSERTING_INT_CAST(UINT16, value & 0xFFFF);
	const BYTE high = WINPR_ASSERTING_INT_CAST(BYTE, (value >> 16) & 0xFF);
	Stream_Write_UINT16(s, low);
	Stream_Write_UINT8(s, high);
}

WINPR_ATTR_NODISCARD
static UINT64 now_ms(void)
{
	return GetTickCount64();
}

WINPR_ATTR_NODISCARD
static UINT32 rto_floor(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return (udp->version == RDPUDP_PROTOCOL_VERSION_1) ? 500 : 300;
}

WINPR_ATTR_NODISCARD
static UINT32 rto_initial(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	const UINT32 floor = rto_floor(udp);
	const UINT64 rtt2 = (UINT64)udp->srtt * 2ull;
	const UINT64 rto = (rtt2 > floor) ? rtt2 : floor;
	return (rto > RDPUDP_MAX_RTO_MS) ? RDPUDP_MAX_RTO_MS : WINPR_ASSERTING_INT_CAST(UINT32, rto);
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
	WINPR_ASSERT(slot);
	free(slot->data);
	slot->data = nullptr;
	slot->length = 0;
	slot->present = FALSE;
}

static void pending_clear(rdpudp_pending* p)
{
	WINPR_ASSERT(p);
	free(p->payload);
	memset(p, 0, sizeof(*p));
}

static void rdpudp_fail(rdpUdp* udp, const char* why)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(why);
	if (udp->state != RDPUDP_STATE_FAILED)
		WLog_Print(udp->log, WLOG_WARN, "RDP-UDP connection failed: %s", why);
	udp->state = RDPUDP_STATE_FAILED;
}

/* Socket errors are handled as on the TCP transport (transport_bio_simple_read and
 * transport_bio_simple_write): a wait condition is retried and anything else fails. A datagram
 * socket adds the conditions of the path, an ICMP error for an earlier datagram, a full buffer,
 * a network that is down or unreachable for a moment: they cost the one datagram, which the
 * retransmit timer recovers, and the peer timeout ends a path that stays broken. A firewall rule
 * that drops the datagram is one of them, send() fails with EPERM then, which WinPR does not map
 * and reports as 0; TCP never sees an error for that, it retransmits. */
WINPR_ATTR_NODISCARD
static BOOL rdpudp_socket_error_is_transient(int error)
{
	switch (error)
	{
		case 0:
		case WSAEWOULDBLOCK:
		case WSAEINTR:
		case WSAEINPROGRESS:
		case WSAEALREADY:
		case WSAENOBUFS:
		case WSAECONNRESET:
		case WSAECONNREFUSED:
		case WSAENETRESET:
		case WSAENETDOWN:
		case WSAENETUNREACH:
		case WSAEHOSTDOWN:
		case WSAEHOSTUNREACH:
			return TRUE;
		default:
			return FALSE;
	}
}

WINPR_ATTR_NODISCARD
static BOOL rdpudp_send_raw(rdpUdp* udp, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data);
	const int rc = _send(udp->sockfd, (const char*)data, WINPR_ASSERTING_INT_CAST(int, length), 0);
	if (rc < 0)
	{
		const int error = WSAGetLastError();
		if (!rdpudp_socket_error_is_transient(error))
		{
			WLog_Print(udp->log, WLOG_ERROR, "send failed with %d", error);
			rdpudp_fail(udp, "sending on the socket failed");
			return FALSE;
		}
		WLog_Print(udp->log, WLOG_DEBUG, "send failed with %d, the datagram counts as lost", error);
		return TRUE;
	}
	udp->lastSent = now_ms();
	return TRUE;
}

/* ------------------------------------------------------------------------------------------ */
/* [MS-RDPEUDP2] encoding                                                                      */
/* ------------------------------------------------------------------------------------------ */

WINPR_ATTR_NODISCARD
static UINT32 v3_timestamp(UINT64 ns)
{
	/* [MS-RDPEUDP2] 3.1.1.1.4: 24 bits in units of 4 microseconds */
	return WINPR_ASSERTING_INT_CAST(UINT32, (ns / 4000ULL) & 0x00FFFFFFULL);
}

/* Milliseconds an acknowledgement waited after the packet it reports arrived. */
WINPR_ATTR_NODISCARD
static BYTE v3_ack_gap(UINT64 arrival, UINT64 now)
{
	if ((arrival == 0) || (now < arrival))
		return 0;
	const UINT64 gap = (now - arrival) / 1000000ULL;
	return (gap > 254) ? 254 : WINPR_ASSERTING_INT_CAST(BYTE, gap); /* 255 means invalid */
}

WINPR_ATTR_NODISCARD
static BOOL v3_has_gap(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return udp->v3BaseKnown && seq16_after(udp->v3Highest, seq16_diff(udp->v3Expected, 1));
}

/* [MS-RDPEUDP2] 2.2.1.2.1 ACK payload. receivedTS is when the acknowledged packet arrived and
 * sendAckTimeGap how long the acknowledgement waited after that. The peer measures the path from
 * both, a send time in receivedTS would make the time spent here look like network delay. */
static void v3_write_ack(const rdpUdp* udp, wStream* s)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(s);
	const UINT64 now = winpr_GetTickCount64NS();
	/* A sequence number the peer gave up on never arrived. An older packet's arrival time would
	 * place its receipt before it was sent, so it is reported as received now. */
	const UINT64 arrival = udp->v3InOrderReceived ? udp->v3InOrderArrival : now;
	const UINT16 seq = seq16_diff(udp->v3Expected, 1);
	Stream_Write_UINT16(s, seq);                   /* SeqNum */
	stream_write_uint24(s, v3_timestamp(arrival)); /* receivedTS */
	/* the Stream_Write macros evaluate their value more than once */
	const BYTE gap = v3_ack_gap(arrival, now);
	Stream_Write_UINT8(s, gap); /* sendAckTimeGapInMs */
	Stream_Write_UINT8(s, 0);   /* numDelayedAcks, delayAckTimeScale */
}

/* Starts a packet: one byte is left free for the prefix, then the header. */
WINPR_ATTR_NODISCARD
static wStream* v3_packet_init(wStream* buffer, BYTE* data, size_t size, UINT16 flags)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(data);
	wStream* s = Stream_StaticInit(buffer, data, size);
	if (!s)
		return nullptr;
	Stream_Seek(s, 1);
	const UINT16 header = WINPR_ASSERTING_INT_CAST(UINT16, flags | (RDPUDP2_LOG_WINDOW_SIZE << 12));
	Stream_Write_UINT16(s, header);
	return s;
}

/* Finishes a packet into its on-wire form and sends it ([MS-RDPEUDP2] 2.2.1): the prefix byte
 * goes in front, a short packet is padded, and the first and eighth byte trade places. */
WINPR_ATTR_NODISCARD
static BOOL v3_send_packet(rdpUdp* udp, wStream* s, BYTE packetType)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(s);
	if (Stream_GetPosition(s) < 1)
		return FALSE;
	const size_t layout = Stream_GetPosition(s) - 1;
	BYTE shortLength = 7;
	if (layout < 7)
	{
		shortLength = WINPR_ASSERTING_INT_CAST(BYTE, layout);
		Stream_Zero(s, 7 - layout);
	}
	const size_t length = Stream_GetPosition(s);

	if (!Stream_SetPosition(s, 0))
		return FALSE;
	const BYTE prefix =
	    WINPR_ASSERTING_INT_CAST(BYTE, (shortLength << 5) | ((packetType & 0x0F) << 1));
	Stream_Write_UINT8(s, prefix);

	/* the swap scrambles the finished datagram, it is not a field of it */
	BYTE* wire = Stream_Buffer(s);
	const BYTE eighth = wire[7];
	wire[7] = wire[0];
	wire[0] = eighth;
	return rdpudp_send_raw(udp, wire, length);
}

WINPR_ATTR_NODISCARD
static BOOL v3_send_ack(rdpUdp* udp)
{
	BYTE buffer[64 + RDPUDP2_MAX_ACKVEC] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(udp);
	if (!udp->v3BaseKnown)
		return TRUE;

	udp->ackPending = FALSE;
	udp->v3UnackedPackets = 0;

	/* A cumulative ACK cannot describe a hole, the ACK vector reports the exact window so only
	 * what is missing gets resent. ACK and ACKVEC are mutually exclusive. */
	const BOOL gap = v3_has_gap(udp);
	udp->v3AckedGap = gap;
	wStream* s = v3_packet_init(&sbuffer, buffer, sizeof(buffer),
	                            RDPUDP2_OVERHEADSIZE | (gap ? RDPUDP2_ACKVEC : RDPUDP2_ACK));
	if (!s)
		return FALSE;
	if (!gap)
	{
		v3_write_ack(udp, s);
		Stream_Write_UINT8(s, 10); /* OverheadSize */
		return v3_send_packet(udp, s, RDPUDP2_PACKET_STANDARD);
	}

	Stream_Write_UINT8(s, 10); /* OverheadSize */

	const UINT16 base = udp->v3Expected;
	const size_t span = (size_t)seq16_diff(udp->v3Highest, base) + 1;
	size_t count = (span + 6) / 7;
	if (count > RDPUDP2_MAX_ACKVEC)
		count = RDPUDP2_MAX_ACKVEC;

	/* [MS-RDPEUDP2] 2.2.1.2.6 ACK vector payload. TimeStamp is when the highest sequence number
	 * received arrived, SendAckTimeGapInMs the time since the latest data packet arrived. */
	const UINT64 now = winpr_GetTickCount64NS();
	const UINT64 highest = udp->v3Arrival[udp->v3Highest % RDPUDP_RECEIVE_SLOTS];
	const BYTE control = WINPR_ASSERTING_INT_CAST(BYTE, 0x80 | count);
	Stream_Write_UINT16(s, base);   /* BaseSeqNum */
	Stream_Write_UINT8(s, control); /* TimeStampPresent, codedAckVecSize */
	stream_write_uint24(s, v3_timestamp((highest != 0) ? highest : now)); /* TimeStamp */
	const BYTE ackGap = v3_ack_gap(udp->v3LatestArrival, now);
	Stream_Write_UINT8(s, ackGap); /* SendAckTimeGapInMs */
	for (size_t x = 0; x < count; x++)
	{
		BYTE bits = 0;
		for (size_t bit = 0; bit < 7; bit++)
		{
			const UINT16 seq = seq16_add(base, x * 7 + bit);
			if (udp->v3Received[seq % RDPUDP_RECEIVE_SLOTS] &&
			    seq16_diff(seq, base) < RDPUDP_RECEIVE_REACH)
				bits |= WINPR_ASSERTING_INT_CAST(BYTE, 1 << bit);
		}
		Stream_Write_UINT8(s, bits); /* state map, bit 0 is the base */
	}
	return v3_send_packet(udp, s, RDPUDP2_PACKET_STANDARD);
}

/* The lowest data sequence number of ours still awaiting acknowledgement, which is what
 * AckOfAcks tells the peer ([MS-RDPEUDP2] 2.2.1.2.4). */
WINPR_ATTR_NODISCARD
static UINT32 sender_base(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	UINT32 base = (udp->version == RDPUDP_PROTOCOL_VERSION_3) ? udp->nextCoded : udp->nextSource;
	BOOL found = FALSE;
	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		const rdpudp_pending* p = &udp->pending[x];
		if (!p->used)
			continue;
		if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		{
			if (!found || seq16_after(WINPR_ASSERTING_INT_CAST(UINT16, base),
			                          WINPR_ASSERTING_INT_CAST(UINT16, p->seq)))
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

WINPR_ATTR_NODISCARD
static BOOL v3_send_data(rdpUdp* udp, const rdpudp_pending* p)
{
	BYTE buffer[RDPUDP_MTU + 32] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);

	const BOOL withAck = udp->ackPending && udp->v3BaseKnown && !v3_has_gap(udp);
	const UINT16 base = WINPR_ASSERTING_INT_CAST(UINT16, sender_base(udp));
	/* repeated on a resend, the first announcement may have been lost with it */
	const BOOL withAoA = !udp->aoaAnnounced || (udp->announcedAoA != base) || (p->retransmits > 0);

	/* Windows sets NOACK on every DATA packet without an ACK */
	UINT16 flags = RDPUDP2_DATA | RDPUDP2_DELAYACKINFO;
	flags |= withAck ? RDPUDP2_ACK : RDPUDP2_NOACK;
	if (withAoA)
		flags |= RDPUDP2_AOA;

	wStream* s = v3_packet_init(&sbuffer, buffer, sizeof(buffer), flags);
	if (!s)
		return FALSE;
	if (withAck)
	{
		v3_write_ack(udp, s);
		udp->ackPending = FALSE;
		udp->v3UnackedPackets = 0;
		udp->v3AckedGap = FALSE;
	}

	Stream_Write_UINT8(s, 1);   /* MaxDelayedAcks */
	Stream_Write_UINT16(s, 20); /* DelayedAckTimeoutInMs */

	if (withAoA)
	{
		/* AckOfAcks: the lowest sequence number still awaiting acknowledgement */
		Stream_Write_UINT16(s, base);
		udp->announcedAoA = base;
		udp->aoaAnnounced = TRUE;
	}

	const UINT16 dataSeq = WINPR_ASSERTING_INT_CAST(UINT16, p->seq);
	Stream_Write_UINT16(s, dataSeq);       /* DataSeqNum */
	Stream_Write_UINT16(s, p->channelSeq); /* ChannelSeqNum */
	if (!Stream_CheckAndLogRequiredCapacityWLog(udp->log, s, p->length))
		return FALSE;
	Stream_Write(s, p->payload, p->length);

	return v3_send_packet(udp, s, RDPUDP2_PACKET_STANDARD);
}

/* ------------------------------------------------------------------------------------------ */
/* [MS-RDPEUDP] version 1 and 2 encoding                                                      */
/* ------------------------------------------------------------------------------------------ */

/* Writes an RDPUDP_ACK_VECTOR_HEADER covering the peer's packets up to the highest one seen,
 * using at most room bytes. */
static void v1_write_ack_vector(const rdpUdp* udp, wStream* s, size_t room)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(s);
	const UINT32 highest = udp->recvHighest;
	UINT32 low = udp->ackVectorStart;
	if (!udp->receivedData || seq32_after(low, highest))
		low = highest;
	if (seq32_diff(highest, low) >= RDPUDP_RECEIVE_REACH)
		low = seq32_diff(highest, RDPUDP_RECEIVE_REACH - 1);

	BYTE elements[RDPUDP_MAX_ACK_VECTOR] = WINPR_C_ARRAY_INIT;
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
				elements[count++] = WINPR_ASSERTING_INT_CAST(BYTE, (state << 6) | run);
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
		elements[count++] = WINPR_ASSERTING_INT_CAST(BYTE, (state << 6) | run);

	/* Too long to fit: keep the newest part, the older part has been reported before. */
	size_t skip = 0;
	while ((2 + (count - skip) + 3) > room && (skip < count))
		skip++;
	count -= skip;

	const UINT16 vectorSize = WINPR_ASSERTING_INT_CAST(UINT16, count);
	Stream_Write_UINT16_BE(s, vectorSize);       /* uAckVectorSize */
	Stream_Write(s, &elements[skip], count);     /* AckVector */
	Stream_Zero(s, (4 - ((2 + count) % 4)) % 4); /* pad to a DWORD boundary */
}

WINPR_ATTR_NODISCARD
static UINT32 v1_cumulative_ack(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return seq32_diff(sender_base(udp), 1);
}

WINPR_ATTR_NODISCARD
static BOOL v1_send_packet(rdpUdp* udp, const rdpudp_pending* p)
{
	BYTE buffer[RDPUDP_MTU + 2048] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(udp);
	WINPR_ASSERT(udp->mtu >= RDPUDP_MIN_MTU);

	const UINT32 cumulative = v1_cumulative_ack(udp);
	const BOOL withAoA = !udp->aoaAnnounced || (udp->announcedAoA != cumulative);

	UINT16 flags = RDPUDP_FLAG_ACK;
	if (withAoA)
		flags |= RDPUDP_FLAG_ACK_OF_ACKS;
	if (p)
	{
		flags |= RDPUDP_FLAG_DATA;
		if (udp->cwrPending)
		{
			flags |= RDPUDP_FLAG_CWR;
			udp->cwrPending = FALSE;
		}
	}

	wStream* s = Stream_StaticInit(&sbuffer, buffer, sizeof(buffer));
	if (!s)
		return FALSE;

	/* RDPUDP_FEC_HEADER */
	Stream_Write_UINT32_BE(s, udp->receivedData ? udp->recvHighest
	                                            : udp->peerInitialSequence); /* snSourceAck */
	Stream_Write_UINT16_BE(s, RDPUDP_RECEIVE_WINDOW); /* uReceiveWindowSize */
	Stream_Write_UINT16_BE(s, flags);                 /* uFlags */

	/* an ACK-only datagram must stay within the MTU too: header, vector, AckOfAcks */
	v1_write_ack_vector(udp, s, p ? RDPUDP_ACK_ROOM - 4 : udp->mtu - 12u);
	udp->ackPending = FALSE;

	if (withAoA)
	{
		Stream_Write_UINT32_BE(s, cumulative); /* snResetSeqNum */
		udp->announcedAoA = cumulative;
		udp->aoaAnnounced = TRUE;
	}

	if (p)
	{
		/* RDPUDP_SOURCE_PAYLOAD_HEADER, a retransmission gets a new coded sequence number */
		const UINT32 snCoded = udp->nextCoded++;
		Stream_Write_UINT32_BE(s, snCoded); /* snCoded */
		Stream_Write_UINT32_BE(s, p->seq);  /* snSourceStart */
		if (!Stream_CheckAndLogRequiredCapacityWLog(udp->log, s, p->length))
			return FALSE;
		Stream_Write(s, p->payload, p->length);
	}

	return rdpudp_send_raw(udp, Stream_Buffer(s), Stream_GetPosition(s));
}

WINPR_ATTR_NODISCARD
static BOOL rdpudp_send_ack(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (udp->state != RDPUDP_STATE_ESTABLISHED)
		return TRUE;
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		return v3_send_ack(udp);
	return v1_send_packet(udp, nullptr);
}

/* [MS-RDPEUDP2] 3.1.5.2 and 4.2: a receiver acknowledges at least every MaxDelayedAcks data
 * packets, and at once when a packet goes missing or a missing one turns up. Checked after every
 * datagram, so a burst is acknowledged while it is still arriving rather than when it ended. */
WINPR_ATTR_NODISCARD
static BOOL v3_ack_if_due(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if ((udp->version != RDPUDP_PROTOCOL_VERSION_3) || (udp->state != RDPUDP_STATE_ESTABLISHED) ||
	    !udp->ackPending)
		return TRUE;
	if ((v3_has_gap(udp) == udp->v3AckedGap) && (udp->v3UnackedPackets < udp->v3MaxDelayedAcks))
		return TRUE;
	return v3_send_ack(udp);
}

WINPR_ATTR_NODISCARD
static BOOL rdpudp_transmit(rdpUdp* udp, const rdpudp_pending* p)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		return v3_send_data(udp, p);
	return v1_send_packet(udp, p);
}

/* ------------------------------------------------------------------------------------------ */
/* Sending                                                                                    */
/* ------------------------------------------------------------------------------------------ */

WINPR_ATTR_NODISCARD
static size_t window_limit(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	size_t limit = RDPUDP_MAX_IN_FLIGHT;
	if ((udp->version != RDPUDP_PROTOCOL_VERSION_3) && (udp->peerWindow > 0) &&
	    (udp->peerWindow < limit))
		limit = udp->peerWindow;
	return limit;
}

/* Takes ownership of data, also when it fails. */
WINPR_ATTR_NODISCARD
static BOOL send_chunk_now(rdpUdp* udp, BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data);
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
	{
		free(data);
		return FALSE;
	}

	/* the pending slot owns the payload from here, pending_clear frees it */
	p->used = TRUE;
	p->payload = data;
	p->length = length;
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
	{
		const UINT16 coded = WINPR_ASSERTING_INT_CAST(UINT16, udp->nextCoded);
		p->seq = coded;
		udp->nextCoded = seq16_add(coded, 1);
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

WINPR_ATTR_NODISCARD
static BOOL flush_send_queue(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
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
			return FALSE;
	}
	return TRUE;
}

BOOL rdpudp_send(rdpUdp* udp, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data);

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
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): Queue_Enqueue takes ownership of chunk
	return flush_send_queue(udp);
}

static void sample_rtt(rdpUdp* udp, const rdpudp_pending* p)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);
	/* Karn: a retransmitted packet says nothing reliable about the round trip */
	if (p->retransmits > 0)
		return;
	const UINT64 sample64 = now_ms() - p->sent;
	const UINT32 sample =
	    (sample64 > UINT32_MAX) ? UINT32_MAX : WINPR_ASSERTING_INT_CAST(UINT32, sample64);
	udp->srtt = (udp->srtt == 0)
	                ? sample
	                : WINPR_ASSERTING_INT_CAST(UINT32, ((UINT64)udp->srtt * 7 + sample) / 8);
}

static void acknowledge(rdpUdp* udp, rdpudp_pending* p)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);
	sample_rtt(udp, p);
	pending_clear(p);
	if (udp->inFlight > 0)
		udp->inFlight--;
}

static void acknowledge_seq(rdpUdp* udp, UINT32 seq)
{
	WINPR_ASSERT(udp);
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

WINPR_ATTR_NODISCARD
static BOOL deliver(rdpUdp* udp, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	udp->lastDelivery = now_ms();
	if (length == 0)
		return TRUE;
	WINPR_ASSERT(data);
	WINPR_ASSERT(udp->receive);
	if (!udp->receive(udp->custom, data, length))
	{
		rdpudp_fail(udp, "receiver rejected data");
		return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL store_slot(rdpUdp* udp, UINT32 seq, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data || (length == 0));
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

WINPR_ATTR_NODISCARD
static BOOL v1_receive_source(rdpUdp* udp, UINT32 seq, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data || (length == 0));
	if (!udp->receivedData)
	{
		/* [MS-RDPEUDP] puts the first source packet at the initial sequence number plus one,
		 * an implementation that uses the initial sequence number itself is accepted too. */
		if (seq == udp->peerInitialSequence)
		{
			udp->recvNext = seq;
			udp->ackVectorStart = seq;
		}
		udp->recvHighest = seq32_diff(udp->recvNext, 1);
	}

	udp->ackPending = TRUE;

	if (seq32_after(udp->recvNext, seq))
		return TRUE; /* already delivered, the ACK above tells the peer again */

	const UINT32 ahead = seq32_diff(seq, udp->recvNext);
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

/* Applies the peer's ACK vector, which ends at its snSourceAck, to our own packets. */
static void v1_process_ack_vector(rdpUdp* udp, UINT32 snSourceAck, wStream* s, size_t count)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(s);
	/* count comes from the wire, v1_process_datagram bounds it */
	WINPR_ASSERT(count <= RDPUDP_MAX_ACK_VECTOR);
	const size_t start = Stream_GetPosition(s);
	UINT32 total = 0;
	for (size_t x = 0; x < count; x++)
		total += Stream_Get_UINT8(s) & 0x3F;
	if ((total == 0) || !Stream_SetPosition(s, start))
		return;

	UINT32 seq = seq32_add(seq32_diff(snSourceAck, total), 1);
	for (size_t x = 0; x < count; x++)
	{
		const BYTE element = Stream_Get_UINT8(s);
		const BYTE state = WINPR_ASSERTING_INT_CAST(BYTE, element >> 6);
		const BYTE run = element & 0x3F;
		for (BYTE y = 0; y < run; y++)
		{
			if (state == DATAGRAM_RECEIVED)
				acknowledge_seq(udp, seq);
			seq++;
		}
	}
}

WINPR_ATTR_NODISCARD
static BOOL v1_process_datagram(rdpUdp* udp, const BYTE* data, size_t length)
{
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data);
	wStream* s = Stream_StaticConstInit(&sbuffer, data, length);

	/* RDPUDP_FEC_HEADER */
	if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 8))
		return TRUE;
	const UINT32 snSourceAck = Stream_Get_UINT32_BE(s);
	const UINT16 window = Stream_Get_UINT16_BE(s);
	const UINT16 flags = Stream_Get_UINT16_BE(s);

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
		/* RDPUDP_ACK_VECTOR_HEADER */
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 2))
			return TRUE;
		const size_t size = Stream_Get_UINT16_BE(s);
		if (size > RDPUDP_MAX_ACK_VECTOR)
			return TRUE; /* [MS-RDPEUDP] 2.2.2.7: more than 2048 elements is invalid, drop */
		const size_t padding = (4 - ((2 + size) % 4)) % 4;
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, size + padding))
			return TRUE;
		v1_process_ack_vector(udp, snSourceAck, s, size);
		Stream_Seek(s, padding);
	}

	if (flags & RDPUDP_FLAG_ACK_OF_ACKS)
	{
		/* RDPUDP_ACK_OF_ACKVECTOR_HEADER */
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 4))
			return TRUE;
		const UINT32 reset = Stream_Get_UINT32_BE(s);
		/* the peer has seen our acknowledgements up to here, the ACK vector can start later */
		const UINT32 start = seq32_add(reset, 1);
		if (seq32_after(start, udp->ackVectorStart) && !seq32_after(start, udp->recvNext))
			udp->ackVectorStart = start;
	}

	if ((flags & RDPUDP_FLAG_CORRELATION_ID) && !Stream_SafeSeek(s, 32))
		return TRUE;

	if (!(flags & RDPUDP_FLAG_DATA))
		return TRUE;

	if (flags & RDPUDP_FLAG_FEC)
		return TRUE; /* FEC packets are not acknowledged, losses are recovered by resending */

	/* RDPUDP_SOURCE_PAYLOAD_HEADER */
	if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 8))
		return TRUE;
	Stream_Seek_UINT32(s); /* snCoded */
	const UINT32 snSourceStart = Stream_Get_UINT32_BE(s);
	return v1_receive_source(udp, snSourceStart, Stream_ConstPointer(s),
	                         Stream_GetRemainingLength(s));
}

static void v3_advance_window(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	while (udp->v3Received[udp->v3Expected % RDPUDP_RECEIVE_SLOTS])
	{
		const size_t slot = udp->v3Expected % RDPUDP_RECEIVE_SLOTS;
		udp->v3InOrderArrival = udp->v3Arrival[slot];
		udp->v3InOrderReceived = TRUE;
		udp->v3Received[slot] = FALSE;
		udp->v3Expected++;
	}
}

static void v3_advance_base(rdpUdp* udp, UINT16 base)
{
	WINPR_ASSERT(udp);
	if (!udp->v3BaseKnown || !seq16_after(base, udp->v3Expected))
		return;
	if (seq16_diff(base, udp->v3Expected) > RDPUDP_RECEIVE_REACH)
		return;

	/* The peer stopped resending everything below base. This only gives up transport sequence
	 * numbers: a chunk of the stream sent there comes back under a later one, with its channel
	 * sequence number unchanged. */
	while (udp->v3Expected != base)
	{
		udp->v3Received[udp->v3Expected % RDPUDP_RECEIVE_SLOTS] = FALSE;
		udp->v3Expected++;
	}
	udp->v3InOrderReceived = FALSE;
	const UINT16 last = seq16_diff(base, 1);
	if (seq16_after(last, udp->v3Highest))
		udp->v3Highest = last;
	v3_advance_window(udp);
	udp->ackPending = TRUE;
}

static void v3_process_ack_vector(rdpUdp* udp, UINT16 base, wStream* s, size_t count)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(s);
	UINT16 seq = base;
	for (size_t x = 0; x < count; x++)
	{
		const BYTE entry = Stream_Get_UINT8(s);
		if (entry & 0x80)
		{
			/* run length: received flag and length */
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
			/* state map of 7 packets, bit 0 first */
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
		if (p->used && seq16_after(base, WINPR_ASSERTING_INT_CAST(UINT16, p->seq)))
			acknowledge(udp, p);
	}
}

static void v3_process_ack(rdpUdp* udp, UINT16 ack)
{
	WINPR_ASSERT(udp);
	for (size_t x = 0; x < RDPUDP_MAX_IN_FLIGHT; x++)
	{
		rdpudp_pending* p = &udp->pending[x];
		if (p->used && !seq16_after(WINPR_ASSERTING_INT_CAST(UINT16, p->seq), ack))
			acknowledge(udp, p);
	}
}

/* Waiting at channel sequence 0 with 1 already here. Windows never sends 0; a peer that does
 * sent it before 1. When 1 came in the data sequence right after the one that carried 0xFFFF,
 * nothing was sent in between and there is no 0. Otherwise 0 gets a grace period to turn up as
 * a resend before the stream moves on without it. */
WINPR_ATTR_NODISCARD
static BOOL v3_skip_channel_zero(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (udp->v3NextChannel != 0)
		return FALSE;
	const rdpudp_slot* zero = &udp->slots[0];
	const rdpudp_slot* one = &udp->slots[1];
	if ((zero->present && (zero->seq == 0)) || !one->present || (one->seq != 1))
	{
		udp->v3ZeroWaitSince = 0;
		return FALSE;
	}

	const UINT64 now = now_ms();
	if (udp->v3ZeroWaitSince == 0)
		udp->v3ZeroWaitSince = now;
	const BOOL adjacent = seq16_add(udp->v3LastChannelDataSeq, 1) == udp->v3FirstChannelDataSeq;
	if (!adjacent && (now - udp->v3ZeroWaitSince < RDPUDP_CHANNEL_ZERO_GRACE_MS))
		return FALSE;

	WLog_Print(udp->log, WLOG_DEBUG, "channel sequence wrapped past 0%s",
	           adjacent ? "" : ", which never arrived");
	udp->v3ZeroWaitSince = 0;
	udp->v3NextChannel = 1;
	return TRUE;
}

/* Hands up the chunks that are next in the stream, in channel sequence order. */
WINPR_ATTR_NODISCARD
static BOOL v3_deliver_ready(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	while (TRUE)
	{
		rdpudp_slot* slot = &udp->slots[udp->v3NextChannel % RDPUDP_RECEIVE_SLOTS];
		if (!slot->present || (slot->seq != udp->v3NextChannel))
		{
			if (v3_skip_channel_zero(udp))
				continue;
			break;
		}
		BYTE* chunk = slot->data;
		const size_t chunkLength = slot->length;
		slot->data = nullptr;
		slot->present = FALSE;
		udp->buffered--;
		udp->v3NextChannel = seq16_add(udp->v3NextChannel, 1);
		const BOOL rc = deliver(udp, chunk, chunkLength);
		free(chunk);
		if (!rc)
			return FALSE;
	}
	return TRUE;
}

WINPR_ATTR_NODISCARD
static BOOL v3_receive_channel_data(rdpUdp* udp, UINT16 dataSeq, UINT16 channelSeq,
                                    const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data || (length == 0));
	/* A retransmission travels under a new data sequence number with its original channel
	 * sequence, so the channel sequence alone says where a chunk belongs and whether it has
	 * been handed up already. */
	const UINT16 ahead = seq16_diff(channelSeq, udp->v3NextChannel);
	if (ahead >= RDPUDP_RECEIVE_REACH)
		return TRUE;

	if (channelSeq == UINT16_MAX)
		udp->v3LastChannelDataSeq = dataSeq;
	else if (channelSeq == 1)
		udp->v3FirstChannelDataSeq = dataSeq;

	if (!store_slot(udp, channelSeq, data, length))
		return FALSE;
	return v3_deliver_ready(udp);
}

WINPR_ATTR_NODISCARD
static BOOL v3_process_datagram(rdpUdp* udp, const BYTE* wire, size_t length)
{
	BYTE buffer[RDPUDP_MTU + 1024] = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(udp);
	WINPR_ASSERT(wire);
	if ((length < 8) || (length > sizeof(buffer)))
		return TRUE;

	/* undo the swap of the first and eighth byte ([MS-RDPEUDP2] 2.2.1) */
	memcpy(buffer, wire, length);
	const BYTE eighth = buffer[7];
	buffer[7] = buffer[0];
	buffer[0] = eighth;

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, buffer, length);

	const BYTE prefix = Stream_Get_UINT8(s);
	if (prefix & 0x01)
		return TRUE;
	const BYTE shortLength = WINPR_ASSERTING_INT_CAST(BYTE, prefix >> 5);
	const BYTE packetType = WINPR_ASSERTING_INT_CAST(BYTE, (prefix >> 1) & 0x0F);
	if ((packetType != RDPUDP2_PACKET_STANDARD) && (packetType != RDPUDP2_PACKET_DUMMY))
		return TRUE;
	/* a packet shorter than 7 bytes was padded, its real length is in the prefix */
	if ((shortLength > 0) && (shortLength < 7) && !Stream_SetLength(s, 1ull + shortLength))
		return TRUE;

	if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 2))
		return TRUE;
	const UINT16 flags = Stream_Get_UINT16(s) & 0x0FFF;

	if ((flags & RDPUDP2_ACK) && (flags & RDPUDP2_ACKVEC))
		return TRUE;

	if (flags & RDPUDP2_ACK)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 7))
			return TRUE;
		const UINT16 ack = Stream_Get_UINT16(s);           /* SeqNum */
		Stream_Seek(s, 4);                                 /* receivedTS, sendAckTimeGapInMs */
		const size_t delayed = Stream_Get_UINT8(s) & 0x0F; /* numDelayedAcks */
		if (!Stream_SafeSeek(s, delayed))                  /* DelayAckTimeAdditions */
			return TRUE;
		v3_process_ack(udp, ack);
	}

	if ((flags & RDPUDP2_OVERHEADSIZE) && !Stream_SafeSeek(s, 1))
		return TRUE;

	if (flags & RDPUDP2_DELAYACKINFO)
	{
		/* [MS-RDPEUDP2] 2.2.1.2.3: how many data packets the peer lets us acknowledge at once.
		 * Acknowledgements never wait past the end of a receive round, well inside its
		 * DelayedAckTimeoutInMs. */
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 3))
			return TRUE;
		const size_t maxDelayed = Stream_Get_UINT8(s); /* MaxDelayedAcks */
		Stream_Seek_UINT16(s);                         /* DelayedAckTimeoutInMs */
		if (maxDelayed == 0)
			udp->v3MaxDelayedAcks = 1;
		else if (maxDelayed > RDPUDP2_MAX_DELAYED_ACKS)
			udp->v3MaxDelayedAcks = RDPUDP2_MAX_DELAYED_ACKS;
		else
			udp->v3MaxDelayedAcks = maxDelayed;
	}

	BOOL haveAoA = FALSE;
	UINT16 aoa = 0;
	if (flags & RDPUDP2_AOA)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 2))
			return TRUE;
		aoa = Stream_Get_UINT16(s);
		haveAoA = TRUE;
	}

	BOOL haveData = FALSE;
	UINT16 dataSeq = 0;
	if (flags & RDPUDP2_DATA)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 2))
			return TRUE;
		dataSeq = Stream_Get_UINT16(s);
		haveData = TRUE;
	}

	if (flags & RDPUDP2_ACKVEC)
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 3))
			return TRUE;
		const UINT16 base = Stream_Get_UINT16(s); /* BaseSeqNum */
		const BYTE control = Stream_Get_UINT8(s);
		const size_t size = control & 0x7F; /* codedAckVecSize */
		/* TimeStamp, SendAckTimeGapInMs */
		if ((control & 0x80) && !Stream_SafeSeek(s, 4))
			return TRUE;
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, size))
			return TRUE;
		v3_process_ack_vector(udp, base, s, size);
	}

	/* AckOfAcks moves the lower edge of our receive window. It is the only way past a lost
	 * dummy packet, those are never resent. */
	if (haveAoA)
		v3_advance_base(udp, aoa);

	if (!haveData)
		return TRUE;

	udp->v3LatestArrival = winpr_GetTickCount64NS();
	udp->v3UnackedPackets++;
	udp->receivedData = TRUE;
	if (!udp->v3BaseKnown)
	{
		/* Windows announces where its sequence space starts in the AckOfAcks of its first
		 * DATA packets. Taking the base from that, rather than from whichever packet arrives
		 * first, keeps a lost or late first packet from being acknowledged unseen. */
		UINT16 base = dataSeq;
		if (haveAoA && !seq16_after(aoa, dataSeq) &&
		    (seq16_diff(dataSeq, aoa) < RDPUDP_RECEIVE_REACH))
			base = aoa;
		udp->v3BaseKnown = TRUE;
		udp->v3Expected = base;
		udp->v3Highest = seq16_diff(base, 1);
	}

	udp->ackPending = TRUE;
	const UINT16 ahead = seq16_diff(dataSeq, udp->v3Expected);
	if (ahead >= 0x8000)
		return TRUE; /* resent because our ACK got lost, acknowledged again above */
	if (ahead >= RDPUDP_RECEIVE_REACH)
		return TRUE;

	if (seq16_after(dataSeq, udp->v3Highest))
		udp->v3Highest = dataSeq;
	udp->v3Received[dataSeq % RDPUDP_RECEIVE_SLOTS] = TRUE;
	udp->v3Arrival[dataSeq % RDPUDP_RECEIVE_SLOTS] = udp->v3LatestArrival;
	v3_advance_window(udp);

	/* A dummy packet occupies a sequence number but its body means nothing. */
	if (packetType == RDPUDP2_PACKET_DUMMY)
		return TRUE;
	if (Stream_GetRemainingLength(s) < 2)
		return TRUE;

	const UINT16 channelSeq = Stream_Get_UINT16(s); /* ChannelSeqNum */
	return v3_receive_channel_data(udp, dataSeq, channelSeq, Stream_ConstPointer(s),
	                               Stream_GetRemainingLength(s));
}

/* ------------------------------------------------------------------------------------------ */
/* Connection setup                                                                           */
/* ------------------------------------------------------------------------------------------ */

WINPR_ATTR_NODISCARD
static BOOL send_syn(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	/* [MS-RDPEUDP] 3.1.5.1.1: zero padded to the MTU, which validates the path MTU */
	BYTE syn[RDPUDP_MTU] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, syn, sizeof(syn));
	if (!s)
		return FALSE;
	const UINT16 version = udp->offerV3 ? RDPUDP_PROTOCOL_VERSION_3 : RDPUDP_PROTOCOL_VERSION_2;

	/* RDPUDP_FEC_HEADER */
	Stream_Write_UINT32_BE(s, UINT32_MAX); /* snSourceAck: -1 */
	Stream_Write_UINT16_BE(s, RDPUDP_RECEIVE_WINDOW);
	Stream_Write_UINT16_BE(s, RDPUDP_FLAG_SYN | RDPUDP_FLAG_SYNEX);

	/* RDPUDP_SYNDATA_PAYLOAD */
	Stream_Write_UINT32_BE(s, udp->initialSequence);
	Stream_Write_UINT16_BE(s, RDPUDP_MTU); /* uUpStreamMtu */
	Stream_Write_UINT16_BE(s, RDPUDP_MTU); /* uDownStreamMtu */

	/* RDPUDP_SYNDATAEX_PAYLOAD */
	Stream_Write_UINT16_BE(s, RDPUDP_VERSION_INFO_VALID);
	Stream_Write_UINT16_BE(s, version);
	if (version == RDPUDP_PROTOCOL_VERSION_3)
		Stream_Write(s, udp->cookieHash, sizeof(udp->cookieHash));

	udp->synSent = now_ms();
	udp->synAttempts++;
	WLog_Print(udp->log, WLOG_DEBUG, "sending SYN #%" PRIu32 " offering version 0x%04" PRIx16,
	           udp->synAttempts, version);
	return rdpudp_send_raw(udp, syn, sizeof(syn));
}

WINPR_ATTR_NODISCARD
static BOOL process_syn_ack(rdpUdp* udp, const BYTE* data, size_t length)
{
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data);
	wStream* s = Stream_StaticConstInit(&sbuffer, data, length);

	/* RDPUDP_FEC_HEADER and RDPUDP_SYNDATA_PAYLOAD */
	if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 16))
		return TRUE;
	const UINT32 snSourceAck = Stream_Get_UINT32_BE(s);
	const UINT16 window = Stream_Get_UINT16_BE(s);
	const UINT16 flags = Stream_Get_UINT16_BE(s);
	if ((flags & (RDPUDP_FLAG_SYN | RDPUDP_FLAG_ACK)) != (RDPUDP_FLAG_SYN | RDPUDP_FLAG_ACK))
		return TRUE;
	/* The server's answer acknowledges the random snInitialSequenceNumber of our SYN. One that
	 * does not was not sent in answer to it and is dropped, so a third party that spoofs the
	 * server's address has to guess that number to set up the connection. */
	if (snSourceAck != udp->initialSequence)
	{
		WLog_Print(udp->log, WLOG_WARN,
		           "dropping a SYN+ACK that acknowledges 0x%08" PRIx32
		           ", the SYN carried 0x%08" PRIx32,
		           snSourceAck, udp->initialSequence);
		return TRUE;
	}

	const UINT32 peerInitial = Stream_Get_UINT32_BE(s); /* snInitialSequenceNumber */
	UINT16 upMtu = Stream_Get_UINT16_BE(s);             /* uUpStreamMtu */
	Stream_Seek_UINT16(s);                              /* uDownStreamMtu */

	if ((flags & RDPUDP_FLAG_CORRELATION_ID) && !Stream_SafeSeek(s, 32))
		return TRUE;

	/* RDPUDP_SYNDATAEX_PAYLOAD, without it the peer only speaks version 1 */
	UINT16 version = RDPUDP_PROTOCOL_VERSION_1;
	if ((flags & RDPUDP_FLAG_SYNEX) && (Stream_GetRemainingLength(s) >= 4))
	{
		const UINT16 synExFlags = Stream_Get_UINT16_BE(s);
		const UINT16 udpVersion = Stream_Get_UINT16_BE(s);
		if (synExFlags & RDPUDP_VERSION_INFO_VALID)
			version = udpVersion;
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
	udp->recvNext = seq32_add(peerInitial, 1);
	udp->recvHighest = peerInitial;
	udp->ackVectorStart = seq32_add(peerInitial, 1);
	udp->lastDelivery = now_ms();
	udp->state = RDPUDP_STATE_ESTABLISHED;

	if (version == RDPUDP_PROTOCOL_VERSION_3)
	{
		/* The 32 bit sequence numbers of the SYN exchange only exist for versions 1 and 2. The
		 * peer's RDP-UDP2 sequence space is learned from its first DATA packet. */
		udp->nextCoded = RDPUDP2_INITIAL_SEQUENCE;
		udp->nextChannelSeq = 1;
		/* The peer's stream starts at channel sequence 1 too. Taking the start from whichever
		 * chunk arrives first would throw away an earlier one that it overtook. */
		udp->v3NextChannel = 1;
		udp->v3MaxDelayedAcks = RDPUDP2_DEFAULT_DELAYED_ACKS;
	}
	else
	{
		udp->nextSource = seq32_add(udp->initialSequence, 1);
		udp->nextCoded = seq32_add(udp->initialSequence, 1);
		/* the client completes the three way handshake by acknowledging the SYN+ACK */
		udp->ackPending = TRUE;
	}

	WLog_Print(udp->log, WLOG_INFO,
	           "RDP-UDP connected, protocol version 0x%04" PRIx16 ", MTU %" PRIu16, version, upMtu);
	return flush_send_queue(udp);
}

WINPR_ATTR_NODISCARD
static BOOL process_datagram(rdpUdp* udp, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data);
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

WINPR_ATTR_NODISCARD
static BOOL run_timers(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
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

	const BOOL v3 = udp->version == RDPUDP_PROTOCOL_VERSION_3;
	if (now - udp->lastReceived > (v3 ? RDPUDP2_PEER_TIMEOUT_MS : RDPUDP_PEER_TIMEOUT_MS))
	{
		rdpudp_fail(udp, v3 ? "nothing heard from the server for 16 seconds"
		                    : "nothing heard from the server for 65 seconds");
		return FALSE;
	}

	/* the grace period at channel sequence 0 can end without a packet coming in */
	if ((udp->version == RDPUDP_PROTOCOL_VERSION_3) && (udp->v3NextChannel == 0) &&
	    !v3_deliver_ready(udp))
		return FALSE;

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
		if (p->retransmits >= (v3 ? RDPUDP2_MAX_RETRANSMITS : RDPUDP_MAX_RETRANSMITS))
		{
			rdpudp_fail(udp, "a datagram went unacknowledged too many times");
			return FALSE;
		}
		p->retransmits++;
		p->rto = (p->rto * 2 > RDPUDP_MAX_RTO_MS) ? RDPUDP_MAX_RTO_MS : p->rto * 2;
		p->due = now + p->rto;
		if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		{
			/* [MS-RDPEUDP2] 3.1.5.1.3: a lost packet is resent under a new data sequence
			 * number, the channel sequence number stays. The AckOfAcks then moves past the
			 * old one, so the peer stops waiting for it. */
			const UINT16 coded = WINPR_ASSERTING_INT_CAST(UINT16, udp->nextCoded);
			p->seq = coded;
			udp->nextCoded = seq16_add(coded, 1);
		}
		else
		{
			udp->cwrPending = TRUE;
		}
		WLog_Print(udp->log, WLOG_TRACE, "retransmitting 0x%08" PRIx32 " (#%" PRIu32 ")", p->seq,
		           p->retransmits);
		if (!rdpudp_transmit(udp, p))
			return FALSE;
	}

	if (now - udp->lastSent >= (v3 ? RDPUDP2_KEEPALIVE_MS : RDPUDP_KEEPALIVE_MS))
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
	if (!obj)
		goto fail;
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
	if (udp->sendQueue)
	{
		/* Queue_Free() misses the chunks of a queue that is exactly full, so they are taken out
		 * first */
		rdpudp_chunk* chunk = nullptr;
		while ((chunk = Queue_Dequeue(udp->sendQueue)))
			chunk_free(chunk);
		Queue_Free(udp->sendQueue);
	}
	free(udp);
}

WINPR_ATTR_NODISCARD
static BOOL set_non_blocking(SOCKET sockfd)
{
	u_long arg = 1;
	return _ioctlsocket(sockfd, FIONBIO, &arg) == 0;
}

BOOL rdpudp_connect(rdpUdp* udp, const struct sockaddr* addr, size_t addrlen)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(addr);

	if (udp->state != RDPUDP_STATE_CLOSED)
		return FALSE;

	udp->sockfd = _socket(addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (udp->sockfd == INVALID_SOCKET)
	{
		WLog_Print(udp->log, WLOG_ERROR, "failed to create the UDP socket");
		return FALSE;
	}

	/* The system can grant less than asked, Linux caps it at net.core.rmem_max. A smaller buffer
	 * only costs resends, so a failure is not fatal. */
	const UINT32 optval = RDPUDP_RECEIVE_BUFFER;
	if (_setsockopt(udp->sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&optval,
	                WINPR_ASSERTING_INT_CAST(int, sizeof(optval))) != 0)
		WLog_Print(udp->log, WLOG_WARN, "setsockopt() SOL_SOCKET, SO_RCVBUF failed");

	if (_connect(udp->sockfd, addr, WINPR_ASSERTING_INT_CAST(int, addrlen)) != 0)
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
	/* the MTU is clamped to [RDPUDP_MIN_MTU, RDPUDP_MTU] when the connection is set up */
	WINPR_ASSERT(udp->mtu >= RDPUDP_MIN_MTU);
	/* v3: prefix, header, ACK, DelayAckInfo, AckOfAcks, DataHeader and channel sequence.
	 * v1/v2: header, room for the ACK vector and AckOfAcks, source payload header. */
	const size_t overhead =
	    (udp->version == RDPUDP_PROTOCOL_VERSION_3) ? 24u : (8u + RDPUDP_ACK_ROOM + 8u);
	if (udp->mtu <= overhead)
		return 0;
	return udp->mtu - overhead;
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
		return WINPR_ASSERTING_INT_CAST(UINT32, timeout);
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
	return WINPR_ASSERTING_INT_CAST(UINT32, timeout);
}

BOOL rdpudp_check(rdpUdp* udp)
{
	WINPR_ASSERT(udp);

	if ((udp->state == RDPUDP_STATE_FAILED) || (udp->state == RDPUDP_STATE_CLOSED))
		return FALSE;

	BYTE* buffer = udp->recvBuffer;
	while (TRUE)
	{
		const int rc = _recv(udp->sockfd, (char*)buffer,
		                     WINPR_ASSERTING_INT_CAST(int, sizeof(udp->recvBuffer)), 0);
		if (rc < 0)
		{
			const int error = WSAGetLastError();
			if (!rdpudp_socket_error_is_transient(error))
			{
				WLog_Print(udp->log, WLOG_ERROR, "recv failed with %d", error);
				rdpudp_fail(udp, "receiving from the socket failed");
				return FALSE;
			}
			/* Nothing left to read, or an ICMP error for an earlier datagram. Anything still
			 * queued keeps the socket readable and is read on the next round. */
			if (error != WSAEWOULDBLOCK)
				WLog_Print(udp->log, WLOG_DEBUG, "recv failed with %d", error);
			break;
		}

		if (!process_datagram(udp, buffer, WINPR_ASSERTING_INT_CAST(size_t, rc)))
		{
			rdpudp_fail(udp, "failed to process a datagram");
			return FALSE;
		}
		if (udp->state == RDPUDP_STATE_FAILED)
			return FALSE;
		if (!v3_ack_if_due(udp))
			return FALSE;
	}

	/* Acknowledge everything that arrived in one go, unless a data packet carried it already. */
	if (udp->ackPending && !rdpudp_send_ack(udp))
		return FALSE;

	if (!flush_send_queue(udp))
		return FALSE;

	return run_timers(udp);
}
