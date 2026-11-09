/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server UDP listener: per-client datagram flow
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
#include <winpr/synch.h>
#include <winpr/winsock.h>

#include <freerdp/log.h>

#include "rdpudp.h"
#include "udplistener_internal.h"

#define TAG FREERDP_TAG("core.udplistener")

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

	/* A registered flow keeps its listener: the last one removed releases a freed listener. */
	if (flow->listener)
		// NOLINTNEXTLINE(clang-analyzer-unix.Malloc): see above
		freerdp_udp_listener_remove_flow(flow->listener, flow);

	if (flow->relay != INVALID_SOCKET)
	{
		/* connectionless sockets don't signal closure on their own */
		if (_send(flow->relay, "", 0, MSG_NOSIGNAL) < 0)
			WLog_DBG(TAG, "the flow relay is already gone");
		closesocket(flow->relay);
		if (!WSACloseEvent(flow->relayEvent))
			WLog_WARN(TAG, "failed to close the socket event");
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

	/* one datagram per message, consistent with freerdp_udp_listener_add_relay() */
	if (!udpsocket_is_message_socket((SOCKET)fd))
		return FALSE;

	HANDLE event = WSACreateEvent();
	if (!event)
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&flow->lock);
	/* this also sets the socket to non-blocking mode */
	if ((flow->relay == INVALID_SOCKET) && (WSAEventSelect((SOCKET)fd, event, FD_READ) == 0))
	{
		flow->relay = (SOCKET)fd;
		flow->relayEvent = event;
		/* sockets passed from another process lose the close-on-exec flag */
		udpsocket_set_no_inherit((SOCKET)fd);
		rc = TRUE;
	}
	LeaveCriticalSection(&flow->lock);

	if (!rc && !WSACloseEvent(event))
		WLog_WARN(TAG, "failed to close the socket event");
	return rc;
}

/* Grow the flow's ring and reset queued datagrams to the start. Caller must hold the flow lock. */
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
	/* if no ring exists, nothing is queued */
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

BOOL freerdp_udp_flow_deliver(rdpUdpFlow* flow, const BYTE* data, size_t length)
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
			/* RDP-UDP timers will detect the unreachable client */
			WLog_DBG(TAG, "flow relay has been closed");
			flow->relayClosed = TRUE;
		}
	}
	LeaveCriticalSection(&flow->lock);
	return rc;
}

void udpflow_wake(rdpUdpFlow* flow)
{
	WINPR_ASSERT(flow);
	/* Use the flow event, not the relay socket event; on non-Windows platforms, setting the
	 * latter triggers a WinPR socket write. The worker waits on both, see udpflow_get_events(). */
	(void)SetEvent(flow->event);
}

DWORD udpflow_get_events(rdpUdpFlow* flow, HANDLE events[2])
{
	WINPR_ASSERT(flow);
	WINPR_ASSERT(events);

	DWORD count = 0;
	EnterCriticalSection(&flow->lock);
	events[count++] = flow->event;
	if ((flow->relay != INVALID_SOCKET) && !flow->relayClosed)
		events[count++] = flow->relayEvent;
	LeaveCriticalSection(&flow->lock);
	return count;
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
	/* hold lock as the listener may close sockets during send */
	if (!sendFn && (flow->relay != INVALID_SOCKET))
	{
		const int sent = _send(flow->relay, (const char*)data,
		                       WINPR_ASSERTING_INT_CAST(int, length), MSG_NOSIGNAL);
		rc = (sent >= 0) || rdpudp_socket_error_is_transient(WSAGetLastError());
	}
	else if (!sendFn && (flow->sockfd != INVALID_SOCKET))
	{
		/* Fall back to system-chosen source address only if the local address is gone; the client
		 * only accepts replies from the address it sent to, so not on other errors. */
		BOOL fallback = !flow->local.valid;
		if (!fallback)
		{
			rc = udpsocket_send_from(flow->sockfd, &flow->peer, flow->peerLength, &flow->local,
			                         data, length);
			if (!rc)
			{
				int error = 0;
				fallback = udpsocket_source_address_unavailable(&error);
				rc = !fallback && rdpudp_socket_error_is_transient(error);
			}
		}
		if (fallback)
		{
			const int sent =
			    _sendto(flow->sockfd, (const char*)data, WINPR_ASSERTING_INT_CAST(int, length), 0,
			            (const struct sockaddr*)&flow->peer,
			            WINPR_ASSERTING_INT_CAST(int, flow->peerLength));
			rc = (sent >= 0) || rdpudp_socket_error_is_transient(WSAGetLastError());
		}
	}
	/* The callback runs under the lock, so freerdp_udp_flow_set_send() waits for it. */
	if (sendFn)
		rc = sendFn(custom, data, length);
	LeaveCriticalSection(&flow->lock);
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
