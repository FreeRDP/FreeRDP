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

#include <freerdp/utils/list.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/debug.h>
#include <freerdp/channels/rdpdr.h>

#include "scard_main.h"

static void scard_free(DEVICE* dev)
{
	IRP* irp;
	COMPLETIONIDINFO* CompletionIdInfo;
	SCARD_DEVICE* scard = (SCARD_DEVICE*) dev;

	freerdp_thread_stop(scard->thread);
	freerdp_thread_free(scard->thread);

	while ((irp = (IRP*) InterlockedPopEntrySList(scard->pIrpList)) != NULL)
		irp->Discard(irp);

	_aligned_free(scard->pIrpList);

	/* Begin TS Client defect workaround. */

	while ((CompletionIdInfo = (COMPLETIONIDINFO*) list_dequeue(scard->CompletionIds)) != NULL)
	        xfree(CompletionIdInfo);

	list_free(scard->CompletionIds);

	/* End TS Client defect workaround. */

	xfree(dev);
	return;
}

static void scard_process_irp(SCARD_DEVICE* scard, IRP* irp)
{
	switch (irp->MajorFunction)
	{
		case IRP_MJ_DEVICE_CONTROL:
			scard_device_control(scard, irp);
			break;

		default:
			printf("MajorFunction 0x%X unexpected for smartcards.", irp->MajorFunction);
			DEBUG_WARN("Smartcard MajorFunction 0x%X not supported.", irp->MajorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void scard_process_irp_list(SCARD_DEVICE* scard)
{
	IRP* irp;

	while (!freerdp_thread_is_stopped(scard->thread))
	{
		irp = (IRP*) InterlockedPopEntrySList(scard->pIrpList);

		if (irp == NULL)
			break;

		scard_process_irp(scard, irp);
	}
}

struct scard_irp_thread_args
{
	SCARD_DEVICE* scard;
	IRP* irp;
	freerdp_thread* thread;
};
 
static void scard_process_irp_thread_func(struct scard_irp_thread_args* args)
{
	scard_process_irp(args->scard, args->irp);

	freerdp_thread_free(args->thread);
	xfree(args);
}

static void* scard_thread_func(void* arg)
{
	SCARD_DEVICE* scard = (SCARD_DEVICE*) arg;

	while (1)
	{
		freerdp_thread_wait(scard->thread);

		if (freerdp_thread_is_stopped(scard->thread))
			break;

		freerdp_thread_reset(scard->thread);
		scard_process_irp_list(scard);
	}

	freerdp_thread_quit(scard->thread);

	return NULL;
}

/* Begin TS Client defect workaround. */
static COMPLETIONIDINFO* scard_mark_duplicate_id(SCARD_DEVICE* scard, uint32 CompletionId)
{
	/*
	 * Search from the beginning of the LIST for one outstanding "CompletionID"
	 * that matches the one passed in.  If there is one, mark it as a duplicate
	 * if it is not already marked.
	 */
	LIST_ITEM* item;
	COMPLETIONIDINFO* CompletionIdInfo;

	for (item = scard->CompletionIds->head; item; item = item->next)
	{
	        CompletionIdInfo = (COMPLETIONIDINFO*)item->data;
	        if (CompletionIdInfo->ID == CompletionId)
	        {
	                if (false == CompletionIdInfo->duplicate)
	                {
	                        CompletionIdInfo->duplicate = true;
	                        DEBUG_WARN("CompletionID number %u is now marked as a duplicate.", CompletionId);
	                }
	                return CompletionIdInfo;
	        }
	}

	return NULL;    /* Either no items in the list or no match. */
}

static boolean  scard_check_for_duplicate_id(SCARD_DEVICE* scard, uint32 CompletionId)
{
	/*
	 * Search from the end of the LIST for one outstanding "CompletionID"
	 * that matches the one passed in.  Remove it from the list and free the
	 * memory associated with it.  Return whether or not it was marked
	 * as a duplicate.
	 */
	LIST_ITEM* item;
	COMPLETIONIDINFO* CompletionIdInfo;
	boolean duplicate;

	for (item = scard->CompletionIds->tail; item; item = item->prev)
	{
	        CompletionIdInfo = (COMPLETIONIDINFO*)item->data;
	        if (CompletionIdInfo->ID == CompletionId)
	        {
	                duplicate = CompletionIdInfo->duplicate;
	                if (true == duplicate)
	                {
	                        DEBUG_WARN("CompletionID number %u was previously marked as a duplicate.  The response to the command is removed.", CompletionId);
	                }
	                list_remove(scard->CompletionIds, CompletionIdInfo);
	                xfree(CompletionIdInfo);
	                return duplicate;
	        }
	}

	/* This function should only be called when there is
	 * at least one outstanding CompletionID item in the list.
	 */
	DEBUG_WARN("Error!!! No CompletionIDs (or no matching IDs) in the list!");

	return false;
}

static void  scard_irp_complete(IRP* irp)
{
	/* This function is (mostly) a copy of the statically-declared "irp_complete()"
	 * function except that this function adds extra operations for the
	 * smart card's handling of duplicate "CompletionID"s.  This function needs
	 * to be in this file so that "scard_irp_request()" can reference it.
	 */
	int pos;
	boolean duplicate;
	SCARD_DEVICE* scard = (SCARD_DEVICE*)irp->device;

	DEBUG_SVC("DeviceId %d FileId %d CompletionId %d", irp->device->id, irp->FileId, irp->CompletionId);

	pos = stream_get_pos(irp->output);
	stream_set_pos(irp->output, 12);
	stream_write_uint32(irp->output, irp->IoStatus);
	stream_set_pos(irp->output, pos);

	/* Begin TS Client defect workaround. */
	WaitForSingleObject(scard->CompletionIdsMutex, INFINITE);
	/* Remove from the list the item identified by the CompletionID.
	 * The function returns whether or not it was a duplicate CompletionID.
	 */
	duplicate = scard_check_for_duplicate_id(scard, irp->CompletionId);
	ReleaseMutex(scard->CompletionIdsMutex);

	if (false == duplicate)
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

static void scard_irp_request(DEVICE* device, IRP* irp)
{
	COMPLETIONIDINFO* CompletionIdInfo;
	SCARD_DEVICE* scard = (SCARD_DEVICE*) device;

	/* Begin TS Client defect workaround. */
	CompletionIdInfo= xnew(COMPLETIONIDINFO);
	CompletionIdInfo->ID = irp->CompletionId;/* "duplicate" member is set 
	                                          * to false by "xnew()"
	                                          */
	WaitForSingleObject(scard->CompletionIdsMutex, INFINITE);
	scard_mark_duplicate_id(scard, irp->CompletionId);
	list_enqueue(scard->CompletionIds, CompletionIdInfo);
	ReleaseMutex(scard->CompletionIdsMutex);

	irp->Complete = scard_irp_complete;	/* Overwrite the previous
						 * assignment made in 
						 * "irp_new()".
						 */
	/* End TS Client defect workaround. */

	if (irp->MajorFunction == IRP_MJ_DEVICE_CONTROL &&
			scard_async_op(irp))
	{
		/*
		 * certain potentially long running operations
		 * get their own thread
		 * TODO: revise this mechanism.. maybe worker pool
		 */
		struct scard_irp_thread_args *args = xmalloc(sizeof(struct scard_irp_thread_args));


		args->thread = freerdp_thread_new();
		args->scard = scard;
		args->irp = irp;
		freerdp_thread_start(args->thread, scard_process_irp_thread_func, args);

		return;
	}

	InterlockedPushEntrySList(scard->pIrpList, &(irp->ItemEntry));

	freerdp_thread_signal(scard->thread);
}

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	char* name;
	char* path;
	int i, length;
	SCARD_DEVICE* scard;

	name = (char*) pEntryPoints->plugin_data->data[1];
	path = (char*) pEntryPoints->plugin_data->data[2];

	if (name)
	{
		/* TODO: check if server supports sc redirect (version 5.1) */

		scard = xnew(SCARD_DEVICE);

		scard->device.type = RDPDR_DTYP_SMARTCARD;
		scard->device.name = "SCARD";
		scard->device.IRPRequest = scard_irp_request;
		scard->device.Free = scard_free;

		length = strlen(scard->device.name);
		scard->device.data = stream_new(length + 1);

		for (i = 0; i <= length; i++)
			stream_write_uint8(scard->device.data, name[i] < 0 ? '_' : name[i]);

		scard->path = path;

		scard->pIrpList = (PSLIST_HEADER) _aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
		InitializeSListHead(scard->pIrpList);

		scard->thread = freerdp_thread_new();

		scard->CompletionIds = list_new();
		scard->CompletionIdsMutex = CreateMutex(NULL, FALSE, NULL);

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE *)scard);

		freerdp_thread_start(scard->thread, scard_thread_func, scard);
	}

	return 0;
}
