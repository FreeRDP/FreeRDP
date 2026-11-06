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
 *  @brief Control of the UDP side of a server that you build with -DWITH_RDPEUDP=ON.
 *
 *  A threaded server needs nothing from this file. A peer that offers the tunnel gets a process
 *  wide listener on the address and port of its TCP connection. The peer also gets its own worker
 *  thread. All items in this file replace a part of that:
 *
 *  - \b freerdp_udp_listener owns sockets and sorts datagrams by client. It runs on its own thread,
 *  or on the thread of the application through its event handles.
 *  - \b rdpUdpFlow is the path of one client for datagrams. A tunnel needs only this from the
 *  network.
 *  - \b FreeRDP_UdpPeerCallbacks lets a peer decide where its flow gets datagrams from. It also
 *  lets the peer follow how far the UDP connection got, and pace what it sends.
 *
 *  Some servers serve their clients in processes other than the process that owns the port. The
 *  reason is that they fork per connection or give connections to a process per user. Such a server
 *  keeps the listener in the process with the port. It needs only a pair of sockets per connection
 *  from freerdp_udp_socket_pair(). One of these sockets gets to the other process by the method
 *  that the server uses to pass sockets:
 *
 *  - in the process with the connection, OfferTunnel calls freerdp_udp_flow_set_socket() with one
 *  end, and passes on the other end and cookieHash
 *  - in the process with the port, freerdp_udp_listener_relay() takes both and does the rest
 *
 *  The process with the connection never gets the shared socket. Because of this, it cannot read
 *  the datagrams of other users. When a socket of the pair closes, the relay ends. The installation
 *  includes this file only with -DWITH_RDPEUDP=ON. This tells you if a FreeRDP can do this.
 *
 *  A datagram shows what client it is from only by its source address. Only the SYN of the client
 *  shows what connection that client belongs to. The SYN does this with a hash that travels in the
 *  clear. Because of this, a UDP connection stays unproven until the security cookie arrives inside
 *  its TLS session, see FreeRDP_UdpLinkState. */

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_udp_listener freerdp_udp_listener;
	typedef struct rdp_udp_flow rdpUdpFlow;

	/** @brief How far the UDP connection of a peer got
	 *  @since version 3.33.0
	 */
	typedef enum
	{
		FREERDP_UDP_LINK_NONE,    /**< no UDP connection, or it failed: the peer stays on TCP */
		FREERDP_UDP_LINK_OFFERED, /**< the peer sent the cookie on TCP, the flow waits for a SYN */
		FREERDP_UDP_LINK_CLAIMED, /**< a SYN gave the hash. All who saw it can send it: no proof */
		FREERDP_UDP_LINK_PROVEN,  /**< the cookie itself arrived inside the tunnel's TLS session */
		FREERDP_UDP_LINK_LINKED   /**< client also accepted on TCP: tunnel can carry channel data */
	} FreeRDP_UdpLinkState;

	/** @brief What the tunnel of a peer knows about the link, to pace an encoder
	 *  @since version 3.33.0 */
	typedef struct
	{
		BOOL writeBlocked;  /**< more waits than the link takes, see WriteUnblocked */
		size_t queuedBytes; /**< accepted and not yet on the network for the first time */
		UINT32 rttMs;       /**< smoothed round trip time, 0 before the first sample */
		UINT64
		rateBytesPerSecond; /**< congestion window rate at rttMs, 0 before the first sample */
	} FreeRDP_UdpSendState;

	/** @brief Hooks of one peer. They run on the thread that runs the check of freerdp_peer.
	 *  OfferTunnel also runs on this thread, but in the connection sequence.
	 *  @since version 3.33.0 */
	typedef struct
	{
		/** The peer will offer the tunnel. flow must get the datagrams of the client whose SYN
		 *  carries cookieHash. This hook runs before the peer sends the offer, so no SYN can come
		 *  first. Because of this, the hook must wait until the thing it asks for is in place.
		 *  Without this hook the process wide listener does this work, and that listener serves one
		 *  process only. \b flow belongs to the peer and stays valid until the link is back at
		 *  FREERDP_UDP_LINK_NONE.
		 *  @return \b FALSE to stay on TCP */
		WINPR_ATTR_NODISCARD BOOL (*OfferTunnel)(void* custom, rdpUdpFlow* flow,
		                                         const BYTE cookieHash[32]);
		/** The link reached state. */
		void (*LinkChanged)(void* custom, FreeRDP_UdpLinkState state);
		/** The tunnel has room again after freerdp_peer's IsWriteBlocked reported it had none. */
		void (*WriteUnblocked)(void* custom);
		/** \b TRUE: no worker thread for this peer. The application runs the tunnel with
		 *  freerdp_udp_flow_get_event(), freerdp_udp_flow_get_timeout() and
		 *  freerdp_udp_flow_check(). It must do this while it waits for all other things, because
		 *  otherwise nobody acknowledges or resends. */
		BOOL ManualWorker;
	} FreeRDP_UdpPeerCallbacks;

	/** Sets the hooks of a peer, after its context exists and before it offers the tunnel.
	 *  @param callbacks the hooks, or \b nullptr for the default behaviour
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_peer_set_udp_callbacks(freerdp_peer* peer,
	                                                const FreeRDP_UdpPeerCallbacks* callbacks,
	                                                void* custom);

	/** A server that will send a lot uses this to pace itself. A reliable tunnel cannot drop data.
	 *  Because of this, a server that continues to send more than the link takes ends its session.
	 *  freerdp_peer_set_udp_send_limit() sets the limit.
	 *  @return \b FALSE without a tunnel
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_peer_get_udp_send_state(freerdp_peer* peer,
	                                                 FreeRDP_UdpSendState* state);

	/** The maximum number of bytes that a tunnel of this peer keeps while they wait for the link.
	 *  @param limit the limit, 0 for the default
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_peer_set_udp_send_limit(freerdp_peer* peer, size_t limit);

	/** @brief Gets one datagram of a flow that gives its datagrams to this function, see
	 *  freerdp_udp_flow_set_receive(). It runs with the listener locked, so it must not block or
	 *  call the listener.
	 *  @return \b FALSE if it did not take the datagram. That only loses the datagram. */
	typedef BOOL (*FreeRDP_UdpFlowReceiveFn)(void* custom, const BYTE* data, size_t length);

	/** @brief Sends one datagram to the client of a flow.
	 *  @return \b FALSE for an error that ends the connection. A datagram that the network lost is
	 *  not an error, because RDP-UDP resends it. */
	typedef BOOL (*FreeRDP_UdpFlowSendFn)(void* custom, const BYTE* data, size_t length);

	/** Frees a flow that freerdp_udp_flow_new() made. A flow of a peer belongs to that peer.
	 *  @since version 3.33.0 */
	FREERDP_API void freerdp_udp_flow_free(rdpUdpFlow* flow);

	/** A flow for a client that another process serves. For example, the parent of a server that
	 *  forks keeps one such flow per child.
	 *  @since version 3.33.0 */
	WINPR_ATTR_MALLOC(freerdp_udp_flow_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API rdpUdpFlow* freerdp_udp_flow_new(const BYTE cookieHash[32]);

	/** @return the 32 byte hash that the client of the flow sends in its SYN
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API const BYTE* freerdp_udp_flow_get_cookie_hash(const rdpUdpFlow* flow);

	/** Gives the datagrams of a flow to receive. The flow does not keep them for a tunnel.
	 *  @since version 3.33.0 */
	FREERDP_API void freerdp_udp_flow_set_receive(rdpUdpFlow* flow,
	                                              FreeRDP_UdpFlowReceiveFn receive, void* custom);

	/** Sends the datagrams of a flow with send, not on the socket of the listener.
	 *  @since version 3.33.0 */
	FREERDP_API void freerdp_udp_flow_set_send(rdpUdpFlow* flow, FreeRDP_UdpFlowSendFn send,
	                                           void* custom);

	/** Connects a flow to a datagram socket with a relay at its other end, see
	 *  freerdp_udp_listener_relay(). The flow takes ownership of the socket. It reads the datagrams
	 *  of the client from the socket, and sends on it.
	 *  @param fd a connected datagram socket
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_flow_set_socket(rdpUdpFlow* flow, int fd);

	/** Gives a flow one datagram from its client. Thread safe. A flow keeps only a limited number
	 *  of datagrams. Past that limit the datagram is lost, and the client resends it.
	 *  @return \b FALSE if the datagram was lost
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_flow_input(rdpUdpFlow* flow, const BYTE* data, size_t length);

	/** Sends one datagram to the client of a flow, as the tunnel of that flow does. Thread safe.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_flow_send(rdpUdpFlow* flow, const BYTE* data, size_t length);

	/** This event stays set while datagrams wait for freerdp_udp_flow_check().
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API HANDLE freerdp_udp_flow_get_event(rdpUdpFlow* flow);

	/** Milliseconds until freerdp_udp_flow_check() must run, for FreeRDP_UdpPeerCallbacks
	 *  ManualWorker.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 freerdp_udp_flow_get_timeout(rdpUdpFlow* flow);

	/** Runs the tunnel of the flow, for FreeRDP_UdpPeerCallbacks ManualWorker.
	 *  @return \b FALSE after the tunnel failed
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_flow_check(rdpUdpFlow* flow);

	/** Stops the thread of the listener, closes its sockets and releases its flows.
	 *  @since version 3.33.0 */
	FREERDP_API void freerdp_udp_listener_free(freerdp_udp_listener* listener);

	/** @since version 3.33.0 */
	WINPR_ATTR_MALLOC(freerdp_udp_listener_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API freerdp_udp_listener* freerdp_udp_listener_new(void);

	/** Binds a socket per address family.
	 *  @param address the address to listen on, \b nullptr for any. A server with several addresses
	 *  must open each address. Replies on an unbound socket leave from the address that the routing
	 *  table selects. That address can be different from the address that the client sent to.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_open(freerdp_udp_listener* listener, const char* address,
	                                           UINT16 port);

	/** Takes ownership of a bound UDP socket, for example a socket that a privileged parent opened.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_open_from_socket(freerdp_udp_listener* listener, int fd);

	/** @return the number of handles to wait on, 0 if they do not fit into count
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API DWORD freerdp_udp_listener_get_event_handles(freerdp_udp_listener* listener,
	                                                         HANDLE* events, DWORD count);

	/** Reads the data that waits on the sockets and sorts it into the flows. Do not use this for a
	 *  listener that runs its own thread.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_check(freerdp_udp_listener* listener);

	/** Runs freerdp_udp_listener_check() on a thread of the listener.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_start(freerdp_udp_listener* listener);

	/** Sends to flow the SYN that carries the cookie hash of flow. Also sends to flow what comes
	 *  from the address of that SYN after it. Thread safe.
	 *  @return \b FALSE if the listener already expects that cookie hash
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_expect(freerdp_udp_listener* listener, rdpUdpFlow* flow);

	/** Makes a pair of connected datagram sockets for freerdp_udp_flow_set_socket() and
	 *  freerdp_udp_listener_relay(). This is a socketpair() of type SOCK_SEQPACKET, or on Windows
	 *  two connected loopback sockets.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_socket_pair(int fds[2]);

	/** Serves a client through a socket, on a thread of its own per relay. What the client whose
	 *  SYN carries cookieHash sends goes to fd, and what arrives on fd goes to that client. A
	 *  datagram that does not fit into the buffer of fd is lost, and resent like any other
	 *  datagram. The relay takes ownership of fd. It ends when the other end is freed, or closes.
	 *  Thread safe.
	 *  @param fd from freerdp_udp_socket_pair(), freerdp_udp_flow_set_socket() has the other end
	 *  @return \b FALSE if the listener already expects cookieHash. The caller then keeps fd.
	 *  @since version 3.33.0 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_udp_listener_relay(freerdp_udp_listener* listener,
	                                            const BYTE cookieHash[32], int fd);

	/** After this returns, the listener does not touch flow again. Thread safe. When you free a
	 *  flow, it also does this.
	 *  @since version 3.33.0 */
	FREERDP_API void freerdp_udp_listener_forget(freerdp_udp_listener* listener, rdpUdpFlow* flow);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_UDP_H */
