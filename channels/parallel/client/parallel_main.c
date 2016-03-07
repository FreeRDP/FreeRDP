/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Redirected Parallel Port Device Service
 *
 * Copyright 2010 O.S. Systems Software Ltda.
 * Copyright 2010 Eduardo Fiss Beloni <beloni@ossystems.com.br>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/stream.h>
#include <winpr/collections.h>
#include <winpr/interlocked.h>

#include <freerdp/types.h>
#include <freerdp/constants.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("drive.client")

struct _PARALLEL_DEVICE
{
	DEVICE device;

	int file;
	char* path;
	UINT32 id;

	HANDLE thread;
	wMessageQueue* queue;
	rdpContext* rdpcontext;
};
typedef struct _PARALLEL_DEVICE PARALLEL_DEVICE;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_process_irp_create(PARALLEL_DEVICE* parallel, IRP* irp)
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
		if (!(path = (char*) calloc(1, 1)))
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

	parallel->id = irp->devman->id_sequence++;
	parallel->file = open(parallel->path, O_RDWR);

	if (parallel->file < 0)
	{
		irp->IoStatus = STATUS_ACCESS_DENIED;
		parallel->id = 0;
	}
	else
	{
		/* all read and write operations should be non-blocking */
		if (fcntl(parallel->file, F_SETFL, O_NONBLOCK) == -1)
		{

		}
	}

	Stream_Write_UINT32(irp->output, parallel->id);
	Stream_Write_UINT8(irp->output, 0);

	free(path);

	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_process_irp_close(PARALLEL_DEVICE* parallel, IRP* irp)
{
	if (close(parallel->file) < 0)
	{

	}
	else
	{

	}

	Stream_Zero(irp->output, 5); /* Padding(5) */

	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_process_irp_read(PARALLEL_DEVICE* parallel, IRP* irp)
{
	UINT32 Length;
	UINT64 Offset;
	ssize_t status;
	BYTE* buffer = NULL;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);

	buffer = (BYTE*) malloc(Length);
	if (!buffer)
	{
		WLog_ERR(TAG, "malloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	status = read(parallel->file, buffer, Length);

	if (status < 0)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		free(buffer);
		buffer = NULL;
		Length = 0;
	}
	else
	{

	}

	Stream_Write_UINT32(irp->output, Length);

	if (Length > 0)
	{
		if (!Stream_EnsureRemainingCapacity(irp->output, Length))
		{
			WLog_ERR(TAG, "Stream_EnsureRemainingCapacity failed!");
			free(buffer);
			return CHANNEL_RC_NO_MEMORY;
		}
		Stream_Write(irp->output, buffer, Length);
	}

	free(buffer);

	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_process_irp_write(PARALLEL_DEVICE* parallel, IRP* irp)
{
	UINT32 len;
	UINT32 Length;
	UINT64 Offset;
	ssize_t status;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	Stream_Seek(irp->input, 20); /* Padding */

	len = Length;

	while (len > 0)
	{
		status = write(parallel->file, Stream_Pointer(irp->input), len);

		if (status < 0)
		{
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			Length = 0;
			break;
		}

		Stream_Seek(irp->input, status);
		len -= status;
	}

	Stream_Write_UINT32(irp->output, Length);
	Stream_Write_UINT8(irp->output, 0); /* Padding */

	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_process_irp_device_control(PARALLEL_DEVICE* parallel, IRP* irp)
{
	Stream_Write_UINT32(irp->output, 0); /* OutputBufferLength */
	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_process_irp(PARALLEL_DEVICE* parallel, IRP* irp)
{
	UINT error;

	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			if ((error = parallel_process_irp_create(parallel, irp)))
			{
				WLog_ERR(TAG, "parallel_process_irp_create failed with error %d!", error);
				return error;
			}
			break;

		case IRP_MJ_CLOSE:
			if ((error = parallel_process_irp_close(parallel, irp)))
			{
				WLog_ERR(TAG, "parallel_process_irp_close failed with error %d!", error);
				return error;
			}
			break;

		case IRP_MJ_READ:
			if ((error = parallel_process_irp_read(parallel, irp)))
			{
				WLog_ERR(TAG, "parallel_process_irp_read failed with error %d!", error);
				return error;
			}
			break;

		case IRP_MJ_WRITE:
			if ((error = parallel_process_irp_write(parallel, irp)))
			{
				WLog_ERR(TAG, "parallel_process_irp_write failed with error %d!", error);
				return error;
			}
			break;

		case IRP_MJ_DEVICE_CONTROL:
			if ((error = parallel_process_irp_device_control(parallel, irp)))
			{
				WLog_ERR(TAG, "parallel_process_irp_device_control failed with error %d!", error);
				return error;
			}
			break;

		default:
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			return irp->Complete(irp);
			break;
	}
	return CHANNEL_RC_OK;
}

static void* parallel_thread_func(void* arg)
{
	IRP* irp;
	wMessage message;
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) arg;
	UINT error = CHANNEL_RC_OK;

	while (1)
	{
		if (!MessageQueue_Wait(parallel->queue))
		{
			WLog_ERR(TAG, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (!MessageQueue_Peek(parallel->queue, &message, TRUE))
		{
			WLog_ERR(TAG, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (message.id == WMQ_QUIT)
			break;

		irp = (IRP*) message.wParam;

		if ((error = parallel_process_irp(parallel, irp)))
		{
			WLog_ERR(TAG, "parallel_process_irp failed with error %d!", error);
			break;
		}
	}
	if (error && parallel->rdpcontext)
		setChannelError(parallel->rdpcontext, error, "parallel_thread_func reported an error");

	ExitThread((DWORD)error);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_irp_request(DEVICE* device, IRP* irp)
{
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) device;

	if (!MessageQueue_Post(parallel->queue, NULL, 0, (void*) irp, NULL))
	{
		WLog_ERR(TAG, "MessageQueue_Post failed!");
		return ERROR_INTERNAL_ERROR;
	}
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_free(DEVICE* device)
{
    UINT error;
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*) device;

	if (MessageQueue_PostQuit(parallel->queue, 0) && (WaitForSingleObject(parallel->thread, INFINITE) == WAIT_FAILED))
    {
        error = GetLastError();
        WLog_ERR(TAG, "WaitForSingleObject failed with error %lu!", error);
        return error;
    }
	CloseHandle(parallel->thread);

	Stream_Free(parallel->device.data, TRUE);
	MessageQueue_Free(parallel->queue);

	free(parallel);
    return CHANNEL_RC_OK;
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	parallel_DeviceServiceEntry
#else
#define DeviceServiceEntry	FREERDP_API DeviceServiceEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	char* name;
	char* path;
	int i;
	size_t length;
	RDPDR_PARALLEL* device;
	PARALLEL_DEVICE* parallel;
	UINT error;

	device = (RDPDR_PARALLEL*) pEntryPoints->device;
	name = device->Name;
	path = device->Path;

	if (!name || (name[0] == '*'))
	{
		/* TODO: implement auto detection of parallel ports */
		return CHANNEL_RC_OK;
	}

	if (name[0] && path[0])
	{
		parallel = (PARALLEL_DEVICE*) calloc(1, sizeof(PARALLEL_DEVICE));
		if (!parallel)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		parallel->device.type = RDPDR_DTYP_PARALLEL;
		parallel->device.name = name;
		parallel->device.IRPRequest = parallel_irp_request;
		parallel->device.Free = parallel_free;
		parallel->rdpcontext = pEntryPoints->rdpcontext;

		length = strlen(name);
		parallel->device.data = Stream_New(NULL, length + 1);
		if (!parallel->device.data)
		{
			WLog_ERR(TAG, "Stream_New failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}

		for (i = 0; i <= length; i++)
			Stream_Write_UINT8(parallel->device.data, name[i] < 0 ? '_' : name[i]);

		parallel->path = path;

		parallel->queue = MessageQueue_New(NULL);
		if (!parallel->queue)
		{
			WLog_ERR(TAG, "MessageQueue_New failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}

		if ((error = pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) parallel)))
		{
			WLog_ERR(TAG, "RegisterDevice failed with error %lu!", error);
			goto error_out;
		}


		if (!(parallel->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) parallel_thread_func, (void*) parallel, 0, NULL)))
		{
			WLog_ERR(TAG, "CreateThread failed!");
			error = ERROR_INTERNAL_ERROR;
			goto error_out;
		}
	}

	return CHANNEL_RC_OK;
error_out:
	MessageQueue_Free(parallel->queue);
	Stream_Free(parallel->device.data, TRUE);
	free(parallel);
	return error;
}
