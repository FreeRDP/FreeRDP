/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server UDP listener: relays to client-serving processes
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

#include <freerdp/log.h>

#include "udplistener_internal.h"

#define TAG FREERDP_TAG("core.udplistener")

/* Detaches relay from listener and closes its socket. */
static void udplistener_free_relay(freerdp_udp_listener* listener, udpListenerRelay* relay)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(relay);

	/* Stop datagrams before closing the socket. */
	freerdp_udp_listener_remove_flow(listener, relay->flow);
	if (relay->sockfd != INVALID_SOCKET)
		closesocket(relay->sockfd);
	freerdp_udp_flow_free(relay->flow);
	free(relay);
}

void udplistener_stop_relays(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	if (listener->relayThread)
	{
		/* Avoid InterlockedExchange; WinPR reads the old value non-atomically. */
		InterlockedIncrement(&listener->relayStop);
		if (_send(listener->relayWake[0], "", 0, MSG_NOSIGNAL) < 0)
			WLog_DBG(TAG, "unable to wake the relay thread");
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
	listener->relayCount = 0;
	LeaveCriticalSection(&listener->lock);
	while (relays)
	{
		udpListenerRelay* relay = relays;
		relays = relay->next;
		/* Notify peer that the relay ended, as udplistener_end_relay() does. */
		if (_send(relay->sockfd, "", 0, MSG_DONTWAIT | MSG_NOSIGNAL) < 0)
			WLog_DBG(TAG, "the remote end of the relay has already disconnected");
		if (_shutdown(relay->sockfd, SHUT_RDWR) != 0)
			WLog_DBG(TAG, "failed to shut down the relay socket");
		udplistener_free_relay(listener, relay);
	}
}

void udplistener_relays_close_after_fork(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	for (size_t x = 0; x < 2; x++)
	{
		const SOCKET sockfd = listener->relayWake[x];
		listener->relayWake[x] = INVALID_SOCKET;
		if (sockfd != INVALID_SOCKET)
			closesocket(sockfd);
	}
	for (udpListenerRelay* relay = listener->relays; relay; relay = relay->next)
	{
		const SOCKET sockfd = relay->sockfd;
		relay->sockfd = INVALID_SOCKET;
		if (sockfd != INVALID_SOCKET)
			closesocket(sockfd);
	}
}

static void udplistener_wake_relays(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);
	if (_send(listener->relayWake[0], "", 0, MSG_NOSIGNAL) < 0)
		WLog_DBG(TAG, "unable to wake the relay thread");
}

/* Called with listener lock held; see FreeRDP_UdpFlowReceiveFn. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_relay_receive(void* custom, const BYTE* data, size_t length)
{
	udpListenerRelay* relay = custom;
	WINPR_ASSERT(relay);
	if (_send(relay->sockfd, (const char*)data, WINPR_ASSERTING_INT_CAST(int, length),
	          MSG_DONTWAIT | MSG_NOSIGNAL) >= 0)
		return TRUE;

	if (udpsocket_peer_closed() && (InterlockedCompareExchange(&relay->ending, 1, 0) == 0))
		udplistener_wake_relays(relay->listener);
	return FALSE;
}

/* Forwards pending data from the relay socket to the client.
 * @return FALSE if the relay ended */
WINPR_ATTR_NODISCARD
static BOOL udplistener_relay_forward(udpListenerRelay* relay)
{
	WINPR_ASSERT(relay);

	for (size_t count = 0; count < UDPLISTENER_READ_BUDGET; count++)
	{
		/* One spare byte: POSIX truncates to buffer size, so a full message is too large. */
		BYTE data[UDPFLOW_MAX_DATAGRAM + 1] = WINPR_C_ARRAY_INIT;
		const int rc = _recv(relay->sockfd, (char*)data, sizeof(data), 0);
		if (rc < 0)
		{
			const int error = WSAGetLastError();
			/* Windows drops oversized messages with this error, while POSIX truncates them;
			 * either way, only this message is lost. */
			if (error == WSAEMSGSIZE)
				continue;
			return (error == WSAEWOULDBLOCK) || (error == WSAEINTR);
		}
		if (rc == 0)
			return FALSE; /* Peer closed. */
		if ((size_t)rc > UDPFLOW_MAX_DATAGRAM)
		{
			WLog_DBG(TAG, "relay dropped an oversized datagram");
			continue;
		}
		if (!freerdp_udp_flow_send(relay->flow, data, WINPR_ASSERTING_INT_CAST(size_t, rc)))
			WLog_DBG(TAG, "relay failed to send a datagram");
	}
	return TRUE;
}

static void udplistener_end_relay(freerdp_udp_listener* listener, udpListenerRelay* relay)
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(relay);

	/* Connectionless sockets do not signal closure automatically. */
	if (_send(relay->sockfd, "", 0, MSG_DONTWAIT | MSG_NOSIGNAL) < 0)
		WLog_DBG(TAG, "the remote end of the relay has already disconnected");

	EnterCriticalSection(&listener->lock);
	udpListenerRelay** link = &listener->relays;
	while (*link && (*link != relay))
		link = &(*link)->next;
	if (*link)
	{
		*link = relay->next;
		listener->relayCount--;
	}
	LeaveCriticalSection(&listener->lock);
	udplistener_free_relay(listener, relay);
}

/* A single thread serves all relays; it is the only entity that ends a relay
 * unless the listener is stopped. */
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
				/* Held under lock; see freerdp_udp_listener_add_relay(). */
				InterlockedIncrement(&listener->relayFailed);
				LeaveCriticalSection(&listener->lock);
				WLog_ERR(TAG, "stopping relays: out of memory");
				break;
			}
			capacity = count;
		}
		udpsocket_poll_set(&fds[0], listener->relayWake[1]);
		polled[0] = nullptr;
		size_t used = 1;
		for (udpListenerRelay* relay = listener->relays; relay; relay = relay->next)
		{
			udpsocket_poll_set(&fds[used], relay->sockfd);
			polled[used++] = relay;
		}
		LeaveCriticalSection(&listener->lock);

		if (udpsocket_poll(fds, used) < 0)
		{
			if (WSAGetLastError() == WSAEINTR)
				continue;
			WLog_ERR(TAG, "relays stopping: poll failed");
			/* Held under lock; see freerdp_udp_listener_add_relay(). */
			EnterCriticalSection(&listener->lock);
			InterlockedIncrement(&listener->relayFailed);
			LeaveCriticalSection(&listener->lock);
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
			udpListenerRelay* relay = polled[x];
			const BOOL ending = (InterlockedCompareExchange(&relay->ending, 0, 0) != 0);
			if (ending || ((fds[x].revents != 0) && !udplistener_relay_forward(relay)))
				udplistener_end_relay(listener, relay);
		}
	}

	/* After a fatal error, stop serving relays and close their sockets so other
	 * processes are notified. */
	if (InterlockedCompareExchange(&listener->relayFailed, 0, 0) != 0)
	{
		while (TRUE)
		{
			EnterCriticalSection(&listener->lock);
			udpListenerRelay* relay = listener->relays;
			LeaveCriticalSection(&listener->lock);
			if (!relay)
				break;
			udplistener_end_relay(listener, relay);
		}
	}

	free((void*)polled);
	free(fds);
	ExitThread(0);
	return 0;
}

/* Caller must hold the lock. */
WINPR_ATTR_NODISCARD
static BOOL udplistener_start_relays(freerdp_udp_listener* listener)
{
	WINPR_ASSERT(listener);

	if (listener->relayThread)
		return TRUE;

	if (!udpsocket_message_pair(listener->relayWake))
		return FALSE;
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

BOOL freerdp_udp_listener_add_relay(freerdp_udp_listener* listener, const BYTE cookieHash[32],
                                    int fd)
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

	/* Ensure one datagram per message, even for fds from other processes. */
	if (!udpsocket_is_message_socket(relay->sockfd))
		goto fail;

	/* Relay thread must not block on any single socket while reading. */
	u_long nonBlocking = 1;
	if (_ioctlsocket(relay->sockfd, FIONBIO, &nonBlocking) != 0)
		goto fail;

	freerdp_udp_flow_set_receive(relay->flow, udplistener_relay_receive, relay);

	/* Reserve the relay slot and start the thread before registering the hash to minimize the
	 * chance that datagrams reach fd if this function fails. */
	EnterCriticalSection(&listener->lock);
	const BOOL reserved = (listener->relayCount < UDPLISTENER_MAX_RELAYS) &&
	                      (InterlockedCompareExchange(&listener->relayFailed, 0, 0) == 0) &&
	                      udplistener_start_relays(listener);
	if (reserved)
		listener->relayCount++;
	LeaveCriticalSection(&listener->lock);
	if (!reserved)
	{
		if (InterlockedCompareExchange(&listener->relayFailed, 0, 0) != 0)
			WLog_ERR(
			    TAG,
			    "relay thread terminated due to an error; no further relays will be processed");
		else
			WLog_WARN(TAG, "unable to serve an additional relay");
		goto fail;
	}

	if (!udplistener_add_flow(listener, relay->flow, INVALID_SOCKET))
	{
		EnterCriticalSection(&listener->lock);
		listener->relayCount--;
		LeaveCriticalSection(&listener->lock);
		goto fail;
	}

	/* The relay thread sets relayFailed and ends all relays under the lock, ensuring that
	 * relays linked before the failure are cleaned up and no new relays are added. */
	EnterCriticalSection(&listener->lock);
	const BOOL linked = (InterlockedCompareExchange(&listener->relayFailed, 0, 0) == 0);
	if (linked)
	{
		relay->next = listener->relays;
		listener->relays = relay;
	}
	else
		listener->relayCount--;
	LeaveCriticalSection(&listener->lock);
	if (!linked)
		goto fail;

	/* Sockets passed from another process may lose the close-on-exec flag. */
	udpsocket_set_no_inherit((SOCKET)fd);
	/* Notify the relay thread to poll this socket. */
	udplistener_wake_relays(listener);
	return TRUE;

fail:
	/* Free the flow; the caller retains ownership of fd. */
	freerdp_udp_flow_free(relay->flow);
	free(relay);
	return FALSE;
}

BOOL freerdp_udp_listener_remove_relay(freerdp_udp_listener* listener, const BYTE cookieHash[32])
{
	WINPR_ASSERT(listener);
	WINPR_ASSERT(cookieHash);

	BOOL found = FALSE;
	EnterCriticalSection(&listener->lock);
	for (udpListenerRelay* relay = listener->relays; relay && !found; relay = relay->next)
	{
		if (memcmp(relay->flow->cookieHash, cookieHash, sizeof(relay->flow->cookieHash)) == 0)
		{
			/* Only the relay thread performs the actual cleanup. */
			InterlockedIncrement(&relay->ending);
			found = TRUE;
		}
	}
	if (found)
		udplistener_wake_relays(listener);
	LeaveCriticalSection(&listener->lock);
	return found;
}
