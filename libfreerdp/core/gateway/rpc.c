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
	printf("rpc_vers: %d\n", header->common.rpc_vers);
	printf("rpc_vers_minor: %d\n", header->common.rpc_vers_minor);

	if (header->common.ptype > PTYPE_RTS)
		printf("ptype: %s (%d)\n", "PTYPE_UNKNOWN", header->common.ptype);
	else
		printf("ptype: %s (%d)\n", PTYPE_STRINGS[header->common.ptype], header->common.ptype);

	printf("pfc_flags (0x%02X) = {", header->common.pfc_flags);
	if (header->common.pfc_flags & PFC_FIRST_FRAG)
		printf(" PFC_FIRST_FRAG");
	if (header->common.pfc_flags & PFC_LAST_FRAG)
		printf(" PFC_LAST_FRAG");
	if (header->common.pfc_flags & PFC_PENDING_CANCEL)
		printf(" PFC_PENDING_CANCEL");
	if (header->common.pfc_flags & PFC_RESERVED_1)
		printf(" PFC_RESERVED_1");
	if (header->common.pfc_flags & PFC_CONC_MPX)
		printf(" PFC_CONC_MPX");
	if (header->common.pfc_flags & PFC_DID_NOT_EXECUTE)
		printf(" PFC_DID_NOT_EXECUTE");
	if (header->common.pfc_flags & PFC_OBJECT_UUID)
		printf(" PFC_OBJECT_UUID");
	printf(" }\n");

	printf("packed_drep[4]: %02X %02X %02X %02X\n",
			header->common.packed_drep[0], header->common.packed_drep[1],
			header->common.packed_drep[2], header->common.packed_drep[3]);

	printf("frag_length: %d\n", header->common.frag_length);
	printf("auth_length: %d\n", header->common.auth_length);
	printf("call_id: %d\n", header->common.call_id);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		printf("alloc_hint: %d\n", header->response.alloc_hint);
		printf("p_cont_id: %d\n", header->response.p_cont_id);
		printf("cancel_count: %d\n", header->response.cancel_count);
		printf("reserved: %d\n", header->response.reserved);
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

BOOL rpc_get_stub_data_info(rdpRpc* rpc, BYTE* buffer, UINT32* offset, UINT32* length)
{
	UINT32 alloc_hint = 0;
	rpcconn_hdr_t* header;

	*offset = RPC_COMMON_FIELDS_LENGTH;
	header = ((rpcconn_hdr_t*) buffer);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		*offset += 4;
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

			/**
			 * All PDUs that carry sec_trailer information share certain common fields:
			 * frag_length and auth_length. The beginning of the sec_trailer structure
			 * for each PDU MUST be calculated to start from offset
			 * (frag_length – auth_length – 8) from the beginning of the PDU.
			 */

			sec_trailer_offset = header->common.frag_length - header->common.auth_length - 8;
			*length = sec_trailer_offset - *offset;
		}
		else
		{
			BYTE auth_pad_length;

			auth_pad_length = *(buffer + header->common.frag_length - header->common.auth_length - 6);
			*length = header->common.frag_length - (header->common.auth_length + *offset + 8 + auth_pad_length);
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
	rpc_pdu_header_print((rpcconn_hdr_t*) data);
	printf("Sending PDU (length: %d)\n", FragBufferSize);
	freerdp_hexdump(data, FragBufferSize);
#endif
	
	status = tls_write_all(rpc->TlsIn, data, length);

	return status;
}

int rpc_recv_pdu_header(rdpRpc* rpc, BYTE* header)
{
	int status;
	int bytesRead;
	UINT32 offset;
    
	/* Read common header fields */
    
	bytesRead = 0;
    
	while (bytesRead < RPC_COMMON_FIELDS_LENGTH)
	{
		status = rpc_out_read(rpc, &header[bytesRead], RPC_COMMON_FIELDS_LENGTH - bytesRead);
        
		if (status < 0)
		{
			printf("rpc_recv_pdu_header: error reading header\n");
			return status;
		}
        
		bytesRead += status;
	}

	rpc_get_stub_data_info(rpc, header, &offset, NULL);

	while (bytesRead < offset)
	{
		status = rpc_out_read(rpc, &header[bytesRead], offset - bytesRead);
        
		if (status < 0)
			return status;
        
		bytesRead += status;
	}
    
	return bytesRead;
}

int rpc_recv_pdu_fragment(rdpRpc* rpc)
{
	int status;
	int headerLength;
	int bytesRead = 0;
	rpcconn_hdr_t* header;

	WaitForSingleObject(rpc->VirtualConnection->DefaultInChannel->Mutex, INFINITE);

	status = rpc_recv_pdu_header(rpc, rpc->FragBuffer);
    
	if (status < 1)
	{
		printf("rpc_recv_pdu_header: error reading header\n");
		return status;
	}
    
	headerLength = status;
	header = (rpcconn_hdr_t*) rpc->FragBuffer;
	bytesRead += status;
	
	if (header->common.frag_length > rpc->FragBufferSize)
	{
		rpc->FragBufferSize = header->common.frag_length;
		rpc->FragBuffer = (BYTE*) realloc(rpc->FragBuffer, rpc->FragBufferSize);
		header = (rpcconn_hdr_t*) rpc->FragBuffer;
	}

	while (bytesRead < header->common.frag_length)
	{
		status = rpc_out_read(rpc, &rpc->FragBuffer[bytesRead], header->common.frag_length - bytesRead);

		if (status < 0)
		{
			printf("rpc_recv_pdu: error reading fragment\n");
			return status;
		}

		bytesRead += status;
	}

	ReleaseMutex(rpc->VirtualConnection->DefaultInChannel->Mutex);

	if (header->common.ptype == PTYPE_RTS) /* RTS PDU */
	{
		if (rpc->VirtualConnection->State < VIRTUAL_CONNECTION_STATE_OPENED)
			return header->common.frag_length;

		printf("Receiving Out-of-Sequence RTS PDU\n");
		rts_recv_out_of_sequence_pdu(rpc, rpc->FragBuffer, header->common.frag_length);

		return rpc_recv_pdu_fragment(rpc);
	}
	else if (header->common.ptype == PTYPE_FAULT)
	{
		rpc_recv_fault_pdu(header);
		return -1;
	}

	rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->common.frag_length;
	rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header->common.frag_length;

#if 0
	printf("BytesReceived: %d ReceiverAvailableWindow: %d ReceiveWindow: %d\n",
			rpc->VirtualConnection->DefaultOutChannel->BytesReceived,
			rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow,
			rpc->ReceiveWindow);
#endif

	if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow < (rpc->ReceiveWindow / 2))
	{
		printf("Sending Flow Control Ack PDU\n");
		rts_send_flow_control_ack_pdu(rpc);
	}

#ifdef WITH_DEBUG_RPC
	rpc_pdu_header_print((rpcconn_hdr_t*) header);
	printf("rpc_recv_pdu_fragment: length: %d\n", header->common.frag_length);
	freerdp_hexdump(rpc->FragBuffer, header->common.frag_length);
	printf("\n");
#endif

	return header->common.frag_length;
}

RPC_PDU* rpc_recv_pdu(rdpRpc* rpc)
{
	int status;
	UINT32 StubOffset;
	UINT32 StubLength;
	rpcconn_hdr_t* header;

	status = rpc_recv_pdu_fragment(rpc);

	if (rpc->State < RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
	{
		rpc->pdu->Flags = 0;
		rpc->pdu->Buffer = rpc->FragBuffer;
		rpc->pdu->Size = rpc->FragBufferSize;
		rpc->pdu->Length = status;

		return rpc->pdu;
	}

	header = (rpcconn_hdr_t*) rpc->FragBuffer;

	if (header->common.ptype != PTYPE_RESPONSE)
	{
		printf("rpc_recv_pdu: unexpected ptype 0x%02X\n", header->common.ptype);
		return NULL;
	}

	if (!rpc_get_stub_data_info(rpc, rpc->FragBuffer, &StubOffset, &StubLength))
	{
		printf("rpc_recv_pdu: expected stub\n");
		return NULL;
	}

	if (header->response.alloc_hint > rpc->StubBufferSize)
	{
		rpc->StubBufferSize = header->response.alloc_hint;
		rpc->StubBuffer = (BYTE*) realloc(rpc->StubBuffer, rpc->StubBufferSize);
	}

	CopyMemory(&rpc->StubBuffer[rpc->StubOffset], &rpc->FragBuffer[StubOffset], StubLength);
	rpc->StubOffset += StubLength;
	rpc->StubFragCount++;

	/**
	 * If alloc_hint is set to a nonzero value and a request or a response is fragmented into multiple
	 * PDUs, implementations of these extensions SHOULD set the alloc_hint field in every PDU to be the
	 * combined stub data length of all remaining fragment PDUs.
	 */

	//printf("Receiving Fragment #%d FragStubLength: %d FragLength: %d AllocHint: %d StubOffset: %d\n",
	//		rpc->StubFragCount, StubLength, header->common.frag_length, header->response.alloc_hint, rpc->StubOffset);

	if ((header->response.alloc_hint == StubLength))
	{
		//printf("Reassembled PDU (%d):\n", rpc->StubOffset);
		//freerdp_hexdump(rpc->StubBuffer, rpc->StubOffset);

		rpc->StubLength = rpc->StubOffset;
		rpc->StubOffset = 0;
		rpc->StubFragCount = 0;

		rpc->pdu->Flags = RPC_PDU_FLAG_STUB;
		rpc->pdu->Buffer = rpc->StubBuffer;
		rpc->pdu->Size = rpc->StubBufferSize;
		rpc->pdu->Length = rpc->StubLength;

		return rpc->pdu;
	}

	return rpc_recv_pdu(rpc);
}

int rpc_write(rdpRpc* rpc, BYTE* data, int length, UINT16 opnum)
{
	BYTE* buffer;
	UINT32 offset;
	rdpNtlm* ntlm;
	UINT32 stub_data_pad;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	SECURITY_STATUS encrypt_status;
	rpcconn_request_hdr_t* request_pdu;

	ntlm = rpc->ntlm;

	if (ntlm->table->QueryContextAttributes(&ntlm->context, SECPKG_ATTR_SIZES, &ntlm->ContextSizes) != SEC_E_OK)
	{
		printf("QueryContextAttributes SECPKG_ATTR_SIZES failure\n");
		return -1;
	}

	request_pdu = (rpcconn_request_hdr_t*) malloc(sizeof(rpcconn_request_hdr_t));
	ZeroMemory(request_pdu, sizeof(rpcconn_request_hdr_t));

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*) request_pdu);

	request_pdu->ptype = PTYPE_REQUEST;
	request_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
	request_pdu->auth_length = ntlm->ContextSizes.cbMaxSignature;
	request_pdu->call_id = ++rpc->call_id;

	/* opnum 8 is TsProxySetupReceivePipe, save call_id for checking pipe responses */

	if (opnum == 8)
		rpc->pipe_call_id = rpc->call_id;

	request_pdu->alloc_hint = length;
	request_pdu->p_cont_id = 0x0000;
	request_pdu->opnum = opnum;

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

	encrypt_status = ntlm->table->EncryptMessage(&ntlm->context, 0, &Message, rpc->send_seq_num++);

	if (encrypt_status != SEC_E_OK)
	{
		printf("EncryptMessage status: 0x%08X\n", encrypt_status);
		return -1;
	}

	CopyMemory(&buffer[offset], Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	offset += Buffers[1].cbBuffer;

	rpc_send_enqueue_pdu(rpc, buffer, request_pdu->frag_length);

	return length;
}

BOOL rpc_connect(rdpRpc* rpc)
{
	rpc->TlsIn = rpc->transport->TlsIn;
	rpc->TlsOut = rpc->transport->TlsOut;

	rpc_client_start(rpc);

	if (!rts_connect(rpc))
	{
		printf("rts_connect error!\n");
		return FALSE;
	}

	rpc->State = RPC_CLIENT_STATE_ESTABLISHED;

	if (rpc_secure_bind(rpc) != 0)
	{
		printf("rpc_secure_bind error!\n");
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
	RpcVirtualConnection* connection = (RpcVirtualConnection*) malloc(sizeof(RpcVirtualConnection));

	if (connection != NULL)
	{
		ZeroMemory(connection, sizeof(RpcVirtualConnection));
		connection->State = VIRTUAL_CONNECTION_STATE_INITIAL;
		connection->DefaultInChannel = (RpcInChannel*) malloc(sizeof(RpcInChannel));
		connection->DefaultOutChannel = (RpcOutChannel*) malloc(sizeof(RpcOutChannel));
		ZeroMemory(connection->DefaultInChannel, sizeof(RpcInChannel));
		ZeroMemory(connection->DefaultOutChannel, sizeof(RpcOutChannel));
		rpc_client_virtual_connection_init(rpc, connection);
	}

	return connection;
}

void rpc_client_virtual_connection_free(RpcVirtualConnection* virtual_connection)
{
	if (virtual_connection != NULL)
	{
		free(virtual_connection);
	}
}

/* Virtual Connection Cookie Table */

RpcVirtualConnectionCookieTable* rpc_virtual_connection_cookie_table_new(rdpRpc* rpc)
{
	RpcVirtualConnectionCookieTable* table;

	table = (RpcVirtualConnectionCookieTable*) malloc(sizeof(RpcVirtualConnectionCookieTable));

	if (table != NULL)
	{
		ZeroMemory(table, sizeof(RpcVirtualConnectionCookieTable));

		table->Count = 0;
		table->ArraySize = 32;

		table->Entries = (RpcVirtualConnectionCookieEntry*) malloc(sizeof(RpcVirtualConnectionCookieEntry) * table->ArraySize);
		ZeroMemory(table->Entries, sizeof(RpcVirtualConnectionCookieEntry) * table->ArraySize);
	}

	return table;
}

void rpc_virtual_connection_cookie_table_free(RpcVirtualConnectionCookieTable* table)
{
	if (table != NULL)
	{
		free(table->Entries);
		free(table);
	}
}

/* RPC Core Module */

rdpRpc* rpc_new(rdpTransport* transport)
{
	rdpRpc* rpc = (rdpRpc*) malloc(sizeof(rdpRpc));

	if (rpc != NULL)
	{
		ZeroMemory(rpc, sizeof(rdpRpc));

		rpc->State = RPC_CLIENT_STATE_INITIAL;

		rpc->transport = transport;
		rpc->settings = transport->settings;

		rpc->send_seq_num = 0;
		rpc->ntlm = ntlm_new();

		rpc->NtlmHttpIn = ntlm_http_new();
		rpc->NtlmHttpOut = ntlm_http_new();

		rpc_ntlm_http_init_channel(rpc, rpc->NtlmHttpIn, TSG_CHANNEL_IN);
		rpc_ntlm_http_init_channel(rpc, rpc->NtlmHttpOut, TSG_CHANNEL_OUT);

		rpc->FragBufferSize = 20;
		rpc->FragBuffer = (BYTE*) malloc(rpc->FragBufferSize);

		rpc->StubOffset = 0;
		rpc->StubBufferSize = 20;
		rpc->StubLength = 0;
		rpc->StubFragCount = 0;
		rpc->StubBuffer = (BYTE*) malloc(rpc->FragBufferSize);

		rpc->rpc_vers = 5;
		rpc->rpc_vers_minor = 0;

		/* little-endian data representation */
		rpc->packed_drep[0] = 0x10;
		rpc->packed_drep[1] = 0x00;
		rpc->packed_drep[2] = 0x00;
		rpc->packed_drep[3] = 0x00;

		rpc->max_xmit_frag = 0x0FF8;
		rpc->max_recv_frag = 0x0FF8;

		rpc->pdu = (RPC_PDU*) _aligned_malloc(sizeof(RPC_PDU), MEMORY_ALLOCATION_ALIGNMENT);

		rpc->SendQueue = (PSLIST_HEADER) _aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
		InitializeSListHead(rpc->SendQueue);

		rpc->ReceiveQueue = (PSLIST_HEADER) _aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
		InitializeSListHead(rpc->ReceiveQueue);

		rpc->ReceiveWindow = 0x00010000;

		rpc->ChannelLifetime = 0x40000000;
		rpc->ChannelLifetimeSet = 0;

		rpc->KeepAliveInterval = 300000;
		rpc->CurrentKeepAliveInterval = rpc->KeepAliveInterval;
		rpc->CurrentKeepAliveTime = 0;

		rpc->VirtualConnection = rpc_client_virtual_connection_new(rpc);
		rpc->VirtualConnectionCookieTable = rpc_virtual_connection_cookie_table_new(rpc);

		rpc->call_id = 1;

		rpc_client_new(rpc);

		rpc->client->SynchronousSend = TRUE;
	}

	return rpc;
}

void rpc_free(rdpRpc* rpc)
{
	if (rpc != NULL)
	{
		RPC_PDU* PduEntry;

		ntlm_http_free(rpc->NtlmHttpIn);
		ntlm_http_free(rpc->NtlmHttpOut);

		_aligned_free(rpc->pdu);

		while ((PduEntry = (RPC_PDU*) InterlockedPopEntrySList(rpc->SendQueue)) != NULL)
			_aligned_free(PduEntry);
		_aligned_free(rpc->SendQueue);

		while ((PduEntry = (RPC_PDU*) InterlockedPopEntrySList(rpc->ReceiveQueue)) != NULL)
			_aligned_free(PduEntry);
		_aligned_free(rpc->ReceiveQueue);

		rpc_client_virtual_connection_free(rpc->VirtualConnection);
		rpc_virtual_connection_cookie_table_free(rpc->VirtualConnectionCookieTable);

		free(rpc);
	}
}
