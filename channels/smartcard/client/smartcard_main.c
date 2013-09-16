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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/utils/list.h>
#include <freerdp/utils/debug.h>
#include <freerdp/utils/svc_plugin.h>

#include <freerdp/channels/rdpdr.h>

#include "smartcard_main.h"

static void smartcard_free(DEVICE* dev)
{
	IRP* irp;
	COMPLETIONIDINFO* CompletionIdInfo;
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) dev;

	SetEvent(smartcard->stopEvent);
	WaitForSingleObject(smartcard->thread, INFINITE);

	while ((irp = (IRP*) InterlockedPopEntrySList(smartcard->pIrpList)) != NULL)
		irp->Discard(irp);

	_aligned_free(smartcard->pIrpList);

	/* Begin TS Client defect workaround. */

	while ((CompletionIdInfo = (COMPLETIONIDINFO*) list_dequeue(smartcard->CompletionIds)) != NULL)
		free(CompletionIdInfo);

	CloseHandle(smartcard->thread);
	CloseHandle(smartcard->irpEvent);
	CloseHandle(smartcard->stopEvent);
	CloseHandle(smartcard->CompletionIdsMutex);

	Stream_Free(smartcard->device.data, TRUE);
	list_free(smartcard->CompletionIds);

	/* End TS Client defect workaround. */

	free(dev);
}

static void smartcard_process_irp(SMARTCARD_DEVICE* smartcard, IRP* irp)
{
	switch (irp->MajorFunction)
	{
		case IRP_MJ_DEVICE_CONTROL:
			smartcard_device_control(smartcard, irp);
			break;

		default:
			fprintf(stderr, "MajorFunction 0x%X unexpected for smartcards.", irp->MajorFunction);
			DEBUG_WARN("Smartcard MajorFunction 0x%X not supported.", irp->MajorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void smartcard_process_irp_list(SMARTCARD_DEVICE* smartcard)
{
	IRP* irp;

	while (1)
	{
		if (WaitForSingleObject(smartcard->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		irp = (IRP*) InterlockedPopEntrySList(smartcard->pIrpList);

		if (irp == NULL)
			break;

		smartcard_process_irp(smartcard, irp);
	}
}

struct _SMARTCARD_IRP_WORKER
{
	SMARTCARD_DEVICE* smartcard;
	IRP* irp;
	HANDLE thread;
};
typedef struct _SMARTCARD_IRP_WORKER SMARTCARD_IRP_WORKER;
 
static void *smartcard_process_irp_thread_func(SMARTCARD_IRP_WORKER* irpWorker)
{
	smartcard_process_irp(irpWorker->smartcard, irpWorker->irp);

	CloseHandle(irpWorker->thread);

	free(irpWorker);

	ExitThread(0);
	return NULL;
}

static void* smartcard_thread_func(void* arg)
{
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) arg;
	HANDLE ev[] = {smartcard->irpEvent, smartcard->stopEvent};

	while (1)
	{
		DWORD status = WaitForMultipleObjects(2, ev, FALSE, INFINITE);

		if (status == WAIT_OBJECT_0 + 1)
			break;
		else if(status != WAIT_OBJECT_0)
			continue;

		ResetEvent(smartcard->irpEvent);
		smartcard_process_irp_list(smartcard);
	}

	return NULL;
}

/* Begin TS Client defect workaround. */
static COMPLETIONIDINFO* smartcard_mark_duplicate_id(SMARTCARD_DEVICE* smartcard, UINT32 CompletionId)
{
	LIST_ITEM* item;
	COMPLETIONIDINFO* CompletionIdInfo;

	/*
	 * Search from the beginning of the LIST for one outstanding "CompletionID"
	 * that matches the one passed in.  If there is one, mark it as a duplicate
	 * if it is not already marked.
	 */

	for (item = smartcard->CompletionIds->head; item; item = item->next)
	{
	        CompletionIdInfo = (COMPLETIONIDINFO*) item->data;

	        if (CompletionIdInfo->ID == CompletionId)
	        {
	                if (!CompletionIdInfo->duplicate)
	                {
	                        CompletionIdInfo->duplicate = TRUE;
	                        DEBUG_WARN("CompletionID number %u is now marked as a duplicate.", CompletionId);
	                }

	                return CompletionIdInfo;
	        }
	}

	return NULL;    /* Either no items in the list or no match. */
}

static BOOL smartcard_check_for_duplicate_id(SMARTCARD_DEVICE* smartcard, UINT32 CompletionId)
{
	BOOL duplicate;
	LIST_ITEM* item;
	COMPLETIONIDINFO* CompletionIdInfo;

	/*
	 * Search from the end of the LIST for one outstanding "CompletionID"
	 * that matches the one passed in.  Remove it from the list and free the
	 * memory associated with it.  Return whether or not it was marked
	 * as a duplicate.
	 */

	for (item = smartcard->CompletionIds->tail; item; item = item->prev)
	{
	        CompletionIdInfo = (COMPLETIONIDINFO*) item->data;

	        if (CompletionIdInfo->ID == CompletionId)
	        {
	                duplicate = CompletionIdInfo->duplicate;

	                if (duplicate)
	                {
	                        DEBUG_WARN("CompletionID number %u was previously marked as a duplicate.", CompletionId);
	                }

	                list_remove(smartcard->CompletionIds, CompletionIdInfo);
	                free(CompletionIdInfo);

	                return duplicate;
	        }
	}

	/* This function should only be called when there is
	 * at least one outstanding CompletionID item in the list.
	 */
	DEBUG_WARN("Error!!! No CompletionIDs (or no matching IDs) in the list!");

	return FALSE;
}

static void smartcard_irp_complete(IRP* irp)
{
	int pos;
	BOOL duplicate;
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) irp->device;

	/* This function is (mostly) a copy of the statically-declared "irp_complete()"
	 * function except that this function adds extra operations for the
	 * smart card's handling of duplicate "CompletionID"s.  This function needs
	 * to be in this file so that "smartcard_irp_request()" can reference it.
	 */

	DEBUG_SVC("DeviceId %d FileId %d CompletionId %d", irp->device->id, irp->FileId, irp->CompletionId);

	pos = Stream_GetPosition(irp->output);
	Stream_SetPosition(irp->output, 12);
	Stream_Write_UINT32(irp->output, irp->IoStatus);
	Stream_SetPosition(irp->output, pos);

	/* Begin TS Client defect workaround. */
	WaitForSingleObject(smartcard->CompletionIdsMutex, INFINITE);
	/* Remove from the list the item identified by the CompletionID.
	 * The function returns whether or not it was a duplicate CompletionID.
	 */
	duplicate = smartcard_check_for_duplicate_id(smartcard, irp->CompletionId);
	ReleaseMutex(smartcard->CompletionIdsMutex);

	if (!duplicate)
	{
	        svc_plugin_send(irp->devman->plugin, irp->output);
		irp->output = NULL;
	}

	/* End TS Client defect workaround. */

	/* irp_free(irp); 	The "irp_free()" function is statically-declared
	 * 			and so is not available to be called
	 * 			here.  Instead, call it indirectly by calling
	 * 			the IRP's "Discard()" function,
	 * 			which has already been assigned 
	 * 			to point to "irp_free()" in "irp_new()".
	 */
	irp->Discard(irp);
}
/* End TS Client defect workaround. */

static void smartcard_irp_request(DEVICE* device, IRP* irp)
{
	COMPLETIONIDINFO* CompletionIdInfo;
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;

	/* Begin TS Client defect workaround. */

	CompletionIdInfo = (COMPLETIONIDINFO*) malloc(sizeof(COMPLETIONIDINFO));
	ZeroMemory(CompletionIdInfo, sizeof(COMPLETIONIDINFO));

	CompletionIdInfo->ID = irp->CompletionId;

	WaitForSingleObject(smartcard->CompletionIdsMutex, INFINITE);
	smartcard_mark_duplicate_id(smartcard, irp->CompletionId);
	list_enqueue(smartcard->CompletionIds, CompletionIdInfo);
	ReleaseMutex(smartcard->CompletionIdsMutex);

	/* Overwrite the previous assignment made in irp_new() */
	irp->Complete = smartcard_irp_complete;

	/* End TS Client defect workaround. */

	if ((irp->MajorFunction == IRP_MJ_DEVICE_CONTROL) && smartcard_async_op(irp))
	{
		/* certain potentially long running operations get their own thread */
		SMARTCARD_IRP_WORKER* irpWorker = malloc(sizeof(SMARTCARD_IRP_WORKER));

		irpWorker->thread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) smartcard_process_irp_thread_func,
				irpWorker, CREATE_SUSPENDED, NULL);

		irpWorker->smartcard = smartcard;
		irpWorker->irp = irp;

		ResumeThread(irpWorker->thread);

		return;
	}

	InterlockedPushEntrySList(smartcard->pIrpList, &(irp->ItemEntry));

	SetEvent(smartcard->irpEvent);
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	smartcard_DeviceServiceEntry
#endif

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	char* name;
	char* path;
	int i, length;
	RDPDR_SMARTCARD* device;
	SMARTCARD_DEVICE* smartcard;

	device = (RDPDR_SMARTCARD*) pEntryPoints->device;
	name = device->Name;
	path = device->Path;

	if (name)
	{
		/* TODO: check if server supports sc redirect (version 5.1) */

		smartcard = (SMARTCARD_DEVICE*) malloc(sizeof(SMARTCARD_DEVICE));
		ZeroMemory(smartcard, sizeof(SMARTCARD_DEVICE));

		smartcard->device.type = RDPDR_DTYP_SMARTCARD;
		smartcard->device.name = "SCARD";
		smartcard->device.IRPRequest = smartcard_irp_request;
		smartcard->device.Free = smartcard_free;

		length = strlen(smartcard->device.name);
		smartcard->device.data = Stream_New(NULL, length + 1);

		for (i = 0; i <= length; i++)
			Stream_Write_UINT8(smartcard->device.data, name[i] < 0 ? '_' : name[i]);

		smartcard->path = path;

		smartcard->pIrpList = (PSLIST_HEADER) _aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
		InitializeSListHead(smartcard->pIrpList);

		smartcard->irpEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		smartcard->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		smartcard->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) smartcard_thread_func,
				smartcard, CREATE_SUSPENDED, NULL);

		smartcard->CompletionIds = list_new();
		smartcard->CompletionIdsMutex = CreateMutex(NULL, FALSE, NULL);

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) smartcard);

		ResumeThread(smartcard->thread);
	}

	return 0;
}

