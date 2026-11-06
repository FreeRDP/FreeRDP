/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multitransport PDUs
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

#ifndef FREERDP_LIB_CORE_MULTITRANSPORT_H
#define FREERDP_LIB_CORE_MULTITRANSPORT_H

typedef struct rdp_multitransport rdpMultitransport;

#include "rdp.h"
#include "state.h"

#include <freerdp/freerdp.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

typedef enum
{
	INITIATE_REQUEST_PROTOCOL_UDPFECR = 0x01,
	INITIATE_REQUEST_PROTOCOL_UDPFECL = 0x02
} MultitransportRequestProtocol;

typedef state_run_t (*MultiTransportRequestCb)(rdpMultitransport* multi, UINT32 reqId,
                                               UINT16 reqProto, const BYTE* cookie);
typedef state_run_t (*MultiTransportResponseCb)(rdpMultitransport* multi, UINT32 reqId,
                                                UINT32 hrResponse);

#define RDPUDP_COOKIE_LEN 16
#define RDPUDP_COOKIE_HASHLEN 32

WINPR_ATTR_NODISCARD
FREERDP_LOCAL state_run_t multitransport_recv_request(rdpMultitransport* multi, wStream* s);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL state_run_t multitransport_server_request(rdpMultitransport* multi, UINT16 reqProto);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL state_run_t multitransport_recv_response(rdpMultitransport* multi, wStream* s);

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_client_send_response(rdpMultitransport* multi, UINT32 reqId,
                                                       HRESULT hr);

/** Event that is set while tunnel events wait for multitransport_check(). */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL HANDLE multitransport_get_event_handle(rdpMultitransport* multi);

/** Handles tunnel events on the thread running the main loop. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_check(rdpMultitransport* multi);

/** Sends an auto-detect PDU, a full RDP_TUNNEL_SUBHEADER, on the reliable UDP tunnel. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_send_autodetect_response(rdpMultitransport* multi,
                                                           const BYTE* data, size_t length);

#if defined(WITH_RDPEUDP)
/** Sends the dynamic virtual channel PDUs that a server collected for the reliable UDP tunnel with
 * freerdp_multitransport_send_dvc(). */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_flush_dvc(rdpMultitransport* multi);

/** Sends the Bandwidth Measure Start or Stop of a server on the tunnel, if transport tells it to.
 *  They stay in order with the channel data, because the client measures the data between them.
 *  @param rc FALSE on an error
 *  @return TRUE for RDP_TRANSPORT_UDP_R */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_server_send_autodetect(rdpMultitransport* multi,
                                                         RDP_TRANSPORT_TYPE transport,
                                                         UINT16 sequenceNumber, UINT16 requestType,
                                                         BOOL* rc);

/** TRUE while more data waits for the tunnel of a server than the link accepts. */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_is_write_blocked(rdpMultitransport* multi);

/** Adds the event of a server that has a tunnel to the used handles in events.
 *  @return the handles in use now */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL DWORD multitransport_server_add_event(rdpMultitransport* multi, HANDLE* events,
                                                    DWORD used, DWORD count);

#else

WINPR_ATTR_NODISCARD
static inline BOOL multitransport_server_send_autodetect(
    WINPR_ATTR_UNUSED rdpMultitransport* multi, WINPR_ATTR_UNUSED RDP_TRANSPORT_TYPE transport,
    WINPR_ATTR_UNUSED UINT16 sequenceNumber, WINPR_ATTR_UNUSED UINT16 requestType,
    WINPR_ATTR_UNUSED BOOL* rc)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline BOOL multitransport_is_write_blocked(WINPR_ATTR_UNUSED rdpMultitransport* multi)
{
	return FALSE;
}

WINPR_ATTR_NODISCARD
static inline DWORD multitransport_server_add_event(WINPR_ATTR_UNUSED rdpMultitransport* multi,
                                                    WINPR_ATTR_UNUSED HANDLE* events, DWORD used,
                                                    WINPR_ATTR_UNUSED DWORD count)
{
	return used;
}

#endif
/** TRUE when the connection was ended because its UDP tunnel broke down. Cleared by
 * multitransport_reset(). */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL multitransport_ended_connection(const rdpMultitransport* multi);

/** Tears down any tunnel, e.g. when the main connection goes away. */
FREERDP_LOCAL void multitransport_reset(rdpMultitransport* multi);

FREERDP_LOCAL void multitransport_free(rdpMultitransport* multi);

WINPR_ATTR_MALLOC(multitransport_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpMultitransport* multitransport_new(rdpRdp* rdp, UINT16 protocol);

#endif /* FREERDP_LIB_CORE_MULTITRANSPORT_H */
