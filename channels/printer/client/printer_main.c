/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Print Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Armin Novak <armin.novak@gmail.com>
 * Copyright 2016 David PHAM-VAN <d.phamvan@inuvika.com>
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

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/string.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/interlocked.h>
#include <winpr/path.h>

#include <freerdp/channels/rdpdr.h>
#include <freerdp/crypto/crypto.h>

#include "../printer.h"

#include <freerdp/client/printer.h>

#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("printer.client")

typedef struct
{
	DEVICE device;

	rdpPrinter* printer;

	WINPR_PSLIST_HEADER pIrpList;

	HANDLE event;
	HANDLE stopEvent;

	HANDLE thread;
	rdpContext* rdpcontext;
	char port[64];
} PRINTER_DEVICE;

typedef enum
{
	PRN_CONF_PORT = 0,
	PRN_CONF_PNP = 1,
	PRN_CONF_DRIVER = 2,
	PRN_CONF_DATA = 3
} prn_conf_t;

static const char* filemap[] = { "PortDosName", "PnPName", "DriverName",
	                             "CachedPrinterConfigData" };

static char* get_printer_config_path(const rdpSettings* settings, const WCHAR* name, size_t length)
{
	const char* path = freerdp_settings_get_string(settings, FreeRDP_ConfigPath);
	char* dir = GetCombinedPath(path, "printers");
	char* bname = crypto_base64_encode((const BYTE*)name, length);
	char* config = GetCombinedPath(dir, bname);

	if (config && !winpr_PathFileExists(config))
	{
		if (!winpr_PathMakePath(config, NULL))
		{
			free(config);
			config = NULL;
		}
	}

	free(dir);
	free(bname);
	return config;
}

static BOOL printer_write_setting(const char* path, prn_conf_t type, const void* data,
                                  size_t length)
{
	DWORD written = 0;
	BOOL rc = FALSE;
	HANDLE file;
	size_t b64len;
	char* base64 = NULL;
	const char* name = filemap[type];
	char* abs = GetCombinedPath(path, name);

	if (!abs || (length > INT32_MAX))
	{
		free(abs);
		return FALSE;
	}

	file = CreateFileA(abs, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	free(abs);

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;

	if (length > 0)
	{
		base64 = crypto_base64_encode(data, length);

		if (!base64)
			goto fail;

		/* base64 char represents 6bit -> 4*(n/3) is the length which is
		 * always smaller than 2*n */
		b64len = strnlen(base64, 2 * length);
		rc = WriteFile(file, base64, b64len, &written, NULL);

		if (b64len != written)
			rc = FALSE;
	}
	else
		rc = TRUE;

fail:
	CloseHandle(file);
	free(base64);
	return rc;
}

static BOOL printer_config_valid(const char* path)
{
	if (!path)
		return FALSE;

	if (!winpr_PathFileExists(path))
		return FALSE;

	return TRUE;
}

static BOOL printer_read_setting(const char* path, prn_conf_t type, void** data, UINT32* length)
{
	DWORD lowSize, highSize;
	DWORD read = 0;
	BOOL rc = FALSE;
	HANDLE file;
	char* fdata = NULL;
	const char* name = filemap[type];
	char* abs = GetCombinedPath(path, name);

	if (!abs)
		return FALSE;

	file = CreateFileA(abs, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	free(abs);

	if (file == INVALID_HANDLE_VALUE)
		return FALSE;

	lowSize = GetFileSize(file, &highSize);

	if ((lowSize == INVALID_FILE_SIZE) || (highSize != 0))
		goto fail;

	if (lowSize != 0)
	{
		fdata = malloc(lowSize);

		if (!fdata)
			goto fail;

		rc = ReadFile(file, fdata, lowSize, &read, NULL);

		if (lowSize != read)
			rc = FALSE;
	}

fail:
	CloseHandle(file);

	if (rc && (lowSize <= INT_MAX))
	{
		size_t blen = 0;
		crypto_base64_decode(fdata, lowSize, (BYTE**)data, &blen);

		if (*data && (blen > 0))
			*length = (UINT32)blen;
		else
		{
			rc = FALSE;
			*length = 0;
		}
	}
	else
	{
		*length = 0;
		*data = NULL;
	}

	free(fdata);
	return rc;
}

static BOOL printer_save_to_config(const rdpSettings* settings, const char* PortDosName,
                                   size_t PortDosNameLen, const WCHAR* PnPName, size_t PnPNameLen,
                                   const WCHAR* DriverName, size_t DriverNameLen,
                                   const WCHAR* PrinterName, size_t PrintNameLen,
                                   const BYTE* CachedPrinterConfigData, size_t CacheFieldsLen)
{
	BOOL rc = FALSE;
	char* path = get_printer_config_path(settings, PrinterName, PrintNameLen);

	if (!path)
		goto fail;

	if (!printer_write_setting(path, PRN_CONF_PORT, PortDosName, PortDosNameLen))
		goto fail;

	if (!printer_write_setting(path, PRN_CONF_PNP, PnPName, PnPNameLen))
		goto fail;

	if (!printer_write_setting(path, PRN_CONF_DRIVER, DriverName, DriverNameLen))
		goto fail;

	if (!printer_write_setting(path, PRN_CONF_DATA, CachedPrinterConfigData, CacheFieldsLen))
		goto fail;

fail:
	free(path);
	return rc;
}

static BOOL printer_update_to_config(const rdpSettings* settings, const WCHAR* name, size_t length,
                                     const BYTE* data, size_t datalen)
{
	BOOL rc = FALSE;
	char* path = get_printer_config_path(settings, name, length);
	rc = printer_write_setting(path, PRN_CONF_DATA, data, datalen);
	free(path);
	return rc;
}

static BOOL printer_remove_config(const rdpSettings* settings, const WCHAR* name, size_t length)
{
	BOOL rc = FALSE;
	char* path = get_printer_config_path(settings, name, length);

	if (!printer_config_valid(path))
		goto fail;

	rc = winpr_RemoveDirectory(path);
fail:
	free(path);
	return rc;
}

static BOOL printer_move_config(const rdpSettings* settings, const WCHAR* oldName, size_t oldLength,
                                const WCHAR* newName, size_t newLength)
{
	BOOL rc = FALSE;
	char* oldPath = get_printer_config_path(settings, oldName, oldLength);
	char* newPath = get_printer_config_path(settings, newName, newLength);

	if (printer_config_valid(oldPath))
		rc = winpr_MoveFile(oldPath, newPath);

	free(oldPath);
	free(newPath);
	return rc;
}

static BOOL printer_load_from_config(const rdpSettings* settings, rdpPrinter* printer,
                                     PRINTER_DEVICE* printer_dev)
{
	BOOL res = FALSE;
	WCHAR* wname = NULL;
	size_t wlen;
	char* path = NULL;
	UINT32 flags = 0;
	void* DriverName = NULL;
	UINT32 DriverNameLen = 0;
	void* PnPName = NULL;
	UINT32 PnPNameLen = 0;
	void* CachedPrinterConfigData = NULL;
	UINT32 CachedFieldsLen = 0;
	UINT32 PrinterNameLen = 0;
	WCHAR* wptr = NULL;

	if (!settings || !printer || !printer->name)
		return FALSE;

	wname = ConvertUtf8ToWCharAlloc(printer->name, &wlen);

	if (!wname)
		goto fail;

	wlen++;
	path = get_printer_config_path(settings, wname, wlen * sizeof(WCHAR));
	PrinterNameLen = wlen * sizeof(WCHAR);

	if (!path)
		goto fail;

	if (printer->is_default)
		flags |= RDPDR_PRINTER_ANNOUNCE_FLAG_DEFAULTPRINTER;

	if (!printer_read_setting(path, PRN_CONF_PNP, &PnPName, &PnPNameLen))
	{
	}

	if (!printer_read_setting(path, PRN_CONF_DRIVER, &DriverName, &DriverNameLen))
	{
		size_t len = 0;
		DriverName = ConvertUtf8ToWCharAlloc(printer->driver, &len);
		if (!DriverName)
			goto fail;
		DriverNameLen = (len + 1) * sizeof(WCHAR);
	}

	if (!printer_read_setting(path, PRN_CONF_DATA, &CachedPrinterConfigData, &CachedFieldsLen))
	{
	}

	Stream_SetPosition(printer_dev->device.data, 0);

	if (!Stream_EnsureRemainingCapacity(printer_dev->device.data, 24))
		goto fail;

	Stream_Write_UINT32(printer_dev->device.data, flags);
	Stream_Write_UINT32(printer_dev->device.data, 0);          /* CodePage, reserved */
	Stream_Write_UINT32(printer_dev->device.data, PnPNameLen); /* PnPNameLen */
	Stream_Write_UINT32(printer_dev->device.data, DriverNameLen);
	Stream_Write_UINT32(printer_dev->device.data, PrinterNameLen);
	Stream_Write_UINT32(printer_dev->device.data, CachedFieldsLen);

	if (!Stream_EnsureRemainingCapacity(printer_dev->device.data, PnPNameLen))
		goto fail;

	if (PnPNameLen > 0)
		Stream_Write(printer_dev->device.data, PnPName, PnPNameLen);

	if (!Stream_EnsureRemainingCapacity(printer_dev->device.data, DriverNameLen))
		goto fail;

	Stream_Write(printer_dev->device.data, DriverName, DriverNameLen);

	if (!Stream_EnsureRemainingCapacity(printer_dev->device.data, PrinterNameLen))
		goto fail;

	for (wptr = wname; (wptr = _wcschr(wptr, L'\\'));)
		*wptr = L'_';
	Stream_Write(printer_dev->device.data, wname, PrinterNameLen);

	if (!Stream_EnsureRemainingCapacity(printer_dev->device.data, CachedFieldsLen))
		goto fail;

	Stream_Write(printer_dev->device.data, CachedPrinterConfigData, CachedFieldsLen);
	res = TRUE;
fail:
	free(path);
	free(wname);
	free(PnPName);
	free(DriverName);
	free(CachedPrinterConfigData);
	return res;
}

static BOOL printer_save_default_config(const rdpSettings* settings, rdpPrinter* printer)
{
	BOOL res = FALSE;
	WCHAR* wname = NULL;
	WCHAR* driver = NULL;
	size_t wlen, dlen;
	char* path = NULL;

	if (!settings || !printer || !printer->name || !printer->driver)
		return FALSE;

	wname = ConvertUtf8ToWCharAlloc(printer->name, NULL);

	if (wname)
		goto fail;

	driver = ConvertUtf8ToWCharAlloc(printer->driver, NULL);

	if (driver)
		goto fail;

	wlen = _wcslen(wname) + 1;
	dlen = _wcslen(driver) + 1;
	path = get_printer_config_path(settings, wname, wlen * sizeof(WCHAR));

	if (!path)
		goto fail;

	if (dlen > 1)
	{
		if (!printer_write_setting(path, PRN_CONF_DRIVER, driver, dlen * sizeof(WCHAR)))
			goto fail;
	}

	res = TRUE;
fail:
	free(path);
	free(wname);
	free(driver);
	return res;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_process_irp_create(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	rdpPrintJob* printjob = NULL;

	WINPR_ASSERT(printer_dev);
	WINPR_ASSERT(irp);

	if (printer_dev->printer)
	{
		WINPR_ASSERT(printer_dev->printer->CreatePrintJob);
		printjob =
		    printer_dev->printer->CreatePrintJob(printer_dev->printer, irp->devman->id_sequence++);
	}

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

	WINPR_ASSERT(printer_dev);
	WINPR_ASSERT(irp);

	if (printer_dev->printer)
	{
		WINPR_ASSERT(printer_dev->printer->FindPrintJob);
		printjob = printer_dev->printer->FindPrintJob(printer_dev->printer, irp->FileId);
	}

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
	void* ptr;

	WINPR_ASSERT(printer_dev);
	WINPR_ASSERT(irp);

	if (!Stream_CheckAndLogRequiredLength(TAG, irp->input, 32))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	Stream_Seek(irp->input, 20); /* Padding */
	ptr = Stream_Pointer(irp->input);
	if (!Stream_SafeSeek(irp->input, Length))
		return ERROR_INVALID_DATA;
	if (printer_dev->printer)
	{
		WINPR_ASSERT(printer_dev->printer->FindPrintJob);
		printjob = printer_dev->printer->FindPrintJob(printer_dev->printer, irp->FileId);
	}

	if (!printjob)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;
	}
	else
	{
		error = printjob->Write(printjob, ptr, Length);
	}

	if (error)
	{
		WLog_ERR(TAG, "printjob->Write failed with error %" PRIu32 "!", error);
		return error;
	}

	Stream_Write_UINT32(irp->output, Length);
	Stream_Write_UINT8(irp->output, 0); /* Padding */

	WINPR_ASSERT(irp->Complete);
	return irp->Complete(irp);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_process_irp_device_control(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	WINPR_ASSERT(printer_dev);
	WINPR_ASSERT(irp);

	Stream_Write_UINT32(irp->output, 0); /* OutputBufferLength */

	WINPR_ASSERT(irp->Complete);
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

	WINPR_ASSERT(printer_dev);
	WINPR_ASSERT(irp);

	switch (irp->MajorFunction)
	{
		case IRP_MJ_CREATE:
			if ((error = printer_process_irp_create(printer_dev, irp)))
			{
				WLog_ERR(TAG, "printer_process_irp_create failed with error %" PRIu32 "!", error);
				return error;
			}

			break;

		case IRP_MJ_CLOSE:
			if ((error = printer_process_irp_close(printer_dev, irp)))
			{
				WLog_ERR(TAG, "printer_process_irp_close failed with error %" PRIu32 "!", error);
				return error;
			}

			break;

		case IRP_MJ_WRITE:
			if ((error = printer_process_irp_write(printer_dev, irp)))
			{
				WLog_ERR(TAG, "printer_process_irp_write failed with error %" PRIu32 "!", error);
				return error;
			}

			break;

		case IRP_MJ_DEVICE_CONTROL:
			if ((error = printer_process_irp_device_control(printer_dev, irp)))
			{
				WLog_ERR(TAG, "printer_process_irp_device_control failed with error %" PRIu32 "!",
				         error);
				return error;
			}

			break;

		default:
			irp->IoStatus = STATUS_NOT_SUPPORTED;
			WINPR_ASSERT(irp->Complete);
			return irp->Complete(irp);
	}

	return CHANNEL_RC_OK;
}

static DWORD WINAPI printer_thread_func(LPVOID arg)
{
	IRP* irp;
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*)arg;
	UINT error = CHANNEL_RC_OK;

	WINPR_ASSERT(printer_dev);

	while (1)
	{
		HANDLE obj[] = { printer_dev->event, printer_dev->stopEvent };
		DWORD rc = WaitForMultipleObjects(ARRAYSIZE(obj), obj, FALSE, INFINITE);

		if (rc == WAIT_FAILED)
		{
			error = GetLastError();
			WLog_ERR(TAG, "WaitForMultipleObjects failed with error %" PRIu32 "!", error);
			break;
		}

		if (rc == WAIT_OBJECT_0 + 1)
			break;
		else if (rc != WAIT_OBJECT_0)
			continue;

		ResetEvent(printer_dev->event);
		irp = (IRP*)InterlockedPopEntrySList(printer_dev->pIrpList);

		if (irp == NULL)
		{
			WLog_ERR(TAG, "InterlockedPopEntrySList failed!");
			error = ERROR_INTERNAL_ERROR;
			break;
		}

		if ((error = printer_process_irp(printer_dev, irp)))
		{
			WLog_ERR(TAG, "printer_process_irp failed with error %" PRIu32 "!", error);
			break;
		}
	}

	if (error && printer_dev->rdpcontext)
		setChannelError(printer_dev->rdpcontext, error, "printer_thread_func reported an error");

	ExitThread(error);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_irp_request(DEVICE* device, IRP* irp)
{
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*)device;

	WINPR_ASSERT(printer_dev);
	WINPR_ASSERT(irp);

	InterlockedPushEntrySList(printer_dev->pIrpList, &(irp->ItemEntry));
	SetEvent(printer_dev->event);
	return CHANNEL_RC_OK;
}

static UINT printer_custom_component(DEVICE* device, UINT16 component, UINT16 packetId, wStream* s)
{
	UINT32 eventID;
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*)device;

	WINPR_ASSERT(printer_dev);
	WINPR_ASSERT(printer_dev->rdpcontext);

	const rdpSettings* settings = printer_dev->rdpcontext->settings;
	WINPR_ASSERT(settings);

	if (component != RDPDR_CTYP_PRN)
		return ERROR_INVALID_DATA;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, eventID);

	switch (packetId)
	{
		case PAKID_PRN_CACHE_DATA:
			switch (eventID)
			{
				case RDPDR_ADD_PRINTER_EVENT:
				{
					char PortDosName[8];
					UINT32 PnPNameLen, DriverNameLen, PrintNameLen, CacheFieldsLen;
					const WCHAR *PnPName, *DriverName, *PrinterName;
					const BYTE* CachedPrinterConfigData;

					if (!Stream_CheckAndLogRequiredLength(TAG, s, 24))
						return ERROR_INVALID_DATA;

					Stream_Read(s, PortDosName, sizeof(PortDosName));
					Stream_Read_UINT32(s, PnPNameLen);
					Stream_Read_UINT32(s, DriverNameLen);
					Stream_Read_UINT32(s, PrintNameLen);
					Stream_Read_UINT32(s, CacheFieldsLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, PnPNameLen))
						return ERROR_INVALID_DATA;

					PnPName = (const WCHAR*)Stream_Pointer(s);
					Stream_Seek(s, PnPNameLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, DriverNameLen))
						return ERROR_INVALID_DATA;

					DriverName = (const WCHAR*)Stream_Pointer(s);
					Stream_Seek(s, DriverNameLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, PrintNameLen))
						return ERROR_INVALID_DATA;

					PrinterName = (const WCHAR*)Stream_Pointer(s);
					Stream_Seek(s, PrintNameLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, CacheFieldsLen))
						return ERROR_INVALID_DATA;

					CachedPrinterConfigData = Stream_Pointer(s);
					Stream_Seek(s, CacheFieldsLen);

					if (!printer_save_to_config(settings, PortDosName, sizeof(PortDosName), PnPName,
					                            PnPNameLen, DriverName, DriverNameLen, PrinterName,
					                            PrintNameLen, CachedPrinterConfigData,
					                            CacheFieldsLen))
						return ERROR_INTERNAL_ERROR;
				}
				break;

				case RDPDR_UPDATE_PRINTER_EVENT:
				{
					UINT32 PrinterNameLen, ConfigDataLen;
					const WCHAR* PrinterName;
					const BYTE* ConfigData;

					if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
						return ERROR_INVALID_DATA;

					Stream_Read_UINT32(s, PrinterNameLen);
					Stream_Read_UINT32(s, ConfigDataLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, PrinterNameLen))
						return ERROR_INVALID_DATA;

					PrinterName = (const WCHAR*)Stream_Pointer(s);
					Stream_Seek(s, PrinterNameLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, ConfigDataLen))
						return ERROR_INVALID_DATA;

					ConfigData = Stream_Pointer(s);
					Stream_Seek(s, ConfigDataLen);

					if (!printer_update_to_config(settings, PrinterName, PrinterNameLen, ConfigData,
					                              ConfigDataLen))
						return ERROR_INTERNAL_ERROR;
				}
				break;

				case RDPDR_DELETE_PRINTER_EVENT:
				{
					UINT32 PrinterNameLen;
					const WCHAR* PrinterName;

					if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
						return ERROR_INVALID_DATA;

					Stream_Read_UINT32(s, PrinterNameLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, PrinterNameLen))
						return ERROR_INVALID_DATA;

					PrinterName = (const WCHAR*)Stream_Pointer(s);
					Stream_Seek(s, PrinterNameLen);
					printer_remove_config(settings, PrinterName, PrinterNameLen);
				}
				break;

				case RDPDR_RENAME_PRINTER_EVENT:
				{
					UINT32 OldPrinterNameLen, NewPrinterNameLen;
					const WCHAR* OldPrinterName;
					const WCHAR* NewPrinterName;

					if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
						return ERROR_INVALID_DATA;

					Stream_Read_UINT32(s, OldPrinterNameLen);
					Stream_Read_UINT32(s, NewPrinterNameLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, OldPrinterNameLen))
						return ERROR_INVALID_DATA;

					OldPrinterName = (const WCHAR*)Stream_Pointer(s);
					Stream_Seek(s, OldPrinterNameLen);

					if (!Stream_CheckAndLogRequiredLength(TAG, s, NewPrinterNameLen))
						return ERROR_INVALID_DATA;

					NewPrinterName = (const WCHAR*)Stream_Pointer(s);
					Stream_Seek(s, NewPrinterNameLen);

					if (!printer_move_config(settings, OldPrinterName, OldPrinterNameLen,
					                         NewPrinterName, NewPrinterNameLen))
						return ERROR_INTERNAL_ERROR;
				}
				break;

				default:
					WLog_ERR(TAG, "Unknown cache data eventID: 0x%08" PRIX32 "", eventID);
					return ERROR_INVALID_DATA;
			}

			break;

		case PAKID_PRN_USING_XPS:
		{
			UINT32 flags;

			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				return ERROR_INVALID_DATA;

			Stream_Read_UINT32(s, flags);
			WLog_ERR(TAG,
			         "Ignoring unhandled message PAKID_PRN_USING_XPS [printerID=%08" PRIx32
			         ", flags=%08" PRIx32 "]",
			         eventID, flags);
		}
		break;

		default:
			WLog_ERR(TAG, "Unknown printing component packetID: 0x%04" PRIX16 "", packetId);
			return ERROR_INVALID_DATA;
	}

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
	PRINTER_DEVICE* printer_dev = (PRINTER_DEVICE*)device;
	UINT error;

	WINPR_ASSERT(printer_dev);

	SetEvent(printer_dev->stopEvent);

	if (WaitForSingleObject(printer_dev->thread, INFINITE) == WAIT_FAILED)
	{
		error = GetLastError();
		WLog_ERR(TAG, "WaitForSingleObject failed with error %" PRIu32 "", error);

		/* The analyzer is confused by this premature return value.
		 * Since this case can not be handled gracefully silence the
		 * analyzer here. */
#ifndef __clang_analyzer__
		return error;
#endif
	}

	while ((irp = (IRP*)InterlockedPopEntrySList(printer_dev->pIrpList)) != NULL)
	{
		WINPR_ASSERT(irp->Discard);
		irp->Discard(irp);
	}

	CloseHandle(printer_dev->thread);
	CloseHandle(printer_dev->stopEvent);
	CloseHandle(printer_dev->event);
	winpr_aligned_free(printer_dev->pIrpList);

	if (printer_dev->printer)
	{
		WINPR_ASSERT(printer_dev->printer->ReleaseRef);
		printer_dev->printer->ReleaseRef(printer_dev->printer);
	}

	Stream_Free(printer_dev->device.data, TRUE);
	free(printer_dev);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_register(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints, rdpPrinter* printer)
{
	PRINTER_DEVICE* printer_dev;
	UINT error = ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(printer);

	printer_dev = (PRINTER_DEVICE*)calloc(1, sizeof(PRINTER_DEVICE));

	if (!printer_dev)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	printer_dev->device.data = Stream_New(NULL, 1024);

	if (!printer_dev->device.data)
		goto error_out;

	sprintf_s(printer_dev->port, sizeof(printer_dev->port), "PRN%" PRIdz, printer->id);
	printer_dev->device.type = RDPDR_DTYP_PRINT;
	printer_dev->device.name = printer_dev->port;
	printer_dev->device.IRPRequest = printer_irp_request;
	printer_dev->device.CustomComponentRequest = printer_custom_component;
	printer_dev->device.Free = printer_free;
	printer_dev->rdpcontext = pEntryPoints->rdpcontext;
	printer_dev->printer = printer;
	printer_dev->pIrpList = (WINPR_PSLIST_HEADER)winpr_aligned_malloc(sizeof(WINPR_SLIST_HEADER),
	                                                                  MEMORY_ALLOCATION_ALIGNMENT);

	if (!printer_dev->pIrpList)
	{
		WLog_ERR(TAG, "_aligned_malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	if (!printer_load_from_config(pEntryPoints->rdpcontext->settings, printer, printer_dev))
		goto error_out;

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

	if ((error = pEntryPoints->RegisterDevice(pEntryPoints->devman, &printer_dev->device)))
	{
		WLog_ERR(TAG, "RegisterDevice failed with error %" PRIu32 "!", error);
		goto error_out;
	}

	if (!(printer_dev->thread =
	          CreateThread(NULL, 0, printer_thread_func, (void*)printer_dev, 0, NULL)))
	{
		WLog_ERR(TAG, "CreateThread failed!");
		error = ERROR_INTERNAL_ERROR;
		goto error_out;
	}

	WINPR_ASSERT(printer->AddRef);
	printer->AddRef(printer);
	return CHANNEL_RC_OK;
error_out:
	printer_free(&printer_dev->device);
	return error;
}

static rdpPrinterDriver* printer_load_backend(const char* backend)
{
	typedef rdpPrinterDriver* (*backend_load_t)(void);
	union
	{
		PVIRTUALCHANNELENTRY entry;
		backend_load_t backend;
	} fktconv;

	fktconv.entry = freerdp_load_channel_addin_entry("printer", backend, NULL, 0);
	if (!fktconv.entry)
		return NULL;

	return fktconv.backend();
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT printer_DeviceServiceEntry(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints)
{
	int i;
	char* name;
	char* driver_name;
	BOOL default_backend = TRUE;
	RDPDR_PRINTER* device = NULL;
	rdpPrinterDriver* driver = NULL;
	UINT error = CHANNEL_RC_OK;

	if (!pEntryPoints || !pEntryPoints->device)
		return ERROR_INVALID_PARAMETER;

	device = (RDPDR_PRINTER*)pEntryPoints->device;
	name = device->device.Name;
	driver_name = _strdup(device->DriverName);

	/* Secondary argument is one of the following:
	 *
	 * <driver_name>                ... name of a printer driver
	 * <driver_name>:<backend_name> ... name of a printer driver and local printer backend to use
	 */
	if (driver_name)
	{
		char* sep = strstr(driver_name, ":");
		if (sep)
		{
			const char* backend = sep + 1;
			*sep = '\0';
			driver = printer_load_backend(backend);
			default_backend = FALSE;
		}
	}

	if (!driver && default_backend)
	{
		const char* backend =
#if defined(WITH_CUPS)
		    "cups"
#elif defined(_WIN32)
		    "win"
#else
		    ""
#endif
		    ;

		driver = printer_load_backend(backend);
	}

	if (!driver)
	{
		WLog_ERR(TAG, "Could not get a printer driver!");
		error = CHANNEL_RC_INITIALIZATION_ERROR;
		goto fail;
	}

	if (name && name[0])
	{
		WINPR_ASSERT(driver->GetPrinter);
		rdpPrinter* printer = driver->GetPrinter(driver, name, driver_name, device->IsDefault);

		if (!printer)
		{
			WLog_ERR(TAG, "Could not get printer %s!", name);
			error = CHANNEL_RC_INITIALIZATION_ERROR;
			goto fail;
		}

		WINPR_ASSERT(printer->ReleaseRef);
		if (!printer_save_default_config(pEntryPoints->rdpcontext->settings, printer))
		{
			error = CHANNEL_RC_INITIALIZATION_ERROR;
			printer->ReleaseRef(printer);
			goto fail;
		}

		error = printer_register(pEntryPoints, printer);
		printer->ReleaseRef(printer);
		if (error)
		{
			WLog_ERR(TAG, "printer_register failed with error %" PRIu32 "!", error);
			goto fail;
		}
	}
	else
	{
		WINPR_ASSERT(driver->EnumPrinters);
		rdpPrinter** printers = driver->EnumPrinters(driver);
		rdpPrinter** current = printers;

		for (i = 0; current[i]; i++)
		{
			rdpPrinter* printer = current[i];

			if ((error = printer_register(pEntryPoints, printer)))
			{
				WLog_ERR(TAG, "printer_register failed with error %" PRIu32 "!", error);
				break;
			}
		}

		WINPR_ASSERT(driver->ReleaseEnumPrinters);
		driver->ReleaseEnumPrinters(printers);
	}

fail:
	free(driver_name);
	if (driver)
	{
		WINPR_ASSERT(driver->ReleaseRef);
		driver->ReleaseRef(driver);
	}

	return error;
}
