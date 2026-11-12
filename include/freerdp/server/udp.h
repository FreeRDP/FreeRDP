/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server side UDP multitransport
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

#ifndef FREERDP_SERVER_UDP_H
#define FREERDP_SERVER_UDP_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/peer.h>

/** @file
 *  @brief Control over the UDP side of a server built with -DWITH_RDPEUDP=ON.
 *
 *  A threaded server requires no configuration from this file: each peer offering a tunnel
 *  shares a process-wide listener on the TCP address/port and is assigned a worker thread.
 *  This API replaces that default behavior:
 *
 *  - \b freerdp_udp_listener owns the sockets and demuxes datagrams by client. It runs in
 *  its own thread or via the application's event loop.
 *  - \b rdpUdpFlow represents the datagram path for one client and is the only network
 *  component a tunnel requires.
 *  - \b FreeRDP_UdpPeerCallbacks allows a peer to specify the datagram source for its flow,
 *  track UDP connection progress, and pace transmissions.
 *
 *  Servers that fork per connection or delegate to per-user processes serve clients
 *  outside the process owning the port. Such servers keep the listener in the port-owning
 *  process and use a socket pair per connection via freerdp_udp_socket_pair(). One end of
 *  the pair is passed to the child process:
 *
 *  - In the connection process: OfferTunnel calls freerdp_udp_flow_set_socket() with one
 *    end and passes the other end along with cookieHash.
 *  - In the port-owning process: freerdp_udp_listener_add_relay() handles the remaining setup.
 *
 *  The connection process never accesses the shared socket, preventing it from reading
 *  other users' datagrams. Listener and socket-pair sockets are close-on-exec; a child
 *  that forks without exec must call freerdp_udp_listener_close_after_fork() instead of
 *  freerdp_udp_listener_free(). The relay ends when either socket in the pair closes.
 *  This header is installed only if WITH_RDPEUDP_SERVER is defined in freerdp/config.h.
 *
 *  Datagrams are identified by source address; only the client's SYN ties a datagram to a
 *  connection via a cleartext hash. A UDP connection is unproven until the security cookie
 *  arrives within the TLS session (see FreeRDP_UdpLinkState). A source address remains
 *  bound to its connection until termination; new connections from the same address/port
 *  (e.g., via NAT) remain on TCP until then. The flow binds to the address of the first SYN
 *  and its cookie hash; if the client's NAT assigns a new port, the tunnel ends.
 *
 *  A tunnel allows at most 4096 datagrams in flight (~5 MB per round trip, e.g., 50 MB/s
 *  at 100 ms), though the congestion window may be smaller. Servers exceeding link
 *  capacity must pace themselves using FreeRDP_UdpSendState. */

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_udp_listener freerdp_udp_listener;
	typedef struct rdp_udp_flow rdpUdpFlow;

	/** @brief UDP connection state for a peer
	 *  @since version 3.34.0
	 *  */
	typedef enum
	{
		FREERDP_UDP_LINK_NONE, /**< No UDP connection exists or it failed; the peer remains on TCP.
		                        */
		FREERDP_UDP_LINK_OFFERED, /**< The tunnel is offered via TCP; the flow is now awaiting a SYN
		packet. */
		FREERDP_UDP_LINK_SYN_RECEIVED, /**< A SYN provided the hash, but identity is unproven as the
		                             hash could be intercepted */
		FREERDP_UDP_LINK_VERIFIED, /**< The security cookie has been received within the tunnel's
		                            TLS session. */
		FREERDP_UDP_LINK_READY     /**< Client has also accepted on TCP; the tunnel can now carry
		                               channel data. */
	} FreeRDP_UdpLinkState;

	/** @brief Link state for pacing encoders. Set size to sizeof(FreeRDP_UdpSendState)
	 *  before calling; newer versions may append fields and only fill the requested size.
	 *  @since version 3.34.0 */
	typedef struct
	{
		size_t size;        /**< Set by the caller; see the structure description above. */
		BOOL writeBlocked;  /**< TRUE if queued data exceeds link capacity; see WriteUnblocked. */
		size_t queuedBytes; /**< Number of bytes accepted but not yet sent for the first time */
		UINT32 rttMs;       /**< Smoothed round-trip time; 0 before the first sample. */
		UINT64
		rateBytesPerSecond; /**< The congestion window rate at rttMs; 0 before the first sample is
		                       taken. */
	} FreeRDP_UdpSendState;

	/** @brief Per-peer UDP callbacks. Executed on the thread running the
	 *  freerdp_peer check; OfferTunnel is called during connection setup. Set Size to
	 *  sizeof(FreeRDP_UdpPeerCallbacks); newer versions may append fields and only use
	 *  those covered by Size.
	 *  @since version 3.34.0 */
	typedef struct
	{
		size_t Size; /**< Set by the caller; see the struct description above. */
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
		/** Called when link state changes. If state is FREERDP_UDP_LINK_NONE, cease all
		 *  flow usage before returning. */
		void (*LinkChanged)(void* custom, FreeRDP_UdpLinkState state);
		/** Called when the tunnel can accept more data after writeBlocked was set.
		 *  Note: freerdp_peer's IsWriteBlocked and DrainOutputBuffer only apply to TCP. */
		void (*WriteUnblocked)(void* custom);
		/** If \b TRUE, no worker thread is created. The application must drive the
		 *  tunnel using freerdp_udp_flow_get_event(), freerdp_udp_flow_get_timeout(), and
		 *  freerdp_udp_flow_check() to ensure acks and retransmissions. If driven on a
		 *  non-peer thread, stop before LinkChanged returns FREERDP_UDP_LINK_NONE. */
		BOOL ManualWorker;
	} FreeRDP_UdpPeerCallbacks;

	/** Sets UDP callbacks for a peer. Call after peer context creation and before
	 *  offering the tunnel.
	 *  @param callbacks the hooks, or \b nullptr for default behavior
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_peer_set_udp_callbacks(freerdp_peer* peer,
	                                                const FreeRDP_UdpPeerCallbacks* callbacks,
	                                                void* custom);

	/** Retrieves link state for pacing. Since the reliable tunnel cannot drop
	 *  data, exceeding the queue limit (set via freerdp_peer_set_udp_send_limit())
	 *  will fail the tunnel. The session terminates only if channels have already
	 *  migrated to UDP; otherwise, it continues on TCP.
	 *  @return \b FALSE if no tunnel exists
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_peer_get_udp_send_state(freerdp_peer* peer,
	                                                 FreeRDP_UdpSendState* state);

	/** Sets the maximum bytes a peer's tunnel queues while awaiting the link.
	 *  @param limit the limit, 0 for default
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_peer_set_udp_send_limit(freerdp_peer* peer, size_t limit);

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
	 *  thread in any order.
	 *  @since version 3.34.0 */
	FREERDP_API void freerdp_udp_flow_free(rdpUdpFlow* flow);

	/** Creates a flow for a client served by another process (e.g., a parent
	 *  process in a forking server).
	 *  @since version 3.34.0 */
	WINPR_ATTR_MALLOC(freerdp_udp_flow_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API rdpUdpFlow* freerdp_udp_flow_new(const BYTE cookieHash[32]);

	/** @return the 32-byte cookie hash expected in the client's SYN
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API const BYTE* freerdp_udp_flow_get_cookie_hash(const rdpUdpFlow* flow);

	/** Redirects flow datagrams to the receive callback instead of queuing them for a tunnel.
	 *  Waits for the previous callback to return, so that its \b custom data can be safely
	 *  freed afterwards.
	 *  @since version 3.34.0 */
	FREERDP_API void freerdp_udp_flow_set_receive(rdpUdpFlow* flow,
	                                              FreeRDP_UdpFlowReceiveFn receive, void* custom);

	/** Redirects flow datagrams to the send callback instead of the listener's socket.
	 *  Waits for the previous callback to return, so that its \b custom can be
	 *  freed afterwards.
	 *  @since version 3.34.0 */
	FREERDP_API void freerdp_udp_flow_set_send(rdpUdpFlow* flow, FreeRDP_UdpFlowSendFn send,
	                                           void* custom);

	/** Connects a flow to a datagram socket relayed via
	 *  freerdp_udp_listener_add_relay(). The flow takes ownership of the socket for all I/O.
	 *  @param fd a connected SOCK_SEQPACKET or SOCK_DGRAM socket
	 *  @return \b FALSE if the flow already has a socket or fd is the wrong type; caller
	 *  retains ownership of fd.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_flow_set_socket(rdpUdpFlow* flow, int fd);

	/** Queues a datagram from the client. Thread-safe. If the queue limit is
	 *  exceeded, the datagram is dropped and must be resent by the client.
	 *  @return \b FALSE if dropped
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_flow_deliver(rdpUdpFlow* flow, const BYTE* data, size_t length);

	/** Sends a datagram to the client. Thread-safe.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_flow_send(rdpUdpFlow* flow, const BYTE* data, size_t length);

	/** Returns an event that remains set while datagrams await
	 *  freerdp_udp_flow_check(). For flows using a socket via freerdp_udp_flow_set_socket(),
	 *  this is the socket's event. Re-fetch the event before each wait as it changes when
	 *  the peer closes. Do not manually set/reset the event, as the OS manages it.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API HANDLE freerdp_udp_flow_get_event(rdpUdpFlow* flow);

	/** Timeout (ms) until freerdp_udp_flow_check() must be called; used when
	 *  ManualWorker is set.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 freerdp_udp_flow_get_timeout(rdpUdpFlow* flow);

	/** Processes the flow's tunnel; used when ManualWorker is set.
	 *  @return \b FALSE if the tunnel failed
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_flow_check(rdpUdpFlow* flow);

	/** Stops the listener thread, terminates relays, and closes sockets. Associated
	 *  flows remain valid but stop receiving datagrams. These flows must be freed by
	 *  their peers, and the last one to be freed releases the listener's memory. For
	 *  child processes after fork(), use freerdp_udp_listener_close_after_fork().
	 *  @since version 3.34.0 */
	FREERDP_API void freerdp_udp_listener_free(freerdp_udp_listener* listener);

	/** Closes the child process's copies of listener and relay sockets after fork().
	 *  This function does not send data or stop threads (which only exist in the parent)
	 *  and avoids the listener lock to prevent deadlocks if the parent held it during fork().
	 *  After calling, do not use or free the listener in the child; memory is reclaimed on
	 *  exit or exec(). Call this before other child work; not needed if the child execs
	 *  immediately. Passing \b nullptr closes the process-wide listener copies. Events
	 *  remain open in the child.
	 *  @since version 3.34.0 */
	FREERDP_API void freerdp_udp_listener_close_after_fork(freerdp_udp_listener* listener);

	/** @since version 3.34.0 */
	WINPR_ATTR_MALLOC(freerdp_udp_listener_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API freerdp_udp_listener* freerdp_udp_listener_new(void);

	/** Binds one socket per address family.
	 *  @param address address to listen on, \b nullptr for any. For 'any', replies use the
	 *  client's destination address. Windows does not support this; the routing table
	 *  determines the source address, so servers with multiple addresses must open each
	 *  explicitly.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_open(freerdp_udp_listener* listener, const char* address,
	                                           UINT16 port);

	/** Takes ownership of a bound UDP socket (e.g., opened by a privileged parent).
	 *  @return \b FALSE if the listener has 32 sockets, fd is not an IPv4 or IPv6 UDP
	 *  socket, or the listener cannot wait on it; caller retains ownership of fd.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_open_from_socket(freerdp_udp_listener* listener, int fd);

	/** @return number of handles to wait on, or 0 if they exceed count. A listener always
	 *  has at least one handle.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API DWORD freerdp_udp_listener_get_event_handles(freerdp_udp_listener* listener,
	                                                         HANDLE* events, DWORD count);

	/** Reads pending datagrams and dispatches them to flows. Do not call if the
	 *  listener is running its own thread.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_check(freerdp_udp_listener* listener);

	/** Starts a listener-owned thread to run freerdp_udp_listener_check(). Does nothing if
	 *  the thread runs already. Not thread-safe: call it from one thread.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_start(freerdp_udp_listener* listener);

	/** Routes the SYN containing the cookie hash, and subsequent datagrams from
	 *  that source address, to the flow. Thread-safe.
	 *  @return \b FALSE if the cookie hash is already expected or the flow is
	 *  already registered with a listener
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_add_flow(freerdp_udp_listener* listener,
	                                               rdpUdpFlow* flow);

	/** Creates a connected socket pair for freerdp_udp_flow_set_socket() and
	 *  freerdp_udp_listener_add_relay(). Uses SOCK_SEQPACKET socketpair() or SOCK_DGRAM if
	 *  SOCK_SEQPACKET is unavailable (e.g., macOS). On Windows, these are connected loopback
	 *  UDP sockets; if one end exits, another local process might bind the port before the
	 *  relay ends, allowing it to intercept the traffic. On Windows, handles
	 *  are passed as ints (similar to freerdp_peer_new()) as they fit within 32 bits.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_socket_pair(int fds[2]);

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
	 *  retains fd, which may have been set non-blocking.
	 *  @since version 3.34.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_add_relay(freerdp_udp_listener* listener,
	                                                const BYTE cookieHash[32], int fd);

	/** Terminates the relay for cookieHash (e.g., on connection end). The relay thread
	 *  closes the socket shortly after, and only then are the cookie hash and the relay's
	 *  place among the 256 free again: freerdp_udp_listener_add_relay() can fail until then.
	 *  Thread-safe.
	 *  @return \b FALSE if no relay exists for cookieHash
	 *  @since version 3.34.0 */
	FREERDP_API BOOL freerdp_udp_listener_remove_relay(freerdp_udp_listener* listener,
	                                                   const BYTE cookieHash[32]);

	/** Detaches the flow from the listener. Thread-safe. This is also performed by
	 *  freerdp_udp_flow_free().
	 *  @since version 3.34.0 */
	FREERDP_API void freerdp_udp_listener_remove_flow(freerdp_udp_listener* listener,
	                                                  rdpUdpFlow* flow);

#ifdef __cplusplus
}
#endif

#endif /* End of FREERDP_SERVER_UDP_H */
