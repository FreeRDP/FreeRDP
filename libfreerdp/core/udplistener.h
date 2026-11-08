/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server UDP listener for multitransport tunnels
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

/** @brief Library implementation of include/freerdp/server/udp.h.
 *
 * Clients send RDP-UDP SYN packets to the TCP connection's address and port ([MS-RDPEMT] 1.3).
 * The listener demultiplexes incoming datagrams by routing them to the corresponding flow,
 * where a per-flow tunnel thread processes them. The listener thread performs no protocol
 * work to prevent slow clients from blocking others. */

/** Maximum datagram size accepted by a flow: RDPUDP_MAX_CHUNK plus datagram headers. */
#define UDPFLOW_MAX_DATAGRAM 2304

/** Dequeues the oldest datagram from a flow.
 * @param data Buffer for UDPFLOW_MAX_DATAGRAM bytes.
 * @return FALSE if no datagram is queued. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udpflow_read(rdpUdpFlow* flow, BYTE* data, size_t* length);

/** Wakes the flow worker waiting on udpflow_get_events(). */
FREERDP_LOCAL void udpflow_wake(rdpUdpFlow* flow);

/** Returns the events the flow worker waits on: the flow's own event and, if applicable,
 * the relay socket's event.
 * @return Number of events (1 or 2). */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL DWORD udpflow_get_events(rdpUdpFlow* flow, HANDLE events[2]);

/** Configures the callbacks used by freerdp_udp_flow_check() and freerdp_udp_flow_get_timeout(). */
FREERDP_LOCAL void udpflow_set_worker(rdpUdpFlow* flow, BOOL (*check)(void* custom),
                                      UINT32 (*timeout)(void* custom), void* custom);

/** Default server listener: a process-wide listener thread bound to the TCP connection's
 * address and port. Sockets are opened by the first requesting flow and closed when the
 * last flow is destroyed. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udplistener_add_flow_on_address(const struct sockaddr* addr, size_t addrlen,
                                                   rdpUdpFlow* flow);

#endif /* End of FREERDP_LIB_CORE_UDPLISTENER_H */
