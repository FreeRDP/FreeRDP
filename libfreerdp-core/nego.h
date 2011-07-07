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
#include <freerdp/utils/debug.h>
#include <freerdp/utils/stream.h>

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

char NEGO_STATE_STRINGS[6][25] =
{
	"NEGO_STATE_INITIAL",
	"NEGO_STATE_NLA",
	"NEGO_STATE_TLS",
	"NEGO_STATE_RDP",
	"NEGO_STATE_FAIL",
	"NEGO_STATE_FINAL"
};

/* RDP Negotiation Messages */
enum RDP_NEG_MSG
{
	/* X224_TPDU_CONNECTION_REQUEST */
	TYPE_RDP_NEG_REQ = 0x1,
	/* X224_TPDU_CONNECTION_CONFIRM */
	TYPE_RDP_NEG_RSP = 0x2,
	TYPE_RDP_NEG_FAILURE = 0x3
};

char PROTOCOL_SECURITY_STRINGS[3][4] =
{
	"RDP",
	"TLS",
	"NLA"
};

struct rdp_nego
{
	int port;
	char* hostname;
	char* cookie;
	char* routing_token;
	NEGO_STATE state;
	int tcp_connected;
	uint32 selected_protocol;
	uint32 requested_protocols;
	uint8 enabled_protocols[3];
	rdpTransport* transport;
};
typedef struct rdp_nego rdpNego;

int nego_connect(rdpNego* nego);

void nego_attempt_nla(rdpNego* nego);
void nego_attempt_tls(rdpNego* nego);
void nego_attempt_rdp(rdpNego* nego);

void nego_send(rdpNego* nego);
int nego_recv(rdpTransport* transport, STREAM* s, void* extra);
void nego_recv_response(rdpNego* nego);

void nego_send_negotiation_request(rdpNego* nego);
void nego_process_negotiation_response(rdpNego* nego, STREAM* s);
void nego_process_negotiation_failure(rdpNego* nego, STREAM* s);

rdpNego* nego_new(struct rdp_transport * transport);
void nego_free(rdpNego* nego);
void nego_init(rdpNego* nego);
void nego_set_target(rdpNego* nego, char* hostname, int port);
void nego_set_protocols(rdpNego* nego, int rdp, int tls, int nla);
void nego_set_routing_token(rdpNego* nego, char* routing_token);
void nego_set_cookie(rdpNego* nego, char* cookie);

#define WITH_DEBUG_NEGO	1

#ifdef WITH_DEBUG_NEGO
#define DEBUG_NEGO(fmt, ...) DEBUG_CLASS(NEGO, fmt, ## __VA_ARGS__)
#else
#define DEBUG_NEGO(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __NEGO_H */
