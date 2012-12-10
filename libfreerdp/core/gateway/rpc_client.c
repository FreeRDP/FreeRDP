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

#include "rpc_client.h"

int rpc_client_frag_recv(rdpRpc* rpc)
{
	rpcconn_hdr_t* header;

	header = (rpcconn_hdr_t*) Stream_Buffer(rpc->RecvFrag);

	if ((rpc->PipeCallId) && (header->common.call_id == rpc->PipeCallId))
	{
		/* TsProxySetupReceivePipe response! */
	}

	return 0;
}

int rpc_client_frag_read(rdpRpc* rpc)
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

	if (status < 0)
		return -1;

	status = Stream_Position(rpc->RecvFrag) - position;

	if (Stream_Position(rpc->RecvFrag) >= header->frag_length)
	{
		/* complete fragment received */
		rpc_client_frag_recv(rpc);
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

	ArrayList_Lock(rpc->ClientCalls);

	client_call = NULL;
	count = ArrayList_Count(rpc->ClientCalls);

	for (index = 0; index < count; index++)
	{
		client_call = (RpcClientCall*) ArrayList_GetItem(rpc->ClientCalls, index);

		if (client_call->CallId == CallId)
			break;
	}

	ArrayList_Unlock(rpc->ClientCalls);

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

	Queue_Enqueue(rpc->SendQueue, pdu);
	ReleaseSemaphore(rpc->client->SendSemaphore, 1, NULL);

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

	pdu = (RPC_PDU*) Queue_Dequeue(rpc->SendQueue);

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

int rpc_recv_enqueue_pdu(rdpRpc* rpc)
{
	RPC_PDU* pdu;

	pdu = rpc_recv_pdu(rpc);

	if (!pdu)
		return 0;

	rpc->pdu = (RPC_PDU*) malloc(sizeof(RPC_PDU));

	if (pdu->Flags & RPC_PDU_FLAG_STUB)
	{
		rpc->StubBufferSize = rpc->max_recv_frag;
		rpc->StubBuffer = (BYTE*) malloc(rpc->StubBufferSize);
	}
	else
	{
		rpc->FragBufferSize = rpc->max_recv_frag;
		rpc->FragBuffer = (BYTE*) malloc(rpc->FragBufferSize);
	}

	Queue_Enqueue(rpc->ReceiveQueue, pdu);
	ReleaseSemaphore(rpc->client->ReceiveSemaphore, 1, NULL);

	return 0;
}

RPC_PDU* rpc_recv_dequeue_pdu(rdpRpc* rpc)
{
	RPC_PDU* pdu;
	DWORD dwMilliseconds;

	pdu = NULL;
	dwMilliseconds = rpc->client->SynchronousReceive ? INFINITE : 0;

	if (rpc->client->SynchronousReceive)
		rpc_recv_enqueue_pdu(rpc);

	if (WaitForSingleObject(rpc->client->ReceiveSemaphore, dwMilliseconds) == WAIT_OBJECT_0)
	{
		pdu = (RPC_PDU*) Queue_Dequeue(rpc->ReceiveQueue);
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
	events[nCount++] = rpc->client->SendSemaphore;
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
			if (!rpc->client->SynchronousReceive)
				rpc_recv_enqueue_pdu(rpc);
		}

		rpc_send_dequeue_pdu(rpc);
	}

	CloseHandle(ReadEvent);

	return NULL;
}

int rpc_client_new(rdpRpc* rpc)
{
	rpc->client = (RpcClient*) malloc(sizeof(RpcClient));

	rpc->client->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rpc_client_thread,
			rpc, CREATE_SUSPENDED, NULL);

	rpc->client->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	rpc->client->SendSemaphore = CreateSemaphore(NULL, 0, 64, NULL);
	rpc->client->PduSentEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	return 0;
}

int rpc_client_start(rdpRpc* rpc)
{
	ResumeThread(rpc->client->Thread);

	return 0;
}
