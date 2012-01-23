/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Print Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/unicode.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/thread.h>
#include <freerdp/utils/svc_plugin.h>

#include "rdpdr_constants.h"
#include "rdpdr_types.h"

#ifdef WITH_CUPS
#include "printer_cups.h"
#endif

#include "printer_main.h"

typedef struct _PRINTER_DEVICE PRINTER_DEVICE;
struct _PRINTER_DEVICE
{
	DEVICE device;

	rdpPrinter* printer;

	LIST* irp_list;
	freerdp_thread* thread;
};

static void printer_process_irp_create(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	rdpPrintJob* printjob = NULL;

	if (printer_dev->printer != NULL)
		printjob = printer_dev->printer->CreatePrintJob(printer_dev->printer, irp->devman->id_sequence++);

	if (printjob != NULL)
	{
		stream_write_uint32(irp->output, printjob->id); /* FileId */

		DEBUG_SVC("printjob id: %d", printjob->id);
	}
	else
	{
		stream_write_uint32(irp->output, 0); /* FileId */
		irp->IoStatus = STATUS_PRINT_QUEUE_FULL;

		DEBUG_WARN("error creating print job.");
	}

	irp->Complete(irp);
}

static void printer_process_irp_close(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	rdpPrintJob* printjob = NULL;

	if (printer_dev->printer != NULL)
		printjob = printer_dev->printer->FindPrintJob(printer_dev->printer, irp->FileId);

	if (printjob == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("printjob id %d not found.", irp->FileId);
	}
	else
	{
		printjob->Close(printjob);

		DEBUG_SVC("printjob id %d closed.", irp->FileId);
	}

	stream_write_zero(irp->output, 4); /* Padding(4) */

	irp->Complete(irp);
}

static void printer_process_irp_write(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	rdpPrintJob* printjob = NULL;
	uint32 Length;
	uint64 Offset;

	stream_read_uint32(irp->input, Length);
	stream_read_uint64(irp->input, Offset);
	stream_seek(irp->input, 20); /* Padding */

	if (printer_dev->printer != NULL)
		printjob = printer_dev->printer->FindPrintJob(printer_dev->printer, irp->FileId);

	if (printjob == NULL)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("printjob id %d not found.", irp->FileId);
	}
	else
	{
		printjob->Write(printjob, stream_get_tail(irp->input), Length);

		DEBUG_SVC("printjob id %d written %d bytes.", irp->FileId, Length);
	}

	stream_write_uint32(irp->output, Length);
	stream_write_uint8(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void printer_process_irp(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			printer_process_irp_create(printer_dev, irp);
			break;

		case IRP_MJ_CLOSE:
			printer_process_irp_close(printer_dev, irp);
			break;

		case IRP_MJ_WRITE:
			printer_process_irp_write(printer_dev, irp);
			break;

		default:
			DEBUG_WARN("MajorFunction 0x%X not supported", irp->MajorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void printer_process_irp_list(PRINTER_DEVICE* printer_dev)
{
	IRP* irp;

	while (1)
	{
		if (freerdp_thread_is_stopped(printer_dev->thread))
			break;

		freerdp_thread_lock(printer_dev->thread);
		irp = (IRP*)list_dequeue(printer_dev->irp_list);
		freerdp_thread_unlock(printer_dev->thread);

		if (irp == NULL)
			break;

		printer_process_irp(printer_dev, irp);
	}
}

static void* printer_thread_func(void* arg)
{
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*)arg;

	while (1)
	{
		freerdp_thread_wait(printer_dev->thread);

		if (freerdp_thread_is_stopped(printer_dev->thread))
			break;

		freerdp_thread_reset(printer_dev->thread);
		printer_process_irp_list(printer_dev);
	}

	freerdp_thread_quit(printer_dev->thread);

	return NULL;
}

static void printer_irp_request(DEVICE* device, IRP* irp)
{
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*)device;

	freerdp_thread_lock(printer_dev->thread);
	list_enqueue(printer_dev->irp_list, irp);
	freerdp_thread_unlock(printer_dev->thread);

	freerdp_thread_signal(printer_dev->thread);
}

static void printer_free(DEVICE* device)
{
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*)device;
	IRP* irp;

	freerdp_thread_stop(printer_dev->thread);
	freerdp_thread_free(printer_dev->thread);
	
	while ((irp = (IRP*)list_dequeue(printer_dev->irp_list)) != NULL)
		irp->Discard(irp);
	list_free(printer_dev->irp_list);

	if (printer_dev->printer)
		printer_dev->printer->Free(printer_dev->printer);

	xfree(printer_dev->device.name);

	xfree(printer_dev);
}

void printer_register(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints, rdpPrinter* printer)
{
	PRINTER_DEVICE* printer_dev;
	char* port;
	UNICONV* uniconv;
	uint32 Flags;
	size_t DriverNameLen;
	char* DriverName;
	size_t PrintNameLen;
	char* PrintName;
	uint32 CachedFieldsLen;
	uint8* CachedPrinterConfigData;

	port = xmalloc(10);
	snprintf(port, 10, "PRN%d", printer->id);

	printer_dev = xnew(PRINTER_DEVICE);

	printer_dev->device.type = RDPDR_DTYP_PRINT;
	printer_dev->device.name = port;
	printer_dev->device.IRPRequest = printer_irp_request;
	printer_dev->device.Free = printer_free;

	printer_dev->printer = printer;

	CachedFieldsLen = 0;
	CachedPrinterConfigData = NULL;

	DEBUG_SVC("Printer %s registered", printer->name);

	Flags = 0;
	if (printer->is_default)
		Flags |= RDPDR_PRINTER_ANNOUNCE_FLAG_DEFAULTPRINTER;

	uniconv = freerdp_uniconv_new();
	DriverName = freerdp_uniconv_out(uniconv, printer->driver, &DriverNameLen);
	PrintName = freerdp_uniconv_out(uniconv, printer->name, &PrintNameLen);
	freerdp_uniconv_free(uniconv);

	printer_dev->device.data = stream_new(28 + DriverNameLen + PrintNameLen + CachedFieldsLen);

	stream_write_uint32(printer_dev->device.data, Flags);
	stream_write_uint32(printer_dev->device.data, 0); /* CodePage, reserved */
	stream_write_uint32(printer_dev->device.data, 0); /* PnPNameLen */
	stream_write_uint32(printer_dev->device.data, DriverNameLen + 2);
	stream_write_uint32(printer_dev->device.data, PrintNameLen + 2);
	stream_write_uint32(printer_dev->device.data, CachedFieldsLen);
	stream_write(printer_dev->device.data, DriverName, DriverNameLen);
	stream_write_uint16(printer_dev->device.data, 0);
	stream_write(printer_dev->device.data, PrintName, PrintNameLen);
	stream_write_uint16(printer_dev->device.data, 0);
	if (CachedFieldsLen > 0)
	{
		stream_write(printer_dev->device.data, CachedPrinterConfigData, CachedFieldsLen);
	}

	xfree(DriverName);
	xfree(PrintName);

	printer_dev->irp_list = list_new();
	printer_dev->thread = freerdp_thread_new();

	pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*)printer_dev);

	freerdp_thread_start(printer_dev->thread, printer_thread_func, printer_dev);
}

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	rdpPrinterDriver* driver = NULL;
	rdpPrinter** printers;
	rdpPrinter* printer;
	int i;
	char* name;
	char* driver_name;

#ifdef WITH_CUPS
	driver = printer_cups_get_driver();
#endif
	if (driver == NULL)
	{
		DEBUG_WARN("no driver.");
		return 1;
	}

	name = (char*)pEntryPoints->plugin_data->data[1];
	driver_name = (char*)pEntryPoints->plugin_data->data[2];

	if (name && name[0])
	{
		printer = driver->GetPrinter(driver, name);
		if (printer == NULL)
		{
			DEBUG_WARN("printer %s not found.", name);
			return 1;
		}
		if (driver_name && driver_name[0])
			printer->driver = driver_name;

		printer_register(pEntryPoints, printer);
	}
	else
	{
		printers = driver->EnumPrinters(driver);
		for (i = 0; printers[i]; i++)
		{
			printer = printers[i];
			printer_register(pEntryPoints, printer);
		}
		xfree(printers);
	}

	return 0;
}
