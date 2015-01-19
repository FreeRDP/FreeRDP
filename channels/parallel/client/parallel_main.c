/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <fcntl.h>
#include <errno.h>

#ifndef _WIN32
#include <termios.h>
#include <strings.h>
#include <sys/ioctl.h>
#endif

#ifdef __LINUX__
#include <linux/ppdev.h>
#include <linux/parport.h>
#endif

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/collections.h>
#include <winpr/interlocked.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <winpr/stream.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/channels/rdpdr.h>

struct _PARALLEL_DEVICE
{
	DEVICE device;

	int file;
	char* path;
	UINT32 id;

	HANDLE thread;
	wMessageQueue* queue;
};
typedef struct _PARALLEL_DEVICE PARALLEL_DEVICE;

static void parallel_process_irp_create(PARALLEL_DEVICE* parallel, IRP* irp)
{
	char* path = NULL;
	int status;
	UINT32 PathLength;

	Stream_Seek(irp->input, 28);
	/* DesiredAccess(4) AllocationSize(8), FileAttributes(4) */
	/* SharedAccess(4) CreateDisposition(4), CreateOptions(4) */
	Stream_Read_UINT32(irp->input, PathLength);

	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(irp->input),
			PathLength / 2, &path, 0, NULL, NULL);

	if (status < 1)
		path = (char*) calloc(1, 1);

	parallel->id = irp->devman->id_sequence++;
	parallel->file = open(parallel->path, O_RDWR);

	if (parallel->file < 0)
	{
		irp->IoStatus = STATUS_ACCESS_DENIED;
		parallel->id = 0;

		DEBUG_WARN("failed to create %s: %s", parallel->path, strerror(errno));
	}
	else
	{
		/* all read and write operations should be non-blocking */
		if (fcntl(parallel->file, F_SETFL, O_NONBLOCK) == -1)
			DEBUG_WARN("%s fcntl %s", path, strerror(errno));

		DEBUG_SVC("%s(%d) created", parallel->path, parallel->file);
	}

	Stream_Write_UINT32(irp->output, parallel->id);
	Stream_Write_UINT8(irp->output, 0);

	free(path);

	irp->Complete(irp);
}

static void parallel_process_irp_close(PARALLEL_DEVICE* parallel, IRP* irp)
{
	if (close(parallel->file) < 0)
		DEBUG_SVC("failed to close %s(%d)", parallel->path, parallel->id);
	else
		DEBUG_SVC("%s(%d) closed", parallel->path, parallel->id);

	Stream_Zero(irp->output, 5); /* Padding(5) */

	irp->Complete(irp);
}

static void parallel_process_irp_read(PARALLEL_DEVICE* parallel, IRP* irp)
{
	UINT32 Length;
	UINT64 Offset;
	ssize_t status;
	BYTE* buffer = NULL;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);

	buffer = (BYTE*) malloc(Length);

	status = read(parallel->file, buffer, Length);

	if (status < 0)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		free(buffer);
		buffer = NULL;
		Length = 0;

		DEBUG_WARN("read %s(%d) failed", parallel->path, parallel->id);
	}
	else
	{
		DEBUG_SVC("read %llu-%llu from %d", Offset, Offset + Length, parallel->id);
	}

	Stream_Write_UINT32(irp->output, Length);

	if (Length > 0)
	{
		Stream_EnsureRemainingCapacity(irp->output, Length);
		Stream_Write(irp->output, buffer, Length);
	}

	free(buffer);

	irp->Complete(irp);
}

static void parallel_process_irp_write(PARALLEL_DEVICE* parallel, IRP* irp)
{
	UINT32 len;
	UINT32 Length;
	UINT64 Offset;
	ssize_t status;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	Stream_Seek(irp->input, 20); /* Padding */

	DEBUG_SVC("Length %u Offset %llu", Length, Offset);

	len = Length;

	while (len > 0)
	{
		status = write(parallel->file, Stream_Pointer(irp->input), len);

		if (status < 0)
		{
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			Length = 0;

			DEBUG_WARN("write %s(%d) failed.", parallel->path, parallel->id);
			break;
		}

		Stream_Seek(irp->input, status);
		len -= status;
	}

	Stream_Write_UINT32(irp->output, Length);
	Stream_Write_UINT8(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void parallel_process_irp_device_control(PARALLEL_DEVICE* parallel, IRP* irp)
{
	DEBUG_SVC("in");
	Stream_Write_UINT32(irp->output, 0); /* OutputBufferLength */
	irp->Complete(irp);
}

static void parallel_process_irp(PARALLEL_DEVICE* parallel, IRP* irp)
{
	DEBUG_SVC("MajorFunction %u", irp->MajorFunction);

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

static void* parallel_thread_func(void* arg)
{
	IRP* irp;
	wMessage message;
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) arg;

	while (1)
	{
		if (!MessageQueue_Wait(parallel->queue))
			break;

		if (!MessageQueue_Peek(parallel->queue, &message, TRUE))
			break;

		if (message.id == WMQ_QUIT)
			break;

		irp = (IRP*) message.wParam;

		parallel_process_irp(parallel, irp);
	}

	return NULL;
}

static void parallel_irp_request(DEVICE* device, IRP* irp)
{
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) device;

	MessageQueue_Post(parallel->queue, NULL, 0, (void*) irp, NULL);
}

static void parallel_free(DEVICE* device)
{
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) device;

	DEBUG_SVC("freeing device");

	MessageQueue_PostQuit(parallel->queue, 0);
	WaitForSingleObject(parallel->thread, INFINITE);
	CloseHandle(parallel->thread);

	Stream_Free(parallel->device.data, TRUE);
	MessageQueue_Free(parallel->queue);

	free(parallel);
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	parallel_DeviceServiceEntry
#endif

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	char* name;
	char* path;
	int i, length;
	RDPDR_PARALLEL* device;
	PARALLEL_DEVICE* parallel;

	device = (RDPDR_PARALLEL*) pEntryPoints->device;
	name = device->Name;
	path = device->Path;

	if (!name || (name[0] == '*'))
	{
		/* TODO: implement auto detection of parallel ports */
		return 0;
	}

	if (name[0] && path[0])
	{
		parallel = (PARALLEL_DEVICE*) calloc(1, sizeof(PARALLEL_DEVICE));

		if (!parallel)
			return -1;

		parallel->device.type = RDPDR_DTYP_PARALLEL;
		parallel->device.name = name;
		parallel->device.IRPRequest = parallel_irp_request;
		parallel->device.Free = parallel_free;

		length = strlen(name);
		parallel->device.data = Stream_New(NULL, length + 1);

		for (i = 0; i <= length; i++)
			Stream_Write_UINT8(parallel->device.data, name[i] < 0 ? '_' : name[i]);

		parallel->path = path;

		parallel->queue = MessageQueue_New(NULL);

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) parallel);

		parallel->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) parallel_thread_func, (void*) parallel, 0, NULL);
	}

	return 0;
}
