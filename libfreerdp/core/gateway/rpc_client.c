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
		pdu = (RPC_PDU *)malloc(sizeof(RPC_PDU));
		if (!pdu)
			return NULL;
		pdu->s = Stream_New(NULL, rpc->max_recv_frag);
		if (!pdu->s)
		{
			free(pdu);
			return NULL;
		}
	}

	pdu->CallId = 0;
	pdu->Flags = 0;

	Stream_Length(pdu->s) = 0;
	Stream_SetPosition(pdu->s, 0);

	return pdu;
}

int rpc_client_receive_pool_return(rdpRpc* rpc, RPC_PDU* pdu)
{
	return Queue_Enqueue(rpc->client->ReceivePool, pdu) == TRUE ? 0 : -1;
}

int rpc_client_on_fragment_received_event(rdpRpc* rpc)
{
	BYTE* buffer;
	UINT32 StubOffset;
	UINT32 StubLength;
	wStream* fragment;
	rpcconn_hdr_t* header;
	freerdp* instance;

	instance = (freerdp *)rpc->transport->settings->instance;

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

	switch (header->common.ptype)
	{
		case PTYPE_RTS:
			if (rpc->VirtualConnection->State < VIRTUAL_CONNECTION_STATE_OPENED)
			{
				DEBUG_WARN( "%s: warning: unhandled RTS PDU\n", __FUNCTION__);
				return 0;
			}
			DEBUG_WARN( "%s: Receiving Out-of-Sequence RTS PDU\n", __FUNCTION__);
			rts_recv_out_of_sequence_pdu(rpc, buffer, header->common.frag_length);
			rpc_client_fragment_pool_return(rpc, fragment);
			return 0;

		case PTYPE_FAULT:
			rpc_recv_fault_pdu(header);
			Queue_Enqueue(rpc->client->ReceiveQueue, NULL);
			return -1;
		case PTYPE_RESPONSE:
			break;
		default:
			DEBUG_WARN( "%s: unexpected RPC PDU type %d\n", __FUNCTION__, header->common.ptype);
			Queue_Enqueue(rpc->client->ReceiveQueue, NULL);
			return -1;
	}

	rpc->VirtualConnection->DefaultOutChannel->BytesReceived += header->common.frag_length;
	rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow -= header->common.frag_length;

	if (!rpc_get_stub_data_info(rpc, buffer, &StubOffset, &StubLength))
	{
		DEBUG_WARN( "%s: expected stub\n", __FUNCTION__);
		Queue_Enqueue(rpc->client->ReceiveQueue, NULL);
		return -1;
	}

	if (StubLength == 4)
	{
		//DEBUG_WARN( "Ignoring TsProxySendToServer Response\n");
		//DEBUG_MSG("Got stub length 4 with flags %d and callid %d\n", header->common.pfc_flags, header->common.call_id);

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
		DEBUG_WARN( "%s: invalid call_id: actual: %d, expected: %d, frag_count: %d\n", __FUNCTION__,
				rpc->StubCallId, header->common.call_id, rpc->StubFragCount);
	}

	Stream_Write(rpc->client->pdu->s, &buffer[StubOffset], StubLength);
	rpc->StubFragCount++;

	rpc_client_fragment_pool_return(rpc, fragment);

	if (rpc->VirtualConnection->DefaultOutChannel->ReceiverAvailableWindow < (rpc->ReceiveWindow / 2))
	{
		//DEBUG_WARN( "Sending Flow Control Ack PDU\n");
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

	while (1)
	{
		if (!rpc->client->RecvFrag)
			rpc->client->RecvFrag = rpc_client_fragment_pool_take(rpc);

		position = Stream_GetPosition(rpc->client->RecvFrag);

		while (Stream_GetPosition(rpc->client->RecvFrag) < RPC_COMMON_FIELDS_LENGTH)
		{
			status = rpc_out_read(rpc, Stream_Pointer(rpc->client->RecvFrag),
					RPC_COMMON_FIELDS_LENGTH - Stream_GetPosition(rpc->client->RecvFrag));

			if (status < 0)
			{
				DEBUG_WARN( "rpc_client_frag_read: error reading header\n");
				return -1;
			}

			if (!status)
				return 0;

			Stream_Seek(rpc->client->RecvFrag, status);
		}

		if (Stream_GetPosition(rpc->client->RecvFrag) < RPC_COMMON_FIELDS_LENGTH)
			return status;


		header = (rpcconn_common_hdr_t*) Stream_Buffer(rpc->client->RecvFrag);

		if (header->frag_length > rpc->max_recv_frag)
		{
			DEBUG_WARN( "rpc_client_frag_read: invalid fragment size: %d (max: %d)\n",
					header->frag_length, rpc->max_recv_frag);
			winpr_HexDump(Stream_Buffer(rpc->client->RecvFrag), Stream_GetPosition(rpc->client->RecvFrag));
			return -1;
		}

		while (Stream_GetPosition(rpc->client->RecvFrag) < header->frag_length)
		{
			status = rpc_out_read(rpc, Stream_Pointer(rpc->client->RecvFrag),
					header->frag_length - Stream_GetPosition(rpc->client->RecvFrag));

			if (status < 0)
			{
				DEBUG_WARN( "%s: error reading fragment body\n", __FUNCTION__);
				return -1;
			}

			if (!status)
				return 0;

			Stream_Seek(rpc->client->RecvFrag, status);
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

int rpc_send_enqueue_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	RPC_PDU* pdu;
	int status;

	pdu = (RPC_PDU*) malloc(sizeof(RPC_PDU));
	if (!pdu)
		return -1;

	pdu->s = Stream_New(buffer, length);
	if (!pdu->s)
		goto out_free;

	if (!Queue_Enqueue(rpc->client->SendQueue, pdu))
		goto out_free_stream;

	if (rpc->client->SynchronousSend)
	{
		status = WaitForSingleObject(rpc->client->PduSentEvent, SYNCHRONOUS_TIMEOUT);
		if (status == WAIT_TIMEOUT)
		{
			DEBUG_WARN( "%s: timed out waiting for pdu sent event %p\n", __FUNCTION__, rpc->client->PduSentEvent);
			return -1;
		}

		ResetEvent(rpc->client->PduSentEvent);
	}

	return 0;

out_free_stream:
	Stream_Free(pdu->s, TRUE);
out_free:
	free(pdu);
	return -1;
}

int rpc_send_dequeue_pdu(rdpRpc* rpc)
{
	int status;
	RPC_PDU* pdu;
	RpcClientCall* clientCall;
	rpcconn_common_hdr_t* header;
	RpcInChannel *inChannel;

	pdu = (RPC_PDU*) Queue_Dequeue(rpc->client->SendQueue);
	if (!pdu)
		return 0;

	inChannel = rpc->VirtualConnection->DefaultInChannel;
	WaitForSingleObject(inChannel->Mutex, INFINITE);

	status = rpc_in_write(rpc, Stream_Buffer(pdu->s), Stream_Length(pdu->s));

	header = (rpcconn_common_hdr_t*) Stream_Buffer(pdu->s);
	clientCall = rpc_client_call_find_by_id(rpc, header->call_id);
	clientCall->State = RPC_CLIENT_CALL_STATE_DISPATCHED;

	ReleaseMutex(inChannel->Mutex);

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

	dwMilliseconds = rpc->client->SynchronousReceive ? SYNCHRONOUS_TIMEOUT * 4 : 0;

	result = WaitForSingleObject(Queue_Event(rpc->client->ReceiveQueue), dwMilliseconds);
	if (result == WAIT_TIMEOUT)
	{
		DEBUG_WARN( "%s: timed out waiting for receive event\n", __FUNCTION__);
		return NULL;
	}

	if (result != WAIT_OBJECT_0)
		return NULL;

	pdu = (RPC_PDU *)Queue_Dequeue(rpc->client->ReceiveQueue);

#ifdef WITH_DEBUG_TSG
	if (pdu)
	{
		DEBUG_WARN( "Receiving PDU (length: %d, CallId: %d)\n", pdu->s->length, pdu->CallId);
		winpr_HexDump(Stream_Buffer(pdu->s), Stream_Length(pdu->s));
		DEBUG_WARN( "\n");
	}
	else
	{
		DEBUG_WARN( "Receiving a NULL PDU\n");
	}
#endif

	return pdu;
}

RPC_PDU* rpc_recv_peek_pdu(rdpRpc* rpc)
{
	DWORD dwMilliseconds;
	DWORD result;

	dwMilliseconds = rpc->client->SynchronousReceive ? SYNCHRONOUS_TIMEOUT : 0;

	result = WaitForSingleObject(Queue_Event(rpc->client->ReceiveQueue), dwMilliseconds);
	if (result != WAIT_OBJECT_0)
		return NULL;

	return (RPC_PDU *)Queue_Peek(rpc->client->ReceiveQueue);
}

static void* rpc_client_thread(void* arg)
{
	rdpRpc* rpc;
	DWORD status;
	DWORD nCount;
	HANDLE events[3];
	HANDLE ReadEvent;
	int fd;

	rpc = (rdpRpc*) arg;
	fd = BIO_get_fd(rpc->TlsOut->bio, NULL);

	ReadEvent = CreateFileDescriptorEvent(NULL, TRUE, FALSE, fd);

	nCount = 0;
	events[nCount++] = rpc->client->StopEvent;
	events[nCount++] = Queue_Event(rpc->client->SendQueue);
	events[nCount++] = ReadEvent;

	/* Do a first free run in case some bytes were set from the HTTP headers.
	 * We also have to do it because most of the time the underlying socket has notified,
	 * and the ssl layer has eaten all bytes, so we won't be notified any more even if the
	 * bytes are buffered locally
	 */
	if (rpc_client_on_read_event(rpc) < 0)
	{
		DEBUG_WARN( "%s: an error occured when treating first packet\n", __FUNCTION__);
		goto out;
	}

	while (rpc->transport->layer != TRANSPORT_LAYER_CLOSED)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, 100);

		if (status == WAIT_TIMEOUT)
			continue;

		if (WaitForSingleObject(rpc->client->StopEvent, 0) == WAIT_OBJECT_0)
			break;

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

out:
	CloseHandle(ReadEvent);

	return NULL;
}

static void rpc_pdu_free(RPC_PDU* pdu)
{
	if (!pdu)
		return;

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

	client = (RpcClient *)calloc(1, sizeof(RpcClient));
	rpc->client = client;
	if (!client)
		return -1;

	client->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rpc_client_thread,
			rpc, CREATE_SUSPENDED, NULL);
	if (!client->Thread)
		return -1;

	client->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!client->StopEvent)
		return -1;
	client->PduSentEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!client->PduSentEvent)
		return -1;

	client->SendQueue = Queue_New(TRUE, -1, -1);
	if (!client->SendQueue)
		return -1;
	Queue_Object(client->SendQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;

	client->pdu = NULL;
	client->ReceivePool = Queue_New(TRUE, -1, -1);
	if (!client->ReceivePool)
		return -1;
	Queue_Object(client->ReceivePool)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;

	client->ReceiveQueue = Queue_New(TRUE, -1, -1);
	if (!client->ReceiveQueue)
		return -1;
	Queue_Object(client->ReceiveQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_pdu_free;

	client->RecvFrag = NULL;
	client->FragmentPool = Queue_New(TRUE, -1, -1);
	if (!client->FragmentPool)
		return -1;
	Queue_Object(client->FragmentPool)->fnObjectFree = (OBJECT_FREE_FN) rpc_fragment_free;

	client->FragmentQueue = Queue_New(TRUE, -1, -1);
	if (!client->FragmentQueue)
		return -1;
	Queue_Object(client->FragmentQueue)->fnObjectFree = (OBJECT_FREE_FN) rpc_fragment_free;

	client->ClientCallList = ArrayList_New(TRUE);
	if (!client->ClientCallList)
		return -1;
	ArrayList_Object(client->ClientCallList)->fnObjectFree = (OBJECT_FREE_FN) rpc_client_call_free;
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

	return rpc_client_free(rpc);
}

int rpc_client_free(rdpRpc* rpc)
{
	RpcClient* client;

	client = rpc->client;

	if (!client)
		return 0;

	if (client->SendQueue)
		Queue_Free(client->SendQueue);

	if (client->RecvFrag)
		rpc_fragment_free(client->RecvFrag);

	if (client->FragmentPool)
		Queue_Free(client->FragmentPool);
	if (client->FragmentQueue)
		Queue_Free(client->FragmentQueue);

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
	if (client->PduSentEvent)
		CloseHandle(client->PduSentEvent);

	if (client->Thread)
		CloseHandle(client->Thread);

	free(client);
	return 0;
}
