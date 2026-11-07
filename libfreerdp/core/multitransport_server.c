/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multitransport: the server side of the reliable UDP tunnel
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

/* The tunnel data that waits for the main loop, at most. A client sends mostly input and
 * acknowledgements, so this is much more than it needs. The client cannot send its data again, so
 * past this limit the connection ends. Without the limit, a client could fill the memory of the
 * server while the main loop waits, for example for a write. */
#define MULTITRANSPORT_SERVER_QUEUE_LIMIT (32 * 1024 * 1024)

/* [MS-RDPBCGR] 2.2.14.3 headerTypeId of a request */
#define TYPE_ID_AUTODETECT_REQUEST 0x00

/* The application's hook, on the thread that runs the main loop. */
static void multitransport_server_link(rdpMultitransport* multi, FreeRDP_UdpLinkState state)
{
	WINPR_ASSERT(multi);
	if (multi->udpCallbacks.LinkChanged)
		multi->udpCallbacks.LinkChanged(multi->udpCustom, state);
}

/* Runs on the tunnel thread. These two are only notes for the main loop. The main loop reads them
 * before the queued events. */
static void multitransport_server_tunnel_event(void* custom, rdpEmt* emt, RDPEMT_EVENT event,
                                               const BYTE* data, size_t length)
{
	rdpMultitransport* multi = custom;
	WINPR_ASSERT(multi);

	switch (event)
	{
		case RDPEMT_EVENT_CLAIMED:
			if (InterlockedCompareExchange(&multi->serverClaimed, 1, 0) != 0)
				WLog_DBG(TAG, "the main loop has not seen the last claim yet");
			(void)SetEvent(multi->event);
			break;
		case RDPEMT_EVENT_WRITABLE:
			if (InterlockedCompareExchange(&multi->serverWritable, 1, 0) != 0)
				WLog_DBG(TAG, "the main loop has not seen the last room yet");
			(void)SetEvent(multi->event);
			break;
		case RDPEMT_EVENT_DATA:
		case RDPEMT_EVENT_AUTODETECT:
		{
			const LONG add = (length > (size_t)MULTITRANSPORT_SERVER_QUEUE_LIMIT)
			                     ? MULTITRANSPORT_SERVER_QUEUE_LIMIT + 1
			                     : (LONG)length;
			/* add only what fits, so that the count cannot grow past the limit */
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
				/* the main loop ends the connection, because data is lost */
				if (InterlockedCompareExchange(&multi->tunnelEventLost, 1, 0) == 0)
					WLog_ERR(TAG, "the client sends more tunnel data than the server takes");
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

/* Sends the datagrams of the client that has this cookie to the tunnel's flow. */
WINPR_ATTR_NODISCARD
static BOOL multitransport_server_route(rdpMultitransport* multi, rdpEmt* emt)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(emt);

	rdpUdpFlow* flow = rdpemt_get_flow(emt);
	if (multi->udpCallbacks.OfferTunnel)
		return multi->udpCallbacks.OfferTunnel(multi->udpCustom, flow,
		                                       freerdp_udp_flow_get_cookie_hash(flow));

	/* [MS-RDPEMT] 1.3: the client sends its UDP datagrams to the address and port of its TCP
	 * connection. The client accepts answers only from that address and port. On a host with
	 * several addresses, this needs a socket on this exact address. If not, the routing table can
	 * select a different address. */
	struct sockaddr_storage local = WINPR_C_ARRAY_INIT;
	size_t localLength = 0;
	if (!transport_get_local_address(multi->rdp->transport, &local, &localLength))
	{
		WLog_INFO(TAG, "not offering UDP, the main connection is not on a TCP socket");
		return FALSE;
	}
	if ((local.ss_family != AF_INET) && (local.ss_family != AF_INET6))
	{
		WLog_INFO(TAG, "not offering UDP, the main connection is not over TCP/IP");
		return FALSE;
	}
	return udplistener_expect_on_address((const struct sockaddr*)&local, localLength, flow);
}

/* Makes the tunnel end for the next request. It uses the certificate and key of the main
 * connection. The client compares the tunnel's TLS session with these. */
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
		WLog_INFO(TAG, "not offering UDP, the server has no TLS certificate");
		return FALSE;
	}

	X509* x509 = freerdp_certificate_get_x509(cert);
	if (!x509)
	{
		WLog_INFO(TAG, "not offering UDP, the server certificate has no X.509 form");
		return FALSE;
	}
	EVP_PKEY* evp = freerdp_key_get_evp_pkey(key);
	if (!evp)
		return FALSE;
	rdpEmt* emt = rdpemt_new_server(WLog_Get(TAG), multi->reliableReqId, multi->reliableCookie,
	                                x509, evp, multi->udpCallbacks.ManualWorker,
	                                multitransport_server_tunnel_event, multi);
	EVP_PKEY_free(evp);
	if (!emt)
	{
		WLog_WARN(TAG, "cannot wait for a UDP tunnel, not offering one");
		return FALSE;
	}
	rdpemt_set_send_limit(emt, multi->udpSendLimit);

	if (!multitransport_server_route(multi, emt))
	{
		WLog_WARN(TAG, "no way to the client's datagrams, not offering a UDP tunnel");
		rdpemt_free(emt);
		return FALSE;
	}

	EnterCriticalSection(&multi->lock);
	/* one request per connection; without this step, an earlier tunnel leaks */
	rdpEmt* old = multi->reliable;
	multi->reliable = emt;
	multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_PENDING;
	multi->serverTunnelUp = FALSE;
	multi->serverResponseOk = FALSE;
	LeaveCriticalSection(&multi->lock);
	rdpemt_free(old);
	WLog_INFO(TAG, "offering a reliable UDP tunnel, request %" PRIu32, multi->reliableReqId);
	multitransport_server_link(multi, FREERDP_UDP_LINK_OFFERED);
	return TRUE;
}

/* Soft-Sync on both sides is the only way this server moves channels onto a tunnel. A server
 * advertises Soft-Sync only when it has the instruction to offer a tunnel. */
WINPR_ATTR_NODISCARD
static BOOL multitransport_server_has_tunnel(const rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(multi->rdp);
	const UINT32 flags =
	    freerdp_settings_get_uint32(multi->rdp->settings, FreeRDP_MultitransportFlags);
	return rdpemt_is_supported() && ((flags & (UINT32)SOFTSYNC_TCP_TO_UDP) != 0);
}

MT_OFFER multitransport_server_offer(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);

	/* A server without Soft-Sync has no tunnel for its request. The client's attempt fails, and the
	 * client's response continues the connection sequence. */
	if (!multitransport_server_has_tunnel(multi))
		return MT_OFFER_PLAIN;

	/* The tunnel waits for the client that has this cookie. Without the tunnel, the server does not
	 * send the request, and the connection continues over TCP. */
	if (!multitransport_server_start_tunnel(multi))
		return MT_OFFER_NONE;

	/* The connection sequence continues immediately, the same as on a Windows server. [MS-RDPBCGR]
	 * 1.3.1.1: the response can come in any later state. */
	multi->serverTunnelOffered = TRUE;
	multi->serverResponsePending = TRUE;
	return MT_OFFER_TUNNEL;
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
	WLog_INFO(TAG, "the client accepted the reliable UDP tunnel");
	multitransport_server_link(multi, FREERDP_UDP_LINK_LINKED);
	return multitransport_notify_state(multi, FREERDP_MULTITRANSPORT_TUNNEL_READY);
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
		multitransport_server_link(multi, FREERDP_UDP_LINK_PROVEN);
	*rc = multitransport_server_check_ready(multi);
	return TRUE;
}

void multitransport_server_tunnel_gone(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	if (multitransport_is_server(multi))
		multitransport_server_link(multi, FREERDP_UDP_LINK_NONE);
}

state_run_t multitransport_server_tunnel_response(rdpMultitransport* multi, UINT32 reqId,
                                                  UINT32 hrResponse)
{
	WINPR_ASSERT(multi);

	/* The connection sequence did not wait for this PDU, so it can come in any state. The message
	 * channel stays open for the whole session. The server accepts only one answer per request.
	 * That answer never changes the connection state. */
	if (!multi->serverResponsePending)
	{
		if (reqId == multi->reliableReqId)
		{
			WLog_DBG(TAG, "ignoring a repeated multi-transport response");
			return STATE_RUN_SUCCESS;
		}
		WLog_WARN(TAG, "unsolicited multi-transport response for request %" PRIu32, reqId);
		return STATE_RUN_FAILED;
	}

	if (reqId != multi->reliableReqId)
	{
		WLog_WARN(
		    TAG, "multi-transport response for request %" PRIu32 ", expected %" PRIu32 ", ignoring",
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

	if (!ok)
	{
		WLog_INFO(TAG, "the client declined the UDP tunnel, hrResponse 0x%08" PRIx32, hrResponse);
		/* the application stops to use the flow before the flow is freed */
		multitransport_server_link(multi, FREERDP_UDP_LINK_NONE);
		rdpemt_free(emt);
	}
	else if (!multitransport_server_check_ready(multi))
		return STATE_RUN_FAILED;

	return STATE_RUN_SUCCESS;
}

void multitransport_tunnel_check(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(multi->rdp);

	/* [MS-RDPBCGR] 3.2.5.17: tunnel data that went directly to the channels counts when the main
	 * loop gets here. This is the same for the data that the main loop sends from the queue. */
	if (multi->reliable)
	{
		const UINT32 direct = rdpemt_take_direct_bytes(multi->reliable);
		rdpAutoDetect* autodetect = multi->rdp->autodetect;
		if (autodetect && autodetect->bandwidthMeasureStarted)
			autodetect->bandwidthMeasureByteCount += direct;
	}

	/* the main loop reads all queued events after this, so their bytes no longer count */
	LONG queued = InterlockedCompareExchange(&multi->serverQueuedBytes, 0, 0);
	while (queued != 0)
	{
		const LONG seen = InterlockedCompareExchange(&multi->serverQueuedBytes, 0, queued);
		if (seen == queued)
			break;
		queued = seen;
	}

	if (InterlockedCompareExchange(&multi->serverClaimed, 0, 1) != 0)
		multitransport_server_link(multi, FREERDP_UDP_LINK_CLAIMED);
	if ((InterlockedCompareExchange(&multi->serverWritable, 0, 1) != 0) &&
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

	/* the application stops to use the flow before the flow is freed */
	multitransport_server_link(multi, FREERDP_UDP_LINK_NONE);
	multitransport_reset(multi);
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

	/* [MS-RDPBCGR] 2.2.14.1: the same PDUs as on the main connection, in a subheader */
	BYTE request[6] = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticInit(&sbuffer, request, sizeof(request));
	Stream_Write_UINT8(s, 6);                          /* headerLength */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId */
	Stream_Write_UINT16(s, sequenceNumber);            /* sequenceNumber */
	Stream_Write_UINT16(s, requestType);               /* requestType */
	*rc = multitransport_send_autodetect_response(multi, request, sizeof(request));
	return TRUE;
}

BOOL multitransport_is_write_blocked(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);

	BOOL blocked = FALSE;
	EnterCriticalSection(&multi->lock);
	if (multi->reliable)
		blocked = rdpemt_is_write_blocked(multi->reliable);
	LeaveCriticalSection(&multi->lock);
	return blocked;
}

DWORD multitransport_server_add_event(rdpMultitransport* multi, HANDLE* events, DWORD used,
                                      DWORD count)
{
	WINPR_ASSERT(multi);
	WINPR_ASSERT(events);

	if ((used == 0) || (used >= count))
		return used;

	/* A server with a UDP tunnel: freerdp_peer_check_fds handles the tunnel events that come from
	 * the client's datagrams */
	if (!multitransport_server_has_tunnel(multi))
		return used;
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

	if (callbacks)
		multi->udpCallbacks = *callbacks;
	else
		memset(&multi->udpCallbacks, 0, sizeof(multi->udpCallbacks));
	multi->udpCustom = callbacks ? custom : nullptr;
	return TRUE;
}

BOOL freerdp_peer_get_udp_send_state(freerdp_peer* peer, FreeRDP_UdpSendState* state)
{
	rdpMultitransport* multi = multitransport_from_peer(peer);
	if (!multi || !state)
		return FALSE;

	EnterCriticalSection(&multi->lock);
	const BOOL rc = multi->reliable != nullptr;
	if (rc)
		rdpemt_get_send_state(multi->reliable, state);
	LeaveCriticalSection(&multi->lock);
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
