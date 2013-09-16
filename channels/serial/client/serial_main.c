/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Serial Port Device Service Virtual Channel
 *
 * Copyright 2011 O.S. Systems Software Ltda.
 * Copyright 2011 Eduardo Fiss Beloni <beloni@ossystems.com.br>
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_MODEM_H
#include <sys/modem.h>
#endif
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#ifdef HAVE_SYS_STRTIO_H
#include <sys/strtio.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "serial_tty.h"
#include "serial_constants.h"

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

#include <freerdp/freerdp.h>
#include <freerdp/utils/list.h>
#include <winpr/stream.h>
#include <freerdp/channels/rdpdr.h>

typedef struct _SERIAL_DEVICE SERIAL_DEVICE;

struct _SERIAL_DEVICE
{
	DEVICE device;

	char* path;
	SERIAL_TTY* tty;

	HANDLE thread;
	HANDLE mthread;
	HANDLE stopEvent;
	HANDLE newEvent;

	wQueue* queue;
	LIST* pending_irps;

	fd_set read_fds;
	fd_set write_fds;
	UINT32 nfds;
	struct timeval tv;
	UINT32 select_timeout;
	UINT32 timeout_id;
};

static void serial_abort_single_io(SERIAL_DEVICE* serial, UINT32 file_id, UINT32 abort_io, UINT32 io_status);
static void serial_check_for_events(SERIAL_DEVICE* serial);
static void serial_handle_async_irp(SERIAL_DEVICE* serial, IRP* irp);
static BOOL serial_check_fds(SERIAL_DEVICE* serial);
static void* serial_thread_mfunc(void* arg);

static void serial_process_irp_create(SERIAL_DEVICE* serial, IRP* irp)
{
	char* path = NULL;
	int status;
	SERIAL_TTY* tty;
	UINT32 PathLength;
	UINT32 FileId;

	Stream_Seek(irp->input, 28); /* DesiredAccess(4) AllocationSize(8), FileAttributes(4) */
					/* SharedAccess(4) CreateDisposition(4), CreateOptions(4) */
	Stream_Read_UINT32(irp->input, PathLength);

	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(irp->input),
			PathLength / 2, &path, 0, NULL, NULL);

	if (status < 1)
		path = (char*) calloc(1, 1);

	FileId = irp->devman->id_sequence++;

	tty = serial_tty_new(serial->path, FileId);

	if (tty == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		FileId = 0;

		DEBUG_WARN("failed to create %s", path);
	}
	else
	{
		serial->tty = tty;
	
		serial_abort_single_io(serial, serial->timeout_id, SERIAL_ABORT_IO_NONE, 
				STATUS_CANCELLED);
		serial_abort_single_io(serial, serial->timeout_id, SERIAL_ABORT_IO_READ, 
				STATUS_CANCELLED);
		serial_abort_single_io(serial, serial->timeout_id, SERIAL_ABORT_IO_WRITE, 
				STATUS_CANCELLED);

		serial->mthread = CreateThread(NULL, 0, 
				(LPTHREAD_START_ROUTINE) serial_thread_mfunc, (void*) serial,
				0, NULL);

		DEBUG_SVC("%s(%d) created.", serial->path, FileId);
	}

	Stream_Write_UINT32(irp->output, FileId);
	Stream_Write_UINT8(irp->output, 0);

	free(path);

	irp->Complete(irp);
}

static void serial_process_irp_close(SERIAL_DEVICE* serial, IRP* irp)
{
	SERIAL_TTY* tty;

	tty = serial->tty;

	if (tty == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		DEBUG_WARN("tty not valid.");
	}
	else
	{
		DEBUG_SVC("%s(%d) closed.", serial->path, tty->id);

		TerminateThread(serial->mthread, 0);
		WaitForSingleObject(serial->mthread, INFINITE);
		CloseHandle(serial->mthread);
		serial->mthread = NULL;

		serial_tty_free(tty);
		serial->tty = NULL;
	}

	Stream_Zero(irp->output, 5); /* Padding(5) */

	irp->Complete(irp);
}

static void serial_process_irp_read(SERIAL_DEVICE* serial, IRP* irp)
{
	SERIAL_TTY* tty;
	UINT32 Length;
	UINT64 Offset;
	BYTE* buffer = NULL;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);

	DEBUG_SVC("length %u offset %llu", Length, Offset);

	tty = serial->tty;

	if (tty == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("tty not valid.");
	}
	else
	{
		buffer = (BYTE*) malloc(Length);

		if (!serial_tty_read(tty, buffer, &Length))
		{
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			free(buffer);
			buffer = NULL;
			Length = 0;

			DEBUG_WARN("read %s(%d) failed.", serial->path, tty->id);
		}
		else
		{
			DEBUG_SVC("read %llu-%llu from %d", Offset, Offset + Length, tty->id);
		}
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

static void serial_process_irp_write(SERIAL_DEVICE* serial, IRP* irp)
{
	SERIAL_TTY* tty;
	UINT32 Length;
	UINT64 Offset;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	Stream_Seek(irp->input, 20); /* Padding */

	DEBUG_SVC("length %u offset %llu", Length, Offset);

	tty = serial->tty;

	if (tty == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("tty not valid.");
	}
	else if (!serial_tty_write(tty, Stream_Pointer(irp->input), Length))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("write %s(%d) failed.", serial->path, tty->id);
	}
	else
	{
		DEBUG_SVC("write %llu-%llu to %s(%d).", Offset, Offset + Length, serial->path, tty->id);
	}

	Stream_Write_UINT32(irp->output, Length);
	Stream_Write_UINT8(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void serial_process_irp_device_control(SERIAL_DEVICE* serial, IRP* irp)
{
	SERIAL_TTY* tty;
	UINT32 IoControlCode;
	UINT32 InputBufferLength;
	UINT32 OutputBufferLength;
	UINT32 abort_io = SERIAL_ABORT_IO_NONE;

	DEBUG_SVC("[in] pending size %d", list_size(serial->pending_irps));

	Stream_Read_UINT32(irp->input, InputBufferLength);
	Stream_Read_UINT32(irp->input, OutputBufferLength);
	Stream_Read_UINT32(irp->input, IoControlCode);
	Stream_Seek(irp->input, 20); /* Padding */

	tty = serial->tty;

	if (!tty)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		OutputBufferLength = 0;

		DEBUG_WARN("tty not valid.");
	}
	else
	{
		irp->IoStatus = serial_tty_control(tty, IoControlCode, irp->input, irp->output, &abort_io);
	}

	if (abort_io & SERIAL_ABORT_IO_WRITE)
		serial_abort_single_io(serial, tty->id, SERIAL_ABORT_IO_WRITE, STATUS_CANCELLED);
	if (abort_io & SERIAL_ABORT_IO_READ)
		serial_abort_single_io(serial, tty->id, SERIAL_ABORT_IO_READ, STATUS_CANCELLED);

	if (irp->IoStatus == STATUS_PENDING)
		list_enqueue(serial->pending_irps, irp);
	else
		irp->Complete(irp);
}

static void serial_process_irp(SERIAL_DEVICE* serial, IRP* irp)
{
	DEBUG_SVC("MajorFunction %u", irp->MajorFunction);

	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			serial_process_irp_create(serial, irp);
			break;

		case IRP_MJ_CLOSE:
			serial_process_irp_close(serial, irp);
			break;

		case IRP_MJ_READ:
			serial_handle_async_irp(serial, irp);
			//serial_process_irp_read(serial, irp);
			break;

		case IRP_MJ_WRITE:
			serial_handle_async_irp(serial, irp);
			//serial_process_irp_write(serial, irp);
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

	serial_check_for_events(serial);
}

/* This thread is used as a workaround for the missing serial event
 * support in WaitForMultipleObjects.
 * It monitors the terminal for events and posts it in a supported
 * form that WaitForMultipleObjects can use it. */
void* serial_thread_mfunc(void* arg)
{
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*)arg;
	
	assert(NULL != serial);
	while(1)
	{
		int sl;
		fd_set rd;

		if(!serial->tty || serial->tty->fd <= 0)
		{
			DEBUG_WARN("Monitor thread still running, but no terminal opened!");
			sleep(1);
		}
		else
		{
			FD_ZERO(&rd);
			FD_SET(serial->tty->fd, &rd);
			sl = select(serial->tty->fd + 1, &rd, NULL,	NULL, NULL);
			if( sl > 0 )
				SetEvent(serial->newEvent);
		}
	}

	ExitThread(0);
	return NULL;
}

static void* serial_thread_func(void* arg)
{
	IRP* irp;
	DWORD status;
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*)arg;
	HANDLE ev[] = {serial->stopEvent, Queue_Event(serial->queue), serial->newEvent};

	assert(NULL != serial);
	while (1)
	{
		status = WaitForMultipleObjects(3, ev, FALSE, INFINITE);
	
		if (WAIT_OBJECT_0 == status)
			break;
		else if (status == WAIT_OBJECT_0 + 1)
		{
			FD_ZERO(&serial->read_fds);
			FD_ZERO(&serial->write_fds);

			serial->tv.tv_sec = 0;
			serial->tv.tv_usec = 0;
			serial->select_timeout = 0;

			if ((irp = (IRP*) Queue_Dequeue(serial->queue)))
				serial_process_irp(serial, irp);
		}
		else if (status == WAIT_OBJECT_0 + 2)
			ResetEvent(serial->newEvent);

		if(serial->tty)
			serial_check_fds(serial);
	}

	ExitThread(0);
	return NULL;
}

static void serial_irp_request(DEVICE* device, IRP* irp)
{
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*) device;

	Queue_Enqueue(serial->queue, irp);
}

static void serial_free(DEVICE* device)
{
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*) device;

	DEBUG_SVC("freeing device");

	/* Stop thread */
	SetEvent(serial->stopEvent);
	if(serial->mthread)
	{
		TerminateThread(serial->mthread, 0);
		WaitForSingleObject(serial->mthread, INFINITE);
		CloseHandle(serial->mthread);
	}
	WaitForSingleObject(serial->thread, INFINITE);

	serial_tty_free(serial->tty);

	/* Clean up resources */
	Stream_Free(serial->device.data, TRUE);
	Queue_Free(serial->queue);
	list_free(serial->pending_irps);
	CloseHandle(serial->stopEvent);
	CloseHandle(serial->newEvent);
	CloseHandle(serial->thread);
	free(serial);
}

static void serial_abort_single_io(SERIAL_DEVICE* serial, UINT32 file_id, UINT32 abort_io, UINT32 io_status)
{
	UINT32 major;
	IRP* irp = NULL;
	SERIAL_TTY* tty;

	DEBUG_SVC("[in] pending size %d", list_size(serial->pending_irps));

	tty = serial->tty;
	if(!tty)
	{
		DEBUG_WARN("tty = %p", tty);
		return;
	}

	switch (abort_io)
	{
		case SERIAL_ABORT_IO_NONE:
			major = 0;
			break;

		case SERIAL_ABORT_IO_READ:
			major = IRP_MJ_READ;
			break;

		case SERIAL_ABORT_IO_WRITE:
			major = IRP_MJ_WRITE;
			break;

		default:
			DEBUG_SVC("unexpected abort_io code %d", abort_io);
			return;
	}

	irp = (IRP*) list_peek(serial->pending_irps);

	while (irp)
	{
		if (irp->FileId != file_id || irp->MajorFunction != major)
		{
			irp = (IRP*) list_next(serial->pending_irps, irp);
			continue;
		}

		/* Process a SINGLE FileId and MajorFunction */
		list_remove(serial->pending_irps, irp);
		irp->IoStatus = io_status;
		Stream_Write_UINT32(irp->output, 0);
		irp->Complete(irp);

		break;
	}

	DEBUG_SVC("[out] pending size %d", list_size(serial->pending_irps));
}

static void serial_check_for_events(SERIAL_DEVICE* serial)
{
	IRP* irp = NULL;
	IRP* prev;
	UINT32 result = 0;
	SERIAL_TTY* tty;

	tty = serial->tty;
	if(!tty)
	{
		DEBUG_WARN("tty = %p", tty);
		return;
	}

	DEBUG_SVC("[in] pending size %d", list_size(serial->pending_irps));

	irp = (IRP*) list_peek(serial->pending_irps);

	while (irp)
	{
		prev = NULL;

		if (irp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
		{
			if (serial_tty_get_event(tty, &result))
			{
				DEBUG_SVC("got event result %u", result);

				irp->IoStatus = STATUS_SUCCESS;
				Stream_Write_UINT32(irp->output, result);
				irp->Complete(irp);

				prev = irp;
				irp = (IRP*) list_next(serial->pending_irps, irp);
				list_remove(serial->pending_irps, prev);
			}
		}

		if (!prev)
			irp = (IRP*) list_next(serial->pending_irps, irp);
	}

	DEBUG_SVC("[out] pending size %d", list_size(serial->pending_irps));
}

void serial_get_timeouts(SERIAL_DEVICE* serial, IRP* irp, UINT32* timeout, UINT32* interval_timeout)
{
	SERIAL_TTY* tty;
	UINT32 Length;
	UINT32 pos;

	pos = Stream_GetPosition(irp->input);
	Stream_Read_UINT32(irp->input, Length);
	Stream_SetPosition(irp->input, pos);

	DEBUG_SVC("length read %u", Length);

	tty = serial->tty;
	if(!tty)
	{
		DEBUG_WARN("tty = %p", tty);
		return;
	}

	*timeout = (tty->read_total_timeout_multiplier * Length) + tty->read_total_timeout_constant;
	*interval_timeout = tty->read_interval_timeout;

	DEBUG_SVC("timeouts %u %u", *timeout, *interval_timeout);
}

static void serial_handle_async_irp(SERIAL_DEVICE* serial, IRP* irp)
{
	UINT32 timeout = 0;
	UINT32 itv_timeout = 0;
	SERIAL_TTY* tty;

	tty = serial->tty;
	if(!tty)
	{
		DEBUG_WARN("tty = %p", tty);
		return;
	}

	switch (irp->MajorFunction)
	{
		case IRP_MJ_WRITE:
			DEBUG_SVC("handling IRP_MJ_WRITE");
			break;

		case IRP_MJ_READ:
			DEBUG_SVC("handling IRP_MJ_READ");

			serial_get_timeouts(serial, irp, &timeout, &itv_timeout);

			/* Check if io request timeout is smaller than current (but not 0). */
			if (timeout && ((serial->select_timeout == 0) || (timeout < serial->select_timeout)))
			{
				serial->select_timeout = timeout;
				serial->tv.tv_sec = serial->select_timeout / 1000;
				serial->tv.tv_usec = (serial->select_timeout % 1000) * 1000;
				serial->timeout_id = tty->id;
			}
			if (itv_timeout && ((serial->select_timeout == 0) || (itv_timeout < serial->select_timeout)))
			{
				serial->select_timeout = itv_timeout;
				serial->tv.tv_sec = serial->select_timeout / 1000;
				serial->tv.tv_usec = (serial->select_timeout % 1000) * 1000;
				serial->timeout_id = tty->id;
			}
			DEBUG_SVC("select_timeout %u, tv_sec %lu tv_usec %lu, timeout_id %u",
				serial->select_timeout, serial->tv.tv_sec, serial->tv.tv_usec, serial->timeout_id);
			break;

		default:
			DEBUG_SVC("no need to handle %d", irp->MajorFunction);
			return;
	}

	irp->IoStatus = STATUS_PENDING;
	list_enqueue(serial->pending_irps, irp);
}

static void __serial_check_fds(SERIAL_DEVICE* serial)
{
	IRP* irp;
	IRP* prev;
	SERIAL_TTY* tty;
	UINT32 result = 0;
	BOOL irp_completed = FALSE;

	ZeroMemory(&serial->tv, sizeof(struct timeval));
	tty = serial->tty;
	if(!tty)
	{
		DEBUG_WARN("tty = %p", tty);
		return;
	}

	/* scan every pending */
	irp = list_peek(serial->pending_irps);

	while (irp)
	{
		DEBUG_SVC("MajorFunction %u", irp->MajorFunction);

		switch (irp->MajorFunction)
		{
			case IRP_MJ_READ:
				if (FD_ISSET(tty->fd, &serial->read_fds))
				{
					irp->IoStatus = STATUS_SUCCESS;
					serial_process_irp_read(serial, irp);
					irp_completed = TRUE;
				}
				break;

			case IRP_MJ_WRITE:
				if (FD_ISSET(tty->fd, &serial->write_fds))
				{
					irp->IoStatus = STATUS_SUCCESS;
					serial_process_irp_write(serial, irp);
					irp_completed = TRUE;
				}
				break;

			case IRP_MJ_DEVICE_CONTROL:
				if (serial_tty_get_event(tty, &result))
				{
					DEBUG_SVC("got event result %u", result);

					irp->IoStatus = STATUS_SUCCESS;
					Stream_Write_UINT32(irp->output, result);
					irp->Complete(irp);
					irp_completed = TRUE;
				}
				break;

			default:
				DEBUG_SVC("no request found");
				break;
		}

		prev = irp;
		irp = (IRP*) list_next(serial->pending_irps, irp);

		if (irp_completed || (prev->IoStatus == STATUS_SUCCESS))
			list_remove(serial->pending_irps, prev);
	}
}

static void serial_set_fds(SERIAL_DEVICE* serial)
{
	IRP* irp;
	fd_set* fds;
	SERIAL_TTY* tty;

	DEBUG_SVC("[in] pending size %d", list_size(serial->pending_irps));

	tty = serial->tty;
	if(!tty)
	{
		DEBUG_WARN("tty = %p", tty);
		return;
	}
	irp = (IRP*) list_peek(serial->pending_irps);

	while (irp)
	{
		fds = NULL;

		switch (irp->MajorFunction)
		{
			case IRP_MJ_WRITE:
				fds = &serial->write_fds;
				break;

			case IRP_MJ_READ:
				fds = &serial->read_fds;
				break;
		}

		if (fds && (tty->fd >= 0))
		{
			FD_SET(tty->fd, fds);
			serial->nfds = MAX(serial->nfds, tty->fd);
		}

		irp = (IRP*) list_next(serial->pending_irps, irp);
	}
}

static BOOL serial_check_fds(SERIAL_DEVICE* serial)
{
	if (list_size(serial->pending_irps) == 0)
		return 1;

	FD_ZERO(&serial->read_fds);
	FD_ZERO(&serial->write_fds);

	serial->tv.tv_sec = 0;
	serial->tv.tv_usec = 0;
	serial->select_timeout = 0;

	serial_set_fds(serial);
	DEBUG_SVC("waiting %lu %lu", serial->tv.tv_sec, serial->tv.tv_usec);

	switch (select(serial->nfds + 1, &serial->read_fds, &serial->write_fds, NULL, &serial->tv))
	{
		case -1:
			DEBUG_SVC("select has returned -1 with error: %s", strerror(errno));
			return 0;

		case 0:
			if (serial->select_timeout)
			{
				__serial_check_fds(serial);
				serial_abort_single_io(serial, serial->timeout_id, SERIAL_ABORT_IO_NONE, STATUS_TIMEOUT);
				serial_abort_single_io(serial, serial->timeout_id, SERIAL_ABORT_IO_READ, STATUS_TIMEOUT);
				serial_abort_single_io(serial, serial->timeout_id, SERIAL_ABORT_IO_WRITE, STATUS_TIMEOUT);
			}
			DEBUG_SVC("select has timed out");
			return 0;

		default:
			break;
	}

	__serial_check_fds(serial);

	return 1;
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	serial_DeviceServiceEntry
#endif

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	int i, len;
	char* name;
	char* path;
	RDPDR_SERIAL* device;
	SERIAL_DEVICE* serial;

	device = (RDPDR_SERIAL*) pEntryPoints->device;
	name = device->Name;
	path = device->Path;

	if ((name && name[0]) && (path && path[0]))
	{
		serial = (SERIAL_DEVICE*) malloc(sizeof(SERIAL_DEVICE));
		ZeroMemory(serial, sizeof(SERIAL_DEVICE));

		serial->device.type = RDPDR_DTYP_SERIAL;
		serial->device.name = name;
		serial->device.IRPRequest = serial_irp_request;
		serial->device.Free = serial_free;

		len = strlen(name);
		serial->device.data = Stream_New(NULL, len + 1);

		for (i = 0; i <= len; i++)
			Stream_Write_UINT8(serial->device.data, name[i] < 0 ? '_' : name[i]);

		serial->path = path;
		serial->queue = Queue_New(TRUE, -1, -1);
		serial->pending_irps = list_new();

		serial->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		serial->newEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) serial);

		serial->thread = CreateThread(NULL, 0, 
				(LPTHREAD_START_ROUTINE) serial_thread_func, (void*) serial, 0, NULL);
		serial->mthread = NULL;
	}

	return 0;
}
