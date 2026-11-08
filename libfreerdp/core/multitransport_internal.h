/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multitransport internals shared by the client and server code
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

#ifndef FREERDP_LIB_CORE_MULTITRANSPORT_INTERNAL_H
#define FREERDP_LIB_CORE_MULTITRANSPORT_INTERNAL_H

#include <winpr/collections.h>
#include <winpr/synch.h>

#include <freerdp/config.h>
#include <freerdp/multitransport.h>
#if defined(WITH_RDPEUDP)
#include <freerdp/server/udp.h>
#endif

#include "rdpemt.h"
#include "multitransport.h"

typedef enum
{
	MT_EVENT_READY,
	MT_EVENT_FAILED,
	MT_EVENT_DATA,
	MT_EVENT_AUTODETECT
} MT_EVENT_TYPE;

/* Tunnel events are queued from the tunnel thread to the main loop. The main loop sends the
 * Initiate Multitransport Response (client), verifies the client (server), delivers tunnel
 * data and auto-detect PDUs, and notifies the channel code of state changes. */
typedef struct
{
	MT_EVENT_TYPE type;
	wStream* s; /* Sealed stream, starting at position 0 */
	size_t length;
} mt_event;

struct rdp_multitransport
{
	rdpRdp* rdp;

	MultiTransportRequestCb MtRequest;
	MultiTransportResponseCb MtResponse;

	/* Server-side data */
	UINT32 reliableReqId;

	BYTE reliableCookie[RDPUDP_COOKIE_LEN];
	BYTE reliableCookieHash[RDPUDP_COOKIE_HASHLEN];
#if defined(WITH_RDPEUDP)

	/* [MS-RDPEDYC] 3.1.5.3: tunnel carries channel data only after the client accepts it
	 * over both UDP (Tunnel Create Request) and TCP (Initiate Multitransport Response), in any
	 * order. Only serverTunnelUp and serverResponseOk are protected by the lock. */
	BOOL serverTunnelUp;
	BOOL serverResponseOk;
	BOOL serverTunnelOffered;
	BOOL serverResponsePending;
	BOOL serverResponseFailed; /* Connection terminated by the Initiate Multitransport Response */
	UINT64 serverOfferTime;    /* GetTickCount64() value at the time of the offer, used for the
	                              response timeout. */

	/* UDP callbacks; see include/freerdp/server/udp.h */
	FreeRDP_UdpPeerCallbacks udpCallbacks;
	void* udpCustom;
	size_t udpSendLimit;

	/* Set by the tunnel thread and read by the main loop. Accessed via Interlocked functions. */
	LONG serverSynPending;
	LONG serverWritablePending;

	/* Tunnel data bytes queued since the main loop last drained events. Accessed via
	 * Interlocked functions. */
	LONG serverQueuedBytes;
#endif

	/* Tunnel state and synchronization; reliableResponseSent is client-only. */
	CRITICAL_SECTION lock;
	HANDLE event;
	wQueue* events;
	rdpEmt* reliable;
	FreeRDP_MultitransportTunnelState reliableState;
	BOOL reliableResponseSent;

	/* Accessed on the main loop thread without locking to avoid blocking tunnel data
	 * reception. The tunnel thread only reads these during direct delivery; see
	 * multitransport_direct. */
	FreeRDP_MultitransportDvcCallbacks callbacks;
	void* custom;

	/* Main loop thread only: connection ended due to tunnel failure. */
	BOOL endedConnection;

	/* Set by the tunnel thread if an event cannot be queued (e.g. due to queue limits or
	 * memory failure), potentially including data already acked to the client.
	 * Accessed via Interlocked functions. */
	LONG tunnelEventLost;
};

/** Server state for the Initiate Multitransport Request */
typedef enum
{
	MT_REQUEST_WAIT,  /**< No tunnel; the connection awaits the client's response. */
	MT_REQUEST_NONE,  /**< No request is sent; the connection proceeds via TCP. */
	MT_REQUEST_TUNNEL /**< A tunnel is listening; the connection continues immediately. */
} MT_REQUEST_MODE;

#if defined(WITH_RDPEUDP)

/** Shared with multitransport_server.c; defined as static when UDP is not enabled. */
#define MT_SHARED

/** This function is executed on the tunnel thread. */
FREERDP_LOCAL void multitransport_tunnel_event(void* custom, rdpEmt* emt, RDPEMT_EVENT event,
                                               const BYTE* data, size_t length);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_notify_state(rdpMultitransport* multi,
                                               FreeRDP_MultitransportTunnelState state);

/** Prepares the offer to be sent in the server's next request. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL MT_REQUEST_MODE multitransport_server_request_mode(rdpMultitransport* multi);

/** Returns TRUE on the server side. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_is_server(const rdpMultitransport* multi);

/** Returns TRUE if the server offered a tunnel; the response is handled here. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_server_tunnel_offered(const rdpMultitransport* multi);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL state_run_t multitransport_server_tunnel_response(rdpMultitransport* multi,
                                                                UINT32 reqId, UINT32 hrResponse);

/** Returns TRUE if multitransport_server_tunnel_response() failed. The peer tracks this
 * failure during finalization, where other failures are ignored. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_server_response_failed(const rdpMultitransport* multi);

/** Server-side handling of MT_EVENT_READY: the client has proven its identity.
 * @param rc Set to FALSE on error.
 * @return TRUE on the server. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_server_tunnel_ready(rdpMultitransport* multi, BOOL* rc);

/** Invoked when the tunnel has stopped. */
FREERDP_LOCAL void multitransport_server_tunnel_closed(rdpMultitransport* multi);

/** Main loop thread: processes flags set by the tunnel thread before handling queued events. */
FREERDP_LOCAL void multitransport_tunnel_check(rdpMultitransport* multi);

#else

#define MT_SHARED static

WINPR_ATTR_NODISCARD
static inline MT_REQUEST_MODE
multitransport_server_request_mode(WINPR_ATTR_UNUSED rdpMultitransport* multi)
{
	return MT_REQUEST_WAIT;
}

WINPR_ATTR_NODISCARD
static inline BOOL multitransport_is_server(WINPR_ATTR_UNUSED const rdpMultitransport* multi)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL
multitransport_server_tunnel_offered(WINPR_ATTR_UNUSED const rdpMultitransport* multi)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline state_run_t
multitransport_server_tunnel_response(WINPR_ATTR_UNUSED rdpMultitransport* multi,
                                      WINPR_ATTR_UNUSED UINT32 reqId,
                                      WINPR_ATTR_UNUSED UINT32 hrResponse)
{
	return STATE_RUN_FAILED;
}

WINPR_ATTR_NODISCARD
static inline BOOL
multitransport_server_response_failed(WINPR_ATTR_UNUSED const rdpMultitransport* multi)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL multitransport_server_tunnel_ready(WINPR_ATTR_UNUSED rdpMultitransport* multi,
                                                      WINPR_ATTR_UNUSED BOOL* rc)
{
	return FALSE;
}

static inline void multitransport_server_tunnel_closed(WINPR_ATTR_UNUSED rdpMultitransport* multi)
{
}

static inline void multitransport_tunnel_check(WINPR_ATTR_UNUSED rdpMultitransport* multi)
{
}

#endif

#endif /* End of FREERDP_LIB_CORE_MULTITRANSPORT_INTERNAL_H */
