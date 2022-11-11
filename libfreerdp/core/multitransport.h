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

FREERDP_LOCAL state_run_t multitransport_client_recv_request(rdpMultitransport* multitransport,
                                                             wStream* s);
FREERDP_LOCAL BOOL multitransport_server_send_request(rdpMultitransport* multitransport);

FREERDP_LOCAL BOOL multitransport_server_recv_response(rdpMultitransport* multitransport,
                                                       wStream* s, HRESULT* hr);
FREERDP_LOCAL BOOL multitransport_client_send_response(rdpMultitransport* multitransport,
                                                       HRESULT hr);

FREERDP_LOCAL rdpMultitransport* multitransport_new(rdpRdp* rdp, UINT16 protocol);
FREERDP_LOCAL void multitransport_free(rdpMultitransport* multitransport);

#endif /* FREERDP_LIB_CORE_MULTITRANSPORT_H */
