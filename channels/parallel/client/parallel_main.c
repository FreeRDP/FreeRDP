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

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include <winpr/assert.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/collections.h>
#include <winpr/interlocked.h>

#include <freerdp/types.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/channels/log.h>
#include <freerdp/utils/rdpdr_utils.h>

#define TAG CHANNELS_TAG("drive.client")

typedef struct
{
	DEVICE device;

	int file;
	char* path;
	UINT32 id;

	HANDLE thread;
	wMessageQueue* queue;
	rdpContext* rdpcontext;
	wLog* log;
} PARALLEL_DEVICE;

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_process_irp_create(PARALLEL_DEVICE* parallel, IRP* irp)
{
	char* path = NULL;
	UINT32 PathLength = 0;

	WINPR_ASSERT(parallel);
	WINPR_ASSERT(irp);

	if (!Stream_SafeSeek(irp->input, 28))
		return ERROR_INVALID_DATA;
	/* DesiredAccess(4) AllocationSize(8), FileAttributes(4) */
	/* SharedAccess(4) CreateDisposition(4), CreateOptions(4) */
	if (!Stream_CheckAndLogRequiredLength(TAG, irp->input, 4))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT32(irp->input, PathLength);
	if (PathLength < sizeof(WCHAR))
		return ERROR_INVALID_DATA;
	const WCHAR* ptr = Stream_ConstPointer(irp->input);
	if (!Stream_SafeSeek(irp->input, PathLength))
		return ERROR_INVALID_DATA;
	path = ConvertWCharNToUtf8Alloc(ptr, PathLength / sizeof(WCHAR), NULL);
	if (!path)
		return CHANNEL_RC_NO_MEMORY;

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
	WINPR_ASSERT(parallel);
	WINPR_ASSERT(irp);

	(void)close(parallel->file);

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
	UINT32 Length = 0;
	UINT64 Offset = 0;
	ssize_t status = 0;
	BYTE* buffer = NULL;

	WINPR_ASSERT(parallel);
	WINPR_ASSERT(irp);

	if (!Stream_CheckAndLogRequiredLength(TAG, irp->input, 12))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	(void)Offset; /* [MS-RDPESP] 3.2.5.1.4 Processing a Server Read Request Message
	               * ignored */
	buffer = (BYTE*)calloc(Length, sizeof(BYTE));

	if (!buffer)
	{
		WLog_Print(parallel->log, WLOG_ERROR, "malloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	status = read(parallel->file, buffer, Length);

	if ((status < 0) || (status > UINT32_MAX))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		free(buffer);
		buffer = NULL;
		Length = 0;
	}
	else
	{
		Length = (UINT32)status;
	}

	Stream_Write_UINT32(irp->output, Length);

	if (Length > 0)
	{
		if (!Stream_EnsureRemainingCapacity(irp->output, Length))
		{
			WLog_Print(parallel->log, WLOG_ERROR, "Stream_EnsureRemainingCapacity failed!");
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
	UINT32 len = 0;
	UINT32 Length = 0;
	UINT64 Offset = 0;

	WINPR_ASSERT(parallel);
	WINPR_ASSERT(irp);

	if (!Stream_CheckAndLogRequiredLength(TAG, irp->input, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	(void)Offset; /* [MS-RDPESP] 3.2.5.1.5 Processing a Server Write Request Message
	               * ignore offset */
	if (!Stream_SafeSeek(irp->input, 20)) /* Padding */
		return ERROR_INVALID_DATA;
	const void* ptr = Stream_ConstPointer(irp->input);
	if (!Stream_SafeSeek(irp->input, Length))
		return ERROR_INVALID_DATA;
	len = Length;

	while (len > 0)
	{
		const ssize_t status = write(parallel->file, ptr, len);

		if ((status < 0) || (status > len))
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
	WINPR_ASSERT(parallel);
	WINPR_ASSERT(irp);

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
	UINT error = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(parallel);
	WINPR_ASSERT(irp);

	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			error = parallel_process_irp_create(parallel, irp);
			break;

		case IRP_MJ_CLOSE:
			error = parallel_process_irp_close(parallel, irp);
			break;

		case IRP_MJ_READ:
			error = parallel_process_irp_read(parallel, irp);
			break;

		case IRP_MJ_WRITE:
			error = parallel_process_irp_write(parallel, irp);
			break;

		case IRP_MJ_DEVICE_CONTROL:
			error = parallel_process_irp_device_control(parallel, irp);
			break;

		default:
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			error = irp->Complete(irp);
			break;
	}

	DWORD level = WLOG_TRACE;
	if (error)
		level = WLOG_WARN;

	WLog_Print(parallel->log, level,
	           "[%s|0x%08" PRIx32 "] completed with %s [0x%08" PRIx32 "] (IoStatus %s [0x%08" PRIx32
	           "])",
	           rdpdr_irp_string(irp->MajorFunction), irp->MajorFunction, WTSErrorToString(error),
	           error, NtStatus2Tag(irp->IoStatus), irp->IoStatus);

	return error;
}

static DWORD WINAPI parallel_thread_func(LPVOID arg)
{
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*)arg;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(parallel);
	while (1)
	{
		if (!MessageQueue_Wait(parallel->queue))
		{
			WLog_Print(parallel->log, WLOG_ERROR, "MessageQueue_Wait failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		wMessage message = { 0 };
		if (!MessageQueue_Peek(parallel->queue, &message, TRUE))
		{
			WLog_Print(parallel->log, WLOG_ERROR, "MessageQueue_Peek failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if (message.id == WMQ_QUIT)
			break;

		IRP* irp = (IRP*)message.wParam;

		error = parallel_process_irp(parallel, irp);
		if (error)
		{
			WLog_Print(parallel->log, WLOG_ERROR,
			           "parallel_process_irp failed with error %" PRIu32 "!", error);
			break;
		}
	}

	if (error && parallel->rdpcontext)
		setChannelError(parallel->rdpcontext, error, "parallel_thread_func reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_irp_request(DEVICE* device, IRP* irp)
{
	PARALLEL_DEVICE* parallel = (PARALLEL_DEVICE*)device;

	WINPR_ASSERT(parallel);

	if (!MessageQueue_Post(parallel->queue, NULL, 0, (void*)irp, NULL))
	{
		WLog_Print(parallel->log, WLOG_ERROR, "MessageQueue_Post failed!");
		return ERROR_INTERNAL_ERROR;
	}

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT parallel_free_int(PARALLEL_DEVICE* parallel)
{
	if (parallel)
	{
		if (!MessageQueue_PostQuit(parallel->queue, 0) ||
		    (WaitForSingleObject(parallel->thread, INFINITE) == WAIT_FAILED))
		{
			const UINT error = GetLastError();
			WLog_Print(parallel->log, WLOG_ERROR,
			           "WaitForSingleObject failed with error %" PRIu32 "!", error);
		}

		(void)CloseHandle(parallel->thread);
		Stream_Free(parallel->device.data, TRUE);
		MessageQueue_Free(parallel->queue);
	}
	free(parallel);
	return CHANNEL_RC_OK;
}

static UINT parallel_free(DEVICE* device)
{
	if (device)
		return parallel_free_int((PARALLEL_DEVICE*)device);
	return CHANNEL_RC_OK;
}

static void parallel_message_free(void* obj)
{
	wMessage* msg = obj;
	if (!msg)
		return;
	if (msg->id != 0)
		return;

	IRP* irp = (IRP*)msg->wParam;
	if (!irp)
		return;
	WINPR_ASSERT(irp->Discard);
	irp->Discard(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(
    UINT VCAPITYPE parallel_DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints))
{
	PARALLEL_DEVICE* parallel = NULL;
	UINT error = 0;

	WINPR_ASSERT(pEntryPoints);

	RDPDR_PARALLEL* device = (RDPDR_PARALLEL*)pEntryPoints->device;
	WINPR_ASSERT(device);

	wLog* log = WLog_Get(TAG);
	WINPR_ASSERT(log);

	char* name = device->device.Name;
	char* path = device->Path;

	if (!name || (name[0] == '*') || !path)
	{
		/* TODO: implement auto detection of parallel ports */
		WLog_Print(log, WLOG_WARN, "Autodetection not implemented, no ports will be redirected");
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	if (name[0] && path[0])
	{
		parallel = (PARALLEL_DEVICE*)calloc(1, sizeof(PARALLEL_DEVICE));

		if (!parallel)
		{
			WLog_Print(log, WLOG_ERROR, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		parallel->log = log;
		parallel->device.type = RDPDR_DTYP_PARALLEL;
		parallel->device.name = name;
		parallel->device.IRPRequest = parallel_irp_request;
		parallel->device.Free = parallel_free;
		parallel->rdpcontext = pEntryPoints->rdpcontext;
		const size_t length = strlen(name);
		parallel->device.data = Stream_New(NULL, length + 1);

		if (!parallel->device.data)
		{
			WLog_Print(parallel->log, WLOG_ERROR, "Stream_New failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}

		for (size_t i = 0; i <= length; i++)
			Stream_Write_UINT8(parallel->device.data, name[i] < 0 ? '_' : name[i]);

		parallel->path = path;
		parallel->queue = MessageQueue_New(NULL);

		if (!parallel->queue)
		{
			WLog_Print(parallel->log, WLOG_ERROR, "MessageQueue_New failed!");
			error = CHANNEL_RC_NO_MEMORY;
			goto error_out;
		}

		wObject* obj = MessageQueue_Object(parallel->queue);
		WINPR_ASSERT(obj);
		obj->fnObjectFree = parallel_message_free;

		error = pEntryPoints->RegisterDevice(pEntryPoints->devman, &parallel->device);
		if (error)
		{
			WLog_Print(parallel->log, WLOG_ERROR, "RegisterDevice failed with error %" PRIu32 "!",
			           error);
			goto error_out;
		}

		parallel->thread = CreateThread(NULL, 0, parallel_thread_func, parallel, 0, NULL);
		if (!parallel->thread)
		{
			WLog_Print(parallel->log, WLOG_ERROR, "CreateThread failed!");
			error = ERROR_INTERNAL_ERROR;
			goto error_out;
		}
	}

	return CHANNEL_RC_OK;
error_out:
	parallel_free_int(parallel);
	return error;
}
