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

#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/interlocked.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

/* the other end of a relay can close at any time. This must not raise a signal. */
#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif
#if !defined(MSG_DONTWAIT)
#define MSG_DONTWAIT 0
#endif
#if !defined(SHUT_RDWR)
#define SHUT_RDWR SD_BOTH
#endif

#include <freerdp/log.h>

#include "rdpudp.h"
#include "udplistener.h"

#define TAG FREERDP_TAG("core.udplistener")

/* Same as for a client connection, see RDPUDP_RECEIVE_BUFFER. Here it takes the data of every
 * session. */
#define UDPLISTENER_RECEIVE_BUFFER (4 * 1024 * 1024)

/* Sockets of a listener. Its thread waits on them and on one event of its own. */
#define UDPLISTENER_MAX_SOCKETS 32

/* Datagrams read from a socket at a time, so that one busy socket does not starve the others. */
#define UDPLISTENER_READ_BUDGET 256

/* Datagrams that a flow keeps for its tunnel. A client mostly sends acknowledgements and input, so
 * this is more than sufficient while the tunnel keeps up. Past this number datagrams are lost, as
 * on the network. Because of this, a client that floods the server costs nobody else anything. */
#define UDPFLOW_SLOTS 256

struct rdp_udp_flow
{
	CRITICAL_SECTION lock;
	BYTE cookieHash[32];

	/* set while datagrams wait. Change all of these only when you hold lock */
	HANDLE event;
	BYTE* ring; /* the first datagram makes it: an absent client costs nothing */
	UINT16 lengths[UDPFLOW_SLOTS];
	size_t head;
	size_t count;

	/* where the client is, on a listener's socket */
	SOCKET sockfd;
	struct sockaddr_storage peer;
	size_t peerLength;

	/* or the socket to a relay, which belongs to the flow */
	SOCKET relay;
	HANDLE relayEvent;
	BOOL relayClosed; /* the other end closed: the socket reads as ready for ever */

	FreeRDP_UdpFlowReceiveFn receive;
	void* receiveCustom;
	FreeRDP_UdpFlowSendFn send;
	void* sendCustom;

	BOOL (*check)(void* custom);
	UINT32 (*timeout)(void* custom);
	void* workerCustom;

	freerdp_udp_listener* listener; /* change this only when you hold the listener's lock */
};

typedef struct
{
	rdpUdpFlow* flow;
	SOCKET sockfd; /* the flow waits on this socket, INVALID_SOCKET for any */
	BOOL bound;    /* the SYN came, from addr */
	struct sockaddr_storage addr;
	size_t addrlen;
} udpListenerEntry;

typedef struct
{
	struct sockaddr_storage addr; /* what it is bound to, if the listener bound it */
	size_t addrlen;
	SOCKET sockfd;
	HANDLE event;
} udpListenerSocket;

typedef struct s_udp_listener_relay
{
	struct s_udp_listener_relay* next;
	freerdp_udp_listener* listener;
	rdpUdpFlow* flow;
	SOCKET sockfd;
	HANDLE thread;
	LONG done; /* the thread no longer uses the flow and the listener */
} udpListenerRelay;

struct rdp_udp_listener
{
	CRITICAL_SECTION lock;

	udpListenerSocket sockets[UDPLISTENER_MAX_SOCKETS];
	size_t socketCount;
	udpListenerEntry* entries;
	size_t entryCount;
	udpListenerRelay* relays;

	HANDLE thread;
	HANDLE wake;
	LONG stop;

	BOOL shared; /* the process wide one, which closes with its last flow */

	BYTE buffer[0x10000];
};

static freerdp_udp_listener* g_listener;
static CRITICAL_SECTION g_listenerLock;
static INIT_ONCE g_listenerOnce = INIT_ONCE_STATIC_INIT;

WINPR_ATTR_NODISCARD
static BOOL CALLBACK udplistener_init(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                      WINPR_ATTR_UNUSED PVOID param,
                                      WINPR_ATTR_UNUSED PVOID* context)
{
	return InitializeCriticalSectionAndSpinCount(&g_listenerLock, 4000);
}

WINPR_ATTR_NODISCARD
static BOOL sockaddr_equal(const struct sockaddr_storage* a, const struct sockaddr* b, size_t blen)
{
	WINPR_ASSERT(a);
	WINPR_ASSERT(b);
	if ((blen < sizeof(b->sa_family)) || (a->ss_family != b->sa_family))
		return FALSE;
	if (a->ss_family == AF_INET)
	{
		const struct sockaddr_in* x = (const struct sockaddr_in*)a;
		const struct sockaddr_in* y = (const struct sockaddr_in*)b;
		return (blen >= sizeof(*y)) && (x->sin_port == y->sin_port) &&
		       (memcmp(&x->sin_addr, &y->sin_addr, sizeof(x->sin_addr)) == 0);
	}
	if (a->ss_family == AF_INET6)
	{
		const struct sockaddr_in6* x = (const struct sockaddr_in6*)a;
		const struct sockaddr_in6* y = (const struct sockaddr_in6*)b;
		return (blen >= sizeof(*y)) && (x->sin6_port == y->sin6_port) &&
		       (memcmp(&x->sin6_addr, &y->sin6_addr, sizeof(x->sin6_addr)) == 0);
	}
	return FALSE;
}

rdpUdpFlow* freerdp_udp_flow_new(const BYTE cookieHash[32])
{
	WINPR_ASSERT(cookieHash);

	rdpUdpFlow* flow = calloc(1, sizeof(rdpUdpFlow));
	if (!flow)
		return nullptr;
	if (!InitializeCriticalSectionAndSpinCount(&flow->lock, 4000))
	{
		free(flow);
		return nullptr;
	}
	flow->event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!flow->event)
	{
		DeleteCriticalSection(&flow->lock);
		free(flow);
		return nullptr;
	}
	memcpy(flow->cookieHash, cookieHash, sizeof(flow->cookieHash));
	flow->sockfd = INVALID_SOCKET;
	flow->relay = INVALID_SOCKET;
	return flow;
}

void freerdp_udp_flow_free(rdpUdpFlow* flow)
{
	if (!flow)
		return;

	if (flow->listener)
		freerdp_udp_listener_forget(flow->listener, flow);

	if (flow->relay != INVALID_SOCKET)
	{
		/* a socket without connections shows no end by itself */
		if (_send(flow->relay, "", 0, MSG_NOSIGNAL) < 0)
			WLog_DBG(TAG, "the relay of a flow is gone already");
		closesocket(flow->relay);
		if (!WSACloseEvent(flow->relayEvent))
			WLog_WARN(TAG, "cannot close a socket's event");
	}
	(void)CloseHandle(flow->event);
	DeleteCriticalSection(&flow->lock);
	free(flow->ring);
	free(flow);
}

const BYTE* freerdp_udp_flow_get_cookie_hash(const rdpUdpFlow* flow)
{
	WINPR_ASSERT(flow);
	return flow->cookieHash;
}

void freerdp_udp_flow_set_receive(rdpUdpFlow* flow, FreeRDP_UdpFlowReceiveFn receive, void* custom)
{
	WINPR_ASSERT(flow);
	EnterCriticalSection(&flow->lock);
	flow->receive = receive;
	flow->receiveCustom = custom;
	LeaveCriticalSection(&flow->lock);
}

void freerdp_udp_flow_set_send(rdpUdpFlow* flow, FreeRDP_UdpFlowSendFn send, void* custom)
{
	WINPR_ASSERT(flow);
	EnterCriticalSection(&flow->lock);
	flow->send = send;
	flow->sendCustom = custom;
	LeaveCriticalSection(&flow->lock);
}

BOOL freerdp_udp_flow_set_socket(rdpUdpFlow* flow, int fd)
{
	WINPR_ASSERT(flow);

	HANDLE event = WSACreateEvent();
	if (!event)
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&flow->lock);
	/* this also makes the socket non blocking */
	if ((flow->relay == INVALID_SOCKET) && (WSAEventSelect((SOCKET)fd, event, FD_READ) == 0))
	{
		flow->relay = (SOCKET)fd;
		flow->relayEvent = event;
		rc = TRUE;
	}
	LeaveCriticalSection(&flow->lock);

	if (!rc && !WSACloseEvent(event))
		WLog_WARN(TAG, "cannot close a socket's event");
	return rc;
}

BOOL freerdp_udp_flow_input(rdpUdpFlow* flow, const BYTE* data, size_t length)
{
	WINPR_ASSERT(flow);
	WINPR_ASSERT(data || (length == 0));

	if (length > UDPFLOW_MAX_DATAGRAM)
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&flow->lock);
	if (!flow->ring)
		flow->ring = malloc(1ull * UDPFLOW_SLOTS * UDPFLOW_MAX_DATAGRAM);
	if (flow->ring && (flow->count < UDPFLOW_SLOTS))
	{
		const size_t slot = (flow->head + flow->count) % UDPFLOW_SLOTS;
		if (length > 0)
			memcpy(&flow->ring[slot * UDPFLOW_MAX_DATAGRAM], data, length);
		flow->lengths[slot] = WINPR_ASSERTING_INT_CAST(UINT16, length);
		flow->count++;
		(void)SetEvent(flow->event);
		rc = TRUE;
	}
	LeaveCriticalSection(&flow->lock);
	return rc;
}

BOOL udpflow_read(rdpUdpFlow* flow, BYTE* data, size_t* length)
{
	WINPR_ASSERT(flow);
	WINPR_ASSERT(data);
	WINPR_ASSERT(length);

	BOOL rc = FALSE;
	EnterCriticalSection(&flow->lock);
	if (flow->count > 0)
	{
		*length = flow->lengths[flow->head];
		memcpy(data, &flow->ring[flow->head * UDPFLOW_MAX_DATAGRAM], *length);
		flow->head = (flow->head + 1) % UDPFLOW_SLOTS;
		flow->count--;
		rc = TRUE;
	}
	if (flow->count == 0)
		(void)ResetEvent(flow->event);

	if (!rc && (flow->relay != INVALID_SOCKET) && !flow->relayClosed)
	{
		(void)WSAResetEvent(flow->relayEvent);
		const int got = _recv(flow->relay, (char*)data, UDPFLOW_MAX_DATAGRAM, 0);
		if (got > 0)
		{
			*length = WINPR_ASSERTING_INT_CAST(size_t, got);
			rc = TRUE;
		}
		else if (got == 0)
		{
			/* the timers find that the client is out of reach */
			WLog_DBG(TAG, "the relay of a flow closed");
			flow->relayClosed = TRUE;
		}
	}
	LeaveCriticalSection(&flow->lock);
	return rc;
}

void udpflow_wake(rdpUdpFlow* flow)
{
	WINPR_ASSERT(flow);
	(void)SetEvent(flow->event);
}

void udpflow_set_worker(rdpUdpFlow* flow, BOOL (*check)(void* custom),
                        UINT32 (*timeout)(void* custom), void* custom)
{
	WINPR_ASSERT(flow);
	flow->check = check;
	flow->timeout = timeout;
	flow->workerCustom = custom;
}

BOOL freerdp_udp_flow_send(rdpUdpFlow* flow, const BYTE* data, size_t length)
{
	WINPR_ASSERT(flow);
	WINPR_ASSERT(data);

	EnterCriticalSection(&flow->lock);
	const FreeRDP_UdpFlowSendFn sendFn = flow->send;
	void* custom = flow->sendCustom;
	BOOL rc = FALSE;
	/* hold the lock, because the listener can close its sockets during the send */
	if (!sendFn && (flow->relay != INVALID_SOCKET))
	{
		const int sent = _send(flow->relay, (const char*)data,
		                       WINPR_ASSERTING_INT_CAST(int, length), MSG_NOSIGNAL);
		rc = (sent >= 0) || rdpudp_socket_error_is_transient(WSAGetLastError());
	}
	else if (!sendFn && (flow->sockfd != INVALID_SOCKET))
	{
		const int sent = _sendto(
		    flow->sockfd, (const char*)data, WINPR_ASSERTING_INT_CAST(int, length), 0,
		    (const struct sockaddr*)&flow->peer, WINPR_ASSERTING_INT_CAST(int, flow->peerLength));
		rc = (sent >= 0) || rdpudp_socket_error_is_transient(WSAGetLastError());
	}
	LeaveCriticalSection(&flow->lock);
	if (sendFn)
		return sendFn(custom, data, length);
	return rc;
}

HANDLE freerdp_udp_flow_get_event(rdpUdpFlow* flow)
{
	WINPR_ASSERT(flow);

	EnterCriticalSection(&flow->lock);
	const BOOL relay = (flow->relay != INVALID_SOCKET) && !flow->relayClosed;
	HANDLE event = relay ? flow->relayEvent : flow->event;
	LeaveCriticalSection(&flow->lock);
	return event;
}

UINT32 freerdp_udp_flow_get_timeout(rdpUdpFlow* flow)
{
	WINPR_ASSERT(flow);
	return flow->timeout ? flow->timeout(flow->workerCustom) : UINT32_MAX;
}

BOOL freerdp_udp_flow_check(rdpUdpFlow* flow)
{
	WINPR_ASSERT(flow);
	return flow->check ? flow->check(flow->workerCustom) : FALSE;
}

/* The caller must hold the listener's lock. */
static void udplistener_bind_flow(rdpUdpFlow* flow, SOCKET sockfd, const struct sockaddr* addr,
                                  size_t addrlen)
{
	WINPR_ASSERT(flow);
	WINPR_ASSERT(addrlen <= sizeof(flow->peer));

	EnterCriticalSection(&flow->lock);
	flow->sockfd = sockfd;
	memcpy(&flow->peer, addr, addrlen);
	flow->peerLength = addrlen;
	LeaveCriticalSection(&flow->lock);
}

/* The caller must hold the listener's lock. */
static void udplistener_dispatch(freerdp_udp_listener* listener, const udpListenerSocket* sock,
                                 const struct sockaddr* addr, size_t addrlen, const BYTE* data,
                                 size_t length)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(sock);
	WINPR_ASSERT(addr);
	WINPR_ASSERT(data || (length == 0));

	udpListenerEntry* entry = nullptr;
	for (size_t x = 0; x < listener->entryCount; x++)
	{
		udpListenerEntry* cur = &listener->entries[x];
		if (cur->bound && sockaddr_equal(&cur->addr, addr, addrlen))
		{
			entry = cur;
			break;
		}
	}

	BYTE hash[32] = WINPR_C_ARRAY_INIT;
	const BOOL hasHash = rdpudp_get_syn_cookie_hash(data, length, hash);

	/* A spoofed SYN can bind a flow to the address of a client before this client connects. The
	 * SYN of the client must not go to that flow, because the process of the flow can then answer
	 * as the server. The address must also not move to the flow of the hash, because a spoofed
	 * SYN can then take a connected client. The listener has no proof of who is correct, so it
	 * drops the SYN. The client then stays on TCP. */
	if (entry && hasHash && (memcmp(entry->flow->cookieHash, hash, sizeof(hash)) != 0))
	{
		WLog_INFO(TAG, "dropping a SYN whose cookie hash is not the hash of the bound flow");
		return;
	}

	if (!entry)
	{
		if (!hasHash)
		{
			WLog_DBG(TAG, "dropping a datagram from an unknown client");
			return;
		}
		for (size_t x = 0; x < listener->entryCount; x++)
		{
			udpListenerEntry* cur = &listener->entries[x];
			if (!cur->bound && ((cur->sockfd == INVALID_SOCKET) || (cur->sockfd == sock->sockfd)) &&
			    (memcmp(cur->flow->cookieHash, hash, sizeof(hash)) == 0))
			{
				entry = cur;
				break;
			}
		}
		if (!entry)
		{
			WLog_INFO(TAG, "dropping a SYN whose cookie hash matches no session");
			return;
		}
		if (addrlen > sizeof(entry->addr))
			return;
		memcpy(&entry->addr, addr, addrlen);
		entry->addrlen = addrlen;
		entry->bound = TRUE;
		udplistener_bind_flow(entry->flow, sock->sockfd, addr, addrlen);
	}

	rdpUdpFlow* flow = entry->flow;
	EnterCriticalSection(&flow->lock);
	const FreeRDP_UdpFlowReceiveFn receive = flow->receive;
	void* custom = flow->receiveCustom;
	LeaveCriticalSection(&flow->lock);
	const BOOL taken =
	    receive ? receive(custom, data, length) : freerdp_udp_flow_input(flow, data, length);
	if (!taken)
		WLog_DBG(TAG, "a flow takes no more, dropping a datagram");
}

/* The caller must hold the listener's lock. */
static void udplistener_free_relay(udpListenerRelay* relay)
{
	WINPR_ASSERT(relay);

	if (relay->thread)
	{
		(void)WaitForSingleObject(relay->thread, INFINITE);
		(void)CloseHandle(relay->thread);
	}
	if (relay->sockfd != INVALID_SOCKET)
		closesocket(relay->sockfd);
	freerdp_udp_flow_free(relay->flow);
	free(relay);
}

/* The caller must hold the lock. */
static void udplistener_reap_relays(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	udpListenerRelay** link = &listener->relays;
	while (*link)
	{
		udpListenerRelay* relay = *link;
		if (InterlockedCompareExchange(&relay->done, 0, 0) == 0)
		{
			link = &relay->next;
			continue;
		}
		*link = relay->next;
		udplistener_free_relay(relay);
	}
}

BOOL freerdp_udp_listener_check(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	EnterCriticalSection(&listener->lock);
	udplistener_reap_relays(listener);
	for (size_t x = 0; x < listener->socketCount; x++)
	{
		const udpListenerSocket* sock = &listener->sockets[x];
		(void)WSAResetEvent(sock->event);
		for (size_t count = 0; count < UDPLISTENER_READ_BUDGET; count++)
		{
			struct sockaddr_storage addr = WINPR_C_ARRAY_INIT;
			int addrlen = WINPR_ASSERTING_INT_CAST(int, sizeof(addr));
			const int rc = _recvfrom(sock->sockfd, (char*)listener->buffer,
			                         WINPR_ASSERTING_INT_CAST(int, sizeof(listener->buffer)), 0,
			                         (struct sockaddr*)&addr, &addrlen);
			if (rc < 0)
				break; /* nothing left, or an ICMP error for an earlier datagram */
			udplistener_dispatch(listener, sock, (const struct sockaddr*)&addr,
			                     WINPR_ASSERTING_INT_CAST(size_t, addrlen), listener->buffer,
			                     WINPR_ASSERTING_INT_CAST(size_t, rc));
		}
	}
	LeaveCriticalSection(&listener->lock);
	return TRUE;
}

WINPR_ATTR_NODISCARD
static DWORD WINAPI udplistener_thread(LPVOID arg)
{
	freerdp_udp_listener* listener = arg;
	WINPR_ASSERT(listener);

	while (InterlockedCompareExchange(&listener->stop, 0, 0) == 0)
	{
		HANDLE events[UDPLISTENER_MAX_SOCKETS + 1] = WINPR_C_ARRAY_INIT;
		DWORD count = 0;
		events[count++] = listener->wake;
		EnterCriticalSection(&listener->lock);
		(void)ResetEvent(listener->wake);
		for (size_t x = 0; x < listener->socketCount; x++)
			events[count++] = listener->sockets[x].event;
		LeaveCriticalSection(&listener->lock);

		if (WaitForMultipleObjects(count, events, FALSE, INFINITE) == WAIT_FAILED)
			break;
		if (InterlockedCompareExchange(&listener->stop, 0, 0) != 0)
			break;
		if (!freerdp_udp_listener_check(listener))
			break;
	}

	ExitThread(0);
	return 0;
}

/* The caller must hold the lock. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_add_socket(freerdp_udp_listener* listener, SOCKET sockfd,
                                   const struct sockaddr* addr, size_t addrlen)
{
	WINPR_ASSERT(listener);

	if (listener->socketCount >= UDPLISTENER_MAX_SOCKETS)
		return FALSE;

	HANDLE event = WSACreateEvent();
	if (!event)
		return FALSE;
	/* this also makes the socket non blocking */
	if (WSAEventSelect(sockfd, event, FD_READ) != 0)
	{
		if (!WSACloseEvent(event))
			WLog_WARN(TAG, "cannot close a socket's event");
		return FALSE;
	}

	udpListenerSocket* sock = &listener->sockets[listener->socketCount++];
	memset(sock, 0, sizeof(*sock));
	if (addr && (addrlen <= sizeof(sock->addr)))
	{
		memcpy(&sock->addr, addr, addrlen);
		sock->addrlen = addrlen;
	}
	sock->sockfd = sockfd;
	sock->event = event;
	(void)SetEvent(listener->wake); /* the thread must wait on it */
	return TRUE;
}

/* The caller must hold the lock. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_open_socket(freerdp_udp_listener* listener, const struct sockaddr* addr,
                                    size_t addrlen)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(addr);

	SOCKET sockfd = _socket(addr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == INVALID_SOCKET)
		return FALSE;

	const int on = 1;
	if ((addr->sa_family == AF_INET6) &&
	    (_setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&on, sizeof(on)) != 0))
		WLog_WARN(TAG, "setsockopt() IPV6_V6ONLY failed");
	/* Do not use SO_REUSEADDR. If a second socket uses the port, it takes some of the datagrams. No
	 * cookie sorts those datagrams across processes. A server that serves its clients in several
	 * processes gets an error here and stays on TCP, unless it relays: see udp.h. */
	/* a smaller buffer only costs resends, see RDPUDP_RECEIVE_BUFFER */
	const UINT32 rcvbuf = UDPLISTENER_RECEIVE_BUFFER;
	if (_setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf)) != 0)
		WLog_WARN(TAG, "setsockopt() SO_RCVBUF failed");

	if ((_bind(sockfd, addr, WINPR_ASSERTING_INT_CAST(int, addrlen)) != 0) ||
	    !udplistener_add_socket(listener, sockfd, addr, addrlen))
	{
		closesocket(sockfd);
		return FALSE;
	}
	WLog_INFO(TAG, "listening for RDP-UDP on %s",
	          (addr->sa_family == AF_INET6) ? "an IPv6 address" : "an IPv4 address");
	return TRUE;
}

/* The caller must hold the lock. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_open_address(freerdp_udp_listener* listener, const char* address,
                                     UINT16 port)
{
	WINPR_ASSERT(listener);

	char service[8] = WINPR_C_ARRAY_INIT;
	if (_snprintf(service, sizeof(service), "%" PRIu16, port) < 0)
		return FALSE;

	struct addrinfo hints = WINPR_C_ARRAY_INIT;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* result = nullptr;
	if (getaddrinfo(address, service, &hints, &result) != 0)
		return FALSE;

	BOOL opened = FALSE;
	for (const struct addrinfo* ai = result; ai; ai = ai->ai_next)
	{
		if (udplistener_open_socket(listener, ai->ai_addr, ai->ai_addrlen))
			opened = TRUE;
	}
	freeaddrinfo(result);
	if (!opened)
		WLog_ERR(TAG, "cannot open a UDP socket on port %" PRIu16, port);
	return opened;
}

freerdp_udp_listener* freerdp_udp_listener_new(void)
{
	freerdp_udp_listener* listener = calloc(1, sizeof(freerdp_udp_listener));
	if (!listener)
		return nullptr;
	if (!InitializeCriticalSectionAndSpinCount(&listener->lock, 4000))
	{
		free(listener);
		return nullptr;
	}
	listener->wake = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	if (!listener->wake)
	{
		DeleteCriticalSection(&listener->lock);
		free(listener);
		return nullptr;
	}
	return listener;
}

BOOL freerdp_udp_listener_open(freerdp_udp_listener* listener, const char* address, UINT16 port)
{
	WINPR_ASSERT(listener);
	EnterCriticalSection(&listener->lock);
	const BOOL rc = udplistener_open_address(listener, address, port);
	LeaveCriticalSection(&listener->lock);
	return rc;
}

BOOL freerdp_udp_listener_open_from_socket(freerdp_udp_listener* listener, int fd)
{
	WINPR_ASSERT(listener);
	EnterCriticalSection(&listener->lock);
	const BOOL rc = udplistener_add_socket(listener, (SOCKET)fd, nullptr, 0);
	LeaveCriticalSection(&listener->lock);
	return rc;
}

DWORD freerdp_udp_listener_get_event_handles(freerdp_udp_listener* listener, HANDLE* events,
                                             DWORD count)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(events);

	DWORD used = 0;
	EnterCriticalSection(&listener->lock);
	if (listener->socketCount <= count)
	{
		for (size_t x = 0; x < listener->socketCount; x++)
			events[used++] = listener->sockets[x].event;
	}
	LeaveCriticalSection(&listener->lock);
	return used;
}

BOOL freerdp_udp_listener_start(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);
	if (listener->thread)
		return TRUE;
	if (InterlockedCompareExchange(&listener->stop, 0, 1) != 0)
		WLog_DBG(TAG, "starting the listener's thread again");
	listener->thread = CreateThread(nullptr, 0, udplistener_thread, listener, 0, nullptr);
	return listener->thread != nullptr;
}

static void udplistener_stop_thread(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);
	if (!listener->thread)
		return;
	/* not InterlockedExchange: WinPR's reads the old value non-atomically */
	InterlockedIncrement(&listener->stop);
	(void)SetEvent(listener->wake);
	(void)WaitForSingleObject(listener->thread, INFINITE);
	(void)CloseHandle(listener->thread);
	listener->thread = nullptr;
}

/* The thread does not run. No flow keeps a socket that closes. */
static void udplistener_close_sockets(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	EnterCriticalSection(&listener->lock);
	for (size_t x = 0; x < listener->entryCount; x++)
	{
		rdpUdpFlow* flow = listener->entries[x].flow;
		EnterCriticalSection(&flow->lock);
		flow->sockfd = INVALID_SOCKET;
		LeaveCriticalSection(&flow->lock);
	}
	for (size_t x = 0; x < listener->socketCount; x++)
	{
		closesocket(listener->sockets[x].sockfd);
		if (!WSACloseEvent(listener->sockets[x].event))
			WLog_WARN(TAG, "cannot close a socket's event");
	}
	listener->socketCount = 0;
	LeaveCriticalSection(&listener->lock);
}

void freerdp_udp_listener_free(freerdp_udp_listener* listener)
{
	if (!listener)
		return;

	udplistener_stop_thread(listener);

	/* the threads of the relays end with their sockets, and need the lock to do that */
	EnterCriticalSection(&listener->lock);
	udpListenerRelay* relays = listener->relays;
	listener->relays = nullptr;
	LeaveCriticalSection(&listener->lock);
	while (relays)
	{
		udpListenerRelay* relay = relays;
		relays = relay->next;
		if (_shutdown(relay->sockfd, SHUT_RDWR) != 0)
			WLog_DBG(TAG, "shutting down a relay's socket failed");
#if defined(_WIN32)
		/* Winsock wakes a blocked receive of a datagram socket only when the socket closes */
		closesocket(relay->sockfd);
		relay->sockfd = INVALID_SOCKET;
#endif
		udplistener_free_relay(relay);
	}

	udplistener_close_sockets(listener);
	for (size_t x = 0; x < listener->entryCount; x++)
		listener->entries[x].flow->listener = nullptr;
	free(listener->entries);
	(void)CloseHandle(listener->wake);
	DeleteCriticalSection(&listener->lock);
	free(listener);
}

WINPR_ATTR_NODISCARD
static BOOL udplistener_expect(freerdp_udp_listener* listener, rdpUdpFlow* flow, SOCKET sockfd)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(flow);

	BOOL rc = FALSE;
	EnterCriticalSection(&listener->lock);
	BOOL known = FALSE;
	for (size_t x = 0; x < listener->entryCount; x++)
	{
		if (memcmp(listener->entries[x].flow->cookieHash, flow->cookieHash,
		           sizeof(flow->cookieHash)) == 0)
			known = TRUE;
	}
	if (!known && !flow->listener &&
	    (listener->entryCount < SIZE_MAX / sizeof(udpListenerEntry) - 1))
	{
		udpListenerEntry* entries =
		    realloc(listener->entries, (listener->entryCount + 1) * sizeof(udpListenerEntry));
		if (entries)
		{
			listener->entries = entries;
			udpListenerEntry* entry = &entries[listener->entryCount++];
			memset(entry, 0, sizeof(*entry));
			entry->flow = flow;
			entry->sockfd = sockfd;
			flow->listener = listener;
			rc = TRUE;
		}
	}
	LeaveCriticalSection(&listener->lock);
	return rc;
}

BOOL freerdp_udp_listener_expect(freerdp_udp_listener* listener, rdpUdpFlow* flow)
{
	return udplistener_expect(listener, flow, INVALID_SOCKET);
}

BOOL freerdp_udp_socket_pair(int fds[2])
{
	WINPR_ASSERT(fds);

	SOCKET pair[2] = { INVALID_SOCKET, INVALID_SOCKET };
#if defined(_WIN32)
	/* Winsock has no datagram socketpair, so use two loopback sockets that take nothing from
	 * others */
	struct sockaddr_in addr[2] = WINPR_C_ARRAY_INIT;
	for (size_t x = 0; x < 2; x++)
	{
		int length = sizeof(addr[x]);
		addr[x].sin_family = AF_INET;
		addr[x].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		pair[x] = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if ((pair[x] == INVALID_SOCKET) ||
		    (_bind(pair[x], (const struct sockaddr*)&addr[x], sizeof(addr[x])) != 0) ||
		    (_getsockname(pair[x], (struct sockaddr*)&addr[x], &length) != 0))
			goto fail;
	}
	for (size_t x = 0; x < 2; x++)
	{
		if (_connect(pair[x], (const struct sockaddr*)&addr[1 - x], sizeof(addr[1 - x])) != 0)
			goto fail;
	}
#else
	int sv[2] = { -1, -1 };
	if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) != 0)
		return FALSE;
	pair[0] = (SOCKET)sv[0];
	pair[1] = (SOCKET)sv[1];
#endif

	/* all of a client's data goes through here: a small buffer only costs resends */
	const UINT32 size = UDPLISTENER_RECEIVE_BUFFER;
	for (size_t x = 0; x < 2; x++)
	{
		if ((_setsockopt(pair[x], SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size)) != 0) ||
		    (_setsockopt(pair[x], SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size)) != 0))
			WLog_DBG(TAG, "cannot set the buffer sizes of a socket pair");
		fds[x] = (int)pair[x];
	}
	return TRUE;

#if defined(_WIN32)
fail:
	for (size_t x = 0; x < 2; x++)
	{
		if (pair[x] != INVALID_SOCKET)
			closesocket(pair[x]);
	}
	return FALSE;
#endif
}

/* Runs with the listener locked, see FreeRDP_UdpFlowReceiveFn. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_relay_receive(void* custom, const BYTE* data, size_t length)
{
	const udpListenerRelay* relay = custom;
	WINPR_ASSERT(relay);
	return _send(relay->sockfd, (const char*)data, WINPR_ASSERTING_INT_CAST(int, length),
	             MSG_DONTWAIT | MSG_NOSIGNAL) >= 0;
}

WINPR_ATTR_NODISCARD
static DWORD WINAPI udplistener_relay_thread(LPVOID arg)
{
	udpListenerRelay* relay = arg;
	WINPR_ASSERT(relay);

	while (TRUE)
	{
		BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
		const int rc = _recv(relay->sockfd, (char*)data, sizeof(data), 0);
		if (rc < 0)
		{
			if (WSAGetLastError() == WSAEINTR)
				continue;
			break;
		}
		if (rc == 0)
			break; /* the other end closed, or the listener shut the socket down */
		if (!freerdp_udp_flow_send(relay->flow, data, WINPR_ASSERTING_INT_CAST(size_t, rc)))
			WLog_DBG(TAG, "a relay could not send a datagram");
	}

	/* a socket without connections shows no end by itself */
	if (_send(relay->sockfd, "", 0, MSG_DONTWAIT | MSG_NOSIGNAL) < 0)
		WLog_DBG(TAG, "the other end of a relay is gone already");
	freerdp_udp_listener_forget(relay->listener, relay->flow);
	InterlockedIncrement(&relay->done);
	(void)SetEvent(relay->listener->wake); /* the listener cleans up after the relay */
	ExitThread(0);
	return 0;
}

BOOL freerdp_udp_listener_relay(freerdp_udp_listener* listener, const BYTE cookieHash[32], int fd)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(cookieHash);

	udpListenerRelay* relay = calloc(1, sizeof(udpListenerRelay));
	if (!relay)
		return FALSE;
	relay->listener = listener;
	relay->sockfd = (SOCKET)fd;
	relay->flow = freerdp_udp_flow_new(cookieHash);
	if (!relay->flow)
		goto fail;

	/* one datagram per message, for all types of fd, because fd can come from another process */
	int type = 0;
	int typeLength = sizeof(type);
	if ((_getsockopt(relay->sockfd, SOL_SOCKET, SO_TYPE, (char*)&type, &typeLength) != 0) ||
	    (type == SOCK_STREAM))
		goto fail;

	/* the thread blocks on the socket */
	u_long nonBlocking = 0;
	if (_ioctlsocket(relay->sockfd, FIONBIO, &nonBlocking) != 0)
		goto fail;

	freerdp_udp_flow_set_receive(relay->flow, udplistener_relay_receive, relay);
	if (!udplistener_expect(listener, relay->flow, INVALID_SOCKET))
		goto fail;

	relay->thread = CreateThread(nullptr, 0, udplistener_relay_thread, relay, 0, nullptr);
	if (!relay->thread)
		goto fail;

	EnterCriticalSection(&listener->lock);
	udplistener_reap_relays(listener);
	relay->next = listener->relays;
	listener->relays = relay;
	LeaveCriticalSection(&listener->lock);
	return TRUE;

fail:
	/* this also forgets the flow, and the caller keeps fd */
	freerdp_udp_flow_free(relay->flow);
	free(relay);
	return FALSE;
}

void freerdp_udp_listener_forget(freerdp_udp_listener* listener, rdpUdpFlow* flow)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(flow);

	EnterCriticalSection(&listener->lock);
	for (size_t x = 0; x < listener->entryCount; x++)
	{
		if (listener->entries[x].flow == flow)
		{
			listener->entryCount--;
			listener->entries[x] = listener->entries[listener->entryCount];
			break;
		}
	}
	flow->listener = nullptr;
	const BOOL shared = listener->shared;
	LeaveCriticalSection(&listener->lock);

	if (!shared)
		return;

	/* the last flow is gone: the thread ends and the sockets close */
	EnterCriticalSection(&g_listenerLock);
	EnterCriticalSection(&listener->lock);
	const BOOL empty = (listener->entryCount == 0);
	LeaveCriticalSection(&listener->lock);
	if (empty)
	{
		udplistener_stop_thread(listener);
		udplistener_close_sockets(listener);
		WLog_DBG(TAG, "no session waits for RDP-UDP any more, the UDP sockets are closed");
	}
	LeaveCriticalSection(&g_listenerLock);
}

/* An IPv4 client of a dual stack TCP socket shows as ::ffff:a.b.c.d. Its UDP arrives on IPv4. */
static void udplistener_unmap_address(const struct sockaddr* addr, size_t addrlen,
                                      struct sockaddr_storage* out, size_t* outlen)
{
	WINPR_ASSERT(addr);
	WINPR_ASSERT(addrlen <= sizeof(*out));

	memcpy(out, addr, addrlen);
	*outlen = addrlen;
	if ((addr->sa_family != AF_INET6) || (addrlen < sizeof(struct sockaddr_in6)))
		return;
	const struct sockaddr_in6* v6 = (const struct sockaddr_in6*)addr;
	if (!IN6_IS_ADDR_V4MAPPED(&v6->sin6_addr))
		return;

	struct sockaddr_in v4 = WINPR_C_ARRAY_INIT;
	v4.sin_family = AF_INET;
	v4.sin_port = v6->sin6_port;
	memcpy(&v4.sin_addr, &v6->sin6_addr.s6_addr[12], sizeof(v4.sin_addr));
	memset(out, 0, sizeof(*out));
	memcpy(out, &v4, sizeof(v4));
	*outlen = sizeof(v4);
}

BOOL udplistener_expect_on_address(const struct sockaddr* addr, size_t addrlen, rdpUdpFlow* flow)
{
	WINPR_ASSERT(addr);
	WINPR_ASSERT(flow);

	if ((addrlen > sizeof(struct sockaddr_storage)) ||
	    !InitOnceExecuteOnce(&g_listenerOnce, udplistener_init, nullptr, nullptr))
		return FALSE;

	struct sockaddr_storage local = WINPR_C_ARRAY_INIT;
	size_t localLength = 0;
	udplistener_unmap_address(addr, addrlen, &local, &localLength);

	BOOL rc = FALSE;
	EnterCriticalSection(&g_listenerLock);
	if (!g_listener)
	{
		g_listener = freerdp_udp_listener_new();
		if (g_listener)
			g_listener->shared = TRUE;
	}
	if (g_listener)
	{
		SOCKET sockfd = INVALID_SOCKET;
		EnterCriticalSection(&g_listener->lock);
		for (size_t x = 0; (sockfd == INVALID_SOCKET) && (x < g_listener->socketCount); x++)
		{
			const udpListenerSocket* sock = &g_listener->sockets[x];
			if (sockaddr_equal(&sock->addr, (const struct sockaddr*)&local, localLength))
				sockfd = sock->sockfd;
		}
		if ((sockfd == INVALID_SOCKET) &&
		    udplistener_open_socket(g_listener, (const struct sockaddr*)&local, localLength))
			sockfd = g_listener->sockets[g_listener->socketCount - 1].sockfd;
		LeaveCriticalSection(&g_listener->lock);

		rc = (sockfd != INVALID_SOCKET) && udplistener_expect(g_listener, flow, sockfd) &&
		     freerdp_udp_listener_start(g_listener);
	}
	LeaveCriticalSection(&g_listenerLock);
	if (!rc && flow->listener)
		freerdp_udp_listener_forget(flow->listener, flow);
	return rc;
}
