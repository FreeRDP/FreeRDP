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

#include "stream.h"
#include "network.h"

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

struct _NEGO
{
	int port;
	char *hostname;
	NEGO_STATE state;
	int tcp_connected;
	struct rdp_network * net;
	uint32 selected_protocol;
	uint32 requested_protocols;
	uint8 enabled_protocols[3];
};
typedef struct _NEGO NEGO;

int nego_connect(NEGO *nego);

void nego_attempt_nla(NEGO *nego);
void nego_attempt_tls(NEGO *nego);
void nego_attempt_rdp(NEGO *nego);

void nego_send(NEGO *nego);
void nego_recv(NEGO *nego, STREAM s);

void nego_process_negotiation_response(NEGO *nego, STREAM s);
void nego_process_negotiation_failure(NEGO *nego, STREAM s);

NEGO* nego_new(struct rdp_network * net);
void nego_init(NEGO *nego);
void nego_free(NEGO *nego);

#endif /* __NEGO_H */
