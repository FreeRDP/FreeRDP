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
#include <winpr/smartcard.h>

#include <freerdp/utils/list.h>
#include <freerdp/utils/debug.h>

#include <freerdp/channels/rdpdr.h>

#include "smartcard_main.h"

static void smartcard_free(DEVICE* device)
{
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;

	MessageQueue_PostQuit(smartcard->IrpQueue, 0);
	WaitForSingleObject(smartcard->thread, INFINITE);

	CloseHandle(smartcard->thread);

	Stream_Free(smartcard->device.data, TRUE);

	free(device);
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

static void* smartcard_thread_func(void* arg)
{
	IRP* irp;
	wMessage message;
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) arg;

	while (1)
	{
		if (!MessageQueue_Wait(smartcard->IrpQueue))
			break;

		if (!MessageQueue_Peek(smartcard->IrpQueue, &message, TRUE))
			break;

		if (message.id == WMQ_QUIT)
			break;

		irp = (IRP*) message.wParam;

		if (irp)
			smartcard_process_irp(smartcard, irp);
	}

	ExitThread(0);
	return NULL;
}

static void smartcard_irp_request(DEVICE* device, IRP* irp)
{
	SMARTCARD_DEVICE* smartcard = (SMARTCARD_DEVICE*) device;
	MessageQueue_Post(smartcard->IrpQueue, NULL, 0, (void*) irp, NULL);
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	smartcard_DeviceServiceEntry
#endif

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

	/* TODO: check if server supports sc redirect (version 5.1) */
	smartcard = (SMARTCARD_DEVICE*) calloc(1, sizeof(SMARTCARD_DEVICE));

	if (!smartcard)
		return -1;

	smartcard->device.type = RDPDR_DTYP_SMARTCARD;
	smartcard->device.name = "SCARD";
	smartcard->device.IRPRequest = smartcard_irp_request;
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
	smartcard->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) smartcard_thread_func,
			smartcard, CREATE_SUSPENDED, NULL);

	pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) smartcard);

	ResumeThread(smartcard->thread);

	return 0;
}

