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

#include <freerdp/config.h>

#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>
#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/winsock.h>

#if !defined(_WIN32)
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include <freerdp/log.h>

#include "rdpudp.h"
#include "udplistener.h"

#define TAG FREERDP_TAG("core.udplistener")

/* As for a client connection, see RDPUDP_RECEIVE_BUFFER: here it takes every session's data. */
#define UDPLISTENER_RECEIVE_BUFFER (4 * 1024 * 1024)

/* How long the thread waits for a datagram before it looks for new sockets or a stop. */
#define UDPLISTENER_POLL_MS 200

typedef struct
{
	BYTE cookieHash[32];
	udpListenerInputFn input;
	void* custom;
	BOOL bound; /* the SYN came, from addr */
	struct sockaddr_storage addr;
	size_t addrlen;
} udpListenerSession;

typedef struct
{
	UINT16 port;
	SOCKET sockfd;
} udpListenerSocket;

typedef struct s_udp_listener udpListener;

typedef struct
{
	udpListener* listener;
	BOOL stop; /* read and written with the listener's lock held */
} udpListenerThread;

struct s_udp_listener
{
	CRITICAL_SECTION lock;

	/* all three only change with the lock held; the sockets stay open while the thread runs */
	udpListenerSocket* sockets;
	size_t socketCount;
	udpListenerSession* sessions;
	size_t sessionCount;

	HANDLE thread;
	udpListenerThread* threadCtx; /* the running thread's own stop flag */

	BYTE buffer[0x10000];
};

static udpListener g_listener;
static INIT_ONCE g_listenerOnce = INIT_ONCE_STATIC_INIT;

WINPR_ATTR_NODISCARD
static BOOL CALLBACK udplistener_init(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                      WINPR_ATTR_UNUSED PVOID param,
                                      WINPR_ATTR_UNUSED PVOID* context)
{
	memset(&g_listener, 0, sizeof(g_listener));
	return InitializeCriticalSectionAndSpinCount(&g_listener.lock, 4000);
}

WINPR_ATTR_NODISCARD
static udpListener* udplistener_get(void)
{
	if (!InitOnceExecuteOnce(&g_listenerOnce, udplistener_init, nullptr, nullptr))
		return nullptr;
	return &g_listener;
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

/* Called with the lock held. */
static void udplistener_remove_session(udpListener* listener, size_t index)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(listener->sessions);
	WINPR_ASSERT(index < listener->sessionCount);
	listener->sessionCount--;
	listener->sessions[index] = listener->sessions[listener->sessionCount];
}

/* Called with the lock held. */
static void udplistener_dispatch(udpListener* listener, SOCKET sockfd, const struct sockaddr* addr,
                                 size_t addrlen, BYTE* data, size_t length)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(addr);
	WINPR_ASSERT(data || (length == 0));

	size_t index = listener->sessionCount;
	for (size_t x = 0; x < listener->sessionCount; x++)
	{
		const udpListenerSession* cur = &listener->sessions[x];
		if (cur->bound && sockaddr_equal(&cur->addr, addr, addrlen))
		{
			index = x;
			break;
		}
	}

	if (index == listener->sessionCount)
	{
		BYTE hash[32] = WINPR_C_ARRAY_INIT;
		if (!rdpudp_get_syn_cookie_hash(data, length, hash))
		{
			WLog_DBG(TAG, "dropping a datagram from an unknown client");
			return;
		}
		for (size_t x = 0; x < listener->sessionCount; x++)
		{
			const udpListenerSession* cur = &listener->sessions[x];
			if (!cur->bound && (memcmp(cur->cookieHash, hash, sizeof(hash)) == 0))
			{
				index = x;
				break;
			}
		}
		if (index == listener->sessionCount)
		{
			WLog_INFO(TAG, "dropping a SYN whose cookie hash matches no session");
			return;
		}
		udpListenerSession* session = &listener->sessions[index];
		if (addrlen > sizeof(session->addr))
			return;
		memcpy(&session->addr, addr, addrlen);
		session->addrlen = addrlen;
		session->bound = TRUE;
	}

	const udpListenerSession* session = &listener->sessions[index];
	WINPR_ASSERT(session->input);
	if (!session->input(session->custom, sockfd, addr, addrlen, data, length))
		udplistener_remove_session(listener, index);
}

/* Waits up to UDPLISTENER_POLL_MS for a socket to become readable. The socket list does not
 * change while the thread runs except by growing, which the next round picks up. */
static void udplistener_wait(udpListener* listener)
{
	WINPR_ASSERT(listener);

	fd_set rset;
	FD_ZERO(&rset);
	SOCKET highest = 0;
	EnterCriticalSection(&listener->lock);
	for (size_t x = 0; x < listener->socketCount; x++)
	{
		const SOCKET sockfd = listener->sockets[x].sockfd;
#if !defined(_WIN32)
		if (sockfd >= FD_SETSIZE)
			continue;
#endif
		FD_SET(sockfd, &rset);
		if (sockfd > highest)
			highest = sockfd;
	}
	LeaveCriticalSection(&listener->lock);

	struct timeval tv = WINPR_C_ARRAY_INIT;
	tv.tv_usec = UDPLISTENER_POLL_MS * 1000;
#if defined(_WIN32)
	(void)select(0, &rset, nullptr, nullptr, &tv);
#else
	(void)select(WINPR_ASSERTING_INT_CAST(int, highest) + 1, &rset, nullptr, nullptr, &tv);
#endif
}

WINPR_ATTR_NODISCARD
static DWORD WINAPI udplistener_thread(LPVOID arg)
{
	udpListenerThread* ctx = arg;
	WINPR_ASSERT(ctx);
	udpListener* listener = ctx->listener;
	WINPR_ASSERT(listener);

	while (TRUE)
	{
		udplistener_wait(listener);

		EnterCriticalSection(&listener->lock);
		const BOOL stop = ctx->stop;
		for (size_t x = 0; !stop && (x < listener->socketCount); x++)
		{
			const SOCKET sockfd = listener->sockets[x].sockfd;
			while (TRUE)
			{
				struct sockaddr_storage addr = WINPR_C_ARRAY_INIT;
				int addrlen = WINPR_ASSERTING_INT_CAST(int, sizeof(addr));
				const int rc = _recvfrom(sockfd, (char*)listener->buffer,
				                         WINPR_ASSERTING_INT_CAST(int, sizeof(listener->buffer)), 0,
				                         (struct sockaddr*)&addr, &addrlen);
				if (rc < 0)
					break; /* nothing left, or an ICMP error for an earlier datagram */
				udplistener_dispatch(listener, sockfd, (const struct sockaddr*)&addr,
				                     WINPR_ASSERTING_INT_CAST(size_t, addrlen), listener->buffer,
				                     WINPR_ASSERTING_INT_CAST(size_t, rc));
			}
		}
		LeaveCriticalSection(&listener->lock);
		if (stop)
			break;
	}

	ExitThread(0);
	return 0;
}

/* Opens the sockets of port, unless they are open. Called with the lock held. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_open_port(udpListener* listener, UINT16 port)
{
	WINPR_ASSERT(listener);

	for (size_t x = 0; x < listener->socketCount; x++)
	{
		if (listener->sockets[x].port == port)
			return TRUE;
	}

	char service[16] = WINPR_C_ARRAY_INIT;
	if (_snprintf(service, sizeof(service), "%" PRIu16, port) < 0)
		return FALSE;
	struct addrinfo hints = WINPR_C_ARRAY_INIT;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	struct addrinfo* result = nullptr;
	if ((getaddrinfo(nullptr, service, &hints, &result) != 0) || !result)
	{
		WLog_ERR(TAG, "cannot resolve the UDP listening address");
		return FALSE;
	}

	BOOL opened = FALSE;
	for (struct addrinfo* ai = result; ai; ai = ai->ai_next)
	{
		if ((ai->ai_family != AF_INET) && (ai->ai_family != AF_INET6))
			continue;

		/* room for one more, without an overflowing size */
		if (listener->socketCount >= SIZE_MAX / sizeof(udpListenerSocket) - 1)
			break;
		udpListenerSocket* sockets =
		    realloc(listener->sockets, (listener->socketCount + 1) * sizeof(udpListenerSocket));
		if (!sockets)
			break;
		listener->sockets = sockets;

		SOCKET sockfd = _socket(ai->ai_family, SOCK_DGRAM, IPPROTO_UDP);
		if (sockfd == INVALID_SOCKET)
			continue;

		const int on = 1;
		if ((ai->ai_family == AF_INET6) &&
		    (_setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&on, sizeof(on)) != 0))
			WLog_WARN(TAG, "setsockopt() IPV6_V6ONLY failed");
		if (_setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on)) != 0)
			WLog_WARN(TAG, "setsockopt() SO_REUSEADDR failed");
		/* a smaller buffer only costs resends, see RDPUDP_RECEIVE_BUFFER */
		const UINT32 rcvbuf = UDPLISTENER_RECEIVE_BUFFER;
		if (_setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf)) != 0)
			WLog_WARN(TAG, "setsockopt() SO_RCVBUF failed");

		u_long nonBlocking = 1;
		if ((_bind(sockfd, ai->ai_addr, WINPR_ASSERTING_INT_CAST(int, ai->ai_addrlen)) != 0) ||
		    (_ioctlsocket(sockfd, FIONBIO, &nonBlocking) != 0))
		{
			closesocket(sockfd);
			continue;
		}

		listener->sockets[listener->socketCount].port = port;
		listener->sockets[listener->socketCount].sockfd = sockfd;
		listener->socketCount++;
		opened = TRUE;
		WLog_INFO(TAG, "listening for RDP-UDP on %s port %" PRIu16,
		          (ai->ai_family == AF_INET6) ? "IPv6" : "IPv4", port);
	}
	freeaddrinfo(result);

	if (!opened)
		WLog_ERR(TAG, "cannot open a UDP socket on port %" PRIu16, port);
	return opened;
}

BOOL udplistener_register(UINT16 port, const BYTE cookieHash[32], udpListenerInputFn input,
                          void* custom)
{
	WINPR_ASSERT(cookieHash);
	WINPR_ASSERT(input);
	WINPR_ASSERT(custom);

	udpListener* listener = udplistener_get();
	if (!listener)
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&listener->lock);
	if (!udplistener_open_port(listener, port))
		goto out;

	/* room for one more, without an overflowing size */
	if (listener->sessionCount >= SIZE_MAX / sizeof(udpListenerSession) - 1)
		goto out;
	udpListenerSession* sessions =
	    realloc(listener->sessions, (listener->sessionCount + 1) * sizeof(udpListenerSession));
	if (!sessions)
		goto out;
	listener->sessions = sessions;
	udpListenerSession* session = &listener->sessions[listener->sessionCount];
	memset(session, 0, sizeof(*session));
	memcpy(session->cookieHash, cookieHash, sizeof(session->cookieHash));
	session->input = input;
	session->custom = custom;
	listener->sessionCount++;

	if (!listener->thread)
	{
		udpListenerThread* ctx = calloc(1, sizeof(udpListenerThread));
		if (ctx)
		{
			ctx->listener = listener;
			listener->thread = CreateThread(nullptr, 0, udplistener_thread, ctx, 0, nullptr);
		}
		if (!listener->thread)
		{
			free(ctx);
			listener->sessionCount--;
			goto out;
		}
		listener->threadCtx = ctx;
	}
	rc = TRUE;
out:
	LeaveCriticalSection(&listener->lock);
	return rc;
}

void udplistener_unregister(void* custom)
{
	udpListener* listener = udplistener_get();
	if (!listener || !custom)
		return;

	HANDLE thread = nullptr;
	udpListenerThread* ctx = nullptr;
	udpListenerSocket* sockets = nullptr;
	size_t socketCount = 0;

	EnterCriticalSection(&listener->lock);
	for (size_t x = 0; x < listener->sessionCount;)
	{
		if (listener->sessions[x].custom == custom)
			udplistener_remove_session(listener, x);
		else
			x++;
	}

	/* the last session is gone: the thread ends and the sockets close */
	if ((listener->sessionCount == 0) && listener->thread)
	{
		/* the thread's own flag: a session that registers before it ended starts another */
		ctx = listener->threadCtx;
		WINPR_ASSERT(ctx);
		ctx->stop = TRUE;
		listener->threadCtx = nullptr;
		thread = listener->thread;
		listener->thread = nullptr;
		sockets = listener->sockets;
		socketCount = listener->socketCount;
		listener->sockets = nullptr;
		listener->socketCount = 0;
		free(listener->sessions);
		listener->sessions = nullptr;
	}
	LeaveCriticalSection(&listener->lock);

	if (thread)
	{
		(void)WaitForSingleObject(thread, INFINITE);
		(void)CloseHandle(thread);
		free(ctx);
		WLog_DBG(TAG, "no session waits for RDP-UDP any more, closing the UDP sockets");
	}
	for (size_t x = 0; x < socketCount; x++)
		closesocket(sockets[x].sockfd);
	free(sockets);
}
