/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Connection Sequence
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "rdp.h"
#include "tpkt.h"
#include "tpdu.h"
#include "nego.h"
#include "mcs.h"
#include "transport.h"
#include "activation.h"

#include <freerdp/settings.h>
#include <freerdp/utils/memory.h>

enum CONNECTION_STATE
{
	CONNECTION_STATE_INITIAL = 0,
	CONNECTION_STATE_NEGO,
	CONNECTION_STATE_MCS_CONNECT,
	CONNECTION_STATE_MCS_ERECT_DOMAIN,
	CONNECTION_STATE_MCS_ATTACH_USER,
	CONNECTION_STATE_CHANNEL_JOIN
};

boolean rdp_client_connect(rdpRdp* rdp);

boolean rdp_server_accept_nego(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_mcs_connect_initial(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_mcs_erect_domain_request(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_mcs_attach_user_request(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_mcs_channel_join_request(rdpRdp* rdp, STREAM* s);

#endif /* __CONNECTION_H */
