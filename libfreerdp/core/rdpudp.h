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
 * The SYN / SYN+ACK exchange always uses the [MS-RDPEUDP] datagram format. The version from that
 * exchange sets the data phase. Versions 1 and 2 continue to use [MS-RDPEUDP], and version 3
 * changes to [MS-RDPEUDP2]. The connection carries a byte stream (in this case a TLS session). The
 * caller gives it chunks no larger than rdpudp_get_max_payload(), one per datagram. The caller gets
 * the chunks of the peer in order through the receive callback. Nothing in here is thread safe, so
 * the owner serializes all calls. */
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

/** Receives one in-order chunk of the peer's stream. Returning FALSE fails the connection. */
typedef BOOL (*rdpUdpReceiveFn)(void* custom, const BYTE* data, size_t length);

/** Sends one datagram of a server connection. A return value of FALSE makes the connection fail. A
 *  datagram that the network lost is not a failure. */
typedef BOOL (*rdpUdpSendFn)(void* custom, const BYTE* data, size_t length);

FREERDP_LOCAL void rdpudp_free(rdpUdp* udp);

/** @param cookieHash the 32 byte [MS-RDPEUDP] cookieHash, or nullptr to only offer version 2 */
WINPR_ATTR_MALLOC(rdpudp_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpUdp* rdpudp_new(wLog* log, const BYTE* cookieHash, rdpUdpReceiveFn receive,
                                 void* custom);

/** Client: opens the socket and sends the SYN. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_connect(rdpUdp* udp, const struct sockaddr* addr, size_t addrlen);

/** Server: answers the SYN of a client with a SYN+ACK. A server connection has no socket. It sends
 *  with send and gets the datagrams of the client through rdpudp_input(). It accepts only version
 *  3. The caller already matched the cookieHash of the SYN.
 *  @param udp a connection made with rdpudp_new() and a nullptr cookieHash */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_accept(rdpUdp* udp, rdpUdpSendFn send, void* sendCustom, const BYTE* syn,
                                 size_t length);

/** Server: processes one datagram from the client, in place. This changes data. rdpudp_check()
 *  still runs the timers.
 *  @return FALSE after the connection failed */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_input(rdpUdp* udp, BYTE* data, size_t length);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL SOCKET rdpudp_get_socket(const rdpUdp* udp);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL RDPUDP_STATE rdpudp_get_state(const rdpUdp* udp);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT16 rdpudp_get_version(const rdpUdp* udp);

/** Largest chunk rdpudp_send() accepts, so the datagram stays within the negotiated MTU. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL size_t rdpudp_get_max_payload(const rdpUdp* udp);

/** Chunks that rdpudp_send() took and that wait behind the congestion window. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL size_t rdpudp_get_send_backlog(const rdpUdp* udp);

/** The datagrams that can be unacknowledged now. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL size_t rdpudp_get_send_window(const rdpUdp* udp);

/** The smoothed round trip time in milliseconds, 0 before the first sample. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 rdpudp_get_rtt(const rdpUdp* udp);

/** Tells if a socket error only means that this datagram is lost. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_socket_error_is_transient(int error);

/** Milliseconds until a timer of this connection is due. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 rdpudp_get_timeout(const rdpUdp* udp);

/** Gets the cookieHash from a version 3 SYN of a client. The cookieHash tells which connection the
 *  SYN is for.
 *  @return FALSE for anything else */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_get_syn_cookie_hash(const BYTE* data, size_t length, BYTE hash[32]);

/** Reads all datagrams that wait on the socket (client only) and runs the timers.
 *  @return FALSE after the connection failed */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_check(rdpUdp* udp);

/** Queues one chunk of the stream for reliable delivery. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_send(rdpUdp* udp, const BYTE* data, size_t length);

#endif /* FREERDP_LIB_CORE_RDPUDP_H */
