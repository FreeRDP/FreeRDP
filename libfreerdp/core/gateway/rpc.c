/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC over HTTP
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/dsparse.h>

#include <freerdp/log.h>

#include <openssl/bio.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "http.h"
#include "ntlm.h"
#include "ncacn_http.h"
#include "rpc_bind.h"
#include "rpc_fault.h"
#include "rpc_client.h"

#include "rpc.h"

#define TAG FREERDP_TAG("core.gateway.rpc")

/* Security Verification Trailer Signature */

rpc_sec_verification_trailer RPC_SEC_VERIFICATION_TRAILER =
{ { 0x8a, 0xe3, 0x13, 0x71, 0x02, 0xf4, 0x36, 0x71 } };

static char* PTYPE_STRINGS[] =
{
	"PTYPE_REQUEST",
	"PTYPE_PING",
	"PTYPE_RESPONSE",
	"PTYPE_FAULT",
	"PTYPE_WORKING",
	"PTYPE_NOCALL",
	"PTYPE_REJECT",
	"PTYPE_ACK",
	"PTYPE_CL_CANCEL",
	"PTYPE_FACK",
	"PTYPE_CANCEL_ACK",
	"PTYPE_BIND",
	"PTYPE_BIND_ACK",
	"PTYPE_BIND_NAK",
	"PTYPE_ALTER_CONTEXT",
	"PTYPE_ALTER_CONTEXT_RESP",
	"PTYPE_RPC_AUTH_3",
	"PTYPE_SHUTDOWN",
	"PTYPE_CO_CANCEL",
	"PTYPE_ORPHANED",
	"PTYPE_RTS",
	""
};

const RPC_SECURITY_PROVIDER_INFO RPC_SECURITY_PROVIDER_INFO_TABLE[] =
{
	{ RPC_C_AUTHN_NONE, TRUE, -1 },
	{ RPC_C_AUTHN_GSS_NEGOTIATE, TRUE, -1 },
	{ RPC_C_AUTHN_WINNT, FALSE, 3 },
	{ RPC_C_AUTHN_GSS_SCHANNEL, TRUE, -1 },
	{ RPC_C_AUTHN_GSS_KERBEROS, TRUE, -1 },
	{ RPC_C_AUTHN_DEFAULT, -1, -1 },
	{ 0, -1, -1 }
};

/**
 * [MS-RPCH]: Remote Procedure Call over HTTP Protocol Specification:
 * http://msdn.microsoft.com/en-us/library/cc243950/
 */

/**
 *                                      Connection Establishment\n
 *
 *     Client                  Outbound Proxy           Inbound Proxy                 Server\n
 *        |                         |                         |                         |\n
 *        |-----------------IN Channel Request--------------->|                         |\n
 *        |---OUT Channel Request-->|                         |<-Legacy Server Response-|\n
 *        |                         |<--------------Legacy Server Response--------------|\n
 *        |                         |                         |                         |\n
 *        |---------CONN_A1-------->|                         |                         |\n
 *        |----------------------CONN_B1--------------------->|                         |\n
 *        |                         |----------------------CONN_A2--------------------->|\n
 *        |                         |                         |                         |\n
 *        |<--OUT Channel Response--|                         |---------CONN_B2-------->|\n
 *        |<--------CONN_A3---------|                         |                         |\n
 *        |                         |<---------------------CONN_C1----------------------|\n
 *        |                         |                         |<--------CONN_B3---------|\n
 *        |<--------CONN_C2---------|                         |                         |\n
 *        |                         |                         |                         |\n
 *
 */

void rpc_pdu_header_print(rpcconn_hdr_t* header)
{
	WLog_INFO(TAG,  "rpc_vers: %d", header->common.rpc_vers);
	WLog_INFO(TAG,  "rpc_vers_minor: %d", header->common.rpc_vers_minor);

	if (header->common.ptype > PTYPE_RTS)
		WLog_INFO(TAG,  "ptype: %s (%d)", "PTYPE_UNKNOWN", header->common.ptype);
	else
		WLog_INFO(TAG,  "ptype: %s (%d)", PTYPE_STRINGS[header->common.ptype], header->common.ptype);

	WLog_INFO(TAG,  "pfc_flags (0x%02X) = {", header->common.pfc_flags);

	if (header->common.pfc_flags & PFC_FIRST_FRAG)
		WLog_INFO(TAG,  " PFC_FIRST_FRAG");

	if (header->common.pfc_flags & PFC_LAST_FRAG)
		WLog_INFO(TAG,  " PFC_LAST_FRAG");

	if (header->common.pfc_flags & PFC_PENDING_CANCEL)
		WLog_INFO(TAG,  " PFC_PENDING_CANCEL");

	if (header->common.pfc_flags & PFC_RESERVED_1)
		WLog_INFO(TAG,  " PFC_RESERVED_1");

	if (header->common.pfc_flags & PFC_CONC_MPX)
		WLog_INFO(TAG,  " PFC_CONC_MPX");

	if (header->common.pfc_flags & PFC_DID_NOT_EXECUTE)
		WLog_INFO(TAG,  " PFC_DID_NOT_EXECUTE");

	if (header->common.pfc_flags & PFC_OBJECT_UUID)
		WLog_INFO(TAG,  " PFC_OBJECT_UUID");

	WLog_INFO(TAG,  " }");
	WLog_INFO(TAG,  "packed_drep[4]: %02X %02X %02X %02X",
			 header->common.packed_drep[0], header->common.packed_drep[1],
			 header->common.packed_drep[2], header->common.packed_drep[3]);
	WLog_INFO(TAG,  "frag_length: %d", header->common.frag_length);
	WLog_INFO(TAG,  "auth_length: %d", header->common.auth_length);
	WLog_INFO(TAG,  "call_id: %d", header->common.call_id);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		WLog_INFO(TAG,  "alloc_hint: %d", header->response.alloc_hint);
		WLog_INFO(TAG,  "p_cont_id: %d", header->response.p_cont_id);
		WLog_INFO(TAG,  "cancel_count: %d", header->response.cancel_count);
		WLog_INFO(TAG,  "reserved: %d", header->response.reserved);
	}
}

void rpc_pdu_header_init(rdpRpc* rpc, rpcconn_hdr_t* header)
{
	header->common.rpc_vers = rpc->rpc_vers;
	header->common.rpc_vers_minor = rpc->rpc_vers_minor;
	header->common.packed_drep[0] = rpc->packed_drep[0];
	header->common.packed_drep[1] = rpc->packed_drep[1];
	header->common.packed_drep[2] = rpc->packed_drep[2];
	header->common.packed_drep[3] = rpc->packed_drep[3];
}

UINT32 rpc_offset_align(UINT32* offset, UINT32 alignment)
{
	UINT32 pad;
	pad = *offset;
	*offset = (*offset + alignment - 1) & ~(alignment - 1);
	pad = *offset - pad;
	return pad;
}

UINT32 rpc_offset_pad(UINT32* offset, UINT32 pad)
{
	*offset += pad;
	return pad;
}

/**
 * PDU Segments:
 *  ________________________________
 * |                                |
 * |           PDU Header           |
 * |________________________________|
 * |                                |
 * |                                |
 * |            PDU Body            |
 * |                                |
 * |________________________________|
 * |                                |
 * |        Security Trailer        |
 * |________________________________|
 * |                                |
 * |      Authentication Token      |
 * |________________________________|
 */

/**
 * PDU Structure with verification trailer
 *
 * MUST only appear in a request PDU!
 *  ________________________________
 * |                                |
 * |           PDU Header           |
 * |________________________________| _______
 * |                                |   /|\
 * |                                |    |
 * |           Stub Data            |    |
 * |                                |    |
 * |________________________________|    |
 * |                                | PDU Body
 * |            Stub Pad            |    |
 * |________________________________|    |
 * |                                |    |
 * |      Verification Trailer      |    |
 * |________________________________|    |
 * |                                |    |
 * |       Authentication Pad       |    |
 * |________________________________| __\|/__
 * |                                |
 * |        Security Trailer        |
 * |________________________________|
 * |                                |
 * |      Authentication Token      |
 * |________________________________|
 *
 */

/**
 * Security Trailer:
 *
 * The sec_trailer structure MUST be placed at the end of the PDU, including past stub data,
 * when present. The sec_trailer structure MUST be 4-byte aligned with respect to the beginning
 * of the PDU. Padding octets MUST be used to align the sec_trailer structure if its natural
 * beginning is not already 4-byte aligned.
 *
 * All PDUs that carry sec_trailer information share certain common fields:
 * frag_length and auth_length. The beginning of the sec_trailer structure for each PDU MUST be
 * calculated to start from offset (frag_length – auth_length – 8) from the beginning of the PDU.
 *
 * Immediately after the sec_trailer structure, there MUST be a BLOB carrying the authentication
 * information produced by the security provider. This BLOB is called the authentication token and
 * MUST be of size auth_length. The size MUST also be equal to the length from the first octet
 * immediately after the sec_trailer structure all the way to the end of the fragment;
 * the two values MUST be the same.
 *
 * A client or a server that (during composing of a PDU) has allocated more space for the
 * authentication token than the security provider fills in SHOULD fill in the rest of
 * the allocated space with zero octets. These zero octets are still considered to belong
 * to the authentication token part of the PDU.
 *
 */

BOOL rpc_get_stub_data_info(rdpRpc* rpc, BYTE* buffer, UINT32* offset, UINT32* length)
{
	UINT32 alloc_hint = 0;
	rpcconn_hdr_t* header;
	UINT32 frag_length;
	UINT32 auth_length;
	UINT32 auth_pad_length;
	UINT32 sec_trailer_offset;
	rpc_sec_trailer* sec_trailer;

	*offset = RPC_COMMON_FIELDS_LENGTH;
	header = ((rpcconn_hdr_t*) buffer);

	switch (header->common.ptype)
	{
		case PTYPE_RESPONSE:
			*offset += 8;
			rpc_offset_align(offset, 8);
			alloc_hint = header->response.alloc_hint;
			break;

		case PTYPE_REQUEST:
			*offset += 4;
			rpc_offset_align(offset, 8);
			alloc_hint = header->request.alloc_hint;
			break;

		case PTYPE_RTS:
			*offset += 4;
			break;

		default:
			WLog_ERR(TAG, "Unknown PTYPE: 0x%04X", header->common.ptype);
			return FALSE;
	}

	if (!length)
		return TRUE;

	if (header->common.ptype == PTYPE_REQUEST)
	{
		UINT32 sec_trailer_offset;
		sec_trailer_offset = header->common.frag_length - header->common.auth_length - 8;
		*length = sec_trailer_offset - *offset;
		return TRUE;
	}

	frag_length = header->common.frag_length;
	auth_length = header->common.auth_length;
	sec_trailer_offset = frag_length - auth_length - 8;
	sec_trailer = (rpc_sec_trailer*) &buffer[sec_trailer_offset];
	auth_pad_length = sec_trailer->auth_pad_length;

#if 0
	WLog_DBG(TAG, "sec_trailer: type: %d level: %d pad_length: %d reserved: %d context_id: %d",
			sec_trailer->auth_type, sec_trailer->auth_level,
			sec_trailer->auth_pad_length, sec_trailer->auth_reserved,
			sec_trailer->auth_context_id);
#endif

	/**
	 * According to [MS-RPCE], auth_pad_length is the number of padding
	 * octets used to 4-byte align the security trailer, but in practice
	 * we get values up to 15, which indicates 16-byte alignment.
	 */

	if ((frag_length - (sec_trailer_offset + 8)) != auth_length)
	{
		WLog_ERR(TAG, "invalid auth_length: actual: %d, expected: %d", auth_length,
				 (frag_length - (sec_trailer_offset + 8)));
	}

	*length = frag_length - auth_length - 24 - 8 - auth_pad_length;
	return TRUE;
}

int rpc_out_channel_read(RpcOutChannel* outChannel, BYTE* data, int length)
{
	int status;

	status = BIO_read(outChannel->tls->bio, data, length);

	if (status > 0)
	{
#ifdef HAVE_VALGRIND_MEMCHECK_H
		VALGRIND_MAKE_MEM_DEFINED(data, status);
#endif
		return status;
	}

	if (BIO_should_retry(outChannel->tls->bio))
		return 0;

	return -1;
}

int rpc_in_channel_write(RpcInChannel* inChannel, const BYTE* data, int length)
{
	int status;

	status = tls_write_all(inChannel->tls, data, length);

	return status;
}

int rpc_out_channel_write(RpcOutChannel* outChannel, const BYTE* data, int length)
{
	int status;

	status = tls_write_all(outChannel->tls, data, length);

	return status;
}

int rpc_in_channel_transition_to_state(RpcInChannel* inChannel, CLIENT_IN_CHANNEL_STATE state)
{
	int status = 1;
	const char* str = "CLIENT_IN_CHANNEL_STATE_UNKNOWN";

	switch (state)
	{
		case CLIENT_IN_CHANNEL_STATE_INITIAL:
			str = "CLIENT_IN_CHANNEL_STATE_INITIAL";
			break;

		case CLIENT_IN_CHANNEL_STATE_CONNECTED:
			str = "CLIENT_IN_CHANNEL_STATE_CONNECTED";
			break;

		case CLIENT_IN_CHANNEL_STATE_SECURITY:
			str = "CLIENT_IN_CHANNEL_STATE_SECURITY";
			break;

		case CLIENT_IN_CHANNEL_STATE_NEGOTIATED:
			str = "CLIENT_IN_CHANNEL_STATE_NEGOTIATED";
			break;

		case CLIENT_IN_CHANNEL_STATE_OPENED:
			str = "CLIENT_IN_CHANNEL_STATE_OPENED";
			break;

		case CLIENT_IN_CHANNEL_STATE_OPENED_A4W:
			str = "CLIENT_IN_CHANNEL_STATE_OPENED_A4W";
			break;

		case CLIENT_IN_CHANNEL_STATE_FINAL:
			str = "CLIENT_IN_CHANNEL_STATE_FINAL";
			break;
	}

	inChannel->State = state;
	WLog_DBG(TAG, "%s", str);

	return status;
}

int rpc_in_channel_rpch_init(rdpRpc* rpc, RpcInChannel* inChannel)
{
	HttpContext* http;

	inChannel->ntlm = ntlm_new();

	if (!inChannel->ntlm)
		return -1;

	inChannel->http = http_context_new();

	if (!inChannel->http)
		return -1;

	http = inChannel->http;

	http_context_set_method(http, "RPC_IN_DATA");

	http_context_set_uri(http, "/rpc/rpcproxy.dll?localhost:3388");
	http_context_set_accept(http, "application/rpc");
	http_context_set_cache_control(http, "no-cache");
	http_context_set_connection(http, "Keep-Alive");
	http_context_set_user_agent(http, "MSRPC");
	http_context_set_host(http, rpc->settings->GatewayHostname);

	http_context_set_pragma(http, "ResourceTypeUuid=44e265dd-7daf-42cd-8560-3cdb6e7a2729");

	return 1;
}

int rpc_in_channel_init(rdpRpc* rpc, RpcInChannel* inChannel)
{
	rts_generate_cookie((BYTE*) &inChannel->Cookie);

	inChannel->rpc = rpc;
	inChannel->State = CLIENT_IN_CHANNEL_STATE_INITIAL;
	inChannel->BytesSent = 0;
	inChannel->SenderAvailableWindow = rpc->ReceiveWindow;
	inChannel->PingOriginator.ConnectionTimeout = 30;
	inChannel->PingOriginator.KeepAliveInterval = 0;

	if (rpc_in_channel_rpch_init(rpc, inChannel) < 0)
		return -1;

	return 1;
}

void rpc_in_channel_rpch_uninit(RpcInChannel* inChannel)
{
	if (inChannel->ntlm)
	{
		ntlm_free(inChannel->ntlm);
		inChannel->ntlm = NULL;
	}

	if (inChannel->http)
	{
		http_context_free(inChannel->http);
		inChannel->http = NULL;
	}
}

RpcInChannel* rpc_in_channel_new(rdpRpc* rpc)
{
	RpcInChannel* inChannel = NULL;

	inChannel = (RpcInChannel*) calloc(1, sizeof(RpcInChannel));

	if (inChannel)
	{
		rpc_in_channel_init(rpc, inChannel);
	}

	return inChannel;
}

void rpc_in_channel_free(RpcInChannel* inChannel)
{
	if (!inChannel)
		return;

	rpc_in_channel_rpch_uninit(inChannel);

	if (inChannel->tls)
	{
		tls_free(inChannel->tls);
		inChannel->tls = NULL;
	}

	free(inChannel);
}

int rpc_out_channel_transition_to_state(RpcOutChannel* outChannel, CLIENT_OUT_CHANNEL_STATE state)
{
	int status = 1;
	const char* str = "CLIENT_OUT_CHANNEL_STATE_UNKNOWN";

	switch (state)
	{
		case CLIENT_OUT_CHANNEL_STATE_INITIAL:
			str = "CLIENT_OUT_CHANNEL_STATE_INITIAL";
			break;

		case CLIENT_OUT_CHANNEL_STATE_CONNECTED:
			str = "CLIENT_OUT_CHANNEL_STATE_CONNECTED";
			break;

		case CLIENT_OUT_CHANNEL_STATE_SECURITY:
			str = "CLIENT_OUT_CHANNEL_STATE_SECURITY";
			break;

		case CLIENT_OUT_CHANNEL_STATE_NEGOTIATED:
			str = "CLIENT_OUT_CHANNEL_STATE_NEGOTIATED";
			break;

		case CLIENT_OUT_CHANNEL_STATE_OPENED:
			str = "CLIENT_OUT_CHANNEL_STATE_OPENED";
			break;

		case CLIENT_OUT_CHANNEL_STATE_OPENED_A6W:
			str = "CLIENT_OUT_CHANNEL_STATE_OPENED_A6W";
			break;

		case CLIENT_OUT_CHANNEL_STATE_OPENED_A10W:
			str = "CLIENT_OUT_CHANNEL_STATE_OPENED_A10W";
			break;

		case CLIENT_OUT_CHANNEL_STATE_OPENED_B3W:
			str = "CLIENT_OUT_CHANNEL_STATE_OPENED_B3W";
			break;

		case CLIENT_OUT_CHANNEL_STATE_RECYCLED:
			str = "CLIENT_OUT_CHANNEL_STATE_RECYCLED";
			break;

		case CLIENT_OUT_CHANNEL_STATE_FINAL:
			str = "CLIENT_OUT_CHANNEL_STATE_FINAL";
			break;
	}

	outChannel->State = state;
	WLog_DBG(TAG, "%s", str);

	return status;
}

int rpc_out_channel_rpch_init(rdpRpc* rpc, RpcOutChannel* outChannel)
{
	HttpContext* http;

	outChannel->ntlm = ntlm_new();

	if (!outChannel->ntlm)
		return -1;

	outChannel->http = http_context_new();

	if (!outChannel->http)
		return -1;

	http = outChannel->http;

	http_context_set_method(http, "RPC_OUT_DATA");

	http_context_set_uri(http, "/rpc/rpcproxy.dll?localhost:3388");
	http_context_set_accept(http, "application/rpc");
	http_context_set_cache_control(http, "no-cache");
	http_context_set_connection(http, "Keep-Alive");
	http_context_set_user_agent(http, "MSRPC");
	http_context_set_host(http, rpc->settings->GatewayHostname);

	http_context_set_pragma(http,
			"ResourceTypeUuid=44e265dd-7daf-42cd-8560-3cdb6e7a2729, "
			"SessionId=fbd9c34f-397d-471d-a109-1b08cc554624");

	return 1;
}

int rpc_out_channel_init(rdpRpc* rpc, RpcOutChannel* outChannel)
{
	rts_generate_cookie((BYTE*) &outChannel->Cookie);

	outChannel->rpc = rpc;
	outChannel->State = CLIENT_OUT_CHANNEL_STATE_INITIAL;
	outChannel->BytesReceived = 0;
	outChannel->ReceiverAvailableWindow = rpc->ReceiveWindow;
	outChannel->ReceiveWindow = rpc->ReceiveWindow;
	outChannel->ReceiveWindowSize = rpc->ReceiveWindow;
	outChannel->AvailableWindowAdvertised = rpc->ReceiveWindow;

	if (rpc_out_channel_rpch_init(rpc, outChannel) < 0)
		return -1;

	return 1;
}

void rpc_out_channel_rpch_uninit(RpcOutChannel* outChannel)
{
	if (outChannel->ntlm)
	{
		ntlm_free(outChannel->ntlm);
		outChannel->ntlm = NULL;
	}

	if (outChannel->http)
	{
		http_context_free(outChannel->http);
		outChannel->http = NULL;
	}
}

RpcOutChannel* rpc_out_channel_new(rdpRpc* rpc)
{
	RpcOutChannel* outChannel = NULL;

	outChannel = (RpcOutChannel*) calloc(1, sizeof(RpcOutChannel));

	if (outChannel)
	{
		rpc_out_channel_init(rpc, outChannel);
	}

	return outChannel;
}

void rpc_out_channel_free(RpcOutChannel* outChannel)
{
	if (!outChannel)
		return;

	rpc_out_channel_rpch_uninit(outChannel);

	if (outChannel->tls)
	{
		tls_free(outChannel->tls);
		outChannel->tls = NULL;
	}

	free(outChannel);
}

int rpc_virtual_connection_transition_to_state(rdpRpc* rpc,
		RpcVirtualConnection* connection, VIRTUAL_CONNECTION_STATE state)
{
	int status = 1;
	const char* str = "VIRTUAL_CONNECTION_STATE_UNKNOWN";

	switch (state)
	{
		case VIRTUAL_CONNECTION_STATE_INITIAL:
			str = "VIRTUAL_CONNECTION_STATE_INITIAL";
			break;

		case VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT:
			str = "VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT";
			break;

		case VIRTUAL_CONNECTION_STATE_WAIT_A3W:
			str = "VIRTUAL_CONNECTION_STATE_WAIT_A3W";
			break;

		case VIRTUAL_CONNECTION_STATE_WAIT_C2:
			str = "VIRTUAL_CONNECTION_STATE_WAIT_C2";
			break;

		case VIRTUAL_CONNECTION_STATE_OPENED:
			str = "VIRTUAL_CONNECTION_STATE_OPENED";
			break;

		case VIRTUAL_CONNECTION_STATE_FINAL:
			str = "VIRTUAL_CONNECTION_STATE_FINAL";
			break;
	}

	connection->State = state;
	WLog_DBG(TAG, "%s", str);

	return status;
}

RpcVirtualConnection* rpc_virtual_connection_new(rdpRpc* rpc)
{
	RpcVirtualConnection* connection;

	connection = (RpcVirtualConnection*) calloc(1, sizeof(RpcVirtualConnection));

	if (!connection)
		return NULL;

	rts_generate_cookie((BYTE*) &(connection->Cookie));
	rts_generate_cookie((BYTE*) &(connection->AssociationGroupId));

	connection->State = VIRTUAL_CONNECTION_STATE_INITIAL;

	connection->DefaultInChannel = rpc_in_channel_new(rpc);

	if (!connection->DefaultInChannel)
		goto out_free;

	connection->DefaultOutChannel = rpc_out_channel_new(rpc);

	if (!connection->DefaultOutChannel)
		goto out_default_in;

	return connection;
out_default_in:
	free(connection->DefaultInChannel);
out_free:
	free(connection);
	return NULL;
}

void rpc_virtual_connection_free(RpcVirtualConnection* connection)
{
	if (!connection)
		return;

	rpc_in_channel_free(connection->DefaultInChannel);
	rpc_in_channel_free(connection->NonDefaultInChannel);

	rpc_out_channel_free(connection->DefaultOutChannel);
	rpc_out_channel_free(connection->NonDefaultOutChannel);

	free(connection);
}

int rpc_channel_tls_connect(RpcChannel* channel, int timeout)
{
	int sockfd;
	rdpTls* tls;
	int tlsStatus;
	BIO* socketBio;
	BIO* bufferedBio;
	rdpRpc* rpc = channel->rpc;
	rdpContext* context = rpc->context;
	rdpSettings* settings = context->settings;

	sockfd = freerdp_tcp_connect(context, settings, settings->GatewayHostname,
					settings->GatewayPort, timeout);

	if (sockfd < 1)
		return -1;

	socketBio = BIO_new(BIO_s_simple_socket());

	if (!socketBio)
		return FALSE;

	BIO_set_fd(socketBio, sockfd, BIO_CLOSE);

	bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
		return FALSE;

	bufferedBio = BIO_push(bufferedBio, socketBio);

	if (!BIO_set_nonblock(bufferedBio, TRUE))
		return -1;

	channel->bio = bufferedBio;

	tls = channel->tls = tls_new(settings);

	if (!tls)
		return -1;

	tls->hostname = settings->GatewayHostname;
	tls->port = settings->GatewayPort;
	tls->isGatewayTransport = TRUE;

	tlsStatus = tls_connect(tls, bufferedBio);

	if (tlsStatus < 1)
	{
		if (tlsStatus < 0)
		{
			if (!freerdp_get_last_error(context))
				freerdp_set_last_error(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		}
		else
		{
			if (!freerdp_get_last_error(context))
				freerdp_set_last_error(context, FREERDP_ERROR_CONNECT_CANCELLED);
		}

		return -1;
	}

	return 1;
}

int rpc_in_channel_connect(RpcInChannel* inChannel, int timeout)
{
	rdpRpc* rpc = inChannel->rpc;

	/* Connect IN Channel */

	if (rpc_channel_tls_connect((RpcChannel*) inChannel, timeout) < 0)
		return -1;

	rpc_in_channel_transition_to_state(inChannel, CLIENT_IN_CHANNEL_STATE_CONNECTED);

	if (rpc_ncacn_http_ntlm_init(rpc, (RpcChannel*) inChannel) < 0)
		return -1;

	/* Send IN Channel Request */

	if (rpc_ncacn_http_send_in_channel_request(rpc, inChannel) < 0)
	{
		WLog_ERR(TAG, "rpc_ncacn_http_send_in_channel_request failure");
		return -1;
	}

	rpc_in_channel_transition_to_state(inChannel, CLIENT_IN_CHANNEL_STATE_SECURITY);

	return 1;
}

int rpc_out_channel_connect(RpcOutChannel* outChannel, int timeout)
{
	rdpRpc* rpc = outChannel->rpc;

	/* Connect OUT Channel */

	if (rpc_channel_tls_connect((RpcChannel*) outChannel, timeout) < 0)
		return -1;

	rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_CONNECTED);

	if (rpc_ncacn_http_ntlm_init(rpc, (RpcChannel*) outChannel) < 0)
		return FALSE;

	/* Send OUT Channel Request */

	if (rpc_ncacn_http_send_out_channel_request(rpc, outChannel, FALSE) < 0)
	{
		WLog_ERR(TAG, "rpc_ncacn_http_send_out_channel_request failure");
		return FALSE;
	}

	rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_SECURITY);

	return 1;
}

int rpc_out_channel_replacement_connect(RpcOutChannel* outChannel, int timeout)
{
	rdpRpc* rpc = outChannel->rpc;

	/* Connect OUT Channel */

	if (rpc_channel_tls_connect((RpcChannel*) outChannel, timeout) < 0)
		return -1;

	rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_CONNECTED);

	if (rpc_ncacn_http_ntlm_init(rpc, (RpcChannel*) outChannel) < 0)
		return FALSE;

	/* Send OUT Channel Request */

	if (rpc_ncacn_http_send_out_channel_request(rpc, outChannel, TRUE) < 0)
	{
		WLog_ERR(TAG, "rpc_ncacn_http_send_out_channel_request failure");
		return FALSE;
	}

	rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_SECURITY);

	return 1;
}

BOOL rpc_connect(rdpRpc* rpc, int timeout)
{
	RpcInChannel* inChannel;
	RpcOutChannel* outChannel;
	RpcVirtualConnection* connection;

	rpc->VirtualConnection = rpc_virtual_connection_new(rpc);

	if (!rpc->VirtualConnection)
		return FALSE;

	connection = rpc->VirtualConnection;
	inChannel = connection->DefaultInChannel;
	outChannel = connection->DefaultOutChannel;

	rpc_virtual_connection_transition_to_state(rpc, connection, VIRTUAL_CONNECTION_STATE_INITIAL);

	if (rpc_in_channel_connect(inChannel, timeout) < 0)
		return FALSE;

	if (rpc_out_channel_connect(outChannel, timeout) < 0)
		return FALSE;

	return TRUE;
}

rdpRpc* rpc_new(rdpTransport* transport)
{
	rdpRpc* rpc = (rdpRpc*) calloc(1, sizeof(rdpRpc));

	if (!rpc)
		return NULL;

	rpc->State = RPC_CLIENT_STATE_INITIAL;

	rpc->transport = transport;
	rpc->settings = transport->settings;
	rpc->context = transport->context;

	rpc->SendSeqNum = 0;

	rpc->ntlm = ntlm_new();

	if (!rpc->ntlm)
		goto out_free;

	rpc->PipeCallId = 0;
	rpc->StubCallId = 0;
	rpc->StubFragCount = 0;
	rpc->rpc_vers = 5;
	rpc->rpc_vers_minor = 0;

	/* little-endian data representation */
	rpc->packed_drep[0] = 0x10;
	rpc->packed_drep[1] = 0x00;
	rpc->packed_drep[2] = 0x00;
	rpc->packed_drep[3] = 0x00;
	rpc->max_xmit_frag = 0x0FF8;
	rpc->max_recv_frag = 0x0FF8;

	rpc->ReceiveWindow = 0x00010000;
	rpc->ChannelLifetime = 0x40000000;
	rpc->KeepAliveInterval = 300000;
	rpc->CurrentKeepAliveInterval = rpc->KeepAliveInterval;
	rpc->CurrentKeepAliveTime = 0;

	rpc->CallId = 2;

	if (rpc_client_new(rpc) < 0)
		goto out_free_rpc_client;

	return rpc;
out_free_rpc_client:
	rpc_client_free(rpc);
out_free:
	free(rpc);
	return NULL;
}

void rpc_free(rdpRpc* rpc)
{
	if (rpc)
	{
		rpc_client_free(rpc);

		if (rpc->ntlm)
		{
			ntlm_client_uninit(rpc->ntlm);
			ntlm_free(rpc->ntlm);
			rpc->ntlm = NULL;
		}

		if (rpc->VirtualConnection)
		{
			rpc_virtual_connection_free(rpc->VirtualConnection);
			rpc->VirtualConnection = NULL;
		}

		free(rpc);
	}
}
