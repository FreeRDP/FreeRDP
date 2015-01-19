/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>

#include "tpkt.h"

#include "nego.h"

#include "transport.h"

static const char* const NEGO_STATE_STRINGS[] =
{
	"NEGO_STATE_INITIAL",
	"NEGO_STATE_EXT",
	"NEGO_STATE_NLA",
	"NEGO_STATE_TLS",
	"NEGO_STATE_RDP",
	"NEGO_STATE_FAIL",
	"NEGO_STATE_FINAL"
};

static const char PROTOCOL_SECURITY_STRINGS[9][4] =
{
	"RDP",
	"TLS",
	"NLA",
	"UNK",
	"UNK",
	"UNK",
	"UNK",
	"UNK",
	"EXT"
};

BOOL nego_security_connect(rdpNego* nego);

/**
 * Negotiate protocol security and connect.
 * @param nego
 * @return
 */

BOOL nego_connect(rdpNego* nego)
{
	if (nego->state == NEGO_STATE_INITIAL)
	{
		if (nego->enabled_protocols[PROTOCOL_EXT])
		{
			nego->state = NEGO_STATE_EXT;
		}
		else if (nego->enabled_protocols[PROTOCOL_NLA])
		{
			nego->state = NEGO_STATE_NLA;
		}
		else if (nego->enabled_protocols[PROTOCOL_TLS])
		{
			nego->state = NEGO_STATE_TLS;
		}
		else if (nego->enabled_protocols[PROTOCOL_RDP])
		{
			nego->state = NEGO_STATE_RDP;
		}
		else
		{
			DEBUG_NEGO("No security protocol is enabled");
			nego->state = NEGO_STATE_FAIL;
			return FALSE;
		}

		if (!nego->NegotiateSecurityLayer)
		{
			DEBUG_NEGO("Security Layer Negotiation is disabled");
			/* attempt only the highest enabled protocol (see nego_attempt_*) */

			nego->enabled_protocols[PROTOCOL_NLA] = FALSE;
			nego->enabled_protocols[PROTOCOL_TLS] = FALSE;
			nego->enabled_protocols[PROTOCOL_RDP] = FALSE;
			nego->enabled_protocols[PROTOCOL_EXT] = FALSE;

			if (nego->state == NEGO_STATE_EXT)
			{
				nego->enabled_protocols[PROTOCOL_EXT] = TRUE;
				nego->enabled_protocols[PROTOCOL_NLA] = TRUE;
				nego->selected_protocol = PROTOCOL_EXT;
			}
			else if (nego->state == NEGO_STATE_NLA)
			{
				nego->enabled_protocols[PROTOCOL_NLA] = TRUE;
				nego->selected_protocol = PROTOCOL_NLA;
			}
			else if (nego->state == NEGO_STATE_TLS)
			{
				nego->enabled_protocols[PROTOCOL_TLS] = TRUE;
				nego->selected_protocol = PROTOCOL_TLS;
			}
			else if (nego->state == NEGO_STATE_RDP)
			{
				nego->enabled_protocols[PROTOCOL_RDP] = TRUE;
				nego->selected_protocol = PROTOCOL_RDP;
			}
		}

		if (!nego_send_preconnection_pdu(nego))
		{
			DEBUG_NEGO("Failed to send preconnection pdu");
			nego->state = NEGO_STATE_FINAL;
			return FALSE;
		}
	}

	do
	{
		DEBUG_NEGO("state: %s", NEGO_STATE_STRINGS[nego->state]);

		nego_send(nego);

		if (nego->state == NEGO_STATE_FAIL)
		{
			DEBUG_NEGO("Protocol Security Negotiation Failure");
			nego->state = NEGO_STATE_FINAL;
			return FALSE;
		}
	}
	while (nego->state != NEGO_STATE_FINAL);

	DEBUG_NEGO("Negotiated %s security", PROTOCOL_SECURITY_STRINGS[nego->selected_protocol]);

	/* update settings with negotiated protocol security */
	nego->transport->settings->RequestedProtocols = nego->requested_protocols;
	nego->transport->settings->SelectedProtocol = nego->selected_protocol;
	nego->transport->settings->NegotiationFlags = nego->flags;

	if (nego->selected_protocol == PROTOCOL_RDP)
	{
		nego->transport->settings->DisableEncryption = TRUE;
		nego->transport->settings->EncryptionMethods = ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_56BIT | ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS;
		nego->transport->settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	}

	/* finally connect security layer (if not already done) */
	if (!nego_security_connect(nego))
	{
		DEBUG_NEGO("Failed to connect with %s security", PROTOCOL_SECURITY_STRINGS[nego->selected_protocol]);
		return FALSE;
	}

	return TRUE;
}

/* connect to selected security layer */
BOOL nego_security_connect(rdpNego* nego)
{
	if (!nego->tcp_connected)
	{
		nego->security_connected = FALSE;
	}
	else if (!nego->security_connected)
	{
		if (nego->selected_protocol == PROTOCOL_NLA)
		{
			DEBUG_NEGO("nego_security_connect with PROTOCOL_NLA");
			nego->security_connected = transport_connect_nla(nego->transport);
		}
		else if (nego->selected_protocol == PROTOCOL_TLS)
		{
			DEBUG_NEGO("nego_security_connect with PROTOCOL_TLS");
			nego->security_connected = transport_connect_tls(nego->transport);
		}
		else if (nego->selected_protocol == PROTOCOL_RDP)
		{
			DEBUG_NEGO("nego_security_connect with PROTOCOL_RDP");
			nego->security_connected = transport_connect_rdp(nego->transport);
		}
		else
		{
			DEBUG_NEGO("cannot connect security layer because no protocol has been selected yet.");
		}
	}

	return nego->security_connected;
}

/**
 * Connect TCP layer.
 * @param nego
 * @return
 */

BOOL nego_tcp_connect(rdpNego* nego)
{
	if (!nego->tcp_connected)
	{
		if (nego->GatewayEnabled)
		{
			if (nego->GatewayBypassLocal)
			{
				/* Attempt a direct connection first, and then fallback to using the gateway */
				transport_set_gateway_enabled(nego->transport, FALSE);
				nego->tcp_connected = transport_connect(nego->transport, nego->hostname, nego->port);
			}

			if (!nego->tcp_connected)
			{
				transport_set_gateway_enabled(nego->transport, TRUE);
				nego->tcp_connected = transport_connect(nego->transport, nego->hostname, nego->port);
			}
		}
		else
		{
			nego->tcp_connected = transport_connect(nego->transport, nego->hostname, nego->port);
		}
	}

	return nego->tcp_connected;
}

/**
 * Connect TCP layer. For direct approach, connect security layer as well.
 * @param nego
 * @return
 */

BOOL nego_transport_connect(rdpNego* nego)
{
	nego_tcp_connect(nego);

	if (nego->tcp_connected && !nego->NegotiateSecurityLayer)
		return nego_security_connect(nego);

	return nego->tcp_connected;
}

/**
 * Disconnect TCP layer.
 * @param nego
 * @return
 */

int nego_transport_disconnect(rdpNego* nego)
{
	if (nego->tcp_connected)
		transport_disconnect(nego->transport);

	nego->tcp_connected = FALSE;
	nego->security_connected = FALSE;

	return 1;
}

/**
 * Send preconnection information if enabled.
 * @param nego
 * @return
 */

BOOL nego_send_preconnection_pdu(rdpNego* nego)
{
	wStream* s;
	UINT32 cbSize;
	UINT16 cchPCB = 0;
	WCHAR* wszPCB = NULL;

	if (!nego->send_preconnection_pdu)
		return TRUE;

	DEBUG_NEGO("Sending preconnection PDU");

	if (!nego_tcp_connect(nego))
		return FALSE;

	/* it's easier to always send the version 2 PDU, and it's just 2 bytes overhead */
	cbSize = PRECONNECTION_PDU_V2_MIN_SIZE;

	if (nego->preconnection_blob)
	{
		cchPCB = (UINT16) ConvertToUnicode(CP_UTF8, 0, nego->preconnection_blob, -1, &wszPCB, 0);
		cchPCB += 1; /* zero-termination */
		cbSize += cchPCB * 2;
	}

	s = Stream_New(NULL, cbSize);

	Stream_Write_UINT32(s, cbSize); /* cbSize */
	Stream_Write_UINT32(s, 0); /* Flags */
	Stream_Write_UINT32(s, PRECONNECTION_PDU_V2); /* Version */
	Stream_Write_UINT32(s, nego->preconnection_id); /* Id */
	Stream_Write_UINT16(s, cchPCB); /* cchPCB */

	if (wszPCB)
	{
		Stream_Write(s, wszPCB, cchPCB * 2); /* wszPCB */
		free(wszPCB);
	}

	Stream_SealLength(s);

	if (transport_write(nego->transport, s) < 0)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	Stream_Free(s, TRUE);

	return TRUE;
}

/**
 * Attempt negotiating NLA + TLS extended security.
 * @param nego
 */

void nego_attempt_ext(rdpNego* nego)
{
	nego->requested_protocols = PROTOCOL_NLA | PROTOCOL_TLS | PROTOCOL_EXT;

	DEBUG_NEGO("Attempting NLA extended security");

	if (!nego_transport_connect(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	DEBUG_NEGO("state: %s", NEGO_STATE_STRINGS[nego->state]);

	if (nego->state != NEGO_STATE_FINAL)
	{
		nego_transport_disconnect(nego);

		if (nego->enabled_protocols[PROTOCOL_NLA])
			nego->state = NEGO_STATE_NLA;
		else if (nego->enabled_protocols[PROTOCOL_TLS])
			nego->state = NEGO_STATE_TLS;
		else if (nego->enabled_protocols[PROTOCOL_RDP])
			nego->state = NEGO_STATE_RDP;
		else
			nego->state = NEGO_STATE_FAIL;
	}
}

/**
 * Attempt negotiating NLA + TLS security.
 * @param nego
 */

void nego_attempt_nla(rdpNego* nego)
{
	nego->requested_protocols = PROTOCOL_NLA | PROTOCOL_TLS;

	DEBUG_NEGO("Attempting NLA security");

	if (!nego_transport_connect(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	DEBUG_NEGO("state: %s", NEGO_STATE_STRINGS[nego->state]);

	if (nego->state != NEGO_STATE_FINAL)
	{
		nego_transport_disconnect(nego);

		if (nego->enabled_protocols[PROTOCOL_TLS])
			nego->state = NEGO_STATE_TLS;
		else if (nego->enabled_protocols[PROTOCOL_RDP])
			nego->state = NEGO_STATE_RDP;
		else
			nego->state = NEGO_STATE_FAIL;
	}
}

/**
 * Attempt negotiating TLS security.
 * @param nego
 */

void nego_attempt_tls(rdpNego* nego)
{
	nego->requested_protocols = PROTOCOL_TLS;

	DEBUG_NEGO("Attempting TLS security");

	if (!nego_transport_connect(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (nego->state != NEGO_STATE_FINAL)
	{
		nego_transport_disconnect(nego);

		if (nego->enabled_protocols[PROTOCOL_RDP])
			nego->state = NEGO_STATE_RDP;
		else
			nego->state = NEGO_STATE_FAIL;
	}
}

/**
 * Attempt negotiating standard RDP security.
 * @param nego
 */

void nego_attempt_rdp(rdpNego* nego)
{
	nego->requested_protocols = PROTOCOL_RDP;

	DEBUG_NEGO("Attempting RDP security");

	if (!nego_transport_connect(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego->state = NEGO_STATE_FAIL;
		return;
	}
}

/**
 * Wait to receive a negotiation response
 * @param nego
 */

BOOL nego_recv_response(rdpNego* nego)
{
	int status;
	wStream* s;

	s = Stream_New(NULL, 1024);
	if (!s)
		return FALSE;

	status = transport_read(nego->transport, s);
	if (status < 0)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	status = nego_recv(nego->transport, s, nego);

	Stream_Free(s, TRUE);

	if (status < 0)
		return FALSE;

	return TRUE;
}

/**
 * Receive protocol security negotiation message.\n
 * @msdn{cc240501}
 * @param transport transport
 * @param s stream
 * @param extra nego pointer
 */

int nego_recv(rdpTransport* transport, wStream* s, void* extra)
{
	BYTE li;
	BYTE type;
	UINT16 length;
	rdpNego* nego = (rdpNego*) extra;

	length = tpkt_read_header(s);

	if (length == 0)
		return -1;

	if (!tpdu_read_connection_confirm(s, &li))
		return -1;

	if (li > 6)
	{
		/* rdpNegData (optional) */

		Stream_Read_UINT8(s, type); /* Type */

		switch (type)
		{
			case TYPE_RDP_NEG_RSP:
				nego_process_negotiation_response(nego, s);

				DEBUG_NEGO("selected_protocol: %d", nego->selected_protocol);

				/* enhanced security selected ? */

				if (nego->selected_protocol)
				{
					if ((nego->selected_protocol == PROTOCOL_NLA) &&
						(!nego->enabled_protocols[PROTOCOL_NLA]))
					{
						nego->state = NEGO_STATE_FAIL;
					}
					if ((nego->selected_protocol == PROTOCOL_TLS) &&
						(!nego->enabled_protocols[PROTOCOL_TLS]))
					{
						nego->state = NEGO_STATE_FAIL;
					}
				}
				else if (!nego->enabled_protocols[PROTOCOL_RDP])
				{
					nego->state = NEGO_STATE_FAIL;
				}
				break;

			case TYPE_RDP_NEG_FAILURE:
				nego_process_negotiation_failure(nego, s);
				break;
		}
	}
	else if (li == 6)
	{
		DEBUG_NEGO("no rdpNegData");

		if (!nego->enabled_protocols[PROTOCOL_RDP])
			nego->state = NEGO_STATE_FAIL;
		else
			nego->state = NEGO_STATE_FINAL;
	}
	else
	{
		fprintf(stderr, "invalid negotiation response\n");
		nego->state = NEGO_STATE_FAIL;
	}

	return 0;
}

/**
 * Read protocol security negotiation request message.\n
 * @param nego
 * @param s stream
 */

BOOL nego_read_request(rdpNego* nego, wStream* s)
{
	BYTE li;
	BYTE c;
	BYTE type;

	tpkt_read_header(s);

	if (!tpdu_read_connection_request(s, &li))
		return FALSE;

	if (li != Stream_GetRemainingLength(s) + 6)
	{
		fprintf(stderr, "Incorrect TPDU length indicator.\n");
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) > 8)
	{
		/* Optional routingToken or cookie, ending with CR+LF */
		while (Stream_GetRemainingLength(s) > 0)
		{
			Stream_Read_UINT8(s, c);

			if (c != '\x0D')
				continue;

			Stream_Peek_UINT8(s, c);

			if (c != '\x0A')
				continue;

			Stream_Seek_UINT8(s);
			break;
		}
	}

	if (Stream_GetRemainingLength(s) >= 8)
	{
		/* rdpNegData (optional) */

		Stream_Read_UINT8(s, type); /* Type */

		if (type != TYPE_RDP_NEG_REQ)
		{
			fprintf(stderr, "Incorrect negotiation request type %d\n", type);
			return FALSE;
		}

		nego_process_negotiation_request(nego, s);
	}

	return TRUE;
}

/**
 * Send protocol security negotiation message.
 * @param nego
 */

void nego_send(rdpNego* nego)
{
	if (nego->state == NEGO_STATE_EXT)
		nego_attempt_ext(nego);
	else if (nego->state == NEGO_STATE_NLA)
		nego_attempt_nla(nego);
	else if (nego->state == NEGO_STATE_TLS)
		nego_attempt_tls(nego);
	else if (nego->state == NEGO_STATE_RDP)
		nego_attempt_rdp(nego);
	else
		DEBUG_NEGO("invalid negotiation state for sending");
}

/**
 * Send RDP Negotiation Request (RDP_NEG_REQ).\n
 * @msdn{cc240500}\n
 * @msdn{cc240470}
 * @param nego
 */

BOOL nego_send_negotiation_request(rdpNego* nego)
{
	wStream* s;
	int length;
	int bm, em;
	BYTE flags = 0;
	int cookie_length;

	s = Stream_New(NULL, 512);

	length = TPDU_CONNECTION_REQUEST_LENGTH;
	bm = Stream_GetPosition(s);
	Stream_Seek(s, length);

	if (nego->RoutingToken)
	{
		Stream_Write(s, nego->RoutingToken, nego->RoutingTokenLength);
		/* Ensure Routing Token is correctly terminated - may already be present in string */
		if (nego->RoutingTokenLength>2 && (nego->RoutingToken[nego->RoutingTokenLength-2]==0x0D && nego->RoutingToken[nego->RoutingTokenLength-1]==0x0A))
		{
			DEBUG_NEGO("Routing token looks correctly terminated - use verbatim");
			length +=nego->RoutingTokenLength;
		}
		else
		{
			DEBUG_NEGO("Adding terminating CRLF to routing token");
			Stream_Write_UINT8(s, 0x0D); /* CR */
			Stream_Write_UINT8(s, 0x0A); /* LF */
			length += nego->RoutingTokenLength + 2;
		}
	}
	else if (nego->cookie)
	{
		cookie_length = strlen(nego->cookie);

		if (cookie_length > (int) nego->cookie_max_length)
			cookie_length = nego->cookie_max_length;

		Stream_Write(s, "Cookie: mstshash=", 17);
		Stream_Write(s, (BYTE*) nego->cookie, cookie_length);
		Stream_Write_UINT8(s, 0x0D); /* CR */
		Stream_Write_UINT8(s, 0x0A); /* LF */
		length += cookie_length + 19;
	}

	DEBUG_NEGO("requested_protocols: %d", nego->requested_protocols);

	if ((nego->requested_protocols > PROTOCOL_RDP) || (nego->sendNegoData))
	{
		/* RDP_NEG_DATA must be present for TLS and NLA */

		if (nego->RestrictedAdminModeRequired)
			flags |= RESTRICTED_ADMIN_MODE_REQUIRED;

		Stream_Write_UINT8(s, TYPE_RDP_NEG_REQ);
		Stream_Write_UINT8(s, flags);
		Stream_Write_UINT16(s, 8); /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, nego->requested_protocols); /* requestedProtocols */
		length += 8;
	}

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	tpkt_write_header(s, length);
	tpdu_write_connection_request(s, length - 5);
	Stream_SetPosition(s, em);

	Stream_SealLength(s);

	if (transport_write(nego->transport, s) < 0)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	Stream_Free(s, TRUE);

	return TRUE;
}

/**
 * Process Negotiation Request from Connection Request message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_request(rdpNego* nego, wStream* s)
{
	BYTE flags;
	UINT16 length;

	DEBUG_NEGO("RDP_NEG_REQ");

	Stream_Read_UINT8(s, flags);
	Stream_Read_UINT16(s, length);
	Stream_Read_UINT32(s, nego->requested_protocols);

	DEBUG_NEGO("requested_protocols: %d", nego->requested_protocols);

	nego->state = NEGO_STATE_FINAL;
}

/**
 * Process Negotiation Response from Connection Confirm message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_response(rdpNego* nego, wStream* s)
{
	UINT16 length;

	DEBUG_NEGO("RDP_NEG_RSP");

	if (Stream_GetRemainingLength(s) < 7)
	{
		DEBUG_NEGO("RDP_INVALID_NEG_RSP");
		nego->state = NEGO_STATE_FAIL;
		return;
	}

	Stream_Read_UINT8(s, nego->flags);
	Stream_Read_UINT16(s, length);
	Stream_Read_UINT32(s, nego->selected_protocol);

	nego->state = NEGO_STATE_FINAL;
}

/**
 * Process Negotiation Failure from Connection Confirm message.
 * @param nego
 * @param s
 */

void nego_process_negotiation_failure(rdpNego* nego, wStream* s)
{
	BYTE flags;
	UINT16 length;
	UINT32 failureCode;

	DEBUG_NEGO("RDP_NEG_FAILURE");

	Stream_Read_UINT8(s, flags);
	Stream_Read_UINT16(s, length);
	Stream_Read_UINT32(s, failureCode);

	switch (failureCode)
	{
		case SSL_REQUIRED_BY_SERVER:
			DEBUG_NEGO("Error: SSL_REQUIRED_BY_SERVER");
			break;

		case SSL_NOT_ALLOWED_BY_SERVER:
			DEBUG_NEGO("Error: SSL_NOT_ALLOWED_BY_SERVER");
			nego->sendNegoData = TRUE;
			break;

		case SSL_CERT_NOT_ON_SERVER:
			DEBUG_NEGO("Error: SSL_CERT_NOT_ON_SERVER");
			nego->sendNegoData = TRUE;
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
 * Send RDP Negotiation Response (RDP_NEG_RSP).\n
 * @param nego
 */

BOOL nego_send_negotiation_response(rdpNego* nego)
{
	int length;
	int bm, em;
	BOOL status;
	wStream* s;
	BYTE flags;
	rdpSettings* settings;

	status = TRUE;
	settings = nego->transport->settings;

	s = Stream_New(NULL, 512);
	if (!s)
		return FALSE;

	length = TPDU_CONNECTION_CONFIRM_LENGTH;
	bm = Stream_GetPosition(s);
	Stream_Seek(s, length);

	if (nego->selected_protocol > PROTOCOL_RDP)
	{
		flags = EXTENDED_CLIENT_DATA_SUPPORTED;

		if (settings->SupportGraphicsPipeline)
			flags |= DYNVC_GFX_PROTOCOL_SUPPORTED;

		/* RDP_NEG_DATA must be present for TLS and NLA */
		Stream_Write_UINT8(s, TYPE_RDP_NEG_RSP);
		Stream_Write_UINT8(s, flags); /* flags */
		Stream_Write_UINT16(s, 8); /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, nego->selected_protocol); /* selectedProtocol */
		length += 8;
	}
	else if (!settings->RdpSecurity)
	{
		flags = 0;

		Stream_Write_UINT8(s, TYPE_RDP_NEG_FAILURE);
		Stream_Write_UINT8(s, flags); /* flags */
		Stream_Write_UINT16(s, 8); /* RDP_NEG_DATA length (8) */
		/*
		 * TODO: Check for other possibilities,
		 *       like SSL_NOT_ALLOWED_BY_SERVER.
		 */
		fprintf(stderr, "%s: client supports only Standard RDP Security\n", __FUNCTION__);
		Stream_Write_UINT32(s, SSL_REQUIRED_BY_SERVER);
		length += 8;
		status = FALSE;
	}

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	tpkt_write_header(s, length);
	tpdu_write_connection_confirm(s, length - 5);
	Stream_SetPosition(s, em);

	Stream_SealLength(s);

	if (transport_write(nego->transport, s) < 0)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	Stream_Free(s, TRUE);

	if (status)
	{
		/* update settings with negotiated protocol security */
		settings->RequestedProtocols = nego->requested_protocols;
		settings->SelectedProtocol = nego->selected_protocol;

		if (settings->SelectedProtocol == PROTOCOL_RDP)
		{
			settings->TlsSecurity = FALSE;
			settings->NlaSecurity = FALSE;
			settings->RdpSecurity = TRUE;

			if (!settings->LocalConnection)
			{
				settings->DisableEncryption = TRUE;
				settings->EncryptionMethods = ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_56BIT | ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS;
				settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
			}

			if (settings->DisableEncryption && !settings->RdpServerRsaKey && !settings->RdpKeyFile)
				return FALSE;
		}
		else if (settings->SelectedProtocol == PROTOCOL_TLS)
		{
			settings->TlsSecurity = TRUE;
			settings->NlaSecurity = FALSE;
			settings->RdpSecurity = FALSE;
			settings->DisableEncryption = FALSE;
			settings->EncryptionMethods = ENCRYPTION_METHOD_NONE;
			settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
		}
		else if (settings->SelectedProtocol == PROTOCOL_NLA)
		{
			settings->TlsSecurity = TRUE;
			settings->NlaSecurity = TRUE;
			settings->RdpSecurity = FALSE;
			settings->DisableEncryption = FALSE;
			settings->EncryptionMethods = ENCRYPTION_METHOD_NONE;
			settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
		}
	}

	return status;
}

/**
 * Initialize NEGO state machine.
 * @param nego
 */

void nego_init(rdpNego* nego)
{
	nego->state = NEGO_STATE_INITIAL;
	nego->requested_protocols = PROTOCOL_RDP;
	nego->transport->ReceiveCallback = nego_recv;
	nego->transport->ReceiveExtra = (void*) nego;
	nego->cookie_max_length = DEFAULT_COOKIE_MAX_LENGTH;
	nego->sendNegoData = FALSE;
	nego->flags = 0;
}

/**
 * Create a new NEGO state machine instance.
 * @param transport
 * @return
 */

rdpNego* nego_new(rdpTransport* transport)
{
	rdpNego* nego = (rdpNego*) calloc(1, sizeof(rdpNego));
	if (!nego)
		return NULL;


	nego->transport = transport;
	nego_init(nego);

	return nego;
}

/**
 * Free NEGO state machine.
 * @param nego
 */

void nego_free(rdpNego* nego)
{
	free(nego->RoutingToken);
	free(nego->cookie);
	free(nego);
}

/**
 * Set target hostname and port.
 * @param nego
 * @param hostname
 * @param port
 */

void nego_set_target(rdpNego* nego, char* hostname, int port)
{
	nego->hostname = hostname;
	nego->port = port;
}

/**
 * Enable security layer negotiation.
 * @param nego pointer to the negotiation structure
 * @param enable_rdp whether to enable security layer negotiation (TRUE for enabled, FALSE for disabled)
 */

void nego_set_negotiation_enabled(rdpNego* nego, BOOL NegotiateSecurityLayer)
{
	DEBUG_NEGO("Enabling security layer negotiation: %s", NegotiateSecurityLayer ? "TRUE" : "FALSE");
	nego->NegotiateSecurityLayer = NegotiateSecurityLayer;
}

/**
 * Enable restricted admin mode.
 * @param nego pointer to the negotiation structure
 * @param enable_restricted whether to enable security layer negotiation (TRUE for enabled, FALSE for disabled)
 */

void nego_set_restricted_admin_mode_required(rdpNego* nego, BOOL RestrictedAdminModeRequired)
{
	DEBUG_NEGO("Enabling restricted admin mode: %s", RestrictedAdminModeRequired ? "TRUE" : "FALSE");
	nego->RestrictedAdminModeRequired = RestrictedAdminModeRequired;
}

void nego_set_gateway_enabled(rdpNego* nego, BOOL GatewayEnabled)
{
	nego->GatewayEnabled = GatewayEnabled;
}

void nego_set_gateway_bypass_local(rdpNego* nego, BOOL GatewayBypassLocal)
{
	nego->GatewayBypassLocal = GatewayBypassLocal;
}

/**
 * Enable RDP security protocol.
 * @param nego pointer to the negotiation structure
 * @param enable_rdp whether to enable normal RDP protocol (TRUE for enabled, FALSE for disabled)
 */

void nego_enable_rdp(rdpNego* nego, BOOL enable_rdp)
{
	DEBUG_NEGO("Enabling RDP security: %s", enable_rdp ? "TRUE" : "FALSE");
	nego->enabled_protocols[PROTOCOL_RDP] = enable_rdp;
}

/**
 * Enable TLS security protocol.
 * @param nego pointer to the negotiation structure
 * @param enable_tls whether to enable TLS + RDP protocol (TRUE for enabled, FALSE for disabled)
 */

void nego_enable_tls(rdpNego* nego, BOOL enable_tls)
{
	DEBUG_NEGO("Enabling TLS security: %s", enable_tls ? "TRUE" : "FALSE");
	nego->enabled_protocols[PROTOCOL_TLS] = enable_tls;
}

/**
 * Enable NLA security protocol.
 * @param nego pointer to the negotiation structure
 * @param enable_nla whether to enable network level authentication protocol (TRUE for enabled, FALSE for disabled)
 */

void nego_enable_nla(rdpNego* nego, BOOL enable_nla)
{
	DEBUG_NEGO("Enabling NLA security: %s", enable_nla ? "TRUE" : "FALSE");
	nego->enabled_protocols[PROTOCOL_NLA] = enable_nla;
}

/**
 * Enable NLA extended security protocol.
 * @param nego pointer to the negotiation structure
 * @param enable_ext whether to enable network level authentication extended protocol (TRUE for enabled, FALSE for disabled)
 */

void nego_enable_ext(rdpNego* nego, BOOL enable_ext)
{
	DEBUG_NEGO("Enabling NLA extended security: %s", enable_ext ? "TRUE" : "FALSE");
	nego->enabled_protocols[PROTOCOL_EXT] = enable_ext;
}

/**
 * Set routing token.
 * @param nego
 * @param RoutingToken
 * @param RoutingTokenLength
 */

BOOL nego_set_routing_token(rdpNego* nego, BYTE* RoutingToken, DWORD RoutingTokenLength)
{
	free(nego->RoutingToken);
	nego->RoutingTokenLength = RoutingTokenLength;
	nego->RoutingToken = (BYTE*) malloc(nego->RoutingTokenLength);
	if (!nego->RoutingToken)
		return FALSE;
	CopyMemory(nego->RoutingToken, RoutingToken, nego->RoutingTokenLength);
	return TRUE;
}

/**
 * Set cookie.
 * @param nego
 * @param cookie
 */

BOOL nego_set_cookie(rdpNego* nego, char* cookie)
{
	if (nego->cookie)
	{
		free(nego->cookie);
		nego->cookie = 0;
	}

	if (!cookie)
		return TRUE;

	nego->cookie = _strdup(cookie);
	if (!nego->cookie)
		return FALSE;
	return TRUE;
}

/**
 * Set cookie maximum length
 * @param nego
 * @param cookie_max_length
 */

void nego_set_cookie_max_length(rdpNego* nego, UINT32 cookie_max_length)
{
	nego->cookie_max_length = cookie_max_length;
}

/**
 * Enable / disable preconnection PDU.
 * @param nego
 * @param send_pcpdu
 */

void nego_set_send_preconnection_pdu(rdpNego* nego, BOOL send_pcpdu)
{
	nego->send_preconnection_pdu = send_pcpdu;
}

/**
 * Set preconnection id.
 * @param nego
 * @param id
 */

void nego_set_preconnection_id(rdpNego* nego, UINT32 id)
{
	nego->preconnection_id = id;
}

/**
 * Set preconnection blob.
 * @param nego
 * @param blob
 */

void nego_set_preconnection_blob(rdpNego* nego, char* blob)
{
	nego->preconnection_blob = blob;
}
