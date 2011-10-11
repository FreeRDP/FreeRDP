/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Redirected Serial Port Device Service
 *
 * Copyright 2010 O.S. Systems Software Ltda.
 * Copyright 2010 Eduardo Fiss Beloni <beloni@ossystems.com.br>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_constants.h"
#include "rdpdr_types.h"

struct _SERIAL_DEVICE
{
	DEVICE device;

	char* path;

	LIST* irp_list;
	freerdp_thread* thread;
};
typedef struct _SERIAL_DEVICE SERIAL_DEVICE;

static void serial_process_irp_create(SERIAL_DEVICE* serial, IRP* irp)
{
	irp->Complete(irp);
}

static void serial_process_irp_close(SERIAL_DEVICE* serial, IRP* irp)
{
	irp->Complete(irp);
}

static void serial_process_irp_read(SERIAL_DEVICE* serial, IRP* irp)
{
	irp->Complete(irp);
}

static void serial_process_irp_write(SERIAL_DEVICE* serial, IRP* irp)
{
	irp->Complete(irp);
}

static void serial_process_irp_device_control(SERIAL_DEVICE* serial, IRP* irp)
{
	irp->Complete(irp);
}

static void serial_process_irp(SERIAL_DEVICE* serial, IRP* irp)
{
	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			serial_process_irp_create(serial, irp);
			break;

		case IRP_MJ_CLOSE:
			serial_process_irp_close(serial, irp);
			break;

		case IRP_MJ_READ:
			serial_process_irp_read(serial, irp);
			break;

		case IRP_MJ_WRITE:
			serial_process_irp_write(serial, irp);
			break;

		case IRP_MJ_DEVICE_CONTROL:
			serial_process_irp_device_control(serial, irp);
			break;

		default:
			DEBUG_WARN("MajorFunction 0x%X not supported", irp->MajorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void serial_process_irp_list(SERIAL_DEVICE* serial)
{
	IRP* irp;

	while (1)
	{
		if (freerdp_thread_is_stopped(serial->thread))
			break;

		freerdp_thread_lock(serial->thread);
		irp = (IRP*) list_dequeue(serial->irp_list);
		freerdp_thread_unlock(serial->thread);

		if (irp == NULL)
			break;

		serial_process_irp(serial, irp);
	}
}

static void* serial_thread_func(void* arg)
{
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*) arg;

	while (1)
	{
		freerdp_thread_wait(serial->thread);

		if (freerdp_thread_is_stopped(serial->thread))
			break;

		freerdp_thread_reset(serial->thread);
		serial_process_irp_list(serial);
	}

	freerdp_thread_quit(serial->thread);

	return NULL;
}

static void serial_irp_request(DEVICE* device, IRP* irp)
{
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*) device;

	freerdp_thread_lock(serial->thread);
	list_enqueue(serial->irp_list, irp);
	freerdp_thread_unlock(serial->thread);

	freerdp_thread_signal(serial->thread);
}

static void serial_free(DEVICE* device)
{
	IRP* irp;
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*) device;

	freerdp_thread_stop(serial->thread);
	freerdp_thread_free(serial->thread);

	while ((irp = (IRP*) list_dequeue(serial->irp_list)) != NULL)
		irp->Discard(irp);

	list_free(serial->irp_list);

	xfree(serial);
}

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	char* name;
	char* path;
	int i, length;
	SERIAL_DEVICE* serial;

	name = (char*) pEntryPoints->plugin_data->data[1];
	path = (char*) pEntryPoints->plugin_data->data[2];

	if (name[0] && path[0])
	{
		serial = xnew(SERIAL_DEVICE);

		serial->device.type = RDPDR_DTYP_SERIAL;
		serial->device.name = name;
		serial->device.IRPRequest = serial_irp_request;
		serial->device.Free = serial_free;

		length = strlen(name);
		serial->device.data = stream_new(length + 1);

		for (i = 0; i <= length; i++)
			stream_write_uint8(serial->device.data, name[i] < 0 ? '_' : name[i]);

		serial->path = path;

		serial->irp_list = list_new();
		serial->thread = freerdp_thread_new();

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) serial);

		freerdp_thread_start(serial->thread, serial_thread_func, serial);
	}

	return 0;
}
