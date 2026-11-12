/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server side of the reliable UDP multitransport tunnel
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

#include <stddef.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/interlocked.h>
#include <winpr/synch.h>
#include <winpr/winsock.h>
#include <freerdp/config.h>

#if !defined(_WIN32)
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include <freerdp/log.h>
#include <freerdp/server/udp.h>

#include "settings.h"
#include "rdp.h"
#include "transport.h"
#include "rdpemt.h"
#include "udplistener.h"
#include "multitransport_internal.h"
#include "../crypto/certificate.h"
#include "../crypto/privatekey.h"

#define TAG FREERDP_TAG("core.multitransport")

/* Upper bound on tunnel data queued for the main loop. Since data is already acked and
 * cannot be retransmitted, exceeding this limit ends the connection to prevent server
 * memory exhaustion if the main loop is blocked (e.g. on a write). The tunnel stops taking
 * data from the client at MULTITRANSPORT_SERVER_QUEUE_HOLD, so the limit is not reached. */
#define MULTITRANSPORT_SERVER_QUEUE_LIMIT (32 * 1024 * 1024)
/* Above this the tunnel accepts no more data from the client (rdpemt_set_receive_hold): a main
 * loop that is busy or blocked then slows the client down as it would on TCP. */
#define MULTITRANSPORT_SERVER_QUEUE_HOLD (MULTITRANSPORT_SERVER_QUEUE_LIMIT / 2)
/* Per-event overhead (event, stream, and allocations) to ensure many small events do
 * not exceed the memory limit. */
#define MULTITRANSPORT_SERVER_EVENT_COST 128

/* Request headerTypeId per [MS-RDPBCGR] 2.2.14.3 */
#define TYPE_ID_AUTODETECT_REQUEST 0x00

/* Invokes the application's callback hook; must be called from the main loop thread. */
static void multitransport_server_notify_link_state(rdpMultitransport* multi,
                                                    FreeRDP_UdpLinkState state)
{
	WINPR_ASSERT(multi);
	if (multi->udpCallbacks.LinkChanged)
		multi->udpCallbacks.LinkChanged(multi->udpCustom, state);
}

/* What an event adds to serverQueuedBytes. */
WINPR_ATTR_NODISCARD
static LONG multitransport_server_event_cost(size_t length)
{
	return (length > (size_t)(MULTITRANSPORT_SERVER_QUEUE_LIMIT - MULTITRANSPORT_SERVER_EVENT_COST))
	           ? MULTITRANSPORT_SERVER_QUEUE_LIMIT + 1
	           : (LONG)length + MULTITRANSPORT_SERVER_EVENT_COST;
}

/* Runs on the tunnel thread before each client datagram. */
WINPR_ATTR_NODISCARD
static BOOL multitransport_server_receive_hold(void* custom)
{
	rdpMultitransport* multi = custom;
	WINPR_ASSERT(multi);
	return (InterlockedCompareExchange(&multi->serverQueuedBytes, 0, 0) >
	        MULTITRANSPORT_SERVER_QUEUE_HOLD) ||
	       (InterlockedCompareExchange(&multi->serverDataHold, 0, 0) != 0);
}

void multitransport_server_event_taken(rdpMultitransport* multi, const mt_event* ev)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(ev);
	if ((ev->type != MT_EVENT_DATA) && (ev->type != MT_EVENT_AUTODETECT))
		return;
	if (!multitransport_is_server(multi))
		return;

	const LONG cost = multitransport_server_event_cost(ev->length);
	LONG queued = InterlockedCompareExchange(&multi->serverQueuedBytes, 0, 0);
	while (TRUE)
	{
		const LONG left = (queued > cost) ? queued - cost : 0;
		const LONG seen = InterlockedCompareExchange(&multi->serverQueuedBytes, left, queued);
		if (seen == queued)
			break;
		queued = seen;
	}
}

void multitransport_hold_tunnel_data(rdpMultitransport* multi, BOOL hold)
{
	if (!multi)
		return;
	/* compare-exchange: WinPR's InterlockedExchange reads the old value non-atomically */
	if (InterlockedCompareExchange(&multi->serverDataHold, hold ? 1 : 0, hold ? 0 : 1) ==
	    (hold ? 0 : 1))
		WLog_DBG(TAG, "tunnel data from the client is %s", hold ? "held back" : "accepted again");
}

BOOL multitransport_tunnel_has_failed(rdpMultitransport* multi)
{
	if (!multi)
		return FALSE;
	EnterCriticalSection(&multi->lock);
	const BOOL failed = multi->reliable && rdpemt_has_failed(multi->reliable);
	LeaveCriticalSection(&multi->lock);
	return failed;
}

/* Runs on the tunnel thread. SYN_RECEIVED and WRITABLE events only set flags that the main
 * loop reads before processing queued events. */
static void multitransport_server_tunnel_event(void* custom, rdpEmt* emt, RDPEMT_EVENT event,
                                               const BYTE* data, size_t length)
{
	rdpMultitransport* multi = custom;
	WINPR_ASSERT(multi);

	switch (event)
	{
		case RDPEMT_EVENT_SYN_RECEIVED:
			if (InterlockedCompareExchange(&multi->serverSynPending, 1, 0) != 0)
				WLog_DBG(TAG,
				         "the main loop has not yet processed the previous SYN received event");
			(void)SetEvent(multi->event);
			break;
		case RDPEMT_EVENT_WRITABLE:
			if (InterlockedCompareExchange(&multi->serverWritablePending, 1, 0) != 0)
				WLog_DBG(TAG, "the main loop has not yet processed the previous writable event");
			(void)SetEvent(multi->event);
			break;
		case RDPEMT_EVENT_RESPONSE_DUE:
			/* Wakeup only; multitransport_tunnel_check handles the timeout. */
			(void)SetEvent(multi->event);
			break;
		case RDPEMT_EVENT_DATA:
		case RDPEMT_EVENT_AUTODETECT:
		{
			const LONG add = multitransport_server_event_cost(length);
			/* Only increment if it fits to ensure the count stays within the limit. */
			BOOL fits = TRUE;
			LONG queued = InterlockedCompareExchange(&multi->serverQueuedBytes, 0, 0);
			while (fits)
			{
				fits = (add <= MULTITRANSPORT_SERVER_QUEUE_LIMIT - queued);
				if (!fits)
					break;
				const LONG seen =
				    InterlockedCompareExchange(&multi->serverQueuedBytes, queued + add, queued);
				if (seen == queued)
					break;
				queued = seen;
			}
			if (!fits)
			{
				/* Data is lost; the main loop will terminate the connection. */
				if (InterlockedCompareExchange(&multi->tunnelEventLost, 1, 0) == 0)
					WLog_ERR(TAG, "client is sending more tunnel data than the server can process");
				(void)SetEvent(multi->event);
				break;
			}
			multitransport_tunnel_event(custom, emt, event, data, length);
			break;
		}
		default:
			multitransport_tunnel_event(custom, emt, event, data, length);
			break;
	}
}

/* Routes client datagrams associated with this cookie to the tunnel flow. */
WINPR_ATTR_NODISCARD
static BOOL multitransport_server_route_flow(rdpMultitransport* multi, rdpEmt* emt)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(emt);

	rdpUdpFlow* flow = rdpemt_get_flow(emt);
	if (multi->udpCallbacks.OfferTunnel)
		return multi->udpCallbacks.OfferTunnel(multi->udpCustom, flow,
		                                       freerdp_udp_flow_get_cookie_hash(flow));

	/* [MS-RDPEMT] 1.3: The client sends UDP datagrams to the TCP connection's address and
	 * port and only accepts replies from there. On multi-homed hosts, the socket must be
	 * bound to this exact address to prevent the routing table from picking a different
	 * source. */
	struct sockaddr_storage local = WINPR_C_ARRAY_INIT;
	size_t localLength = 0;
	if (!transport_get_local_address(multi->rdp->transport, &local, &localLength))
	{
		WLog_INFO(TAG, "UDP is not offered because the main connection is not using a TCP socket");
		return FALSE;
	}
	if ((local.ss_family != AF_INET) && (local.ss_family != AF_INET6))
	{
		WLog_INFO(TAG, "UDP is not offered because the main connection is not over TCP/IP");
		return FALSE;
	}
	return udplistener_add_flow_on_address((const struct sockaddr*)&local, localLength, flow);
}

static void multitransport_server_drop_pending_events(rdpMultitransport* multi);

/* Creates the server end of the tunnel for the next request. Uses the main
 * connection's certificate and key for the client to verify the tunnel's TLS session. */
WINPR_ATTR_NODISCARD
static BOOL multitransport_server_start_tunnel(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(multi->rdp);
	rdpSettings* settings = multi->rdp->settings;
	WINPR_ASSERT(settings);

	rdpCertificate* cert =
	    freerdp_settings_get_pointer_writable(settings, FreeRDP_RdpServerCertificate);
	const rdpPrivateKey* key = freerdp_settings_get_pointer(settings, FreeRDP_RdpServerRsaKey);
	if (!cert || !key)
	{
		WLog_INFO(TAG, "UDP is not being offered because the server lacks a TLS certificate");
		return FALSE;
	}

	X509* x509 = freerdp_certificate_get_x509(cert);
	if (!x509)
	{
		WLog_INFO(TAG, "UDP not offered because the server certificate lacks X.509 format");
		return FALSE;
	}
	EVP_PKEY* evp = freerdp_key_get_evp_pkey(key);
	if (!evp)
		return FALSE;
	/* The tunnel's TLS signs with RSA or ECDSA on P-256 or P-384 (emt_start_tls). With another
	 * key every handshake would fail, after the client had spent its attempt. */
	const int keyType = EVP_PKEY_base_id(evp);
	const int keyBits = EVP_PKEY_bits(evp);
	if ((keyType != EVP_PKEY_RSA) &&
	    ((keyType != EVP_PKEY_EC) || ((keyBits != 256) && (keyBits != 384))))
	{
		WLog_INFO(TAG, "UDP is not offered: the tunnel's TLS cannot use the server's key type");
		EVP_PKEY_free(evp);
		return FALSE;
	}
	rdpEmt* emt = rdpemt_new_server(WLog_Get(TAG), multi->reliableReqId, multi->reliableCookie,
	                                x509, evp, multi->udpCallbacks.ManualWorker,
	                                multitransport_server_tunnel_event, multi);
	EVP_PKEY_free(evp);
	if (!emt)
	{
		WLog_WARN(TAG, "unable to wait for a UDP tunnel, so one will not be offered");
		return FALSE;
	}
	rdpemt_set_receive_hold(emt, multitransport_server_receive_hold);
	/* Apply the main connection's TLS limits to the tunnel. */
	if (!rdpemt_set_tls_settings(emt, freerdp_settings_get_uint16(settings, FreeRDP_TLSMinVersion),
	                             freerdp_settings_get_uint16(settings, FreeRDP_TLSMaxVersion),
	                             freerdp_settings_get_uint32(settings, FreeRDP_TlsSecLevel),
	                             freerdp_settings_get_string(settings, FreeRDP_AllowedTlsCiphers)))
	{
		rdpemt_free(emt);
		return FALSE;
	}
	/* Record start time before OfferTunnel (which may block). The response wakeup fires
	 * RDPEMT_SERVER_RESPONSE_TIMEOUT_MS later. */
	multi->serverOfferTime = GetTickCount64();

	if (!multitransport_server_route_flow(multi, emt))
	{
		WLog_WARN(TAG, "unable to reach client datagrams, not offering a UDP tunnel");
		rdpemt_free(emt);
		return FALSE;
	}

	EnterCriticalSection(&multi->lock);
	/* One request per connection; free any existing tunnel to prevent leaks. */
	rdpEmt* old = multi->reliable;
	/* Under lock: freerdp_peer_set_udp_send_limit() may be called from another thread. */
	rdpemt_set_send_limit(emt, multi->udpSendLimit);
	multi->reliable = emt;
	multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_PENDING;
	multi->serverTunnelUp = FALSE;
	multi->serverResponseOk = FALSE;
	LeaveCriticalSection(&multi->lock);
	if (old)
	{
		/* Ensure the application stops using the old flow before it is freed. */
		multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_NONE);
		rdpemt_free(old);
		multitransport_server_drop_pending_events(multi);
	}
	WLog_INFO(TAG, "proposing a reliable UDP tunnel, request %" PRIu32, multi->reliableReqId);
	multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_OFFERED);
	return TRUE;
}

/* This server only migrates channels to a tunnel via Soft-Sync on both sides, and
 * only advertises Soft-Sync when configured to offer a tunnel. */
WINPR_ATTR_NODISCARD
static BOOL multitransport_server_has_tunnel(const rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(multi->rdp);
	const UINT32 flags =
	    freerdp_settings_get_uint32(multi->rdp->settings, FreeRDP_MultitransportFlags);
	return rdpemt_is_supported() && ((flags & (UINT32)SOFTSYNC_TCP_TO_UDP) != 0);
}

MT_REQUEST_MODE multitransport_server_request_mode(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);

	/* Without Soft-Sync, no tunnel is offered; the connection sequence waits for the client's
	 * response. */
	if (!multitransport_server_has_tunnel(multi))
		return MT_REQUEST_WAIT;

	/* The tunnel waits for the client using this cookie. If it cannot be established, no
	 * request is sent and the connection continues over TCP. */
	if (!multitransport_server_start_tunnel(multi))
		return MT_REQUEST_NONE;

	/* Mimicking Windows, continue the connection sequence immediately; per [MS-RDPBCGR] 1.3.1.1,
	 * the response may arrive in any later state. */
	multi->serverTunnelOffered = TRUE;
	multi->serverResponsePending = TRUE;
	return MT_REQUEST_TUNNEL;
}

BOOL multitransport_is_server(const rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(multi->rdp);
	return freerdp_settings_get_bool(multi->rdp->settings, FreeRDP_ServerMode);
}

BOOL multitransport_server_tunnel_offered(const rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	return multi->serverTunnelOffered;
}

WINPR_ATTR_NODISCARD
static BOOL multitransport_server_check_ready(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);

	EnterCriticalSection(&multi->lock);
	const BOOL ready = multi->reliable && multi->serverTunnelUp && multi->serverResponseOk &&
	                   (multi->reliableState != FREERDP_MULTITRANSPORT_TUNNEL_READY);
	if (ready)
		multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_READY;
	LeaveCriticalSection(&multi->lock);

	if (!ready)
		return TRUE;
	WLog_INFO(TAG, "client accepted the reliable UDP tunnel");
	multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_READY);
	return multitransport_notify_state(multi, FREERDP_MULTITRANSPORT_TUNNEL_READY);
}

/* Prevent pending flags of the freed tunnel from reaching the application after LinkChanged(NONE).
 */
static void multitransport_server_drop_pending_events(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	/* clear both, a || would skip the second one when the first was set */
	const LONG synPending = InterlockedCompareExchange(&multi->serverSynPending, 0, 1);
	const LONG writable = InterlockedCompareExchange(&multi->serverWritablePending, 0, 1);
	if ((synPending != 0) || (writable != 0))
		WLog_DBG(TAG, "dropped pending SYN and writable events for a terminated tunnel");
}

BOOL multitransport_server_tunnel_ready(rdpMultitransport* multi, BOOL* rc)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(rc);

	if (!multitransport_is_server(multi))
		return FALSE;

	EnterCriticalSection(&multi->lock);
	const BOOL tunnel = multi->reliable != nullptr;
	if (tunnel)
		multi->serverTunnelUp = TRUE;
	LeaveCriticalSection(&multi->lock);
	if (tunnel)
		multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_VERIFIED);
	*rc = multitransport_server_check_ready(multi);
	return TRUE;
}

void multitransport_server_tunnel_closed(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	if (multitransport_is_server(multi))
	{
		multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_NONE);
		multitransport_server_drop_pending_events(multi);
	}
}

BOOL multitransport_server_response_failed(const rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	return multi->serverResponseFailed;
}

WINPR_ATTR_NODISCARD
static state_run_t multitransport_server_process_response(rdpMultitransport* multi, UINT32 reqId,
                                                          UINT32 hrResponse)
{
	WINPR_ASSERT(multi);

	/* The connection sequence does not wait for this PDU, so it may arrive in any state
	 * (the message channel remains open for the session). Only one answer per request is
	 * accepted; it does not change the connection state. */
	if (!multi->serverResponsePending)
	{
		if (reqId == multi->reliableReqId)
		{
			WLog_DBG(TAG, "skipping a duplicate multi-transport response");
			return STATE_RUN_SUCCESS;
		}
		WLog_WARN(TAG, "received an unexpected multi-transport response for request %" PRIu32,
		          reqId);
		return STATE_RUN_FAILED;
	}

	if (reqId != multi->reliableReqId)
	{
		WLog_WARN(TAG,
		          "received multi-transport response for request %" PRIu32 ", but expected %" PRIu32
		          ", ignoring",
		          reqId, multi->reliableReqId);
		return STATE_RUN_SUCCESS;
	}
	multi->serverResponsePending = FALSE;

	EnterCriticalSection(&multi->lock);
	rdpEmt* emt = multi->reliable;
	const BOOL ok = emt && SUCCEEDED((HRESULT)hrResponse);
	if (ok)
		multi->serverResponseOk = TRUE;
	else
	{
		multi->reliable = nullptr;
		multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_NONE;
	}
	LeaveCriticalSection(&multi->lock);

	if (!ok && emt)
	{
		WLog_INFO(TAG, "client rejected the UDP tunnel, hrResponse 0x%08" PRIx32, hrResponse);
		/* Application must stop using the flow before it is freed */
		multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_NONE);
		rdpemt_free(emt);
		multitransport_server_drop_pending_events(multi);
	}
	else if (!ok)
		WLog_DBG(TAG, "multi-transport response 0x%08" PRIx32 " received after the tunnel ended",
		         hrResponse);
	else if (!multitransport_server_check_ready(multi))
		return STATE_RUN_FAILED;

	return STATE_RUN_SUCCESS;
}

state_run_t multitransport_server_tunnel_response(rdpMultitransport* multi, UINT32 reqId,
                                                  UINT32 hrResponse)
{
	const state_run_t rc = multitransport_server_process_response(multi, reqId, hrResponse);
	if (state_run_failed(rc))
		multi->serverResponseFailed = TRUE;
	return rc;
}

void multitransport_tunnel_check(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(multi->rdp);

	/* [MS-RDPBCGR] 3.2.5.17: Count tunnel data delivered directly to channels, as well as
	 * data dispatched from the queue by the main loop. */
	/* Server locks to prevent the tunnel from being destroyed during the check (see
	 * multitransport_server_disconnect). On clients, only the main loop modifies the tunnel
	 * to avoid blocking on a sending thread. */
	const BOOL server = multitransport_is_server(multi);
	UINT32 direct = 0;
	if (server)
		EnterCriticalSection(&multi->lock);
	if (multi->reliable)
		direct = rdpemt_take_direct_bytes(multi->reliable);
	if (server)
		LeaveCriticalSection(&multi->lock);
	rdpAutoDetect* autodetect = multi->rdp->autodetect;
	if (autodetect && autodetect->bandwidthMeasureStarted)
		autodetect->bandwidthMeasureByteCount += direct;

	if (InterlockedCompareExchange(&multi->serverSynPending, 0, 1) != 0)
		multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_SYN_RECEIVED);

	/* Tear down the tunnel if the client fails to send the Initiate Multitransport
	 * Response within the timeout to avoid pinning a flow, thread, and TLS session. A late
	 * response will find no pending request. */
	if (multitransport_is_server(multi) && multi->serverResponsePending &&
	    (GetTickCount64() - multi->serverOfferTime > RDPEMT_SERVER_RESPONSE_TIMEOUT_MS))
	{
		multi->serverResponsePending = FALSE;
		EnterCriticalSection(&multi->lock);
		rdpEmt* emt = multi->reliable;
		multi->reliable = nullptr;
		multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_NONE;
		LeaveCriticalSection(&multi->lock);
		if (emt)
		{
			WLog_INFO(TAG, "multi-transport response timed out; closing the UDP tunnel");
			multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_NONE);
			rdpemt_free(emt);
			multitransport_server_drop_pending_events(multi);
		}
	}
	if ((InterlockedCompareExchange(&multi->serverWritablePending, 0, 1) != 0) &&
	    multi->udpCallbacks.WriteUnblocked)
		multi->udpCallbacks.WriteUnblocked(multi->udpCustom);
}

void multitransport_server_disconnect(rdpContext* context)
{
	if (!context || !context->rdp)
		return;
	rdpMultitransport* multi = context->rdp->multitransport;
	if (!multi || !multitransport_is_server(multi))
		return;

	EnterCriticalSection(&multi->lock);
	const BOOL tunnel = (multi->reliable != nullptr);
	LeaveCriticalSection(&multi->lock);
	if (!tunnel)
		return;

	/* Application must stop using the flow before it is freed */
	multitransport_server_notify_link_state(multi, FREERDP_UDP_LINK_NONE);
	multitransport_reset(multi);
}

BOOL multitransport_queue_dvc(rdpMultitransport* multi, const BYTE* data, size_t length)
{
	if (!multi)
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&multi->lock);
	if (multi->reliable && (multi->reliableState == FREERDP_MULTITRANSPORT_TUNNEL_READY))
		rc = rdpemt_send_data(multi->reliable, data, length);
	LeaveCriticalSection(&multi->lock);
	return rc;
}

BOOL multitransport_flush_dvc(rdpMultitransport* multi)
{
	if (!multi)
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&multi->lock);
	if (multi->reliable && (multi->reliableState == FREERDP_MULTITRANSPORT_TUNNEL_READY))
		rc = rdpemt_flush(multi->reliable);
	LeaveCriticalSection(&multi->lock);
	return rc;
}

BOOL multitransport_server_send_autodetect(rdpMultitransport* multi, RDP_TRANSPORT_TYPE transport,
                                           UINT16 sequenceNumber, UINT16 requestType, BOOL* rc)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(rc);

	if (transport != RDP_TRANSPORT_UDP_R)
		return FALSE;
	/* Tunnel measurements are not sent over TCP to avoid reporting TCP results or
	 * obtaining mixed measurements if the tunnel activates between Start and Stop.
	 * The request fails if the tunnel is not ready. */
	EnterCriticalSection(&multi->lock);
	const BOOL ready =
	    multi->reliable && (multi->reliableState == FREERDP_MULTITRANSPORT_TUNNEL_READY);
	LeaveCriticalSection(&multi->lock);
	if (!ready)
	{
		WLog_DBG(TAG, "no UDP tunnel for the auto-detect request %" PRIu16, sequenceNumber);
		*rc = FALSE;
		return TRUE;
	}

	/* [MS-RDPBCGR] 2.2.14.1: Same PDU as on the main connection, wrapped in a subheader */
	BYTE request[6] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, request, sizeof(request));
	Stream_Write_UINT8(s, 6);                          /* Header length */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* Header type identifier */
	Stream_Write_UINT16(s, sequenceNumber);            /* The sequence number */
	Stream_Write_UINT16(s, requestType);               /* The request type */
	*rc = multitransport_send_autodetect_response(multi, request, sizeof(request));
	return TRUE;
}

DWORD multitransport_server_add_event(rdpMultitransport* multi, HANDLE* events, DWORD used,
                                      DWORD count)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(events);

	if (used == 0)
		return used;

	/* Let freerdp_peer_check_fds handle tunnel events triggered by client datagrams */
	if (!multitransport_server_has_tunnel(multi))
		return used;
	if (used >= count)
	{
		/* as for the transport's handles: without this one, tunnel events would wait for
		 * activity on TCP */
		WLog_ERR(TAG, "no room for the UDP tunnel's event handle, %" PRIu32 " given", count);
		return 0;
	}
	events[used] = multitransport_get_event_handle(multi);
	return used + 1;
}

WINPR_ATTR_NODISCARD
static rdpMultitransport* multitransport_from_peer(const freerdp_peer* peer)
{
	if (!peer || !peer->context || !peer->context->rdp)
		return nullptr;
	return peer->context->rdp->multitransport;
}

BOOL freerdp_peer_set_udp_callbacks(freerdp_peer* peer, const FreeRDP_UdpPeerCallbacks* callbacks,
                                    void* custom)
{
	rdpMultitransport* multi = multitransport_from_peer(peer);
	if (!multi)
		return FALSE;

	/* Callback struct version 3.34.0 ends with ManualWorker */
	const size_t required =
	    offsetof(FreeRDP_UdpPeerCallbacks, ManualWorker) + sizeof(callbacks->ManualWorker);
	if (callbacks && (callbacks->Size < required))
	{
		WLog_ERR(TAG,
		         "FreeRDP_UdpPeerCallbacks size %" PRIuz " is too small, expected at least %" PRIuz,
		         callbacks->Size, required);
		return FALSE;
	}

	memset(&multi->udpCallbacks, 0, sizeof(multi->udpCallbacks));
	if (callbacks)
	{
		const size_t size = (callbacks->Size < sizeof(multi->udpCallbacks))
		                        ? callbacks->Size
		                        : sizeof(multi->udpCallbacks);
		memcpy(&multi->udpCallbacks, callbacks, size);
		multi->udpCallbacks.Size = sizeof(multi->udpCallbacks);
	}
	multi->udpCustom = callbacks ? custom : nullptr;
	return TRUE;
}

BOOL freerdp_peer_get_udp_send_state(freerdp_peer* peer, FreeRDP_UdpSendState* state)
{
	rdpMultitransport* multi = multitransport_from_peer(peer);
	if (!multi || !state)
		return FALSE;
	/* State struct version 3.34.0 ends with rateBytesPerSecond */
	const size_t required =
	    offsetof(FreeRDP_UdpSendState, rateBytesPerSecond) + sizeof(state->rateBytesPerSecond);
	if (state->size < required)
	{
		WLog_ERR(TAG,
		         "the size of FreeRDP_UdpSendState is %" PRIuz
		         ", but it should be at least %" PRIuz,
		         state->size, required);
		return FALSE;
	}

	FreeRDP_UdpSendState full = WINPR_C_ARRAY_INIT;
	EnterCriticalSection(&multi->lock);
	const BOOL rc = multi->reliable != nullptr;
	if (rc)
		rdpemt_get_send_state(multi->reliable, &full);
	LeaveCriticalSection(&multi->lock);
	if (rc)
	{
		/* Update size to inform the caller how many bytes were filled */
		const size_t size = (state->size < sizeof(full)) ? state->size : sizeof(full);
		full.size = size;
		memcpy(state, &full, size);
	}
	return rc;
}

BOOL freerdp_peer_set_udp_send_limit(freerdp_peer* peer, size_t limit)
{
	rdpMultitransport* multi = multitransport_from_peer(peer);
	if (!multi)
		return FALSE;

	EnterCriticalSection(&multi->lock);
	multi->udpSendLimit = limit;
	if (multi->reliable)
		rdpemt_set_send_limit(multi->reliable, limit);
	LeaveCriticalSection(&multi->lock);
	return TRUE;
}
