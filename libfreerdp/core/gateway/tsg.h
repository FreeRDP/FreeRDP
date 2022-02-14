/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Terminal Server Gateway (TSG)
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_GATEWAY_TSG_H
#define FREERDP_LIB_CORE_GATEWAY_TSG_H

typedef struct rdp_tsg rdpTsg;

#include "rpc.h"

#include "../transport.h"

#include <winpr/rpc.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <freerdp/types.h>
#include <freerdp/api.h>

typedef enum
{
	TSG_STATE_INITIAL,
	TSG_STATE_CONNECTED,
	TSG_STATE_AUTHORIZED,
	TSG_STATE_CHANNEL_CREATED,
	TSG_STATE_PIPE_CREATED,
	TSG_STATE_TUNNEL_CLOSE_PENDING,
	TSG_STATE_CHANNEL_CLOSE_PENDING,
	TSG_STATE_FINAL
} TSG_STATE;

#define TsProxyCreateTunnelOpnum 1
#define TsProxyAuthorizeTunnelOpnum 2
#define TsProxyMakeTunnelCallOpnum 3
#define TsProxyCreateChannelOpnum 4
#define TsProxyUnused5Opnum 5
#define TsProxyCloseChannelOpnum 6
#define TsProxyCloseTunnelOpnum 7
#define TsProxySetupReceivePipeOpnum 8
#define TsProxySendToServerOpnum 9

#define MAX_RESOURCE_NAMES 50

#define TS_GATEWAY_TRANSPORT 0x5452

#define TSG_ASYNC_MESSAGE_CONSENT_MESSAGE 0x00000001
#define TSG_ASYNC_MESSAGE_SERVICE_MESSAGE 0x00000002
#define TSG_ASYNC_MESSAGE_REAUTH 0x00000003

#define TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST 0x00000001
#define TSG_TUNNEL_CANCEL_ASYNC_MSG_REQUEST 0x00000002

#define TSG_NAP_CAPABILITY_QUAR_SOH 0x00000001
#define TSG_NAP_CAPABILITY_IDLE_TIMEOUT 0x00000002
#define TSG_MESSAGING_CAP_CONSENT_SIGN 0x00000004
#define TSG_MESSAGING_CAP_SERVICE_MSG 0x00000008
#define TSG_MESSAGING_CAP_REAUTH 0x00000010
#define TSG_MESSAGING_MAX_MESSAGE_LENGTH 65536

/* Error Codes */

#define E_PROXY_INTERNALERROR 0x800759D8
#define E_PROXY_RAP_ACCESSDENIED 0x800759DA
#define E_PROXY_NAP_ACCESSDENIED 0x800759DB
#define E_PROXY_TS_CONNECTFAILED 0x800759DD
#define E_PROXY_ALREADYDISCONNECTED 0x800759DF
#define E_PROXY_QUARANTINE_ACCESSDENIED 0x800759ED
#define E_PROXY_NOCERTAVAILABLE 0x800759EE
#define E_PROXY_COOKIE_BADPACKET 0x800759F7
#define E_PROXY_COOKIE_AUTHENTICATION_ACCESS_DENIED 0x800759F8
#define E_PROXY_UNSUPPORTED_AUTHENTICATION_METHOD 0x800759F9
#define E_PROXY_CAPABILITYMISMATCH 0x800759E9

#define E_PROXY_NOTSUPPORTED 0x000059E8
#define E_PROXY_MAXCONNECTIONSREACHED 0x000059E6
#define E_PROXY_SESSIONTIMEOUT 0x000059F6
#define E_PROXY_REAUTH_AUTHN_FAILED 0x000059FA
#define E_PROXY_REAUTH_CAP_FAILED 0x000059FB
#define E_PROXY_REAUTH_RAP_FAILED 0x000059FC
#define E_PROXY_SDR_NOT_SUPPORTED_BY_TS 0x000059FD
#define E_PROXY_REAUTH_NAP_FAILED 0x00005A00
#define E_PROXY_CONNECTIONABORTED 0x000004D4

FREERDP_LOCAL rdpTsg* tsg_new(rdpTransport* transport);
FREERDP_LOCAL void tsg_free(rdpTsg* tsg);

FREERDP_LOCAL BOOL tsg_proxy_begin(rdpTsg* tsg);

FREERDP_LOCAL BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port, DWORD timeout);
FREERDP_LOCAL BOOL tsg_disconnect(rdpTsg* tsg);

FREERDP_LOCAL BOOL tsg_recv_pdu(rdpTsg* tsg, RPC_PDU* pdu);

FREERDP_LOCAL BOOL tsg_check_event_handles(rdpTsg* tsg);
FREERDP_LOCAL DWORD tsg_get_event_handles(rdpTsg* tsg, HANDLE* events, DWORD count);

FREERDP_LOCAL TSG_STATE tsg_get_state(rdpTsg* tsg);
FREERDP_LOCAL BOOL tsg_set_state(rdpTsg* tsg, TSG_STATE state);

FREERDP_LOCAL BIO* tsg_get_bio(rdpTsg* tsg);

#endif /* FREERDP_LIB_CORE_GATEWAY_TSG_H */
