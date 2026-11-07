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

/* struct in6_pktinfo needs these before the first system header */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif
#if defined(__APPLE__) && !defined(__APPLE_USE_RFC_3542)
#define __APPLE_USE_RFC_3542 // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif

#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/collections.h>
#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/interlocked.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
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

/* A socket on the unspecified address gets the address that the client sent to with each datagram.
 * The replies then go out from that address. */
#if !defined(_WIN32) && defined(IP_PKTINFO)
#define UDPLISTENER_LOCAL_ADDRESS_IPV4
#endif
#if !defined(_WIN32) && defined(IPV6_RECVPKTINFO) && defined(IPV6_PKTINFO)
#define UDPLISTENER_LOCAL_ADDRESS_IPV6
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

/* Datagrams read from a socket at a time, so that one busy socket does not starve the others. The
 * relay thread uses the same number for each relay. */
#define UDPLISTENER_READ_BUDGET 256

/* Datagrams that a flow keeps for its tunnel. A client mostly sends acknowledgements and input, so
 * this is more than sufficient while the tunnel keeps up. Past this number datagrams are lost, as
 * on the network. Because of this, a client that floods the server costs nobody else anything. */
#define UDPFLOW_SLOTS 256

/* The first datagram of a flow makes room for this number of datagrams. The room doubles when it
 * is full, up to UDPFLOW_SLOTS. Thus a flow uses memory only for the datagrams that wait. */
#define UDPFLOW_FIRST_SLOTS 8

/* The address that a client sent its datagrams to */
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

	/* set while datagrams wait. Change all of these only when you hold lock */
	HANDLE event;
	BYTE* ring; /* the first datagram makes it: an absent client costs nothing */
	size_t slots;
	UINT16 lengths[UDPFLOW_SLOTS];
	size_t head;
	size_t count;

	/* where the client is, on a listener's socket */
	SOCKET sockfd;
	struct sockaddr_storage peer;
	size_t peerLength;
	udpLocalAddress local; /* the replies go out from here */

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
	struct sockaddr_storage addr; /* what it is bound to */
	size_t addrlen;
	SOCKET sockfd;
	HANDLE event;
	BOOL localAddress; /* each datagram tells the address that the client sent to */
} udpListenerSocket;

typedef struct s_udp_listener_relay
{
	struct s_udp_listener_relay* next;
	rdpUdpFlow* flow;
	SOCKET sockfd;
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

	/* Each entry is in entriesByHash. After its SYN, an entry is also in entriesByAddress. The
	 * entries belong to entriesByHash. */
	wHashTable* entriesByHash;
	wHashTable* entriesByAddress;

	/* one thread serves all relays */
	udpListenerRelay* relays;
	HANDLE relayThread;
	LONG relayStop;
	SOCKET relayWake[2]; /* write to 0 to wake the thread, which waits on 1 */

	HANDLE thread;
	HANDLE wake;
	LONG stop;

	BOOL shared; /* the process wide one, which closes with its last flow */

	BYTE buffer[0x10000];
};

static freerdp_udp_listener* g_listener;
static CRITICAL_SECTION g_listenerLock;
static INIT_ONCE g_listenerOnce = INIT_ONCE_STATIC_INIT;

/* A random value for the hash of addresses. Thus a client cannot select addresses that go into
 * the same bucket. */
static UINT32 g_addressSeed;

WINPR_ATTR_NODISCARD
static BOOL CALLBACK udplistener_init(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                      WINPR_ATTR_UNUSED PVOID param,
                                      WINPR_ATTR_UNUSED PVOID* context)
{
	if (winpr_RAND(&g_addressSeed, sizeof(g_addressSeed)) < 0)
		return FALSE;
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

/* The keys of entriesByAddress are struct sockaddr_storage. */
WINPR_ATTR_NODISCARD
static UINT32 udplistener_address_hash(const void* key)
{
	const struct sockaddr_storage* addr = key;
	WINPR_ASSERT(addr);

	const BYTE* bytes = nullptr;
	size_t length = 0;
	UINT16 port = 0;
	if (addr->ss_family == AF_INET)
	{
		const struct sockaddr_in* in = (const struct sockaddr_in*)addr;
		bytes = (const BYTE*)&in->sin_addr;
		length = sizeof(in->sin_addr);
		port = in->sin_port;
	}
	else if (addr->ss_family == AF_INET6)
	{
		const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)addr;
		bytes = (const BYTE*)&in6->sin6_addr;
		length = sizeof(in6->sin6_addr);
		port = in6->sin6_port;
	}

	/* FNV-1a, which starts from the random seed */
	UINT32 hash = 2166136261u ^ g_addressSeed;
	for (size_t x = 0; x < length; x++)
	{
		hash ^= bytes[x];
		hash *= 16777619u;
	}
	hash ^= (UINT32)(port & 0xFF);
	hash *= 16777619u;
	hash ^= (UINT32)(port >> 8);
	hash *= 16777619u;
	return hash;
}

WINPR_ATTR_NODISCARD
static BOOL udplistener_address_equal(const void* a, const void* b)
{
	return sockaddr_equal(a, b, sizeof(struct sockaddr_storage));
}

/* The keys of entriesByHash are cookie hashes. A cookie hash is a SHA-256 value. Thus its first
 * bytes are sufficient as the hash. */
WINPR_ATTR_NODISCARD
static UINT32 udplistener_cookie_hash_hash(const void* key)
{
	WINPR_ASSERT(key);
	UINT32 hash = 0;
	memcpy(&hash, key, sizeof(hash));
	return hash;
}

WINPR_ATTR_NODISCARD
static BOOL udplistener_cookie_hash_equal(const void* a, const void* b)
{
	WINPR_ASSERT(a);
	WINPR_ASSERT(b);
	return memcmp(a, b, 32) == 0;
}

WINPR_ATTR_NODISCARD
static wHashTable* udplistener_table_new(HASH_TABLE_HASH_FN hash, OBJECT_EQUALS_FN equal)
{
	wHashTable* table = HashTable_New(FALSE);
	if (!table)
		return nullptr;
	wObject* key = HashTable_KeyObject(table);
	if (!key || !HashTable_SetHashFunction(table, hash))
	{
		HashTable_Free(table);
		return nullptr;
	}
	key->fnObjectEquals = equal;
	return table;
}

WINPR_ATTR_NODISCARD
static BOOL udplistener_is_any_address(const struct sockaddr_storage* addr)
{
	WINPR_ASSERT(addr);
	if (addr->ss_family == AF_INET)
	{
		const struct sockaddr_in* in = (const struct sockaddr_in*)addr;
		return in->sin_addr.s_addr == htonl(INADDR_ANY);
	}
	if (addr->ss_family == AF_INET6)
	{
		const struct sockaddr_in6* in6 = (const struct sockaddr_in6*)addr;
		return IN6_IS_ADDR_UNSPECIFIED(&in6->sin6_addr);
	}
	return FALSE;
}

/* Asks the system to tell the destination address of each datagram, see udplistener_receive(). */
static void udplistener_enable_local_address(udpListenerSocket* sock)
{
	WINPR_ASSERT(sock);

	if (!udplistener_is_any_address(&sock->addr))
		return;

	int rc = -1;
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4) || defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	const int on = 1;
#endif
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4)
	if (sock->addr.ss_family == AF_INET)
		rc = _setsockopt(sock->sockfd, IPPROTO_IP, IP_PKTINFO, (const char*)&on, sizeof(on));
#endif
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	if (sock->addr.ss_family == AF_INET6)
		rc =
		    _setsockopt(sock->sockfd, IPPROTO_IPV6, IPV6_RECVPKTINFO, (const char*)&on, sizeof(on));
#endif
	sock->localAddress = (rc == 0);
	if (!sock->localAddress)
		WLog_WARN(TAG, "a socket on any address cannot tell where a datagram went to. With "
		               "more than one address, a reply can go out from an incorrect address");
}

/* Receives one datagram. For a socket on any address, it also gets the address that the client
 * sent to.
 * @return the length of the datagram, or a negative value when nothing waits */
WINPR_ATTR_NODISCARD
static int udplistener_receive(const udpListenerSocket* sock, BYTE* data, size_t size,
                               struct sockaddr_storage* addr, int* addrlen, udpLocalAddress* local)
{
	WINPR_ASSERT(sock);
	WINPR_ASSERT(data);
	WINPR_ASSERT(addr);
	WINPR_ASSERT(addrlen);
	WINPR_ASSERT(local);

	memset(local, 0, sizeof(*local));
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4) || defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	if (sock->localAddress)
	{
		union
		{
			struct cmsghdr header;
			BYTE data[256];
		} control;
		memset(&control, 0, sizeof(control));
		struct iovec iov = { .iov_base = data, .iov_len = size };
		struct msghdr msg = WINPR_C_ARRAY_INIT;
		msg.msg_name = addr;
		msg.msg_namelen = WINPR_ASSERTING_INT_CAST(socklen_t, *addrlen);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = control.data;
		msg.msg_controllen = sizeof(control.data);
		const ssize_t got = recvmsg(WINPR_ASSERTING_INT_CAST(int, sock->sockfd), &msg, 0);
		if (got < 0)
			return -1;
		*addrlen = WINPR_ASSERTING_INT_CAST(int, msg.msg_namelen);

		for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg))
		{
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4)
			if ((cmsg->cmsg_level == IPPROTO_IP) && (cmsg->cmsg_type == IP_PKTINFO) &&
			    (cmsg->cmsg_len >= CMSG_LEN(sizeof(struct in_pktinfo))))
			{
				struct in_pktinfo info = WINPR_C_ARRAY_INIT;
				memcpy(&info, CMSG_DATA(cmsg), sizeof(info));
				local->family = AF_INET;
				local->ipv4 = info.ipi_addr;
				local->interfaceIndex = WINPR_ASSERTING_INT_CAST(UINT32, info.ipi_ifindex);
				local->valid = TRUE;
			}
#endif
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
			if ((cmsg->cmsg_level == IPPROTO_IPV6) && (cmsg->cmsg_type == IPV6_PKTINFO) &&
			    (cmsg->cmsg_len >= CMSG_LEN(sizeof(struct in6_pktinfo))))
			{
				struct in6_pktinfo info = WINPR_C_ARRAY_INIT;
				memcpy(&info, CMSG_DATA(cmsg), sizeof(info));
				local->family = AF_INET6;
				local->ipv6 = info.ipi6_addr;
				local->interfaceIndex = WINPR_ASSERTING_INT_CAST(UINT32, info.ipi6_ifindex);
				local->valid = TRUE;
			}
#endif
		}
		return WINPR_ASSERTING_INT_CAST(int, got);
	}
#endif
	return _recvfrom(sock->sockfd, (char*)data, WINPR_ASSERTING_INT_CAST(int, size), 0,
	                 (struct sockaddr*)addr, addrlen);
}

/* Sends one datagram from the address in local.
 * @return FALSE if the system did not take it */
WINPR_ATTR_NODISCARD
static BOOL udplistener_send_from(SOCKET sockfd, const struct sockaddr_storage* to, size_t tolen,
                                  const udpLocalAddress* local, const BYTE* data, size_t length)
{
	WINPR_ASSERT(to);
	WINPR_ASSERT(local);
	WINPR_ASSERT(data);

#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4) || defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	union
	{
		struct cmsghdr header;
		BYTE data[256];
	} control;
	memset(&control, 0, sizeof(control));
	struct iovec iov = { .iov_base = WINPR_CAST_CONST_PTR_AWAY(data, void*), .iov_len = length };
	struct msghdr msg = WINPR_C_ARRAY_INIT;
	msg.msg_name = WINPR_CAST_CONST_PTR_AWAY(to, void*);
	msg.msg_namelen = WINPR_ASSERTING_INT_CAST(socklen_t, tolen);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control.data;
	msg.msg_controllen = sizeof(control.data);
	struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
	if (!cmsg)
		return FALSE;

	BOOL known = FALSE;
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV4)
	if (local->family == AF_INET)
	{
		/* the route selects the interface */
		struct in_pktinfo info = WINPR_C_ARRAY_INIT;
		info.ipi_spec_dst = local->ipv4;
		cmsg->cmsg_level = IPPROTO_IP;
		cmsg->cmsg_type = IP_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(info));
		memcpy(CMSG_DATA(cmsg), &info, sizeof(info));
		msg.msg_controllen = CMSG_SPACE(sizeof(info));
		known = TRUE;
	}
#endif
#if defined(UDPLISTENER_LOCAL_ADDRESS_IPV6)
	if (local->family == AF_INET6)
	{
		/* a link local address is valid only on its interface */
		struct in6_pktinfo info = WINPR_C_ARRAY_INIT;
		info.ipi6_addr = local->ipv6;
		info.ipi6_ifindex = local->interfaceIndex;
		cmsg->cmsg_level = IPPROTO_IPV6;
		cmsg->cmsg_type = IPV6_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(info));
		memcpy(CMSG_DATA(cmsg), &info, sizeof(info));
		msg.msg_controllen = CMSG_SPACE(sizeof(info));
		known = TRUE;
	}
#endif
	if (!known)
		return FALSE;
	return sendmsg(WINPR_ASSERTING_INT_CAST(int, sockfd), &msg, 0) >= 0;
#else
	WINPR_UNUSED(sockfd);
	WINPR_UNUSED(tolen);
	WINPR_UNUSED(length);
	return FALSE;
#endif
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

/* Makes more room in the ring of a flow. The datagrams that wait move to the start.
 * The caller must hold the lock of the flow. */
WINPR_ATTR_NODISCARD
static BOOL udpflow_grow(rdpUdpFlow* flow)
{
	WINPR_ASSERT(flow);

	const size_t slots = (flow->slots == 0) ? UDPFLOW_FIRST_SLOTS : 2 * flow->slots;
	if (slots > UDPFLOW_SLOTS)
		return FALSE;
	BYTE* ring = malloc(slots * UDPFLOW_MAX_DATAGRAM);
	if (!ring)
		return FALSE;

	UINT16 lengths[UDPFLOW_SLOTS] = WINPR_C_ARRAY_INIT;
	/* datagrams wait only in a ring that exists */
	const size_t count = (flow->slots > 0) ? flow->count : 0;
	for (size_t x = 0; x < count; x++)
	{
		const size_t from = (flow->head + x) % flow->slots;
		memcpy(&ring[x * UDPFLOW_MAX_DATAGRAM], &flow->ring[from * UDPFLOW_MAX_DATAGRAM],
		       flow->lengths[from]);
		lengths[x] = flow->lengths[from];
	}
	memcpy(flow->lengths, lengths, sizeof(lengths));
	free(flow->ring);
	flow->ring = ring;
	flow->slots = slots;
	flow->head = 0;
	return TRUE;
}

BOOL freerdp_udp_flow_input(rdpUdpFlow* flow, const BYTE* data, size_t length)
{
	WINPR_ASSERT(flow);
	WINPR_ASSERT(data || (length == 0));

	if (length > UDPFLOW_MAX_DATAGRAM)
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&flow->lock);
	if ((flow->count < flow->slots) || udpflow_grow(flow))
	{
		const size_t slot = (flow->head + flow->count) % flow->slots;
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
		flow->head = (flow->head + 1) % flow->slots;
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
		/* If the address is gone, the system selects a new one */
		rc = flow->local.valid && udplistener_send_from(flow->sockfd, &flow->peer, flow->peerLength,
		                                                &flow->local, data, length);
		if (!rc)
		{
			const int sent =
			    _sendto(flow->sockfd, (const char*)data, WINPR_ASSERTING_INT_CAST(int, length), 0,
				        (const struct sockaddr*)&flow->peer,
				        WINPR_ASSERTING_INT_CAST(int, flow->peerLength));
			rc = (sent >= 0) || rdpudp_socket_error_is_transient(WSAGetLastError());
		}
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
                                  size_t addrlen, const udpLocalAddress* local)
{
	WINPR_ASSERT(flow);
	WINPR_ASSERT(addrlen <= sizeof(flow->peer));
	WINPR_ASSERT(local);

	EnterCriticalSection(&flow->lock);
	flow->sockfd = sockfd;
	memcpy(&flow->peer, addr, addrlen);
	flow->peerLength = addrlen;
	flow->local = *local;
	LeaveCriticalSection(&flow->lock);
}

/* The caller must hold the listener's lock. */
static void udplistener_dispatch(freerdp_udp_listener* listener, const udpListenerSocket* sock,
                                 const struct sockaddr_storage* addr, size_t addrlen,
                                 const udpLocalAddress* local, const BYTE* data, size_t length)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(sock);
	WINPR_ASSERT(addr);
	WINPR_ASSERT(local);
	WINPR_ASSERT(data || (length == 0));

	/* An RDP-UDP datagram is never empty. A relay uses an empty message to tell its end. Thus an
	 * empty datagram from the network must not get to a flow. */
	if (length == 0)
	{
		WLog_DBG(TAG, "dropping an empty datagram");
		return;
	}

	udpListenerEntry* entry = HashTable_GetItemValue(listener->entriesByAddress, addr);

	BYTE hash[32] = WINPR_C_ARRAY_INIT;
	const BOOL hasHash = rdpudp_get_syn_cookie_hash(data, length, hash);

	/* A spoofed SYN can bind a flow to the address of a client before this client connects. The
	 * SYN of the client must not go to that flow, because the process of the flow can then answer
	 * as the server. The address must also not move to the flow of the hash, because a spoofed
	 * SYN can then take a connected client. The listener has no proof of who is correct, so it
	 * drops the SYN. The client then stays on TCP. */
	if (entry && hasHash && (memcmp(entry->flow->cookieHash, hash, sizeof(hash)) != 0))
	{
		/* not at INFO: all on the network can send this, and fill the log */
		WLog_DBG(TAG, "dropping a SYN whose cookie hash is not the hash of the bound flow");
		return;
	}

	if (!entry)
	{
		if (!hasHash)
		{
			WLog_DBG(TAG, "dropping a datagram from an unknown client");
			return;
		}
		entry = HashTable_GetItemValue(listener->entriesByHash, hash);
		if (!entry || entry->bound ||
		    ((entry->sockfd != INVALID_SOCKET) && (entry->sockfd != sock->sockfd)))
		{
			WLog_DBG(TAG, "dropping a SYN whose cookie hash matches no session");
			return;
		}
		if (addrlen > sizeof(entry->addr))
			return;
		memcpy(&entry->addr, addr, addrlen);
		entry->addrlen = addrlen;
		if (!HashTable_Insert(listener->entriesByAddress, &entry->addr, entry))
			return;
		entry->bound = TRUE;
		udplistener_bind_flow(entry->flow, sock->sockfd, (const struct sockaddr*)addr, addrlen,
		                      local);
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

BOOL freerdp_udp_listener_check(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	EnterCriticalSection(&listener->lock);
	for (size_t x = 0; x < listener->socketCount; x++)
	{
		const udpListenerSocket* sock = &listener->sockets[x];
		(void)WSAResetEvent(sock->event);
		for (size_t count = 0; count < UDPLISTENER_READ_BUDGET; count++)
		{
			struct sockaddr_storage addr = WINPR_C_ARRAY_INIT;
			int addrlen = WINPR_ASSERTING_INT_CAST(int, sizeof(addr));
			udpLocalAddress local = WINPR_C_ARRAY_INIT;
			const int rc = udplistener_receive(sock, listener->buffer, sizeof(listener->buffer),
			                                   &addr, &addrlen, &local);
			if (rc < 0)
				break; /* nothing left, or an ICMP error for an earlier datagram */
			udplistener_dispatch(listener, sock, &addr, WINPR_ASSERTING_INT_CAST(size_t, addrlen),
			                     &local, listener->buffer, WINPR_ASSERTING_INT_CAST(size_t, rc));
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
static BOOL udplistener_add_socket(freerdp_udp_listener* listener, SOCKET sockfd)
{
	WINPR_ASSERT(listener);

	if (listener->socketCount >= UDPLISTENER_MAX_SOCKETS)
		return FALSE;

	struct sockaddr_storage addr = WINPR_C_ARRAY_INIT;
	int addrlen = WINPR_ASSERTING_INT_CAST(int, sizeof(addr));
	if (_getsockname(sockfd, (struct sockaddr*)&addr, &addrlen) != 0)
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
	memcpy(&sock->addr, &addr, sizeof(addr));
	sock->addrlen = WINPR_ASSERTING_INT_CAST(size_t, addrlen);
	sock->sockfd = sockfd;
	sock->event = event;
	udplistener_enable_local_address(sock);
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
#if defined(_WIN32)
	/* Without this, another process of the same user can bind the port too, and get some of the
	 * datagrams. */
	if (_setsockopt(sockfd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&on, sizeof(on)) != 0)
	{
		closesocket(sockfd);
		return FALSE;
	}
#endif
	/* Do not use SO_REUSEADDR. If a second socket uses the port, it takes some of the datagrams. No
	 * cookie sorts those datagrams across processes. A server that serves its clients in several
	 * processes gets an error here and stays on TCP, unless it relays: see udp.h. */
	/* a smaller buffer only costs resends, see RDPUDP_RECEIVE_BUFFER */
	const UINT32 rcvbuf = UDPLISTENER_RECEIVE_BUFFER;
	if (_setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf)) != 0)
		WLog_WARN(TAG, "setsockopt() SO_RCVBUF failed");

	if ((_bind(sockfd, addr, WINPR_ASSERTING_INT_CAST(int, addrlen)) != 0) ||
	    !udplistener_add_socket(listener, sockfd))
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
	if (!InitOnceExecuteOnce(&g_listenerOnce, udplistener_init, nullptr, nullptr))
		return nullptr;

	freerdp_udp_listener* listener = calloc(1, sizeof(freerdp_udp_listener));
	if (!listener)
		return nullptr;
	listener->relayWake[0] = INVALID_SOCKET;
	listener->relayWake[1] = INVALID_SOCKET;
	if (!InitializeCriticalSectionAndSpinCount(&listener->lock, 4000))
	{
		free(listener);
		return nullptr;
	}
	listener->wake = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	listener->entriesByHash =
	    udplistener_table_new(udplistener_cookie_hash_hash, udplistener_cookie_hash_equal);
	listener->entriesByAddress =
	    udplistener_table_new(udplistener_address_hash, udplistener_address_equal);
	if (!listener->wake || !listener->entriesByHash || !listener->entriesByAddress)
	{
		HashTable_Free(listener->entriesByAddress);
		HashTable_Free(listener->entriesByHash);
		if (listener->wake)
			(void)CloseHandle(listener->wake);
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
	const BOOL rc = udplistener_add_socket(listener, (SOCKET)fd);
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

WINPR_ATTR_NODISCARD
static BOOL udplistener_unbind_socket(WINPR_ATTR_UNUSED const void* key, void* value,
                                      WINPR_ATTR_UNUSED void* arg)
{
	udpListenerEntry* entry = value;
	WINPR_ASSERT(entry);
	EnterCriticalSection(&entry->flow->lock);
	entry->flow->sockfd = INVALID_SOCKET;
	LeaveCriticalSection(&entry->flow->lock);
	return TRUE;
}

/* The thread does not run. No flow keeps a socket that closes. */
static void udplistener_close_sockets(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	EnterCriticalSection(&listener->lock);
	if (!HashTable_Foreach(listener->entriesByHash, udplistener_unbind_socket, nullptr))
		WLog_WARN(TAG, "cannot release the sockets of all flows");
	for (size_t x = 0; x < listener->socketCount; x++)
	{
		closesocket(listener->sockets[x].sockfd);
		if (!WSACloseEvent(listener->sockets[x].event))
			WLog_WARN(TAG, "cannot close a socket's event");
	}
	listener->socketCount = 0;
	LeaveCriticalSection(&listener->lock);
}

/* After this, the listener does not use the relay. Its socket closes. */
static void udplistener_free_relay(freerdp_udp_listener* listener, udpListenerRelay* relay)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(relay);

	/* first stop the datagrams to the socket, then close the socket */
	freerdp_udp_listener_forget(listener, relay->flow);
	if (relay->sockfd != INVALID_SOCKET)
		closesocket(relay->sockfd);
	freerdp_udp_flow_free(relay->flow);
	free(relay);
}

static void udplistener_stop_relays(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	if (listener->relayThread)
	{
		/* not InterlockedExchange: WinPR's reads the old value non-atomically */
		InterlockedIncrement(&listener->relayStop);
		if (_send(listener->relayWake[0], "", 0, MSG_NOSIGNAL) < 0)
			WLog_DBG(TAG, "cannot wake the relay thread");
		(void)WaitForSingleObject(listener->relayThread, INFINITE);
		(void)CloseHandle(listener->relayThread);
		listener->relayThread = nullptr;
	}
	for (size_t x = 0; x < 2; x++)
	{
		if (listener->relayWake[x] != INVALID_SOCKET)
			closesocket(listener->relayWake[x]);
		listener->relayWake[x] = INVALID_SOCKET;
	}

	EnterCriticalSection(&listener->lock);
	udpListenerRelay* relays = listener->relays;
	listener->relays = nullptr;
	LeaveCriticalSection(&listener->lock);
	while (relays)
	{
		udpListenerRelay* relay = relays;
		relays = relay->next;
		/* the process at the other end sees the end of the relay */
		if (_shutdown(relay->sockfd, SHUT_RDWR) != 0)
			WLog_DBG(TAG, "shutting down a relay's socket failed");
		udplistener_free_relay(listener, relay);
	}
}

WINPR_ATTR_NODISCARD
static BOOL udplistener_free_entry(WINPR_ATTR_UNUSED const void* key, void* value,
                                   WINPR_ATTR_UNUSED void* arg)
{
	udpListenerEntry* entry = value;
	WINPR_ASSERT(entry);
	entry->flow->listener = nullptr;
	free(entry);
	return TRUE;
}

void freerdp_udp_listener_free(freerdp_udp_listener* listener)
{
	if (!listener)
		return;

	udplistener_stop_thread(listener);
	udplistener_stop_relays(listener);
	udplistener_close_sockets(listener);

	/* entriesByAddress has the same entries */
	if (!HashTable_Foreach(listener->entriesByHash, udplistener_free_entry, nullptr))
		WLog_WARN(TAG, "cannot release all flows");
	HashTable_Free(listener->entriesByAddress);
	HashTable_Free(listener->entriesByHash);
	(void)CloseHandle(listener->wake);
	DeleteCriticalSection(&listener->lock);
	free(listener);
}

WINPR_ATTR_NODISCARD
static BOOL udplistener_expect(freerdp_udp_listener* listener, rdpUdpFlow* flow, SOCKET sockfd)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(flow);

	udpListenerEntry* entry = calloc(1, sizeof(udpListenerEntry));
	if (!entry)
		return FALSE;
	entry->flow = flow;
	entry->sockfd = sockfd;

	BOOL rc = FALSE;
	EnterCriticalSection(&listener->lock);
	if (!flow->listener && !HashTable_Contains(listener->entriesByHash, flow->cookieHash) &&
	    HashTable_Insert(listener->entriesByHash, flow->cookieHash, entry))
	{
		flow->listener = listener;
		rc = TRUE;
	}
	LeaveCriticalSection(&listener->lock);
	if (!rc)
		free(entry);
	// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): entriesByHash owns entry
	return rc;
}

BOOL freerdp_udp_listener_expect(freerdp_udp_listener* listener, rdpUdpFlow* flow)
{
	return udplistener_expect(listener, flow, INVALID_SOCKET);
}

WINPR_ATTR_NODISCARD
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
		/* no other process can bind the same port and read or send on the pair */
		const BOOL exclusive = TRUE;
		if ((pair[x] == INVALID_SOCKET) ||
		    (_setsockopt(pair[x], SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive,
		                 sizeof(exclusive)) != 0) ||
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
	/* Some systems, for example macOS, have no SOCK_SEQPACKET for AF_UNIX. A pair of SOCK_DGRAM
	 * sockets also keeps the datagrams apart. It shows no end when the other end closes, but both
	 * ends send an empty message before they close. */
	int sv[2] = { -1, -1 };
	if ((socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) != 0) &&
	    (socketpair(AF_UNIX, SOCK_DGRAM, 0, sv) != 0))
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

/* Sends to the client what waits on the socket of a relay.
 * @return FALSE when the relay ended */
WINPR_ATTR_NODISCARD
static BOOL udplistener_relay_forward(udpListenerRelay* relay)
{
	WINPR_ASSERT(relay);

	for (size_t count = 0; count < UDPLISTENER_READ_BUDGET; count++)
	{
		BYTE data[UDPFLOW_MAX_DATAGRAM] = WINPR_C_ARRAY_INIT;
		const int rc = _recv(relay->sockfd, (char*)data, sizeof(data), 0);
		if (rc < 0)
		{
			const int error = WSAGetLastError();
			return (error == WSAEWOULDBLOCK) || (error == WSAEINTR);
		}
		if (rc == 0)
			return FALSE; /* the other end closed */
		if (!freerdp_udp_flow_send(relay->flow, data, WINPR_ASSERTING_INT_CAST(size_t, rc)))
			WLog_DBG(TAG, "a relay could not send a datagram");
	}
	return TRUE;
}

static void udplistener_end_relay(freerdp_udp_listener* listener, udpListenerRelay* relay)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(relay);

	/* a socket without connections shows no end by itself */
	if (_send(relay->sockfd, "", 0, MSG_DONTWAIT | MSG_NOSIGNAL) < 0)
		WLog_DBG(TAG, "the other end of a relay is gone already");

	EnterCriticalSection(&listener->lock);
	udpListenerRelay** link = &listener->relays;
	while (*link && (*link != relay))
		link = &(*link)->next;
	if (*link)
		*link = relay->next;
	LeaveCriticalSection(&listener->lock);
	udplistener_free_relay(listener, relay);
}

static void udplistener_poll_set(udpPollFd* fd, SOCKET sockfd)
{
	WINPR_ASSERT(fd);
#if defined(_WIN32)
	fd->fd = sockfd;
#else
	fd->fd = WINPR_ASSERTING_INT_CAST(int, sockfd);
#endif
	fd->events = POLLIN;
	fd->revents = 0;
}

WINPR_ATTR_NODISCARD
static int udplistener_poll(udpPollFd* fds, size_t count)
{
#if defined(_WIN32)
	return WSAPoll(fds, WINPR_ASSERTING_INT_CAST(ULONG, count), -1);
#else
	return poll(fds, WINPR_ASSERTING_INT_CAST(nfds_t, count), -1);
#endif
}

/* One thread for all relays. Only this thread ends a relay, until the listener stops it. */
WINPR_ATTR_NODISCARD
static DWORD WINAPI udplistener_relay_thread(LPVOID arg)
{
	freerdp_udp_listener* listener = arg;
	WINPR_ASSERT(listener);

	udpPollFd* fds = nullptr;
	udpListenerRelay** polled = nullptr;
	size_t capacity = 0;

	while (InterlockedCompareExchange(&listener->relayStop, 0, 0) == 0)
	{
		EnterCriticalSection(&listener->lock);
		size_t count = 1;
		for (const udpListenerRelay* relay = listener->relays; relay; relay = relay->next)
			count++;
		if (count > capacity)
		{
			udpPollFd* newFds = realloc(fds, count * sizeof(udpPollFd));
			if (newFds)
				fds = newFds;
			udpListenerRelay** newPolled =
			    (udpListenerRelay**)realloc((void*)polled, count * sizeof(udpListenerRelay*));
			if (newPolled)
				polled = newPolled;
			if (!newFds || !newPolled)
			{
				LeaveCriticalSection(&listener->lock);
				WLog_ERR(TAG, "the relays stop: no memory");
				break;
			}
			capacity = count;
		}
		udplistener_poll_set(&fds[0], listener->relayWake[1]);
		polled[0] = nullptr;
		size_t used = 1;
		for (udpListenerRelay* relay = listener->relays; relay; relay = relay->next)
		{
			udplistener_poll_set(&fds[used], relay->sockfd);
			polled[used++] = relay;
		}
		LeaveCriticalSection(&listener->lock);

		if (udplistener_poll(fds, used) < 0)
		{
			if (WSAGetLastError() == WSAEINTR)
				continue;
			WLog_ERR(TAG, "the relays stop: poll failed");
			break;
		}

		if (fds[0].revents != 0)
		{
			BYTE drain[16] = WINPR_C_ARRAY_INIT;
			while (_recv(listener->relayWake[1], (char*)drain, sizeof(drain), 0) >= 0)
			{
			}
		}
		for (size_t x = 1; x < used; x++)
		{
			if ((fds[x].revents != 0) && !udplistener_relay_forward(polled[x]))
				udplistener_end_relay(listener, polled[x]);
		}
	}

	free((void*)polled);
	free(fds);
	ExitThread(0);
	return 0;
}

/* The caller must hold the lock. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_start_relays(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	if (listener->relayThread)
		return TRUE;

	int wake[2] = { -1, -1 };
	if (!freerdp_udp_socket_pair(wake))
		return FALSE;
	listener->relayWake[0] = (SOCKET)wake[0];
	listener->relayWake[1] = (SOCKET)wake[1];
	u_long nonBlocking = 1;
	if ((_ioctlsocket(listener->relayWake[0], FIONBIO, &nonBlocking) != 0) ||
	    (_ioctlsocket(listener->relayWake[1], FIONBIO, &nonBlocking) != 0))
		goto fail;

	listener->relayThread =
	    CreateThread(nullptr, 0, udplistener_relay_thread, listener, 0, nullptr);
	if (listener->relayThread)
		return TRUE;

fail:
	for (size_t x = 0; x < 2; x++)
	{
		closesocket(listener->relayWake[x]);
		listener->relayWake[x] = INVALID_SOCKET;
	}
	return FALSE;
}

BOOL freerdp_udp_listener_relay(freerdp_udp_listener* listener, const BYTE cookieHash[32], int fd)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(cookieHash);

	udpListenerRelay* relay = calloc(1, sizeof(udpListenerRelay));
	if (!relay)
		return FALSE;
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

	/* the relay thread reads all sockets that have data, and does not wait on one */
	u_long nonBlocking = 1;
	if (_ioctlsocket(relay->sockfd, FIONBIO, &nonBlocking) != 0)
		goto fail;

	freerdp_udp_flow_set_receive(relay->flow, udplistener_relay_receive, relay);
	if (!udplistener_expect(listener, relay->flow, INVALID_SOCKET))
		goto fail;

	EnterCriticalSection(&listener->lock);
	const BOOL started = udplistener_start_relays(listener);
	if (started)
	{
		relay->next = listener->relays;
		listener->relays = relay;
	}
	LeaveCriticalSection(&listener->lock);
	if (!started)
		goto fail;

	/* the relay thread must also wait on this socket */
	if (_send(listener->relayWake[0], "", 0, MSG_NOSIGNAL) < 0)
		WLog_DBG(TAG, "cannot wake the relay thread");
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
	udpListenerEntry* entry = HashTable_GetItemValue(listener->entriesByHash, flow->cookieHash);
	if (entry && (entry->flow == flow))
	{
		if (entry->bound && !HashTable_Remove(listener->entriesByAddress, &entry->addr))
			WLog_WARN(TAG, "a bound flow had no address");
		if (!HashTable_Remove(listener->entriesByHash, flow->cookieHash))
			WLog_WARN(TAG, "cannot remove a flow");
		free(entry);

		/* the socket belongs to the listener and can close after this */
		EnterCriticalSection(&flow->lock);
		flow->sockfd = INVALID_SOCKET;
		LeaveCriticalSection(&flow->lock);
	}
	/* a flow of another listener stays with that listener */
	if (flow->listener == listener)
		flow->listener = nullptr;
	const BOOL shared = listener->shared;
	LeaveCriticalSection(&listener->lock);

	if (!shared)
		return;

	/* the last flow is gone: the thread ends and the sockets close */
	EnterCriticalSection(&g_listenerLock);
	EnterCriticalSection(&listener->lock);
	const BOOL empty = (HashTable_Count(listener->entriesByHash) == 0);
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
