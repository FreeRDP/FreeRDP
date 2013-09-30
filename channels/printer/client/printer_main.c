/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/interlocked.h>

#include <winpr/stream.h>
#include <freerdp/channels/rdpdr.h>
#include <freerdp/utils/svc_plugin.h>

#ifdef WITH_CUPS
#include "printer_cups.h"
#endif

#include "printer_main.h"

#ifdef WIN32
#include "printer_win.h"
#endif

typedef struct _PRINTER_DEVICE PRINTER_DEVICE;
struct _PRINTER_DEVICE
{
	DEVICE device;

	rdpPrinter* printer;

	PSLIST_HEADER pIrpList;

	HANDLE event;
	HANDLE stopEvent;

	HANDLE thread;
};

static void printer_process_irp_create(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	rdpPrintJob* printjob = NULL;

	if (printer_dev->printer)
		printjob = printer_dev->printer->CreatePrintJob(printer_dev->printer, irp->devman->id_sequence++);

	if (printjob)
	{
		Stream_Write_UINT32(irp->output, printjob->id); /* FileId */

		DEBUG_SVC("printjob id: %d", printjob->id);
	}
	else
	{
		Stream_Write_UINT32(irp->output, 0); /* FileId */
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

	if (!printjob)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;

		DEBUG_WARN("printjob id %d not found.", irp->FileId);
	}
	else
	{
		printjob->Close(printjob);

		DEBUG_SVC("printjob id %d closed.", irp->FileId);
	}

	Stream_Zero(irp->output, 4); /* Padding(4) */

	irp->Complete(irp);
}

static void printer_process_irp_write(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	UINT32 Length;
	UINT64 Offset;
	rdpPrintJob* printjob = NULL;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	Stream_Seek(irp->input, 20); /* Padding */

	if (printer_dev->printer)
		printjob = printer_dev->printer->FindPrintJob(printer_dev->printer, irp->FileId);

	if (!printjob)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;

		DEBUG_WARN("printjob id %d not found.", irp->FileId);
	}
	else
	{
		printjob->Write(printjob, Stream_Pointer(irp->input), Length);

		DEBUG_SVC("printjob id %d written %d bytes.", irp->FileId, Length);
	}

	Stream_Write_UINT32(irp->output, Length);
	Stream_Write_UINT8(irp->output, 0); /* Padding */

	irp->Complete(irp);
}

static void printer_process_irp_device_control(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	Stream_Write_UINT32(irp->output, 0); /* OutputBufferLength */
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

		case IRP_MJ_DEVICE_CONTROL:
			printer_process_irp_device_control(printer_dev, irp);
			break;

		default:
			DEBUG_WARN("MajorFunction 0x%X not supported", irp->MajorFunction);
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			irp->Complete(irp);
			break;
	}
}

static void* printer_thread_func(void* arg)
{
	IRP* irp;
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*) arg;
	HANDLE obj[] = {printer_dev->event, printer_dev->stopEvent};

	while (1)
	{
		DWORD rc = WaitForMultipleObjects(2, obj, FALSE, INFINITE);

		if (rc == WAIT_OBJECT_0 + 1)
			break;
		else if( rc != WAIT_OBJECT_0 )
			continue;

		ResetEvent(printer_dev->event);

		irp = (IRP*) InterlockedPopEntrySList(printer_dev->pIrpList);

		if (irp == NULL)
			break;

		printer_process_irp(printer_dev, irp);
	}

	ExitThread(0);

	return NULL;
}

static void printer_irp_request(DEVICE* device, IRP* irp)
{
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*) device;

	InterlockedPushEntrySList(printer_dev->pIrpList, &(irp->ItemEntry));

	SetEvent(printer_dev->event);
}

static void printer_free(DEVICE* device)
{
	IRP* irp;
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*) device;

	SetEvent(printer_dev->stopEvent);
	WaitForSingleObject(printer_dev->thread, INFINITE);

	while ((irp = (IRP*) InterlockedPopEntrySList(printer_dev->pIrpList)) != NULL)
		irp->Discard(irp);

	CloseHandle(printer_dev->thread);
	CloseHandle(printer_dev->stopEvent);
	CloseHandle(printer_dev->event);

	_aligned_free(printer_dev->pIrpList);

	if (printer_dev->printer)
		printer_dev->printer->Free(printer_dev->printer);

	free(printer_dev->device.name);

	free(printer_dev);
}

void printer_register(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints, rdpPrinter* printer)
{
	char* port;
	UINT32 Flags;
	int DriverNameLen;
	WCHAR* DriverName = NULL;
	int PrintNameLen;
	WCHAR* PrintName = NULL;
	UINT32 CachedFieldsLen;
	BYTE* CachedPrinterConfigData;
	PRINTER_DEVICE* printer_dev;

	port = malloc(10);
	snprintf(port, 10, "PRN%d", printer->id);

	printer_dev = (PRINTER_DEVICE*) malloc(sizeof(PRINTER_DEVICE));
	ZeroMemory(printer_dev, sizeof(PRINTER_DEVICE));

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

	DriverNameLen = ConvertToUnicode(CP_UTF8, 0, printer->driver, -1, &DriverName, 0) * 2;
	PrintNameLen = ConvertToUnicode(CP_UTF8, 0, printer->name, -1, &PrintName, 0) * 2;

	printer_dev->device.data = Stream_New(NULL, 28 + DriverNameLen + PrintNameLen + CachedFieldsLen);

	Stream_Write_UINT32(printer_dev->device.data, Flags);
	Stream_Write_UINT32(printer_dev->device.data, 0); /* CodePage, reserved */
	Stream_Write_UINT32(printer_dev->device.data, 0); /* PnPNameLen */
	Stream_Write_UINT32(printer_dev->device.data, DriverNameLen + 2);
	Stream_Write_UINT32(printer_dev->device.data, PrintNameLen + 2);
	Stream_Write_UINT32(printer_dev->device.data, CachedFieldsLen);
	Stream_Write(printer_dev->device.data, DriverName, DriverNameLen);
	Stream_Write_UINT16(printer_dev->device.data, 0);
	Stream_Write(printer_dev->device.data, PrintName, PrintNameLen);
	Stream_Write_UINT16(printer_dev->device.data, 0);

	if (CachedFieldsLen > 0)
	{
		Stream_Write(printer_dev->device.data, CachedPrinterConfigData, CachedFieldsLen);
	}

	free(DriverName);
	free(PrintName);

	printer_dev->pIrpList = (PSLIST_HEADER) _aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);
	InitializeSListHead(printer_dev->pIrpList);

	printer_dev->event = CreateEvent(NULL, TRUE, FALSE, NULL);
	printer_dev->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) printer_dev);

	printer_dev->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) printer_thread_func, (void*) printer_dev, 0, NULL);
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	printer_DeviceServiceEntry
#endif

int DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	int i;
	char* name;
	char* driver_name;
	rdpPrinter* printer;
	rdpPrinter** printers;
	RDPDR_PRINTER* device;
	rdpPrinterDriver* driver = NULL;

#ifdef WITH_CUPS
	driver = printer_cups_get_driver();
#endif

#ifdef WIN32
	driver = printer_win_get_driver();
#endif

	if (driver == NULL)
	{
		DEBUG_WARN("no driver");
		return 1;
	}

	device = (RDPDR_PRINTER*) pEntryPoints->device;
	name = device->Name;
	driver_name = device->DriverName;

	if (name && name[0])
	{
		printer = driver->GetPrinter(driver, name);

		if (!printer)
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

		free(printers);
	}

	return 0;
}
