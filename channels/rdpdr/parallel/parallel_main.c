/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Redirected Parallel Port Device Service
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

struct _PARALLEL_DEVICE
{
	DEVICE device;

	int file;
	char* path;

	LIST* irp_list;
	freerdp_thread* thread;
};
typedef struct _PARALLEL_DEVICE PARALLEL_DEVICE;

static int get_error_status()
{
	int status = 0;

	switch (errno)
	{
		case EAGAIN:
			status = STATUS_DEVICE_OFF_LINE;
			break;

		case ENOSPC:
			status = STATUS_DEVICE_PAPER_EMPTY;
			break;

		case EIO:
			status = STATUS_DEVICE_OFF_LINE;
			break;

		default:
			status = STATUS_DEVICE_POWERED_OFF;
			break;
	}

	return status;
}

static void parallel_process_irp_create(PARALLEL_DEVICE* parallel, IRP* irp)
{
	parallel->file = open(parallel->path, O_RDWR);

	if (parallel->file < 0)
	{
		perror("parallel open");
		irp->IoStatus = STATUS_ACCESS_DENIED;
		return;
	}

	/* all read and write operations should be non-blocking */
	if (fcntl(parallel->file, F_SETFL, O_NONBLOCK) == -1)
		perror("fcntl");

	irp->Complete(irp);
}

static void parallel_process_irp_close(PARALLEL_DEVICE* parallel, IRP* irp)
{
	close(parallel->file);
	irp->Complete(irp);
}

static void parallel_process_irp_read(PARALLEL_DEVICE* parallel, IRP* irp)
{
	uint32 length;
	uint64 offset;
	ssize_t status;

	stream_read_uint32(irp->input, length);
	stream_read_uint64(irp->input, offset);

	irp->output = stream_new(length);

	status = read(parallel->file, irp->output->p, length);

	if (status < 0)
	{
		stream_free(irp->output);
		irp->IoStatus = get_error_status();
		return;
	}

	irp->Complete(irp);
}

static void parallel_process_irp_write(PARALLEL_DEVICE* parallel, IRP* irp)
{
	uint32 length;
	uint64 offset;
	ssize_t status;

	stream_read_uint32(irp->input, length);
	stream_read_uint64(irp->input, offset);
	stream_seek(irp->input, 20); /* Padding */

	while (stream_get_left(irp->input))
	{
		status = write(parallel->file, irp->input->p, stream_get_left(irp->input));

		if (status < 0)
		{
			irp->IoStatus = get_error_status();
			return;
		}

		stream_seek(irp->input, status);
		length += status;
	}

	stream_write_uint32(irp->output, length);
	stream_write_uint8(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void parallel_process_irp_device_control(PARALLEL_DEVICE* parallel, IRP* irp)
{
	stream_write_uint32(irp->output, 0); /* OutputBufferLength */
	irp->Complete(irp);
}

static void parallel_process_irp(PARALLEL_DEVICE* parallel, IRP* irp)
{
	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			parallel_process_irp_create(parallel, irp);
			break;

		case IRP_MJ_CLOSE:
			parallel_process_irp_close(parallel, irp);
			break;

		case IRP_MJ_READ:
			parallel_process_irp_read(parallel, irp);
			break;

		case IRP_MJ_WRITE:
			parallel_process_irp_write(parallel, irp);
			break;

		case IRP_MJ_DEVICE_CONTROL:
			parallel_process_irp_device_control(parallel, irp);
			break;

		default:
			DEBUG_WARN("MajorFunction 0x%X not supported", irp->MajorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void parallel_process_irp_list(PARALLEL_DEVICE* parallel)
{
	IRP* irp;

	while (1)
	{
		if (freerdp_thread_is_stopped(parallel->thread))
			break;

		freerdp_thread_lock(parallel->thread);
		irp = (IRP*) list_dequeue(parallel->irp_list);
		freerdp_thread_unlock(parallel->thread);

		if (irp == NULL)
			break;

		parallel_process_irp(parallel, irp);
	}
}

static void* parallel_thread_func(void* arg)
{
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) arg;

	while (1)
	{
		freerdp_thread_wait(parallel->thread);

		if (freerdp_thread_is_stopped(parallel->thread))
			break;

		freerdp_thread_reset(parallel->thread);
		parallel_process_irp_list(parallel);
	}

	freerdp_thread_quit(parallel->thread);

	return NULL;
}

static void parallel_irp_request(DEVICE* device, IRP* irp)
{
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) device;

	freerdp_thread_lock(parallel->thread);
	list_enqueue(parallel->irp_list, irp);
	freerdp_thread_unlock(parallel->thread);

	freerdp_thread_signal(parallel->thread);
}

static void parallel_free(DEVICE* device)
{
	IRP* irp;
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) device;

	freerdp_thread_stop(parallel->thread);
	freerdp_thread_free(parallel->thread);

	while ((irp = (IRP*) list_dequeue(parallel->irp_list)) != NULL)
		irp->Discard(irp);

	list_free(parallel->irp_list);

	xfree(parallel);
}

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	char* name;
	char* path;
	int i, length;
	PARALLEL_DEVICE* parallel;

	name = (char*) pEntryPoints->plugin_data->data[1];
	path = (char*) pEntryPoints->plugin_data->data[2];

	if (name[0] && path[0])
	{
		parallel = xnew(PARALLEL_DEVICE);

		parallel->device.type = RDPDR_DTYP_PARALLEL;
		parallel->device.name = name;
		parallel->device.IRPRequest = parallel_irp_request;
		parallel->device.Free = parallel_free;

		length = strlen(name);
		parallel->device.data = stream_new(length + 1);

		for (i = 0; i <= length; i++)
			stream_write_uint8(parallel->device.data, name[i] < 0 ? '_' : name[i]);

		parallel->path = path;

		parallel->irp_list = list_new();
		parallel->thread = freerdp_thread_new();

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) parallel);

		freerdp_thread_start(parallel->thread, parallel_thread_func, parallel);
	}

	return 0;
}
