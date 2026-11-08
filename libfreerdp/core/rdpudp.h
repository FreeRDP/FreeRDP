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

#ifndef FREERDP_LIB_CORE_RDPUDP_H
#define FREERDP_LIB_CORE_RDPUDP_H

#include <winpr/wtypes.h>
#include <winpr/winsock.h>
#include <winpr/wlog.h>

#include <freerdp/api.h>

/** @brief One end of a reliable (RDP-UDP-R) connection.
 *
 * The SYN/SYN+ACK exchange always uses the [MS-RDPEUDP] format. The negotiated version
 * determines the data phase: versions 1 and 2 use [MS-RDPEUDP], while version 3 uses
 * [MS-RDPEUDP2].
 *
 * The connection carries an ordered byte stream (e.g., a TLS session). The caller
 * provides chunks up to rdpudp_get_max_payload(), each sent in a single datagram;
 * peer data is delivered in order via the receive callback.
 *
 * Not thread-safe; the caller must serialize access.
 */
typedef struct rdp_udp rdpUdp;

typedef enum
{
	RDPUDP_STATE_CLOSED,
	RDPUDP_STATE_SYN_SENT,
	RDPUDP_STATE_ESTABLISHED,
	RDPUDP_STATE_FAILED
} RDPUDP_STATE;

#define RDPUDP_PROTOCOL_VERSION_1 0x0001
#define RDPUDP_PROTOCOL_VERSION_2 0x0002
#define RDPUDP_PROTOCOL_VERSION_3 0x0101

/** Returns time in nanoseconds; only relative differences are meaningful. */
typedef UINT64 (*rdpUdpClockFn)(void* custom);

/** Receives one in-order chunk of the peer's stream. Returning FALSE fails the connection. */
typedef BOOL (*rdpUdpReceiveFn)(void* custom, const BYTE* data, size_t length);

/** Sends a datagram for a server connection. Return FALSE to fail the connection;
 *  network loss is not a failure. */
typedef BOOL (*rdpUdpSendFn)(void* custom, const BYTE* data, size_t length);

FREERDP_LOCAL void rdpudp_free(rdpUdp* udp);

/** @param cookieHash the 32 byte [MS-RDPEUDP] cookieHash, or nullptr to only offer version 2 */
WINPR_ATTR_MALLOC(rdpudp_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpUdp* rdpudp_new(wLog* log, const BYTE* cookieHash, rdpUdpReceiveFn receive,
                                 void* custom);

/** Client: opens the socket and sends the initial SYN packet. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_connect(rdpUdp* udp, const struct sockaddr* addr, size_t addrlen);

/** Server: responds to a client SYN with a SYN+ACK. Only version 3 is accepted; the
 *  caller must have already verified the SYN's cookieHash. Server connections are
 *  socketless: they use the send callback and receive datagrams via rdpudp_receive().
 *  @param udp connection created by rdpudp_new() with a NULL cookieHash */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_accept(rdpUdp* udp, rdpUdpSendFn send, void* sendCustom, const BYTE* syn,
                                 size_t length);

/** Server: processes a client datagram. Operates in-place (modifies data); timers
 *  are handled by rdpudp_check().
 *  @return FALSE if the connection failed */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_receive(rdpUdp* udp, BYTE* data, size_t length);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL SOCKET rdpudp_get_socket(const rdpUdp* udp);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL RDPUDP_STATE rdpudp_get_state(const rdpUdp* udp);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT16 rdpudp_get_version(const rdpUdp* udp);

/** Largest chunk rdpudp_send() accepts, so the datagram stays within the negotiated MTU. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL size_t rdpudp_get_max_payload(const rdpUdp* udp);

/** Number of chunks accepted by rdpudp_send() that are queued behind the congestion window. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL size_t rdpudp_get_send_backlog(const rdpUdp* udp);

/** Maximum number of datagrams allowed in flight. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL size_t rdpudp_get_send_window(const rdpUdp* udp);

/** Smoothed round-trip time (SRTT) in milliseconds (server only), or 0 before the first sample. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 rdpudp_get_rtt(const rdpUdp* udp);

/** Returns TRUE if the socket error indicates a transient datagram loss. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_socket_error_is_transient(int error);

/** Milliseconds until a timer of this connection is due. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 rdpudp_get_timeout(const rdpUdp* udp);

/** Extracts the connection-identifying cookieHash from a client version 3 SYN.
 *  Since SYNs are padded to MTU, inputs shorter than the minimum MTU are rejected.
 *  @return FALSE on failure */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_get_syn_cookie_hash(const BYTE* data, size_t length, BYTE hash[32]);

/** Reads all pending socket datagrams (client only) and updates timers.
 *  @return FALSE if the connection failed */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_check(rdpUdp* udp);

/** Queues one chunk of the stream for reliable delivery. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_send(rdpUdp* udp, const BYTE* data, size_t length);

/** Server: marks the client as having proven cookie ownership. Until then, only a
 *  limited number of out-of-order chunks are buffered. */
FREERDP_LOCAL void rdpudp_set_peer_verified(rdpUdp* udp);

/** Overrides the system clock for this connection to allow tests to advance time.
 *  @param clock clock function, or NULL to use the system clock */
FREERDP_LOCAL void rdpudp_set_clock(rdpUdp* udp, rdpUdpClockFn clock, void* custom);

/** Current connection clock in milliseconds, for use by the caller's timers. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT64 rdpudp_get_time(const rdpUdp* udp);

#endif /* FREERDP_LIB_CORE_RDPUDP_H */
