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
	CONNECTION_STATE_MCS_CHANNEL_JOIN,
	CONNECTION_STATE_ESTABLISH_KEYS,
	CONNECTION_STATE_LICENSE,
	CONNECTION_STATE_CAPABILITY,
	CONNECTION_STATE_FINALIZATION,
	CONNECTION_STATE_ACTIVE
};

boolean rdp_client_connect(rdpRdp* rdp);
boolean rdp_client_redirect(rdpRdp* rdp);
boolean rdp_client_connect_mcs_connect_response(rdpRdp* rdp, STREAM* s);
boolean rdp_client_connect_mcs_attach_user_confirm(rdpRdp* rdp, STREAM* s);
boolean rdp_client_connect_mcs_channel_join_confirm(rdpRdp* rdp, STREAM* s);
boolean rdp_client_connect_license(rdpRdp* rdp, STREAM* s);
boolean rdp_client_connect_demand_active(rdpRdp* rdp, STREAM* s);
boolean rdp_client_connect_finalize(rdpRdp* rdp);

boolean rdp_server_accept_nego(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_mcs_connect_initial(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_mcs_erect_domain_request(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_mcs_attach_user_request(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_mcs_channel_join_request(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_client_keys(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_client_info(rdpRdp* rdp, STREAM* s);
boolean rdp_server_accept_confirm_active(rdpRdp* rdp, STREAM* s);
boolean rdp_server_reactivate(rdpRdp* rdp);

#endif /* __CONNECTION_H */
