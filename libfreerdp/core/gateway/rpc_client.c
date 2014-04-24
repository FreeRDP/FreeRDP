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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/utils/tcp.h>

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include "rpc_fault.h"

#include "rpc_client.h"

#include "../rdp.h"

#define SYNCHRONOUS_TIMEOUT 5000

wStream* rpc_client_fragment_pool_take(rdpRpc* rpc)
{
	wStream* fragment = NULL;

	if (WaitForSingleObject(Queue_Event(rpc->client->FragmentPool), 0) == WAIT_OBJECT_0)
		fragment = Queue_Dequeue(rpc->client->FragmentPool);

	if (!fragment)
		fragment = Stream_New(NULL, rpc->max_recv_frag);

	return fragment;
}

int rpc_client_fragment_pool_return(rdpRpc* rpc, wStream* fragment)
{
	Queue_Enqueue(rpc->client->FragmentPool, fragment);
	return 0;
}

RPC_PDU* rpc_client_receive_pool_take(rdpRpc* rpc)
{
	RPC_PDU* pdu = NULL;

	if (WaitForSingleObject(Queue_Event(rpc->client->ReceivePool), 0) == WAIT_OBJECT_0)
		pdu = Queue_Dequeue(rpc->client->ReceivePool);

	if (!pdu)
	{
		pdu = (RPC_PDU*) malloc(sizeof(RPC_PDU));
		pdu->s = Stream_New(NULL, rpc->max_recv_frag);
	}

	pdu->CallId = 0;
	pdu->Flags = 0;

	Stream_Length(pdu->s) = 0;
	Stream_SetPosition(pdu->s, 0);

	return pdu;
}

int rpc_client_receive_pool_return(rdpRpc* rpc, RPC_PDU* pdu)
{
	Queue_Enqueue(rpc->client->ReceivePool, pdu);
	return 0;
}

int rpc_client_on_fragment_received_event(rdpRpc* rpc)
{
	BYTE* buffer;
	UINT32 StubOffset;
	UINT32 StubLength;
	wStream* fragment;
	rpcconn_hdr_t* header;
	freerdp* instance;

	instance = (freerdp*) rpc->transport->settings->instance;

	if (!rpc->client->pdu)
		rpc->client->pdu = rpc_client_receive_pool_take(rpc);

	fragment = Queue_Dequeue(rpc->client->FragmentQueue);

	buffer = (BYTE*) Stream_Buffer(fragment);
	header = (rpcconn_hdr_t*) Stream_Buffer(fragment);

	if (rpc->State < RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
	{
		rpc->client->pdu->Flags = 0;
		rpc->client->pdu->CallId = header->common.call_id;

		Stream_EnsureCapacity(rpc->client->pdu->s, Stream_Length(fragment));
		Stream_Write(rpc->client->pdu->s, buffer, Stream_Length(fragment));
		Stream_Length(rpc->client->pdu->s) = Stream_GetPosition(rpc->client->pdu->s);

		rpc_client_fragment_pool_return(rpc, fragment);

		Queue_Enqueue(rpc->client->ReceiveQueue, rpc->client->pdu);
		SetEvent(rpc->transport->ReceiveEvent);
		rpc->client->pdu = NULL;

		return 0;
	}

	if (header->common.ptype == PTYPE_RTS)
	{
		if (rpc->VirtualConnection->State >= VIRTUAL_CONNECTION_STATE_OPENED)
		{
			//fprintf(stderr, "Receiving Out-of-Sequence RTS PDU\n");
			rts_recv_out_of_sequence_pdu(rpc, buffer, header->common.frag_length);

			rpc_client_fragment_pool_return(rpc, fragment);
		}
		else
		{
			fprintf(stderr, "warning: unhandled RTS PDU\n");
		}

		return 0;
	}
	else if (header->common.ptype == PTYPE_FAULT)
	{
		rpc_recv_fault_pdu(header);
		Queue_Enqueue(rpc->client->ReceiveQueue, NULL);
		return -1;
	}

	if (header->common.ptype != PTYPE_RESPONSE)
	{
		fprintf(stderr, "Unexpected RPC PDU type: %d\n", header->common.ptype);
		Queue_Enqueue(rpc->client->ReceiveQueue, NULL);
		return -1;
	}

	rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->common.frag_length;
	rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header->common.frag_length;

	if (!rpc_get_stub_data_info(rpc, buffer, &StubOffset, &StubLength))
	{
		fprintf(stderr, "rpc_recv_pdu_fragment: expected stub\n");
		Queue_Enqueue(rpc->client->ReceiveQueue, NULL);
		return -1;
	}

	if (StubLength == 4)
	{
		//fprintf(stderr, "Ignoring TsProxySendToServer Response\n");
		//printf("Got stub length 4 with flags %d and callid %d\n", header->common.pfc_flags, header->common.call_id);

		/* received a disconnect request from the server? */
		if ((header->common.call_id == rpc->PipeCallId) && (header->common.pfc_flags & PFC_LAST_FRAG))
		{
			TerminateEventArgs e;

			instance->context->rdp->disconnect = TRUE;
			rpc->transport->tsg->state = TSG_STATE_TUNNEL_CLOSE_PENDING;

			EventArgsInit(&e, "freerdp");
			e.code = 0;
			PubSub_OnTerminate(instance->context->pubSub, instance->context, &e);
		}

		rpc_client_fragment_pool_return(rpc, fragment);
		return 0;
	}

	Stream_EnsureCapacity(rpc->client->pdu->s, header->response.alloc_hint);
	buffer = (BYTE*) Stream_Buffer(fragment);
	header = (rpcconn_hdr_t*) Stream_Buffer(fragment);

	if (rpc->StubFragCount == 0)
		rpc->StubCallId = header->common.call_id;

	if (rpc->StubCallId != header->common.call_id)
	{
		fprintf(stderr, "invalid call_id: actual: %d, expected: %d, frag_count: %d\n",
				rpc->StubCallId, header->common.call_id, rpc->StubFragCount);
	}

	Stream_Write(rpc->client->pdu->s, &buffer[StubOffset], StubLength);
	rpc->StubFragCount++;

	rpc_client_fragment_pool_return(rpc, fragment);

	if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow < (rpc->ReceiveWindow / 2))
	{
		//fprintf(stderr, "Sending Flow Control Ack PDU\n");
		rts_send_flow_control_ack_pdu(rpc);
	}

	/**
	 * If alloc_hint is set to a nonzero value and a request or a response is fragmented into multiple
	 * PDUs, implementations of these extensions SHOULD set the alloc_hint field in every PDU to be the
	 * combined stub data length of all remaining fragment PDUs.
	 */

	if (header->response.alloc_hint == StubLength)
	{
		rpc->client->pdu->Flags = RPC_PDU_FLAG_STUB;
		rpc->client->pdu->CallId = rpc->StubCallId;

		Stream_Length(rpc->client->pdu->s) = Stream_GetPosition(rpc->client->pdu->s);

		rpc->StubFragCount = 0;
		rpc->StubCallId = 0;

		Queue_Enqueue(rpc->client->ReceiveQueue, rpc->client->pdu);

		rpc->client->pdu = NULL;

		return 0;
	}

	return 0;
}

int rpc_client_on_read_event(rdpRpc* rpc)
{
	int position;
	int status = -1;
	rpcconn_common_hdr_t* header;

	if (!rpc->client->RecvFrag)
		rpc->client->RecvFrag = rpc_client_fragment_pool_take(rpc);

	position = Stream_GetPosition(rpc->client->RecvFrag);

	if (Stream_GetPosition(rpc->client->RecvFrag) < RPC_COMMON_FIELDS_LENGTH)
	{
		status = rpc_out_read(rpc, Stream_Pointer(rpc->client->RecvFrag),
				RPC_COMMON_FIELDS_LENGTH - Stream_GetPosition(rpc->client->RecvFrag));

		if (status < 0)
		{
			fprintf(stderr, "rpc_client_frag_read: error reading header\n");
			return -1;
		}

		Stream_Seek(rpc->client->RecvFrag, status);
	}

	if (Stream_GetPosition(rpc->client->RecvFrag) >= RPC_COMMON_FIELDS_LENGTH)
	{
		header = (rpcconn_common_hdr_t*) Stream_Buffer(rpc->client->RecvFrag);

		if (header->frag_length > rpc->max_recv_frag)
		{
			fprintf(stderr, "rpc_client_frag_read: invalid fragment size: %d (max: %d)\n",
					header->frag_length, rpc->max_recv_frag);
			winpr_HexDump(Stream_Buffer(rpc->client->RecvFrag), Stream_GetPosition(rpc->client->RecvFrag));
			return -1;
		}

		if (Stream_GetPosition(rpc->client->RecvFrag) < header->frag_length)
		{
			status = rpc_out_read(rpc, Stream_Pointer(rpc->client->RecvFrag),
					header->frag_length - Stream_GetPosition(rpc->client->RecvFrag));

			if (status < 0)
			{
				fprintf(stderr, "rpc_client_frag_read: error reading fragment body\n");
				return -1;
			}

			Stream_Seek(rpc->client->RecvFrag, status);
		}
	}
	else
	{
		return status;
	}

	if (status < 0)
		return -1;

	status = Stream_GetPosition(rpc->client->RecvFrag) - position;

	if (Stream_GetPosition(rpc->client->RecvFrag) >= header->frag_length)
	{
		/* complete fragment received */

		Stream_Length(rpc->client->RecvFrag) = Stream_GetPosition(rpc->client->RecvFrag);
		Stream_SetPosition(rpc->client->RecvFrag, 0);

		Queue_Enqueue(rpc->client->FragmentQueue, rpc->client->RecvFrag);
		rpc->client->RecvFrag = NULL;

		if (rpc_client_on_fragment_received_event(rpc) < 0)
			return -1;
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
	RpcClientCall* clientCall;

	ArrayList_Lock(rpc->client->ClientCallList);

	clientCall = NULL;
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

	clientCall = (RpcClientCall*) malloc(sizeof(RpcClientCall));

	if (clientCall)
	{
		clientCall->CallId = CallId;
		clientCall->OpNum = OpNum;
		clientCall->State = RPC_CLIENT_CALL_STATE_SEND_PDUS;
	}

	return clientCall;
}

void rpc_client_call_free(RpcClientCall* clientCall)
{
	free(clientCall);
}

int rpc_send_enqueue_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	RPC_PDU* pdu;
	int status;

	pdu = (RPC_PDU*) malloc(sizeof(RPC_PDU));
	pdu->s = Stream_New(buffer, length);

	Queue_Enqueue(rpc->client->SendQueue, pdu);

	if (rpc->client->SynchronousSend)
	{
		status = WaitForSingleObject(rpc->client->PduSentEvent, SYNCHRONOUS_TIMEOUT);
		if (status == WAIT_TIMEOUT)
		{
			fprintf(stderr, "rpc_send_enqueue_pdu: timed out waiting for pdu sent event\n");
			return -1;
		}

		ResetEvent(rpc->client->PduSentEvent);
	}

	return 0;
}

int rpc_send_dequeue_pdu(rdpRpc* rpc)
{
	int status;
	RPC_PDU* pdu;
	RpcClientCall* clientCall;
	rpcconn_common_hdr_t* header;

	pdu = (RPC_PDU*) Queue_Dequeue(rpc->client->SendQueue);

	if (!pdu)
		return 0;

	WaitForSingleObject(rpc->VirtualConnection->DefaultInChannel->Mutex, INFINITE);

	status = rpc_in_write(rpc, Stream_Buffer(pdu->s), Stream_Length(pdu->s));

	header = (rpcconn_common_hdr_t*) Stream_Buffer(pdu->s);
	clientCall = rpc_client_call_find_by_id(rpc, header->call_id);
	clientCall->State = RPC_CLIENT_CALL_STATE_DISPATCHED;

	ReleaseMutex(rpc->VirtualConnection->DefaultInChannel->Mutex);

	/*
	 * This protocol specifies that only RPC PDUs are subject to the flow control abstract
	 * data model. RTS PDUs and the HTTP request and response headers are not subject to flow control.
	 * Implementations of this protocol MUST NOT include them when computing any of the variables
	 * specified by this abstract data model.
	 */

	if (header->ptype == PTYPE_REQUEST)
	{
		rpc->VirtualConnection->DefaultInChannel->BytesSent += status;
		rpc->VirtualConnection->DefaultInChannel->SenderAvailableWindow -= status;
	}

	Stream_Free(pdu->s, TRUE);
	free(pdu);

	if (rpc->client->SynchronousSend)
		SetEvent(rpc->client->PduSentEvent);

	return status;
}

RPC_PDU* rpc_recv_dequeue_pdu(rdpRpc* rpc)
{
	RPC_PDU* pdu;
	DWORD dwMilliseconds;
	DWORD result;

	pdu = NULL;
	dwMilliseconds = rpc->client->SynchronousReceive ? SYNCHRONOUS_TIMEOUT : 0;

	result = WaitForSingleObject(Queue_Event(rpc->client->ReceiveQueue), dwMilliseconds);
	if (result == WAIT_TIMEOUT)
	{
		fprintf(stderr, "rpc_recv_dequeue_pdu: timed out waiting for receive event\n");
		return NULL;
	}

	if (result == WAIT_OBJECT_0)
	{
		pdu = (RPC_PDU*) Queue_Dequeue(rpc->client->ReceiveQueue);

#ifdef WITH_DEBUG_TSG
		if (pdu)
		{
			fprintf(stderr, "Receiving PDU (length: %d, CallId: %d)\n", pdu->s->length, pdu->CallId);
			winpr_HexDump(Stream_Buffer(pdu->s), Stream_Length(pdu->s));
			fprintf(stderr, "\n");
		}
#endif

		return pdu;
	}

	return pdu;
}

RPC_PDU* rpc_recv_peek_pdu(rdpRpc* rpc)
{
	RPC_PDU* pdu;
	DWORD dwMilliseconds;
	DWORD result;

	pdu = NULL;
	dwMilliseconds = rpc->client->SynchronousReceive ? SYNCHRONOUS_TIMEOUT : 0;

	result = WaitForSingleObject(Queue_Event(rpc->client->ReceiveQueue), dwMilliseconds);
	if (result == WAIT_TIMEOUT)
	{
		return NULL;
	}

	if (result == WAIT_OBJECT_0)
	{
		pdu = (RPC_PDU*) Queue_Peek(rpc->client->ReceiveQueue);
		return pdu;
	}

	return pdu;
}

static void* rpc_client_thread(void* arg)
{
	rdpRpc* rpc;
	DWORD status;
	DWORD nCount;
	HANDLE events[3];
	HANDLE ReadEvent;

	rpc = (rdpRpc*) arg;

	ReadEvent = CreateFileDescriptorEvent(NULL, TRUE, FALSE, rpc->TlsOut->sockfd);

	nCount = 0;
	events[nCount++] = rpc->client->StopEvent;
	events[nCount++] = Queue_Event(rpc->client->SendQueue);
	events[nCount++] = ReadEvent;

	while (rpc->transport->layer != TRANSPORT_LAYER_CLOSED)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, 100);

		if (status != WAIT_TIMEOUT)
		{
			if (WaitForSingleObject(rpc->client->StopEvent, 0) == WAIT_OBJECT_0)
			{
				break;
			}

			if (WaitForSingleObject(ReadEvent, 0) == WAIT_OBJECT_0)
			{
				if (rpc_client_on_read_event(rpc) < 0)
					break;
			}

			if (WaitForSingleObject(Queue_Event(rpc->client->SendQueue), 0) == WAIT_OBJECT_0)
			{
				rpc_send_dequeue_pdu(rpc);
			}
		}
	}

	CloseHandle(ReadEvent);

	return NULL;
}

static void rpc_pdu_free(RPC_PDU* pdu)
{
	Stream_Free(pdu->s, TRUE);
	free(pdu);
}

static void rpc_fragment_free(wStream* fragment)
{
	Stream_Free(fragment, TRUE);
}

int rpc_client_new(rdpRpc* rpc)
{
	RpcClient* client = NULL;

	client = (RpcClient*) calloc(1, sizeof(RpcClient));

	if (client)
	{
		client->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		client->PduSentEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		client->SendQueue = Queue_New(TRUE, -1, -1);
		Queue_Object(client->SendQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;

		client->pdu = NULL;
		client->ReceivePool = Queue_New(TRUE, -1, -1);
		client->ReceiveQueue = Queue_New(TRUE, -1, -1);
		Queue_Object(client->ReceivePool)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;
		Queue_Object(client->ReceiveQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;

		client->RecvFrag = NULL;
		client->FragmentPool = Queue_New(TRUE, -1, -1);
		client->FragmentQueue = Queue_New(TRUE, -1, -1);

		Queue_Object(client->FragmentPool)->fnObjectFree = (OBJECT_FREE_FN) rpc_fragment_free;
		Queue_Object(client->FragmentQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_fragment_free;

		client->ClientCallList = ArrayList_New(TRUE);
		ArrayList_Object(client->ClientCallList)->fnObjectFree = (OBJECT_FREE_FN) rpc_client_call_free;
	}

	rpc->client = client;

	return 0;
}

int rpc_client_start(rdpRpc* rpc)
{
	rpc->client->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rpc_client_thread,
			rpc, 0, NULL);

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

	rpc_client_free(rpc);

	return 0;
}

int rpc_client_free(rdpRpc* rpc)
{
	RpcClient* client;

	client = rpc->client;

	if (client)
	{
		Queue_Free(client->SendQueue);

		if (client->RecvFrag)
			rpc_fragment_free(client->RecvFrag);

		Queue_Free(client->FragmentPool);
		Queue_Free(client->FragmentQueue);

		if (client->pdu)
			rpc_pdu_free(client->pdu);

		Queue_Free(client->ReceivePool);
		Queue_Free(client->ReceiveQueue);

		ArrayList_Free(client->ClientCallList);

		CloseHandle(client->StopEvent);
		CloseHandle(client->PduSentEvent);

		CloseHandle(client->Thread);

		free(client);
	}

	return 0;
}
