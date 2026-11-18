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

/** @brief Internal library implementation of include/freerdp/server/udp.h and the server UDP API,
 * including the listener, flows, relays, and peer flow hooks.
 *
 * Following [MS-RDPEMT] 1.3, clients send RDP-UDP SYN packets to the TCP connection's address
 * and port. The listener demultiplexes incoming datagrams by routing them to the corresponding
 * flow, where a per-flow tunnel thread processes them. The listener thread performs no
 * protocol work to prevent slow clients from blocking others.
 *
 * - \b freerdp_udp_listener owns the sockets and demuxes datagrams by client. It runs in
 *   its own thread or via an event loop.
 * - \b rdpUdpFlow represents the datagram path for one client and is the only network
 *   component a tunnel requires.
 * - \b rdpUdpPeerHooks allows a peer to specify the datagram source for its flow, track
 *   UDP connection progress, and pace transmissions.
 *
 * Servers that fork per connection or delegate to per-user processes serve clients outside
 * the process owning the port. Such servers keep the listener in the port-owning process
 * and use a socket pair per connection via freerdp_udp_socket_pair(). One end of the pair
 * is passed to the child process:
 *
 * - In the connection process: OfferTunnel calls freerdp_udp_flow_set_socket() with one
 *   end and passes the other end along with cookieHash.
 * - In the port-owning process: freerdp_udp_listener_add_relay() handles the remaining setup.
 *
 * The connection process never accesses the shared socket, preventing it from reading
 * other users' datagrams. Listener and socket-pair sockets are close-on-exec; a child
 * that forks without exec must call freerdp_udp_listener_close_after_fork() instead of
 * freerdp_udp_listener_free(). The relay ends when either socket in the pair closes.
 *
 * Datagrams are identified by source address; only the client's SYN ties a datagram to a
 * connection via a cleartext hash. A UDP connection is unproven until the security cookie
 * arrives within the TLS session (see FreeRDP_UdpLinkState). A source address remains
 * bound to its connection until termination; new connections from the same address/port
 * (e.g., via NAT) remain on TCP until then. The flow binds to the address of the first SYN
 * and its cookie hash; if the client's NAT assigns a new port, the tunnel ends.
 *
 * A tunnel allows at most 4096 datagrams in flight (~5 MB per round trip, e.g., 50 MB/s
 * at 100 ms), though the congestion window may be smaller. */

typedef struct rdp_udp_listener freerdp_udp_listener;
typedef struct rdp_udp_flow rdpUdpFlow;

/** @brief Comprehensive UDP send state information, including additional metrics. */
typedef struct
{
	BOOL writeBlocked;  /**< TRUE if queued data exceeds link capacity; see WriteUnblocked. */
	size_t queuedBytes; /**< Number of bytes accepted but not yet sent for the first time */
	UINT32 rttMs;       /**< Smoothed round-trip time; 0 before the first sample. */
	UINT64 rateBytesPerSecond; /**< The congestion window rate at rttMs; 0 before the first
	                              sample is taken. */
} rdpUdpSendState;

/** @brief Callbacks from FreeRDP_UdpPeerCallbacks and internal hooks. Executed on the thread
 * running the freerdp_peer check; OfferTunnel is called during connection setup. */
typedef struct
{
	psPeerUdpLinkChanged LinkChanged;
	psPeerUdpWriteUnblocked WriteUnblocked;
	/** Called before offering the tunnel. Route datagrams from the client
	 *  (identified by cookieHash in SYN) to flow. This hook must not return until routing
	 *  is established. If not provided, the process-wide listener handles routing
	 *  (single-process only). \b flow is owned by the peer and remains valid until
	 *  LinkChanged reports FREERDP_UDP_LINK_NONE. Call peer's Disconnect before freeing
	 *  the peer context on the peer's check thread.
	 *  @return \b FALSE to remain on TCP. The peer will free the flow without calling
	 *  LinkChanged; cease all flow usage before returning \b FALSE. */
	WINPR_ATTR_NODISCARD BOOL (*OfferTunnel)(void* custom, rdpUdpFlow* flow,
	                                         const BYTE cookieHash[32]);
	/** If \b TRUE, no worker thread is created. The caller must manually drive the tunnel
	 *  using freerdp_udp_flow_get_event(), freerdp_udp_flow_get_timeout(), and
	 *  freerdp_udp_flow_check() to handle ACKs and retransmissions. If driven on a
	 *  non-peer thread, stop before LinkChanged returns FREERDP_UDP_LINK_NONE. */
	BOOL ManualWorker;
} rdpUdpPeerHooks;

/** Configures the UDP hooks for a peer, overriding those set by
 *  freerdp_peer_set_udp_callbacks(). Call this after the peer context is created and before
 *  the tunnel is offered.
 *  @param hooks The hooks to apply, or \b nullptr for default behavior. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_peer_set_udp_hooks(freerdp_peer* peer, const rdpUdpPeerHooks* hooks,
                                              void* custom);

/** Retrieves the full link state; see freerdp_peer_get_udp_send_state() for details.
 *  @return \b FALSE if no tunnel exists */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_peer_get_udp_send_state_full(freerdp_peer* peer, rdpUdpSendState* state);

/** Sets the maximum bytes a peer's tunnel queues while awaiting the link.
 *  @param limit the limit, 0 for default */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_peer_set_udp_send_limit(freerdp_peer* peer, size_t limit);

/** @brief Callback for receiving datagrams; see freerdp_udp_flow_set_receive().
 *  Called with the listener's and the flow's locks held. Must not block, call
 *  listener functions, or free the flow or set its callbacks.
 *  @return \b FALSE to drop the datagram. */
typedef BOOL (*FreeRDP_UdpFlowReceiveFn)(void* custom, const BYTE* data, size_t length);

/** @brief Callback for sending datagrams to a client; see freerdp_udp_flow_set_send().
 *  Invoked while the flow's lock is held; must not block on threads using the flow, call
 *  listener functions, or free the flow or set its callbacks.
 *  @return \b FALSE on fatal errors. Network loss is not an error, as RDP-UDP
 *  handles retransmission. */
typedef BOOL (*FreeRDP_UdpFlowSendFn)(void* custom, const BYTE* data, size_t length);

/** Frees a flow created via freerdp_udp_flow_new(). Peer-owned flows are
 *  managed by the peer. Flows and their listener may be freed from any
 *  thread in any order. */
FREERDP_LOCAL void freerdp_udp_flow_free(rdpUdpFlow* flow);

/** Creates a flow for a client served by another process (e.g., a parent
 *  process in a forking server). */
WINPR_ATTR_MALLOC(freerdp_udp_flow_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpUdpFlow* freerdp_udp_flow_new(const BYTE cookieHash[32]);

/** @return the 32-byte cookie hash expected in the client's SYN */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL const BYTE* freerdp_udp_flow_get_cookie_hash(const rdpUdpFlow* flow);

/** Redirects flow datagrams to the receive callback instead of queuing them for a tunnel.
 *  Waits for the previous callback to return, so that its \b custom data can be safely
 *  freed afterwards. */
FREERDP_LOCAL void freerdp_udp_flow_set_receive(rdpUdpFlow* flow, FreeRDP_UdpFlowReceiveFn receive,
                                                void* custom);

/** Redirects flow datagrams to the send callback instead of the listener's socket.
 *  Waits for the previous callback to return, so that its \b custom can be
 *  freed afterwards. */
FREERDP_LOCAL void freerdp_udp_flow_set_send(rdpUdpFlow* flow, FreeRDP_UdpFlowSendFn send,
                                             void* custom);

/** Connects a flow to a datagram socket relayed via
 *  freerdp_udp_listener_add_relay(). The flow takes ownership of the socket for all I/O.
 *  @param fd a connected SOCK_SEQPACKET or SOCK_DGRAM socket
 *  @return \b FALSE if the flow already has a socket or fd is the wrong type; caller
 *  retains ownership of fd. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_flow_set_socket(rdpUdpFlow* flow, int fd);

/** Queues a datagram from the client. Thread-safe. If the queue limit is
 *  exceeded, the datagram is dropped and must be resent by the client.
 *  @return \b FALSE if dropped */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_flow_deliver(rdpUdpFlow* flow, const BYTE* data, size_t length);

/** Sends a datagram to the client. Thread-safe. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_flow_send(rdpUdpFlow* flow, const BYTE* data, size_t length);

/** Returns an event that remains set while datagrams await
 *  freerdp_udp_flow_check(). For flows using a socket via freerdp_udp_flow_set_socket(),
 *  this is the socket's event. Re-fetch the event before each wait as it changes when
 *  the peer closes. Do not manually set/reset the event, as the OS manages it. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL HANDLE freerdp_udp_flow_get_event(rdpUdpFlow* flow);

/** Timeout (ms) until freerdp_udp_flow_check() must be called; used when
 *  ManualWorker is set. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT32 freerdp_udp_flow_get_timeout(rdpUdpFlow* flow);

/** Processes the flow's tunnel; used when ManualWorker is set.
 *  @return \b FALSE if the tunnel failed */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_flow_check(rdpUdpFlow* flow);

/** Stops the listener thread, terminates relays, and closes sockets. Associated
 *  flows remain valid but stop receiving datagrams. These flows must be freed by
 *  their peers, and the last one to be freed releases the listener's memory. For
 *  child processes after fork(), use freerdp_udp_listener_close_after_fork(). */
FREERDP_LOCAL void freerdp_udp_listener_free(freerdp_udp_listener* listener);

/** Closes the child process's copies of listener and relay sockets after fork().
 *  This function does not send data or stop threads (which only exist in the parent)
 *  and avoids the listener lock to prevent deadlocks if the parent held it during fork().
 *  After calling, do not use or free the listener in the child; memory is reclaimed on
 *  exit or exec(). Call this before other child work; not needed if the child execs
 *  immediately. Passing \b nullptr closes the process-wide listener copies. Events
 *  remain open in the child. */
FREERDP_LOCAL void freerdp_udp_listener_close_after_fork(freerdp_udp_listener* listener);

WINPR_ATTR_MALLOC(freerdp_udp_listener_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL freerdp_udp_listener* freerdp_udp_listener_new(void);

/** Binds one socket per address family.
 *  @param address address to listen on, \b nullptr for any. For 'any', replies use the
 *  client's destination address. Windows does not support this; the routing table
 *  determines the source address, so servers with multiple addresses must open each
 *  explicitly. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_listener_open(freerdp_udp_listener* listener, const char* address,
                                             UINT16 port);

/** Takes ownership of a bound UDP socket (e.g., opened by a privileged parent).
 *  @return \b FALSE if the listener has 32 sockets, fd is not an IPv4 or IPv6 UDP
 *  socket, or the listener cannot wait on it; caller retains ownership of fd. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_listener_open_from_socket(freerdp_udp_listener* listener, int fd);

/** @return number of handles to wait on, or 0 if they exceed count. A listener always
 *  has at least one handle. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL DWORD freerdp_udp_listener_get_event_handles(freerdp_udp_listener* listener,
                                                           HANDLE* events, DWORD count);

/** Reads pending datagrams and dispatches them to flows. Do not call if the
 *  listener is running its own thread. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_listener_check(freerdp_udp_listener* listener);

/** Starts a listener-owned thread to run freerdp_udp_listener_check(). Does nothing if
 *  the thread runs already. Not thread-safe: call it from one thread. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_listener_start(freerdp_udp_listener* listener);

/** Routes the SYN containing the cookie hash, and subsequent datagrams from
 *  that source address, to the flow. Thread-safe.
 *  @return \b FALSE if the cookie hash is already expected or the flow is
 *  already registered with a listener */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_listener_add_flow(freerdp_udp_listener* listener, rdpUdpFlow* flow);

/** Creates a connected socket pair for freerdp_udp_flow_set_socket() and
 *  freerdp_udp_listener_add_relay(). Uses SOCK_SEQPACKET socketpair() or SOCK_DGRAM if
 *  SOCK_SEQPACKET is unavailable (e.g., macOS). On Windows, these are connected loopback
 *  UDP sockets; if one end exits, another local process might bind the port before the
 *  relay ends, allowing it to intercept the traffic. On Windows, handles
 *  are passed as ints (similar to freerdp_peer_new()) as they fit within 32 bits. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_socket_pair(int fds[2]);

/** Relays client traffic through a socket; a single listener thread handles all
 *  relays. Datagrams from the client (identified by cookieHash in SYN) are routed to fd,
 *  and datagrams on fd are sent to the client. Datagrams exceeding fd's buffer are dropped
 *  and resent. The relay takes ownership of fd and terminates when the other end closes.
 *  Thread-safe. The listener does not verify cookieHash (as the cookie is in another
 *  process); only provide hashes from trusted processes. The first relay or flow to
 *  claim a cookie hash owns it. Note that the other process can send datagrams from the
 *  listener's port to the SYN's source address (which may be spoofed). Maximum 256 relays
 *  per listener; call freerdp_udp_listener_remove_relay() upon connection termination.
 *  @param fd from freerdp_udp_socket_pair() (other end to freerdp_udp_flow_set_socket());
 *  must be SOCK_SEQPACKET or SOCK_DGRAM.
 *  @return \b FALSE if cookieHash is already expected or relay limit reached; caller
 *  retains fd, which may have been set non-blocking. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_udp_listener_add_relay(freerdp_udp_listener* listener,
                                                  const BYTE cookieHash[32], int fd);

/** Terminates the relay for cookieHash (e.g., on connection end). The relay thread
 *  closes the socket shortly after, and only then are the cookie hash and the relay's
 *  place among the 256 free again: freerdp_udp_listener_add_relay() can fail until then.
 *  Thread-safe.
 *  @return \b FALSE if no relay exists for cookieHash */
FREERDP_LOCAL BOOL freerdp_udp_listener_remove_relay(freerdp_udp_listener* listener,
                                                     const BYTE cookieHash[32]);

/** Detaches the flow from the listener. Thread-safe. This is also performed by
 *  freerdp_udp_flow_free(). */
FREERDP_LOCAL void freerdp_udp_listener_remove_flow(freerdp_udp_listener* listener,
                                                    rdpUdpFlow* flow);

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
