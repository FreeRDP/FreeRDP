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
#endif

#include <freerdp/log.h>

#include "rdpudp.h"
#include "udplistener_internal.h"

#define TAG FREERDP_TAG("core.udplistener")

static freerdp_udp_listener* g_listener;
static CRITICAL_SECTION g_listenerLock;
static INIT_ONCE g_listenerOnce = INIT_ONCE_STATIC_INIT;

/* Random seed for table hashes to prevent hash collision attacks. */
static UINT32 g_hashSeed;

WINPR_ATTR_NODISCARD
static BOOL CALLBACK udplistener_init(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                      WINPR_ATTR_UNUSED PVOID param,
                                      WINPR_ATTR_UNUSED PVOID* context)
{
	if (winpr_RAND(&g_hashSeed, sizeof(g_hashSeed)) < 0)
		return FALSE;
	return InitializeCriticalSectionAndSpinCount(&g_listenerLock, 4000);
}

/* MurmurHash3 fmix32, the final mixing step for table hashes. FNV-1a alone may map keys with
 * identical low bits to the same bucket regardless of the seed; this mixes all bits into the
 * low bits. */
WINPR_ATTR_NODISCARD
static UINT32 udplistener_fmix32(UINT32 hash)
{
	hash ^= hash >> 16;
	hash *= 0x85ebca6bu;
	hash ^= hash >> 13;
	hash *= 0xc2b2ae35u;
	hash ^= hash >> 16;
	return hash;
}

/* The entriesByAddress table is keyed by struct sockaddr_storage. */
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

	/* Apply the FNV-1a hash algorithm, using a random seed. */
	UINT32 hash = 2166136261u ^ g_hashSeed;
	for (size_t x = 0; x < length; x++)
	{
		hash ^= bytes[x];
		hash *= 16777619u;
	}
	hash ^= (UINT32)(port & 0xFF);
	hash *= 16777619u;
	hash ^= (UINT32)(port >> 8);
	hash *= 16777619u;
	return udplistener_fmix32(hash);
}

WINPR_ATTR_NODISCARD
static BOOL udplistener_address_equal(const void* a, const void* b)
{
	return udpsocket_address_equal(a, b, sizeof(struct sockaddr_storage));
}

/* entriesByHash is keyed by cookie hash. For relays, the cookie hash is provided by the
 * process serving the client and can be arbitrary, so it is seeded like the address hash. */
WINPR_ATTR_NODISCARD
static UINT32 udplistener_cookie_hash_hash(const void* key)
{
	const BYTE* bytes = key;
	WINPR_ASSERT(bytes);

	/* FNV-1a hash, seeded with a random value. */
	UINT32 hash = 2166136261u ^ g_hashSeed;
	for (size_t x = 0; x < 32; x++)
	{
		hash ^= bytes[x];
		hash *= 16777619u;
	}
	return udplistener_fmix32(hash);
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

/* This function assumes the caller holds the listener's lock. */
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

/* The listener's lock must be held by the caller. */
static void udplistener_dispatch(freerdp_udp_listener* listener, const udpListenerSocket* sock,
                                 const struct sockaddr_storage* addr, size_t addrlen,
                                 const udpLocalAddress* local, const BYTE* data, size_t length)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(sock);
	WINPR_ASSERT(addr);
	WINPR_ASSERT(local);
	WINPR_ASSERT(data || (length == 0));

	/* RDP-UDP datagrams are never empty; relays use empty messages to signal termination, so
	 * empty datagrams from the network must not reach a flow. */
	if (length == 0)
	{
		WLog_DBG(TAG, "discarding an empty datagram");
		return;
	}
	/* Exceeds maximum size sent by clients or carried by a relay in one piece. */
	if (length > UDPFLOW_MAX_DATAGRAM)
	{
		WLog_DBG(TAG, "discarding a datagram of %" PRIuz " bytes", length);
		return;
	}

	udpListenerEntry* entry = HashTable_GetItemValue(listener->entriesByAddress, addr);

	BYTE hash[32] = WINPR_C_ARRAY_INIT;
	const BOOL hasHash = rdpudp_get_syn_cookie_hash(data, length, hash);

	/* A spoofed SYN could bind a flow to a client's address before the client connects. To
	 * prevent the flow's process from answering as the server or a spoofed SYN from hijacking a
	 * connected client, the listener drops the SYN if the hash doesn't match, forcing the client
	 * to remain on TCP. */
	if (entry && hasHash && (memcmp(entry->flow->cookieHash, hash, sizeof(hash)) != 0))
	{
		/* Use DBG instead of INFO to prevent log flooding by network attackers. */
		WLog_DBG(TAG, "discarding SYN packet due to cookie hash mismatch with the bound flow");
		return;
	}

	if (!entry)
	{
		if (!hasHash)
		{
			WLog_DBG(TAG, "discarding a datagram from an unrecognized client");
			return;
		}
		entry = HashTable_GetItemValue(listener->entriesByHash, hash);
		if (!entry || entry->bound ||
		    ((entry->sockfd != INVALID_SOCKET) && (entry->sockfd != sock->sockfd)))
		{
			WLog_DBG(TAG,
			         "discarding SYN packet because its cookie hash does not match any session");
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
	/* The callback runs under the flow's lock, so freerdp_udp_flow_set_receive() waits for it. */
	EnterCriticalSection(&flow->lock);
	const BOOL taken = flow->receive ? flow->receive(flow->receiveCustom, data, length)
	                                 : freerdp_udp_flow_deliver(flow, data, length);
	LeaveCriticalSection(&flow->lock);
	if (!taken)
		WLog_DBG(TAG, "flow is full, dropping a datagram");
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
			const int rc = udpsocket_receive(sock, listener->buffer, sizeof(listener->buffer),
			                                 &addr, &addrlen, &local);
			if (rc < 0)
			{
				/* ICMP errors for previous replies indicate a client has disconnected or is
				 * unreachable. */
				const int error = WSAGetLastError();
				if ((error == WSAECONNRESET) || (error == WSAENETRESET))
					continue;
				break; /* no more data available */
			}
			udplistener_dispatch(listener, sock, &addr, WINPR_ASSERTING_INT_CAST(size_t, addrlen),
			                     &local, listener->buffer, WINPR_ASSERTING_INT_CAST(size_t, rc));
		}
	}
	LeaveCriticalSection(&listener->lock);
	return TRUE;
}

static void udplistener_close_socket(udpListenerSocket* sock)
{
	WINPR_ASSERT(sock);
	const SOCKET sockfd = sock->sockfd;
	sock->sockfd = INVALID_SOCKET;
	closesocket(sockfd);
	if (!WSACloseEvent(sock->event))
		WLog_WARN(TAG, "failed to close the socket event");
}

/* Closes retired sockets. Caller holds the lock, and the listener thread does not wait on
 * their events: it is the caller, or it is stopped. */
static void udplistener_close_retired(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);
	for (size_t x = 0; x < listener->retiredCount; x++)
		udplistener_close_socket(&listener->retired[x]);
	listener->retiredCount = 0;
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
		/* udplistener_stop_thread sets stop before wake: a stop that came after the check above
		 * either shows here, or its wake-up comes after the reset and ends the wait */
		if (InterlockedCompareExchange(&listener->stop, 0, 0) != 0)
		{
			LeaveCriticalSection(&listener->lock);
			break;
		}
		/* the last wait set is done with, sockets retired since can go */
		udplistener_close_retired(listener);
		for (size_t x = 0; x < listener->socketCount; x++)
			events[count++] = listener->sockets[x].event;
		LeaveCriticalSection(&listener->lock);

		if (WaitForMultipleObjects(count, events, FALSE, INFINITE) == WAIT_FAILED)
		{
			WLog_ERR(TAG, "UDP listener stopping: wait failed, new clients will use TCP");
			break;
		}
		if (InterlockedCompareExchange(&listener->stop, 0, 0) != 0)
			break;
		if (!freerdp_udp_listener_check(listener))
		{
			WLog_ERR(TAG, "UDP listener stopped: check failed, new clients will use TCP");
			break;
		}
	}

	ExitThread(0);
	return 0;
}

/* This function requires the caller to hold the lock. */
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
	/* This also sets the socket to non-blocking mode. */
	if (WSAEventSelect(sockfd, event, FD_READ) != 0)
	{
		if (!WSACloseEvent(event))
			WLog_WARN(TAG, "failed to close the socket event");
		return FALSE;
	}

	/* Increment count only after the entry is fully initialized to prevent forked children
	 * from seeing a partial entry. */
	udpListenerSocket* sock = &listener->sockets[listener->socketCount];
	memset(sock, 0, sizeof(*sock));
	memcpy(&sock->addr, &addr, sizeof(addr));
	sock->addrlen = WINPR_ASSERTING_INT_CAST(size_t, addrlen);
	sock->sockfd = sockfd;
	sock->event = event;
	udpsocket_enable_local_address(sock);
	listener->socketCount++;
	udpsocket_ignore_icmp_errors(sockfd);
	(void)SetEvent(listener->wake); /* wake the listener thread to update the wait set */
	return TRUE;
}

/* Caller must hold the lock. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_open_socket(freerdp_udp_listener* listener, const struct sockaddr* addr,
                                    size_t addrlen)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(addr);

	SOCKET sockfd = udpsocket_bind(addr, addrlen);
	if (sockfd == INVALID_SOCKET)
		return FALSE;
	if (!udplistener_add_socket(listener, sockfd))
	{
		closesocket(sockfd);
		return FALSE;
	}
	WLog_INFO(TAG, "waiting for RDP-UDP connections on %s",
	          (addr->sa_family == AF_INET6) ? "an IPv6 address" : "an IPv4 address");
	return TRUE;
}

/* Caller must hold the lock. */
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
		WLog_ERR(TAG, "unable to open a UDP socket on port %" PRIu16, port);
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
	/* Only for sockets owned by the listener; on failure, the caller retains ownership of fd */
	if (rc)
		udpsocket_set_no_inherit((SOCKET)fd);
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
		WLog_DBG(TAG, "restarting the listener thread");
	listener->thread = CreateThread(nullptr, 0, udplistener_thread, listener, 0, nullptr);
	return listener->thread != nullptr;
}

static void udplistener_stop_thread(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);
	if (!listener->thread)
		return;
	/* Avoid InterlockedExchange; WinPR reads the old value non-atomically */
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

/* Thread must not be running. Detaches all flows from sockets before closing them. */
static void udplistener_close_sockets(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	EnterCriticalSection(&listener->lock);
	if (!HashTable_Foreach(listener->entriesByHash, udplistener_unbind_socket, nullptr))
		WLog_WARN(TAG, "failed to release sockets for all flows");
	/* Invalidate the entry before closing the socket; see close_after_fork */
	for (size_t x = 0; x < listener->socketCount; x++)
		udplistener_close_socket(&listener->sockets[x]);
	listener->socketCount = 0;
	udplistener_close_retired(listener);
	LeaveCriticalSection(&listener->lock);
}

/* Releases the memory once the listener was freed and its last flow removed. */
static void udplistener_release(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);
	HashTable_Free(listener->entriesByAddress);
	HashTable_Free(listener->entriesByHash);
	(void)CloseHandle(listener->wake);
	DeleteCriticalSection(&listener->lock);
	free(listener);
}

void freerdp_udp_listener_close_after_fork(freerdp_udp_listener* listener)
{
	const BOOL shared = !listener || (listener == g_listener);
	if (!listener)
		listener = g_listener;
	if (shared)
	{
		/* Parent may hold the lock and the listener thread is not inherited by the child; the child
		 * must initialize a new process-wide listener. */
		g_listener = nullptr;
		if (!InitializeCriticalSectionAndSpinCount(&g_listenerLock, 4000))
			WLog_WARN(TAG, "unable to reset the process-wide listener lock");
	}
	if (!listener)
		return;

	/* Parent continues using these sockets; close local copies without sending messages or
	 * shutdown. */
	for (size_t x = 0; x < listener->socketCount; x++)
	{
		const SOCKET sockfd = listener->sockets[x].sockfd;
		listener->sockets[x].sockfd = INVALID_SOCKET;
		if (sockfd != INVALID_SOCKET)
			closesocket(sockfd);
	}
	for (size_t x = 0; x < listener->retiredCount; x++)
	{
		const SOCKET sockfd = listener->retired[x].sockfd;
		listener->retired[x].sockfd = INVALID_SOCKET;
		if (sockfd != INVALID_SOCKET)
			closesocket(sockfd);
	}
	udplistener_relays_close_after_fork(listener);
}

void freerdp_udp_listener_free(freerdp_udp_listener* listener)
{
	if (!listener)
		return;

	udplistener_stop_thread(listener);
	udplistener_stop_relays(listener);
	udplistener_close_sockets(listener);

	/* Flows still registered keep their entries and their pointer to the listener, so a peer
	 * may free its flow at any time; the last one removed releases the listener. */
	EnterCriticalSection(&listener->lock);
	listener->freed = TRUE;
	const BOOL release = (HashTable_Count(listener->entriesByHash) == 0);
	LeaveCriticalSection(&listener->lock);
	if (release)
		udplistener_release(listener);
}

BOOL udplistener_add_flow(freerdp_udp_listener* listener, rdpUdpFlow* flow, SOCKET sockfd)
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

BOOL freerdp_udp_listener_add_flow(freerdp_udp_listener* listener, rdpUdpFlow* flow)
{
	return udplistener_add_flow(listener, flow, INVALID_SOCKET);
}

typedef struct
{
	const freerdp_udp_listener* listener;
	BOOL used[UDPLISTENER_MAX_SOCKETS];
} udpSocketUse;

WINPR_ATTR_NODISCARD
static BOOL udplistener_mark_socket_use(WINPR_ATTR_UNUSED const void* key, void* value, void* arg)
{
	const udpListenerEntry* entry = value;
	udpSocketUse* use = arg;
	WINPR_ASSERT(entry);
	WINPR_ASSERT(use);
	for (size_t x = 0; x < use->listener->socketCount; x++)
	{
		if (use->listener->sockets[x].sockfd == entry->sockfd)
			use->used[x] = TRUE;
	}
	return TRUE;
}

/* Finds sockets not associated with any registered flow. Flows registered with
 * INVALID_SOCKET do not keep any alive. Caller must hold the lock. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_find_unused_sockets(const freerdp_udp_listener* listener, udpSocketUse* use)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(use);
	memset(use, 0, sizeof(*use));
	use->listener = listener;
	BOOL unused = FALSE;
	if (!HashTable_Foreach(listener->entriesByHash, udplistener_mark_socket_use, use))
		return FALSE;
	for (size_t x = 0; x < listener->socketCount; x++)
	{
		if (!use->used[x])
			unused = TRUE;
	}
	return unused;
}

/* Caller must hold the lock. A running listener thread may wait on these events: the sockets
 * are retired and the thread, woken, closes them when it builds its next wait set. Without a
 * thread they are closed now. */
static void udplistener_close_unused_sockets(freerdp_udp_listener* listener,
                                             const udpSocketUse* use)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(use);
	size_t kept = 0;
	for (size_t x = 0; x < listener->socketCount; x++)
	{
		if (use->used[x])
		{
			listener->sockets[kept++] = listener->sockets[x];
			continue;
		}
		if (!listener->thread)
			udplistener_close_socket(&listener->sockets[x]);
		else if (listener->retiredCount < UDPLISTENER_MAX_SOCKETS)
			listener->retired[listener->retiredCount++] = listener->sockets[x];
		else /* the thread may wait on it: it stays open, a later removal retires it */
			listener->sockets[kept++] = listener->sockets[x];
	}
	listener->socketCount = kept;
	if (listener->thread)
		(void)SetEvent(listener->wake);
}

void freerdp_udp_listener_remove_flow(freerdp_udp_listener* listener, rdpUdpFlow* flow)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(flow);

	EnterCriticalSection(&listener->lock);
	udpListenerEntry* entry = HashTable_GetItemValue(listener->entriesByHash, flow->cookieHash);
	if (entry && (entry->flow == flow))
	{
		if (entry->bound && !HashTable_Remove(listener->entriesByAddress, &entry->addr))
			WLog_WARN(TAG, "bound flow is missing an address");
		if (!HashTable_Remove(listener->entriesByHash, flow->cookieHash))
			WLog_WARN(TAG, "failed to remove flow");
		free(entry);

		/* Socket is owned by the listener and can be closed after this function returns. */
		EnterCriticalSection(&flow->lock);
		flow->sockfd = INVALID_SOCKET;
		LeaveCriticalSection(&flow->lock);
	}
	/* Flows registered with other listeners remain unaffected. */
	if (flow->listener == listener)
		flow->listener = nullptr;
	const BOOL shared = listener->shared;
	const BOOL release = listener->freed && (HashTable_Count(listener->entriesByHash) == 0);
	LeaveCriticalSection(&listener->lock);

	/* the process-wide listener is never freed, so it does not get here */
	if (release)
	{
		udplistener_release(listener);
		return;
	}
	if (!shared)
		return;

	/* Close the thread and sockets when the last flow is gone. Sockets not used by any flow are
	 * also closed, as a host with changing addresses may exceed UDPLISTENER_MAX_SOCKETS over time.
	 */
	udpSocketUse use = WINPR_C_ARRAY_INIT;
	EnterCriticalSection(&g_listenerLock);
	EnterCriticalSection(&listener->lock);
	const BOOL empty = (HashTable_Count(listener->entriesByHash) == 0);
	const BOOL unused = !empty && udplistener_find_unused_sockets(listener, &use);
	LeaveCriticalSection(&listener->lock);
	if (empty)
	{
		udplistener_stop_thread(listener);
		udplistener_close_sockets(listener);
		WLog_DBG(TAG, "no more sessions are waiting for RDP-UDP, closing the UDP sockets");
	}
	else if (unused)
	{
		/* The thread keeps running: stopping and restarting it would leave the sessions on
		 * UDP without a reader if the restart failed. */
		EnterCriticalSection(&listener->lock);
		if (udplistener_find_unused_sockets(listener, &use))
			udplistener_close_unused_sockets(listener, &use);
		LeaveCriticalSection(&listener->lock);
	}
	LeaveCriticalSection(&g_listenerLock);
}

BOOL udplistener_add_flow_on_address(const struct sockaddr* addr, size_t addrlen, rdpUdpFlow* flow)
{
	WINPR_ASSERT(addr);
	WINPR_ASSERT(flow);

	if ((addrlen > sizeof(struct sockaddr_storage)) ||
	    !InitOnceExecuteOnce(&g_listenerOnce, udplistener_init, nullptr, nullptr))
		return FALSE;

	struct sockaddr_storage local = WINPR_C_ARRAY_INIT;
	size_t localLength = 0;
	udpsocket_unmap_address(addr, addrlen, &local, &localLength);

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
			if (udpsocket_address_equal(&sock->addr, (const struct sockaddr*)&local, localLength))
				sockfd = sock->sockfd;
		}
		/* A socket retired a moment ago is still bound until the listener thread closes it,
		 * and a second bind to its address would fail: it is taken back into use. The thread
		 * may still have its event in the wait set, which does no harm. */
		for (size_t x = 0; (sockfd == INVALID_SOCKET) && (x < g_listener->retiredCount) &&
		                   (g_listener->socketCount < UDPLISTENER_MAX_SOCKETS);
		     x++)
		{
			const udpListenerSocket* sock = &g_listener->retired[x];
			if (!udpsocket_address_equal(&sock->addr, (const struct sockaddr*)&local, localLength))
				continue;
			sockfd = sock->sockfd;
			g_listener->sockets[g_listener->socketCount++] = *sock;
			g_listener->retired[x] = g_listener->retired[--g_listener->retiredCount];
			(void)SetEvent(g_listener->wake);
		}
		if ((sockfd == INVALID_SOCKET) &&
		    udplistener_open_socket(g_listener, (const struct sockaddr*)&local, localLength))
			sockfd = g_listener->sockets[g_listener->socketCount - 1].sockfd;
		LeaveCriticalSection(&g_listener->lock);

		rc = (sockfd != INVALID_SOCKET) && udplistener_add_flow(g_listener, flow, sockfd) &&
		     freerdp_udp_listener_start(g_listener);
	}
	LeaveCriticalSection(&g_listenerLock);
	if (!rc && flow->listener)
		freerdp_udp_listener_remove_flow(flow->listener, flow);
	return rc;
}
