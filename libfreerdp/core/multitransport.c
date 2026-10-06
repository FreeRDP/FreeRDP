/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MULTITRANSPORT PDUs
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
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
#include <winpr/collections.h>
#include <winpr/synch.h>
#include <winpr/winsock.h>
#include <freerdp/config.h>

#if !defined(_WIN32)
#include <netdb.h>
#include <sys/socket.h>
#endif
#include <freerdp/log.h>
#include <freerdp/multitransport.h>
#include <freerdp/channels/drdynvc.h>

#include "settings.h"
#include "rdp.h"
#include "transport.h"
#include "rdpemt.h"
#include "multitransport.h"

typedef enum
{
	MT_EVENT_READY,
	MT_EVENT_FAILED,
	MT_EVENT_DATA
} MT_EVENT_TYPE;

/* Tunnel events travel from the tunnel thread to the thread that runs the main loop, which is
 * where the Initiate Multitransport Response goes out and where the dynamic channel code is
 * called. */
typedef struct
{
	MT_EVENT_TYPE type;
	BYTE* data;
	size_t length;
} mt_event;

struct rdp_multitransport
{
	rdpRdp* rdp;

	MultiTransportRequestCb MtRequest;
	MultiTransportResponseCb MtResponse;

	/* server-side data */
	UINT32 reliableReqId;

	BYTE reliableCookie[RDPUDP_COOKIE_LEN];
	BYTE reliableCookieHash[RDPUDP_COOKIE_HASHLEN];

	/* client-side data */
	CRITICAL_SECTION lock;
	HANDLE event;
	wQueue* events;
	rdpEmt* reliable;
	FreeRDP_MultitransportTunnelState reliableState;
	BOOL reliableResponseSent;

	FreeRDP_MultitransportDvcCallbacks callbacks;
	BOOL haveCallbacks;
	void* custom;
};

enum
{
	RDPTUNNEL_ACTION_CREATEREQUEST = 0x00,
	RDPTUNNEL_ACTION_CREATERESPONSE = 0x01,
	RDPTUNNEL_ACTION_DATA = 0x02
};

#define TAG FREERDP_TAG("core.multitransport")

state_run_t multitransport_recv_request(rdpMultitransport* multi, wStream* s)
{
	WINPR_ASSERT(multi);
	rdpSettings* settings = multi->rdp->settings;

	if (freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
	{
		WLog_ERR(TAG, "not expecting a multi-transport request in server mode");
		return STATE_RUN_FAILED;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
		return STATE_RUN_FAILED;

	UINT32 requestId = 0;
	UINT16 requestedProto = 0;
	UINT16 reserved = 0;
	const BYTE* cookie = nullptr;

	Stream_Read_UINT32(s, requestId);      /* requestId (4 bytes) */
	Stream_Read_UINT16(s, requestedProto); /* requestedProtocol (2 bytes) */
	Stream_Read_UINT16(s, reserved);       /* reserved (2 bytes) */
	cookie = Stream_ConstPointer(s);
	Stream_Seek(s, RDPUDP_COOKIE_LEN); /* securityCookie (16 bytes) */
	if (reserved != 0)
	{
		/*
		 * If the reserved filed is not 0 the request PDU seems to contain some extra data.
		 * If the reserved value is 1, then two bytes of 0 (probably a version field)
		 * are followed by a JSON payload (not null terminated, until the end of the packet.
		 * There seems to be no dedicated length field)
		 *
		 * for now just ignore all that
		 */
		WLog_WARN(TAG,
		          "reserved is %" PRIu16 " instead of 0, skipping %" PRIuz "bytes of unknown data",
		          reserved, Stream_GetRemainingLength(s));
		if (!Stream_SafeSeek(s, Stream_GetRemainingLength(s)))
			return STATE_RUN_FAILED;
	}

	WINPR_ASSERT(multi->MtRequest);
	return multi->MtRequest(multi, requestId, requestedProto, cookie);
}

static BOOL multitransport_request_send(rdpMultitransport* multi, UINT32 reqId, UINT16 reqProto,
                                        const BYTE* cookie)
{
	WINPR_ASSERT(multi);
	UINT16 sec_flags = 0;
	wStream* s = rdp_message_channel_pdu_init(multi->rdp, &sec_flags);
	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 24))
	{
		Stream_Release(s);
		return FALSE;
	}

	Stream_Write_UINT32(s, reqId);              /* requestId (4 bytes) */
	Stream_Write_UINT16(s, reqProto);           /* requestedProtocol (2 bytes) */
	Stream_Zero(s, 2);                          /* reserved (2 bytes) */
	Stream_Write(s, cookie, RDPUDP_COOKIE_LEN); /* securityCookie (16 bytes) */

	return rdp_send_message_channel_pdu(multi->rdp, s, sec_flags | SEC_TRANSPORT_REQ);
}

state_run_t multitransport_server_request(rdpMultitransport* multi, UINT16 reqProto)
{
	WINPR_ASSERT(multi);

	/* TODO: move this static variable to the listener */
	static UINT32 reqId = 0;

	if (reqProto == INITIATE_REQUEST_PROTOCOL_UDPFECR)
	{
		multi->reliableReqId = reqId++;
		if (winpr_RAND(multi->reliableCookie, sizeof(multi->reliableCookie)) < 0)
			return STATE_RUN_FAILED;

		return multitransport_request_send(multi, multi->reliableReqId, reqProto,
		                                   multi->reliableCookie)
		           ? STATE_RUN_SUCCESS
		           : STATE_RUN_FAILED;
	}

	WLog_ERR(TAG, "only reliable transport is supported");
	return STATE_RUN_CONTINUE;
}

BOOL multitransport_client_send_response(rdpMultitransport* multi, UINT32 reqId, HRESULT hr)
{
	WINPR_ASSERT(multi);

	UINT16 sec_flags = 0;
	wStream* s = rdp_message_channel_pdu_init(multi->rdp, &sec_flags);
	if (!s)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(s, 28))
	{
		Stream_Release(s);
		return FALSE;
	}

	Stream_Write_UINT32(s, reqId); /* requestId (4 bytes) */

	/* [MS-RDPBCGR] 2.2.15.2 Client Initiate Multitransport Response PDU defines this as 4byte
	 * UNSIGNED but https://learn.microsoft.com/en-us/windows/win32/learnwin32/error-codes-in-com
	 * defines this as signed... assume the spec is (implicitly) assuming twos complement. */
	Stream_Write_INT32(s, hr); /* HResult (4 bytes) */
	return rdp_send_message_channel_pdu(multi->rdp, s, sec_flags | SEC_TRANSPORT_RSP);
}

state_run_t multitransport_recv_response(rdpMultitransport* multi, wStream* s)
{
	WINPR_ASSERT(multi && multi->rdp);
	WINPR_ASSERT(s);

	rdpSettings* settings = multi->rdp->settings;
	WINPR_ASSERT(settings);

	if (!freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
	{
		WLog_ERR(TAG, "client is not expecting a multi-transport resp packet");
		return STATE_RUN_FAILED;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return STATE_RUN_FAILED;

	UINT32 requestId = 0;
	UINT32 hr = 0;

	Stream_Read_UINT32(s, requestId); /* requestId (4 bytes) */
	Stream_Read_UINT32(s, hr);        /* hrResponse (4 bytes) */

	state_run_t res = STATE_RUN_SUCCESS;
	IFCALLRET(multi->MtResponse, res, multi, requestId, hr);
	return res;
}

static BOOL multitransport_soft_sync(const rdpMultitransport* multi)
{
	const UINT32 flags =
	    freerdp_settings_get_uint32(multi->rdp->settings, FreeRDP_MultitransportFlags);
	return (flags & SOFTSYNC_TCP_TO_UDP) != 0;
}

static state_run_t multitransport_decline(rdpMultitransport* multi, UINT32 reqId)
{
	return multitransport_client_send_response(multi, reqId, E_ABORT) ? STATE_RUN_SUCCESS
	                                                                  : STATE_RUN_FAILED;
}

static void mt_event_free(void* obj)
{
	mt_event* ev = obj;
	if (!ev)
		return;
	free(ev->data);
	free(ev);
}

/* Runs on the tunnel thread. */
static void multitransport_tunnel_event(void* custom, rdpEmt* emt, RDPEMT_EVENT event,
                                        const BYTE* data, size_t length)
{
	rdpMultitransport* multi = custom;
	WINPR_ASSERT(multi);
	WINPR_UNUSED(emt);

	mt_event* ev = calloc(1, sizeof(mt_event));
	if (!ev)
		return;

	switch (event)
	{
		case RDPEMT_EVENT_READY:
			ev->type = MT_EVENT_READY;
			break;
		case RDPEMT_EVENT_FAILED:
			ev->type = MT_EVENT_FAILED;
			break;
		case RDPEMT_EVENT_DATA:
		default:
			ev->type = MT_EVENT_DATA;
			ev->data = malloc(length);
			if (!ev->data)
			{
				free(ev);
				return;
			}
			memcpy(ev->data, data, length);
			ev->length = length;
			break;
	}

	if (!Queue_Enqueue(multi->events, ev))
	{
		mt_event_free(ev);
		return;
	}
	(void)SetEvent(multi->event);
}

/* The UDP listener sits on the same address and port as the TCP one. */
static BOOL multitransport_resolve_server(const rdpSettings* settings,
                                          struct sockaddr_storage* address, size_t* length)
{
	const char* hostname = freerdp_settings_get_string(settings, FreeRDP_ServerHostname);
	if (!hostname)
		return FALSE;

	char port[16] = WINPR_C_ARRAY_INIT;
	(void)_snprintf(port, sizeof(port), "%" PRIu32,
	                freerdp_settings_get_uint32(settings, FreeRDP_ServerPort));

	struct addrinfo hints = WINPR_C_ARRAY_INIT;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	struct addrinfo* result = nullptr;
	if ((getaddrinfo(hostname, port, &hints, &result) != 0) || !result)
		return FALSE;

	BOOL rc = FALSE;
	if (result->ai_addrlen <= sizeof(*address))
	{
		memcpy(address, result->ai_addr, result->ai_addrlen);
		*length = result->ai_addrlen;
		rc = TRUE;
	}
	freeaddrinfo(result);
	return rc;
}

static state_run_t multitransport_client_request(rdpMultitransport* multi, UINT32 reqId,
                                                 UINT16 reqProto, const BYTE* cookie)
{
	WINPR_ASSERT(multi);
	rdpRdp* rdp = multi->rdp;
	WINPR_ASSERT(rdp);
	rdpSettings* settings = rdp->settings;
	WINPR_ASSERT(settings);

	WLog_DBG(TAG, "Initiate Multitransport Request: requestId %" PRIu32 ", protocol 0x%04" PRIx16,
	         reqId, reqProto);

	if (reqProto != INITIATE_REQUEST_PROTOCOL_UDPFECR)
	{
		WLog_INFO(TAG, "declining lossy UDP, only the reliable transport is implemented");
		return multitransport_decline(multi, reqId);
	}

	if (!rdpemt_is_supported())
	{
		WLog_DBG(TAG, "declining UDP, built without -DWITH_RDPEUDP=ON");
		return multitransport_decline(multi, reqId);
	}

	if (!freerdp_settings_get_bool(settings, FreeRDP_SupportMultitransport) ||
	    !(freerdp_settings_get_uint32(settings, FreeRDP_MultitransportFlags) &
	      TRANSPORT_TYPE_UDP_FECR))
		return multitransport_decline(multi, reqId);

	EnterCriticalSection(&multi->lock);
	const BOOL busy = (multi->reliable != nullptr);
	LeaveCriticalSection(&multi->lock);
	if (busy)
	{
		WLog_WARN(TAG, "a reliable UDP tunnel already exists, declining request %" PRIu32, reqId);
		return multitransport_decline(multi, reqId);
	}

	/* The tunnel's TLS session has to end at the server the main connection was verified
	 * against, which needs the main connection to be TLS directly to that server. */
	const BYTE* publicKey = nullptr;
	DWORD publicKeyLength = 0;
	if (!transport_get_public_key(rdp->transport, &publicKey, &publicKeyLength) || !publicKey ||
	    (publicKeyLength == 0))
	{
		WLog_INFO(TAG, "declining UDP, the main connection does not use TLS");
		return multitransport_decline(multi, reqId);
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_GatewayEnabled))
	{
		WLog_INFO(TAG, "declining UDP, the main connection goes through a gateway");
		return multitransport_decline(multi, reqId);
	}
	if (freerdp_settings_get_uint32(settings, FreeRDP_ProxyType) != PROXY_TYPE_NONE)
	{
		/* the TCP peer is the proxy, the server's UDP port is not reachable through it */
		WLog_INFO(TAG, "declining UDP, the main connection goes through a proxy");
		return multitransport_decline(multi, reqId);
	}

	struct sockaddr_storage address = WINPR_C_ARRAY_INIT;
	size_t addressLength = 0;
	if (!transport_get_peer_address(rdp->transport, &address, &addressLength) &&
	    !multitransport_resolve_server(settings, &address, &addressLength))
	{
		WLog_INFO(TAG, "declining UDP, the server address is unknown");
		return multitransport_decline(multi, reqId);
	}

	rdpEmt* emt = rdpemt_new(WLog_Get(TAG), reqId, cookie,
	                         freerdp_settings_get_string(settings, FreeRDP_ServerHostname),
	                         publicKey, publicKeyLength, multitransport_tunnel_event, multi);
	if (!emt)
		return multitransport_decline(multi, reqId);

	EnterCriticalSection(&multi->lock);
	multi->reliable = emt;
	multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_PENDING;
	multi->reliableResponseSent = FALSE;
	LeaveCriticalSection(&multi->lock);

	if (!rdpemt_start(emt, (const struct sockaddr*)&address, addressLength))
	{
		EnterCriticalSection(&multi->lock);
		multi->reliable = nullptr;
		multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_NONE;
		LeaveCriticalSection(&multi->lock);
		rdpemt_free(emt);
		return multitransport_decline(multi, reqId);
	}

	/* The response goes out once the tunnel is up, or has failed. Answering before the server
	 * has accepted the tunnel makes it drop the session. */
	WLog_INFO(TAG, "setting up a reliable UDP tunnel for request %" PRIu32, reqId);
	return STATE_RUN_SUCCESS;
}

static void multitransport_notify_state(rdpMultitransport* multi,
                                        FreeRDP_MultitransportTunnelState state)
{
	if (multi->haveCallbacks && multi->callbacks.TunnelStateChanged)
		multi->callbacks.TunnelStateChanged(multi->custom, TUNNELTYPE_UDPFECR, state);
}

static BOOL multitransport_handle_event(rdpMultitransport* multi, const mt_event* ev)
{
	switch (ev->type)
	{
		case MT_EVENT_READY:
		{
			EnterCriticalSection(&multi->lock);
			const UINT32 reqId = multi->reliable ? rdpemt_get_request_id(multi->reliable) : 0;
			const BOOL haveTunnel = multi->reliable != nullptr;
			LeaveCriticalSection(&multi->lock);
			if (!haveTunnel)
				return TRUE;

			/* [MS-RDPBCGR] 2.2.15.2: S_OK only goes to a server that does Soft-Sync. Without
			 * it, success is not reported at all. */
			if (multitransport_soft_sync(multi))
			{
				if (!multitransport_client_send_response(multi, reqId, S_OK))
					return FALSE;
			}

			EnterCriticalSection(&multi->lock);
			multi->reliableResponseSent = TRUE;
			multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_READY;
			LeaveCriticalSection(&multi->lock);
			multitransport_notify_state(multi, FREERDP_MULTITRANSPORT_TUNNEL_READY);
		}
		break;

		case MT_EVENT_FAILED:
		{
			EnterCriticalSection(&multi->lock);
			rdpEmt* emt = multi->reliable;
			const BOOL responseSent = multi->reliableResponseSent;
			const BOOL wasReady = multi->reliableState == FREERDP_MULTITRANSPORT_TUNNEL_READY;
			multi->reliable = nullptr;
			multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_NONE;
			LeaveCriticalSection(&multi->lock);
			if (!emt)
				return TRUE;

			const UINT32 reqId = rdpemt_get_request_id(emt);
			rdpemt_free(emt);

			/* Soft-Sync only moves channels from TCP to UDP ([MS-RDPEDYC] 3.1.5.3), there is no
			 * way back. What was in flight on the tunnel is lost in both directions, so the
			 * channels on it cannot carry on over TCP: the connection ends like a broken main
			 * transport, and the client reconnects. */
			if (wasReady)
			{
				WLog_WARN(TAG, "the UDP tunnel broke down, ending the connection");
				freerdp_set_last_error_if_not(multi->rdp->context,
				                              FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
				return FALSE;
			}
			if (!responseSent)
			{
				if (!multitransport_client_send_response(multi, reqId, E_ABORT))
					return FALSE;
			}
			multitransport_notify_state(multi, FREERDP_MULTITRANSPORT_TUNNEL_NONE);
		}
		break;

		case MT_EVENT_DATA:
		default:
			if (multi->haveCallbacks && multi->callbacks.TunnelDataReceived)
				multi->callbacks.TunnelDataReceived(multi->custom, TUNNELTYPE_UDPFECR, ev->data,
				                                    ev->length);
			else
				WLog_DBG(TAG, "dropping %" PRIuz " bytes of tunnel data, no receiver", ev->length);
			break;
	}
	return TRUE;
}

BOOL multitransport_check(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);

	(void)ResetEvent(multi->event);
	while (Queue_Count(multi->events) > 0)
	{
		mt_event* ev = Queue_Dequeue(multi->events);
		if (!ev)
			break;
		const BOOL rc = multitransport_handle_event(multi, ev);
		mt_event_free(ev);
		if (!rc)
			return FALSE;
	}
	return TRUE;
}

HANDLE multitransport_get_event_handle(rdpMultitransport* multi)
{
	WINPR_ASSERT(multi);
	return multi->event;
}

void multitransport_reset(rdpMultitransport* multi)
{
	if (!multi)
		return;

	EnterCriticalSection(&multi->lock);
	rdpEmt* emt = multi->reliable;
	multi->reliable = nullptr;
	multi->reliableState = FREERDP_MULTITRANSPORT_TUNNEL_NONE;
	multi->reliableResponseSent = FALSE;
	LeaveCriticalSection(&multi->lock);

	rdpemt_free(emt);
	Queue_Clear(multi->events);
	(void)ResetEvent(multi->event);
}

static state_run_t multitransport_server_handle_response(rdpMultitransport* multi,
                                                         WINPR_ATTR_UNUSED UINT32 reqId,
                                                         WINPR_ATTR_UNUSED UINT32 hrResponse)
{
	rdpRdp* rdp = multi->rdp;

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE))
		return STATE_RUN_FAILED;

	return STATE_RUN_CONTINUE;
}

rdpMultitransport* multitransport_new(rdpRdp* rdp, WINPR_ATTR_UNUSED UINT16 protocol)
{
	WINPR_ASSERT(rdp);

	rdpSettings* settings = rdp->settings;
	WINPR_ASSERT(settings);

	rdpMultitransport* multi = calloc(1, sizeof(rdpMultitransport));
	if (!multi)
		return nullptr;

	if (!InitializeCriticalSectionAndSpinCount(&multi->lock, 4000))
	{
		free(multi);
		return nullptr;
	}

	multi->rdp = rdp;
	multi->event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	multi->events = Queue_New(TRUE, -1, -1);
	if (!multi->event || !multi->events)
		goto fail;
	{
		wObject* obj = Queue_Object(multi->events);
		obj->fnObjectFree = mt_event_free;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_ServerMode))
	{
		multi->MtResponse = multitransport_server_handle_response;
	}
	else
	{
		multi->MtRequest = multitransport_client_request;
	}

	return multi;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	multitransport_free(multi);
	WINPR_PRAGMA_DIAG_POP
	return nullptr;
}

void multitransport_free(rdpMultitransport* multitransport)
{
	if (!multitransport)
		return;

	multitransport_reset(multitransport);
	Queue_Free(multitransport->events);
	if (multitransport->event)
		(void)CloseHandle(multitransport->event);
	DeleteCriticalSection(&multitransport->lock);
	free(multitransport);
}

static rdpMultitransport* multitransport_from_context(rdpContext* context)
{
	if (!context || !context->rdp)
		return nullptr;
	return context->rdp->multitransport;
}

BOOL freerdp_multitransport_set_dvc_callbacks(rdpContext* context,
                                              const FreeRDP_MultitransportDvcCallbacks* callbacks,
                                              void* custom)
{
	rdpMultitransport* multi = multitransport_from_context(context);
	if (!multi)
		return FALSE;

	EnterCriticalSection(&multi->lock);
	if (callbacks)
		multi->callbacks = *callbacks;
	else
		memset(&multi->callbacks, 0, sizeof(multi->callbacks));
	multi->haveCallbacks = (callbacks != nullptr);
	multi->custom = custom;
	LeaveCriticalSection(&multi->lock);
	return TRUE;
}

FreeRDP_MultitransportTunnelState freerdp_multitransport_get_tunnel_state(rdpContext* context,
                                                                          UINT32 tunnelType)
{
	rdpMultitransport* multi = multitransport_from_context(context);
	if (!multi || (tunnelType != TUNNELTYPE_UDPFECR))
		return FREERDP_MULTITRANSPORT_TUNNEL_NONE;

	EnterCriticalSection(&multi->lock);
	const FreeRDP_MultitransportTunnelState state = multi->reliableState;
	LeaveCriticalSection(&multi->lock);
	return state;
}

BOOL freerdp_multitransport_send_dvc(rdpContext* context, UINT32 tunnelType, const BYTE* data,
                                     size_t length)
{
	rdpMultitransport* multi = multitransport_from_context(context);
	if (!multi || (tunnelType != TUNNELTYPE_UDPFECR))
		return FALSE;

	BOOL rc = FALSE;
	EnterCriticalSection(&multi->lock);
	if (multi->reliable && (multi->reliableState == FREERDP_MULTITRANSPORT_TUNNEL_READY))
		rc = rdpemt_send_data(multi->reliable, data, length);
	LeaveCriticalSection(&multi->lock);
	return rc;
}
