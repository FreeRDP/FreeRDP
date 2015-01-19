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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/tchar.h>
#include <winpr/synch.h>
#include <winpr/dsparse.h>

#include <openssl/rand.h>

#include "http.h"
#include "ntlm.h"
#include "ncacn_http.h"
#include "rpc_bind.h"
#include "rpc_fault.h"
#include "rpc_client.h"

#include "rpc.h"

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

void rpc_pdu_header_print(rpcconn_hdr_t* header)
{
	fprintf(stderr, "rpc_vers: %d\n", header->common.rpc_vers);
	fprintf(stderr, "rpc_vers_minor: %d\n", header->common.rpc_vers_minor);

	if (header->common.ptype > PTYPE_RTS)
		fprintf(stderr, "ptype: %s (%d)\n", "PTYPE_UNKNOWN", header->common.ptype);
	else
		fprintf(stderr, "ptype: %s (%d)\n", PTYPE_STRINGS[header->common.ptype], header->common.ptype);

	fprintf(stderr, "pfc_flags (0x%02X) = {", header->common.pfc_flags);
	if (header->common.pfc_flags & PFC_FIRST_FRAG)
		fprintf(stderr, " PFC_FIRST_FRAG");
	if (header->common.pfc_flags & PFC_LAST_FRAG)
		fprintf(stderr, " PFC_LAST_FRAG");
	if (header->common.pfc_flags & PFC_PENDING_CANCEL)
		fprintf(stderr, " PFC_PENDING_CANCEL");
	if (header->common.pfc_flags & PFC_RESERVED_1)
		fprintf(stderr, " PFC_RESERVED_1");
	if (header->common.pfc_flags & PFC_CONC_MPX)
		fprintf(stderr, " PFC_CONC_MPX");
	if (header->common.pfc_flags & PFC_DID_NOT_EXECUTE)
		fprintf(stderr, " PFC_DID_NOT_EXECUTE");
	if (header->common.pfc_flags & PFC_OBJECT_UUID)
		fprintf(stderr, " PFC_OBJECT_UUID");
	fprintf(stderr, " }\n");

	fprintf(stderr, "packed_drep[4]: %02X %02X %02X %02X\n",
			header->common.packed_drep[0], header->common.packed_drep[1],
			header->common.packed_drep[2], header->common.packed_drep[3]);

	fprintf(stderr, "frag_length: %d\n", header->common.frag_length);
	fprintf(stderr, "auth_length: %d\n", header->common.auth_length);
	fprintf(stderr, "call_id: %d\n", header->common.call_id);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		fprintf(stderr, "alloc_hint: %d\n", header->response.alloc_hint);
		fprintf(stderr, "p_cont_id: %d\n", header->response.p_cont_id);
		fprintf(stderr, "cancel_count: %d\n", header->response.cancel_count);
		fprintf(stderr, "reserved: %d\n", header->response.reserved);
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

	*offset = RPC_COMMON_FIELDS_LENGTH;
	header = ((rpcconn_hdr_t*) buffer);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		*offset += 8;
		rpc_offset_align(offset, 8);
		alloc_hint = header->response.alloc_hint;
	}
	else if (header->common.ptype == PTYPE_REQUEST)
	{
		*offset += 4;
		rpc_offset_align(offset, 8);
		alloc_hint = header->request.alloc_hint;
	}
	else if (header->common.ptype == PTYPE_RTS)
	{
		*offset += 4;
	}
	else
	{
		return FALSE;
	}

	if (length)
	{
		if (header->common.ptype == PTYPE_REQUEST)
		{
			UINT32 sec_trailer_offset;

			sec_trailer_offset = header->common.frag_length - header->common.auth_length - 8;
			*length = sec_trailer_offset - *offset;
		}
		else
		{
			UINT32 frag_length;
			UINT32 auth_length;
			UINT32 auth_pad_length;
			UINT32 sec_trailer_offset;
			rpc_sec_trailer* sec_trailer;

			frag_length = header->common.frag_length;
			auth_length = header->common.auth_length;

			sec_trailer_offset = frag_length - auth_length - 8;
			sec_trailer = (rpc_sec_trailer*) &buffer[sec_trailer_offset];
			auth_pad_length = sec_trailer->auth_pad_length;

#if 0
			fprintf(stderr, "sec_trailer: type: %d level: %d pad_length: %d reserved: %d context_id: %d\n",
					sec_trailer->auth_type,
					sec_trailer->auth_level,
					sec_trailer->auth_pad_length,
					sec_trailer->auth_reserved,
					sec_trailer->auth_context_id);
#endif

			/**
			 * According to [MS-RPCE], auth_pad_length is the number of padding
			 * octets used to 4-byte align the security trailer, but in practice
			 * we get values up to 15, which indicates 16-byte alignment.
			 */

			if ((frag_length - (sec_trailer_offset + 8)) != auth_length)
			{
				fprintf(stderr, "invalid auth_length: actual: %d, expected: %d\n", auth_length,
						(frag_length - (sec_trailer_offset + 8)));
			}

			*length = frag_length - auth_length - 24 - 8 - auth_pad_length;
		}
	}

	return TRUE;
}

int rpc_out_read(rdpRpc* rpc, BYTE* data, int length)
{
	int status;

	status = tls_read(rpc->TlsOut, data, length);

	return status;
}

int rpc_out_write(rdpRpc* rpc, BYTE* data, int length)
{
	int status;

	status = tls_write_all(rpc->TlsOut, data, length);

	return status;
}

int rpc_in_write(rdpRpc* rpc, BYTE* data, int length)
{
	int status;

#ifdef WITH_DEBUG_TSG
	fprintf(stderr, "Sending PDU (length: %d)\n", length);
	rpc_pdu_header_print((rpcconn_hdr_t*) data);
	winpr_HexDump(data, length);
	fprintf(stderr, "\n");
#endif
	
	status = tls_write_all(rpc->TlsIn, data, length);

	return status;
}

int rpc_write(rdpRpc* rpc, BYTE* data, int length, UINT16 opnum)
{
	BYTE* buffer;
	UINT32 offset;
	rdpNtlm* ntlm;
	UINT32 stub_data_pad;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	RpcClientCall* clientCall;
	SECURITY_STATUS encrypt_status;
	rpcconn_request_hdr_t* request_pdu;

	ntlm = rpc->ntlm;

	if ((!ntlm) || (!ntlm->table))
	{
		fprintf(stderr, "rpc_write: invalid ntlm context\n");
		return -1;
	}

	if (ntlm->table->QueryContextAttributes(&ntlm->context, SECPKG_ATTR_SIZES, &ntlm->ContextSizes) != SEC_E_OK)
	{
		fprintf(stderr, "QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
		return -1;
	}

	request_pdu = (rpcconn_request_hdr_t*) malloc(sizeof(rpcconn_request_hdr_t));
	ZeroMemory(request_pdu, sizeof(rpcconn_request_hdr_t));

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*) request_pdu);

	request_pdu->ptype = PTYPE_REQUEST;
	request_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
	request_pdu->auth_length = (UINT16) ntlm->ContextSizes.cbMaxSignature;
	request_pdu->call_id = rpc->CallId++;
	request_pdu->alloc_hint = length;
	request_pdu->p_cont_id = 0x0000;
	request_pdu->opnum = opnum;

	clientCall = rpc_client_call_new(request_pdu->call_id, request_pdu->opnum);
	ArrayList_Add(rpc->client->ClientCallList, clientCall);

	if (request_pdu->opnum == TsProxySetupReceivePipeOpnum)
		rpc->PipeCallId = request_pdu->call_id;

	request_pdu->stub_data = data;

	offset = 24;
	stub_data_pad = 0;
	stub_data_pad = rpc_offset_align(&offset, 8);

	offset += length;
	request_pdu->auth_verifier.auth_pad_length = rpc_offset_align(&offset, 4);
	request_pdu->auth_verifier.auth_type = RPC_C_AUTHN_WINNT;
	request_pdu->auth_verifier.auth_level = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY;
	request_pdu->auth_verifier.auth_reserved = 0x00;
	request_pdu->auth_verifier.auth_context_id = 0x00000000;
	offset += (8 + request_pdu->auth_length);

	request_pdu->frag_length = offset;

	buffer = (BYTE*) malloc(request_pdu->frag_length);

	CopyMemory(buffer, request_pdu, 24);

	offset = 24;
	rpc_offset_pad(&offset, stub_data_pad);
	CopyMemory(&buffer[offset], request_pdu->stub_data, length);
	offset += length;

	rpc_offset_pad(&offset, request_pdu->auth_verifier.auth_pad_length);
	CopyMemory(&buffer[offset], &request_pdu->auth_verifier.auth_type, 8);
	offset += 8;

	Buffers[0].BufferType = SECBUFFER_DATA; /* auth_data */
	Buffers[1].BufferType = SECBUFFER_TOKEN; /* signature */

	Buffers[0].pvBuffer = buffer;
	Buffers[0].cbBuffer = offset;

	Buffers[1].cbBuffer = ntlm->ContextSizes.cbMaxSignature;
	Buffers[1].pvBuffer = malloc(Buffers[1].cbBuffer);
	ZeroMemory(Buffers[1].pvBuffer, Buffers[1].cbBuffer);

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;

	encrypt_status = ntlm->table->EncryptMessage(&ntlm->context, 0, &Message, rpc->SendSeqNum++);

	if (encrypt_status != SEC_E_OK)
	{
		fprintf(stderr, "EncryptMessage status: 0x%08X\n", encrypt_status);
		free(request_pdu);
		return -1;
	}

	CopyMemory(&buffer[offset], Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	offset += Buffers[1].cbBuffer;
	free(Buffers[1].pvBuffer);

	if (rpc_send_enqueue_pdu(rpc, buffer, request_pdu->frag_length) != 0)
		length = -1;

	free(request_pdu);

	return length;
}

BOOL rpc_connect(rdpRpc* rpc)
{
	rpc->TlsIn = rpc->transport->TlsIn;
	rpc->TlsOut = rpc->transport->TlsOut;

	if (!rts_connect(rpc))
	{
		fprintf(stderr, "rts_connect error!\n");
		return FALSE;
	}

	rpc->State = RPC_CLIENT_STATE_ESTABLISHED;

	if (rpc_secure_bind(rpc) != 0)
	{
		fprintf(stderr, "rpc_secure_bind error!\n");
		return FALSE;
	}

	return TRUE;
}

void rpc_client_virtual_connection_init(rdpRpc* rpc, RpcVirtualConnection* connection)
{
	connection->DefaultInChannel->State = CLIENT_IN_CHANNEL_STATE_INITIAL;
	connection->DefaultInChannel->BytesSent = 0;
	connection->DefaultInChannel->SenderAvailableWindow = rpc->ReceiveWindow;
	connection->DefaultInChannel->PingOriginator.ConnectionTimeout = 30;
	connection->DefaultInChannel->PingOriginator.KeepAliveInterval = 0;
	connection->DefaultInChannel->Mutex = CreateMutex(NULL, FALSE, NULL);

	connection->DefaultOutChannel->State = CLIENT_OUT_CHANNEL_STATE_INITIAL;
	connection->DefaultOutChannel->BytesReceived = 0;
	connection->DefaultOutChannel->ReceiverAvailableWindow = rpc->ReceiveWindow;
	connection->DefaultOutChannel->ReceiveWindow = rpc->ReceiveWindow;
	connection->DefaultOutChannel->ReceiveWindowSize = rpc->ReceiveWindow;
	connection->DefaultOutChannel->AvailableWindowAdvertised = rpc->ReceiveWindow;
	connection->DefaultOutChannel->Mutex = CreateMutex(NULL, FALSE, NULL);
}

RpcVirtualConnection* rpc_client_virtual_connection_new(rdpRpc* rpc)
{
	RpcVirtualConnection* connection = (RpcVirtualConnection*) calloc(1, sizeof(RpcVirtualConnection));

	if (!connection)
		return NULL;

	connection->State = VIRTUAL_CONNECTION_STATE_INITIAL;
	connection->DefaultInChannel = (RpcInChannel *)calloc(1, sizeof(RpcInChannel));
	if (!connection->DefaultInChannel)
		goto out_free;
	connection->DefaultOutChannel = (RpcOutChannel*)calloc(1, sizeof(RpcOutChannel));
	if (!connection->DefaultOutChannel)
		goto out_default_in;
	rpc_client_virtual_connection_init(rpc, connection);

	return connection;

out_default_in:
	free(connection->DefaultInChannel);
out_free:
	free(connection);
	return NULL;
}

void rpc_client_virtual_connection_free(RpcVirtualConnection* virtual_connection)
{
	if (virtual_connection != NULL)
	{
		free(virtual_connection->DefaultInChannel);
		free(virtual_connection->DefaultOutChannel);
		free(virtual_connection);
	}
}

rdpRpc* rpc_new(rdpTransport* transport)
{
	rdpRpc* rpc = (rdpRpc*) calloc(1, sizeof(rdpRpc));
	if (!rpc)
		return NULL;

	rpc->State = RPC_CLIENT_STATE_INITIAL;

	rpc->transport = transport;
	rpc->settings = transport->settings;

	rpc->SendSeqNum = 0;
	rpc->ntlm = ntlm_new();
	if (!rpc->ntlm)
		goto out_free;

	rpc->NtlmHttpIn = ntlm_http_new();
	if (!rpc->NtlmHttpIn)
		goto out_free_ntlm;
	rpc->NtlmHttpOut = ntlm_http_new();
	if (!rpc->NtlmHttpOut)
		goto out_free_ntlm_http_in;

	rpc_ntlm_http_init_channel(rpc, rpc->NtlmHttpIn, TSG_CHANNEL_IN);
	rpc_ntlm_http_init_channel(rpc, rpc->NtlmHttpOut, TSG_CHANNEL_OUT);

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
	rpc->ChannelLifetimeSet = 0;

	rpc->KeepAliveInterval = 300000;
	rpc->CurrentKeepAliveInterval = rpc->KeepAliveInterval;
	rpc->CurrentKeepAliveTime = 0;

	rpc->VirtualConnection = rpc_client_virtual_connection_new(rpc);
	if (!rpc->VirtualConnection)
		goto out_free_ntlm_http_out;

	rpc->VirtualConnectionCookieTable = ArrayList_New(TRUE);
	if (!rpc->VirtualConnectionCookieTable)
		goto out_free_virtual_connection;

	rpc->CallId = 2;

	rpc_client_new(rpc);

	rpc->client->SynchronousSend = TRUE;
	rpc->client->SynchronousReceive = TRUE;

	return rpc;

out_free_virtual_connection:
	rpc_client_virtual_connection_free(rpc->VirtualConnection);
out_free_ntlm_http_out:
	ntlm_http_free(rpc->NtlmHttpOut);
out_free_ntlm_http_in:
	ntlm_http_free(rpc->NtlmHttpIn);
out_free_ntlm:
	ntlm_free(rpc->ntlm);
out_free:
	free(rpc);
	return NULL;
}

void rpc_free(rdpRpc* rpc)
{
	if (rpc != NULL)
	{
		rpc_client_stop(rpc);

		if (rpc->State >= RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
		{
			ntlm_client_uninit(rpc->ntlm);
			ntlm_free(rpc->ntlm);
		}

		rpc_client_virtual_connection_free(rpc->VirtualConnection);

		ArrayList_Clear(rpc->VirtualConnectionCookieTable);
		ArrayList_Free(rpc->VirtualConnectionCookieTable);

		free(rpc);
	}
}
