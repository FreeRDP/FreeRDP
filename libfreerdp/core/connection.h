/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_LIB_CORE_CONNECTION_H
#define FREERDP_LIB_CORE_CONNECTION_H

#include "rdp.h"
#include "tpkt.h"
#include "tpdu.h"
#include "nego.h"
#include "mcs.h"
#include "activation.h"

#include <freerdp/settings.h>
#include <freerdp/api.h>

enum CONNECTION_STATE
{
	CONNECTION_STATE_INITIAL,
	CONNECTION_STATE_NEGO,
	CONNECTION_STATE_NLA,
	CONNECTION_STATE_MCS_CONNECT,
	CONNECTION_STATE_MCS_ERECT_DOMAIN,
	CONNECTION_STATE_MCS_ATTACH_USER,
	CONNECTION_STATE_MCS_CHANNEL_JOIN,
	CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT,
	CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE,
	CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT,
	CONNECTION_STATE_LICENSING,
	CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING,
	CONNECTION_STATE_CAPABILITIES_EXCHANGE,
	CONNECTION_STATE_FINALIZATION,
	CONNECTION_STATE_ACTIVE
};

enum CLIENT_CONNECTION_STATE
{
	CLIENT_STATE_INITIAL,
	CLIENT_STATE_PRECONNECT_PASSED,
	CLIENT_STATE_POSTCONNECT_PASSED
};

FREERDP_LOCAL BOOL rdp_client_connect(rdpRdp* rdp);
FREERDP_LOCAL BOOL rdp_client_disconnect(rdpRdp* rdp);
FREERDP_LOCAL BOOL rdp_client_disconnect_and_clear(rdpRdp* rdp);
FREERDP_LOCAL BOOL rdp_client_reconnect(rdpRdp* rdp);
FREERDP_LOCAL BOOL rdp_client_redirect(rdpRdp* rdp);
FREERDP_LOCAL BOOL rdp_client_connect_mcs_channel_join_confirm(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_client_connect_auto_detect(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL int rdp_client_connect_license(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL int rdp_client_connect_demand_active(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL int rdp_client_transition_to_state(rdpRdp* rdp, int state);
FREERDP_LOCAL const char* rdp_client_connection_state_string(int state);

FREERDP_LOCAL BOOL rdp_server_accept_nego(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_server_accept_mcs_connect_initial(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_server_accept_mcs_erect_domain_request(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_server_accept_mcs_attach_user_request(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_server_accept_mcs_channel_join_request(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_server_accept_confirm_active(rdpRdp* rdp, wStream* s, UINT16 pduLength);
FREERDP_LOCAL BOOL rdp_server_establish_keys(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_server_reactivate(rdpRdp* rdp);
FREERDP_LOCAL int rdp_server_transition_to_state(rdpRdp* rdp, int state);
FREERDP_LOCAL const char* rdp_server_connection_state_string(int state);

#endif /* FREERDP_LIB_CORE_CONNECTION_H */
