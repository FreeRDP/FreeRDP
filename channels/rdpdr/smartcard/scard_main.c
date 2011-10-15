/*
   FreeRDP: A Remote Desktop Protocol client.
   Redirected Smart Card Device Service

   Copyright 2011 O.S. Systems Software Ltda.
   Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
   Copyright 2011 Anthony Tong <atong@trustedcs.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/utils/list.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_types.h"
#include "rdpdr_constants.h"

#include "scard_main.h"


static void
scard_free(DEVICE* dev)
{
	SCARD_DEVICE* scard = (SCARD_DEVICE*)dev;
	IRP* irp;

	freerdp_thread_stop(scard->thread);
	freerdp_thread_free(scard->thread);
	
	while ((irp = (IRP*)list_dequeue(scard->irp_list)) != NULL)
		irp->Discard(irp);
	list_free(scard->irp_list);

	xfree(dev);
	return;
}


static void
scard_process_irp(SCARD_DEVICE* scard, IRP* irp)
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


static void
scard_process_irp_list(SCARD_DEVICE* scard)
{
	IRP *irp;

	while (!freerdp_thread_is_stopped(scard->thread))
	{
		freerdp_thread_lock(scard->thread);
		irp = (IRP *) list_dequeue(scard->irp_list);
		freerdp_thread_unlock(scard->thread);

		if (irp == NULL)
			break;

		scard_process_irp(scard, irp);
	}
}


struct scard_irp_thread_args {
	SCARD_DEVICE* scard;
	IRP* irp;
	freerdp_thread* thread;
};
 

static void
scard_process_irp_thread_func(struct scard_irp_thread_args* args)
{
	scard_process_irp(args->scard, args->irp);

	freerdp_thread_free(args->thread);
	xfree(args);
}


static void *
scard_thread_func(void* arg)
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


static void
scard_irp_request(DEVICE* device, IRP* irp)
{
	SCARD_DEVICE* scard = (SCARD_DEVICE*)device;

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

	freerdp_thread_lock(scard->thread);
	list_enqueue(scard->irp_list, irp);
	freerdp_thread_unlock(scard->thread);

	freerdp_thread_signal(scard->thread);
}


int
DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	SCARD_DEVICE* scard;
	char* name;
	char* path;
	int i, length;

	name = (char *)pEntryPoints->plugin_data->data[1];
	path = (char *)pEntryPoints->plugin_data->data[2];

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

		scard->irp_list = list_new();
		scard->thread = freerdp_thread_new();

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE *)scard);

		freerdp_thread_start(scard->thread, scard_thread_func, scard);
	}

	return 0;
}
