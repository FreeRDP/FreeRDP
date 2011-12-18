/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Protocol Security Negotiation
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

#ifndef __NEGO_H
#define __NEGO_H

#include "transport.h"
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/blob.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>

/* Protocol Security Negotiation Protocols */
enum RDP_NEG_PROTOCOLS
{
	PROTOCOL_RDP = 0x00000000,
	PROTOCOL_TLS = 0x00000001,
	PROTOCOL_NLA = 0x00000002
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

enum _NEGO_STATE
{
	NEGO_STATE_INITIAL,
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

#define EXTENDED_CLIENT_DATA_SUPPORTED 0x01

struct rdp_nego
{
	int port;
	uint32 flags;
	char* hostname;
	char* cookie;
	NEGO_STATE state;
	int tcp_connected;
	rdpBlob* routing_token;
	uint32 selected_protocol;
	uint32 requested_protocols;
	uint8 enabled_protocols[3];
	rdpTransport* transport;
};
typedef struct rdp_nego rdpNego;

boolean nego_connect(rdpNego* nego);

void nego_attempt_nla(rdpNego* nego);
void nego_attempt_tls(rdpNego* nego);
void nego_attempt_rdp(rdpNego* nego);

void nego_send(rdpNego* nego);
boolean nego_recv(rdpTransport* transport, STREAM* s, void* extra);
boolean nego_recv_response(rdpNego* nego);
boolean nego_read_request(rdpNego* nego, STREAM* s);

boolean nego_send_negotiation_request(rdpNego* nego);
void nego_process_negotiation_request(rdpNego* nego, STREAM* s);
void nego_process_negotiation_response(rdpNego* nego, STREAM* s);
void nego_process_negotiation_failure(rdpNego* nego, STREAM* s);
boolean nego_send_negotiation_response(rdpNego* nego);

rdpNego* nego_new(struct rdp_transport * transport);
void nego_free(rdpNego* nego);
void nego_init(rdpNego* nego);
void nego_set_target(rdpNego* nego, char* hostname, int port);
void nego_enable_rdp(rdpNego* nego, boolean enable_rdp);
void nego_enable_nla(rdpNego* nego, boolean enable_nla);
void nego_enable_tls(rdpNego* nego, boolean enable_tls);
void nego_set_routing_token(rdpNego* nego, rdpBlob* routing_token);
void nego_set_cookie(rdpNego* nego, char* cookie);

#ifdef WITH_DEBUG_NEGO
#define DEBUG_NEGO(fmt, ...) DEBUG_CLASS(NEGO, fmt, ## __VA_ARGS__)
#else
#define DEBUG_NEGO(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __NEGO_H */
