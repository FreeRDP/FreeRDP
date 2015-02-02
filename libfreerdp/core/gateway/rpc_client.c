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

#include "rpc_fault.h"
#include "rpc_client.h"
#include "../rdp.h"

#define TAG FREERDP_TAG("core.gateway")

#define SYNCHRONOUS_TIMEOUT 5000

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

	pdu->Type = 0;
	pdu->Flags = 0;
	pdu->CallId = 0;

	return pdu;
}

static void rpc_pdu_free(RPC_PDU* pdu)
{
	if (!pdu)
		return;

	Stream_Free(pdu->s, TRUE);
	free(pdu);
}

RPC_PDU* rpc_client_receive_pool_take(rdpRpc* rpc)
{
	RPC_PDU* pdu = NULL;

	if (WaitForSingleObject(Queue_Event(rpc->client->ReceivePool), 0) == WAIT_OBJECT_0)
		pdu = Queue_Dequeue(rpc->client->ReceivePool);

	if (!pdu)
		pdu = rpc_pdu_new();

	pdu->Type = 0;
	pdu->Flags = 0;
	pdu->CallId = 0;

	Stream_SetPosition(pdu->s, 0);

	return pdu;
}

int rpc_client_receive_pool_return(rdpRpc* rpc, RPC_PDU* pdu)
{
	return Queue_Enqueue(rpc->client->ReceivePool, pdu) == TRUE ? 0 : -1;
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

int rpc_client_recv_pdu(rdpRpc* rpc, RPC_PDU* pdu)
{
	Queue_Enqueue(rpc->client->ReceiveQueue, pdu);
	return 1;
}

int rpc_client_recv_fragment(rdpRpc* rpc, wStream* fragment)
{
	BYTE* buffer;
	RPC_PDU* pdu;
	UINT32 StubOffset;
	UINT32 StubLength;
	RpcClientCall* call;
	rpcconn_hdr_t* header;

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
			if (!rpc->client->pdu)
				rpc->client->pdu = rpc_client_receive_pool_take(rpc);

			pdu = rpc->client->pdu;

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
				rpc->client->pdu = NULL;
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

		return 0;
	}
	else if (header->common.ptype == PTYPE_RTS)
	{
		if (rpc->State < RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
		{
			if (!rpc->client->pdu)
				rpc->client->pdu = rpc_client_receive_pool_take(rpc);

			pdu = rpc->client->pdu;

			pdu->Flags = 0;
			pdu->Type = header->common.ptype;
			pdu->CallId = header->common.call_id;
			Stream_EnsureCapacity(pdu->s, Stream_Length(fragment));
			Stream_Write(pdu->s, buffer, Stream_Length(fragment));
			Stream_SealLength(pdu->s);
			rpc_client_recv_pdu(rpc, pdu);
			rpc->client->pdu = NULL;
			return 0;
		}
		else
		{
			if (rpc->VirtualConnection->State < VIRTUAL_CONNECTION_STATE_OPENED)
				WLog_ERR(TAG, "warning: unhandled RTS PDU");

			WLog_DBG(TAG, "Receiving Out-of-Sequence RTS PDU");
			rts_recv_out_of_sequence_pdu(rpc, buffer, header->common.frag_length);
		}
	}
	else if (header->common.ptype == PTYPE_BIND_ACK)
	{
		if (!rpc->client->pdu)
			rpc->client->pdu = rpc_client_receive_pool_take(rpc);

		pdu = rpc->client->pdu;

		pdu->Flags = 0;
		pdu->Type = header->common.ptype;
		pdu->CallId = header->common.call_id;
		Stream_EnsureCapacity(pdu->s, Stream_Length(fragment));
		Stream_Write(pdu->s, buffer, Stream_Length(fragment));
		Stream_SealLength(pdu->s);
		rpc_client_recv_pdu(rpc, pdu);
		rpc->client->pdu = NULL;
		return 0;
	}
	else if (header->common.ptype == PTYPE_FAULT)
	{
		rpc_recv_fault_pdu(header);
		return -1;
	}
	else
	{
		WLog_ERR(TAG, "unexpected RPC PDU type 0x%04X", header->common.ptype);
	}

	return 0;
}

int rpc_client_recv(rdpRpc* rpc)
{
	int position;
	int status = -1;
	rpcconn_common_hdr_t* header;

	while (1)
	{
		position = Stream_GetPosition(rpc->client->ReceiveFragment);

		while (Stream_GetPosition(rpc->client->ReceiveFragment) < RPC_COMMON_FIELDS_LENGTH)
		{
			status = rpc_out_read(rpc, Stream_Pointer(rpc->client->ReceiveFragment),
					RPC_COMMON_FIELDS_LENGTH - Stream_GetPosition(rpc->client->ReceiveFragment));

			if (status < 0)
				return -1;

			if (!status)
				return 0;

			Stream_Seek(rpc->client->ReceiveFragment, status);
		}

		if (Stream_GetPosition(rpc->client->ReceiveFragment) < RPC_COMMON_FIELDS_LENGTH)
			return status;

		header = (rpcconn_common_hdr_t*) Stream_Buffer(rpc->client->ReceiveFragment);

		if (header->frag_length > rpc->max_recv_frag)
		{
			WLog_ERR(TAG, "rpc_client_frag_read: invalid fragment size: %d (max: %d)",
					 header->frag_length, rpc->max_recv_frag);
			winpr_HexDump(TAG, WLOG_ERROR, Stream_Buffer(rpc->client->ReceiveFragment), Stream_GetPosition(rpc->client->ReceiveFragment));
			return -1;
		}

		while (Stream_GetPosition(rpc->client->ReceiveFragment) < header->frag_length)
		{
			status = rpc_out_read(rpc, Stream_Pointer(rpc->client->ReceiveFragment),
					header->frag_length - Stream_GetPosition(rpc->client->ReceiveFragment));

			if (status < 0)
			{
				WLog_ERR(TAG, "error reading fragment body");
				return -1;
			}

			if (!status)
				return 0;

			Stream_Seek(rpc->client->ReceiveFragment, status);
		}

		if (status < 0)
			return -1;

		status = Stream_GetPosition(rpc->client->ReceiveFragment) - position;

		if (Stream_GetPosition(rpc->client->ReceiveFragment) >= header->frag_length)
		{
			/* complete fragment received */

			Stream_SealLength(rpc->client->ReceiveFragment);
			Stream_SetPosition(rpc->client->ReceiveFragment, 0);

			status = rpc_client_recv_fragment(rpc, rpc->client->ReceiveFragment);

			if (status < 0)
				return status;

			Stream_SetPosition(rpc->client->ReceiveFragment, 0);
		}

		//if (WaitForSingleObject(Queue_Event(rpc->client->SendQueue), 0) == WAIT_OBJECT_0)
		//	break;
	}

	return 0;
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

int rpc_send_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	int status;
	RpcInChannel* inChannel;
	RpcClientCall* clientCall;
	rpcconn_common_hdr_t* header;

	inChannel = rpc->VirtualConnection->DefaultInChannel;

	status = rpc_in_write(rpc, buffer, length);

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

	free(buffer);

	return status;
}

RPC_PDU* rpc_recv_dequeue_pdu(rdpRpc* rpc, BOOL blocking)
{
	RPC_PDU* pdu;
	DWORD timeout;
	DWORD waitStatus;

	timeout = blocking ? SYNCHRONOUS_TIMEOUT * 4 : 0;

	waitStatus = WaitForSingleObject(Queue_Event(rpc->client->ReceiveQueue), timeout);

	if (waitStatus == WAIT_TIMEOUT)
	{
		WLog_ERR(TAG, "timed out waiting for receive event");
		return NULL;
	}

	if (waitStatus != WAIT_OBJECT_0)
		return NULL;

	pdu = (RPC_PDU*) Queue_Dequeue(rpc->client->ReceiveQueue);

	return pdu;
}

static void* rpc_client_thread(void* arg)
{
	DWORD nCount;
	HANDLE events[8];
	DWORD waitStatus;
	HANDLE ReadEvent = NULL;
	rdpRpc* rpc = (rdpRpc*) arg;

	BIO_get_event(rpc->TlsOut->bio, &ReadEvent);

	nCount = 0;
	events[nCount++] = rpc->client->StopEvent;
	events[nCount++] = ReadEvent;

	/*
	 * OpenSSL buffers TLS records, which can sometimes cause
	 * bytes to be buffered in OpenSSL without the TCP socket
	 * being signaled for read. Do a first read to work around
	 * the problem for now.
	 */

	if (rpc_client_recv(rpc) < 0)
	{
		rpc->transport->layer = TRANSPORT_LAYER_CLOSED;
		return NULL;
	}

	while (rpc->transport->layer != TRANSPORT_LAYER_CLOSED)
	{
		waitStatus = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(rpc->client->StopEvent, 0) == WAIT_OBJECT_0)
			break;

		if (WaitForSingleObject(ReadEvent, 0) == WAIT_OBJECT_0)
		{
			if (rpc_client_recv(rpc) < 0)
			{
				rpc->transport->layer = TRANSPORT_LAYER_CLOSED;
				break;
			}
		}
	}

	return NULL;
}

int rpc_client_new(rdpRpc* rpc)
{
	RpcClient* client;

	client = (RpcClient*) calloc(1, sizeof(RpcClient));

	rpc->client = client;

	if (!client)
		return -1;

	client->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	if (!client->StopEvent)
		return -1;

	client->pdu = NULL;

	client->ReceivePool = Queue_New(TRUE, -1, -1);

	if (!client->ReceivePool)
		return -1;

	Queue_Object(client->ReceivePool)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;

	client->ReceiveQueue = Queue_New(TRUE, -1, -1);

	if (!client->ReceiveQueue)
		return -1;

	Queue_Object(client->ReceiveQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;

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
	return 0;
}

int rpc_client_start(rdpRpc* rpc)
{
	rpc->client->Thread = CreateThread(NULL, 0,
		(LPTHREAD_START_ROUTINE) rpc_client_thread, rpc, 0, NULL);

	return 0;
}

int rpc_client_stop(rdpRpc* rpc)
{
	if (rpc->client->Thread)
	{
		SetEvent(rpc->client->StopEvent);
		WaitForSingleObject(rpc->client->Thread, INFINITE);
		rpc->client->Thread = NULL;
	}

	return 0;
}

int rpc_client_free(rdpRpc* rpc)
{
	RpcClient* client = rpc->client;

	if (!client)
		return 0;

	rpc_client_stop(rpc);

	if (client->ReceiveFragment)
		Stream_Free(client->ReceiveFragment, TRUE);

	if (client->PipeEvent)
		CloseHandle(client->PipeEvent);

	ringbuffer_destroy(&(client->ReceivePipe));

	DeleteCriticalSection(&(client->PipeLock));

	if (client->pdu)
		rpc_pdu_free(client->pdu);

	if (client->ReceivePool)
		Queue_Free(client->ReceivePool);

	if (client->ReceiveQueue)
		Queue_Free(client->ReceiveQueue);

	if (client->ClientCallList)
		ArrayList_Free(client->ClientCallList);

	if (client->StopEvent)
		CloseHandle(client->StopEvent);

	if (client->Thread)
		CloseHandle(client->Thread);

	free(client);
	rpc->client = NULL;

	return 0;
}
