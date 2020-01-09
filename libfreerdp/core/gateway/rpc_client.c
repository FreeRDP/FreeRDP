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
#include "../proxy.h"

#define TAG FREERDP_TAG("core.gateway.rpc")

static void rpc_pdu_reset(RPC_PDU* pdu)
{
	pdu->Type = 0;
	pdu->Flags = 0;
	pdu->CallId = 0;
	Stream_SetPosition(pdu->s, 0);
}

static RPC_PDU* rpc_pdu_new(void)
{
	RPC_PDU* pdu;
	pdu = (RPC_PDU*)malloc(sizeof(RPC_PDU));

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

static int rpc_client_receive_pipe_write(RpcClient* client, const BYTE* buffer, size_t length)
{
	int status = 0;

	if (!client || !buffer)
		return -1;

	EnterCriticalSection(&(client->PipeLock));

	if (ringbuffer_write(&(client->ReceivePipe), buffer, length))
		status += (int)length;

	if (ringbuffer_used(&(client->ReceivePipe)) > 0)
		SetEvent(client->PipeEvent);

	LeaveCriticalSection(&(client->PipeLock));
	return status;
}

int rpc_client_receive_pipe_read(RpcClient* client, BYTE* buffer, size_t length)
{
	int index = 0;
	int status = 0;
	int nchunks = 0;
	DataChunk chunks[2];

	if (!client || !buffer)
		return -1;

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

static int rpc_client_transition_to_state(rdpRpc* rpc, RPC_CLIENT_STATE state)
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

static int rpc_client_recv_pdu(rdpRpc* rpc, RPC_PDU* pdu)
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
				rts = (rpcconn_rts_hdr_t*)Stream_Buffer(pdu->s);

				if (!rts_match_pdu_signature(&RTS_PDU_CONN_A3_SIGNATURE, rts))
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

				rpc_virtual_connection_transition_to_state(rpc, rpc->VirtualConnection,
				                                           VIRTUAL_CONNECTION_STATE_WAIT_C2);
				status = 1;
				break;

			case VIRTUAL_CONNECTION_STATE_WAIT_C2:
				rts = (rpcconn_rts_hdr_t*)Stream_Buffer(pdu->s);

				if (!rts_match_pdu_signature(&RTS_PDU_CONN_C2_SIGNATURE, rts))
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

				rpc_virtual_connection_transition_to_state(rpc, rpc->VirtualConnection,
				                                           VIRTUAL_CONNECTION_STATE_OPENED);
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
				WLog_ERR(TAG,
				         "RPC_CLIENT_STATE_WAIT_SECURE_BIND_ACK unexpected pdu type: 0x%08" PRIX32
				         "",
				         pdu->Type);
				return -1;
			}

			if (rpc_send_rpc_auth_3_pdu(rpc) < 0)
			{
				WLog_ERR(TAG, "rpc_secure_bind: error sending rpc_auth_3 pdu!");
				return -1;
			}

			rpc_client_transition_to_state(rpc, RPC_CLIENT_STATE_CONTEXT_NEGOTIATED);

			if (!tsg_proxy_begin(tsg))
			{
				WLog_ERR(TAG, "tsg_proxy_begin failure");
				return -1;
			}

			status = 1;
		}
		else
		{
			WLog_ERR(TAG, "rpc_client_recv_pdu: invalid rpc->State: %d", rpc->State);
		}
	}
	else if (rpc->State >= RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
	{
		if (!tsg_recv_pdu(tsg, pdu))
			status = -1;
		else
			status = 1;
	}

	return status;
}

static int rpc_client_recv_fragment(rdpRpc* rpc, wStream* fragment)
{
	BYTE* buffer;
	RPC_PDU* pdu;
	UINT32 StubOffset;
	UINT32 StubLength;
	RpcClientCall* call;
	rpcconn_hdr_t* header;
	pdu = rpc->client->pdu;
	buffer = (BYTE*)Stream_Buffer(fragment);
	header = (rpcconn_hdr_t*)Stream_Buffer(fragment);

	if (header->common.ptype == PTYPE_RESPONSE)
	{
		rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->common.frag_length;
		rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -=
		    header->common.frag_length;

		if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow <
		    (rpc->ReceiveWindow / 2))
		{
			if (rts_send_flow_control_ack_pdu(rpc) < 0)
				return -1;
		}

		if (!rpc_get_stub_data_info(rpc, buffer, &StubOffset, &StubLength))
		{
			WLog_ERR(TAG, "expected stub");
			return -1;
		}

		if (StubLength == 4)
		{
			if ((header->common.call_id == rpc->PipeCallId) &&
			    (header->common.pfc_flags & PFC_LAST_FRAG))
			{
				/* End of TsProxySetupReceivePipe */
				TerminateEventArgs e;
				rpc->result = *((UINT32*)&buffer[StubOffset]);
				freerdp_abort_connect(rpc->context->instance);
				tsg_set_state(rpc->transport->tsg, TSG_STATE_TUNNEL_CLOSE_PENDING);
				EventArgsInit(&e, "freerdp");
				e.code = 0;
				PubSub_OnTerminate(rpc->context->pubSub, rpc->context, &e);
				return 0;
			}

			if (header->common.call_id != rpc->PipeCallId)
			{
				/* Ignoring non-TsProxySetupReceivePipe Response */
				return 0;
			}
		}

		if (rpc->StubFragCount == 0)
			rpc->StubCallId = header->common.call_id;

		if (rpc->StubCallId != header->common.call_id)
		{
			WLog_ERR(TAG,
			         "invalid call_id: actual: %" PRIu32 ", expected: %" PRIu32
			         ", frag_count: %" PRIu32 "",
			         rpc->StubCallId, header->common.call_id, rpc->StubFragCount);
		}

		call = rpc_client_call_find_by_id(rpc->client, rpc->StubCallId);

		if (!call)
			return -1;

		if (call->OpNum != TsProxySetupReceivePipeOpnum)
		{
			if (!Stream_EnsureCapacity(pdu->s, header->response.alloc_hint))
				return -1;

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
			rpc_client_receive_pipe_write(rpc->client, &buffer[StubOffset], (size_t)StubLength);
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

			if (!Stream_EnsureCapacity(pdu->s, Stream_Length(fragment)))
				return -1;

			Stream_Write(pdu->s, buffer, Stream_Length(fragment));
			Stream_SealLength(pdu->s);

			if (rpc_client_recv_pdu(rpc, pdu) < 0)
				return -1;

			rpc_pdu_reset(pdu);
		}
		else
		{
			if (rts_recv_out_of_sequence_pdu(rpc, buffer, header->common.frag_length) < 0)
				return -1;
		}

		return 1;
	}
	else if (header->common.ptype == PTYPE_BIND_ACK)
	{
		pdu->Flags = 0;
		pdu->Type = header->common.ptype;
		pdu->CallId = header->common.call_id;

		if (!Stream_EnsureCapacity(pdu->s, Stream_Length(fragment)))
			return -1;

		Stream_Write(pdu->s, buffer, Stream_Length(fragment));
		Stream_SealLength(pdu->s);

		if (rpc_client_recv_pdu(rpc, pdu) < 0)
			return -1;

		rpc_pdu_reset(pdu);
		return 1;
	}
	else if (header->common.ptype == PTYPE_FAULT)
	{
		rpc_recv_fault_pdu(header->fault.status);
		return -1;
	}
	else
	{
		WLog_ERR(TAG, "unexpected RPC PDU type 0x%02" PRIX8 "", header->common.ptype);
		return -1;
	}

	return 1;
}

static int rpc_client_default_out_channel_recv(rdpRpc* rpc)
{
	int status = -1;
	UINT32 statusCode;
	HttpResponse* response;
	RpcInChannel* inChannel;
	RpcOutChannel* outChannel;
	HANDLE outChannelEvent = NULL;
	RpcVirtualConnection* connection = rpc->VirtualConnection;
	inChannel = connection->DefaultInChannel;
	outChannel = connection->DefaultOutChannel;
	BIO_get_event(outChannel->common.tls->bio, &outChannelEvent);

	if (outChannel->State < CLIENT_OUT_CHANNEL_STATE_OPENED)
	{
		if (WaitForSingleObject(outChannelEvent, 0) != WAIT_OBJECT_0)
			return 1;

		response = http_response_recv(outChannel->common.tls, TRUE);

		if (!response)
			return -1;

		if (outChannel->State == CLIENT_OUT_CHANNEL_STATE_SECURITY)
		{
			/* Receive OUT Channel Response */
			if (!rpc_ncacn_http_recv_out_channel_response(&outChannel->common, response))
			{
				http_response_free(response);
				WLog_ERR(TAG, "rpc_ncacn_http_recv_out_channel_response failure");
				return -1;
			}

			/* Send OUT Channel Request */

			if (!rpc_ncacn_http_send_out_channel_request(&outChannel->common, FALSE))
			{
				http_response_free(response);
				WLog_ERR(TAG, "rpc_ncacn_http_send_out_channel_request failure");
				return -1;
			}

			rpc_ncacn_http_ntlm_uninit(&outChannel->common);
			rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_NEGOTIATED);

			/* Send CONN/A1 PDU over OUT channel */

			if (rts_send_CONN_A1_pdu(rpc) < 0)
			{
				http_response_free(response);
				WLog_ERR(TAG, "rpc_send_CONN_A1_pdu error!");
				return -1;
			}

			rpc_out_channel_transition_to_state(outChannel, CLIENT_OUT_CHANNEL_STATE_OPENED);

			if (inChannel->State == CLIENT_IN_CHANNEL_STATE_OPENED)
			{
				rpc_virtual_connection_transition_to_state(
				    rpc, connection, VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT);
			}

			status = 1;
		}

		http_response_free(response);
	}
	else if (connection->State == VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT)
	{
		/* Receive OUT channel response */
		if (WaitForSingleObject(outChannelEvent, 0) != WAIT_OBJECT_0)
			return 1;

		response = http_response_recv(outChannel->common.tls, FALSE);

		if (!response)
			return -1;

		statusCode = http_response_get_status_code(response);

		if (statusCode != HTTP_STATUS_OK)
		{
			WLog_ERR(TAG, "error! Status Code: %" PRIu32 "", statusCode);
			http_response_print(response);

			if (statusCode == HTTP_STATUS_DENIED)
			{
				freerdp_set_last_error_if_not(rpc->context, FREERDP_ERROR_AUTHENTICATION_FAILED);
			}

			http_response_free(response);
			return -1;
		}

		http_response_free(response);
		rpc_virtual_connection_transition_to_state(rpc, rpc->VirtualConnection,
		                                           VIRTUAL_CONNECTION_STATE_WAIT_A3W);
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
				status = rpc_channel_read(&outChannel->common, fragment,
				                          RPC_COMMON_FIELDS_LENGTH - Stream_GetPosition(fragment));

				if (status < 0)
					return -1;

				if (Stream_GetPosition(fragment) < RPC_COMMON_FIELDS_LENGTH)
					return 0;
			}

			header = (rpcconn_common_hdr_t*)Stream_Buffer(fragment);

			if (header->frag_length > rpc->max_recv_frag)
			{
				WLog_ERR(TAG,
				         "rpc_client_recv: invalid fragment size: %" PRIu16 " (max: %" PRIu16 ")",
				         header->frag_length, rpc->max_recv_frag);
				winpr_HexDump(TAG, WLOG_ERROR, Stream_Buffer(fragment),
				              Stream_GetPosition(fragment));
				return -1;
			}

			while (Stream_GetPosition(fragment) < header->frag_length)
			{
				status = rpc_channel_read(&outChannel->common, fragment,
				                          header->frag_length - Stream_GetPosition(fragment));

				if (status < 0)
				{
					WLog_ERR(TAG, "error reading fragment body");
					return -1;
				}

				if (Stream_GetPosition(fragment) < header->frag_length)
					return 0;
			}

			{
				/* complete fragment received */
				Stream_SealLength(fragment);
				Stream_SetPosition(fragment, 0);
				status = rpc_client_recv_fragment(rpc, fragment);

				if (status < 0)
					return status;

				/* channel recycling may update channel pointers */
				if (outChannel->State == CLIENT_OUT_CHANNEL_STATE_RECYCLED &&
				    connection->NonDefaultOutChannel)
				{
					rpc_channel_free(&connection->DefaultOutChannel->common);
					connection->DefaultOutChannel = connection->NonDefaultOutChannel;
					connection->NonDefaultOutChannel = NULL;
					rpc_out_channel_transition_to_state(connection->DefaultOutChannel,
					                                    CLIENT_OUT_CHANNEL_STATE_OPENED);
					rpc_virtual_connection_transition_to_state(
					    rpc, connection, VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT);
					return 0;
				}

				Stream_SetPosition(fragment, 0);
			}
		}
	}

	return status;
}

static int rpc_client_nondefault_out_channel_recv(rdpRpc* rpc)
{
	int status = -1;
	HttpResponse* response;
	RpcOutChannel* nextOutChannel;
	HANDLE nextOutChannelEvent = NULL;
	nextOutChannel = rpc->VirtualConnection->NonDefaultOutChannel;
	BIO_get_event(nextOutChannel->common.tls->bio, &nextOutChannelEvent);

	if (WaitForSingleObject(nextOutChannelEvent, 0) != WAIT_OBJECT_0)
		return 1;

	response = http_response_recv(nextOutChannel->common.tls, TRUE);

	if (response)
	{
		switch (nextOutChannel->State)
		{
			case CLIENT_OUT_CHANNEL_STATE_SECURITY:
				if (rpc_ncacn_http_recv_out_channel_response(&nextOutChannel->common, response))
				{
					if (rpc_ncacn_http_send_out_channel_request(&nextOutChannel->common, TRUE))
					{
						rpc_ncacn_http_ntlm_uninit(&nextOutChannel->common);
						status = rts_send_OUT_R1_A3_pdu(rpc);

						if (status >= 0)
						{
							rpc_out_channel_transition_to_state(
							    nextOutChannel, CLIENT_OUT_CHANNEL_STATE_OPENED_A6W);
						}
						else
						{
							WLog_ERR(TAG, "rts_send_OUT_R1/A3_pdu failure");
						}
					}
					else
					{
						WLog_ERR(TAG, "rpc_ncacn_http_send_out_channel_request failure");
					}
				}
				else
				{
					WLog_ERR(TAG, "rpc_ncacn_http_recv_out_channel_response failure");
				}

				break;

			default:
				WLog_ERR(TAG,
				         "rpc_client_nondefault_out_channel_recv: Unexpected message %08" PRIx32,
				         nextOutChannel->State);
				return -1;
		}

		http_response_free(response);
	}

	return status;
}

int rpc_client_out_channel_recv(rdpRpc* rpc)
{
	int status;
	RpcVirtualConnection* connection = rpc->VirtualConnection;

	if (connection->DefaultOutChannel)
	{
		status = rpc_client_default_out_channel_recv(rpc);

		if (status < 0)
			return -1;
	}

	if (connection->NonDefaultOutChannel)
	{
		status = rpc_client_nondefault_out_channel_recv(rpc);

		if (status < 0)
			return -1;
	}

	return 1;
}

int rpc_client_in_channel_recv(rdpRpc* rpc)
{
	int status = 1;
	HttpResponse* response;
	RpcInChannel* inChannel;
	RpcOutChannel* outChannel;
	HANDLE InChannelEvent = NULL;
	RpcVirtualConnection* connection = rpc->VirtualConnection;
	inChannel = connection->DefaultInChannel;
	outChannel = connection->DefaultOutChannel;
	BIO_get_event(inChannel->common.tls->bio, &InChannelEvent);

	if (WaitForSingleObject(InChannelEvent, 0) != WAIT_OBJECT_0)
		return 1;

	if (inChannel->State < CLIENT_IN_CHANNEL_STATE_OPENED)
	{
		response = http_response_recv(inChannel->common.tls, TRUE);

		if (!response)
			return -1;

		if (inChannel->State == CLIENT_IN_CHANNEL_STATE_SECURITY)
		{
			if (!rpc_ncacn_http_recv_in_channel_response(&inChannel->common, response))
			{
				WLog_ERR(TAG, "rpc_ncacn_http_recv_in_channel_response failure");
				http_response_free(response);
				return -1;
			}

			/* Send IN Channel Request */

			if (!rpc_ncacn_http_send_in_channel_request(&inChannel->common))
			{
				WLog_ERR(TAG, "rpc_ncacn_http_send_in_channel_request failure");
				http_response_free(response);
				return -1;
			}

			rpc_ncacn_http_ntlm_uninit(&inChannel->common);
			rpc_in_channel_transition_to_state(inChannel, CLIENT_IN_CHANNEL_STATE_NEGOTIATED);

			/* Send CONN/B1 PDU over IN channel */

			if (rts_send_CONN_B1_pdu(rpc) < 0)
			{
				WLog_ERR(TAG, "rpc_send_CONN_B1_pdu error!");
				http_response_free(response);
				return -1;
			}

			rpc_in_channel_transition_to_state(inChannel, CLIENT_IN_CHANNEL_STATE_OPENED);

			if (outChannel->State == CLIENT_OUT_CHANNEL_STATE_OPENED)
			{
				rpc_virtual_connection_transition_to_state(
				    rpc, connection, VIRTUAL_CONNECTION_STATE_OUT_CHANNEL_WAIT);
			}

			status = 1;
		}

		http_response_free(response);
	}
	else
	{
		response = http_response_recv(inChannel->common.tls, TRUE);

		if (!response)
			return -1;

		/* We can receive an unauthorized HTTP response on the IN channel */
		http_response_free(response);
	}

	return status;
}

/**
 * [MS-RPCE] Client Call:
 * http://msdn.microsoft.com/en-us/library/gg593159/
 */

RpcClientCall* rpc_client_call_find_by_id(RpcClient* client, UINT32 CallId)
{
	int index;
	int count;
	RpcClientCall* clientCall = NULL;

	if (!client)
		return NULL;

	ArrayList_Lock(client->ClientCallList);
	count = ArrayList_Count(client->ClientCallList);

	for (index = 0; index < count; index++)
	{
		clientCall = (RpcClientCall*)ArrayList_GetItem(client->ClientCallList, index);

		if (clientCall->CallId == CallId)
			break;
	}

	ArrayList_Unlock(client->ClientCallList);
	return clientCall;
}

RpcClientCall* rpc_client_call_new(UINT32 CallId, UINT32 OpNum)
{
	RpcClientCall* clientCall;
	clientCall = (RpcClientCall*)calloc(1, sizeof(RpcClientCall));

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

static void rpc_array_client_call_free(void* call)
{
	rpc_client_call_free((RpcClientCall*)call);
}

int rpc_in_channel_send_pdu(RpcInChannel* inChannel, BYTE* buffer, UINT32 length)
{
	int status;
	RpcClientCall* clientCall;
	rpcconn_common_hdr_t* header;
	status = rpc_channel_write(&inChannel->common, buffer, length);

	if (status <= 0)
		return -1;

	header = (rpcconn_common_hdr_t*)buffer;
	clientCall = rpc_client_call_find_by_id(inChannel->common.client, header->call_id);
	clientCall->State = RPC_CLIENT_CALL_STATE_DISPATCHED;

	/*
	 * This protocol specifies that only RPC PDUs are subject to the flow control abstract
	 * data model. RTS PDUs and the HTTP request and response headers are not subject to flow
	 * control. Implementations of this protocol MUST NOT include them when computing any of the
	 * variables specified by this abstract data model.
	 */

	if (header->ptype == PTYPE_REQUEST)
	{
		inChannel->BytesSent += status;
		inChannel->SenderAvailableWindow -= status;
	}

	return status;
}

BOOL rpc_client_write_call(rdpRpc* rpc, wStream* s, UINT16 opnum)
{
	UINT32 offset;
	BYTE* buffer = NULL;
	UINT32 stub_data_pad;
	SecBuffer Buffers[2] = { 0 };
	SecBufferDesc Message;
	RpcClientCall* clientCall = NULL;
	rdpNtlm* ntlm;
	rpcconn_request_hdr_t* request_pdu = NULL;
	RpcVirtualConnection* connection;
	RpcInChannel* inChannel;
	size_t length;
	SSIZE_T size;
	BOOL rc = FALSE;

	if (!s)
		return FALSE;

	if (!rpc)
		goto fail;

	ntlm = rpc->ntlm;
	connection = rpc->VirtualConnection;

	if (!ntlm)
	{
		WLog_ERR(TAG, "invalid ntlm context");
		goto fail;
	}

	if (!connection)
		goto fail;

	inChannel = connection->DefaultInChannel;

	if (!inChannel)
		goto fail;

	Stream_SealLength(s);
	length = Stream_Length(s);

	if (ntlm_client_query_auth_size(ntlm) < 0)
		goto fail;

	request_pdu = (rpcconn_request_hdr_t*)calloc(1, sizeof(rpcconn_request_hdr_t));

	if (!request_pdu)
		goto fail;

	size = ntlm_client_get_context_max_size(ntlm);

	if (size < 0)
		goto fail;

	rpc_pdu_header_init(rpc, (rpcconn_hdr_t*)request_pdu);
	request_pdu->ptype = PTYPE_REQUEST;
	request_pdu->pfc_flags = PFC_FIRST_FRAG | PFC_LAST_FRAG;
	request_pdu->auth_length = (UINT16)size;
	request_pdu->call_id = rpc->CallId++;
	request_pdu->alloc_hint = length;
	request_pdu->p_cont_id = 0x0000;
	request_pdu->opnum = opnum;
	clientCall = rpc_client_call_new(request_pdu->call_id, request_pdu->opnum);

	if (!clientCall)
		goto fail;

	if (ArrayList_Add(rpc->client->ClientCallList, clientCall) < 0)
	{
		rpc_client_call_free(clientCall);
		goto fail;
	}

	if (request_pdu->opnum == TsProxySetupReceivePipeOpnum)
		rpc->PipeCallId = request_pdu->call_id;

	request_pdu->stub_data = Stream_Buffer(s);
	offset = 24;
	stub_data_pad = rpc_offset_align(&offset, 8);
	offset += length;
	request_pdu->auth_verifier.auth_pad_length = rpc_offset_align(&offset, 4);
	request_pdu->auth_verifier.auth_type = RPC_C_AUTHN_WINNT;
	request_pdu->auth_verifier.auth_level = RPC_C_AUTHN_LEVEL_PKT_INTEGRITY;
	request_pdu->auth_verifier.auth_reserved = 0x00;
	request_pdu->auth_verifier.auth_context_id = 0x00000000;
	offset += (8 + request_pdu->auth_length);
	request_pdu->frag_length = offset;
	buffer = (BYTE*)calloc(1, request_pdu->frag_length);

	if (!buffer)
		goto fail;

	CopyMemory(buffer, request_pdu, 24);
	offset = 24;
	rpc_offset_pad(&offset, stub_data_pad);
	CopyMemory(&buffer[offset], request_pdu->stub_data, length);
	offset += length;
	rpc_offset_pad(&offset, request_pdu->auth_verifier.auth_pad_length);
	CopyMemory(&buffer[offset], &request_pdu->auth_verifier.auth_type, 8);
	offset += 8;
	Buffers[0].BufferType = SECBUFFER_DATA;  /* auth_data */
	Buffers[1].BufferType = SECBUFFER_TOKEN; /* signature */
	Buffers[0].pvBuffer = buffer;
	Buffers[0].cbBuffer = offset;
	Buffers[1].cbBuffer = size;
	Buffers[1].pvBuffer = calloc(1, Buffers[1].cbBuffer);

	if (!Buffers[1].pvBuffer)
		goto fail;

	Message.cBuffers = 2;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer)&Buffers;

	if (!ntlm_client_encrypt(ntlm, 0, &Message, rpc->SendSeqNum++))
		goto fail;

	CopyMemory(&buffer[offset], Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	offset += Buffers[1].cbBuffer;

	if (rpc_in_channel_send_pdu(inChannel, buffer, request_pdu->frag_length) < 0)
		goto fail;

	rc = TRUE;
fail:
	free(buffer);
	free(Buffers[1].pvBuffer);
	free(request_pdu);
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL rpc_client_resolve_gateway(rdpSettings* settings, char** host, UINT16* port,
                                       BOOL* isProxy)
{
	struct addrinfo* result;

	if (!settings || !host || !port || !isProxy)
		return FALSE;
	else
	{
		const char* peerHostname = settings->GatewayHostname;
		const char* proxyUsername = settings->ProxyUsername;
		const char* proxyPassword = settings->ProxyPassword;
		*port = settings->GatewayPort;
		*isProxy = proxy_prepare(settings, &peerHostname, port, &proxyUsername, &proxyPassword);
		result = freerdp_tcp_resolve_host(peerHostname, *port, 0);

		if (!result)
			return FALSE;

		*host =
		    freerdp_tcp_address_to_string((const struct sockaddr_storage*)result->ai_addr, NULL);
		freeaddrinfo(result);
		return TRUE;
	}
}

RpcClient* rpc_client_new(rdpContext* context, UINT32 max_recv_frag)
{
	RpcClient* client = (RpcClient*)calloc(1, sizeof(RpcClient));

	if (!client)
		return NULL;

	if (!rpc_client_resolve_gateway(context->settings, &client->host, &client->port,
	                                &client->isProxy))
		goto fail;

	client->context = context;

	if (!client->context)
		goto fail;

	client->pdu = rpc_pdu_new();

	if (!client->pdu)
		goto fail;

	client->ReceiveFragment = Stream_New(NULL, max_recv_frag);

	if (!client->ReceiveFragment)
		goto fail;

	client->PipeEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!client->PipeEvent)
		goto fail;

	if (!ringbuffer_init(&(client->ReceivePipe), 4096))
		goto fail;

	if (!InitializeCriticalSectionAndSpinCount(&(client->PipeLock), 4000))
		goto fail;

	client->ClientCallList = ArrayList_New(TRUE);

	if (!client->ClientCallList)
		goto fail;

	ArrayList_Object(client->ClientCallList)->fnObjectFree = rpc_array_client_call_free;
	return client;
fail:
	rpc_client_free(client);
	return NULL;
}

void rpc_client_free(RpcClient* client)
{
	if (!client)
		return;

	free(client->host);

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
}
