/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multitransport: what multitransport.c shares with the server side
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

/* Tunnel events travel from the tunnel thread to the thread that runs the main loop, which is
 * where the Initiate Multitransport Response goes out and where the dynamic channel code is
 * called. */
typedef struct
{
	MT_EVENT_TYPE type;
	wStream* s; /* sealed, at position 0 */
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
#if defined(WITH_RDPEUDP)

	/* [MS-RDPEDYC] 3.1.5.3: the tunnel carries channel data after the client accepted it over UDP
	 * (Tunnel Create Request) and over TCP (Initiate Multitransport Response). The client can do
	 * these in either order. The lock protects these fields. */
	BOOL serverTunnelUp;
	BOOL serverResponseOk;
	BOOL serverTunnelOffered;
	BOOL serverResponsePending;

	/* the application's control of the UDP side, see include/freerdp/server/udp.h */
	FreeRDP_UdpPeerCallbacks udpCallbacks;
	void* udpCustom;
	size_t udpSendLimit;

	/* The tunnel thread sets these for the main loop. The application's hooks run on the main loop.
	 * Use Interlocked functions to access these fields. */
	LONG serverClaimed;
	LONG serverWritable;

	/* The bytes of tunnel data that the tunnel thread queued after the main loop last started to
	 * read events. Use Interlocked functions to access this field. */
	LONG serverQueuedBytes;
#endif

	/* client-side data */
	CRITICAL_SECTION lock;
	HANDLE event;
	wQueue* events;
	rdpEmt* reliable;
	FreeRDP_MultitransportTunnelState reliableState;
	BOOL reliableResponseSent;

	/* Written on the thread that runs the main loop, which reads them there without locking:
	 * receiving tunnel data must not wait for a sender, which holds lock while it writes to the
	 * tunnel. The tunnel thread reads them only while it hands data on directly, see
	 * multitransport_direct. */
	FreeRDP_MultitransportDvcCallbacks callbacks;
	void* custom;

	/* Main loop thread only: the connection was ended because its tunnel broke down. */
	BOOL endedConnection;

	/* Set by the tunnel thread when it could not queue an event, received data that was
	 * acknowledged to the server already among them. Accessed with Interlocked functions. */
	LONG tunnelEventLost;
};

/** What a server has ready for its Initiate Multitransport Request */
typedef enum
{
	MT_OFFER_PLAIN, /**< no tunnel, the connection waits for the client's answer */
	MT_OFFER_NONE,  /**< sends no request, the connection continues over TCP */
	MT_OFFER_TUNNEL /**< a tunnel waits, the connection continues immediately */
} MT_OFFER;

#if defined(WITH_RDPEUDP)

/** multitransport_server.c uses these functions too. Without UDP they stay static. */
#define MT_SHARED

/** Runs on the tunnel thread. */
FREERDP_LOCAL void multitransport_tunnel_event(void* custom, rdpEmt* emt, RDPEMT_EVENT event,
                                               const BYTE* data, size_t length);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_notify_state(rdpMultitransport* multi,
                                               FreeRDP_MultitransportTunnelState state);

/** Prepares the offer for the request that the server sends next. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL MT_OFFER multitransport_server_offer(rdpMultitransport* multi);

/** TRUE when this is a server's multitransport. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_is_server(const rdpMultitransport* multi);

/** TRUE when the server has a tunnel ready for its request. Then this code handles the response. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_server_tunnel_offered(const rdpMultitransport* multi);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL state_run_t multitransport_server_tunnel_response(rdpMultitransport* multi,
                                                                UINT32 reqId, UINT32 hrResponse);

/** MT_EVENT_READY of a server: the client proved itself.
 *  @param rc FALSE on an error
 *  @return TRUE on a server */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_server_tunnel_ready(rdpMultitransport* multi, BOOL* rc);

/** The tunnel stopped. */
FREERDP_LOCAL void multitransport_server_tunnel_gone(rdpMultitransport* multi);

/** Main loop thread: handles what the tunnel thread set, before the queued events. */
FREERDP_LOCAL void multitransport_tunnel_check(rdpMultitransport* multi);

#else

#define MT_SHARED static

WINPR_ATTR_NODISCARD
static inline MT_OFFER multitransport_server_offer(WINPR_ATTR_UNUSED rdpMultitransport* multi)
{
	return MT_OFFER_PLAIN;
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
static inline BOOL multitransport_server_tunnel_ready(WINPR_ATTR_UNUSED rdpMultitransport* multi,
                                                      WINPR_ATTR_UNUSED BOOL* rc)
{
	return FALSE;
}

static inline void multitransport_server_tunnel_gone(WINPR_ATTR_UNUSED rdpMultitransport* multi)
{
}

static inline void multitransport_tunnel_check(WINPR_ATTR_UNUSED rdpMultitransport* multi)
{
}

#endif

#endif /* FREERDP_LIB_CORE_MULTITRANSPORT_INTERNAL_H */
