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

/* Out of order data held while waiting for a gap to fill, a slot per sequence number. RDP-UDP2
 * numbers are 16 bit, serial arithmetic tells at most 32767 ahead from behind, and the peer may
 * get that far: Windows ignores the uReceiveWindowSize of versions 1 and 2 there, and had 5751
 * chunks outstanding after a 2 second outage. */
#define RDPUDP_RECEIVE_SLOTS 32768
#define RDPUDP_RECEIVE_REACH 32767

/* Max chunk size a server accepts, capping client slot usage at 64 MB. MTU is not used
 * as the limit because ACKs are appended to full chunks, and Windows/mstsc datagrams
 * can exceed MTU by 28 bytes. */
#define RDPUDP_MAX_CHUNK 2048

/* Server: number of chunks buffered past a gap until rdpudp_set_peer_verified(). This
 * bounds the number of chunks a sender who knows the cookie hash but is not yet
 * proven can make the server buffer. Since the client only sends the TLS handshake
 * and Tunnel Create Request during this phase, a small limit suffices; out-of-reach
 * chunks are not acked and will be retransmitted. */
#define RDPUDP_UNVERIFIED_REACH 128

/* Max datagrams in flight: caps the server's congestion window and defines the fixed
 * client limit. 4096 datagrams are ~5 MB per round trip, ~50 MB/s at 100 ms. */
#define RDPUDP_MAX_IN_FLIGHT 4096
#define RDPUDP_CLIENT_MAX_IN_FLIGHT 64

/* Datagrams in flight are found by sequence number through an index with one entry per
 * 16-bit sequence number. RDP-UDP2 numbers are 16 bit, so a resend under a new number
 * cannot land on an entry still in use. */
#define RDPUDP_PENDING_INDEX 65536

/* Server only: RDP-UDP2 congestion control ([MS-RDPEUDP] 3.1.1.8), RTT-based retransmit
 * timeout, and fast retransmit on reported loss.
 *
 * The window (in datagrams) grows by one per ack during slow start, then by one per
 * RDPUDP2_GROWTH_ACKS acks (~12% per RTT) if the queue is not building. It shrinks to
 * 70% once per RTT if congested: either the round's loss is at least
 * RDPUDP2_CONGESTED_LOSS_PERCENT and at least RDPUDP2_LOSS_RISE_PERCENT points above the
 * smoothed loss of earlier rounds, or smoothed RTT exceeds the lowest sample of the last
 * RDPUDP2_RTT_MIN_WINDOW_MS by more than the maximum of RDPUDP2_QUEUE_DELAY_MS, half the
 * minimum RTT, or 4 * rttvar.
 *
 * Random loss is repaired without slowing down. A Windows 11 server maintained ~4 MB/s
 * under 2% loss and 100 ms delay, whereas halving the window on every loss dropped
 * throughput to 0.12 MB/s. Comparing each round with the smoothed loss keeps a steady
 * background loss (for example 10% on a bad radio link) from shrinking the window by
 * chance in every other round, while loss that grows with the sending rate, as at a
 * policer or a full queue, still does.
 *
 * Lost datagrams are retransmitted when an ACK vector reports them missing (provided
 * enough subsequent datagrams were received) or when the retransmit timer expires.
 * The reorder threshold starts at RDPUDP2_REORDER_THRESHOLD and doubles up to
 * RDPUDP2_MAX_REORDER_THRESHOLD if the original datagram arrives late ([MS-RDPEUDP2]
 * 3.1.1.2.3). It decays by one every RDPUDP2_REORDER_DECAY_ROUNDS rounds without such
 * events. Spurious losses are excluded from congestion calculations. */
#define RDPUDP2_INITIAL_CWND 32
#define RDPUDP2_MIN_CWND 8
#define RDPUDP2_REORDER_THRESHOLD 3
#define RDPUDP2_MAX_REORDER_THRESHOLD 128
#define RDPUDP2_REORDER_DECAY_ROUNDS 8
/* Retransmitted data sequence numbers, used to detect late-arriving originals. */
#define RDPUDP2_RESENT_HISTORY 256
/* [RFC6298] 5.5: after a timeout, new datagrams use the doubled timeout until an RTT
 * sample arrives, backing off by at most 2^RDPUDP2_MAX_RTO_BACKOFF. This ensures RTT
 * growth past the timeout is measured, as Karn's rule discards samples from
 * retransmitted datagrams. */
#define RDPUDP2_MAX_RTO_BACKOFF 6
#define RDPUDP2_GROWTH_ACKS 8
#define RDPUDP2_CONGESTED_LOSS_PERCENT 10
#define RDPUDP2_LOSS_RISE_PERCENT 5
/* Weight of a round in the smoothed loss, 1/2^RDPUDP2_LOSS_AVG_SHIFT */
#define RDPUDP2_LOSS_AVG_SHIFT 3
#define RDPUDP2_ROUND_MIN_SAMPLES 16
#define RDPUDP2_QUEUE_DELAY_MS 30
#define RDPUDP2_RTT_MIN_WINDOW_MS 10000

/* Max distance the newest data sequence number can lead the oldest unacked one.
 * Data received behind a gap is buffered until the gap closes. With mstsc (100 ms delay,
 * 1% loss), spans of 1024+ sustained full rate, while a span of 384 limited throughput
 * to <4.7 MB/s. mstsc accepts new data up to a span of 6142. */
#define RDPUDP2_MAX_SPAN RDPUDP_MAX_IN_FLIGHT

/* [MS-RDPEUDP2] LogWindowSize. Windows always advertises the largest window (2^15 * MTU), a
 * smaller value throttles the server's sender for no benefit. */
#define RDPUDP2_LOG_WINDOW_SIZE 15

/* Both Windows endpoints start the RDP-UDP2 data sequence space at 100. */
#define RDPUDP2_INITIAL_SEQUENCE 100

/* [MS-RDPEUDP2] 3.1.5.2: a receiver acknowledges at least every 8 data packets until the peer's
 * DelayAckInfo says otherwise, which allows at most 15. */
#define RDPUDP2_DEFAULT_DELAYED_ACKS 8
#define RDPUDP2_MAX_DELAYED_ACKS 15

/* What an RDP-UDP2 DATA datagram carries besides its payload: prefix, header, an ACK without
 * delayed acknowledgements, DelayAckInfo, AckOfAcks, DataHeader and channel sequence number. */
#define RDPUDP2_DATA_OVERHEAD 19

/* Room kept in a full DATA datagram for the arrival gaps of the ACK it carries. */
#define RDPUDP2_DATA_ACK_ROOM 5

/* Asked for as the socket receive buffer. A redraw arrives as a burst of hundreds of datagrams,
 * more than the default buffer holds, and a datagram that does not fit waits for a resend. */
#define RDPUDP_RECEIVE_BUFFER (2 * 1024 * 1024)

/* RDPUDP2 ACK vectors hold at most 127 state map bytes of 7 packets each. */
#define RDPUDP2_MAX_ACKVEC 127

/* Room left in every version 1/2 data datagram for the ACK vector and AckOfAcks. */
#define RDPUDP_ACK_ROOM 64

/* SYN and SYN+ACK are padded to the MTU ([MS-RDPEUDP] 3.1.5.1.1), so a line that drops large
 * datagrams drops them first: with 4 attempts the tunnel was not set up on a line losing 43% of
 * them. Ten, a second apart, stay well inside the setup time of the tunnel
 * (RDPEMT_SETUP_TIMEOUT_MS). */
#define RDPUDP_SYN_TIMEOUT_MS 1000
#define RDPUDP_SYN_ATTEMPTS 3

/* [MS-RDPEUDP] 3.1.6.1: the retransmit timeout is twice the round trip, at least the version's
 * minimum, and doubles with every resend of the same datagram. Windows lets it grow to 120
 * seconds (note <8>). Below the round trip, every datagram is resent before its acknowledgement
 * can arrive. */
#define RDPUDP_MAX_RTO_MS 120000
/* RDP-UDP2: cap on the backed-off timeout, as in Windows, which clamps it to 2 seconds. A
 * receiver ends the stream once a chunk has been missing for RDPUDP_STALL_TIMEOUT_MS; with the
 * timeout doubling to RDPUDP_MAX_RTO_MS, a datagram lost a few times in a row would be resent
 * too late. Dead connections are handled by the peer timeout. See rto_v3_cap for RTT-based cap
 * increases. */
#define RDPUDP2_MAX_RTO_MS 2000
/* Server: upper bound on the initial RTT sample from the handshake. */
#define RDPUDP2_HANDSHAKE_RTT_MAX_MS 500

/* [MS-RDPEUDP] 3.1.5.4: a datagram retransmitted three to five times without a response ends
 * the connection, and so does 65 seconds without any datagram from the peer (3.1.1.9). */
#define RDPUDP_MAX_RETRANSMITS 5
#define RDPUDP_KEEPALIVE_MS 5000
#define RDPUDP_PEER_TIMEOUT_MS 65000

/* [MS-RDPEUDP2] 3.1.1.3: both ends send at least every 16 seconds, Windows every 4, and 16
 * seconds without a datagram mean the peer is gone. The retransmit limit is left to the sender,
 * the peer timeout already bounds how long a dead connection goes unnoticed. */
#define RDPUDP2_MAX_RETRANSMITS 10

/* Reported loss retransmits don't count toward the limit to prevent ACK vector-induced
 * flooding. Chunks are retransmitted for reports at most once per RTT since last send,
 * capped at cwnd/4 per RTT (min RDPUDP2_REPORT_MIN_BUDGET). Both use smoothed RTT,
 * minimum RDPUDP2_REPORT_MIN_ROUND_MS. */
#define RDPUDP2_REPORT_MIN_ROUND_MS 20
#define RDPUDP2_REPORT_MIN_BUDGET 16

#define RDPUDP2_KEEPALIVE_MS 4000
#define RDPUDP2_PEER_TIMEOUT_MS 16000

/* Silence after which the peer may have moved more than half the sequence space on, see
 * v3_resync_base. Two keepalive intervals, an idle tunnel never gets there. */
#define RDPUDP2_RESYNC_MS (2ull * RDPUDP2_KEEPALIVE_MS)

/* Server: how far behind its window an AckOfAcks after a silence leaves the window where it is,
 * see v3_resync_base: Windows resends at most 8190 past its oldest unacknowledged datagram, plus
 * as much again for datagrams still under way. A real move landing there takes 49152 numbers
 * within the peer timeout, Windows moves at most ~2500 a second. */
#define RDPUDP2_STALE_AOA_REACH 16384

/* Server: the newest data sequence number stays this close to the oldest one in flight, resends
 * included. [MS-RDPEUDP2] 3.1.1.1.3 limits the active range to 32767, past half the space serial
 * arithmetic takes the AckOfAcks for a number ahead. */
#define RDPUDP2_MAX_RESEND_SPAN (RDPUDP_RECEIVE_REACH - 1)

/* [MS-RDPEUDP2] 3.1.5.1.3: the peer resends a lost chunk under a new data sequence number, and
 * the receiver has no way to ask for it. A stream stuck behind a missing chunk for as long as the
 * peer timeout is no better than a peer that is gone: the tunnel fails, ending the connection like
 * a broken transport, instead of leaving the session frozen while the peer's keepalives keep it
 * alive. */
#define RDPUDP_STALL_TIMEOUT_MS RDPUDP2_PEER_TIMEOUT_MS

typedef struct
{
	UINT32 seq; /* source sequence (v1/v2) or data sequence (v3) */
	UINT16 channelSeq;
	BYTE* payload;
	size_t length;
	UINT64 sent;
	UINT64 lastSent; /* Server: timestamp of the most recent transmission; 'sent' stores the initial
	                    transmission time. */
	UINT64 due;
	UINT32 rto;
	UINT32 retransmits;
	UINT32 timeouts; /* Retransmissions due to timeout; only these count toward the limit. */
	BOOL lost;       /* Reported missing by an ACK vector; do not wait for the due timer. */
	/* server: reported missing within one RTT of sending. run_timers processes the
	 * report after the RTT expires, as the peer may have further reports. */
	BOOL reportHeld;
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
	BOOL ownsSocket;     /* FALSE for server connections, which do not own the socket */
	rdpUdpClockFn clock; /* If nullptr, the system clock is used; see rdpudp_set_clock(). */
	void* clockCustom;
	UINT16 chunkReach; /* Maximum channel sequence distance ahead of the expected sequence that is
	                      accepted */
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
	UINT32 rttvar;     /* RTT variation in milliseconds, as defined in RFC 6298 */
	BOOL rttSampled;   /* True if srtt and rttvar have been initialized with at least one sample */
	UINT64 synAckSent; /* server: timestamp of the SYN+ACK transmission, until the first datagram;
	                      otherwise 0 */
	BOOL synAckResent; /* server: the SYN+ACK was retransmitted */

	/* sending side */
	rdpudp_pending pending[RDPUDP_MAX_IN_FLIGHT]; /* the first inFlight entries are in use */
	size_t inFlight;
	/* position in pending + 1 by the low 16 bits of the sequence number, 0 when free */
	UINT16 pendingIndex[RDPUDP_PENDING_INDEX];
	UINT32 sendBase;     /* lowest sequence number in flight, while inFlight > 0 */
	UINT64 pendingDue;   /* run_timers looks at the datagrams in flight from this time on */
	UINT32 cwnd;         /* Congestion window for RDP-UDP2, measured in datagrams */
	UINT32 ssthresh;     /* The slow start threshold. */
	UINT32 ccAcked;      /* Number of ACKs counted toward the next congestion window increase */
	UINT64 ccRoundStart; /* Start time of the round trip being tracked for congestion control; see
	                        cc_round */
	UINT32 ccRoundAcked;
	UINT32 ccRoundLost;
	UINT32 ccLossAvg;        /* Smoothed loss of the rounds with enough samples, in 1/1000 */
	UINT32 reorderThreshold; /* Sequence distance beyond which a missing datagram is
	                          * considered lost */
	UINT32 reorderCalmRounds;
	UINT32 rtoBackoff; /* Number of times the timeout has doubled since the last RTT sample */
	struct
	{
		BOOL valid;
		BOOL reported; /* Retransmitted due to an ACK vector report rather than a timeout */
		UINT16 seq;
	} resent[RDPUDP2_RESENT_HISTORY]; /* Data sequence numbers that have been superseded by a
	                                     retransmission */
	size_t resentNext;
	UINT32 rttMin; /* Lowest RTT sample from the previous and current windows; UINT32_MAX if none
	                  was recorded */
	UINT32 rttMinNext; /* lowest RTT sample for the window currently being collected */
	UINT64 rttMinStart;
	/* RTT for the reported-loss budget (see report_allowed). Unlike ccRoundStart,
	 * this is purely time-based and ignores peer traffic. */
	UINT64 reportRoundStart;
	UINT32 reportRoundUsed;
	UINT32 peerWindow;
	wQueue* sendQueue;
	UINT32 initialSequence;
	UINT32 nextSource;
	UINT32 nextCoded;
	BOOL cwrPending;
	UINT32 announcedAoA;
	BOOL aoaAnnounced;
	BOOL peerAcked; /* TRUE if the peer has acknowledged at least one data datagram */
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
	BOOL v3Resync; /* silent for RDPUDP2_RESYNC_MS: no ACK until an AckOfAcks sets the base */
	UINT16 v3Expected;
	UINT16 v3Highest;
	BOOL v3Received[RDPUDP_RECEIVE_SLOTS];
	UINT64 v3Arrival[RDPUDP_RECEIVE_SLOTS]; /* when each data sequence arrived, in ns */
	UINT64 v3InOrderArrival;                /* when data sequence v3Expected - 1 arrived */
	BOOL v3InOrderReceived; /* FALSE while v3Expected - 1 is one AckOfAcks gave up on */
	UINT64 v3LatestArrival; /* when the latest data packet arrived */
	size_t v3MaxDelayedAcks;
	UINT32 v3DelayedAckTimeout; /* in ms, UINT32_MAX until the peer's DelayAckInfo sets it */
	UINT64 v3AckDue; /* in ms, when what waits for our acknowledgement must get it; 0: nothing */
	size_t v3UnackedPackets; /* data packets received since our last acknowledgement */
	UINT16 v3AckedSeq;       /* the SeqNum of our last ACK payload */
	BOOL v3AckedGap;         /* whether our last acknowledgement reported a missing packet */
	BOOL v3WindowPending;    /* the window has news for an acknowledgement */
	/* data packets behind the window, acknowledged on their own, see v3_send_ack: a run of
	 * consecutive sequence numbers from v3BehindBase, and when the newest of them arrived */
	UINT16 v3BehindBase;
	size_t v3BehindCount;
	UINT64 v3BehindArrival;
	UINT16 v3NextChannel;

	/* Server: send functions and the SYN+ACK used for repeated SYN requests. */
	BOOL server;
	rdpUdpSendFn sendFn;
	void* sendCustom;
	BYTE synAck[RDPUDP_MTU];
	size_t synAckLength;

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
 * same, both sending and receiving. */
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
static UINT64 now_ns(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (udp->clock)
		return udp->clock(udp->clockCustom);
	return winpr_GetTickCount64NS();
}

WINPR_ATTR_NODISCARD
static UINT64 now_ms(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (udp->clock)
		return udp->clock(udp->clockCustom) / 1000000ULL;
	return GetTickCount64();
}

/* [MS-RDPEUDP] 3.1.6.1 specifies 500ms for v1 and 300ms for v2. [MS-RDPEUDP2] 3.1.1.2.3
 * requires a loss timeout exceeding the delayed ACK time (20ms, sent in DelayAckInfo).
 * Servers typically have an RTT sample from the handshake; otherwise (e.g. SYN+ACK
 * resent), 300ms is used to prevent premature retransmissions of TLS flights on
 * long-RTT paths. After the first sample, rto_initial tracks the samples. */
WINPR_ATTR_NODISCARD
static UINT32 rto_floor(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (udp->server)
		return udp->rttSampled ? 100 : 300;
	return (udp->version == RDPUDP_PROTOCOL_VERSION_1) ? 500 : 300;
}

/* Server RTO: srtt + 4 * rttvar ([RFC6298] 2.3), with a minimum variation term.
 * On LANs, mstsc/Windows retransmit after roughly RTT + 80-90ms, with the margin
 * increasing with RTT. This floor also accounts for receivers that delay ACKs
 * (mstsc up to ~70ms). */
#define RDPUDP2_RTO_MIN_VARIATION_MS 80

WINPR_ATTR_NODISCARD
static UINT64 rto_base(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	UINT64 rto = 0;
	if (udp->server && udp->rttSampled)
	{
		const UINT64 variation = 4ull * udp->rttvar;
		const UINT64 margin =
		    (variation > RDPUDP2_RTO_MIN_VARIATION_MS) ? variation : RDPUDP2_RTO_MIN_VARIATION_MS;
		rto = udp->srtt + margin;
	}
	else
	{
		const UINT32 floor = rto_floor(udp);
		const UINT64 rtt2 = (UINT64)udp->srtt * 2ull;
		rto = (rtt2 > floor) ? rtt2 : floor;
	}
	return rto;
}

/* RDP-UDP2 backoff cap. If RTT exceeds RDPUDP2_MAX_RTO_MS, the cap is raised
 * to ensure datagrams aren't retransmitted before their ACKs can arrive. */
WINPR_ATTR_NODISCARD
static UINT64 rto_v3_cap(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	const UINT64 base = rto_base(udp);
	return (base > RDPUDP2_MAX_RTO_MS) ? base : RDPUDP2_MAX_RTO_MS;
}

WINPR_ATTR_NODISCARD
static UINT32 rto_initial(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	UINT64 rto = rto_base(udp);
	if (udp->server)
	{
		/* srtt and margin are < 2^35; shift will not overflow 64 bits */
		WINPR_ASSERT(udp->rtoBackoff <= RDPUDP2_MAX_RTO_BACKOFF);
		rto <<= udp->rtoBackoff;
		const UINT64 cap = rto_v3_cap(udp);
		if (rto > cap)
			rto = cap;
	}
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
BOOL rdpudp_socket_error_is_transient(int error)
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
	if (udp->sendFn)
	{
		if (!udp->sendFn(udp->sendCustom, data, length))
		{
			rdpudp_fail(udp, "sending failed");
			return FALSE;
		}
		udp->lastSent = now_ms(udp);
		return TRUE;
	}

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
	udp->lastSent = now_ms(udp);
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
 * both, a send time in receivedTS would make the time spent here look like network delay.
 *
 * The packets acknowledged along with SeqNum since our previous ACK follow as delayed
 * acknowledgements, at most room of them (3.1.5.6): the gaps between their arrivals, newest
 * first. The peer estimates the bandwidth of the path from these, mstsc sends them. */
static void v3_write_ack(rdpUdp* udp, wStream* s, size_t room)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(s);
	const UINT64 now = now_ns(udp);
	/* A sequence number the peer gave up on never arrived. An older packet's arrival time would
	 * place its receipt before it was sent, so it is reported as received now. */
	const UINT64 arrival = udp->v3InOrderReceived ? udp->v3InOrderArrival : now;
	const UINT16 seq = seq16_diff(udp->v3Expected, 1);

	UINT64 gaps[RDPUDP2_MAX_DELAYED_ACKS] = WINPR_C_ARRAY_INIT; /* microseconds, newest first */
	size_t count = 0;
	if (udp->v3InOrderReceived)
	{
		size_t limit = seq16_diff(seq, udp->v3AckedSeq); /* packets acknowledged, SeqNum included */
		limit = (limit > 0) ? limit - 1 : 0;
		if (limit > udp->v3MaxDelayedAcks)
			limit = udp->v3MaxDelayedAcks;
		if (limit > room)
			limit = room;
		if (limit > RDPUDP2_MAX_DELAYED_ACKS)
			limit = RDPUDP2_MAX_DELAYED_ACKS;

		UINT16 current = seq;
		UINT64 currentArrival = arrival;
		while (count < limit)
		{
			const UINT16 previous = seq16_diff(current, 1);
			const UINT64 previousArrival = udp->v3Arrival[previous % RDPUDP_RECEIVE_SLOTS];
			if (previousArrival == 0)
				break; /* given up on by AckOfAcks, it never arrived */
			/* packets that overtook each other count as arriving together */
			gaps[count++] = (currentArrival > previousArrival)
			                    ? (currentArrival - previousArrival) / 1000ULL
			                    : 0;
			current = previous;
			currentArrival = previousArrival;
		}
	}

	/* each gap has to fit a byte in units of (1 << scale) microseconds */
	BYTE scale = 0;
	for (size_t x = 0; x < count; x++)
	{
		while ((scale < 15) && ((gaps[x] >> scale) > 255))
			scale++;
	}

	Stream_Write_UINT16(s, seq);                   /* SeqNum */
	stream_write_uint24(s, v3_timestamp(arrival)); /* receivedTS */
	/* the Stream_Write macros evaluate their value more than once */
	const BYTE gap = v3_ack_gap(arrival, now);
	Stream_Write_UINT8(s, gap); /* sendAckTimeGapInMs */
	const BYTE delayed = WINPR_ASSERTING_INT_CAST(BYTE, count | ((size_t)scale << 4));
	Stream_Write_UINT8(s, delayed); /* numDelayedAcks, delayAckTimeScale */
	for (size_t x = 0; x < count; x++)
	{
		const UINT64 scaled = gaps[x] >> scale;
		const BYTE addition = (scaled > 255) ? 255 : WINPR_ASSERTING_INT_CAST(BYTE, scaled);
		Stream_Write_UINT8(s, addition); /* delayAckTimeAdditions */
	}
	udp->v3AckedSeq = seq;
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

/* [MS-RDPEUDP2] 2.2.1.2.6 ACK vector payload. TimeStamp is when the newest packet it reports
 * arrived, SendAckTimeGapInMs the time since the latest data packet arrived. */
WINPR_ATTR_NODISCARD
static BOOL v3_send_ack_vector(rdpUdp* udp, UINT16 base, const BYTE* vec, size_t count,
                               UINT64 arrival)
{
	BYTE buffer[64 + RDPUDP2_MAX_ACKVEC] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(udp);
	WINPR_ASSERT(vec);
	WINPR_ASSERT((count > 0) && (count <= RDPUDP2_MAX_ACKVEC));
	wStream* s =
	    v3_packet_init(&sbuffer, buffer, sizeof(buffer), RDPUDP2_OVERHEADSIZE | RDPUDP2_ACKVEC);
	if (!s)
		return FALSE;
	const UINT64 now = now_ns(udp);
	const BYTE control = WINPR_ASSERTING_INT_CAST(BYTE, 0x80 | count);
	/* the Stream_Write macros evaluate their value more than once */
	const BYTE ackGap = v3_ack_gap(udp->v3LatestArrival, now);
	const UINT32 timestamp = v3_timestamp((arrival != 0) ? arrival : now);
	Stream_Write_UINT8(s, 10);         /* OverheadSize */
	Stream_Write_UINT16(s, base);      /* BaseSeqNum */
	Stream_Write_UINT8(s, control);    /* TimeStampPresent, codedAckVecSize */
	stream_write_uint24(s, timestamp); /* TimeStamp */
	Stream_Write_UINT8(s, ackGap);     /* SendAckTimeGapInMs */
	Stream_Write(s, vec, count);       /* codedAckVector */
	return v3_send_packet(udp, s, RDPUDP2_PACKET_STANDARD);
}

WINPR_ATTR_NODISCARD
static BOOL v3_send_ack(rdpUdp* udp)
{
	BYTE buffer[64] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(udp);
	if (!udp->v3BaseKnown)
		return TRUE;

	/* Data packets behind our window, see v3_process_datagram, go in an ACK vector of their own:
	 * received, as runs of up to 63. A cumulative ACK would name our own position, which after a
	 * short outage at a high rate can lie ahead of what the peer has sent, and Windows ends the
	 * connection for that. It would also mark everything below it received ([MS-RDPEUDP2] 4.1.3).
	 * The vector reports what arrived and nothing more (4.2.2). */
	if (udp->v3BehindCount > 0)
	{
		BYTE vec[RDPUDP2_MAX_ACKVEC] = WINPR_C_ARRAY_INIT;
		size_t count = 0;
		for (size_t left = udp->v3BehindCount; left > 0;)
		{
			const size_t run = (left > 63) ? 63 : left;
			vec[count++] = WINPR_ASSERTING_INT_CAST(BYTE, 0x80 | 0x40 | run);
			left -= run;
		}
		udp->v3BehindCount = 0;
		if (!v3_send_ack_vector(udp, udp->v3BehindBase, vec, count, udp->v3BehindArrival))
			return FALSE;
		if (!udp->v3WindowPending)
		{
			udp->ackPending = FALSE;
			udp->v3UnackedPackets = 0;
			udp->v3AckDue = 0;
			return TRUE;
		}
	}

	/* After a silence our position may be ahead of the peer's numbers (v3_resync_base): no ACK
	 * until its next AckOfAcks. Nothing stays due meanwhile, or the tunnel thread would spin. */
	if (udp->v3Resync)
	{
		udp->ackPending = FALSE;
		udp->v3UnackedPackets = 0;
		udp->v3AckDue = 0;
		return TRUE;
	}

	udp->ackPending = FALSE;
	udp->v3WindowPending = FALSE;
	udp->v3UnackedPackets = 0;
	udp->v3AckDue = 0;

	/* A cumulative ACK cannot describe a hole, the ACK vector reports the exact window so only
	 * what is missing gets resent. ACK and ACKVEC are mutually exclusive. */
	const BOOL gap = v3_has_gap(udp);
	udp->v3AckedGap = gap;
	if (!gap)
	{
		wStream* s =
		    v3_packet_init(&sbuffer, buffer, sizeof(buffer), RDPUDP2_OVERHEADSIZE | RDPUDP2_ACK);
		if (!s)
			return FALSE;
		v3_write_ack(udp, s, RDPUDP2_MAX_DELAYED_ACKS);
		Stream_Write_UINT8(s, 10); /* OverheadSize */
		return v3_send_packet(udp, s, RDPUDP2_PACKET_STANDARD);
	}

	const UINT16 base = udp->v3Expected;
	size_t span = (size_t)seq16_diff(udp->v3Highest, base) + 1;
	if (span > RDPUDP_RECEIVE_REACH)
		span = RDPUDP_RECEIVE_REACH;

	/* [MS-RDPEUDP2] 2.2.1.2.6 ACK vector payload. Runs of seven or more packets in the same
	 * state go in run-length mode (up to 63 per byte), the rest in 7 bit state maps: with state
	 * maps only, 127 bytes cover 889 packets, ~70 ms behind a hole at 130 Mbit/s, and everything
	 * past that would stay unacknowledged until the hole is filled. */
	BYTE vec[RDPUDP2_MAX_ACKVEC] = WINPR_C_ARRAY_INIT;
	size_t count = 0;
	size_t pos = 0;
	while ((pos < span) && (count < RDPUDP2_MAX_ACKVEC))
	{
		const BOOL state = udp->v3Received[seq16_add(base, pos) % RDPUDP_RECEIVE_SLOTS];
		size_t run = 1;
		while ((pos + run < span) && (run < 63) &&
		       (udp->v3Received[seq16_add(base, pos + run) % RDPUDP_RECEIVE_SLOTS] == state))
			run++;
		if (run >= 7)
		{
			vec[count++] = WINPR_ASSERTING_INT_CAST(BYTE, 0x80 | (state ? 0x40 : 0x00) | run);
			pos += run;
			continue;
		}
		BYTE bits = 0;
		for (size_t bit = 0; (bit < 7) && (pos + bit < span); bit++)
		{
			if (udp->v3Received[seq16_add(base, pos + bit) % RDPUDP_RECEIVE_SLOTS])
				bits |= WINPR_ASSERTING_INT_CAST(BYTE, 1 << bit);
		}
		vec[count++] = bits; /* state map, bit 0 first */
		pos += 7;
	}

	/* when the highest sequence number received arrived */
	const UINT64 highest = udp->v3Arrival[udp->v3Highest % RDPUDP_RECEIVE_SLOTS];
	return v3_send_ack_vector(udp, base, vec, count, highest);
}

/* The lowest data sequence number of ours still awaiting acknowledgement, which is what
 * AckOfAcks tells the peer ([MS-RDPEUDP2] 2.2.1.2.4). */
WINPR_ATTR_NODISCARD
static UINT32 sender_base(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (udp->inFlight > 0)
		return udp->sendBase;
	return (udp->version == RDPUDP_PROTOCOL_VERSION_3) ? udp->nextCoded : udp->nextSource;
}

/* The sequence number the next new or resent datagram gets */
WINPR_ATTR_NODISCARD
static UINT32 next_send_seq(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return (udp->version == RDPUDP_PROTOCOL_VERSION_3) ? udp->nextCoded : udp->nextSource;
}

WINPR_ATTR_NODISCARD
static UINT32 send_seq_add(const rdpUdp* udp, UINT32 seq, size_t n)
{
	WINPR_ASSERT(udp);
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		return seq16_add(WINPR_ASSERTING_INT_CAST(UINT16, seq), n);
	return seq32_add(seq, WINPR_ASSERTING_INT_CAST(UINT32, n));
}

WINPR_ATTR_NODISCARD
static rdpudp_pending* pending_find(rdpUdp* udp, UINT32 seq)
{
	WINPR_ASSERT(udp);
	const UINT16 pos = udp->pendingIndex[seq & 0xFFFF];
	if (pos == 0)
		return nullptr;
	WINPR_ASSERT(pos <= udp->inFlight);
	rdpudp_pending* p = &udp->pending[pos - 1];
	return (p->seq == seq) ? p : nullptr;
}

/* Moves sendBase up to the oldest datagram still in flight. Every datagram in flight lies
 * between sendBase and the next sequence number, so this ends there at the latest. */
static void send_base_advance(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (udp->inFlight == 0)
	{
		udp->sendBase = next_send_seq(udp);
		return;
	}
	while (!pending_find(udp, udp->sendBase))
	{
		WINPR_ASSERT(udp->sendBase != next_send_seq(udp));
		udp->sendBase = send_seq_add(udp, udp->sendBase, 1);
	}
}

/* Frees a datagram in flight; the last entry moves into its place. */
static void pending_remove(rdpUdp* udp, rdpudp_pending* p)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);
	WINPR_ASSERT(udp->inFlight > 0);
	const size_t pos = WINPR_ASSERTING_INT_CAST(size_t, p - udp->pending);
	const size_t last = udp->inFlight - 1;
	WINPR_ASSERT(pos <= last);
	udp->pendingIndex[p->seq & 0xFFFF] = 0;
	pending_clear(p);
	if (pos != last)
	{
		*p = udp->pending[last];
		memset(&udp->pending[last], 0, sizeof(udp->pending[last]));
		udp->pendingIndex[p->seq & 0xFFFF] = WINPR_ASSERTING_INT_CAST(UINT16, pos + 1);
	}
	udp->inFlight = last;
	send_base_advance(udp);
}

/* Earliest time run_timers has to look at the datagrams in flight again */
static void pending_due_at(rdpUdp* udp, UINT64 at)
{
	WINPR_ASSERT(udp);
	if (at < udp->pendingDue)
		udp->pendingDue = at;
}

WINPR_ATTR_NODISCARD
static UINT64 report_resend_interval(const rdpUdp* udp);

/* The earlier of due and the time run_timers has to act on this datagram */
WINPR_ATTR_NODISCARD
static UINT64 pending_next_due(const rdpUdp* udp, const rdpudp_pending* p, UINT64 due)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);
	if (p->lost)
		return 0;
	if (p->due < due)
		due = p->due;
	if (p->reportHeld)
	{
		const UINT64 at = p->lastSent + report_resend_interval(udp);
		if (at < due)
			due = at;
	}
	return due;
}

WINPR_ATTR_NODISCARD
static BOOL v3_send_data(rdpUdp* udp, const rdpudp_pending* p)
{
	BYTE buffer[RDPUDP_MTU + 32] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);

	/* Not while packets behind the window wait for their ACK vector: the ACK here would clear
	 * ackPending, and nothing would send the vector until the next standalone ACK. Not after a
	 * silence either, our position may lie ahead of the peer's numbers then (v3_resync_base). */
	const BOOL withAck = udp->ackPending && udp->v3BaseKnown && !v3_has_gap(udp) &&
	                     (udp->v3BehindCount == 0) && !udp->v3Resync;
	const UINT16 base = WINPR_ASSERTING_INT_CAST(UINT16, sender_base(udp));
	/* Include on retransmissions in case the original announcement was lost, and in all data
	 * until the peer acknowledges ([MS-RDPEUDP2] 3.1.5.3, as Windows does). The peer determines
	 * the sequence space start from the first AckOfAcks; otherwise, it uses the first arriving
	 * datagram and assumes preceding packets were received. */
	const BOOL withAoA = !udp->aoaAnnounced || (udp->announcedAoA != base) ||
	                     (p->retransmits > 0) || !udp->peerAcked;

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
		/* arrival gaps only where the payload leaves room in the datagram */
		const size_t used = RDPUDP2_DATA_OVERHEAD + p->length;
		v3_write_ack(udp, s, (udp->mtu > used) ? udp->mtu - used : 0);
		/* left pending, a later ACK would repeat this position, which after a jump of the
		 * peer's numbers names a packet it did not send */
		udp->ackPending = FALSE;
		udp->v3WindowPending = FALSE;
		udp->v3UnackedPackets = 0;
		udp->v3AckDue = 0;
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
 * using at most room bytes. Windows encodes a run of n datagrams as n - 1 in an element, so a run
 * holds 1 to 64 datagrams; before the peer's first data packet the vector is empty. */
static void v1_write_ack_vector(const rdpUdp* udp, wStream* s, size_t room)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(s);
	if (!udp->receivedData)
	{
		Stream_Write_UINT16_BE(s, 0); /* uAckVectorSize */
		Stream_Zero(s, 2);            /* pad to a DWORD boundary */
		return;
	}

	const UINT32 highest = udp->recvHighest;
	UINT32 low = udp->ackVectorStart;
	if (seq32_after(low, highest))
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
		if (!seq32_after(udp->recvNext, seq))
		{
			const rdpudp_slot* slot = &udp->slots[seq % RDPUDP_RECEIVE_SLOTS];
			if (!slot->present || (slot->seq != seq))
				current = DATAGRAM_NOT_YET_RECEIVED;
		}

		if ((current != state) || (run == 0x40))
		{
			if (run > 0)
			{
				if (count >= ARRAYSIZE(elements))
					break;
				elements[count++] = WINPR_ASSERTING_INT_CAST(BYTE, (state << 6) | (run - 1));
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
		elements[count++] = WINPR_ASSERTING_INT_CAST(BYTE, (state << 6) | (run - 1));

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

/* [MS-RDPEUDP2] 3.1.5.2 and 4.2: a receiver acknowledges once MaxDelayedAcks + 1 data packets
 * wait, the most one ACK can describe, and at once when a packet goes missing or a missing one
 * turns up. Checked after every datagram, so a burst is acknowledged while it is still arriving
 * rather than when it ended. Fewer packets wait for ack_when_due. */
WINPR_ATTR_NODISCARD
static BOOL v3_ack_if_due(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if ((udp->version != RDPUDP_PROTOCOL_VERSION_3) || (udp->state != RDPUDP_STATE_ESTABLISHED) ||
	    !udp->ackPending)
		return TRUE;
	if ((v3_has_gap(udp) == udp->v3AckedGap) && (udp->v3UnackedPackets <= udp->v3MaxDelayedAcks))
		return TRUE;
	return v3_send_ack(udp);
}

/* [MS-RDPEUDP2] 3.1.5.2: half the round trip time until the peer's DelayAckInfo says otherwise */
WINPR_ATTR_NODISCARD
static UINT32 v3_ack_delay(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return (udp->v3DelayedAckTimeout != UINT32_MAX) ? udp->v3DelayedAckTimeout : udp->srtt / 2;
}

/* Runs at the end of every receive round. Versions 1 and 2 acknowledge everything that arrived in
 * one go, unless a data packet carried the ACK already. [MS-RDPEUDP2] 2.2.1.2.3: version 3 lets
 * an acknowledgement wait DelayedAckTimeoutInMs, so that one ACK covers what arrives meanwhile.
 * A receive round can be a single datagram, a burst has not ended with it. */
WINPR_ATTR_NODISCARD
static BOOL ack_when_due(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (!udp->ackPending)
		return TRUE;
	if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
	{
		if ((udp->state != RDPUDP_STATE_ESTABLISHED) || !udp->v3BaseKnown)
			return TRUE;
		const UINT64 now = now_ms(udp);
		if (udp->v3AckDue == 0)
			udp->v3AckDue = now + v3_ack_delay(udp);
		if (now < udp->v3AckDue)
			return TRUE;
	}
	return rdpudp_send_ack(udp);
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
	if (udp->server)
		return (udp->cwnd < RDPUDP_MAX_IN_FLIGHT) ? udp->cwnd : RDPUDP_MAX_IN_FLIGHT;
	size_t limit = RDPUDP_CLIENT_MAX_IN_FLIGHT;
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
	if ((udp->inFlight >= RDPUDP_MAX_IN_FLIGHT) ||
	    (udp->pendingIndex[next_send_seq(udp) & 0xFFFF] != 0))
	{
		free(data);
		return FALSE;
	}
	rdpudp_pending* p = &udp->pending[udp->inFlight];

	/* the pending slot owns the payload from here, pending_clear frees it */
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

	const UINT64 now = now_ms(udp);
	p->sent = now;
	if (udp->server)
		p->lastSent = now;
	p->rto = rto_initial(udp);
	p->due = now + p->rto;
	p->retransmits = 0;
	p->timeouts = 0;
	if (udp->inFlight == 0)
		udp->sendBase = p->seq;
	udp->inFlight++;
	udp->pendingIndex[p->seq & 0xFFFF] = WINPR_ASSERTING_INT_CAST(UINT16, udp->inFlight);
	pending_due_at(udp, p->due);

	return rdpudp_transmit(udp, p);
}

/* Check if another datagram can be sent without exceeding RDPUDP2_MAX_SPAN from the
 * oldest unacked packet, and without taking the index entry of one still in flight. */
WINPR_ATTR_NODISCARD
static BOOL span_has_room(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (udp->inFlight == 0)
		return TRUE;
	if (udp->pendingIndex[next_send_seq(udp) & 0xFFFF] != 0)
		return FALSE;
	if (!udp->server)
		return TRUE;
	const UINT16 base = WINPR_ASSERTING_INT_CAST(UINT16, sender_base(udp));
	return seq16_diff(WINPR_ASSERTING_INT_CAST(UINT16, udp->nextCoded), base) < RDPUDP2_MAX_SPAN;
}

WINPR_ATTR_NODISCARD
static BOOL flush_send_queue(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	while ((udp->state == RDPUDP_STATE_ESTABLISHED) && (udp->inFlight < window_limit(udp)) &&
	       (Queue_Count(udp->sendQueue) > 0) && span_has_room(udp))
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
	const UINT64 sample64 = now_ms(udp) - p->sent;
	const UINT32 sample =
	    (sample64 > UINT32_MAX) ? UINT32_MAX : WINPR_ASSERTING_INT_CAST(UINT32, sample64);
	if (!udp->server)
	{
		udp->srtt = (udp->srtt == 0)
		                ? sample
		                : WINPR_ASSERTING_INT_CAST(UINT32, ((UINT64)udp->srtt * 7 + sample) / 8);
		return;
	}
	udp->rtoBackoff = 0;
	if (sample < udp->rttMin)
		udp->rttMin = sample;
	if (sample < udp->rttMinNext)
		udp->rttMinNext = sample;
	if (now_ms(udp) - udp->rttMinStart >= RDPUDP2_RTT_MIN_WINDOW_MS)
	{
		/* Update minimum RTT to avoid comparing against stale values if the route slows down. */
		udp->rttMin = udp->rttMinNext;
		udp->rttMinNext = UINT32_MAX;
		udp->rttMinStart = now_ms(udp);
	}
	if (!udp->rttSampled)
	{
		/* Initialize SRTT and RTTVAR according to RFC 6298, Section 2.2 */
		udp->srtt = sample;
		udp->rttvar = sample / 2;
		udp->rttSampled = TRUE;
		return;
	}
	/* [RFC6298] 2.3: Update the RTT variation first, using the previous smoothed RTT. */
	const UINT32 delta = (udp->srtt > sample) ? udp->srtt - sample : sample - udp->srtt;
	udp->rttvar = WINPR_ASSERTING_INT_CAST(UINT32, ((UINT64)udp->rttvar * 3 + delta) / 4);
	udp->srtt = WINPR_ASSERTING_INT_CAST(UINT32, ((UINT64)udp->srtt * 7 + sample) / 8);
}

/* Check if smoothed RTT indicates queue buildup on the path. */
WINPR_ATTR_NODISCARD
static BOOL cc_queue_is_building(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (!udp->rttSampled || (udp->rttMin == UINT32_MAX))
		return FALSE;
	/* Use variation to widen the allowance, as jitter can cause samples to drop far below
	 * the smoothed value. */
	UINT64 allowance =
	    (udp->rttMin / 2 > RDPUDP2_QUEUE_DELAY_MS) ? udp->rttMin / 2 : RDPUDP2_QUEUE_DELAY_MS;
	if (4ull * udp->rttvar > allowance)
		allowance = 4ull * udp->rttvar;
	return udp->srtt > udp->rttMin + allowance;
}

/* End the current round after one smoothed RTT; shrink window to 70% if congestion was
 * detected. */
static void cc_round(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	const UINT64 now = now_ms(udp);
	const UINT32 rtt = (udp->srtt > 0) ? udp->srtt : rto_floor(udp);
	if (udp->ccRoundStart == 0)
		udp->ccRoundStart = now;
	if (now - udp->ccRoundStart < rtt)
		return;

	/* Use 64-bit sum to prevent overflow. */
	const UINT64 total = (UINT64)udp->ccRoundAcked + udp->ccRoundLost;
	BOOL lossy = FALSE;
	if (total >= RDPUDP2_ROUND_MIN_SAMPLES)
	{
		/* Congested only when loss is high and rose above the smoothed loss of earlier
		 * rounds: a steady background loss is repaired, not a reason to slow down. */
		const UINT32 loss = WINPR_ASSERTING_INT_CAST(UINT32, udp->ccRoundLost * 1000ull / total);
		lossy = (loss >= RDPUDP2_CONGESTED_LOSS_PERCENT * 10u) &&
		        (loss >= udp->ccLossAvg + RDPUDP2_LOSS_RISE_PERCENT * 10u);
		const INT64 delta = (INT64)loss - (INT64)udp->ccLossAvg;
		udp->ccLossAvg = WINPR_ASSERTING_INT_CAST(
		    UINT32, (INT64)udp->ccLossAvg + delta / (1 << RDPUDP2_LOSS_AVG_SHIFT));
	}
	const BOOL queued = cc_queue_is_building(udp);
	if (lossy || queued)
	{
		const UINT32 reduced = WINPR_ASSERTING_INT_CAST(UINT32, udp->cwnd * 7ull / 10);
		udp->ssthresh = (reduced > RDPUDP2_MIN_CWND) ? reduced : RDPUDP2_MIN_CWND;
		udp->cwnd = udp->ssthresh;
		udp->ccAcked = 0;
		WLog_Print(udp->log, WLOG_DEBUG,
		           "congestion (%" PRIu32 " of %" PRIu64 " lost, round trip %" PRIu32
		           " ms, lowest %" PRIu32 " ms), congestion window %" PRIu32,
		           udp->ccRoundLost, total, udp->srtt, udp->rttMin, udp->cwnd);
	}
	if ((udp->reorderThreshold > RDPUDP2_REORDER_THRESHOLD) &&
	    (++udp->reorderCalmRounds >= RDPUDP2_REORDER_DECAY_ROUNDS))
	{
		udp->reorderThreshold--;
		udp->reorderCalmRounds = 0;
	}
	udp->ccRoundStart = now;
	udp->ccRoundAcked = 0;
	udp->ccRoundLost = 0;
}

static void cc_on_ack(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	udp->ccRoundAcked++;
	if (udp->cwnd < udp->ssthresh)
		udp->cwnd++;
	else if (!cc_queue_is_building(udp) && (++udp->ccAcked >= RDPUDP2_GROWTH_ACKS))
	{
		udp->ccAcked = 0;
		udp->cwnd++;
	}
	if (udp->cwnd > RDPUDP_MAX_IN_FLIGHT)
		udp->cwnd = RDPUDP_MAX_IN_FLIGHT;
	cc_round(udp);
}

/* Record a missing or timed-out datagram for the current round. */
static void cc_on_loss(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	udp->ccRoundLost++;
	cc_round(udp);
}

/* Server: duration a chunk is exempt from reports after sending, and the duration of the
 * report-driven retransmit budget. */
WINPR_ATTR_NODISCARD
static UINT64 report_resend_interval(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return (udp->srtt > RDPUDP2_REPORT_MIN_ROUND_MS) ? udp->srtt : RDPUDP2_REPORT_MIN_ROUND_MS;
}

/* Server: mark reported chunk as lost if the round trip budget allows; otherwise, keep
 * pending for the next vector or timer. Only marking triggers cc_on_loss, ensuring loss
 * is counted once per retransmit. Held-back chunks are counted when acted upon;
 * duplicate reports are ignored. */
static void handle_reported_loss(rdpUdp* udp, rdpudp_pending* p)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);
	const UINT64 now = now_ms(udp);
	if (now - udp->reportRoundStart >= report_resend_interval(udp))
	{
		udp->reportRoundStart = now;
		udp->reportRoundUsed = 0;
	}
	const UINT32 share = udp->cwnd / 4;
	const UINT32 budget = (share > RDPUDP2_REPORT_MIN_BUDGET) ? share : RDPUDP2_REPORT_MIN_BUDGET;
	p->reportHeld = FALSE;
	if (udp->reportRoundUsed >= budget)
		return;
	udp->reportRoundUsed++;
	p->lost = TRUE;
	pending_due_at(udp, 0);
	cc_on_loss(udp);
}

static void acknowledge(rdpUdp* udp, rdpudp_pending* p)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(p);
	sample_rtt(udp, p);
	pending_remove(udp, p);
	udp->peerAcked = TRUE;
	if (udp->server)
		cc_on_ack(udp);
}

static void acknowledge_seq(rdpUdp* udp, UINT32 seq)
{
	WINPR_ASSERT(udp);
	rdpudp_pending* p = pending_find(udp, seq);
	if (p)
		acknowledge(udp, p);
}

/* ------------------------------------------------------------------------------------------ */
/* Receiving                                                                                  */
/* ------------------------------------------------------------------------------------------ */

WINPR_ATTR_NODISCARD
static BOOL deliver(rdpUdp* udp, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	udp->lastDelivery = now_ms(udp);
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
		udp->bufferingSince = now_ms(udp);
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
		total += (Stream_Get_UINT8(s) & 0x3Fu) + 1u; /* a run of n datagrams is sent as n - 1 */
	if ((total == 0) || !Stream_SetPosition(s, start))
		return;

	UINT32 seq = seq32_add(seq32_diff(snSourceAck, total), 1);
	for (size_t x = 0; x < count; x++)
	{
		const BYTE element = Stream_Get_UINT8(s);
		const BYTE state = WINPR_ASSERTING_INT_CAST(BYTE, element >> 6);
		const BYTE run = WINPR_ASSERTING_INT_CAST(BYTE, (element & 0x3F) + 1);
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

/* After a silence the peer may have moved more than half the 16 bit sequence space on: through
 * an outage it resends its window again and again under new numbers, Windows used about 45000 in
 * 25 s. Serial arithmetic then takes its numbers for old ones, and acknowledging our old position
 * names numbers it has not sent, for which Windows resets the connection. So after a silence the
 * peer's next AckOfAcks becomes the base, whichever way it moves it. */
static void v3_resync_base(rdpUdp* udp, UINT16 base)
{
	WINPR_ASSERT(udp);
	udp->v3Resync = FALSE;
	/* Server: a base a little behind ours means the peer's numbers did not wrap, our position
	 * still holds. Our acknowledgements were lost in the silence, or the datagram is older than
	 * it (a repeat, or one reordered across it). Taken, the base moves the window back below what
	 * was acknowledged already, and mstsc ends the connection on the ACK vector that follows. A
	 * real move cannot land there: the peer keeps its numbers within RDPUDP2_STALE_AOA_REACH of
	 * its oldest unacknowledged one, and the peer timeout ends the connection long before it can
	 * move almost the whole sequence space on. The window stays, and acknowledging resumes. */
	if (udp->server && seq16_after(udp->v3Expected, base) &&
	    (seq16_diff(udp->v3Expected, base) <= RDPUDP2_STALE_AOA_REACH))
	{
		udp->ackPending = TRUE;
		udp->v3WindowPending = TRUE;
		return;
	}
	if (base == udp->v3Expected)
		return;

	WLog_Print(udp->log, WLOG_DEBUG,
	           "heard from the peer again, its AckOfAcks 0x%04" PRIx16 " replaces 0x%04" PRIx16,
	           base, udp->v3Expected);
	memset(udp->v3Received, 0, sizeof(udp->v3Received));
	memset(udp->v3Arrival, 0, sizeof(udp->v3Arrival));
	udp->v3Expected = base;
	udp->v3Highest = seq16_diff(base, 1);
	udp->v3InOrderReceived = FALSE;
	udp->ackPending = TRUE;
	udp->v3WindowPending = TRUE;
}

/* Marks a silence of RDPUDP2_RESYNC_MS, see v3_resync_base. */
static void v3_note_silence(rdpUdp* udp, UINT64 now)
{
	WINPR_ASSERT(udp);
	if ((udp->version == RDPUDP_PROTOCOL_VERSION_3) && udp->v3BaseKnown &&
	    (now - udp->lastReceived >= RDPUDP2_RESYNC_MS))
		udp->v3Resync = TRUE;
}

static void v3_advance_base(rdpUdp* udp, UINT16 base)
{
	WINPR_ASSERT(udp);
	/* However far ahead: through an outage the peer resends its window again and again, each
	 * time under new sequence numbers, and Windows moved almost 5000 ahead in a 2 second outage.
	 * Only its AckOfAcks brings this side along, data that far ahead is out of reach. seq16_after
	 * limits the jump to half the sequence space, v3_resync_base goes further after a silence. */
	if (!udp->v3BaseKnown)
		return;
	if (udp->v3Resync)
	{
		v3_resync_base(udp, base);
		return;
	}
	if (!seq16_after(base, udp->v3Expected))
		return;

	/* The peer stopped resending everything below base. This only gives up transport sequence
	 * numbers: a chunk of the stream sent there comes back under a later one, with its channel
	 * sequence number unchanged. */
	if (udp->server)
	{
		/* A single client datagram can move the base by up to 32767; clear slots in one or two
		 * ranges instead of individually. */
		const size_t count = seq16_diff(base, udp->v3Expected);
		const size_t first = udp->v3Expected % RDPUDP_RECEIVE_SLOTS;
		const size_t head =
		    (count < RDPUDP_RECEIVE_SLOTS - first) ? count : RDPUDP_RECEIVE_SLOTS - first;
		memset(&udp->v3Received[first], 0, head * sizeof(udp->v3Received[0]));
		memset(&udp->v3Arrival[first], 0, head * sizeof(udp->v3Arrival[0]));
		memset(&udp->v3Received[0], 0, (count - head) * sizeof(udp->v3Received[0]));
		memset(&udp->v3Arrival[0], 0, (count - head) * sizeof(udp->v3Arrival[0]));
		udp->v3Expected = base;
	}
	while (udp->v3Expected != base)
	{
		const size_t slot = udp->v3Expected % RDPUDP_RECEIVE_SLOTS;
		udp->v3Received[slot] = FALSE;
		udp->v3Arrival[slot] = 0; /* never arrived, no arrival gap to report for it */
		udp->v3Expected++;
	}
	udp->v3InOrderReceived = FALSE;
	const UINT16 last = seq16_diff(base, 1);
	if (seq16_after(last, udp->v3Highest))
		udp->v3Highest = last;
	v3_advance_window(udp);
	udp->ackPending = TRUE;
	udp->v3WindowPending = TRUE;
}

/* The data sequence number of the last datagram this end sent. Only data packets take one, all
 * from nextCoded. */
WINPR_ATTR_NODISCARD
static UINT16 v3_last_sent(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return seq16_diff(WINPR_ASSERTING_INT_CAST(UINT16, udp->nextCoded), 1);
}

/* TRUE if seq is past the last datagram this end sent. */
WINPR_ATTR_NODISCARD
static BOOL v3_not_sent(const rdpUdp* udp, UINT16 seq)
{
	return seq16_after(seq, v3_last_sent(udp));
}

/* TRUE if seq lies from sendBase up to the next sequence number, with datagrams in flight. Exact
 * where serial arithmetic is not: everything in flight is within RDPUDP2_MAX_RESEND_SPAN. */
WINPR_ATTR_NODISCARD
static BOOL v3_from_send_base(const rdpUdp* udp, UINT16 seq)
{
	WINPR_ASSERT(udp);
	const UINT16 base = WINPR_ASSERTING_INT_CAST(UINT16, udp->sendBase);
	return seq16_diff(seq, base) <=
	       seq16_diff(WINPR_ASSERTING_INT_CAST(UINT16, udp->nextCoded), base);
}

static void v3_process_ack_vector(rdpUdp* udp, UINT16 base, wStream* s, size_t count)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(s);
	/* count is from the wire, bounded by the 7-bit codedAckVecSize; v3_process_datagram
	 * verified the available bytes. */
	WINPR_ASSERT(count <= RDPUDP2_MAX_ACKVEC);
	WINPR_ASSERT(Stream_GetRemainingLength(s) >= count);

	/* 127 entries; each covers up to 63 (run length) or 7 (state map) datagrams. */
	BYTE received[RDPUDP2_MAX_ACKVEC * 63] = WINPR_C_ARRAY_INIT;
	size_t covered = 0;
	size_t highest = 0; /* The index one past the last datagram reported as received */
	for (size_t x = 0; x < count; x++)
	{
		const BYTE entry = Stream_Get_UINT8(s);
		if (entry & 0x80)
		{
			/* run length: received flag and length */
			const BOOL got = (entry & 0x40) != 0;
			const BYTE run = entry & 0x3F;
			for (BYTE y = 0; (y < run) && (covered < ARRAYSIZE(received)); y++)
			{
				received[covered++] = got ? 1 : 0;
				if (got)
					highest = covered;
			}
		}
		else
		{
			/* state map of 7 packets, bit 0 first */
			for (BYTE bit = 0; (bit < 7) && (covered < ARRAYSIZE(received)); bit++)
			{
				const BOOL got = (entry & (1 << bit)) != 0;
				received[covered++] = got ? 1 : 0;
				if (got)
					highest = covered;
			}
		}
	}

	if (udp->server)
	{
		/* A real peer only reports datagrams this end sent, whatever loss or reordering did to
		 * them, so a base or a received entry past the last one sent is forged. */
		const UINT16 last = v3_last_sent(udp);
		const size_t sentFromBase = seq16_after(base, last) ? 0 : seq16_diff(last, base) + 1u;
		if (v3_not_sent(udp, seq16_diff(base, 1)) || (highest > sentFromBase))
		{
			WLog_Print(udp->log, WLOG_DEBUG,
			           "ignoring an ACK vector at base 0x%04" PRIx16
			           " that reports datagrams never sent (last sent 0x%04" PRIx16 ")",
			           base, last);
			return;
		}
	}

	/* All packets below the base have been received, unless the vector reports its base
	 * received: the base of an ACK vector is the receiver's first missing packet ([MS-RDPEUDP2]
	 * 3.1.5.7), one that arrived is a report on a packet behind the receiver's window, which a
	 * FreeRDP client sends (see v3_process_datagram) and which says nothing about what lies below
	 * it (4.2.2). After a silence, such a packet may only look behind a stale window. Server: not
	 * either when the base lies behind everything in flight, see v3_process_ack. */
	const BOOL baseIsMissing = (covered == 0) || !received[0];
	const BOOL baseInRange =
	    baseIsMissing && (!udp->server || (udp->inFlight == 0) || v3_from_send_base(udp, base));
	while (baseInRange && (udp->inFlight > 0) &&
	       seq16_after(base, WINPR_ASSERTING_INT_CAST(UINT16, udp->sendBase)))
		acknowledge(udp, pending_find(udp, udp->sendBase));

	const size_t sent = seq16_diff(WINPR_ASSERTING_INT_CAST(UINT16, udp->nextCoded), base);
	for (size_t offset = 0; (offset < covered) && (offset < sent) && (udp->inFlight > 0); offset++)
	{
		rdpudp_pending* p = pending_find(udp, seq16_add(base, offset));
		if (!p)
			continue;
		if (received[offset])
			acknowledge(udp, p);
		else if (udp->server && (highest > offset + udp->reorderThreshold) && !p->lost)
		{
			/* Missing despite later datagrams arriving, so it's lost. Retransmit now, or after
			 * one report round since last send. */
			if (now_ms(udp) - p->lastSent >= report_resend_interval(udp))
				handle_reported_loss(udp, p);
			else
			{
				p->reportHeld = TRUE;
				pending_due_at(udp, p->lastSent + report_resend_interval(udp));
			}
		}
	}

	if (!udp->server)
		return;

	/* The original copy of a retransmitted datagram arrived; it was delayed, not lost. */
	for (size_t x = 0; x < RDPUDP2_RESENT_HISTORY; x++)
	{
		if (!udp->resent[x].valid)
			continue;
		const UINT16 seq = udp->resent[x].seq;
		if (seq16_after(base, seq))
		{
			udp->resent[x].valid = FALSE;
			continue;
		}
		const size_t offset = seq16_diff(seq, base);
		if ((offset >= covered) || !received[offset])
			continue;
		udp->resent[x].valid = FALSE;
		if (udp->ccRoundLost > 0)
			udp->ccRoundLost--;
		if (!udp->resent[x].reported)
			continue;
		if (udp->reorderThreshold < RDPUDP2_MAX_REORDER_THRESHOLD)
		{
			udp->reorderThreshold *= 2;
			if (udp->reorderThreshold > RDPUDP2_MAX_REORDER_THRESHOLD)
				udp->reorderThreshold = RDPUDP2_MAX_REORDER_THRESHOLD;
			WLog_Print(udp->log, WLOG_DEBUG,
			           "datagram 0x%04" PRIx16
			           " arrived after its resend, reordering threshold: %" PRIu32,
			           seq, udp->reorderThreshold);
		}
		udp->reorderCalmRounds = 0;
	}
}

static void v3_process_ack(rdpUdp* udp, UINT16 ack)
{
	WINPR_ASSERT(udp);
	/* a real peer cannot acknowledge a datagram this end never sent */
	if (udp->server && v3_not_sent(udp, ack))
	{
		WLog_Print(udp->log, WLOG_DEBUG,
		           "ignoring an ACK of 0x%04" PRIx16 ", past the last datagram sent 0x%04" PRIx16,
		           ack, v3_last_sent(udp));
		return;
	}
	/* An ACK behind everything in flight acknowledges nothing. From more than half the sequence
	 * space back (the peer heard nothing from us for a long time) serial arithmetic would read it
	 * as ahead of sendBase and acknowledge datagrams the peer never got. */
	if (udp->server && (udp->inFlight > 0) && !v3_from_send_base(udp, ack))
		return;
	/* cumulative: everything up to and including ack */
	while ((udp->inFlight > 0) &&
	       !seq16_after(WINPR_ASSERTING_INT_CAST(UINT16, udp->sendBase), ack))
		acknowledge(udp, pending_find(udp, udp->sendBase));
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

WINPR_ATTR_NODISCARD
static BOOL v3_receive_channel_data(rdpUdp* udp, UINT16 channelSeq, const BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data || (length == 0));
	WINPR_ASSERT(!udp->server || (length <= RDPUDP_MAX_CHUNK));

	/* A retransmission travels under a new data sequence number with its original channel
	 * sequence, so the channel sequence alone says where a chunk belongs and whether it has
	 * been handed up already. */
	const UINT16 ahead = seq16_diff(channelSeq, udp->v3NextChannel);
	if (ahead >= RDPUDP_RECEIVE_REACH)
		return TRUE;

	/* The chunk next in line goes up straight from the datagram, without a copy in a slot. */
	if ((ahead == 0) && !udp->slots[channelSeq % RDPUDP_RECEIVE_SLOTS].present)
	{
		udp->v3NextChannel = channel_seq_next(udp->v3NextChannel);
		if (!deliver(udp, data, length))
			return FALSE;
		return v3_deliver_ready(udp);
	}

	if (!store_slot(udp, channelSeq, data, length))
		return FALSE;
	return v3_deliver_ready(udp);
}

WINPR_ATTR_NODISCARD
static BOOL v3_process_datagram(rdpUdp* udp, BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data);
	if (length < 8)
		return TRUE;

	/* undo the swap of the first and eighth byte ([MS-RDPEUDP2] 2.2.1). In place: with only
	 * DATA set the chunk starts at the eighth byte, so a copy of the header would not do. */
	const BYTE eighth = data[7];
	data[7] = data[0];
	data[0] = eighth;

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, data, length);

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
		/* [MS-RDPEUDP2] 2.2.1.2.3: how many data packets the peer lets us acknowledge at once,
		 * and how long an acknowledgement may wait */
		if (!Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 3))
			return TRUE;
		const size_t maxDelayed = Stream_Get_UINT8(s);   /* MaxDelayedAcks */
		udp->v3DelayedAckTimeout = Stream_Get_UINT16(s); /* DelayedAckTimeoutInMs */
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

	/* Server: the base comes from an AckOfAcks only. FreeRDP clients before version 3.34 put one
	 * on their first data packet and on resends, not on the ones after (since then they repeat
	 * it until acknowledged, see v3_send_data): taken from one of those, a lost or late first
	 * packet would lie below the base, and the cumulative ACK that follows tells the client it
	 * arrived. Ignored, the packet is resent with an AckOfAcks. */
	if (udp->server && !udp->v3BaseKnown && !haveAoA)
		return TRUE;

	udp->v3LatestArrival = now_ns(udp);
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
		udp->v3AckedSeq = seq16_diff(base, 1);
	}

	const UINT16 ahead = seq16_diff(dataSeq, udp->v3Expected);
	/* Behind the window: resent because our ACK got lost, given up by an AckOfAcks, or ahead of
	 * a position of ours the peer's numbers wrapped past. The window only tracks acknowledgements
	 * ([MS-RDPEUDP2] 3.1.1.2.2): the packet is acknowledged on its own, and its chunk still goes
	 * to the channel sequence, which drops it if it was handed up already (3.1.1.2.4.2). */
	const BOOL behind = (ahead >= 0x8000);
	/* Windows at times puts the next sequence number in the AckOfAcks of a data packet, past the
	 * packet itself. The peer has the acknowledgments for everything below its AckOfAcks, the
	 * receiver does not report those again (2.2.1.2.4), and Windows ends the connection on an ACK
	 * vector for such a packet. Only its chunk counts. */
	BOOL acknowledged = haveAoA && seq16_after(aoa, dataSeq);

	/* mstsc sometimes repeats old data packets and closes the connection on an ACK vector for a
	 * packet already covered by a cumulative ACK. To avoid this, the server does not report
	 * packets behind its window. If the server's position becomes incorrect after a silence,
	 * it waits for the next AckOfAcks to resync: an ACK vector based on a packet that only looks
	 * behind a stale window would also tell a peer that takes everything below the base as
	 * received (as v3_process_ack_vector does) that packets never seen arrived. Unacknowledged,
	 * the packet comes again with an AckOfAcks. */
	if (udp->server && behind)
	{
		WLog_Print(udp->log, WLOG_DEBUG,
		           "data packet 0x%04" PRIx16 " is behind the window at 0x%04" PRIx16
		           ", skipping ACK vector",
		           dataSeq, udp->v3Expected);
		acknowledged = TRUE;
	}

	if (udp->server && (packetType != RDPUDP2_PACKET_DUMMY) && (Stream_GetRemainingLength(s) >= 2))
	{
		/* The server validates the chunk before marking the packet for ACK or ACK vector, as the
		 * peer never resends acked packets. Chunks out of reach wait for retransmission. Oversized
		 * chunks are dropped as if lost; this prevents a single spoofed datagram from killing the
		 * connection, while a client repeatedly sending them will stall its own stream until the
		 * stall check triggers. */
		UINT16 nextChannel = 0;
		Stream_Peek_UINT16(s, nextChannel); /* Peek the channel sequence number */
		const UINT16 chunkAhead = seq16_diff(nextChannel, udp->v3NextChannel);
		if ((chunkAhead >= udp->chunkReach) && (chunkAhead < 0x8000))
			return TRUE;
		if (Stream_GetRemainingLength(s) > 2ull + RDPUDP_MAX_CHUNK)
		{
			WLog_Print(udp->log, WLOG_DEBUG, "discarding a chunk of %" PRIuz " bytes",
			           Stream_GetRemainingLength(s) - 2);
			return TRUE;
		}
	}

	if (behind && !acknowledged)
	{
		/* a run of consecutive ones goes in one ACK vector, see v3_send_ack */
		const BOOL extends = (udp->v3BehindCount > 0) &&
		                     (dataSeq == seq16_add(udp->v3BehindBase, udp->v3BehindCount)) &&
		                     (udp->v3BehindCount < 63ull * RDPUDP2_MAX_ACKVEC);
		if ((udp->v3BehindCount > 0) && !extends && !v3_send_ack(udp))
			return FALSE;
		if (!extends)
		{
			udp->v3BehindBase = dataSeq;
			udp->v3BehindCount = 0;
		}
		udp->v3BehindCount++;
		udp->v3BehindArrival = udp->v3LatestArrival;
		udp->ackPending = TRUE;
	}
	else if (!behind)
	{
		udp->ackPending = TRUE;
		udp->v3WindowPending = TRUE;
		if (ahead >= RDPUDP_RECEIVE_REACH)
			return TRUE;
	}

	/* A dummy packet occupies a sequence number but its body means nothing. */
	const BOOL withChunk =
	    (packetType != RDPUDP2_PACKET_DUMMY) && (Stream_GetRemainingLength(s) >= 2);
	UINT16 channelSeq = 0;
	if (withChunk)
	{
		/* A chunk out of reach cannot be kept. Its packet stays unacknowledged then, so the
		 * peer sends it again: acknowledged, it would be gone for good. */
		channelSeq = Stream_Get_UINT16(s); /* ChannelSeqNum */
		const UINT16 chunkAhead = seq16_diff(channelSeq, udp->v3NextChannel);
		if ((chunkAhead >= udp->chunkReach) && (chunkAhead < 0x8000))
			return TRUE;
	}

	if (!behind)
	{
		if (seq16_after(dataSeq, udp->v3Highest))
			udp->v3Highest = dataSeq;
		udp->v3Received[dataSeq % RDPUDP_RECEIVE_SLOTS] = TRUE;
		udp->v3Arrival[dataSeq % RDPUDP_RECEIVE_SLOTS] = udp->v3LatestArrival;
		v3_advance_window(udp);
	}

	if (!withChunk)
		return TRUE;
	return v3_receive_channel_data(udp, channelSeq, Stream_ConstPointer(s),
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

	udp->synSent = now_ms(udp);
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
	udp->lastDelivery = now_ms(udp);
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
		udp->v3DelayedAckTimeout = UINT32_MAX;
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

/* A SYN datagram has snSourceAck = -1 and the SYN flag set without ACK. No RDP-UDP2 data
 * phase datagram begins with four 0xFF bytes followed by these flags. */
WINPR_ATTR_NODISCARD
static BOOL is_syn(const BYTE* data, size_t length)
{
	WINPR_ASSERT(data || (length == 0));
	if (length < 8)
		return FALSE;
	const UINT16 flags = WINPR_ASSERTING_INT_CAST(UINT16, (data[6] << 8) | data[7]);
	return (data[0] == 0xFF) && (data[1] == 0xFF) && (data[2] == 0xFF) && (data[3] == 0xFF) &&
	       ((flags & (RDPUDP_FLAG_SYN | RDPUDP_FLAG_ACK)) == RDPUDP_FLAG_SYN);
}

WINPR_ATTR_NODISCARD
static BOOL process_datagram(rdpUdp* udp, BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data);

	/* The client missed our SYN+ACK and resent its SYN. Since SYNs are padded to the MTU
	 * ([MS-RDPEUDP] 3.1.5.1.1), a genuine SYN is never shorter than our reply; shorter packets
	 * may be from a spoofed source attempting to induce asymmetric traffic. */
	if (udp->server && is_syn(data, length))
	{
		if (length < udp->synAckLength)
		{
			WLog_Print(udp->log, WLOG_DEBUG, "discarding a duplicate SYN shorter than the MTU");
			return TRUE;
		}
		WLog_Print(udp->log, WLOG_DEBUG, "received duplicate SYN, re-sending SYN+ACK");
		udp->synAckResent = TRUE;
		return rdpudp_send_raw(udp, udp->synAck, udp->synAckLength);
	}

	const UINT64 now = now_ms(udp);
	if (udp->server && (udp->synAckSent != 0))
	{
		/* [RFC6298] 2.2: the first datagram after the SYN+ACK provides the initial RTT sample.
		 * Without it, the first retransmission timer uses the minimum value, which may cause
		 * premature retransmissions of initial TLS flights on slow paths. Per Karn's algorithm,
		 * no sample is taken if the SYN+ACK was resent. Any datagram from the client's address is
		 * used, even if invalid; the sample is clamped between 1 ms and
		 * RDPUDP2_HANDSHAKE_RTT_MAX_MS, with subsequent data samples providing corrections. */
		if (!udp->synAckResent && !udp->rttSampled && (now >= udp->synAckSent))
		{
			UINT64 sample = now - udp->synAckSent;
			if (sample < 1)
				sample = 1;
			if (sample > RDPUDP2_HANDSHAKE_RTT_MAX_MS)
				sample = RDPUDP2_HANDSHAKE_RTT_MAX_MS;
			udp->srtt = WINPR_ASSERTING_INT_CAST(UINT32, sample);
			udp->rttvar = udp->srtt / 2;
			udp->rttSampled = TRUE;
		}
		udp->synAckSent = 0;
	}
	v3_note_silence(udp, now);
	udp->lastReceived = now;

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
	const UINT64 now = now_ms(udp);

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
		rdpudp_fail(udp, v3 ? "nothing heard from the peer for 16 seconds"
		                    : "nothing heard from the peer for 65 seconds");
		return FALSE;
	}
	v3_note_silence(udp, now);

	/* stuck: chunks have been waiting behind a hole, and nothing moved, for the whole timeout */
	const UINT64 progress =
	    (udp->bufferingSince > udp->lastDelivery) ? udp->bufferingSince : udp->lastDelivery;
	if ((udp->buffered > 0) && (now - progress > RDPUDP_STALL_TIMEOUT_MS))
	{
		rdpudp_fail(udp, "the stream is stuck behind a chunk the peer no longer resends");
		return FALSE;
	}

	/* [RFC6298] 5.5: double the timeout once per expiry. If every timing-out datagram counted, a
	 * single loss burst would max out the backoff and cause excessive wait times. */
	BOOL backedOff = FALSE;
	/* Datagrams in flight are only looked at once one of them is due. */
	const BOOL scan = (now >= udp->pendingDue);
	UINT64 nextDue = UINT64_MAX;
	for (size_t x = 0; scan && (x < udp->inFlight); x++)
	{
		rdpudp_pending* p = &udp->pending[x];
		if (p->reportHeld && (now - p->lastSent >= report_resend_interval(udp)))
			handle_reported_loss(udp, p);
		if ((now < p->due) && !p->lost)
		{
			nextDue = pending_next_due(udp, p, nextDue);
			continue;
		}
		/* A resend takes a new number. Once the newest is about to get too far from the oldest in
		 * flight, the oldest goes out again first, which moves sendBase on; the others wait for
		 * the next scan, which comes right away. */
		if (udp->server && v3 && (p->seq != udp->sendBase) &&
		    (seq16_diff(WINPR_ASSERTING_INT_CAST(UINT16, udp->nextCoded),
		                WINPR_ASSERTING_INT_CAST(UINT16, udp->sendBase)) >=
		     RDPUDP2_MAX_RESEND_SPAN))
		{
			rdpudp_pending* oldest = pending_find(udp, udp->sendBase);
			WINPR_ASSERT(oldest);
			oldest->lost = TRUE;
			nextDue = 0;
			continue;
		}
		const BOOL reported = p->lost;
		if (p->lost)
		{
			/* Reported missing: retransmit immediately without increasing RTO. Reordering may cause
			 * repeated reports, so this doesn't count toward the limit; peer timeout and stall
			 * checks handle dead connections. */
			p->lost = FALSE;
		}
		else
		{
			if (p->timeouts >= (v3 ? RDPUDP2_MAX_RETRANSMITS : RDPUDP_MAX_RETRANSMITS))
			{
				rdpudp_fail(udp, "a datagram went unacknowledged too many times");
				return FALSE;
			}
			p->timeouts++;
			UINT64 maxRto = v3 ? rto_v3_cap(udp) : RDPUDP_MAX_RTO_MS;
			if (maxRto > RDPUDP_MAX_RTO_MS)
				maxRto = RDPUDP_MAX_RTO_MS;
			p->rto =
			    (p->rto * 2ull > maxRto) ? WINPR_ASSERTING_INT_CAST(UINT32, maxRto) : p->rto * 2;
			if (udp->server)
			{
				if (!backedOff && (udp->rtoBackoff < RDPUDP2_MAX_RTO_BACKOFF))
					udp->rtoBackoff++;
				backedOff = TRUE;
				cc_on_loss(udp);
			}
		}
		p->retransmits++;
		p->due = now + p->rto;
		if (udp->server)
		{
			p->lastSent = now;
			p->reportHeld = FALSE;
		}
		if (udp->version == RDPUDP_PROTOCOL_VERSION_3)
		{
			/* [MS-RDPEUDP2] 3.1.5.1.3: a lost packet is resent under a new data sequence
			 * number, the channel sequence number stays. The AckOfAcks then moves past the
			 * old one, so the peer stops waiting for it. */
			const UINT16 coded = WINPR_ASSERTING_INT_CAST(UINT16, udp->nextCoded);
			if (udp->pendingIndex[coded] != 0)
			{
				rdpudp_fail(udp, "a datagram is still in flight 65536 sequence numbers back");
				return FALSE;
			}
			if (udp->server)
			{
				udp->resent[udp->resentNext].valid = TRUE;
				udp->resent[udp->resentNext].reported = reported;
				udp->resent[udp->resentNext].seq = WINPR_ASSERTING_INT_CAST(UINT16, p->seq);
				udp->resentNext = (udp->resentNext + 1) % RDPUDP2_RESENT_HISTORY;
			}
			udp->pendingIndex[p->seq & 0xFFFF] = 0;
			udp->pendingIndex[coded] = WINPR_ASSERTING_INT_CAST(UINT16, x + 1);
			p->seq = coded;
			udp->nextCoded = seq16_add(coded, 1);
			send_base_advance(udp);
		}
		else
		{
			udp->cwrPending = TRUE;
		}
		WLog_Print(udp->log, WLOG_TRACE, "retransmitting 0x%08" PRIx32 " (#%" PRIu32 ")", p->seq,
		           p->retransmits);
		if (!rdpudp_transmit(udp, p))
			return FALSE;
		nextDue = pending_next_due(udp, p, nextDue);
	}
	if (scan)
		udp->pendingDue = nextDue;

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
	udp->ownsSocket = TRUE;
	udp->state = RDPUDP_STATE_CLOSED;
	udp->receive = receive;
	udp->custom = custom;
	udp->mtu = RDPUDP_MTU;
	udp->cwnd = RDPUDP2_INITIAL_CWND;
	udp->ssthresh = RDPUDP_MAX_IN_FLIGHT;
	udp->rttMin = UINT32_MAX;
	udp->rttMinNext = UINT32_MAX;
	udp->reorderThreshold = RDPUDP2_REORDER_THRESHOLD;
	udp->chunkReach = RDPUDP_RECEIVE_REACH;
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

	if (udp->ownsSocket && (udp->sockfd != INVALID_SOCKET))
		closesocket(udp->sockfd);

	for (size_t x = 0; x < udp->inFlight; x++)
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
	udp->lastReceived = now_ms(udp);
	return send_syn(udp);
}

/* [MS-RDPEUDP] 3.1.5.1.3: The SYN+ACK is a SYN that acknowledges the client's
 * snInitialSequenceNumber and carries the negotiated MTU and version. Like the
 * SYN, it is zero-padded to the MTU, but contains no cookieHash. */
WINPR_ATTR_NODISCARD
static BOOL build_syn_ack(rdpUdp* udp, UINT32 clientInitial)
{
	WINPR_ASSERT(udp);
	/* rdpudp_accept clamps the MTU to the range [RDPUDP_MIN_MTU, RDPUDP_MTU], where
	 * RDPUDP_MTU is the size of synAck. */
	if ((udp->mtu < RDPUDP_MIN_MTU) || (udp->mtu > sizeof(udp->synAck)))
		return FALSE;
	memset(udp->synAck, 0, sizeof(udp->synAck));
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, udp->synAck, sizeof(udp->synAck));
	if (!s)
		return FALSE;

	/* Write the RDPUDP_FEC_HEADER */
	Stream_Write_UINT32_BE(
	    s, clientInitial); /* Write the source acknowledgement sequence number (snSourceAck) */
	Stream_Write_UINT16_BE(s, RDPUDP_RECEIVE_WINDOW);
	Stream_Write_UINT16_BE(s, RDPUDP_FLAG_SYN | RDPUDP_FLAG_ACK | RDPUDP_FLAG_SYNEX);

	/* Write the RDPUDP_SYNDATA_PAYLOAD */
	Stream_Write_UINT32_BE(s, udp->initialSequence);
	Stream_Write_UINT16_BE(s, udp->mtu); /* Set the upstream MTU (uUpStreamMtu) */
	Stream_Write_UINT16_BE(s, udp->mtu); /* Downstream MTU */

	/* Write the RDPUDP_SYNDATAEX_PAYLOAD */
	Stream_Write_UINT16_BE(s, RDPUDP_VERSION_INFO_VALID);
	Stream_Write_UINT16_BE(s, udp->version);

	udp->synAckLength = udp->mtu;
	return TRUE;
}

BOOL rdpudp_accept(rdpUdp* udp, rdpUdpSendFn send, void* sendCustom, const BYTE* syn, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(send);
	WINPR_ASSERT(syn);

	if ((udp->state != RDPUDP_STATE_CLOSED) || !is_syn(syn, length))
		return FALSE;

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, syn, length);
	if (!s || !Stream_CheckAndLogRequiredLengthWLog(udp->log, s, 16))
		return FALSE;

	/* Parse the RDPUDP_FEC_HEADER */
	Stream_Seek_UINT32(s); /* Skip snSourceAck, which is expected to be -1 */
	const UINT16 window = Stream_Get_UINT16_BE(s);
	const UINT16 flags = Stream_Get_UINT16_BE(s);

	/* Parse RDPUDP_SYNDATA_PAYLOAD */
	const UINT32 clientInitial = Stream_Get_UINT32_BE(s);
	const UINT16 upMtu = Stream_Get_UINT16_BE(s);
	const UINT16 downMtu = Stream_Get_UINT16_BE(s);

	if ((flags & RDPUDP_FLAG_CORRELATION_ID) && !Stream_SafeSeek(s, 32))
		return FALSE;

	/* RDPUDP_SYNDATAEX_PAYLOAD. The listener already matched a version 3 SYN's cookieHash to this
	 * connection; other versions lack a cookieHash and are indistinguishable. */
	UINT16 version = RDPUDP_PROTOCOL_VERSION_1;
	if ((flags & RDPUDP_FLAG_SYNEX) && (Stream_GetRemainingLength(s) >= 4))
	{
		const UINT16 synExFlags = Stream_Get_UINT16_BE(s);
		const UINT16 udpVersion = Stream_Get_UINT16_BE(s);
		if (synExFlags & RDPUDP_VERSION_INFO_VALID)
			version = udpVersion;
	}
	if (version != RDPUDP_PROTOCOL_VERSION_3)
	{
		WLog_Print(udp->log, WLOG_WARN,
		           "client offered RDP-UDP version 0x%04" PRIx16 ", only 0x%04" PRIx16
		           " is implemented for a server",
		           version, RDPUDP_PROTOCOL_VERSION_3);
		return FALSE;
	}

	/* [MS-RDPEUDP] 3.1.1.3: neither end sends more than the smaller of the two MTUs. */
	UINT16 mtu = (upMtu < downMtu) ? upMtu : downMtu;
	if (mtu > RDPUDP_MTU)
		mtu = RDPUDP_MTU;
	if (mtu < RDPUDP_MIN_MTU)
		mtu = RDPUDP_MIN_MTU;

	/* [MS-RDPEUDP] 3.1.5.1.1: SYN is padded to the smaller MTU, and SYN+ACK is mtu bytes. A
	 * shorter SYN may indicate a spoofed source, as our reply would be larger than the received
	 * packet. */
	if (length < mtu)
	{
		WLog_Print(udp->log, WLOG_WARN,
		           "rejecting a SYN of %" PRIuz " bytes, which is shorter than the MTU of %" PRIu16,
		           length, mtu);
		return FALSE;
	}

	udp->server = TRUE;
	udp->ownsSocket = FALSE;
	udp->sendFn = send;
	udp->sendCustom = sendCustom;
	udp->chunkReach = RDPUDP_UNVERIFIED_REACH;

	udp->mtu = mtu;
	udp->version = version;
	udp->peerWindow = window;
	udp->lastReceived = now_ms(udp);
	udp->lastDelivery = udp->lastReceived;
	udp->state = RDPUDP_STATE_ESTABLISHED;

	/* Same as client; see process_syn_ack */
	udp->nextCoded = RDPUDP2_INITIAL_SEQUENCE;
	udp->nextChannelSeq = 1;
	udp->v3NextChannel = 1;
	udp->v3MaxDelayedAcks = RDPUDP2_DEFAULT_DELAYED_ACKS;
	udp->v3DelayedAckTimeout = UINT32_MAX;

	if (!build_syn_ack(udp, clientInitial))
		return FALSE;
	WLog_Print(udp->log, WLOG_INFO,
	           "accepted RDP-UDP client, protocol version 0x%04" PRIx16 ", MTU %" PRIu16, version,
	           mtu);
	udp->synAckSent = now_ms(udp);
	if (udp->synAckSent == 0)
		udp->synAckSent = 1; /* 0 indicates not sent; a test clock might start at 0 */
	return rdpudp_send_raw(udp, udp->synAck, udp->synAckLength);
}

BOOL rdpudp_get_syn_cookie_hash(const BYTE* data, size_t length, BYTE hash[32])
{
	WINPR_ASSERT(hash);
	/* SYN is padded to MTU, which is at least RDPUDP_MIN_MTU ([MS-RDPEUDP] 3.1.5.1.1) */
	if ((length < RDPUDP_MIN_MTU) || !is_syn(data, length))
		return FALSE;

	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, data, length);
	if (!s || !Stream_SafeSeek(s, 6)) /* Skip snSourceAck and uReceiveWindowSize */
		return FALSE;
	const UINT16 flags = Stream_Get_UINT16_BE(s);
	if (!(flags & RDPUDP_FLAG_SYNEX) || !Stream_SafeSeek(s, 8)) /* Skip RDPUDP_SYNDATA_PAYLOAD */
		return FALSE;
	if ((flags & RDPUDP_FLAG_CORRELATION_ID) && !Stream_SafeSeek(s, 32))
		return FALSE;
	if (Stream_GetRemainingLength(s) < 4 + 32)
		return FALSE;
	const UINT16 synExFlags = Stream_Get_UINT16_BE(s);
	const UINT16 version = Stream_Get_UINT16_BE(s);
	if (!(synExFlags & RDPUDP_VERSION_INFO_VALID) || (version != RDPUDP_PROTOCOL_VERSION_3))
		return FALSE;
	Stream_Read(s, hash, 32);
	return TRUE;
}

BOOL rdpudp_receive(rdpUdp* udp, BYTE* data, size_t length)
{
	WINPR_ASSERT(udp);
	WINPR_ASSERT(data); /* process_datagram requires a buffer even for empty datagrams */

	if (udp->state != RDPUDP_STATE_ESTABLISHED)
		return FALSE;
	if (!process_datagram(udp, data, length))
	{
		rdpudp_fail(udp, "failed to process a datagram");
		return FALSE;
	}
	if (udp->state == RDPUDP_STATE_FAILED)
		return FALSE;
	if (!v3_ack_if_due(udp))
		return FALSE;
	if (!ack_when_due(udp))
		return FALSE;
	if (!run_timers(udp))
		return FALSE;
	return flush_send_queue(udp);
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
	/* v3: the DATA datagram fields and room for some arrival gaps of the ACK it carries.
	 * v1/v2: header, room for the ACK vector and AckOfAcks, source payload header. */
	const size_t overhead = (udp->version == RDPUDP_PROTOCOL_VERSION_3)
	                            ? (RDPUDP2_DATA_OVERHEAD + RDPUDP2_DATA_ACK_ROOM)
	                            : (8u + RDPUDP_ACK_ROOM + 8u);
	if (udp->mtu <= overhead)
		return 0;
	return udp->mtu - overhead;
}

size_t rdpudp_get_send_backlog(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	return Queue_Count(udp->sendQueue);
}

size_t rdpudp_get_send_window(const rdpUdp* udp)
{
	return window_limit(udp);
}

UINT32 rdpudp_get_rtt(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	if (!udp->rttSampled)
		return 0;
	/* Samples below clock resolution are still valid samples */
	return (udp->srtt > 0) ? udp->srtt : 1;
}

UINT32 rdpudp_get_timeout(const rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	const UINT64 now = now_ms(udp);
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

	if (udp->inFlight > 0)
	{
		if (udp->pendingDue <= now)
			return 0;
		if (udp->pendingDue - now < timeout)
			timeout = udp->pendingDue - now;
	}

	/* an acknowledgement that waits, see ack_when_due */
	if (udp->ackPending && (udp->v3AckDue != 0))
	{
		if (udp->v3AckDue <= now)
			return 0;
		if (udp->v3AckDue - now < timeout)
			timeout = udp->v3AckDue - now;
	}
	return WINPR_ASSERTING_INT_CAST(UINT32, timeout);
}

BOOL rdpudp_check(rdpUdp* udp)
{
	WINPR_ASSERT(udp);

	if ((udp->state == RDPUDP_STATE_FAILED) || (udp->state == RDPUDP_STATE_CLOSED))
		return FALSE;

	BYTE* buffer = udp->recvBuffer;
	while (udp->ownsSocket)
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

	if (!ack_when_due(udp))
		return FALSE;

	if (!flush_send_queue(udp))
		return FALSE;

	if (!run_timers(udp))
		return FALSE;
	/* Server: flush queued chunks that now fit the window instead of waiting for wakeup */
	if (udp->server)
		return flush_send_queue(udp);
	return TRUE;
}

void rdpudp_set_peer_verified(rdpUdp* udp)
{
	WINPR_ASSERT(udp);
	udp->chunkReach = RDPUDP_RECEIVE_REACH;
}

void rdpudp_set_clock(rdpUdp* udp, rdpUdpClockFn clock, void* custom)
{
	WINPR_ASSERT(udp);
	udp->clock = clock;
	udp->clockCustom = custom;
}

UINT64 rdpudp_get_time(const rdpUdp* udp)
{
	return now_ms(udp);
}
