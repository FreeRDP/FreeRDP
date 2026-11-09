/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server UDP listener: internals shared by its source files
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

#ifndef FREERDP_LIB_CORE_UDPLISTENER_INTERNAL_H
#define FREERDP_LIB_CORE_UDPLISTENER_INTERNAL_H

#include <winpr/wtypes.h>
#include <winpr/synch.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <freerdp/api.h>

#include "udplistener.h"

/* Listener implementation is split across these files:
 * - udpsocket.c: platform-specific socket operations to avoid #if in other files.
 * - udpflow.c: per-client datagram path (see rdpUdpFlow in include/freerdp/server/udp.h),
 *   including the receive queue and sending logic.
 * - udplistener.c: socket and flow table management, datagram dispatch, and the
 *   process-wide listener thread.
 * - udplistener_relay.c: relays datagrams to other processes (e.g. after fork()) using
 *   socket pairs and a dedicated relay thread. */

/* Relay endpoints may close at any time; must prevent SIGPIPE */
#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif
#if !defined(MSG_DONTWAIT)
#define MSG_DONTWAIT 0
#endif
#if defined(SOCK_CLOEXEC)
#define UDPLISTENER_SOCK_CLOEXEC SOCK_CLOEXEC
#else
#define UDPLISTENER_SOCK_CLOEXEC 0
#endif
#if !defined(SHUT_RDWR)
#define SHUT_RDWR SD_BOTH
#endif

/* Shared receive buffer for all sessions (similar to RDPUDP_RECEIVE_BUFFER). */
#define UDPLISTENER_RECEIVE_BUFFER (4 * 1024 * 1024)

/* Max sockets per listener. The listener thread polls these plus one internal event. */
#define UDPLISTENER_MAX_SOCKETS 32

/* Max datagrams read per socket per pass to prevent starvation. Relay thread uses the
 * same budget per relay. */
#define UDPLISTENER_READ_BUDGET 256

/* Queue size for datagrams awaiting the tunnel. Sufficient for ACKs and input if the
 * tunnel keeps up. Excess datagrams are dropped to prevent a flooding client from
 * impacting other flows. */
#define UDPFLOW_SLOTS 256

/* Initial ring capacity, allocated on the first datagram. Doubles when full up to
 * UDPFLOW_SLOTS to minimize memory for inactive flows. */
#define UDPFLOW_FIRST_SLOTS 8

/* Max relays per listener. Each relay consumes a file descriptor; limited to prevent
 * descriptor exhaustion (typical limit is 1024). */
#define UDPLISTENER_MAX_RELAYS 256

/* The local address the client sent datagrams to */
typedef struct
{
	BOOL valid;
	int family;
	struct in_addr ipv4;
	struct in6_addr ipv6;
	UINT32 interfaceIndex;
} udpLocalAddress;

struct rdp_udp_flow
{
	CRITICAL_SECTION lock;
	BYTE cookieHash[32];

	/* Event is set when datagrams are pending; protected by lock */
	HANDLE event;
	BYTE*
	    ring; /* Allocated upon the first datagram to avoid memory overhead for inactive clients. */
	size_t slots;
	UINT16 lengths[UDPFLOW_SLOTS];
	size_t head;
	size_t count;

	/* Client address, reached via one of the listener's sockets */
	SOCKET sockfd;
	struct sockaddr_storage peer;
	size_t peerLength;
	udpLocalAddress local; /* Local address used for sending replies */

	/* Socket to a relay, owned by the flow */
	SOCKET relay;
	HANDLE relayEvent;
	BOOL relayClosed; /* Indicates the remote end closed, meaning the socket will remain readable
	                     indefinitely. */

	FreeRDP_UdpFlowReceiveFn receive;
	void* receiveCustom;
	FreeRDP_UdpFlowSendFn send;
	void* sendCustom;

	BOOL (*check)(void* custom);
	UINT32 (*timeout)(void* custom);
	void* workerCustom;

	freerdp_udp_listener* listener; /* Set under the listener's lock */
};

typedef struct
{
	rdpUdpFlow* flow;
	SOCKET sockfd; /* The socket on which the SYN must arrive; INVALID_SOCKET allows any socket. */
	BOOL bound;    /* TRUE if a SYN packet has arrived from the address in 'addr' */
	struct sockaddr_storage addr;
	size_t addrlen;
} udpListenerEntry;

typedef struct
{
	struct sockaddr_storage addr; /* The address the socket is bound to */
	size_t addrlen;
	SOCKET sockfd;
	HANDLE event;
	BOOL localAddress; /* Indicates whether each datagram reports the address the client sent to */
} udpListenerSocket;

typedef struct s_udp_listener_relay
{
	struct s_udp_listener_relay* next;
	freerdp_udp_listener* listener;
	rdpUdpFlow* flow;
	SOCKET sockfd;
	LONG ending; /* Signals the relay thread to terminate this relay; access via Interlocked
	                functions. */
} udpListenerRelay;

#if defined(_WIN32)
typedef WSAPOLLFD udpPollFd;
#else
typedef struct pollfd udpPollFd;
#endif

struct rdp_udp_listener
{
	CRITICAL_SECTION lock;

	udpListenerSocket sockets[UDPLISTENER_MAX_SOCKETS];
	size_t socketCount;

	/* All entries are owned by entriesByHash; added to entriesByAddress after SYN arrives. */
	wHashTable* entriesByHash;
	wHashTable* entriesByAddress;

	/* Single thread serving all relays */
	udpListenerRelay* relays;
	size_t relayCount;
	HANDLE relayThread;
	LONG relayStop;
	LONG
	    relayFailed; /* Relay thread stopped due to an error; access using Interlocked functions. */
	SOCKET relayWake[2]; /* Write to [0] to wake the thread waiting on [1]. */

	HANDLE thread;
	HANDLE wake;
	LONG stop;

	BOOL shared; /* Process-wide listener that closes when the last flow is closed. */
	BOOL freed;  /* freerdp_udp_listener_free() was called; the last flow removed releases it. */

	BYTE buffer[0x10000];
};

/* Implementation in udpsocket.c */

/** Returns TRUE if a and b have the same address and port. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udpsocket_address_equal(const struct sockaddr_storage* a,
                                           const struct sockaddr* b, size_t blen);

/** Converts v4-mapped addresses. IPv4 clients on dual-stack TCP sockets appear as
 *  ::ffff:a.b.c.d, but UDP arrives via IPv4. */
FREERDP_LOCAL void udpsocket_unmap_address(const struct sockaddr* addr, size_t addrlen,
                                           struct sockaddr_storage* out, size_t* outlen);

/** Creates a socket that is not inherited by child processes. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL SOCKET udpsocket_new(int family, int type, int protocol);

/** Prevents child processes from inheriting the specified socket. */
FREERDP_LOCAL void udpsocket_set_no_inherit(SOCKET sockfd);

/** Creates a UDP listener socket bound to addr.
 *  @return INVALID_SOCKET on failure */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL SOCKET udpsocket_bind(const struct sockaddr* addr, size_t addrlen);

/** Prevents listener socket from reporting ICMP errors from previous replies. */
FREERDP_LOCAL void udpsocket_ignore_icmp_errors(SOCKET sockfd);

/** Returns TRUE if the socket preserves datagram boundaries (e.g., SOCK_SEQPACKET or
 *  SOCK_DGRAM). */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udpsocket_is_message_socket(SOCKET sockfd);

/** Enables reporting of each datagram's destination address; see udpsocket_receive(). */
FREERDP_LOCAL void udpsocket_enable_local_address(udpListenerSocket* sock);

/** Receives one datagram. For sockets bound to INADDR_ANY, also returns the local
 *  address the client sent to.
 *  @return datagram length, or a negative value if no data is pending */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL int udpsocket_receive(const udpListenerSocket* sock, BYTE* data, size_t size,
                                    struct sockaddr_storage* addr, int* addrlen,
                                    udpLocalAddress* local);

/** Sends one datagram from the specified local address.
 *  @return FALSE if the send failed */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udpsocket_send_from(SOCKET sockfd, const struct sockaddr_storage* to,
                                       size_t tolen, const udpLocalAddress* local, const BYTE* data,
                                       size_t length);

/** Called after udpsocket_send_from() fails. Returns TRUE if the source address is no
 *  longer available and the system must choose one.
 *  @param error the send error */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udpsocket_source_address_unavailable(int* error);

/** Called after a relay socket send fails. Returns TRUE if the peer is gone. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udpsocket_peer_closed(void);

/** Creates a connected socket pair that preserves datagram boundaries. Used by the
 *  relay thread for self-wakeups. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udpsocket_message_pair(SOCKET pair[2]);

FREERDP_LOCAL void udpsocket_poll_set(udpPollFd* fd, SOCKET sockfd);

/** Waits until one of the fds is ready. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL int udpsocket_poll(udpPollFd* fds, size_t count);

/* Implementation in udplistener.c */

/** Adds a flow to the listener's tables.
 *  @param sockfd the socket the flow's SYN must arrive on; INVALID_SOCKET for any */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL udplistener_add_flow(freerdp_udp_listener* listener, rdpUdpFlow* flow,
                                        SOCKET sockfd);

/* Functions implemented in udplistener_relay.c */

/** Stops the relay thread and terminates all active relays. */
FREERDP_LOCAL void udplistener_stop_relays(freerdp_udp_listener* listener);

/** Closes relay sockets after fork, similar to freerdp_udp_listener_close_after_fork(). */
FREERDP_LOCAL void udplistener_relays_close_after_fork(freerdp_udp_listener* listener);

#endif /* End of FREERDP_LIB_CORE_UDPLISTENER_INTERNAL_H */
