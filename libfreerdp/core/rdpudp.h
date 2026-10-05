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

/** @brief The client side of a reliable (RDP-UDP-R) connection.
 *
 * The SYN / SYN+ACK exchange always uses the [MS-RDPEUDP] datagram format. The version agreed
 * there decides the data phase: versions 1 and 2 keep using [MS-RDPEUDP], version 3 switches to
 * [MS-RDPEUDP2].
 *
 * The connection carries a byte stream (in this case a TLS session). The caller hands it chunks
 * no larger than rdpudp_get_max_payload(), each of which travels in one datagram, and gets the
 * peer's chunks back in order through the receive callback.
 *
 * Nothing in here is thread safe, the owner serializes all calls.
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

/** Receives one in-order chunk of the peer's stream. Returning FALSE fails the connection. */
typedef BOOL (*rdpUdpReceiveFn)(void* custom, const BYTE* data, size_t length);

FREERDP_LOCAL void rdpudp_free(rdpUdp* udp);

/** @param cookieHash the 32 byte [MS-RDPEUDP] cookieHash, or nullptr to only offer version 2 */
WINPR_ATTR_MALLOC(rdpudp_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpUdp* rdpudp_new(wLog* log, const BYTE* cookieHash, rdpUdpReceiveFn receive,
                                 void* custom);

/** Opens the socket and sends the SYN. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_connect(rdpUdp* udp, const struct sockaddr* addr, size_t addrlen);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL SOCKET rdpudp_get_socket(const rdpUdp* udp);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL RDPUDP_STATE rdpudp_get_state(const rdpUdp* udp);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT16 rdpudp_get_version(const rdpUdp* udp);

/** Largest chunk rdpudp_send() accepts, so the datagram stays within the negotiated MTU. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL size_t rdpudp_get_max_payload(const rdpUdp* udp);

/** Milliseconds until a timer of this connection is due. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 rdpudp_get_timeout(const rdpUdp* udp);

/** Reads every datagram waiting on the socket and runs the timers.
 *  @return FALSE once the connection has failed */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_check(rdpUdp* udp);

/** Queues one chunk of the stream for reliable delivery. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL rdpudp_send(rdpUdp* udp, const BYTE* data, size_t length);

#endif /* FREERDP_LIB_CORE_RDPUDP_H */
