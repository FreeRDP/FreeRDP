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

#include <stdio.h>
#include <string.h>

#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>

#include "tpkt.h"

#include "nego.h"

/**
 * Negotiate protocol security and connect.
 * @param nego
 * @return
 */

int nego_connect(rdpNego *nego)
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
		DEBUG_NEGO("state: %s", NEGO_STATE_STRINGS[nego->state]);

		nego_send(nego);

		if (nego->state == NEGO_STATE_FAIL)
		{
			nego->state = NEGO_STATE_FINAL;
			return 0;
		}
	}

	DEBUG_NEGO("Negotiated %s security", PROTOCOL_SECURITY_STRINGS[nego->selected_protocol]);

	return 1;
}

/**
 * Connect TCP layer.
 * @param nego
 * @return
 */

int nego_tcp_connect(rdpNego *nego)
{
	if (nego->tcp_connected == 0)
	{
		if (transport_connect(nego->transport, nego->hostname, nego->port) == False)
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

int nego_tcp_disconnect(rdpNego *nego)
{
	if (nego->tcp_connected)
		transport_disconnect(nego->transport);

	nego->tcp_connected = 0;
	return 1;
}

/**
 * Attempt negotiating NLA + TLS security.
 * @param nego
 */

void nego_attempt_nla(rdpNego *nego)
{
	uint8 code;
	nego->requested_protocols = PROTOCOL_NLA | PROTOCOL_TLS;

	DEBUG_NEGO("Attempting NLA security");

	nego_tcp_connect(nego);
	nego_send_negotiation_request(nego);

	transport_check_fds(nego->transport);

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

void nego_attempt_tls(rdpNego *nego)
{
	uint8 code;
	nego->requested_protocols = PROTOCOL_TLS;

	DEBUG_NEGO("Attempting TLS security");

	nego_tcp_connect(nego);
	nego_send_negotiation_request(nego);

	transport_check_fds(nego->transport);

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

void nego_attempt_rdp(rdpNego *nego)
{
	uint8 code;
	nego->requested_protocols = PROTOCOL_RDP;

	DEBUG_NEGO("Attempting RDP security");

	nego_tcp_connect(nego);
	nego_send_negotiation_request(nego);

	transport_check_fds(nego->transport);
}

/**
 * Receive protocol security negotiation message.\n
 * @msdn{cc240501}
 * @param transport transport
 * @param s stream
 * @param extra nego pointer
 */

int nego_recv(rdpTransport * transport, STREAM* s, void * extra)
{
	uint8 li;
	uint8 type;
	rdpNego *nego = (rdpNego*) extra;

	tpkt_read_header(s);
	li = tpdu_read_connection_confirm(s);

	if (li > 6)
	{
		/* rdpNegData (optional) */

		stream_read_uint8(s, type); /* Type */

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

	nego->state = NEGO_STATE_FINAL;

	return 0;
}

/**
 * Send protocol security negotiation message.
 * @param nego
 */

void nego_send(rdpNego *nego)
{
	if (nego->state == NEGO_STATE_NLA)
		nego_attempt_nla(nego);
	else if (nego->state == NEGO_STATE_TLS)
		nego_attempt_tls(nego);
	else if (nego->state == NEGO_STATE_RDP)
		nego_attempt_rdp(nego);
}

/**
 * Send RDP Negotiation Request (RDP_NEG_REQ).\n
 * @msdn{cc240500}\n
 * @msdn{cc240470}
 * @param nego
 */

void nego_send_negotiation_request(rdpNego *nego)
{
	STREAM* s;
	int length;
	uint8 *bm, *em;

	s = stream_new(64);
	length = TPKT_HEADER_LENGTH + TPDU_CONNECTION_REQUEST_LENGTH;
	stream_get_mark(s, bm);
	stream_seek(s, length);

	if (nego->cookie)
	{
		int cookie_length = strlen(nego->cookie);
		stream_write_buffer(s, "Cookie: mstshash=", 17);
		stream_write_buffer(s, nego->cookie, cookie_length);
		stream_write_uint8(s, 0x0D); /* CR */
		stream_write_uint8(s, 0x0A); /* LF */
		length += cookie_length + 19;
	}
	else if (nego->routing_token)
	{
		int routing_token_length = strlen(nego->routing_token);
		stream_write_buffer(s, nego->routing_token, routing_token_length);
		length += routing_token_length;
	}

	if (nego->requested_protocols > PROTOCOL_RDP)
	{
		/* RDP_NEG_DATA must be present for TLS and NLA */
		stream_write_uint8(s, TYPE_RDP_NEG_REQ);
		stream_write_uint8(s, 0); /* flags, must be set to zero */
		stream_write_uint16(s, 8); /* RDP_NEG_DATA length (8) */
		stream_write_uint32(s, nego->requested_protocols); /* requestedProtocols */
		length += 8;
	}

	stream_get_mark(s, em);
	stream_set_mark(s, bm);
	tpkt_write_header(s, length);
	tpdu_write_connection_request(s, length - 5);
	stream_set_mark(s, em);

	transport_send(nego->transport, s);
}

/**
 * Process Negotiation Response from Connection Confirm message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_response(rdpNego *nego, STREAM* s)
{
	uint8 flags;
	uint16 length;
	uint32 selectedProtocol;

	DEBUG_NEGO("RDP_NEG_RSP");

	stream_read_uint8(s, flags);
	stream_read_uint16(s, length);
	stream_read_uint32(s, selectedProtocol);

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

void nego_process_negotiation_failure(rdpNego *nego, STREAM* s)
{
	uint8 flags;
	uint16 length;
	uint32 failureCode;

	DEBUG_NEGO("RDP_NEG_FAILURE");

	stream_read_uint8(s, flags);
	stream_read_uint16(s, length);
	stream_read_uint32(s, failureCode);

	switch (failureCode)
	{
		case SSL_REQUIRED_BY_SERVER:
			DEBUG_NEGO("Error: SSL_REQUIRED_BY_SERVER");
			break;
		case SSL_NOT_ALLOWED_BY_SERVER:
			DEBUG_NEGO("Error: SSL_NOT_ALLOWED_BY_SERVER");
			break;
		case SSL_CERT_NOT_ON_SERVER:
			DEBUG_NEGO("Error: SSL_CERT_NOT_ON_SERVER");
			break;
		case INCONSISTENT_FLAGS:
			DEBUG_NEGO("Error: INCONSISTENT_FLAGS");
			break;
		case HYBRID_REQUIRED_BY_SERVER:
			DEBUG_NEGO("Error: HYBRID_REQUIRED_BY_SERVER");
			break;
		default:
			DEBUG_NEGO("Error: Unknown protocol security error %d", failureCode);
			break;
	}

	nego->state = NEGO_STATE_FAIL;
}

/**
 * Initialize NEGO state machine.
 * @param nego
 */

void nego_init(rdpNego *nego)
{
	nego->state = NEGO_STATE_INITIAL;
	nego->requested_protocols = PROTOCOL_RDP;
	nego->transport->recv_callback = nego_recv;
	nego->transport->recv_extra = (void*) nego;
}

/**
 * Create a new NEGO state machine instance.
 * @param transport
 * @return
 */

rdpNego* nego_new(struct rdp_transport * transport)
{
	rdpNego *nego = (rdpNego*) xzalloc(sizeof(rdpNego));

	if (nego != NULL)
	{
		nego->transport = transport;
		nego_init(nego);
	}

	return nego;
}

/**
 * Free NEGO state machine.
 * @param nego
 */

void nego_free(rdpNego *nego)
{
	xfree(nego);
}

/**
 * Set target hostname and port.
 * @param nego
 * @param hostname
 * @param port
 */

void nego_set_target(rdpNego *nego, char* hostname, int port)
{
	nego->hostname = hostname;
	nego->port = port;
}

/**
 * Set enabled security protocols.
 * @param nego
 * @param rdp
 * @param tls
 * @param nla
 */

void nego_set_protocols(rdpNego *nego, int rdp, int tls, int nla)
{
	nego->enabled_protocols[PROTOCOL_RDP] = rdp;
	nego->enabled_protocols[PROTOCOL_TLS] = tls;
	nego->enabled_protocols[PROTOCOL_NLA] = nla;
}

/**
 * Set routing token.
 * @param nego
 * @param routing_token
 */

void nego_set_routing_token(rdpNego *nego, char* routing_token)
{
	nego->routing_token = routing_token;
}

/**
 * Set cookie.
 * @param nego
 * @param cookie
 */

void nego_set_cookie(rdpNego *nego, char* cookie)
{
	nego->cookie = cookie;
}
