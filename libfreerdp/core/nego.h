/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Protocol Security Negotiation
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_NEGO_H
#define FREERDP_LIB_CORE_NEGO_H

#include "transport.h"

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/log.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

/* Protocol Security Negotiation Protocols */
#define PROTOCOL_RDP	0x00000000
#define PROTOCOL_TLS	0x00000001
#define PROTOCOL_NLA	0x00000002
#define PROTOCOL_EXT	0x00000008

#define PROTOCOL_FAILED_NEGO	0x80000000 /* only used internally, not on the wire */

/* Protocol Security Negotiation Failure Codes */
enum RDP_NEG_FAILURE_FAILURECODES
{
	SSL_REQUIRED_BY_SERVER = 0x00000001,
	SSL_NOT_ALLOWED_BY_SERVER = 0x00000002,
	SSL_CERT_NOT_ON_SERVER = 0x00000003,
	INCONSISTENT_FLAGS = 0x00000004,
	HYBRID_REQUIRED_BY_SERVER = 0x00000005
};

/* Authorization Result */
#define AUTHZ_SUCCESS		0x00000000
#define AUTHZ_ACCESS_DENIED	0x0000052E

enum _NEGO_STATE
{
	NEGO_STATE_INITIAL,
	NEGO_STATE_EXT, /* Extended NLA (NLA + TLS implicit) */
	NEGO_STATE_NLA, /* Network Level Authentication (TLS implicit) */
	NEGO_STATE_TLS, /* TLS Encryption without NLA */
	NEGO_STATE_RDP, /* Standard Legacy RDP Encryption */
	NEGO_STATE_FAIL, /* Negotiation failure */
	NEGO_STATE_FINAL
};
typedef enum _NEGO_STATE NEGO_STATE;

/* RDP Negotiation Messages */
enum RDP_NEG_MSG
{
	/* X224_TPDU_CONNECTION_REQUEST */
	TYPE_RDP_NEG_REQ = 0x1,
	/* X224_TPDU_CONNECTION_CONFIRM */
	TYPE_RDP_NEG_RSP = 0x2,
	TYPE_RDP_NEG_FAILURE = 0x3
};

#define EXTENDED_CLIENT_DATA_SUPPORTED				0x01
#define DYNVC_GFX_PROTOCOL_SUPPORTED				0x02
#define RDP_NEGRSP_RESERVED					0x04
#define RESTRICTED_ADMIN_MODE_SUPPORTED				0x08

#define PRECONNECTION_PDU_V1_SIZE				16
#define PRECONNECTION_PDU_V2_MIN_SIZE				(PRECONNECTION_PDU_V1_SIZE + 2)

#define PRECONNECTION_PDU_V1					1
#define PRECONNECTION_PDU_V2					2

#define RESTRICTED_ADMIN_MODE_REQUIRED				0x01
#define CORRELATION_INFO_PRESENT				0x08

struct rdp_nego
{
	int port;
	UINT32 flags;
	char* hostname;
	char* cookie;
	BYTE* RoutingToken;
	DWORD RoutingTokenLength;
	BOOL SendPreconnectionPdu;
	UINT32 PreconnectionId;
	char* PreconnectionBlob;

	NEGO_STATE state;
	BOOL TcpConnected;
	BOOL SecurityConnected;
	UINT32 CookieMaxLength;

	BOOL sendNegoData;
	UINT32 SelectedProtocol;
	UINT32 RequestedProtocols;
	BOOL NegotiateSecurityLayer;
	BYTE EnabledProtocols[16];
	BOOL RestrictedAdminModeRequired;
	BOOL GatewayEnabled;
	BOOL GatewayBypassLocal;

	rdpTransport* transport;
};
typedef struct rdp_nego rdpNego;

FREERDP_LOCAL BOOL nego_connect(rdpNego* nego);
FREERDP_LOCAL BOOL nego_disconnect(rdpNego* nego);

FREERDP_LOCAL BOOL nego_send_preconnection_pdu(rdpNego* nego);

FREERDP_LOCAL void nego_attempt_ext(rdpNego* nego);
FREERDP_LOCAL void nego_attempt_nla(rdpNego* nego);
FREERDP_LOCAL void nego_attempt_tls(rdpNego* nego);
FREERDP_LOCAL void nego_attempt_rdp(rdpNego* nego);

FREERDP_LOCAL void nego_send(rdpNego* nego);
FREERDP_LOCAL int nego_recv(rdpTransport* transport, wStream* s, void* extra);
FREERDP_LOCAL BOOL nego_recv_response(rdpNego* nego);
FREERDP_LOCAL BOOL nego_read_request(rdpNego* nego, wStream* s);

FREERDP_LOCAL BOOL nego_send_negotiation_request(rdpNego* nego);
FREERDP_LOCAL void nego_process_negotiation_request(rdpNego* nego, wStream* s);
FREERDP_LOCAL void nego_process_negotiation_response(rdpNego* nego, wStream* s);
FREERDP_LOCAL void nego_process_negotiation_failure(rdpNego* nego, wStream* s);
FREERDP_LOCAL BOOL nego_send_negotiation_response(rdpNego* nego);

FREERDP_LOCAL rdpNego* nego_new(rdpTransport* transport);
FREERDP_LOCAL void nego_free(rdpNego* nego);

FREERDP_LOCAL void nego_init(rdpNego* nego);
FREERDP_LOCAL void nego_set_target(rdpNego* nego, char* hostname, int port);
FREERDP_LOCAL void nego_set_negotiation_enabled(rdpNego* nego,
        BOOL NegotiateSecurityLayer);
FREERDP_LOCAL void nego_set_restricted_admin_mode_required(rdpNego* nego,
        BOOL RestrictedAdminModeRequired);
FREERDP_LOCAL void nego_set_gateway_enabled(rdpNego* nego, BOOL GatewayEnabled);
FREERDP_LOCAL void nego_set_gateway_bypass_local(rdpNego* nego,
        BOOL GatewayBypassLocal);
FREERDP_LOCAL void nego_enable_rdp(rdpNego* nego, BOOL enable_rdp);
FREERDP_LOCAL void nego_enable_tls(rdpNego* nego, BOOL enable_tls);
FREERDP_LOCAL void nego_enable_nla(rdpNego* nego, BOOL enable_nla);
FREERDP_LOCAL void nego_enable_ext(rdpNego* nego, BOOL enable_ext);
FREERDP_LOCAL BOOL nego_set_routing_token(rdpNego* nego, BYTE* RoutingToken,
        DWORD RoutingTokenLength);
FREERDP_LOCAL BOOL nego_set_cookie(rdpNego* nego, char* cookie);
FREERDP_LOCAL void nego_set_cookie_max_length(rdpNego* nego,
        UINT32 CookieMaxLength);
FREERDP_LOCAL void nego_set_send_preconnection_pdu(rdpNego* nego,
        BOOL SendPreconnectionPdu);
FREERDP_LOCAL void nego_set_preconnection_id(rdpNego* nego,
        UINT32 PreconnectionId);
FREERDP_LOCAL void nego_set_preconnection_blob(rdpNego* nego,
        char* PreconnectionBlob);

#endif /* FREERDP_LIB_CORE_NEGO_H */
