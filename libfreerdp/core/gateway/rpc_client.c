/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RPC Client
 *
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

#include <freerdp/log.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include "http.h"
#include "ncacn_http.h"

#include "rpc_bind.h"
#include "rpc_fault.h"
#include "rpc_client.h"
#include "../rdp.h"

#define TAG FREERDP_TAG("core.gateway.rpc")

static void rpc_pdu_reset(RPC_PDU* pdu)
{
	pdu->Type = 0;
	pdu->Flags = 0;
	pdu->CallId = 0;
	Stream_SetPosition(pdu->s, 0);
}

RPC_PDU* rpc_pdu_new()
{
	RPC_PDU* pdu;

	pdu = (RPC_PDU*) malloc(sizeof(RPC_PDU));

	if (!pdu)
		return NULL;

	pdu->s = Stream_New(NULL, 4096);

	if (!pdu->s)
	{
		free(pdu);
		return NULL;
	}

	rpc_pdu_reset(pdu);

	return pdu;
}

static void rpc_pdu_free(RPC_PDU* pdu)
{
	if (!pdu)
		return;

	Stream_Free(pdu->s, TRUE);
	free(pdu);
}

int rpc_client_receive_pipe_write(rdpRpc* rpc, const BYTE* buffer, size_t length)
{
	int status = 0;
	RpcClient* client = rpc->client;

	EnterCriticalSection(&(rpc->client->PipeLock));

	if (ringbuffer_write(&(client->ReceivePipe), buffer, length))
		status += (int) length;

	if (ringbuffer_used(&(client->ReceivePipe)) > 0)
		SetEvent(client->PipeEvent);

	LeaveCriticalSection(&(rpc->client->PipeLock));

	return status;
}

int rpc_client_receive_pipe_read(rdpRpc* rpc, BYTE* buffer, size_t length)
{
	int index = 0;
	int status = 0;
	int nchunks = 0;
	DataChunk chunks[2];
	RpcClient* client = rpc->client;

	EnterCriticalSection(&(client->PipeLock));

	nchunks = ringbuffer_peek(&(client->ReceivePipe), chunks, length);

	for (index = 0; index < nchunks; index++)
	{
		CopyMemory(&buffer[status], chunks[index].data, chunks[index].size);
		status += chunks[index].size;
	}

	if (status > 0)
		ringbuffer_commit_read_bytes(&(client->ReceivePipe), status);

	if (ringbuffer_used(&(client->ReceivePipe)) < 1)
		ResetEvent(client->PipeEvent);

	LeaveCriticalSection(&(client->PipeLock));

	return status;
}

int rpc_client_transition_to_state(rdpRpc* rpc, RPC_CLIENT_STATE state)
{
	int status = 1;
	const char* str = "RPC_CLIENT_STATE_UNKNOWN";

	switch (state)
	{
		case RPC_CLIENT_STATE_INITIAL:
			str = "RPC_CLIENT_STATE_INITIAL";
			break;

		case RPC_CLIENT_STATE_ESTABLISHED:
			str = "RPC_CLIENT_STATE_ESTABLISHED";
			break;

		case RPC_CLIENT_STATE_WAIT_SECURE_BIND_ACK:
			str = "RPC_CLIENT_STATE_WAIT_SECURE_BIND_ACK";
			break;

		case RPC_CLIENT_STATE_WAIT_UNSECURE_BIND_ACK:
			str = "RPC_CLIENT_STATE_WAIT_UNSECURE_BIND_ACK";
			break;

		case RPC_CLIENT_STATE_WAIT_SECURE_ALTER_CONTEXT_RESPONSE:
			str = "RPC_CLIENT_STATE_WAIT_SECURE_ALTER_CONTEXT_RESPONSE";
			break;

		case RPC_CLIENT_STATE_CONTEXT_NEGOTIATED:
			str = "RPC_CLIENT_STATE_CONTEXT_NEGOTIATED";
			break;

		case RPC_CLIENT_STATE_WAIT_RESPONSE:
			str = "RPC_CLIENT_STATE_WAIT_RESPONSE";
			break;

		case RPC_CLIENT_STATE_FINAL:
			str = "RPC_CLIENT_STATE_FINAL";
			break;
	}

	rpc->State = state;
	WLog_DBG(TAG, "%s", str);

	return status;
}

int rpc_client_recv_pdu(rdpRpc* rpc, RPC_PDU* pdu)
{
	int status = -1;
	rpcconn_rts_hdr_t* rts;
	rdpTsg* tsg = rpc->transport->tsg;

	if (rpc->VirtualConnection->State < VIRTUAL_CONNECTION_STATE_OPENED)
	{
		switch (rpc->VirtualConnection->State)
		{
			case VIRTUAL_CONNECTION_STATE_INITIAL:
				break;

			case VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT:
				break;

			case VIRTUAL_CONNECTION_STATE_WAIT_A3W:

				rts = (rpcconn_rts_hdr_t*) Stream_Buffer(pdu->s);

				if (!rts_match_pdu_signature(rpc, &RTS_PDU_CONN_A3_SIGNATURE, rts))
				{
					WLog_ERR(TAG, "unexpected RTS PDU: Expected CONN/A3");
					return -1;
				}

				status = rts_recv_CONN_A3_pdu(rpc, Stream_Buffer(pdu->s), Stream_Length(pdu->s));

				if (status < 0)
				{
					WLog_ERR(TAG, "rts_recv_CONN_A3_pdu failure");
					return -1;
				}

				rpc_virtual_connection_transition_to_state(rpc,
						rpc->VirtualConnection, VIRTUAL_CONNECTION_STATE_WAIT_C2);

				status = 1;

				break;

			case VIRTUAL_CONNECTION_STATE_WAIT_C2:

				rts = (rpcconn_rts_hdr_t*) Stream_Buffer(pdu->s);

				if (!rts_match_pdu_signature(rpc, &RTS_PDU_CONN_C2_SIGNATURE, rts))
				{
					WLog_ERR(TAG, "unexpected RTS PDU: Expected CONN/C2");
					return -1;
				}

				status = rts_recv_CONN_C2_pdu(rpc, Stream_Buffer(pdu->s), Stream_Length(pdu->s));

				if (status < 0)
				{
					WLog_ERR(TAG, "rts_recv_CONN_C2_pdu failure");
					return -1;
				}

				rpc_virtual_connection_transition_to_state(rpc,
						rpc->VirtualConnection, VIRTUAL_CONNECTION_STATE_OPENED);

				rpc_client_transition_to_state(rpc, RPC_CLIENT_STATE_ESTABLISHED);

				if (rpc_send_bind_pdu(rpc) < 0)
				{
					WLog_ERR(TAG, "rpc_send_bind_pdu failure");
					return -1;
				}

				rpc_client_transition_to_state(rpc, RPC_CLIENT_STATE_WAIT_SECURE_BIND_ACK);

				status = 1;

				break;

			case VIRTUAL_CONNECTION_STATE_OPENED:
				break;

			case VIRTUAL_CONNECTION_STATE_FINAL:
				break;
		}
	}
	else if (rpc->State < RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
	{
		if (rpc->State == RPC_CLIENT_STATE_WAIT_SECURE_BIND_ACK)
		{
			if (pdu->Type == PTYPE_BIND_ACK)
			{
				if (rpc_recv_bind_ack_pdu(rpc, Stream_Buffer(pdu->s), Stream_Length(pdu->s)) <= 0)
				{
					WLog_ERR(TAG, "rpc_recv_bind_ack_pdu failure");
					return -1;
				}
			}
			else
			{
				WLog_ERR(TAG, "RPC_CLIENT_STATE_WAIT_SECURE_BIND_ACK unexpected pdu type: 0x%04X", pdu->Type);
				return -1;
			}

			if (rpc_send_rpc_auth_3_pdu(rpc) < 0)
			{
				WLog_ERR(TAG, "rpc_secure_bind: error sending rpc_auth_3 pdu!");
				return -1;
			}

			rpc_client_transition_to_state(rpc, RPC_CLIENT_STATE_CONTEXT_NEGOTIATED);

			if (!TsProxyCreateTunnel(tsg, NULL, NULL, NULL, NULL))
			{
				WLog_ERR(TAG, "TsProxyCreateTunnel failure");
				tsg->state = TSG_STATE_FINAL;
				return -1;
			}

			tsg_transition_to_state(tsg, TSG_STATE_INITIAL);

			status = 1;
		}
		else
		{
			WLog_ERR(TAG, "rpc_client_recv_pdu: invalid rpc->State: %d", rpc->State);
		}
	}
	else if (rpc->State >= RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
	{
		if (tsg->state != TSG_STATE_PIPE_CREATED)
		{
			status = tsg_recv_pdu(tsg, pdu);
		}
	}

	return status;
}

int rpc_client_recv_fragment(rdpRpc* rpc, wStream* fragment)
{
	BYTE* buffer;
	RPC_PDU* pdu;
	UINT32 StubOffset;
	UINT32 StubLength;
	RpcClientCall* call;
	rpcconn_hdr_t* header;

	pdu = rpc->client->pdu;
	buffer = (BYTE*) Stream_Buffer(fragment);
	header = (rpcconn_hdr_t*) Stream_Buffer(fragment);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->common.frag_length;
		rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header->common.frag_length;

		if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow < (rpc->ReceiveWindow / 2))
		{
			rts_send_flow_control_ack_pdu(rpc);
		}

		if (!rpc_get_stub_data_info(rpc, buffer, &StubOffset, &StubLength))
		{
			WLog_ERR(TAG, "expected stub");
			return -1;
		}

		if (StubLength == 4)
		{
			/* received a disconnect request from the server? */
			if ((header->common.call_id == rpc->PipeCallId) && (header->common.pfc_flags & PFC_LAST_FRAG))
			{
				TerminateEventArgs e;

				rpc->result = *((UINT32*) &buffer[StubOffset]);

				rpc->context->rdp->disconnect = TRUE;
				rpc->transport->tsg->state = TSG_STATE_TUNNEL_CLOSE_PENDING;
				EventArgsInit(&e, "freerdp");
				e.code = 0;
				PubSub_OnTerminate(rpc->context->pubSub, rpc->context, &e);
			}

			return 0;
		}

		if (rpc->StubFragCount == 0)
			rpc->StubCallId = header->common.call_id;

		if (rpc->StubCallId != header->common.call_id)
		{
			WLog_ERR(TAG, "invalid call_id: actual: %d, expected: %d, frag_count: %d",
					 rpc->StubCallId, header->common.call_id, rpc->StubFragCount);
		}

		call = rpc_client_call_find_by_id(rpc, rpc->StubCallId);

		if (!call)
			return -1;

		if (call->OpNum != TsProxySetupReceivePipeOpnum)
		{
			Stream_EnsureCapacity(pdu->s, header->response.alloc_hint);
			Stream_Write(pdu->s, &buffer[StubOffset], StubLength);
			rpc->StubFragCount++;

			if (header->response.alloc_hint == StubLength)
			{
				pdu->Flags = RPC_PDU_FLAG_STUB;
				pdu->Type = PTYPE_RESPONSE;
				pdu->CallId = rpc->StubCallId;
				Stream_SealLength(pdu->s);
				rpc_client_recv_pdu(rpc, pdu);
				rpc_pdu_reset(pdu);
				rpc->StubFragCount = 0;
				rpc->StubCallId = 0;
			}
		}
		else
		{
			rpc_client_receive_pipe_write(rpc, &buffer[StubOffset], (size_t) StubLength);
			rpc->StubFragCount++;

			if (header->response.alloc_hint == StubLength)
			{
				rpc->StubFragCount = 0;
				rpc->StubCallId = 0;
			}
		}

		return 1;
	}
	else if (header->common.ptype == PTYPE_RTS)
	{
		if (rpc->State < RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
		{
			pdu->Flags = 0;
			pdu->Type = header->common.ptype;
			pdu->CallId = header->common.call_id;
			Stream_EnsureCapacity(pdu->s, Stream_Length(fragment));
			Stream_Write(pdu->s, buffer, Stream_Length(fragment));
			Stream_SealLength(pdu->s);
			rpc_client_recv_pdu(rpc, pdu);
			rpc_pdu_reset(pdu);
		}
		else
		{
			if (rpc->VirtualConnection->State < VIRTUAL_CONNECTION_STATE_OPENED)
				WLog_ERR(TAG, "warning: unhandled RTS PDU");

			WLog_DBG(TAG, "Receiving Out-of-Sequence RTS PDU");
			rts_recv_out_of_sequence_pdu(rpc, buffer, header->common.frag_length);
		}

		return 1;
	}
	else if (header->common.ptype == PTYPE_BIND_ACK)
	{
		pdu->Flags = 0;
		pdu->Type = header->common.ptype;
		pdu->CallId = header->common.call_id;
		Stream_EnsureCapacity(pdu->s, Stream_Length(fragment));
		Stream_Write(pdu->s, buffer, Stream_Length(fragment));
		Stream_SealLength(pdu->s);
		rpc_client_recv_pdu(rpc, pdu);
		rpc_pdu_reset(pdu);

		return 1;
	}
	else if (header->common.ptype == PTYPE_FAULT)
	{
		rpc_recv_fault_pdu(header);
		return -1;
	}
	else
	{
		WLog_ERR(TAG, "unexpected RPC PDU type 0x%04X", header->common.ptype);
		return -1;
	}

	return 1;
}

int rpc_client_out_channel_recv(rdpRpc* rpc)
{
	int status = -1;
	HttpResponse* response;
	RpcInChannel* inChannel;
	RpcOutChannel* outChannel;

	inChannel = rpc->VirtualConnection->DefaultInChannel;
	outChannel = rpc->VirtualConnection->DefaultOutChannel;

	if (outChannel->State < CLIENT_OUT_CHANNEL_STATE_OPENED)
	{
		response = http_response_recv(outChannel->tls);

		if (!response)
			return -1;

		if (outChannel->State == CLIENT_OUT_CHANNEL_STATE_SECURITY)
		{
			/* Receive OUT Channel Response */

			if (rpc_ncacn_http_recv_out_channel_response(rpc, outChannel, response) < 0)
			{
				WLog_ERR(TAG, "rpc_ncacn_http_recv_out_channel_response failure");
				return -1;
			}

			/* Send OUT Channel Request */

			if (rpc_ncacn_http_send_out_channel_request(rpc, outChannel, FALSE) < 0)
			{
				WLog_ERR(TAG, "rpc_ncacn_http_send_out_channel_request failure");
				return -1;
			}

			rpc_ncacn_http_ntlm_uninit(rpc, (RpcChannel*) outChannel);

			rpc_out_channel_transition_to_state(outChannel,
					CLIENT_OUT_CHANNEL_STATE_NEGOTIATED);

			/* Send CONN/A1 PDU over OUT channel */

			if (rts_send_CONN_A1_pdu(rpc) < 0)
			{
				WLog_ERR(TAG, "rpc_send_CONN_A1_pdu error!");
				return -1;
			}

			rpc_out_channel_transition_to_state(outChannel,
					CLIENT_OUT_CHANNEL_STATE_OPENED);

			if (inChannel->State == CLIENT_IN_CHANNEL_STATE_OPENED)
			{
				rpc_virtual_connection_transition_to_state(rpc,
					rpc->VirtualConnection, VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT);
			}

			status = 1;
		}

		http_response_free(response);
	}
	else if (rpc->VirtualConnection->State == VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT)
	{
		/* Receive OUT channel response */

		response = http_response_recv(outChannel->tls);

		if (!response)
			return -1;

		if (response->StatusCode != HTTP_STATUS_OK)
		{
			WLog_ERR(TAG, "error! Status Code: %d", response->StatusCode);
			http_response_print(response);
			http_response_free(response);

			if (response->StatusCode == HTTP_STATUS_DENIED)
			{
				if (!connectErrorCode)
					connectErrorCode = AUTHENTICATIONERROR;

				if (!freerdp_get_last_error(rpc->context))
				{
					freerdp_set_last_error(rpc->context, FREERDP_ERROR_AUTHENTICATION_FAILED);
				}
			}

			return -1;
		}

		http_response_free(response);

		rpc_virtual_connection_transition_to_state(rpc,
				rpc->VirtualConnection, VIRTUAL_CONNECTION_STATE_WAIT_A3W);

		status = 1;
	}
	else
	{
		wStream* fragment;
		rpcconn_common_hdr_t* header;

		fragment = rpc->client->ReceiveFragment;

		while (1)
		{
			while (Stream_GetPosition(fragment) < RPC_COMMON_FIELDS_LENGTH)
			{
				status = rpc_out_channel_read(outChannel, Stream_Pointer(fragment),
						RPC_COMMON_FIELDS_LENGTH - Stream_GetPosition(fragment));

				if (status < 0)
					return -1;

				if (!status)
					return 0;

				Stream_Seek(fragment, status);
			}

			if (Stream_GetPosition(fragment) < RPC_COMMON_FIELDS_LENGTH)
				return status;

			header = (rpcconn_common_hdr_t*) Stream_Buffer(fragment);

			if (header->frag_length > rpc->max_recv_frag)
			{
				WLog_ERR(TAG, "rpc_client_recv: invalid fragment size: %d (max: %d)",
						 header->frag_length, rpc->max_recv_frag);
				winpr_HexDump(TAG, WLOG_ERROR, Stream_Buffer(fragment), Stream_GetPosition(fragment));
				return -1;
			}

			while (Stream_GetPosition(fragment) < header->frag_length)
			{
				status = rpc_out_channel_read(outChannel, Stream_Pointer(fragment),
						header->frag_length - Stream_GetPosition(fragment));

				if (status < 0)
				{
					WLog_ERR(TAG, "error reading fragment body");
					return -1;
				}

				if (!status)
					return 0;

				Stream_Seek(fragment, status);
			}

			if (status < 0)
				return -1;

			if (Stream_GetPosition(fragment) >= header->frag_length)
			{
				/* complete fragment received */

				Stream_SealLength(fragment);
				Stream_SetPosition(fragment, 0);

				status = rpc_client_recv_fragment(rpc, fragment);

				if (status < 0)
					return status;

				Stream_SetPosition(fragment, 0);
			}
		}
	}

	return status;
}

int rpc_client_in_channel_recv(rdpRpc* rpc)
{
	int status = -1;
	HttpResponse* response;
	RpcInChannel* inChannel;
	RpcOutChannel* outChannel;
	HANDLE InChannelEvent = NULL;

	inChannel = rpc->VirtualConnection->DefaultInChannel;
	outChannel = rpc->VirtualConnection->DefaultOutChannel;

	BIO_get_event(inChannel->tls->bio, &InChannelEvent);

	if (WaitForSingleObject(InChannelEvent, 0) != WAIT_OBJECT_0)
		return 1;

	if (inChannel->State < CLIENT_IN_CHANNEL_STATE_OPENED)
	{
		response = http_response_recv(inChannel->tls);

		if (!response)
			return -1;

		if (inChannel->State == CLIENT_IN_CHANNEL_STATE_SECURITY)
		{
			if (rpc_ncacn_http_recv_in_channel_response(rpc, inChannel, response) < 0)
			{
				WLog_ERR(TAG, "rpc_ncacn_http_recv_in_channel_response failure");
				return -1;
			}

			/* Send IN Channel Request */

			if (rpc_ncacn_http_send_in_channel_request(rpc, inChannel) < 0)
			{
				WLog_ERR(TAG, "rpc_ncacn_http_send_in_channel_request failure");
				return -1;
			}

			rpc_ncacn_http_ntlm_uninit(rpc, (RpcChannel*) inChannel);

			rpc_in_channel_transition_to_state(inChannel,
					CLIENT_IN_CHANNEL_STATE_NEGOTIATED);

			/* Send CONN/B1 PDU over IN channel */

			if (rts_send_CONN_B1_pdu(rpc) < 0)
			{
				WLog_ERR(TAG, "rpc_send_CONN_B1_pdu error!");
				return -1;
			}

			rpc_in_channel_transition_to_state(inChannel,
					CLIENT_IN_CHANNEL_STATE_OPENED);

			if (outChannel->State == CLIENT_OUT_CHANNEL_STATE_OPENED)
			{
				rpc_virtual_connection_transition_to_state(rpc,
					rpc->VirtualConnection, VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT);
			}

			status = 1;
		}

		http_response_free(response);
	}

	return status;
}

/**
 * [MS-RPCE] Client Call:
 * http://msdn.microsoft.com/en-us/library/gg593159/
 */

RpcClientCall* rpc_client_call_find_by_id(rdpRpc* rpc, UINT32 CallId)
{
	int index;
	int count;
	RpcClientCall* clientCall = NULL;

	ArrayList_Lock(rpc->client->ClientCallList);
	
	count = ArrayList_Count(rpc->client->ClientCallList);

	for (index = 0; index < count; index++)
	{
		clientCall = (RpcClientCall*) ArrayList_GetItem(rpc->client->ClientCallList, index);

		if (clientCall->CallId == CallId)
			break;
	}

	ArrayList_Unlock(rpc->client->ClientCallList);

	return clientCall;
}

RpcClientCall* rpc_client_call_new(UINT32 CallId, UINT32 OpNum)
{
	RpcClientCall* clientCall;

	clientCall = (RpcClientCall*) calloc(1, sizeof(RpcClientCall));

	if (!clientCall)
		return NULL;

	clientCall->CallId = CallId;
	clientCall->OpNum = OpNum;
	clientCall->State = RPC_CLIENT_CALL_STATE_SEND_PDUS;
	
	return clientCall;
}

void rpc_client_call_free(RpcClientCall* clientCall)
{
	free(clientCall);
}

int rpc_in_channel_send_pdu(RpcInChannel* inChannel, BYTE* buffer, UINT32 length)
{
	int status;
	RpcClientCall* clientCall;
	rpcconn_common_hdr_t* header;
	rdpRpc* rpc = inChannel->rpc;

	status = rpc_in_channel_write(inChannel, buffer, length);

	if (status <= 0)
		return -1;

	header = (rpcconn_common_hdr_t*) buffer;
	clientCall = rpc_client_call_find_by_id(rpc, header->call_id);
	clientCall->State = RPC_CLIENT_CALL_STATE_DISPATCHED;

	/*
	 * This protocol specifies that only RPC PDUs are subject to the flow control abstract
	 * data model. RTS PDUs and the HTTP request and response headers are not subject to flow control.
	 * Implementations of this protocol MUST NOT include them when computing any of the variables
	 * specified by this abstract data model.
	 */

	if (header->ptype == PTYPE_REQUEST)
	{
		inChannel->BytesSent += status;
		inChannel->SenderAvailableWindow -= status;
	}

	return status;
}

int rpc_client_write_call(rdpRpc* rpc, BYTE* data, int length, UINT16 opnum)
{
	UINT32 offset;
	BYTE* buffer = NULL;
	UINT32 stub_data_pad;
	SecBuffer Buffers[2];
	SecBufferDesc Message;
	RpcClientCall* clientCall;
	rdpNtlm* ntlm = rpc->ntlm;
	SECURITY_STATUS encrypt_status;
	rpcconn_request_hdr_t* request_pdu = NULL;
	RpcVirtualConnection* connection = rpc->VirtualConnection;
	RpcInChannel* inChannel = connection->DefaultInChannel;

	if (!ntlm || !ntlm->table)
	{
		WLog_ERR(TAG, "invalid ntlm context");
		return -1;
	}

	if (ntlm->table->QueryContextAttributes(&ntlm->context, SECPKG_ATTR_SIZES, &ntlm->ContextSizes) != SEC_E_OK)
	{
		WLog_ERR(TAG, "QueryContextAttributes SECPKG_ATTR_SIZES failure");
		return -1;
	}

	ZeroMemory(&Buffers, sizeof(Buffers));

	request_pdu = (rpcconn_request_hdr_t*) calloc(1, sizeof(rpcconn_request_hdr_t));

	if (!request_pdu)
		return -1;

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*) request_pdu);
	request_pdu->ptype = PTYPE_REQUEST;
	request_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
	request_pdu->auth_length = (UINT16) ntlm->ContextSizes.cbMaxSignature;
	request_pdu->call_id = rpc->CallId++;
	request_pdu->alloc_hint = length;
	request_pdu->p_cont_id = 0x0000;
	request_pdu->opnum = opnum;

	clientCall = rpc_client_call_new(request_pdu->call_id, request_pdu->opnum);

	if (!clientCall)
		goto out_free_pdu;

	if (ArrayList_Add(rpc->client->ClientCallList, clientCall) < 0)
		goto out_free_clientCall;

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

	buffer = (BYTE*) calloc(1, request_pdu->frag_length);

	if (!buffer)
		goto out_free_pdu;

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
	Buffers[1].pvBuffer = calloc(1, Buffers[1].cbBuffer);

	if (!Buffers[1].pvBuffer)
		goto out_free_pdu;

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer) &Buffers;
	encrypt_status = ntlm->table->EncryptMessage(&ntlm->context, 0, &Message, rpc->SendSeqNum++);

	if (encrypt_status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage status: 0x%08X", encrypt_status);
		goto out_free_pdu;
	}

	CopyMemory(&buffer[offset], Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	offset += Buffers[1].cbBuffer;
	free(Buffers[1].pvBuffer);

	if (rpc_in_channel_send_pdu(inChannel, buffer, request_pdu->frag_length) < 0)
		length = -1;

	free(request_pdu);
	free(buffer);

	return length;

out_free_clientCall:
	rpc_client_call_free(clientCall);
out_free_pdu:
	free(buffer);
	free(Buffers[1].pvBuffer);
	free(request_pdu);
	return -1;
}

int rpc_client_new(rdpRpc* rpc)
{
	RpcClient* client;

	client = (RpcClient*) calloc(1, sizeof(RpcClient));

	rpc->client = client;

	if (!client)
		return -1;

	client->pdu = rpc_pdu_new();

	if (!client->pdu)
		return -1;

	client->ReceiveFragment = Stream_New(NULL, rpc->max_recv_frag);

	if (!client->ReceiveFragment)
		return -1;

	client->PipeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!client->PipeEvent)
		return -1;

	if (!ringbuffer_init(&(client->ReceivePipe), 4096))
		return -1;

	if (!InitializeCriticalSectionAndSpinCount(&(client->PipeLock), 4000))
		return -1;

	client->ClientCallList = ArrayList_New(TRUE);

	if (!client->ClientCallList)
		return -1;

	ArrayList_Object(client->ClientCallList)->fnObjectFree = (OBJECT_FREE_FN) rpc_client_call_free;

	return 1;
}

void rpc_client_free(rdpRpc* rpc)
{
	RpcClient* client = rpc->client;

	if (!client)
		return;

	if (client->ReceiveFragment)
		Stream_Free(client->ReceiveFragment, TRUE);

	if (client->PipeEvent)
		CloseHandle(client->PipeEvent);

	ringbuffer_destroy(&(client->ReceivePipe));

	DeleteCriticalSection(&(client->PipeLock));

	if (client->pdu)
		rpc_pdu_free(client->pdu);

	if (client->ClientCallList)
		ArrayList_Free(client->ClientCallList);

	free(client);
	rpc->client = NULL;
}
