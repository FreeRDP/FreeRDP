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

#ifndef FREERDP_CORE_NEGO_H
#define FREERDP_CORE_NEGO_H

#include "transport.h"

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/debug.h>

#include <winpr/stream.h>

/* Protocol Security Negotiation Protocols */
enum RDP_NEG_PROTOCOLS
{
	PROTOCOL_RDP = 0x00000000,
	PROTOCOL_TLS = 0x00000001,
	PROTOCOL_NLA = 0x00000002,
	PROTOCOL_EXT = 0x00000008
};

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

#define PRECONNECTION_PDU_V1_SIZE				16
#define PRECONNECTION_PDU_V2_MIN_SIZE				(PRECONNECTION_PDU_V1_SIZE + 2)

#define PRECONNECTION_PDU_V1					1
#define PRECONNECTION_PDU_V2					2

struct rdp_nego
{
	int port;
	UINT32 flags;
	char* hostname;
	char* cookie;
	BYTE* RoutingToken;
	DWORD RoutingTokenLength;
	BOOL send_preconnection_pdu;
	UINT32 preconnection_id;
	char* preconnection_blob;

	NEGO_STATE state;
	BOOL tcp_connected;
	BOOL security_connected;
	UINT32 cookie_max_length;

	BOOL sendNegoData;
	UINT32 selected_protocol;
	UINT32 requested_protocols;
	BOOL NegotiateSecurityLayer;
	BYTE enabled_protocols[16];

	rdpTransport* transport;
};
typedef struct rdp_nego rdpNego;

BOOL nego_connect(rdpNego* nego);

BOOL nego_send_preconnection_pdu(rdpNego* nego);

void nego_attempt_ext(rdpNego* nego);
void nego_attempt_nla(rdpNego* nego);
void nego_attempt_tls(rdpNego* nego);
void nego_attempt_rdp(rdpNego* nego);

void nego_send(rdpNego* nego);
int nego_recv(rdpTransport* transport, wStream* s, void* extra);
BOOL nego_recv_response(rdpNego* nego);
BOOL nego_read_request(rdpNego* nego, wStream* s);

BOOL nego_send_negotiation_request(rdpNego* nego);
void nego_process_negotiation_request(rdpNego* nego, wStream* s);
void nego_process_negotiation_response(rdpNego* nego, wStream* s);
void nego_process_negotiation_failure(rdpNego* nego, wStream* s);
BOOL nego_send_negotiation_response(rdpNego* nego);

rdpNego* nego_new(struct rdp_transport * transport);
void nego_free(rdpNego* nego);

void nego_init(rdpNego* nego);
void nego_set_target(rdpNego* nego, char* hostname, int port);
void nego_set_negotiation_enabled(rdpNego* nego, BOOL NegotiateSecurityLayer_enabled);
void nego_enable_rdp(rdpNego* nego, BOOL enable_rdp);
void nego_enable_tls(rdpNego* nego, BOOL enable_tls);
void nego_enable_nla(rdpNego* nego, BOOL enable_nla);
void nego_enable_ext(rdpNego* nego, BOOL enable_ext);
void nego_set_routing_token(rdpNego* nego, BYTE* RoutingToken, DWORD RoutingTokenLength);
void nego_set_cookie(rdpNego* nego, char* cookie);
void nego_set_cookie_max_length(rdpNego* nego, UINT32 cookie_max_length);
void nego_set_send_preconnection_pdu(rdpNego* nego, BOOL send_pcpdu);
void nego_set_preconnection_id(rdpNego* nego, UINT32 id);
void nego_set_preconnection_blob(rdpNego* nego, char* blob);

#ifdef WITH_DEBUG_NEGO
#define DEBUG_NEGO(fmt, ...) DEBUG_CLASS(NEGO, fmt, ## __VA_ARGS__)
#else
#define DEBUG_NEGO(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __NEGO_H */
