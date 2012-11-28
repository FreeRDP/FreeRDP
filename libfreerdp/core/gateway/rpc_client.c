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

#include "rpc_client.h"

int rpc_send_enqueue_pdu(rdpRpc* rpc, BYTE* buffer, UINT32 length)
{
	RPC_PDU_ENTRY* PduEntry;

	PduEntry = (RPC_PDU_ENTRY*) _aligned_malloc(sizeof(RPC_PDU_ENTRY), MEMORY_ALLOCATION_ALIGNMENT);
	PduEntry->Buffer = buffer;
	PduEntry->Length = length;

	InterlockedPushEntrySList(rpc->SendQueue, &(PduEntry->ItemEntry));

	return 0;
}

int rpc_send_dequeue_pdu(rdpRpc* rpc)
{
	int status;
	RPC_PDU_ENTRY* PduEntry;

	PduEntry = (RPC_PDU_ENTRY*) InterlockedPopEntrySList(rpc->SendQueue);

	status = rpc_in_write(rpc, PduEntry->Buffer, PduEntry->Length);

	/*
	 * This protocol specifies that only RPC PDUs are subject to the flow control abstract
	 * data model. RTS PDUs and the HTTP request and response headers are not subject to flow control.
	 * Implementations of this protocol MUST NOT include them when computing any of the variables
	 * specified by this abstract data model.
	 */
	rpc->VirtualConnection->DefaultInChannel->BytesSent += status;
	rpc->VirtualConnection->DefaultInChannel->SenderAvailableWindow -= status;

	_aligned_free(PduEntry);

	return status;
}

static void* rpc_client_thread(void* arg)
{
	rdpRpc* rpc;
	DWORD status;
	HANDLE events[3];

	rpc = (rdpRpc*) arg;

	events[0] = rpc->client->StopEvent;
	events[1] = rpc->client->SendEvent;
	events[2] = rpc->client->ReceiveEvent;

	while (1)
	{
		status = WaitForMultipleObjects(3, events, FALSE, INFINITE);

		if (WaitForSingleObject(rpc->client->StopEvent, 0) == WAIT_OBJECT_0)
			break;

		if (WaitForSingleObject(rpc->client->SendEvent, 0) == WAIT_OBJECT_0)
		{
			rpc_send_dequeue_pdu(rpc);
		}

		if (WaitForSingleObject(rpc->client->ReceiveEvent, 0) == WAIT_OBJECT_0)
		{

		}
	}

	return NULL;
}

int rpc_client_start(rdpRpc* rpc)
{
	rpc->client = (RpcClient*) malloc(sizeof(RpcClient));

	rpc->client->Thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) rpc_client_thread,
			rpc, CREATE_SUSPENDED, NULL);

	rpc->client->SendEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	rpc->client->ReceiveEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	rpc->client->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	ResumeThread(rpc->client->Thread);

	return 0;
}
