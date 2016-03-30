/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Print Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Armin Novak <armin.novak@gmail.com>
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
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/interlocked.h>

#include <freerdp/channels/rdpdr.h>

#include "../printer.h"

#ifdef WITH_CUPS
#include "printer_cups.h"
#endif

#include "printer_main.h"

#if defined(_WIN32) && !defined(_UWP)
#include "printer_win.h"
#endif

#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("printer.client")

typedef struct _PRINTER_DEVICE PRINTER_DEVICE;
struct _PRINTER_DEVICE
{
	DEVICE device;

	rdpPrinter* printer;

	PSLIST_HEADER pIrpList;

	HANDLE event;
	HANDLE stopEvent;

	HANDLE thread;
	rdpContext* rdpcontext;
};

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_process_irp_create(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	rdpPrintJob* printjob = NULL;

	if (printer_dev->printer)
		printjob = printer_dev->printer->CreatePrintJob(printer_dev->printer, irp->devman->id_sequence++);

	if (printjob)
	{
		Stream_Write_UINT32(irp->output, printjob->id); /* FileId */
	}
	else
	{
		Stream_Write_UINT32(irp->output, 0); /* FileId */
		irp->IoStatus = STATUS_PRINT_QUEUE_FULL;
	}

	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_process_irp_close(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	rdpPrintJob* printjob = NULL;

	if (printer_dev->printer)
		printjob = printer_dev->printer->FindPrintJob(printer_dev->printer, irp->FileId);

	if (!printjob)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
	}
	else
	{
		printjob->Close(printjob);
	}

	Stream_Zero(irp->output, 4); /* Padding(4) */

	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_process_irp_write(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	UINT32 Length;
	UINT64 Offset;
	rdpPrintJob* printjob = NULL;
	UINT error = CHANNEL_RC_OK;

	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	Stream_Seek(irp->input, 20); /* Padding */

	if (printer_dev->printer)
		printjob = printer_dev->printer->FindPrintJob(printer_dev->printer, irp->FileId);

	if (!printjob)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;
	}
	else
	{
		error = printjob->Write(printjob, Stream_Pointer(irp->input), Length);
	}

	if (error)
	{
		WLog_ERR(TAG, "printjob->Write failed with error %lu!", error);
		return error;
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
static UINT printer_process_irp_device_control(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	Stream_Write_UINT32(irp->output, 0); /* OutputBufferLength */
	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_process_irp(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	UINT error;
	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			if ((error = printer_process_irp_create(printer_dev, irp)))
			{
				WLog_ERR(TAG, "printer_process_irp_create failed with error %lu!", error);
				return error;
			}
			break;

		case IRP_MJ_CLOSE:
			if ((error = printer_process_irp_close(printer_dev, irp)))
			{
				WLog_ERR(TAG, "printer_process_irp_close failed with error %lu!", error);
				return error;
			}
			break;

		case IRP_MJ_WRITE:
			if ((error = printer_process_irp_write(printer_dev, irp)))
			{
				WLog_ERR(TAG, "printer_process_irp_write failed with error %lu!", error);
				return error;
			}
			break;

		case IRP_MJ_DEVICE_CONTROL:
			if ((error = printer_process_irp_device_control(printer_dev, irp)))
			{
				WLog_ERR(TAG, "printer_process_irp_device_control failed with error %lu!", error);
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

static void* printer_thread_func(void* arg)
{
	IRP* irp;
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*) arg;
	HANDLE obj[] = {printer_dev->event, printer_dev->stopEvent};
	UINT error = CHANNEL_RC_OK;

	while (1)
	{
		DWORD rc = WaitForMultipleObjects(2, obj, FALSE, INFINITE);
        if (rc == WAIT_FAILED)
        {
            error = GetLastError();
            WLog_ERR(TAG, "WaitForMultipleObjects failed with error %lu!", error);
            break;
        }

		if (rc == WAIT_OBJECT_0 + 1)
			break;
		else if( rc != WAIT_OBJECT_0 )
			continue;

		ResetEvent(printer_dev->event);

		irp = (IRP*) InterlockedPopEntrySList(printer_dev->pIrpList);

		if (irp == NULL)
		{
			WLog_ERR(TAG, "InterlockedPopEntrySList failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if ((error = printer_process_irp(printer_dev, irp)))
		{
			WLog_ERR(TAG, "printer_process_irp failed with error %d!", error);
			break;
		}
	}
	if (error && printer_dev->rdpcontext)
		setChannelError(printer_dev->rdpcontext, error, "printer_thread_func reported an error");

	ExitThread((DWORD) error);

	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_irp_request(DEVICE* device, IRP* irp)
{
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*) device;

	InterlockedPushEntrySList(printer_dev->pIrpList, &(irp->ItemEntry));

	SetEvent(printer_dev->event);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_free(DEVICE* device)
{
	IRP* irp;
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*) device;
    UINT error;

	SetEvent(printer_dev->stopEvent);
	if (WaitForSingleObject(printer_dev->thread, INFINITE) == WAIT_FAILED)
    {
        error = GetLastError();
        WLog_ERR(TAG, "WaitForSingleObject failed with error %lu", error);
        return error;
    }

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
    return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT printer_register(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints, rdpPrinter* printer)
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
	UINT error;

	port = malloc(10);
	if (!port)
	{
		WLog_ERR(TAG, "malloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}
	sprintf_s(port, 10, "PRN%d", printer->id);

	printer_dev = (PRINTER_DEVICE*) calloc(1, sizeof(PRINTER_DEVICE));
	if (!printer_dev)
	{
		WLog_ERR(TAG, "calloc failed!");
		free(port);
		return CHANNEL_RC_NO_MEMORY;
	}

	printer_dev->device.type = RDPDR_DTYP_PRINT;
	printer_dev->device.name = port;
	printer_dev->device.IRPRequest = printer_irp_request;
	printer_dev->device.Free = printer_free;
	printer_dev->rdpcontext = pEntryPoints->rdpcontext;

	printer_dev->printer = printer;

	CachedFieldsLen = 0;
	CachedPrinterConfigData = NULL;

	Flags = 0;

	if (printer->is_default)
		Flags |= RDPDR_PRINTER_ANNOUNCE_FLAG_DEFAULTPRINTER;

	DriverNameLen = ConvertToUnicode(CP_UTF8, 0, printer->driver, -1, &DriverName, 0) * 2;
	PrintNameLen = ConvertToUnicode(CP_UTF8, 0, printer->name, -1, &PrintName, 0) * 2;

	printer_dev->device.data = Stream_New(NULL, 28 + DriverNameLen + PrintNameLen + CachedFieldsLen);
	if (!printer_dev->device.data)
	{
		WLog_ERR(TAG, "calloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		free(DriverName);
		free(PrintName);
		goto error_out;
	}

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
	if (!printer_dev->pIrpList)
	{
		WLog_ERR(TAG, "_aligned_malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}
	InitializeSListHead(printer_dev->pIrpList);

	if (!(printer_dev->event = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		error = ERROR_INTERNAL_ERROR;
		goto error_out;
	}
	if (!(printer_dev->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
	{
		WLog_ERR(TAG, "CreateEvent failed!");
		error = ERROR_INTERNAL_ERROR;
		goto error_out;
	}

	if ((error = pEntryPoints->RegisterDevice(pEntryPoints->devman, (DEVICE*) printer_dev)))
	{
		WLog_ERR(TAG, "RegisterDevice failed with error %d!", error);
		goto error_out;
	}

	if (!(printer_dev->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) printer_thread_func, (void*) printer_dev, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		error = ERROR_INTERNAL_ERROR;
		goto error_out;
	}

	return CHANNEL_RC_OK;
error_out:
	CloseHandle(printer_dev->stopEvent);
	CloseHandle(printer_dev->event);
	_aligned_free(printer_dev->pIrpList);
	Stream_Free(printer_dev->device.data, TRUE);
	free(printer_dev);
	free(port);
	return error;
}

#ifdef STATIC_CHANNELS
#define DeviceServiceEntry	printer_DeviceServiceEntry
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
	int i;
	char* name;
	char* driver_name;
	rdpPrinter* printer;
	rdpPrinter** printers;
	RDPDR_PRINTER* device;
	rdpPrinterDriver* driver = NULL;
	UINT error;

#ifdef WITH_CUPS
	driver = printer_cups_get_driver();
#endif

#if defined(_WIN32) && !defined(_UWP)
	driver = printer_win_get_driver();
#endif

	if (!driver)
	{
		WLog_ERR(TAG, "Could not get a printer driver!");
		return CHANNEL_RC_INITIALIZATION_ERROR;
	}

	device = (RDPDR_PRINTER*) pEntryPoints->device;
	name = device->Name;
	driver_name = device->DriverName;

	if (name && name[0])
	{
		printer = driver->GetPrinter(driver, name, driver_name);

		if (!printer)
		{
			WLog_ERR(TAG, "Could not get printer %s!", name);
			return CHANNEL_RC_INITIALIZATION_ERROR;
		}

		if ((error = printer_register(pEntryPoints, printer)))
		{
			WLog_ERR(TAG, "printer_register failed with error %lu!", error);
			return error;
		}
	}
	else
	{
		printers = driver->EnumPrinters(driver);

		for (i = 0; printers[i]; i++)
		{
			printer = printers[i];
			if ((error = printer_register(pEntryPoints, printer)))
			{
				WLog_ERR(TAG, "printer_register failed with error %lu!", error);
				free(printers);
				return error;

			}
		}

		free(printers);
	}

	return CHANNEL_RC_OK;
}
