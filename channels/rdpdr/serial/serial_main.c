/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "config.h"

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

#include "rdpdr_types.h"
#include "rdpdr_constants.h"
#include "devman.h"
#include "serial_tty.h"
#include "serial_constants.h"

#include <freerdp/utils/stream.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/unicode.h>
#include <freerdp/utils/wait_obj.h>

typedef struct _SERIAL_DEVICE SERIAL_DEVICE;
struct _SERIAL_DEVICE
{
	DEVICE device;

	char* path;
	SERIAL_TTY* tty;

	LIST* irp_list;
	LIST* pending_irps;
	freerdp_thread* thread;
	struct wait_obj* in_event;

	fd_set read_fds;
	fd_set write_fds;
	uint32 nfds;
	struct timeval tv;
	uint32 select_timeout;
	uint32 timeout_id;
};

static void serial_abort_single_io(SERIAL_DEVICE* serial, uint32 file_id, uint32 abort_io, uint32 io_status);
static void serial_check_for_events(SERIAL_DEVICE* serial);
static void serial_handle_async_irp(SERIAL_DEVICE* serial, IRP* irp);
static boolean serial_check_fds(SERIAL_DEVICE* serial);

static void serial_process_irp_create(SERIAL_DEVICE* serial, IRP* irp)
{
	SERIAL_TTY* tty;
	uint32 PathLength;
	uint32 FileId;
	char* path;
	UNICONV* uniconv;

	stream_seek(irp->input, 28); /* DesiredAccess(4) AllocationSize(8), FileAttributes(4) */
								 /* SharedAccess(4) CreateDisposition(4), CreateOptions(4) */
	stream_read_uint32(irp->input, PathLength);

	uniconv = freerdp_uniconv_new();
	path = freerdp_uniconv_in(uniconv, stream_get_tail(irp->input), PathLength);
	freerdp_uniconv_free(uniconv);

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
		DEBUG_SVC("%s(%d) created.", serial->path, FileId);
	}

	stream_write_uint32(irp->output, FileId);
	stream_write_uint8(irp->output, 0);

	xfree(path);

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

		serial_tty_free(tty);
		serial->tty = NULL;
	}

	stream_write_zero(irp->output, 5); /* Padding(5) */

	irp->Complete(irp);
}

static void serial_process_irp_read(SERIAL_DEVICE* serial, IRP* irp)
{
	SERIAL_TTY* tty;
	uint32 Length;
	uint64 Offset;
	uint8* buffer = NULL;

	stream_read_uint32(irp->input, Length);
	stream_read_uint64(irp->input, Offset);

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
		buffer = (uint8*)xmalloc(Length);
		if (!serial_tty_read(tty, buffer, &Length))
		{
			irp->IoStatus = STATUS_UNSUCCESSFUL;
			xfree(buffer);
			buffer = NULL;
			Length = 0;

			DEBUG_WARN("read %s(%d) failed.", serial->path, tty->id);
		}
		else
		{
			DEBUG_SVC("read %llu-%llu from %d", Offset, Offset + Length, tty->id);
		}
	}

	stream_write_uint32(irp->output, Length);
	if (Length > 0)
	{
		stream_check_size(irp->output, Length);
		stream_write(irp->output, buffer, Length);
	}
	xfree(buffer);

	irp->Complete(irp);
}

static void serial_process_irp_write(SERIAL_DEVICE* serial, IRP* irp)
{
	SERIAL_TTY* tty;
	uint32 Length;
	uint64 Offset;

	stream_read_uint32(irp->input, Length);
	stream_read_uint64(irp->input, Offset);
	stream_seek(irp->input, 20); /* Padding */

	DEBUG_SVC("length %u offset %llu", Length, Offset);

	tty = serial->tty;
	if (tty == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("tty not valid.");
	}
	else if (!serial_tty_write(tty, stream_get_tail(irp->input), Length))
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("write %s(%d) failed.", serial->path, tty->id);
	}
	else
	{
		DEBUG_SVC("write %llu-%llu to %s(%d).", Offset, Offset + Length, serial->path, tty->id);
	}

	stream_write_uint32(irp->output, Length);
	stream_write_uint8(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void serial_process_irp_device_control(SERIAL_DEVICE* serial, IRP* irp)
{
	SERIAL_TTY* tty;
	uint32 IoControlCode;
	uint32 InputBufferLength;
	uint32 OutputBufferLength;
	uint32 abort_io = SERIAL_ABORT_IO_NONE;

	DEBUG_SVC("[in] pending size %d", list_size(serial->pending_irps));

	stream_read_uint32(irp->input, InputBufferLength);
	stream_read_uint32(irp->input, OutputBufferLength);
	stream_read_uint32(irp->input, IoControlCode);
	stream_seek(irp->input, 20); /* Padding */

	tty = serial->tty;
	if (tty == NULL)
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

static void serial_process_irp_list(SERIAL_DEVICE* serial)
{
	IRP* irp;

	while (1)
	{
		if (freerdp_thread_is_stopped(serial->thread))
			break;

		freerdp_thread_lock(serial->thread);
		irp = (IRP*)list_dequeue(serial->irp_list);
		freerdp_thread_unlock(serial->thread);

		if (irp == NULL)
			break;

		serial_process_irp(serial, irp);
	}
}

static void* serial_thread_func(void* arg)
{
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*)arg;

	while (1)
	{
		freerdp_thread_wait(serial->thread);

		serial->nfds = 1;
		FD_ZERO(&serial->read_fds);
		FD_ZERO(&serial->write_fds);

		serial->tv.tv_sec = 20;
		serial->tv.tv_usec = 0;
		serial->select_timeout = 0;

		if (freerdp_thread_is_stopped(serial->thread))
			break;

		freerdp_thread_reset(serial->thread);
		serial_process_irp_list(serial);

		if (wait_obj_is_set(serial->in_event))
		{
			if (serial_check_fds(serial))
				wait_obj_clear(serial->in_event);
		}
	}

	freerdp_thread_quit(serial->thread);

	return NULL;
}

static void serial_irp_request(DEVICE* device, IRP* irp)
{
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*)device;

	freerdp_thread_lock(serial->thread);
	list_enqueue(serial->irp_list, irp);
	freerdp_thread_unlock(serial->thread);

	freerdp_thread_signal(serial->thread);
}

static void serial_free(DEVICE* device)
{
	SERIAL_DEVICE* serial = (SERIAL_DEVICE*)device;
	IRP* irp;

	DEBUG_SVC("freeing device");

	freerdp_thread_stop(serial->thread);
	freerdp_thread_free(serial->thread);

	while ((irp = (IRP*)list_dequeue(serial->irp_list)) != NULL)
		irp->Discard(irp);
	list_free(serial->irp_list);

	while ((irp = (IRP*)list_dequeue(serial->pending_irps)) != NULL)
		irp->Discard(irp);
	list_free(serial->pending_irps);

	xfree(serial);
}

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	SERIAL_DEVICE* serial;
	char* name;
	char* path;
	int i, len;

	name = (char*)pEntryPoints->plugin_data->data[1];
	path = (char*)pEntryPoints->plugin_data->data[2];

	if (name[0] && path[0])
	{
		serial = xnew(SERIAL_DEVICE);

		serial->device.type = RDPDR_DTYP_SERIAL;
		serial->device.name = name;
		serial->device.IRPRequest = serial_irp_request;
		serial->device.Free = serial_free;

		len = strlen(name);
		serial->device.data = stream_new(len + 1);
		for (i = 0; i <= len; i++)
			stream_write_uint8(serial->device.data, name[i] < 0 ? '_' : name[i]);

		serial->path = path;
		serial->irp_list = list_new();
		serial->pending_irps = list_new();
		serial->thread = freerdp_thread_new();
		serial->in_event = wait_obj_new();

		pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*)serial);

		freerdp_thread_start(serial->thread, serial_thread_func, serial);
	}

	return 0;
}

static void serial_abort_single_io(SERIAL_DEVICE* serial, uint32 file_id, uint32 abort_io, uint32 io_status)
{
	IRP* irp = NULL;
	uint32 major;
	SERIAL_TTY* tty;

	DEBUG_SVC("[in] pending size %d", list_size(serial->pending_irps));

	tty = serial->tty;

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

	irp = (IRP*)list_peek(serial->pending_irps);
	while (irp)
	{
		if (irp->FileId != file_id || irp->MajorFunction != major)
		{
			irp = (IRP*)list_next(serial->pending_irps, irp);
			continue;
		}

		/* Process a SINGLE FileId and MajorFunction */
		list_remove(serial->pending_irps, irp);
		irp->IoStatus = io_status;
		stream_write_uint32(irp->output, 0);
		irp->Complete(irp);

		wait_obj_set(serial->in_event);
		break;
	}

	DEBUG_SVC("[out] pending size %d", list_size(serial->pending_irps));
}

static void serial_check_for_events(SERIAL_DEVICE* serial)
{
	IRP* irp = NULL;
	IRP* prev;
	uint32 result = 0;
	SERIAL_TTY* tty;

	tty = serial->tty;

	DEBUG_SVC("[in] pending size %d", list_size(serial->pending_irps));

	irp = (IRP*)list_peek(serial->pending_irps);
	while (irp)
	{
		prev = NULL;

		if (irp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
		{
			if (serial_tty_get_event(tty, &result))
			{
				DEBUG_SVC("got event result %u", result);

				irp->IoStatus = STATUS_SUCCESS;
				stream_write_uint32(irp->output, result);
				irp->Complete(irp);

				prev = irp;
				irp = (IRP*)list_next(serial->pending_irps, irp);
				list_remove(serial->pending_irps, prev);

				wait_obj_set(serial->in_event);
			}
		}

		if (!prev)
			irp = (IRP*)list_next(serial->pending_irps, irp);
	}

	DEBUG_SVC("[out] pending size %d", list_size(serial->pending_irps));
}

void serial_get_timeouts(SERIAL_DEVICE* serial, IRP* irp, uint32* timeout, uint32* interval_timeout)
{
	SERIAL_TTY* tty;
	uint32 Length;
	uint32 pos;

	pos = stream_get_pos(irp->input);
	stream_read_uint32(irp->input, Length);
	stream_set_pos(irp->input, pos);

	DEBUG_SVC("length read %u", Length);

	tty = serial->tty;
	*timeout = (tty->read_total_timeout_multiplier * Length) +
		tty->read_total_timeout_constant;
	*interval_timeout = tty->read_interval_timeout;

	DEBUG_SVC("timeouts %u %u", *timeout, *interval_timeout);
}

static void serial_handle_async_irp(SERIAL_DEVICE* serial, IRP* irp)
{
	uint32 timeout = 0;
	uint32 itv_timeout = 0;
	SERIAL_TTY* tty;

	tty = serial->tty;

	switch (irp->MajorFunction)
	{
	case IRP_MJ_WRITE:
		DEBUG_SVC("handling IRP_MJ_WRITE");
		break;

	case IRP_MJ_READ:
		DEBUG_SVC("handling IRP_MJ_READ");

		serial_get_timeouts(serial, irp, &timeout, &itv_timeout);

		/* Check if io request timeout is smaller than current (but not 0). */
		if (timeout && (serial->select_timeout == 0 || timeout < serial->select_timeout))
		{
			serial->select_timeout = timeout;
			serial->tv.tv_sec = serial->select_timeout / 1000;
			serial->tv.tv_usec = (serial->select_timeout % 1000) * 1000;
			serial->timeout_id = tty->id;
		}
		if (itv_timeout && (serial->select_timeout == 0 || itv_timeout < serial->select_timeout))
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
	wait_obj_set(serial->in_event);
}

static void __serial_check_fds(SERIAL_DEVICE* serial)
{
	IRP* irp;
	IRP* prev;
	SERIAL_TTY* tty;
	uint32 result = 0;

	memset(&serial->tv, 0, sizeof(struct timeval));
	tty = serial->tty;

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
				}
				break;

			case IRP_MJ_WRITE:
				if (FD_ISSET(tty->fd, &serial->write_fds))
				{
					irp->IoStatus = STATUS_SUCCESS;
					serial_process_irp_write(serial, irp);
				}
				break;

			case IRP_MJ_DEVICE_CONTROL:
				if (serial_tty_get_event(tty, &result))
				{
					DEBUG_SVC("got event result %u", result);

					irp->IoStatus = STATUS_SUCCESS;
					stream_write_uint32(irp->output, result);
					irp->Complete(irp);
				}
				break;

			default:
				DEBUG_SVC("no request found");
				break;
		}

		prev = irp;
		irp = (IRP*)list_next(serial->pending_irps, irp);
		if (prev->IoStatus == STATUS_SUCCESS)
		{
			list_remove(serial->pending_irps, prev);
			wait_obj_set(serial->in_event);
		}
	}
}

static void serial_set_fds(SERIAL_DEVICE* serial)
{
	fd_set* fds;
	IRP* irp;
	SERIAL_TTY* tty;

	DEBUG_SVC("[in] pending size %d", list_size(serial->pending_irps));

	tty = serial->tty;
	irp = (IRP*)list_peek(serial->pending_irps);
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
		irp = (IRP*)list_next(serial->pending_irps, irp);
	}
}

static boolean serial_check_fds(SERIAL_DEVICE* serial)
{
	if (list_size(serial->pending_irps) == 0)
		return 1;

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
