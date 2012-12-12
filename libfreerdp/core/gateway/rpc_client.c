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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>

#include "rpc_fault.h"

#include "rpc_client.h"

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

int rpc_client_on_fragment_received_event(rdpRpc* rpc)
{
	BYTE* buffer;
	UINT32 StubOffset;
	UINT32 StubLength;
	wStream* fragment;
	rpcconn_hdr_t* header;

	fragment = Queue_Dequeue(rpc->client->FragmentQueue);

	buffer = (BYTE*) Stream_Buffer(fragment);
	header = (rpcconn_hdr_t*) Stream_Buffer(fragment);

	rpc_pdu_header_print(header);

	if (rpc->State < RPC_CLIENT_STATE_CONTEXT_NEGOTIATED)
	{
		rpc->pdu->Flags = 0;
		rpc->pdu->Buffer = Stream_Buffer(fragment);
		rpc->pdu->Size = Stream_Length(fragment);
		rpc->pdu->Length = Stream_Length(fragment);
		rpc->pdu->CallId = header->common.call_id;

		Queue_Enqueue(rpc->client->ReceiveQueue, rpc->pdu);
		rpc->pdu = (RPC_PDU*) malloc(sizeof(RPC_PDU));

		return 0;
	}

	if (header->common.ptype == PTYPE_RTS)
	{
		if (rpc->VirtualConnection->State >= VIRTUAL_CONNECTION_STATE_OPENED)
		{
			printf("Receiving Out-of-Sequence RTS PDU\n");
			rts_recv_out_of_sequence_pdu(rpc, buffer, header->common.frag_length);

			rpc_client_fragment_pool_return(rpc, fragment);
		}

		return 0;
	}
	else if (header->common.ptype == PTYPE_FAULT)
	{
		rpc_recv_fault_pdu(header);
		return -1;
	}

	if (header->common.ptype != PTYPE_RESPONSE)
	{
		printf("Unexpected RPC PDU type: %d\n", header->common.ptype);
		return -1;
	}

	rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->common.frag_length;
	rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header->common.frag_length;

	if (!rpc_get_stub_data_info(rpc, buffer, &StubOffset, &StubLength))
	{
		printf("rpc_recv_pdu_fragment: expected stub\n");
		return -1;
	}

	if (StubLength == 4)
	{
		printf("Ignoring TsProxySendToServer Response\n");
		rpc_client_fragment_pool_return(rpc, fragment);
		return 0;
	}

	if (header->response.alloc_hint > rpc->StubBufferSize)
	{
		rpc->StubBufferSize = header->response.alloc_hint;
		rpc->StubBuffer = (BYTE*) realloc(rpc->StubBuffer, rpc->StubBufferSize);
	}

	if (rpc->StubFragCount == 0)
		rpc->StubCallId = header->common.call_id;

	if (rpc->StubCallId != header->common.call_id)
	{
		printf("invalid call_id: actual: %d, expected: %d, frag_count: %d\n",
				rpc->StubCallId, header->common.call_id, rpc->StubFragCount);
	}

	CopyMemory(&rpc->StubBuffer[rpc->StubOffset], &buffer[StubOffset], StubLength);
	rpc->StubOffset += StubLength;
	rpc->StubFragCount++;

	rpc_client_fragment_pool_return(rpc, fragment);

	if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow < (rpc->ReceiveWindow / 2))
	{
		printf("Sending Flow Control Ack PDU\n");
		rts_send_flow_control_ack_pdu(rpc);
	}

	/**
	 * If alloc_hint is set to a nonzero value and a request or a response is fragmented into multiple
	 * PDUs, implementations of these extensions SHOULD set the alloc_hint field in every PDU to be the
	 * combined stub data length of all remaining fragment PDUs.
	 */

	if ((header->response.alloc_hint == StubLength))
	{
		rpc->pdu->CallId = rpc->StubCallId;
		rpc->StubLength = rpc->StubOffset;

		rpc->StubOffset = 0;
		rpc->StubFragCount = 0;
		rpc->StubCallId = 0;

		rpc->pdu->Flags = RPC_PDU_FLAG_STUB;
		rpc->pdu->Buffer = rpc->StubBuffer;
		rpc->pdu->Size = rpc->StubBufferSize;
		rpc->pdu->Length = rpc->StubLength;

		rpc->StubBufferSize = rpc->max_recv_frag;
		rpc->StubBuffer = (BYTE*) malloc(rpc->StubBufferSize);

		Queue_Enqueue(rpc->client->ReceiveQueue, rpc->pdu);
		rpc->pdu = (RPC_PDU*) malloc(sizeof(RPC_PDU));

		return 0;
	}

	return 0;
}

int rpc_client_on_read_event(rdpRpc* rpc)
{
	int position;
	int status = -1;
	rpcconn_common_hdr_t* header;

	position = Stream_Position(rpc->RecvFrag);

	if (Stream_Position(rpc->RecvFrag) < RPC_COMMON_FIELDS_LENGTH)
	{
		status = rpc_out_read(rpc, Stream_Pointer(rpc->RecvFrag),
				RPC_COMMON_FIELDS_LENGTH - Stream_Position(rpc->RecvFrag));

		if (status < 0)
		{
			printf("rpc_client_frag_read: error reading header\n");
			return -1;
		}

		Stream_Seek(rpc->RecvFrag, status);
	}

	if (Stream_Position(rpc->RecvFrag) >= RPC_COMMON_FIELDS_LENGTH)
	{
		header = (rpcconn_common_hdr_t*) Stream_Buffer(rpc->RecvFrag);

		if (header->frag_length > rpc->max_recv_frag)
		{
			printf("rpc_client_frag_read: invalid fragment size: %d (max: %d)\n",
					header->frag_length, rpc->max_recv_frag);
			return -1;
		}

		if (Stream_Position(rpc->RecvFrag) < header->frag_length)
		{
			status = rpc_out_read(rpc, Stream_Pointer(rpc->RecvFrag),
					header->frag_length - Stream_Position(rpc->RecvFrag));

			if (status < 0)
			{
				printf("rpc_client_frag_read: error reading fragment body\n");
				return -1;
			}

			Stream_Seek(rpc->RecvFrag, status);
		}
	}
	else
	{
		return status;
	}

	if (status < 0)
		return -1;

	status = Stream_Position(rpc->RecvFrag) - position;

	if (Stream_Position(rpc->RecvFrag) >= header->frag_length)
	{
		/* complete fragment received */

		Stream_Length(rpc->RecvFrag) = Stream_Position(rpc->RecvFrag);
		Stream_SetPosition(rpc->RecvFrag, 0);

		Queue_Enqueue(rpc->client->FragmentQueue, rpc->RecvFrag);
		rpc->RecvFrag = rpc_client_fragment_pool_take(rpc);

		rpc_client_on_fragment_received_event(rpc);
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
	RpcClientCall* client_call;

	ArrayList_Lock(rpc->client->ClientCallList);

	client_call = NULL;
	count = ArrayList_Count(rpc->client->ClientCallList);

	for (index = 0; index < count; index++)
	{
		client_call = (RpcClientCall*) ArrayList_GetItem(rpc->client->ClientCallList, index);

		if (client_call->CallId == CallId)
			break;
	}

	ArrayList_Unlock(rpc->client->ClientCallList);

	return client_call;
}

RpcClientCall* rpc_client_call_new(UINT32 CallId, UINT32 OpNum)
{
	RpcClientCall* client_call;

	client_call = (RpcClientCall*) malloc(sizeof(RpcClientCall));

	if (client_call)
	{
		client_call->CallId = CallId;
		client_call->OpNum = OpNum;
		client_call->State = RPC_CLIENT_CALL_STATE_SEND_PDUS;
	}

	return client_call;
}

void rpc_client_call_free(RpcClientCall* client_call)
{
	free(client_call);
}

int rpc_send_enqueue_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	RPC_PDU* pdu;

	pdu = (RPC_PDU*) malloc(sizeof(RPC_PDU));
	pdu->Buffer = buffer;
	pdu->Length = length;

	Queue_Enqueue(rpc->client->SendQueue, pdu);

	if (rpc->client->SynchronousSend)
	{
		WaitForSingleObject(rpc->client->PduSentEvent, INFINITE);
		ResetEvent(rpc->client->PduSentEvent);
	}

	return 0;
}

int rpc_send_dequeue_pdu(rdpRpc* rpc)
{
	int status;
	RPC_PDU* pdu;
	RpcClientCall* client_call;
	rpcconn_common_hdr_t* header;

	pdu = (RPC_PDU*) Queue_Dequeue(rpc->client->SendQueue);

	if (!pdu)
		return 0;

	WaitForSingleObject(rpc->VirtualConnection->DefaultInChannel->Mutex, INFINITE);

	status = rpc_in_write(rpc, pdu->Buffer, pdu->Length);

	header = (rpcconn_common_hdr_t*) pdu->Buffer;
	client_call = rpc_client_call_find_by_id(rpc, header->call_id);
	client_call->State = RPC_CLIENT_CALL_STATE_DISPATCHED;

	ReleaseMutex(rpc->VirtualConnection->DefaultInChannel->Mutex);

	/*
	 * This protocol specifies that only RPC PDUs are subject to the flow control abstract
	 * data model. RTS PDUs and the HTTP request and response headers are not subject to flow control.
	 * Implementations of this protocol MUST NOT include them when computing any of the variables
	 * specified by this abstract data model.
	 */
	rpc->VirtualConnection->DefaultInChannel->BytesSent += status;
	rpc->VirtualConnection->DefaultInChannel->SenderAvailableWindow -= status;

	free(pdu->Buffer);
	free(pdu);

	if (rpc->client->SynchronousSend)
		SetEvent(rpc->client->PduSentEvent);

	return status;
}

RPC_PDU* rpc_recv_dequeue_pdu(rdpRpc* rpc)
{
	RPC_PDU* pdu;
	DWORD dwMilliseconds;

	pdu = NULL;
	dwMilliseconds = rpc->client->SynchronousReceive ? INFINITE : 0;

	if (WaitForSingleObject(Queue_Event(rpc->client->ReceiveQueue), dwMilliseconds) == WAIT_OBJECT_0)
	{
		pdu = (RPC_PDU*) Queue_Dequeue(rpc->client->ReceiveQueue);
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

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

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

	CloseHandle(ReadEvent);

	rpc_client_free(rpc);

	return NULL;
}

static void rpc_pdu_free(RPC_PDU* pdu)
{
	free(pdu->Buffer);
	free(pdu);
}

static void rpc_fragment_free(wStream* fragment)
{
	Stream_Free(fragment, TRUE);
}

int rpc_client_new(rdpRpc* rpc)
{
	rpc->client = (RpcClient*) malloc(sizeof(RpcClient));

	rpc->client->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rpc_client_thread,
			rpc, CREATE_SUSPENDED, NULL);

	rpc->client->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	rpc->client->PduSentEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	rpc->client->SendQueue = Queue_New(TRUE, -1, -1);
	rpc->client->ReceiveQueue = Queue_New(TRUE, -1, -1);

	Queue_Object(rpc->client->SendQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;
	Queue_Object(rpc->client->ReceiveQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;

	rpc->client->FragmentPool = Queue_New(TRUE, -1, -1);
	rpc->client->FragmentQueue = Queue_New(TRUE, -1, -1);

	Queue_Object(rpc->client->FragmentPool)->fnObjectFree = (OBJECT_FREE_FN) rpc_fragment_free;
	Queue_Object(rpc->client->FragmentQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_fragment_free;

	rpc->client->ClientCallList = ArrayList_New(TRUE);
	ArrayList_Object(rpc->client->ClientCallList)->fnObjectFree = (OBJECT_FREE_FN) rpc_client_call_free;

	return 0;
}

int rpc_client_start(rdpRpc* rpc)
{
	ResumeThread(rpc->client->Thread);

	return 0;
}

int rpc_client_stop(rdpRpc* rpc)
{
	SetEvent(rpc->client->StopEvent);

	return 0;
}

int rpc_client_free(rdpRpc* rpc)
{
	Queue_Clear(rpc->client->SendQueue);
	Queue_Free(rpc->client->SendQueue);

	Queue_Clear(rpc->client->ReceiveQueue);
	Queue_Free(rpc->client->ReceiveQueue);

	ArrayList_Clear(rpc->client->ClientCallList);
	ArrayList_Free(rpc->client->ClientCallList);

	return 0;
}
