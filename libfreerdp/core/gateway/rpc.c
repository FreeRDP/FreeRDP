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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/dsparse.h>
#include <winpr/crypto.h>

#include <freerdp/log.h>

#include <openssl/bio.h>

#ifdef HAVE_VALGRIND_MEMCHECK_H
#include <valgrind/memcheck.h>
#endif

#include "../proxy.h"
#include "http.h"
#include "ntlm.h"
#include "ncacn_http.h"
#include "rpc_bind.h"
#include "rpc_fault.h"
#include "rpc_client.h"

#include "rpc.h"
#include "rts.h"

#define TAG FREERDP_TAG("core.gateway.rpc")

static const char* PTYPE_STRINGS[] = { "PTYPE_REQUEST",       "PTYPE_PING",
	                                   "PTYPE_RESPONSE",      "PTYPE_FAULT",
	                                   "PTYPE_WORKING",       "PTYPE_NOCALL",
	                                   "PTYPE_REJECT",        "PTYPE_ACK",
	                                   "PTYPE_CL_CANCEL",     "PTYPE_FACK",
	                                   "PTYPE_CANCEL_ACK",    "PTYPE_BIND",
	                                   "PTYPE_BIND_ACK",      "PTYPE_BIND_NAK",
	                                   "PTYPE_ALTER_CONTEXT", "PTYPE_ALTER_CONTEXT_RESP",
	                                   "PTYPE_RPC_AUTH_3",    "PTYPE_SHUTDOWN",
	                                   "PTYPE_CO_CANCEL",     "PTYPE_ORPHANED",
	                                   "PTYPE_RTS",           "" };

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

void rpc_pdu_header_print(const rpcconn_hdr_t* header)
{
	WINPR_ASSERT(header);

	WLog_INFO(TAG, "rpc_vers: %" PRIu8 "", header->common.rpc_vers);
	WLog_INFO(TAG, "rpc_vers_minor: %" PRIu8 "", header->common.rpc_vers_minor);

	if (header->common.ptype > PTYPE_RTS)
		WLog_INFO(TAG, "ptype: %s (%" PRIu8 ")", "PTYPE_UNKNOWN", header->common.ptype);
	else
		WLog_INFO(TAG, "ptype: %s (%" PRIu8 ")", PTYPE_STRINGS[header->common.ptype],
		          header->common.ptype);

	WLog_INFO(TAG, "pfc_flags (0x%02" PRIX8 ") = {", header->common.pfc_flags);

	if (header->common.pfc_flags & PFC_FIRST_FRAG)
		WLog_INFO(TAG, " PFC_FIRST_FRAG");

	if (header->common.pfc_flags & PFC_LAST_FRAG)
		WLog_INFO(TAG, " PFC_LAST_FRAG");

	if (header->common.pfc_flags & PFC_PENDING_CANCEL)
		WLog_INFO(TAG, " PFC_PENDING_CANCEL");

	if (header->common.pfc_flags & PFC_RESERVED_1)
		WLog_INFO(TAG, " PFC_RESERVED_1");

	if (header->common.pfc_flags & PFC_CONC_MPX)
		WLog_INFO(TAG, " PFC_CONC_MPX");

	if (header->common.pfc_flags & PFC_DID_NOT_EXECUTE)
		WLog_INFO(TAG, " PFC_DID_NOT_EXECUTE");

	if (header->common.pfc_flags & PFC_OBJECT_UUID)
		WLog_INFO(TAG, " PFC_OBJECT_UUID");

	WLog_INFO(TAG, " }");
	WLog_INFO(TAG, "packed_drep[4]: %02" PRIX8 " %02" PRIX8 " %02" PRIX8 " %02" PRIX8 "",
	          header->common.packed_drep[0], header->common.packed_drep[1],
	          header->common.packed_drep[2], header->common.packed_drep[3]);
	WLog_INFO(TAG, "frag_length: %" PRIu16 "", header->common.frag_length);
	WLog_INFO(TAG, "auth_length: %" PRIu16 "", header->common.auth_length);
	WLog_INFO(TAG, "call_id: %" PRIu32 "", header->common.call_id);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		WLog_INFO(TAG, "alloc_hint: %" PRIu32 "", header->response.alloc_hint);
		WLog_INFO(TAG, "p_cont_id: %" PRIu16 "", header->response.p_cont_id);
		WLog_INFO(TAG, "cancel_count: %" PRIu8 "", header->response.cancel_count);
		WLog_INFO(TAG, "reserved: %" PRIu8 "", header->response.reserved);
	}
}

rpcconn_common_hdr_t rpc_pdu_header_init(const rdpRpc* rpc)
{
	rpcconn_common_hdr_t header = { 0 };
	WINPR_ASSERT(rpc);

	header.rpc_vers = rpc->rpc_vers;
	header.rpc_vers_minor = rpc->rpc_vers_minor;
	header.packed_drep[0] = rpc->packed_drep[0];
	header.packed_drep[1] = rpc->packed_drep[1];
	header.packed_drep[2] = rpc->packed_drep[2];
	header.packed_drep[3] = rpc->packed_drep[3];
	return header;
}

size_t rpc_offset_align(size_t* offset, size_t alignment)
{
	size_t pad;
	pad = *offset;
	*offset = (*offset + alignment - 1) & ~(alignment - 1);
	pad = *offset - pad;
	return pad;
}

size_t rpc_offset_pad(size_t* offset, size_t pad)
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

BOOL rpc_get_stub_data_info(const rpcconn_hdr_t* header, size_t* poffset, size_t* length)
{
	size_t used = 0;
	size_t offset = 0;
	BOOL rc = FALSE;
	UINT32 frag_length;
	UINT32 auth_length;
	UINT32 auth_pad_length = 0;
	UINT32 sec_trailer_offset;
	const rpc_sec_trailer* sec_trailer = NULL;

	WINPR_ASSERT(header);
	WINPR_ASSERT(poffset);
	WINPR_ASSERT(length);

	offset = RPC_COMMON_FIELDS_LENGTH;

	switch (header->common.ptype)
	{
		case PTYPE_RESPONSE:
			offset += 8;
			rpc_offset_align(&offset, 8);
			sec_trailer = &header->response.auth_verifier;
			break;

		case PTYPE_REQUEST:
			offset += 4;
			rpc_offset_align(&offset, 8);
			sec_trailer = &header->request.auth_verifier;
			break;

		case PTYPE_RTS:
			offset += 4;
			break;

		default:
			WLog_ERR(TAG, "Unknown PTYPE: 0x%02" PRIX8 "", header->common.ptype);
			goto fail;
	}

	frag_length = header->common.frag_length;
	auth_length = header->common.auth_length;

	if (poffset)
		*poffset = offset;

	/* The fragment must be larger than the authentication trailer */
	used = offset + auth_length + 8ull;
	if (sec_trailer)
	{
		auth_pad_length = sec_trailer->auth_pad_length;
		used += sec_trailer->auth_pad_length;
	}

	if (frag_length < used)
		goto fail;

	if (!length)
		return TRUE;

	sec_trailer_offset = frag_length - auth_length - 8;

	/**
	 * According to [MS-RPCE], auth_pad_length is the number of padding
	 * octets used to 4-byte align the security trailer, but in practice
	 * we get values up to 15, which indicates 16-byte alignment.
	 */

	if ((frag_length - (sec_trailer_offset + 8)) != auth_length)
	{
		WLog_ERR(TAG, "invalid auth_length: actual: %" PRIu32 ", expected: %" PRIu32 "",
		         auth_length, (frag_length - (sec_trailer_offset + 8)));
	}

	*length = sec_trailer_offset - auth_pad_length - offset;

	rc = TRUE;
fail:
	return rc;
}

SSIZE_T rpc_channel_read(RpcChannel* channel, wStream* s, size_t length)
{
	int status;

	if (!channel || (length > INT32_MAX))
		return -1;

	ERR_clear_error();
	status = BIO_read(channel->tls->bio, Stream_Pointer(s), (INT32)length);

	if (status > 0)
	{
		Stream_Seek(s, (size_t)status);
		return status;
	}

	if (BIO_should_retry(channel->tls->bio))
		return 0;

	WLog_ERR(TAG, "rpc_channel_read: Out of retries");
	return -1;
}

SSIZE_T rpc_channel_write(RpcChannel* channel, const BYTE* data, size_t length)
{
	int status;

	if (!channel || (length > INT32_MAX))
		return -1;

	status = tls_write_all(channel->tls, data, (INT32)length);
	return status;
}

BOOL rpc_in_channel_transition_to_state(RpcInChannel* inChannel, CLIENT_IN_CHANNEL_STATE state)
{
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

	if (!inChannel)
		return FALSE;

	inChannel->State = state;
	WLog_DBG(TAG, "%s", str);
	return TRUE;
}

static int rpc_channel_rpch_init(RpcClient* client, RpcChannel* channel, const char* inout)
{
	HttpContext* http;
	rdpSettings* settings;

	if (!client || !channel || !inout || !client->context || !client->context->settings)
		return -1;

	settings = client->context->settings;
	channel->ntlm = ntlm_new();
	rts_generate_cookie((BYTE*)&channel->Cookie);
	channel->client = client;

	if (!channel->ntlm)
		return -1;

	channel->http = http_context_new();

	if (!channel->http)
		return -1;

	http = channel->http;

	if (!http_context_set_method(http, inout) ||
	    !http_context_set_uri(http, "/rpc/rpcproxy.dll?localhost:3388") ||
	    !http_context_set_accept(http, "application/rpc") ||
	    !http_context_set_cache_control(http, "no-cache") ||
	    !http_context_set_connection(http, "Keep-Alive") ||
	    !http_context_set_user_agent(http, "MSRPC") ||
	    !http_context_set_host(http, settings->GatewayHostname) ||
	    !http_context_set_pragma(http, "ResourceTypeUuid=44e265dd-7daf-42cd-8560-3cdb6e7a2729, "
	                                   "SessionId=fbd9c34f-397d-471d-a109-1b08cc554624"))
		return -1;

	return 1;
}

static int rpc_in_channel_init(rdpRpc* rpc, RpcInChannel* inChannel)
{
	inChannel->State = CLIENT_IN_CHANNEL_STATE_INITIAL;
	inChannel->BytesSent = 0;
	inChannel->SenderAvailableWindow = rpc->ReceiveWindow;
	inChannel->PingOriginator.ConnectionTimeout = 30;
	inChannel->PingOriginator.KeepAliveInterval = 0;

	if (rpc_channel_rpch_init(rpc->client, &inChannel->common, "RPC_IN_DATA") < 0)
		return -1;

	return 1;
}

static RpcInChannel* rpc_in_channel_new(rdpRpc* rpc)
{
	RpcInChannel* inChannel = NULL;
	inChannel = (RpcInChannel*)calloc(1, sizeof(RpcInChannel));

	if (inChannel)
	{
		rpc_in_channel_init(rpc, inChannel);
	}

	return inChannel;
}

void rpc_channel_free(RpcChannel* channel)
{
	if (!channel)
		return;

	ntlm_free(channel->ntlm);
	http_context_free(channel->http);
	tls_free(channel->tls);
	free(channel);
}

BOOL rpc_out_channel_transition_to_state(RpcOutChannel* outChannel, CLIENT_OUT_CHANNEL_STATE state)
{
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

	if (!outChannel)
		return FALSE;

	outChannel->State = state;
	WLog_DBG(TAG, "%s", str);
	return TRUE;
}

static int rpc_out_channel_init(rdpRpc* rpc, RpcOutChannel* outChannel)
{
	outChannel->State = CLIENT_OUT_CHANNEL_STATE_INITIAL;
	outChannel->BytesReceived = 0;
	outChannel->ReceiverAvailableWindow = rpc->ReceiveWindow;
	outChannel->ReceiveWindow = rpc->ReceiveWindow;
	outChannel->ReceiveWindowSize = rpc->ReceiveWindow;
	outChannel->AvailableWindowAdvertised = rpc->ReceiveWindow;

	if (rpc_channel_rpch_init(rpc->client, &outChannel->common, "RPC_OUT_DATA") < 0)
		return -1;

	return 1;
}

RpcOutChannel* rpc_out_channel_new(rdpRpc* rpc)
{
	RpcOutChannel* outChannel = NULL;
	outChannel = (RpcOutChannel*)calloc(1, sizeof(RpcOutChannel));

	if (outChannel)
	{
		rpc_out_channel_init(rpc, outChannel);
	}

	return outChannel;
}

BOOL rpc_virtual_connection_transition_to_state(rdpRpc* rpc, RpcVirtualConnection* connection,
                                                VIRTUAL_CONNECTION_STATE state)
{
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

	if (!connection)
		return FALSE;

	connection->State = state;
	WLog_DBG(TAG, "%s", str);
	return TRUE;
}

static RpcVirtualConnection* rpc_virtual_connection_new(rdpRpc* rpc)
{
	RpcVirtualConnection* connection;
	connection = (RpcVirtualConnection*)calloc(1, sizeof(RpcVirtualConnection));

	if (!connection)
		return NULL;

	rts_generate_cookie((BYTE*)&(connection->Cookie));
	rts_generate_cookie((BYTE*)&(connection->AssociationGroupId));
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

static void rpc_virtual_connection_free(RpcVirtualConnection* connection)
{
	if (!connection)
		return;

	rpc_channel_free(&connection->DefaultInChannel->common);
	rpc_channel_free(&connection->NonDefaultInChannel->common);
	rpc_channel_free(&connection->DefaultOutChannel->common);
	rpc_channel_free(&connection->NonDefaultOutChannel->common);
	free(connection);
}

static BOOL rpc_channel_tls_connect(RpcChannel* channel, UINT32 timeout)
{
	int sockfd;
	rdpTls* tls;
	int tlsStatus;
	BIO* socketBio;
	BIO* bufferedBio;
	rdpContext* context;
	rdpSettings* settings;
	const char* proxyUsername;
	const char* proxyPassword;

	if (!channel || !channel->client || !channel->client->context ||
	    !channel->client->context->settings)
		return FALSE;

	context = channel->client->context;
	settings = context->settings;
	proxyUsername = freerdp_settings_get_string(settings, FreeRDP_ProxyUsername);
	proxyPassword = freerdp_settings_get_string(settings, FreeRDP_ProxyPassword);
	{
		sockfd =
		    freerdp_tcp_connect(context, channel->client->host, channel->client->port, timeout);

		if (sockfd < 0)
			return FALSE;
	}
	socketBio = BIO_new(BIO_s_simple_socket());

	if (!socketBio)
	{
		closesocket(sockfd);
		return FALSE;
	}

	BIO_set_fd(socketBio, sockfd, BIO_CLOSE);
	bufferedBio = BIO_new(BIO_s_buffered_socket());

	if (!bufferedBio)
	{
		BIO_free_all(socketBio);
		return FALSE;
	}

	bufferedBio = BIO_push(bufferedBio, socketBio);

	if (!BIO_set_nonblock(bufferedBio, TRUE))
	{
		BIO_free_all(bufferedBio);
		return FALSE;
	}

	if (channel->client->isProxy)
	{
		if (!proxy_connect(settings, bufferedBio, proxyUsername, proxyPassword,
		                   settings->GatewayHostname, settings->GatewayPort))
		{
			BIO_free_all(bufferedBio);
			return FALSE;
		}
	}

	channel->bio = bufferedBio;
	tls = channel->tls = tls_new(settings);

	if (!tls)
		return FALSE;

	tls->hostname = settings->GatewayHostname;
	tls->port = settings->GatewayPort;
	tls->isGatewayTransport = TRUE;
	tlsStatus = tls_connect(tls, bufferedBio);

	if (tlsStatus < 1)
	{
		if (tlsStatus < 0)
		{
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_TLS_CONNECT_FAILED);
		}
		else
		{
			freerdp_set_last_error_if_not(context, FREERDP_ERROR_CONNECT_CANCELLED);
		}

		return FALSE;
	}

	return TRUE;
}

static int rpc_in_channel_connect(RpcInChannel* inChannel, UINT32 timeout)
{
	rdpContext* context;

	if (!inChannel || !inChannel->common.client || !inChannel->common.client->context)
		return -1;

	context = inChannel->common.client->context;

	/* Connect IN Channel */

	if (!rpc_channel_tls_connect(&inChannel->common, timeout))
		return -1;

	rpc_in_channel_transition_to_state(inChannel, CLIENT_IN_CHANNEL_STATE_CONNECTED);

	if (!rpc_ncacn_http_ntlm_init(context, &inChannel->common))
		return -1;

	/* Send IN Channel Request */

	if (!rpc_ncacn_http_send_in_channel_request(&inChannel->common))
	{
		WLog_ERR(TAG, "rpc_ncacn_http_send_in_channel_request failure");
		return -1;
	}

	if (!rpc_in_channel_transition_to_state(inChannel, CLIENT_IN_CHANNEL_STATE_SECURITY))
		return -1;

	return 1;
}

static int rpc_out_channel_connect(RpcOutChannel* outChannel, int timeout)
{
	rdpContext* context;

	if (!outChannel || !outChannel->common.client || !outChannel->common.client->context)
		return -1;

	context = outChannel->common.client->context;

	/* Connect OUT Channel */

	if (!rpc_channel_tls_connect(&outChannel->common, timeout))
		return -1;

	rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_CONNECTED);

	if (!rpc_ncacn_http_ntlm_init(context, &outChannel->common))
		return FALSE;

	/* Send OUT Channel Request */

	if (!rpc_ncacn_http_send_out_channel_request(&outChannel->common, FALSE))
	{
		WLog_ERR(TAG, "rpc_ncacn_http_send_out_channel_request failure");
		return FALSE;
	}

	rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_SECURITY);
	return 1;
}

int rpc_out_channel_replacement_connect(RpcOutChannel* outChannel, int timeout)
{
	rdpContext* context;

	if (!outChannel || !outChannel->common.client || !outChannel->common.client->context)
		return -1;

	context = outChannel->common.client->context;

	/* Connect OUT Channel */

	if (!rpc_channel_tls_connect(&outChannel->common, timeout))
		return -1;

	rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_CONNECTED);

	if (!rpc_ncacn_http_ntlm_init(context, (RpcChannel*)outChannel))
		return FALSE;

	/* Send OUT Channel Request */

	if (!rpc_ncacn_http_send_out_channel_request(&outChannel->common, TRUE))
	{
		WLog_ERR(TAG, "rpc_ncacn_http_send_out_channel_request failure");
		return FALSE;
	}

	rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_SECURITY);
	return 1;
}

BOOL rpc_connect(rdpRpc* rpc, UINT32 timeout)
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
	rdpContext* context = transport_get_context(transport);
	rdpRpc* rpc;

	WINPR_ASSERT(context);

	rpc = (rdpRpc*)calloc(1, sizeof(rdpRpc));

	if (!rpc)
		return NULL;

	rpc->State = RPC_CLIENT_STATE_INITIAL;
	rpc->transport = transport;
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
	rpc->client = rpc_client_new(context, rpc->max_recv_frag);

	if (!rpc->client)
		goto out_free;

	return rpc;
out_free:
	rpc_free(rpc);
	return NULL;
}

void rpc_free(rdpRpc* rpc)
{
	if (rpc)
	{
		rpc_client_free(rpc->client);
		ntlm_free(rpc->ntlm);
		rpc_virtual_connection_free(rpc->VirtualConnection);
		free(rpc);
	}
}
