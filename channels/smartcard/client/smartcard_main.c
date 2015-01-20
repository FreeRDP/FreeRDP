/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
 * Copyright 2011 Anthony Tong <atong@trustedcs.com>
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

#include <winpr/crt.h>
#include <winpr/smartcard.h>
#include <winpr/environment.h>

#include <freerdp/channels/rdpdr.h>

#include "smartcard_main.h"

void* smartcard_context_thread(SMARTCARD_CONTEXT* pContext)
{
	DWORD nCount;
	DWORD status;
	HANDLE hEvents[2];
	wMessage message;
	SMARTCARD_DEVICE* smartcard;
	SMARTCARD_OPERATION* operation;

	smartcard = pContext->smartcard;

	nCount = 0;
	hEvents[nCount++] = MessageQueue_Event(pContext->IrpQueue);

	while (1)
	{
		status = WaitForMultipleObjects(nCount, hEvents, FALSE, INFINITE);

		if (WaitForSingleObject(MessageQueue_Event(pContext->IrpQueue), 0) == WAIT_OBJECT_0)
		{
			if (!MessageQueue_Peek(pContext->IrpQueue, &message, TRUE))
				break;

			if (message.id == WMQ_QUIT)
				break;

			operation = (SMARTCARD_OPERATION*) message.wParam;

			if (operation)
			{
				status = smartcard_irp_device_control_call(smartcard, operation);

				Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) operation->irp);

				free(operation);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

SMARTCARD_CONTEXT* smartcard_context_new(SMARTCARD_DEVICE* smartcard, SCARDCONTEXT hContext)
{
	SMARTCARD_CONTEXT* pContext;

	pContext = (SMARTCARD_CONTEXT*) calloc(1, sizeof(SMARTCARD_CONTEXT));

	if (!pContext)
		return pContext;

	pContext->smartcard = smartcard;

	pContext->hContext = hContext;

	pContext->IrpQueue = MessageQueue_New(NULL);

	pContext->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) smartcard_context_thread,
			pContext, 0, NULL);

	return pContext;
}

void smartcard_context_free(SMARTCARD_CONTEXT* pContext)
{
	if (!pContext)
		return;

	/* cancel blocking calls like SCardGetStatusChange */
	SCardCancel(pContext->hContext);

	MessageQueue_PostQuit(pContext->IrpQueue, 0);
	WaitForSingleObject(pContext->thread, INFINITE);
	CloseHandle(pContext->thread);

	MessageQueue_Free(pContext->IrpQueue);

	free(pContext);
}

static void smartcard_free(DEVICE* device)
{
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;

	if (smartcard->IrpQueue)
	{
		MessageQueue_PostQuit(smartcard->IrpQueue, 0);
		WaitForSingleObject(smartcard->thread, INFINITE);

		MessageQueue_Free(smartcard->IrpQueue);
		smartcard->IrpQueue = NULL;

		CloseHandle(smartcard->thread);
		smartcard->thread = NULL;
	}

	if (smartcard->device.data)
	{
		Stream_Free(smartcard->device.data, TRUE);
		smartcard->device.data = NULL;
	}

	ListDictionary_Free(smartcard->rgSCardContextList);
	ListDictionary_Free(smartcard->rgOutstandingMessages);
	Queue_Free(smartcard->CompletedIrpQueue);

	if (smartcard->StartedEvent)
	{
		SCardReleaseStartedEvent();
		smartcard->StartedEvent = NULL;
	}

	free(device);
}

/**
 * Initialization occurs when the protocol server sends a device announce message.
 * At that time, we need to cancel all outstanding IRPs.
 */

static void smartcard_init(DEVICE* device)
{
	int index;
	int keyCount;
	ULONG_PTR* pKeys;
	SCARDCONTEXT hContext;
	SMARTCARD_CONTEXT* pContext;
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;

	/**
	 * On protocol termination, the following actions are performed:
	 * For each context in rgSCardContextList, SCardCancel is called causing all outstanding messages to be processed.
	 * After there are no more outstanding messages, SCardReleaseContext is called on each context and the context MUST
	 * be removed from rgSCardContextList.
	 */

	/**
	 * Call SCardCancel on existing contexts, unblocking all outstanding IRPs.
	 */

	if (ListDictionary_Count(smartcard->rgSCardContextList) > 0)
	{
		pKeys = NULL;
		keyCount = ListDictionary_GetKeys(smartcard->rgSCardContextList, &pKeys);

		for (index = 0; index < keyCount; index++)
		{
			pContext = (SMARTCARD_CONTEXT*) ListDictionary_GetItemValue(smartcard->rgSCardContextList, (void*) pKeys[index]);

			if (!pContext)
				continue;

			hContext = pContext->hContext;

			if (SCardIsValidContext(hContext))
			{
				SCardCancel(hContext);
			}
		}

		free(pKeys);
	}

	/**
	 * Call SCardReleaseContext on remaining contexts and remove them from rgSCardContextList.
	 */

	if (ListDictionary_Count(smartcard->rgSCardContextList) > 0)
	{
		pKeys = NULL;
		keyCount = ListDictionary_GetKeys(smartcard->rgSCardContextList, &pKeys);

		for (index = 0; index < keyCount; index++)
		{
			pContext = (SMARTCARD_CONTEXT*) ListDictionary_Remove(smartcard->rgSCardContextList, (void*) pKeys[index]);

			if (!pContext)
				continue;

			hContext = pContext->hContext;

			if (SCardIsValidContext(hContext))
			{
				SCardReleaseContext(hContext);
			}
		}

		free(pKeys);
	}
}

void smartcard_complete_irp(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	void* key;

	key = (void*) (size_t) irp->CompletionId;
	ListDictionary_Remove(smartcard->rgOutstandingMessages, key);

	irp->Complete(irp);
}

void* smartcard_process_irp_worker_proc(SMARTCARD_OPERATION* operation)
{
	IRP* irp;
	UINT32 status;
	SMARTCARD_DEVICE* smartcard;

	irp = operation->irp;
	smartcard = (SMARTCARD_DEVICE*) irp->device;

	status = smartcard_irp_device_control_call(smartcard, operation);

	Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) irp);

	free(operation);

	ExitThread(0);
	return NULL;
}

/**
 * Multiple threads and SCardGetStatusChange:
 * http://musclecard.996296.n3.nabble.com/Multiple-threads-and-SCardGetStatusChange-td4430.html
 */

void smartcard_process_irp(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	void* key;
	UINT32 status;
	BOOL asyncIrp = FALSE;
	SMARTCARD_CONTEXT* pContext = NULL;
	SMARTCARD_OPERATION* operation = NULL;

	key = (void*) (size_t) irp->CompletionId;
	ListDictionary_Add(smartcard->rgOutstandingMessages, key, irp);

	if (irp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
	{
		operation = (SMARTCARD_OPERATION*) calloc(1, sizeof(SMARTCARD_OPERATION));

		if (!operation)
			return;

		operation->irp = irp;

		status = smartcard_irp_device_control_decode(smartcard, operation);

		if (status != SCARD_S_SUCCESS)
		{
			irp->IoStatus = STATUS_UNSUCCESSFUL;

			Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) irp);

			return;
		}

		asyncIrp = TRUE;

		/**
		 * The following matches mstsc's behavior of processing
		 * only certain requests asynchronously while processing
		 * those expected to return fast synchronously.
		 */

		switch (operation->ioControlCode)
		{
			case SCARD_IOCTL_ESTABLISHCONTEXT:
			case SCARD_IOCTL_RELEASECONTEXT:
			case SCARD_IOCTL_ISVALIDCONTEXT:
			case SCARD_IOCTL_LISTREADERGROUPSA:
			case SCARD_IOCTL_LISTREADERGROUPSW:
			case SCARD_IOCTL_LISTREADERSA:
			case SCARD_IOCTL_LISTREADERSW:
			case SCARD_IOCTL_INTRODUCEREADERGROUPA:
			case SCARD_IOCTL_INTRODUCEREADERGROUPW:
			case SCARD_IOCTL_FORGETREADERGROUPA:
			case SCARD_IOCTL_FORGETREADERGROUPW:
			case SCARD_IOCTL_INTRODUCEREADERA:
			case SCARD_IOCTL_INTRODUCEREADERW:
			case SCARD_IOCTL_FORGETREADERA:
			case SCARD_IOCTL_FORGETREADERW:
			case SCARD_IOCTL_ADDREADERTOGROUPA:
			case SCARD_IOCTL_ADDREADERTOGROUPW:
			case SCARD_IOCTL_REMOVEREADERFROMGROUPA:
			case SCARD_IOCTL_REMOVEREADERFROMGROUPW:
			case SCARD_IOCTL_LOCATECARDSA:
			case SCARD_IOCTL_LOCATECARDSW:
			case SCARD_IOCTL_LOCATECARDSBYATRA:
			case SCARD_IOCTL_LOCATECARDSBYATRW:
			case SCARD_IOCTL_CANCEL:
			case SCARD_IOCTL_READCACHEA:
			case SCARD_IOCTL_READCACHEW:
			case SCARD_IOCTL_WRITECACHEA:
			case SCARD_IOCTL_WRITECACHEW:
			case SCARD_IOCTL_GETREADERICON:
			case SCARD_IOCTL_GETDEVICETYPEID:
				asyncIrp = FALSE;
				break;

			case SCARD_IOCTL_GETSTATUSCHANGEA:
			case SCARD_IOCTL_GETSTATUSCHANGEW:
				asyncIrp = TRUE;
				break;

			case SCARD_IOCTL_CONNECTA:
			case SCARD_IOCTL_CONNECTW:
			case SCARD_IOCTL_RECONNECT:
			case SCARD_IOCTL_DISCONNECT:
			case SCARD_IOCTL_BEGINTRANSACTION:
			case SCARD_IOCTL_ENDTRANSACTION:
			case SCARD_IOCTL_STATE:
			case SCARD_IOCTL_STATUSA:
			case SCARD_IOCTL_STATUSW:
			case SCARD_IOCTL_TRANSMIT:
			case SCARD_IOCTL_CONTROL:
			case SCARD_IOCTL_GETATTRIB:
			case SCARD_IOCTL_SETATTRIB:
			case SCARD_IOCTL_GETTRANSMITCOUNT:
				asyncIrp = TRUE;
				break;

			case SCARD_IOCTL_ACCESSSTARTEDEVENT:
			case SCARD_IOCTL_RELEASESTARTEDEVENT:
				asyncIrp = FALSE;
				break;
		}

		pContext = ListDictionary_GetItemValue(smartcard->rgSCardContextList, (void*) operation->hContext);

		if (!pContext)
			asyncIrp = FALSE;

		if (!asyncIrp)
		{
			status = smartcard_irp_device_control_call(smartcard, operation);
			Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) irp);
			free(operation);
		}
		else
		{
			if (pContext)
			{
				MessageQueue_Post(pContext->IrpQueue, NULL, 0, (void*) operation, NULL);
			}
		}
	}
	else
	{
		WLog_ERR(TAG, "Unexpected SmartCard IRP: MajorFunction 0x%08X MinorFunction: 0x%08X",
				 irp->MajorFunction, irp->MinorFunction);
		irp->IoStatus = STATUS_NOT_SUPPORTED;

		Queue_Enqueue(smartcard->CompletedIrpQueue, (void*) irp);
	}
}

static void* smartcard_thread_func(void* arg)
{
	IRP* irp;
	DWORD nCount;
	DWORD status;
	HANDLE hEvents[2];
	wMessage message;
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) arg;

	nCount = 0;
	hEvents[nCount++] = MessageQueue_Event(smartcard->IrpQueue);
	hEvents[nCount++] = Queue_Event(smartcard->CompletedIrpQueue);

	while (1)
	{
		status = WaitForMultipleObjects(nCount, hEvents, FALSE, INFINITE);

		if (WaitForSingleObject(MessageQueue_Event(smartcard->IrpQueue), 0) == WAIT_OBJECT_0)
		{
			if (!MessageQueue_Peek(smartcard->IrpQueue, &message, TRUE))
				break;

			if (message.id == WMQ_QUIT)
			{
				while (WaitForSingleObject(Queue_Event(smartcard->CompletedIrpQueue), 0) == WAIT_OBJECT_0)
				{
					irp = (IRP*) Queue_Dequeue(smartcard->CompletedIrpQueue);

					if (irp)
					{
						if (irp->thread)
						{
							WaitForSingleObject(irp->thread, INFINITE);
							CloseHandle(irp->thread);
							irp->thread = NULL;
						}

						smartcard_complete_irp(smartcard, irp);
					}
				}

				break;
			}

			irp = (IRP*) message.wParam;

			if (irp)
			{
				smartcard_process_irp(smartcard, irp);
			}
		}

		if (WaitForSingleObject(Queue_Event(smartcard->CompletedIrpQueue), 0) == WAIT_OBJECT_0)
		{
			irp = (IRP*) Queue_Dequeue(smartcard->CompletedIrpQueue);

			if (irp)
			{
				if (irp->thread)
				{
					WaitForSingleObject(irp->thread, INFINITE);
					CloseHandle(irp->thread);
					irp->thread = NULL;
				}

				smartcard_complete_irp(smartcard, irp);
			}
		}
	}

	ExitThread(0);
	return NULL;
}

static void smartcard_irp_request(DEVICE* device, IRP* irp)
{
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;
	MessageQueue_Post(smartcard->IrpQueue, NULL, 0, (void*) irp, NULL);
}

/* smartcard is always built-in */
#define DeviceServiceEntry	smartcard_DeviceServiceEntry

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	char* name;
	char* path;
	int length, ck;
	RDPDR_SMARTCARD* device;
	SMARTCARD_DEVICE* smartcard;

	device = (RDPDR_SMARTCARD*) pEntryPoints->device;

	name = device->Name;
	path = device->Path;

	smartcard = (SMARTCARD_DEVICE*) calloc(1, sizeof(SMARTCARD_DEVICE));

	if (!smartcard)
		return -1;

	smartcard->device.type = RDPDR_DTYP_SMARTCARD;
	smartcard->device.name = "SCARD";
	smartcard->device.IRPRequest = smartcard_irp_request;
	smartcard->device.Init = smartcard_init;
	smartcard->device.Free = smartcard_free;

	length = strlen(smartcard->device.name);
	smartcard->device.data = Stream_New(NULL, length + 1);

	Stream_Write(smartcard->device.data, "SCARD", 6);

	smartcard->name = NULL;
	smartcard->path = NULL;

	if (path)
	{
		smartcard->path = path;
		smartcard->name = name;
	}
	else if (name)
	{
		if (1 == sscanf(name, "%d", &ck))
			smartcard->path = name;
		else
			smartcard->name = name;
	}

	smartcard->IrpQueue = MessageQueue_New(NULL);

	smartcard->CompletedIrpQueue = Queue_New(TRUE, -1, -1);

	smartcard->rgSCardContextList = ListDictionary_New(TRUE);

	ListDictionary_ValueObject(smartcard->rgSCardContextList)->fnObjectFree =
			(OBJECT_FREE_FN) smartcard_context_free;

	smartcard->rgOutstandingMessages = ListDictionary_New(TRUE);

	smartcard->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) smartcard_thread_func,
			smartcard, CREATE_SUSPENDED, NULL);

	pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) smartcard);

	ResumeThread(smartcard->thread);

	return 0;
}

