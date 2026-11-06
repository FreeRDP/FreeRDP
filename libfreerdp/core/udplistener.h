/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * UDP listener of the server side multitransport tunnels
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

#ifndef FREERDP_LIB_CORE_UDPLISTENER_H
#define FREERDP_LIB_CORE_UDPLISTENER_H

#include <winpr/wtypes.h>
#include <winpr/winsock.h>

#include <freerdp/api.h>
#include <freerdp/server/udp.h>

/** @brief The library's side of include/freerdp/server/udp.h.
 *
 * Clients send their RDP-UDP SYN to the address and port that their TCP connection went to
 * ([MS-RDPEMT] 1.3). A listener only sorts: it reads a datagram, finds its flow and copies the
 * datagram there. Its thread does no protocol work, so a slow client does not delay other clients.
 * The tunnel of the flow gets the datagram on its own thread. */

/** The largest datagram that a flow takes. RDPUDP_MAX_CHUNK sets the limit of what a client can
 *  use, plus the headers of the datagram around it. */
#define UDPFLOW_MAX_DATAGRAM 2304

/** Takes the oldest datagram of a flow.
 *  @param data room for UDPFLOW_MAX_DATAGRAM bytes
 *  @return FALSE when none waits */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udpflow_read(rdpUdpFlow* flow, BYTE* data, size_t* length);

/** Wakes who waits on freerdp_udp_flow_get_event(). */
FREERDP_LOCAL void udpflow_wake(rdpUdpFlow* flow);

/** What freerdp_udp_flow_check() and freerdp_udp_flow_get_timeout() run. */
FREERDP_LOCAL void udpflow_set_worker(rdpUdpFlow* flow, BOOL (*check)(void* custom),
                                      UINT32 (*timeout)(void* custom), void* custom);

/** The default of a server: a process wide listener with a thread of its own. It waits on the
 *  address and port of the TCP connection. Its sockets open with the first flow that waits on them.
 *  Everything closes when the last flow is gone. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udplistener_expect_on_address(const struct sockaddr* addr, size_t addrlen,
                                                 rdpUdpFlow* flow);

#endif /* FREERDP_LIB_CORE_UDPLISTENER_H */
