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

#include "stream.h"
#include <freerdp/utils/memory.h>

#include "nego.h"

/**
 * Negotiate protocol security and connect.
 * @param nego
 * @return
 */

int nego_connect(NEGO *nego)
{
	if (nego->state == NEGO_STATE_INITIAL)
	{
		if (nego->enabled_protocols[PROTOCOL_NLA] > 0)
			nego->state = NEGO_STATE_NLA;
		else if (nego->enabled_protocols[PROTOCOL_TLS] > 0)
			nego->state = NEGO_STATE_TLS;
		else if (nego->enabled_protocols[PROTOCOL_RDP] > 0)
			nego->state = NEGO_STATE_RDP;
		else
			nego->state = NEGO_STATE_FAIL;
	}

	while (nego->state != NEGO_STATE_FINAL)
	{
		nego_send(nego);

		if (nego->state == NEGO_STATE_FAIL)
		{
			nego->state = NEGO_STATE_FINAL;
			return 0;
		}
	}

	return 1;
}

/**
 * Connect TCP layer.
 * @param nego
 * @return
 */

int nego_tcp_connect(NEGO *nego)
{
	if (nego->tcp_connected == 0)
	{
		if (tcp_connect(nego->net->tcp, nego->hostname, nego->port) == False)
		{
			nego->tcp_connected = 0;
			return 0;
		}
		else
		{
			nego->tcp_connected = 1;
			return 1;
		}
	}

	return 1;
}

/**
 * Disconnect TCP layer.
 * @param nego
 * @return
 */

int nego_tcp_disconnect(NEGO *nego)
{
	if (nego->tcp_connected)
		tcp_disconnect(nego->net->tcp);

	nego->tcp_connected = 0;
	return 1;
}

/**
 * Attempt negotiating NLA + TLS security.
 * @param nego
 */

void nego_attempt_nla(NEGO *nego)
{
	uint8 code;
	nego->requested_protocols = PROTOCOL_NLA | PROTOCOL_TLS;

	nego_tcp_connect(nego);
	x224_send_connection_request(nego->net->iso);
	tpkt_recv(nego->net->iso, &code, NULL);

	if (nego->state != NEGO_STATE_FINAL)
	{
		nego_tcp_disconnect(nego);

		if (nego->enabled_protocols[PROTOCOL_TLS] > 0)
			nego->state = NEGO_STATE_TLS;
		else if (nego->enabled_protocols[PROTOCOL_RDP] > 0)
			nego->state = NEGO_STATE_RDP;
		else
			nego->state = NEGO_STATE_FAIL;
	}
}

/**
 * Attempt negotiating TLS security.
 * @param nego
 */

void nego_attempt_tls(NEGO *nego)
{
	uint8 code;
	nego->requested_protocols = PROTOCOL_TLS;

	nego_tcp_connect(nego);
	x224_send_connection_request(nego->net->iso);
	tpkt_recv(nego->net->iso, &code, NULL);

	if (nego->state != NEGO_STATE_FINAL)
	{
		nego_tcp_disconnect(nego);

		if (nego->enabled_protocols[PROTOCOL_RDP] > 0)
			nego->state = NEGO_STATE_RDP;
		else
			nego->state = NEGO_STATE_FAIL;
	}
}

/**
 * Attempt negotiating standard RDP security.
 * @param nego
 */

void nego_attempt_rdp(NEGO *nego)
{
	uint8 code;
	nego->requested_protocols = PROTOCOL_RDP;

	nego_tcp_connect(nego);
	x224_send_connection_request(nego->net->iso);

	if (tpkt_recv(nego->net->iso, &code, NULL) == NULL)
		nego->state = NEGO_STATE_FAIL;
	else
		nego->state = NEGO_STATE_FINAL;
}

/**
 * Receive protocol security negotiation message.
 * @param nego
 * @param s
 */

void nego_recv(NEGO *nego, STREAM s)
{
	uint8 type;
	in_uint8(s, type); /* Type */

	switch (type)
	{
		case TYPE_RDP_NEG_RSP:
			nego_process_negotiation_response(nego, s);
			break;
		case TYPE_RDP_NEG_FAILURE:
			nego_process_negotiation_failure(nego, s);
			break;
	}
}

/**
 * Send protocol security negotiation message.
 * @param nego
 */

void nego_send(NEGO *nego)
{
	if (nego->state == NEGO_STATE_NLA)
		nego_attempt_nla(nego);
	else if (nego->state == NEGO_STATE_TLS)
		nego_attempt_tls(nego);
	else if (nego->state == NEGO_STATE_RDP)
		nego_attempt_rdp(nego);
}

/**
 * Process Negotiation Response from Connection Confirm message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_response(NEGO *nego, STREAM s)
{
	uint8 flags;
	uint16 length;
	uint32 selectedProtocol;

	in_uint8(s, flags);
	in_uint16_le(s, length);
	in_uint32_le(s, selectedProtocol);

	if (selectedProtocol == PROTOCOL_NLA)
		nego->selected_protocol = PROTOCOL_NLA;
	else if (selectedProtocol == PROTOCOL_TLS)
		nego->selected_protocol = PROTOCOL_TLS;
	else if (selectedProtocol == PROTOCOL_RDP)
		nego->selected_protocol = PROTOCOL_RDP;

	nego->state = NEGO_STATE_FINAL;
}

/**
 * Process Negotiation Failure from Connection Confirm message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_failure(NEGO *nego, STREAM s)
{
	uint8 flags;
	uint16 length;
	uint32 failureCode;

	in_uint8(s, flags);
	in_uint16_le(s, length);
	in_uint32_le(s, failureCode);

	switch (failureCode)
	{
		case SSL_REQUIRED_BY_SERVER:
			//printf("Error: SSL_REQUIRED_BY_SERVER\n");
			break;
		case SSL_NOT_ALLOWED_BY_SERVER:
			//printf("Error: SSL_NOT_ALLOWED_BY_SERVER\n");
			break;
		case SSL_CERT_NOT_ON_SERVER:
			//printf("Error: SSL_CERT_NOT_ON_SERVER\n");
			break;
		case INCONSISTENT_FLAGS:
			//printf("Error: INCONSISTENT_FLAGS\n");
			break;
		case HYBRID_REQUIRED_BY_SERVER:
			//printf("Error: HYBRID_REQUIRED_BY_SERVER\n");
			break;
		default:
			printf("Error: Unknown protocol security error %d\n", failureCode);
			break;
	}
}

/**
 * Create a new NEGO state machine instance.
 * @param iso
 * @return
 */

NEGO* nego_new(struct rdp_network * net)
{
	NEGO *nego = (NEGO*) xmalloc(sizeof(NEGO));

	if (nego != NULL)
	{
		memset(nego, '\0', sizeof(NEGO));
		nego->net = net;
		nego_init(nego);
	}

	return nego;
}

/**
 * Initialize NEGO state machine.
 * @param nego
 */

void nego_init(NEGO *nego)
{
	nego->state = NEGO_STATE_INITIAL;
	nego->requested_protocols = PROTOCOL_RDP;
}

/**
 * Free NEGO state machine.
 * @param nego
 */

void nego_free(NEGO *nego)
{
	xfree(nego);
}
