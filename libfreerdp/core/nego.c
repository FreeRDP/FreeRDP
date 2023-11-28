/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Protocol Security Negotiation
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/stream.h>

#include <freerdp/log.h>

#include "tpkt.h"

#include "nego.h"
#include "aad.h"

#include "transport.h"

#define TAG FREERDP_TAG("core.nego")

struct rdp_nego
{
	UINT16 port;
	UINT32 flags;
	const char* hostname;
	char* cookie;
	BYTE* RoutingToken;
	DWORD RoutingTokenLength;
	BOOL SendPreconnectionPdu;
	UINT32 PreconnectionId;
	const char* PreconnectionBlob;

	NEGO_STATE state;
	BOOL TcpConnected;
	BOOL SecurityConnected;
	UINT32 CookieMaxLength;

	BOOL sendNegoData;
	UINT32 SelectedProtocol;
	UINT32 RequestedProtocols;
	BOOL NegotiateSecurityLayer;
	BOOL EnabledProtocols[32];
	BOOL RestrictedAdminModeRequired;
	BOOL RemoteCredsGuardRequired;
	BOOL RemoteCredsGuardActive;
	BOOL RemoteCredsGuardSupported;
	BOOL GatewayEnabled;
	BOOL GatewayBypassLocal;
	BOOL ConnectChildSession;

	rdpTransport* transport;
};

static const char* nego_state_string(NEGO_STATE state)
{
	static const char* const NEGO_STATE_STRINGS[] = { "NEGO_STATE_INITIAL", "NEGO_STATE_RDSTLS",
		                                              "NEGO_STATE_AAD",     "NEGO_STATE_EXT",
		                                              "NEGO_STATE_NLA",     "NEGO_STATE_TLS",
		                                              "NEGO_STATE_RDP",     "NEGO_STATE_FAIL",
		                                              "NEGO_STATE_FINAL",   "NEGO_STATE_INVALID" };
	if (state >= ARRAYSIZE(NEGO_STATE_STRINGS))
		return NEGO_STATE_STRINGS[ARRAYSIZE(NEGO_STATE_STRINGS) - 1];
	return NEGO_STATE_STRINGS[state];
}

static const char* protocol_security_string(UINT32 security)
{
	static const char* PROTOCOL_SECURITY_STRINGS[] = { "RDP", "TLS", "NLA", "UNK", "RDSTLS",
		                                               "UNK", "UNK", "UNK", "EXT", "UNK",
		                                               "UNK", "UNK", "UNK", "UNK", "UNK",
		                                               "UNK", "AAD", "UNK", "UNK", "UNK" };
	if (security >= ARRAYSIZE(PROTOCOL_SECURITY_STRINGS))
		return PROTOCOL_SECURITY_STRINGS[ARRAYSIZE(PROTOCOL_SECURITY_STRINGS) - 1];
	return PROTOCOL_SECURITY_STRINGS[security];
}

static BOOL nego_tcp_connect(rdpNego* nego);
static BOOL nego_transport_connect(rdpNego* nego);
static BOOL nego_transport_disconnect(rdpNego* nego);
static BOOL nego_security_connect(rdpNego* nego);
static BOOL nego_send_preconnection_pdu(rdpNego* nego);
static BOOL nego_recv_response(rdpNego* nego);
static void nego_send(rdpNego* nego);
static BOOL nego_process_negotiation_request(rdpNego* nego, wStream* s);
static BOOL nego_process_negotiation_response(rdpNego* nego, wStream* s);
static BOOL nego_process_negotiation_failure(rdpNego* nego, wStream* s);

BOOL nego_update_settings_from_state(rdpNego* nego, rdpSettings* settings)
{
	WINPR_ASSERT(nego);

	/* update settings with negotiated protocol security */
	return freerdp_settings_set_uint32(settings, FreeRDP_RequestedProtocols,
	                                   nego->RequestedProtocols) &&
	       freerdp_settings_set_uint32(settings, FreeRDP_SelectedProtocol,
	                                   nego->SelectedProtocol) &&
	       freerdp_settings_set_uint32(settings, FreeRDP_NegotiationFlags, nego->flags);
}

/**
 * Negotiate protocol security and connect.
 *
 * @param nego A pointer to the NEGO struct
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_connect(rdpNego* nego)
{
	rdpContext* context;
	rdpSettings* settings;
	WINPR_ASSERT(nego);
	context = transport_get_context(nego->transport);
	WINPR_ASSERT(context);
	settings = context->settings;
	WINPR_ASSERT(settings);

	if (nego_get_state(nego) == NEGO_STATE_INITIAL)
	{
		if (nego->EnabledProtocols[PROTOCOL_RDSAAD])
		{
			nego_set_state(nego, NEGO_STATE_AAD);
		}
		else if (nego->EnabledProtocols[PROTOCOL_RDSTLS])
		{
			nego_set_state(nego, NEGO_STATE_RDSTLS);
		}
		else if (nego->EnabledProtocols[PROTOCOL_HYBRID_EX])
		{
			nego_set_state(nego, NEGO_STATE_EXT);
		}
		else if (nego->EnabledProtocols[PROTOCOL_HYBRID])
		{
			nego_set_state(nego, NEGO_STATE_NLA);
		}
		else if (nego->EnabledProtocols[PROTOCOL_SSL])
		{
			nego_set_state(nego, NEGO_STATE_TLS);
		}
		else if (nego->EnabledProtocols[PROTOCOL_RDP])
		{
			nego_set_state(nego, NEGO_STATE_RDP);
		}
		else
		{
			WLog_ERR(TAG, "No security protocol is enabled");
			nego_set_state(nego, NEGO_STATE_FAIL);
			return FALSE;
		}

		if (!nego->NegotiateSecurityLayer)
		{
			WLog_DBG(TAG, "Security Layer Negotiation is disabled");
			/* attempt only the highest enabled protocol (see nego_attempt_*) */
			nego->EnabledProtocols[PROTOCOL_RDSAAD] = FALSE;
			nego->EnabledProtocols[PROTOCOL_HYBRID] = FALSE;
			nego->EnabledProtocols[PROTOCOL_SSL] = FALSE;
			nego->EnabledProtocols[PROTOCOL_RDP] = FALSE;
			nego->EnabledProtocols[PROTOCOL_HYBRID_EX] = FALSE;
			nego->EnabledProtocols[PROTOCOL_RDSTLS] = FALSE;

			UINT32 SelectedProtocol = 0;
			switch (nego_get_state(nego))
			{
				case NEGO_STATE_AAD:
					nego->EnabledProtocols[PROTOCOL_RDSAAD] = TRUE;
					SelectedProtocol = PROTOCOL_RDSAAD;
					break;
				case NEGO_STATE_RDSTLS:
					nego->EnabledProtocols[PROTOCOL_RDSTLS] = TRUE;
					SelectedProtocol = PROTOCOL_RDSTLS;
					break;
				case NEGO_STATE_EXT:
					nego->EnabledProtocols[PROTOCOL_HYBRID_EX] = TRUE;
					nego->EnabledProtocols[PROTOCOL_HYBRID] = TRUE;
					SelectedProtocol = PROTOCOL_HYBRID_EX;
					break;
				case NEGO_STATE_NLA:
					nego->EnabledProtocols[PROTOCOL_HYBRID] = TRUE;
					SelectedProtocol = PROTOCOL_HYBRID;
					break;
				case NEGO_STATE_TLS:
					nego->EnabledProtocols[PROTOCOL_SSL] = TRUE;
					SelectedProtocol = PROTOCOL_SSL;
					break;
				case NEGO_STATE_RDP:
					nego->EnabledProtocols[PROTOCOL_RDP] = TRUE;
					SelectedProtocol = PROTOCOL_RDP;
					break;
				default:
					WLog_ERR(TAG, "Invalid NEGO state 0x%08" PRIx32, nego_get_state(nego));
					return FALSE;
			}
			if (!nego_set_selected_protocol(nego, SelectedProtocol))
				return FALSE;
		}

		if (!nego_tcp_connect(nego))
		{
			WLog_ERR(TAG, "Failed to connect");
			return FALSE;
		}

		if (nego->SendPreconnectionPdu)
		{
			if (!nego_send_preconnection_pdu(nego))
			{
				WLog_ERR(TAG, "Failed to send preconnection pdu");
				nego_set_state(nego, NEGO_STATE_FINAL);
				return FALSE;
			}
		}
	}

	if (!nego->NegotiateSecurityLayer)
	{
		nego_set_state(nego, NEGO_STATE_FINAL);
	}
	else
	{
		do
		{
			WLog_DBG(TAG, "state: %s", nego_state_string(nego_get_state(nego)));
			nego_send(nego);

			if (nego_get_state(nego) == NEGO_STATE_FAIL)
			{
				if (freerdp_get_last_error(transport_get_context(nego->transport)) ==
				    FREERDP_ERROR_SUCCESS)
					WLog_ERR(TAG, "Protocol Security Negotiation Failure");

				nego_set_state(nego, NEGO_STATE_FINAL);
				return FALSE;
			}
		} while (nego_get_state(nego) != NEGO_STATE_FINAL);
	}

	WLog_DBG(TAG, "Negotiated %s security", protocol_security_string(nego->SelectedProtocol));

	/* update settings with negotiated protocol security */
	if (!nego_update_settings_from_state(nego, settings))
		return FALSE;

	if (nego->SelectedProtocol == PROTOCOL_RDP)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, TRUE))
			return FALSE;

		if (freerdp_settings_get_uint32(settings, FreeRDP_EncryptionMethods) == 0)
		{
			/**
			 * Advertise all supported encryption methods if the client
			 * implementation did not set any security methods
			 */
			if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionMethods,
			                                 ENCRYPTION_METHOD_40BIT | ENCRYPTION_METHOD_56BIT |
			                                     ENCRYPTION_METHOD_128BIT | ENCRYPTION_METHOD_FIPS))
				return FALSE;
		}
	}

	/* finally connect security layer (if not already done) */
	if (!nego_security_connect(nego))
	{
		WLog_DBG(TAG, "Failed to connect with %s security",
		         protocol_security_string(nego->SelectedProtocol));
		return FALSE;
	}

	return TRUE;
}

BOOL nego_disconnect(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	nego_set_state(nego, NEGO_STATE_INITIAL);
	return nego_transport_disconnect(nego);
}

static BOOL nego_try_connect(rdpNego* nego)
{
	WINPR_ASSERT(nego);

	switch (nego->SelectedProtocol)
	{
		case PROTOCOL_RDSAAD:
			WLog_DBG(TAG, "nego_security_connect with PROTOCOL_RDSAAD");
			nego->SecurityConnected = transport_connect_aad(nego->transport);
			break;
		case PROTOCOL_RDSTLS:
			WLog_DBG(TAG, "nego_security_connect with PROTOCOL_RDSTLS");
			nego->SecurityConnected = transport_connect_rdstls(nego->transport);
			break;
		case PROTOCOL_HYBRID:
			WLog_DBG(TAG, "nego_security_connect with PROTOCOL_HYBRID");
			nego->SecurityConnected = transport_connect_nla(nego->transport, FALSE);
			break;
		case PROTOCOL_HYBRID_EX:
			WLog_DBG(TAG, "nego_security_connect with PROTOCOL_HYBRID_EX");
			nego->SecurityConnected = transport_connect_nla(nego->transport, TRUE);
			break;
		case PROTOCOL_SSL:
			WLog_DBG(TAG, "nego_security_connect with PROTOCOL_SSL");
			nego->SecurityConnected = transport_connect_tls(nego->transport);
			break;
		case PROTOCOL_RDP:
			WLog_DBG(TAG, "nego_security_connect with PROTOCOL_RDP");
			nego->SecurityConnected = transport_connect_rdp(nego->transport);
			break;
		default:
			WLog_ERR(TAG,
			         "cannot connect security layer because no protocol has been selected yet.");
			return FALSE;
	}
	return nego->SecurityConnected;
}

/* connect to selected security layer */
BOOL nego_security_connect(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	if (!nego->TcpConnected)
	{
		nego->SecurityConnected = FALSE;
	}
	else if (!nego->SecurityConnected)
	{
		if (!nego_try_connect(nego))
			return FALSE;
	}

	return nego->SecurityConnected;
}

static BOOL nego_tcp_connect(rdpNego* nego)
{
	rdpContext* context;
	WINPR_ASSERT(nego);
	if (!nego->TcpConnected)
	{
		UINT32 TcpConnectTimeout;

		context = transport_get_context(nego->transport);
		WINPR_ASSERT(context);

		TcpConnectTimeout =
		    freerdp_settings_get_uint32(context->settings, FreeRDP_TcpConnectTimeout);

		if (nego->GatewayEnabled)
		{
			if (nego->GatewayBypassLocal)
			{
				/* Attempt a direct connection first, and then fallback to using the gateway */
				WLog_INFO(TAG,
				          "Detecting if host can be reached locally. - This might take some time.");
				WLog_INFO(TAG, "To disable auto detection use /gateway-usage-method:direct");
				transport_set_gateway_enabled(nego->transport, FALSE);
				nego->TcpConnected = transport_connect(nego->transport, nego->hostname, nego->port,
				                                       TcpConnectTimeout);
			}

			if (!nego->TcpConnected)
			{
				transport_set_gateway_enabled(nego->transport, TRUE);
				nego->TcpConnected = transport_connect(nego->transport, nego->hostname, nego->port,
				                                       TcpConnectTimeout);
			}
		}
		else if (nego->ConnectChildSession)
		{
			nego->TcpConnected = transport_connect_childsession(nego->transport);
		}
		else
		{
			nego->TcpConnected =
			    transport_connect(nego->transport, nego->hostname, nego->port, TcpConnectTimeout);
		}
	}

	return nego->TcpConnected;
}

/**
 * Connect TCP layer. For direct approach, connect security layer as well.
 *
 * @param nego A pointer to the NEGO struct
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_transport_connect(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	if (!nego_tcp_connect(nego))
		return FALSE;

	if (nego->TcpConnected && !nego->NegotiateSecurityLayer)
		return nego_security_connect(nego);

	return nego->TcpConnected;
}

/**
 * Disconnect TCP layer.
 *
 * @param nego A pointer to the NEGO struct
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_transport_disconnect(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	if (nego->TcpConnected)
		transport_disconnect(nego->transport);

	nego->TcpConnected = FALSE;
	nego->SecurityConnected = FALSE;
	return TRUE;
}

/**
 * Send preconnection information if enabled.
 *
 * @param nego A pointer to the NEGO struct
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_send_preconnection_pdu(rdpNego* nego)
{
	wStream* s;
	UINT32 cbSize;
	UINT16 cchPCB = 0;
	WCHAR* wszPCB = NULL;

	WINPR_ASSERT(nego);

	WLog_DBG(TAG, "Sending preconnection PDU");

	if (!nego_tcp_connect(nego))
		return FALSE;

	/* it's easier to always send the version 2 PDU, and it's just 2 bytes overhead */
	cbSize = PRECONNECTION_PDU_V2_MIN_SIZE;

	if (nego->PreconnectionBlob)
	{
		size_t len = 0;
		wszPCB = ConvertUtf8ToWCharAlloc(nego->PreconnectionBlob, &len);
		if (len > UINT16_MAX - 1)
		{
			free(wszPCB);
			return FALSE;
		}
		cchPCB = len;
		cchPCB += 1; /* zero-termination */
		cbSize += cchPCB * sizeof(WCHAR);
	}

	s = Stream_New(NULL, cbSize);

	if (!s)
	{
		free(wszPCB);
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	Stream_Write_UINT32(s, cbSize);                /* cbSize */
	Stream_Write_UINT32(s, 0);                     /* Flags */
	Stream_Write_UINT32(s, PRECONNECTION_PDU_V2);  /* Version */
	Stream_Write_UINT32(s, nego->PreconnectionId); /* Id */
	Stream_Write_UINT16(s, cchPCB);                /* cchPCB */

	if (wszPCB)
	{
		Stream_Write(s, wszPCB, cchPCB * sizeof(WCHAR)); /* wszPCB */
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

static void nego_attempt_rdstls(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	nego->RequestedProtocols = PROTOCOL_RDSTLS | PROTOCOL_SSL;
	WLog_DBG(TAG, "Attempting RDSTLS security");

	if (!nego_transport_connect(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	WLog_DBG(TAG, "state: %s", nego_state_string(nego_get_state(nego)));

	if (nego_get_state(nego) != NEGO_STATE_FINAL)
	{
		nego_transport_disconnect(nego);

		if (nego->EnabledProtocols[PROTOCOL_HYBRID_EX])
			nego_set_state(nego, NEGO_STATE_EXT);
		else if (nego->EnabledProtocols[PROTOCOL_HYBRID])
			nego_set_state(nego, NEGO_STATE_NLA);
		else if (nego->EnabledProtocols[PROTOCOL_SSL])
			nego_set_state(nego, NEGO_STATE_TLS);
		else if (nego->EnabledProtocols[PROTOCOL_RDP])
			nego_set_state(nego, NEGO_STATE_RDP);
		else
			nego_set_state(nego, NEGO_STATE_FAIL);
	}
}

static void nego_attempt_rdsaad(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	nego->RequestedProtocols = PROTOCOL_RDSAAD;
	WLog_DBG(TAG, "Attempting RDS AAD Auth security");

	if (!nego_transport_connect(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	WLog_DBG(TAG, "state: %s", nego_state_string(nego_get_state(nego)));

	if (nego_get_state(nego) != NEGO_STATE_FINAL)
	{
		nego_transport_disconnect(nego);

		if (nego->EnabledProtocols[PROTOCOL_HYBRID_EX])
			nego_set_state(nego, NEGO_STATE_EXT);
		else if (nego->EnabledProtocols[PROTOCOL_HYBRID])
			nego_set_state(nego, NEGO_STATE_NLA);
		else if (nego->EnabledProtocols[PROTOCOL_SSL])
			nego_set_state(nego, NEGO_STATE_TLS);
		else if (nego->EnabledProtocols[PROTOCOL_RDP])
			nego_set_state(nego, NEGO_STATE_RDP);
		else
			nego_set_state(nego, NEGO_STATE_FAIL);
	}
}

static void nego_attempt_ext(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	nego->RequestedProtocols = PROTOCOL_HYBRID | PROTOCOL_SSL | PROTOCOL_HYBRID_EX;
	WLog_DBG(TAG, "Attempting NLA extended security");

	if (!nego_transport_connect(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	WLog_DBG(TAG, "state: %s", nego_state_string(nego_get_state(nego)));

	if (nego_get_state(nego) != NEGO_STATE_FINAL)
	{
		nego_transport_disconnect(nego);

		if (nego->EnabledProtocols[PROTOCOL_HYBRID])
			nego_set_state(nego, NEGO_STATE_NLA);
		else if (nego->EnabledProtocols[PROTOCOL_SSL])
			nego_set_state(nego, NEGO_STATE_TLS);
		else if (nego->EnabledProtocols[PROTOCOL_RDP])
			nego_set_state(nego, NEGO_STATE_RDP);
		else
			nego_set_state(nego, NEGO_STATE_FAIL);
	}
}

static void nego_attempt_nla(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	nego->RequestedProtocols = PROTOCOL_HYBRID | PROTOCOL_SSL;
	WLog_DBG(TAG, "Attempting NLA security");

	if (!nego_transport_connect(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	WLog_DBG(TAG, "state: %s", nego_state_string(nego_get_state(nego)));

	if (nego_get_state(nego) != NEGO_STATE_FINAL)
	{
		nego_transport_disconnect(nego);

		if (nego->EnabledProtocols[PROTOCOL_SSL])
			nego_set_state(nego, NEGO_STATE_TLS);
		else if (nego->EnabledProtocols[PROTOCOL_RDP])
			nego_set_state(nego, NEGO_STATE_RDP);
		else
			nego_set_state(nego, NEGO_STATE_FAIL);
	}
}

static void nego_attempt_tls(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	nego->RequestedProtocols = PROTOCOL_SSL;
	WLog_DBG(TAG, "Attempting TLS security");

	if (!nego_transport_connect(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (nego_get_state(nego) != NEGO_STATE_FINAL)
	{
		nego_transport_disconnect(nego);

		if (nego->EnabledProtocols[PROTOCOL_RDP])
			nego_set_state(nego, NEGO_STATE_RDP);
		else
			nego_set_state(nego, NEGO_STATE_FAIL);
	}
}

static void nego_attempt_rdp(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	nego->RequestedProtocols = PROTOCOL_RDP;
	WLog_DBG(TAG, "Attempting RDP security");

	if (!nego_transport_connect(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_send_negotiation_request(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}

	if (!nego_recv_response(nego))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return;
	}
}

/**
 * Wait to receive a negotiation response
 *
 * @param nego A pointer to the NEGO struct
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL nego_recv_response(rdpNego* nego)
{
	int status;
	wStream* s;

	WINPR_ASSERT(nego);
	s = Stream_New(NULL, 1024);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	status = transport_read_pdu(nego->transport, s);

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
 * Receive protocol security negotiation message.
 * msdn{cc240501}
 *
 * @param transport The transport to read from
 * @param s A stream to read the received data from
 * @param extra nego pointer
 *
 * @return \b 0 for success, \b -1 for failure
 */

int nego_recv(rdpTransport* transport, wStream* s, void* extra)
{
	BYTE li;
	BYTE type;
	UINT16 length;
	rdpNego* nego = (rdpNego*)extra;

	WINPR_ASSERT(nego);
	if (!tpkt_read_header(s, &length))
		return -1;

	if (!tpdu_read_connection_confirm(s, &li, length))
		return -1;

	if (li > 6)
	{
		/* rdpNegData (optional) */
		Stream_Read_UINT8(s, type); /* Type */

		switch (type)
		{
			case TYPE_RDP_NEG_RSP:
				if (!nego_process_negotiation_response(nego, s))
					return -1;
				WLog_DBG(TAG, "selected_protocol: %" PRIu32 "", nego->SelectedProtocol);

				/* enhanced security selected ? */

				if (nego->SelectedProtocol)
				{
					if ((nego->SelectedProtocol == PROTOCOL_RDSAAD) &&
					    (!nego->EnabledProtocols[PROTOCOL_RDSAAD]))
					{
						nego_set_state(nego, NEGO_STATE_FAIL);
					}
					if ((nego->SelectedProtocol == PROTOCOL_HYBRID) &&
					    (!nego->EnabledProtocols[PROTOCOL_HYBRID]))
					{
						nego_set_state(nego, NEGO_STATE_FAIL);
					}

					if ((nego->SelectedProtocol == PROTOCOL_SSL) &&
					    (!nego->EnabledProtocols[PROTOCOL_SSL]))
					{
						nego_set_state(nego, NEGO_STATE_FAIL);
					}
				}
				else if (!nego->EnabledProtocols[PROTOCOL_RDP])
				{
					nego_set_state(nego, NEGO_STATE_FAIL);
				}

				break;

			case TYPE_RDP_NEG_FAILURE:
				if (!nego_process_negotiation_failure(nego, s))
					return -1;
				break;
		}
	}
	else if (li == 6)
	{
		WLog_DBG(TAG, "no rdpNegData");

		if (!nego->EnabledProtocols[PROTOCOL_RDP])
			nego_set_state(nego, NEGO_STATE_FAIL);
		else
			nego_set_state(nego, NEGO_STATE_FINAL);
	}
	else
	{
		WLog_ERR(TAG, "invalid negotiation response");
		nego_set_state(nego, NEGO_STATE_FAIL);
	}

	if (!tpkt_ensure_stream_consumed(s, length))
		return -1;
	return 0;
}

/**
 * Read optional routing token or cookie of X.224 Connection Request PDU.
 * msdn{cc240470}
 */

static BOOL nego_read_request_token_or_cookie(rdpNego* nego, wStream* s)
{
	/* routingToken and cookie are optional and mutually exclusive!
	 *
	 * routingToken (variable): An optional and variable-length routing
	 * token (used for load balancing) terminated by a 0x0D0A two-byte
	 * sequence: (check [MSFT-SDLBTS] for details!)
	 * Cookie:[space]msts=[ip address].[port].[reserved][\x0D\x0A]
	 * tsv://MS Terminal Services Plugin.1.[\x0D\x0A]
	 *
	 * cookie (variable): An optional and variable-length ANSI character
	 * string terminated by a 0x0D0A two-byte sequence:
	 * Cookie:[space]mstshash=[ANSISTRING][\x0D\x0A]
	 */
	UINT16 crlf = 0;
	size_t len;
	BOOL result = FALSE;
	BOOL isToken = FALSE;
	size_t remain = Stream_GetRemainingLength(s);

	WINPR_ASSERT(nego);

	const char* str = Stream_ConstPointer(s);
	const size_t pos = Stream_GetPosition(s);

	/* minimum length for token is 15 */
	if (remain < 15)
		return TRUE;

	if (memcmp(Stream_ConstPointer(s), "Cookie: mstshash=", 17) != 0)
	{
		if (memcmp(Stream_ConstPointer(s), "Cookie: msts=", 13) != 0)
		{
			if (memcmp(Stream_ConstPointer(s), "tsv:", 4) != 0)
			{
				/* remaining bytes are neither a token nor a cookie */
				return TRUE;
			}
		}
		isToken = TRUE;
	}
	else
	{
		/* not a token, minimum length for cookie is 19 */
		if (remain < 19)
			return TRUE;

		Stream_Seek(s, 17);
	}

	while (Stream_GetRemainingLength(s) >= 2)
	{
		Stream_Read_UINT16(s, crlf);

		if (crlf == 0x0A0D)
			break;

		Stream_Rewind(s, 1);
	}

	if (crlf == 0x0A0D)
	{
		Stream_Rewind(s, 2);
		len = Stream_GetPosition(s) - pos;
		Stream_Write_UINT16(s, 0);

		if (strnlen(str, len) == len)
		{
			if (isToken)
				result = nego_set_routing_token(nego, str, len);
			else
				result = nego_set_cookie(nego, str);
		}
	}

	if (!result)
	{
		Stream_SetPosition(s, pos);
		WLog_ERR(TAG, "invalid %s received", isToken ? "routing token" : "cookie");
	}
	else
	{
		WLog_DBG(TAG, "received %s [%s]", isToken ? "routing token" : "cookie", str);
	}

	return result;
}

/**
 * Read protocol security negotiation request message.
 *
 * @param nego A pointer to the NEGO struct
 * @param s A stream to read from
 *
 * @return \b TRUE for success, \b FALSE for failure
 */

BOOL nego_read_request(rdpNego* nego, wStream* s)
{
	BYTE li;
	BYTE type;
	UINT16 length;

	WINPR_ASSERT(nego);
	WINPR_ASSERT(s);

	if (!tpkt_read_header(s, &length))
		return FALSE;

	if (!tpdu_read_connection_request(s, &li, length))
		return FALSE;

	if (li != Stream_GetRemainingLength(s) + 6)
	{
		WLog_ERR(TAG, "Incorrect TPDU length indicator.");
		return FALSE;
	}

	if (!nego_read_request_token_or_cookie(nego, s))
	{
		WLog_ERR(TAG, "Failed to parse routing token or cookie.");
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) >= 8)
	{
		/* rdpNegData (optional) */
		Stream_Read_UINT8(s, type); /* Type */

		if (type != TYPE_RDP_NEG_REQ)
		{
			WLog_ERR(TAG, "Incorrect negotiation request type %" PRIu8 "", type);
			return FALSE;
		}

		if (!nego_process_negotiation_request(nego, s))
			return FALSE;
	}

	return tpkt_ensure_stream_consumed(s, length);
}

/**
 * Send protocol security negotiation message.
 *
 * @param nego A pointer to the NEGO struct
 */

void nego_send(rdpNego* nego)
{
	WINPR_ASSERT(nego);

	switch (nego_get_state(nego))
	{
		case NEGO_STATE_AAD:
			nego_attempt_rdsaad(nego);
			break;
		case NEGO_STATE_RDSTLS:
			nego_attempt_rdstls(nego);
			break;
		case NEGO_STATE_EXT:
			nego_attempt_ext(nego);
			break;
		case NEGO_STATE_NLA:
			nego_attempt_nla(nego);
			break;
		case NEGO_STATE_TLS:
			nego_attempt_tls(nego);
			break;
		case NEGO_STATE_RDP:
			nego_attempt_rdp(nego);
			break;
		default:
			WLog_ERR(TAG, "invalid negotiation state for sending");
			break;
	}
}

/**
 * Send RDP Negotiation Request (RDP_NEG_REQ).
 * msdn{cc240500}
 * msdn{cc240470}
 *
 * @param nego A pointer to the NEGO struct
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_send_negotiation_request(rdpNego* nego)
{
	BOOL rc = FALSE;
	wStream* s;
	size_t length;
	size_t bm, em;
	BYTE flags = 0;
	size_t cookie_length;
	s = Stream_New(NULL, 512);

	WINPR_ASSERT(nego);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	length = TPDU_CONNECTION_REQUEST_LENGTH;
	bm = Stream_GetPosition(s);
	Stream_Seek(s, length);

	if (nego->RoutingToken)
	{
		Stream_Write(s, nego->RoutingToken, nego->RoutingTokenLength);

		/* Ensure Routing Token is correctly terminated - may already be present in string */

		if ((nego->RoutingTokenLength > 2) &&
		    (nego->RoutingToken[nego->RoutingTokenLength - 2] == 0x0D) &&
		    (nego->RoutingToken[nego->RoutingTokenLength - 1] == 0x0A))
		{
			WLog_DBG(TAG, "Routing token looks correctly terminated - use verbatim");
			length += nego->RoutingTokenLength;
		}
		else
		{
			WLog_DBG(TAG, "Adding terminating CRLF to routing token");
			Stream_Write_UINT8(s, 0x0D); /* CR */
			Stream_Write_UINT8(s, 0x0A); /* LF */
			length += nego->RoutingTokenLength + 2;
		}
	}
	else if (nego->cookie)
	{
		cookie_length = strlen(nego->cookie);

		if (cookie_length > nego->CookieMaxLength)
			cookie_length = nego->CookieMaxLength;

		Stream_Write(s, "Cookie: mstshash=", 17);
		Stream_Write(s, (BYTE*)nego->cookie, cookie_length);
		Stream_Write_UINT8(s, 0x0D); /* CR */
		Stream_Write_UINT8(s, 0x0A); /* LF */
		length += cookie_length + 19;
	}

	WLog_DBG(TAG, "RequestedProtocols: %" PRIu32 "", nego->RequestedProtocols);

	if ((nego->RequestedProtocols > PROTOCOL_RDP) || (nego->sendNegoData))
	{
		/* RDP_NEG_DATA must be present for TLS and NLA */
		if (nego->RestrictedAdminModeRequired)
			flags |= RESTRICTED_ADMIN_MODE_REQUIRED;

		if (nego->RemoteCredsGuardRequired)
			flags |= REDIRECTED_AUTHENTICATION_MODE_REQUIRED;

		Stream_Write_UINT8(s, TYPE_RDP_NEG_REQ);
		Stream_Write_UINT8(s, flags);
		Stream_Write_UINT16(s, 8);                        /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, nego->RequestedProtocols); /* requestedProtocols */
		length += 8;
	}

	if (length > UINT16_MAX)
		goto fail;

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	if (!tpkt_write_header(s, (UINT16)length))
		goto fail;
	if (!tpdu_write_connection_request(s, (UINT16)length - 5))
		goto fail;
	Stream_SetPosition(s, em);
	Stream_SealLength(s);
	rc = (transport_write(nego->transport, s) >= 0);
fail:
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL nego_process_correlation_info(rdpNego* nego, wStream* s)
{
	UINT8 type, flags, x;
	UINT16 length;
	BYTE correlationId[16] = { 0 };

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 36))
	{
		WLog_ERR(TAG, "RDP_NEG_REQ::flags CORRELATION_INFO_PRESENT but data is missing");
		return FALSE;
	}

	Stream_Read_UINT8(s, type);
	if (type != TYPE_RDP_CORRELATION_INFO)
	{
		WLog_ERR(TAG, "(RDP_NEG_CORRELATION_INFO::type != TYPE_RDP_CORRELATION_INFO");
		return FALSE;
	}
	Stream_Read_UINT8(s, flags);
	if (flags != 0)
	{
		WLog_ERR(TAG, "(RDP_NEG_CORRELATION_INFO::flags != 0");
		return FALSE;
	}
	Stream_Read_UINT16(s, length);
	if (length != 36)
	{
		WLog_ERR(TAG, "(RDP_NEG_CORRELATION_INFO::length != 36");
		return FALSE;
	}

	Stream_Read(s, correlationId, sizeof(correlationId));
	if ((correlationId[0] == 0x00) || (correlationId[0] == 0xF4))
	{
		WLog_ERR(TAG, "(RDP_NEG_CORRELATION_INFO::correlationId[0] has invalid value 0x%02" PRIx8,
		         correlationId[0]);
		return FALSE;
	}
	for (x = 0; x < ARRAYSIZE(correlationId); x++)
	{
		if (correlationId[x] == 0x0D)
		{
			WLog_ERR(TAG,
			         "(RDP_NEG_CORRELATION_INFO::correlationId[%" PRIu8
			         "] has invalid value 0x%02" PRIx8,
			         x, correlationId[x]);
			return FALSE;
		}
	}
	Stream_Seek(s, 16); /* skip reserved bytes */

	WLog_INFO(TAG,
	          "RDP_NEG_CORRELATION_INFO::correlationId = { %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8
	          ", %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8
	          ", %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8 ", %02" PRIx8
	          ", %02" PRIx8 " }",
	          correlationId[0], correlationId[1], correlationId[2], correlationId[3],
	          correlationId[4], correlationId[5], correlationId[6], correlationId[7],
	          correlationId[8], correlationId[9], correlationId[10], correlationId[11],
	          correlationId[12], correlationId[13], correlationId[14], correlationId[15]);
	return TRUE;
}

BOOL nego_process_negotiation_request(rdpNego* nego, wStream* s)
{
	BYTE flags;
	UINT16 length;

	WINPR_ASSERT(nego);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 7))
		return FALSE;
	Stream_Read_UINT8(s, flags);
	if ((flags & ~(RESTRICTED_ADMIN_MODE_REQUIRED | REDIRECTED_AUTHENTICATION_MODE_REQUIRED |
	               CORRELATION_INFO_PRESENT)) != 0)
	{
		WLog_ERR(TAG, "RDP_NEG_REQ::flags invalid value 0x%02" PRIx8, flags);
		return FALSE;
	}
	if (flags & RESTRICTED_ADMIN_MODE_REQUIRED)
		WLog_INFO(TAG, "RDP_NEG_REQ::flags RESTRICTED_ADMIN_MODE_REQUIRED");

	if (flags & REDIRECTED_AUTHENTICATION_MODE_REQUIRED)
	{
		if (!nego->RemoteCredsGuardSupported)
		{
			WLog_ERR(TAG,
			         "RDP_NEG_REQ::flags REDIRECTED_AUTHENTICATION_MODE_REQUIRED but disabled");
			return FALSE;
		}
		else
		{
			WLog_INFO(TAG, "RDP_NEG_REQ::flags REDIRECTED_AUTHENTICATION_MODE_REQUIRED");
		}
		nego->RemoteCredsGuardActive = TRUE;
	}

	Stream_Read_UINT16(s, length);
	if (length != 8)
	{
		WLog_ERR(TAG, "RDP_NEG_REQ::length != 8");
		return FALSE;
	}
	Stream_Read_UINT32(s, nego->RequestedProtocols);

	if (flags & CORRELATION_INFO_PRESENT)
	{
		if (!nego_process_correlation_info(nego, s))
			return FALSE;
	}

	WLog_DBG(TAG, "RDP_NEG_REQ: RequestedProtocol: 0x%08" PRIX32 "", nego->RequestedProtocols);
	nego_set_state(nego, NEGO_STATE_FINAL);
	return TRUE;
}

static const char* nego_rdp_neg_rsp_flags_str(UINT32 flags)
{
	static char buffer[1024] = { 0 };

	_snprintf(buffer, ARRAYSIZE(buffer), "[0x%02" PRIx32 "] ", flags);
	if (flags & EXTENDED_CLIENT_DATA_SUPPORTED)
		winpr_str_append("EXTENDED_CLIENT_DATA_SUPPORTED", buffer, sizeof(buffer), "|");
	if (flags & DYNVC_GFX_PROTOCOL_SUPPORTED)
		winpr_str_append("DYNVC_GFX_PROTOCOL_SUPPORTED", buffer, sizeof(buffer), "|");
	if (flags & RDP_NEGRSP_RESERVED)
		winpr_str_append("RDP_NEGRSP_RESERVED", buffer, sizeof(buffer), "|");
	if (flags & RESTRICTED_ADMIN_MODE_SUPPORTED)
		winpr_str_append("RESTRICTED_ADMIN_MODE_SUPPORTED", buffer, sizeof(buffer), "|");
	if (flags & REDIRECTED_AUTHENTICATION_MODE_SUPPORTED)
		winpr_str_append("REDIRECTED_AUTHENTICATION_MODE_SUPPORTED", buffer, sizeof(buffer), "|");
	if ((flags &
	     ~(EXTENDED_CLIENT_DATA_SUPPORTED | DYNVC_GFX_PROTOCOL_SUPPORTED | RDP_NEGRSP_RESERVED |
	       RESTRICTED_ADMIN_MODE_SUPPORTED | REDIRECTED_AUTHENTICATION_MODE_SUPPORTED)))
		winpr_str_append("UNKNOWN", buffer, sizeof(buffer), "|");

	return buffer;
}

BOOL nego_process_negotiation_response(rdpNego* nego, wStream* s)
{
	UINT16 length;

	WINPR_ASSERT(nego);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 7))
	{
		nego_set_state(nego, NEGO_STATE_FAIL);
		return FALSE;
	}

	Stream_Read_UINT8(s, nego->flags);
	WLog_DBG(TAG, "RDP_NEG_RSP::flags = { %s }", nego_rdp_neg_rsp_flags_str(nego->flags));

	Stream_Read_UINT16(s, length);
	if (length != 8)
	{
		WLog_ERR(TAG, "RDP_NEG_RSP::length != 8");
		nego_set_state(nego, NEGO_STATE_FAIL);
		return FALSE;
	}
	UINT32 SelectedProtocol;
	Stream_Read_UINT32(s, SelectedProtocol);

	if (!nego_set_selected_protocol(nego, SelectedProtocol))
		return FALSE;
	return nego_set_state(nego, NEGO_STATE_FINAL);
}

/**
 * Process Negotiation Failure from Connection Confirm message.
 * @param nego A pointer to the NEGO struct
 * @param s The stream to read from
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_process_negotiation_failure(rdpNego* nego, wStream* s)
{
	BYTE flags;
	UINT16 length;
	UINT32 failureCode;

	WINPR_ASSERT(nego);
	WINPR_ASSERT(s);

	WLog_DBG(TAG, "RDP_NEG_FAILURE");
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 7))
		return FALSE;

	Stream_Read_UINT8(s, flags);
	if (flags != 0)
	{
		WLog_WARN(TAG, "RDP_NEG_FAILURE::flags = 0x%02" PRIx8, flags);
		return FALSE;
	}
	Stream_Read_UINT16(s, length);
	if (length != 8)
	{
		WLog_ERR(TAG, "RDP_NEG_FAILURE::length != 8");
		return FALSE;
	}
	Stream_Read_UINT32(s, failureCode);

	switch (failureCode)
	{
		case SSL_REQUIRED_BY_SERVER:
			WLog_WARN(TAG, "Error: SSL_REQUIRED_BY_SERVER");
			break;

		case SSL_NOT_ALLOWED_BY_SERVER:
			WLog_WARN(TAG, "Error: SSL_NOT_ALLOWED_BY_SERVER");
			nego->sendNegoData = TRUE;
			break;

		case SSL_CERT_NOT_ON_SERVER:
			WLog_ERR(TAG, "Error: SSL_CERT_NOT_ON_SERVER");
			nego->sendNegoData = TRUE;
			break;

		case INCONSISTENT_FLAGS:
			WLog_ERR(TAG, "Error: INCONSISTENT_FLAGS");
			break;

		case HYBRID_REQUIRED_BY_SERVER:
			WLog_WARN(TAG, "Error: HYBRID_REQUIRED_BY_SERVER");
			break;

		default:
			WLog_ERR(TAG, "Error: Unknown protocol security error %" PRIu32 "", failureCode);
			break;
	}

	nego_set_state(nego, NEGO_STATE_FAIL);
	return TRUE;
}

/**
 * Send RDP Negotiation Response (RDP_NEG_RSP).
 * @param nego A pointer to the NEGO struct
 */

BOOL nego_send_negotiation_response(rdpNego* nego)
{
	UINT16 length;
	size_t bm, em;
	BOOL status;
	wStream* s;
	BYTE flags;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(nego);
	context = transport_get_context(nego->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	s = Stream_New(NULL, 512);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	length = TPDU_CONNECTION_CONFIRM_LENGTH;
	bm = Stream_GetPosition(s);
	Stream_Seek(s, length);

	if (nego->SelectedProtocol & PROTOCOL_FAILED_NEGO)
	{
		UINT32 errorCode = (nego->SelectedProtocol & ~PROTOCOL_FAILED_NEGO);
		flags = 0;
		Stream_Write_UINT8(s, TYPE_RDP_NEG_FAILURE);
		Stream_Write_UINT8(s, flags); /* flags */
		Stream_Write_UINT16(s, 8);    /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, errorCode);
		length += 8;
	}
	else
	{
		flags = EXTENDED_CLIENT_DATA_SUPPORTED;

		if (freerdp_settings_get_bool(settings, FreeRDP_SupportGraphicsPipeline))
			flags |= DYNVC_GFX_PROTOCOL_SUPPORTED;

		if (freerdp_settings_get_bool(settings, FreeRDP_RestrictedAdminModeRequired))
			flags |= RESTRICTED_ADMIN_MODE_SUPPORTED;

		if (nego->RemoteCredsGuardSupported)
			flags |= REDIRECTED_AUTHENTICATION_MODE_SUPPORTED;

		/* RDP_NEG_DATA must be present for TLS, NLA, RDP and RDSTLS */
		Stream_Write_UINT8(s, TYPE_RDP_NEG_RSP);
		Stream_Write_UINT8(s, flags);                   /* flags */
		Stream_Write_UINT16(s, 8);                      /* RDP_NEG_DATA length (8) */
		Stream_Write_UINT32(s, nego->SelectedProtocol); /* selectedProtocol */
		length += 8;
	}

	em = Stream_GetPosition(s);
	Stream_SetPosition(s, bm);
	status = tpkt_write_header(s, length);
	if (status)
	{
		tpdu_write_connection_confirm(s, length - 5);
		Stream_SetPosition(s, em);
		Stream_SealLength(s);

		status = (transport_write(nego->transport, s) >= 0);
	}
	Stream_Free(s, TRUE);

	if (status)
	{
		/* update settings with negotiated protocol security */
		if (!freerdp_settings_set_uint32(settings, FreeRDP_RequestedProtocols,
		                                 nego->RequestedProtocols))
			return FALSE;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_SelectedProtocol,
		                                 nego->SelectedProtocol))
			return FALSE;

		switch (nego->SelectedProtocol)
		{
			case PROTOCOL_RDP:
				if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, TRUE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, TRUE))
					return FALSE;

				if (freerdp_settings_get_uint32(settings, FreeRDP_EncryptionLevel) ==
				    ENCRYPTION_LEVEL_NONE)
				{
					/**
					 * If the server implementation did not explicitely set a
					 * encryption level we default to client compatible
					 */
					if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel,
					                                 ENCRYPTION_LEVEL_CLIENT_COMPATIBLE))
						return FALSE;
				}

				if (freerdp_settings_get_bool(settings, FreeRDP_LocalConnection))
				{
					/**
					 * Note: This hack was firstly introduced in commit 95f5e115 to
					 * disable the unnecessary encryption with peers connecting to
					 * 127.0.0.1 or local unix sockets.
					 * This also affects connections via port tunnels! (e.g. ssh -L)
					 */
					WLog_INFO(TAG,
					          "Turning off encryption for local peer with standard rdp security");
					if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, FALSE))
						return FALSE;
					if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel,
					                                 ENCRYPTION_LEVEL_NONE))
						return FALSE;
				}
				else if (!freerdp_settings_get_pointer(settings, FreeRDP_RdpServerRsaKey))
				{
					WLog_ERR(TAG, "Missing server certificate");
					return FALSE;
				}
				break;
			case PROTOCOL_SSL:
				if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdstlsSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, FALSE))
					return FALSE;

				if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel,
				                                 ENCRYPTION_LEVEL_NONE))
					return FALSE;
				break;
			case PROTOCOL_HYBRID:
				if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, TRUE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdstlsSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, FALSE))
					return FALSE;

				if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel,
				                                 ENCRYPTION_LEVEL_NONE))
					return FALSE;
				break;
			case PROTOCOL_RDSTLS:
				if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdstlsSecurity, TRUE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, FALSE))
					return FALSE;
				if (!freerdp_settings_set_bool(settings, FreeRDP_UseRdpSecurityLayer, FALSE))
					return FALSE;

				if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel,
				                                 ENCRYPTION_LEVEL_NONE))
					return FALSE;
				break;
			default:
				break;
		}
	}

	return status;
}

/**
 * Initialize NEGO state machine.
 * @param nego A pointer to the NEGO struct
 */

void nego_init(rdpNego* nego)
{
	WINPR_ASSERT(nego);
	nego_set_state(nego, NEGO_STATE_INITIAL);
	nego->RequestedProtocols = PROTOCOL_RDP;
	nego->CookieMaxLength = DEFAULT_COOKIE_MAX_LENGTH;
	nego->sendNegoData = FALSE;
	nego->flags = 0;
}

/**
 * Create a new NEGO state machine instance.
 *
 * @param transport The transport to use
 *
 * @return A pointer to the allocated NEGO instance or NULL
 */

rdpNego* nego_new(rdpTransport* transport)
{
	rdpNego* nego = (rdpNego*)calloc(1, sizeof(rdpNego));

	if (!nego)
		return NULL;

	nego->transport = transport;
	nego_init(nego);
	return nego;
}

/**
 * Free NEGO state machine.
 * @param nego A pointer to the NEGO struct
 */

void nego_free(rdpNego* nego)
{
	if (nego)
	{
		free(nego->RoutingToken);
		free(nego->cookie);
		free(nego);
	}
}

/**
 * Set target hostname and port.
 * @param nego A pointer to the NEGO struct
 * @param hostname The hostname to set
 * @param port The port to set
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_set_target(rdpNego* nego, const char* hostname, UINT16 port)
{
	if (!nego || !hostname)
		return FALSE;

	nego->hostname = hostname;
	nego->port = port;
	return TRUE;
}

/**
 * Enable security layer negotiation.
 * @param nego A pointer to the NEGO struct pointer to the negotiation structure
 * @param NegotiateSecurityLayer whether to enable security layer negotiation (TRUE for enabled,
 * FALSE for disabled)
 */

void nego_set_negotiation_enabled(rdpNego* nego, BOOL NegotiateSecurityLayer)
{
	WLog_DBG(TAG, "Enabling security layer negotiation: %s",
	         NegotiateSecurityLayer ? "TRUE" : "FALSE");
	nego->NegotiateSecurityLayer = NegotiateSecurityLayer;
}

/**
 * Enable restricted admin mode.
 * @param nego A pointer to the NEGO struct pointer to the negotiation structure
 * @param RestrictedAdminModeRequired whether to enable security layer negotiation (TRUE for
 * enabled, FALSE for disabled)
 */

void nego_set_restricted_admin_mode_required(rdpNego* nego, BOOL RestrictedAdminModeRequired)
{
	WLog_DBG(TAG, "Enabling restricted admin mode: %s",
	         RestrictedAdminModeRequired ? "TRUE" : "FALSE");
	nego->RestrictedAdminModeRequired = RestrictedAdminModeRequired;
}

void nego_set_RCG_required(rdpNego* nego, BOOL enabled)
{
	WINPR_ASSERT(nego);

	WLog_DBG(TAG, "Enabling remoteCredentialGuards: %s", enabled ? "TRUE" : "FALSE");
	nego->RemoteCredsGuardRequired = enabled;
}

void nego_set_RCG_supported(rdpNego* nego, BOOL enabled)
{
	WINPR_ASSERT(nego);

	nego->RemoteCredsGuardSupported = enabled;
}

BOOL nego_get_remoteCredentialGuard(rdpNego* nego)
{
	WINPR_ASSERT(nego);

	return nego->RemoteCredsGuardActive;
}

void nego_set_childsession_enabled(rdpNego* nego, BOOL ChildSessionEnabled)
{
	WINPR_ASSERT(nego);
	nego->ConnectChildSession = ChildSessionEnabled;
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
 * @param nego A pointer to the NEGO struct pointer to the negotiation structure
 * @param enable_rdp whether to enable normal RDP protocol (TRUE for enabled, FALSE for disabled)
 */

void nego_enable_rdp(rdpNego* nego, BOOL enable_rdp)
{
	WLog_DBG(TAG, "Enabling RDP security: %s", enable_rdp ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_RDP] = enable_rdp;
}

/**
 * Enable TLS security protocol.
 * @param nego A pointer to the NEGO struct pointer to the negotiation structure
 * @param enable_tls whether to enable TLS + RDP protocol (TRUE for enabled, FALSE for disabled)
 */

void nego_enable_tls(rdpNego* nego, BOOL enable_tls)
{
	WLog_DBG(TAG, "Enabling TLS security: %s", enable_tls ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_SSL] = enable_tls;
}

/**
 * Enable NLA security protocol.
 * @param nego A pointer to the NEGO struct pointer to the negotiation structure
 * @param enable_nla whether to enable network level authentication protocol (TRUE for enabled,
 * FALSE for disabled)
 */

void nego_enable_nla(rdpNego* nego, BOOL enable_nla)
{
	WLog_DBG(TAG, "Enabling NLA security: %s", enable_nla ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_HYBRID] = enable_nla;
}

/**
 * Enable RDSTLS security protocol.
 * @param nego A pointer to the NEGO struct pointer to the negotiation structure
 * @param enable_rdstls whether to enable RDSTLS protocol (TRUE for enabled,
 * FALSE for disabled)
 */

void nego_enable_rdstls(rdpNego* nego, BOOL enable_rdstls)
{
	WLog_DBG(TAG, "Enabling RDSTLS security: %s", enable_rdstls ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_RDSTLS] = enable_rdstls;
}

/**
 * Enable NLA extended security protocol.
 * @param nego A pointer to the NEGO struct pointer to the negotiation structure
 * @param enable_ext whether to enable network level authentication extended protocol (TRUE for
 * enabled, FALSE for disabled)
 */

void nego_enable_ext(rdpNego* nego, BOOL enable_ext)
{
	WLog_DBG(TAG, "Enabling NLA extended security: %s", enable_ext ? "TRUE" : "FALSE");
	nego->EnabledProtocols[PROTOCOL_HYBRID_EX] = enable_ext;
}

/**
 * Enable RDS AAD security protocol.
 * @param nego A pointer to the NEGO struct pointer to the negotiation structure
 * @param enable_aad whether to enable RDS AAD Auth protocol (TRUE for
 * enabled, FALSE for disabled)
 */

void nego_enable_aad(rdpNego* nego, BOOL enable_aad)
{
	if (aad_is_supported())
	{
		WLog_DBG(TAG, "Enabling RDS AAD security: %s", enable_aad ? "TRUE" : "FALSE");
		nego->EnabledProtocols[PROTOCOL_RDSAAD] = enable_aad;
	}
	else
	{
		WLog_WARN(TAG, "This build does not support AAD security, disabling.");
	}
}

/**
 * Set routing token.
 * @param nego A pointer to the NEGO struct
 * @param RoutingToken A pointer to the routing token
 * @param RoutingTokenLength The lenght of the routing token
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_set_routing_token(rdpNego* nego, const void* RoutingToken, DWORD RoutingTokenLength)
{
	if (RoutingTokenLength == 0)
		return FALSE;

	free(nego->RoutingToken);
	nego->RoutingTokenLength = RoutingTokenLength;
	nego->RoutingToken = (BYTE*)malloc(nego->RoutingTokenLength);

	if (!nego->RoutingToken)
		return FALSE;

	CopyMemory(nego->RoutingToken, RoutingToken, nego->RoutingTokenLength);
	return TRUE;
}

/**
 * Set cookie.
 * @param nego A pointer to the NEGO struct
 * @param cookie A pointer to the cookie string
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nego_set_cookie(rdpNego* nego, const char* cookie)
{
	if (nego->cookie)
	{
		free(nego->cookie);
		nego->cookie = NULL;
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
 * @param nego A pointer to the NEGO struct
 * @param CookieMaxLength the length to set
 */

void nego_set_cookie_max_length(rdpNego* nego, UINT32 CookieMaxLength)
{
	nego->CookieMaxLength = CookieMaxLength;
}

/**
 * Enable / disable preconnection PDU.
 * @param nego A pointer to the NEGO struct
 * @param SendPreconnectionPdu The value to set
 */

void nego_set_send_preconnection_pdu(rdpNego* nego, BOOL SendPreconnectionPdu)
{
	nego->SendPreconnectionPdu = SendPreconnectionPdu;
}

/**
 * Set preconnection id.
 * @param nego A pointer to the NEGO struct
 * @param PreconnectionId the ID to set
 */

void nego_set_preconnection_id(rdpNego* nego, UINT32 PreconnectionId)
{
	nego->PreconnectionId = PreconnectionId;
}

/**
 * Set preconnection blob.
 * @param nego A pointer to the NEGO struct
 * @param PreconnectionBlob A pointer to the blob to use
 */

void nego_set_preconnection_blob(rdpNego* nego, const char* PreconnectionBlob)
{
	nego->PreconnectionBlob = PreconnectionBlob;
}

UINT32 nego_get_selected_protocol(rdpNego* nego)
{
	if (!nego)
		return 0;

	return nego->SelectedProtocol;
}

BOOL nego_set_selected_protocol(rdpNego* nego, UINT32 SelectedProtocol)
{
	WINPR_ASSERT(nego);
	nego->SelectedProtocol = SelectedProtocol;
	return TRUE;
}

UINT32 nego_get_requested_protocols(rdpNego* nego)
{
	if (!nego)
		return 0;

	return nego->RequestedProtocols;
}

BOOL nego_set_requested_protocols(rdpNego* nego, UINT32 RequestedProtocols)
{
	if (!nego)
		return FALSE;

	nego->RequestedProtocols = RequestedProtocols;
	return TRUE;
}

NEGO_STATE nego_get_state(rdpNego* nego)
{
	if (!nego)
		return NEGO_STATE_FAIL;

	return nego->state;
}

BOOL nego_set_state(rdpNego* nego, NEGO_STATE state)
{
	if (!nego)
		return FALSE;

	nego->state = state;
	return TRUE;
}

SEC_WINNT_AUTH_IDENTITY* nego_get_identity(rdpNego* nego)
{
	rdpNla* nla;
	if (!nego)
		return NULL;

	nla = transport_get_nla(nego->transport);
	return nla_get_identity(nla);
}

void nego_free_nla(rdpNego* nego)
{
	if (!nego || !nego->transport)
		return;

	transport_set_nla(nego->transport, NULL);
}

const BYTE* nego_get_routing_token(rdpNego* nego, DWORD* RoutingTokenLength)
{
	if (!nego)
		return NULL;
	if (RoutingTokenLength)
		*RoutingTokenLength = nego->RoutingTokenLength;
	return nego->RoutingToken;
}
