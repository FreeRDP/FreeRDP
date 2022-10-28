/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Print Virtual Channel - WIN driver
 *
 * Copyright 2012 Gerald Richter
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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/windows.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winspool.h>

#include <freerdp/client/printer.h>

#define WIDEN_INT(x) L##x
#define WIDEN(x) WIDEN_INT(x)
#define PRINTER_TAG CHANNELS_TAG("printer.client")
#ifdef WITH_DEBUG_WINPR
#define DEBUG_WINPR(...) WLog_DBG(PRINTER_TAG, __VA_ARGS__)
#else
#define DEBUG_WINPR(...) \
	do                   \
	{                    \
	} while (0)
#endif

typedef struct
{
	rdpPrinterDriver driver;

	size_t id_sequence;
	size_t references;
} rdpWinPrinterDriver;

typedef struct
{
	rdpPrintJob printjob;
	DOC_INFO_1 di;
	DWORD handle;

	void* printjob_object;
	int printjob_id;
} rdpWinPrintJob;

typedef struct
{
	rdpPrinter printer;
	HANDLE hPrinter;
	rdpWinPrintJob* printjob;
} rdpWinPrinter;

static WCHAR* printer_win_get_printjob_name(size_t id)
{
	time_t tt;
	struct tm tres;
	errno_t err;
	WCHAR* str;
	size_t len = 1024;
	int rc;

	tt = time(NULL);
	err = localtime_s(&tres, &tt);

	str = calloc(len, sizeof(WCHAR));
	if (!str)
		return NULL;

	rc = swprintf_s(str, len,
	                WIDEN("FreeRDP Print %04d-%02d-%02d% 02d-%02d-%02d - Job %") WIDEN(PRIuz)
	                    WIDEN("\0"),
	                tres.tm_year + 1900, tres.tm_mon + 1, tres.tm_mday, tres.tm_hour, tres.tm_min,
	                tres.tm_sec, id);

	return str;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_win_write_printjob(rdpPrintJob* printjob, const BYTE* data, size_t size)
{
	rdpWinPrinter* printer;
	LPCVOID pBuf = data;
	DWORD cbBuf = size;
	DWORD pcWritten;

	if (!printjob || !data)
		return ERROR_BAD_ARGUMENTS;

	printer = (rdpWinPrinter*)printjob->printer;
	if (!printer)
		return ERROR_BAD_ARGUMENTS;

	if (!WritePrinter(printer->hPrinter, pBuf, cbBuf, &pcWritten))
		return ERROR_INTERNAL_ERROR;
	return CHANNEL_RC_OK;
}

static void printer_win_close_printjob(rdpPrintJob* printjob)
{
	rdpWinPrintJob* win_printjob = (rdpWinPrintJob*)printjob;
	rdpWinPrinter* win_printer;

	if (!printjob)
		return;

	win_printer = (rdpWinPrinter*)printjob->printer;
	if (!win_printer)
		return;

	if (!EndPagePrinter(win_printer->hPrinter))
	{
	}

	if (!ClosePrinter(win_printer->hPrinter))
	{
	}

	win_printer->printjob = NULL;

	free(win_printjob->di.pDocName);
	free(win_printjob);
}

static rdpPrintJob* printer_win_create_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*)printer;
	rdpWinPrintJob* win_printjob;

	if (win_printer->printjob != NULL)
		return NULL;

	win_printjob = (rdpWinPrintJob*)calloc(1, sizeof(rdpWinPrintJob));
	if (!win_printjob)
		return NULL;

	win_printjob->printjob.id = id;
	win_printjob->printjob.printer = printer;
	win_printjob->di.pDocName = printer_win_get_printjob_name(id);
	win_printjob->di.pDatatype = NULL;
	win_printjob->di.pOutputFile = NULL;

	win_printjob->handle = StartDocPrinter(win_printer->hPrinter, 1, (LPBYTE) & (win_printjob->di));

	if (!win_printjob->handle)
	{
		free(win_printjob->di.pDocName);
		free(win_printjob);
		return NULL;
	}

	if (!StartPagePrinter(win_printer->hPrinter))
	{
		free(win_printjob->di.pDocName);
		free(win_printjob);
		return NULL;
	}

	win_printjob->printjob.Write = printer_win_write_printjob;
	win_printjob->printjob.Close = printer_win_close_printjob;

	win_printer->printjob = win_printjob;

	return &win_printjob->printjob;
}

static rdpPrintJob* printer_win_find_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*)printer;

	if (!win_printer->printjob)
		return NULL;

	if (win_printer->printjob->printjob.id != id)
		return NULL;

	return (rdpPrintJob*)win_printer->printjob;
}

static void printer_win_free_printer(rdpPrinter* printer)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*)printer;

	if (win_printer->printjob)
		win_printer->printjob->printjob.Close((rdpPrintJob*)win_printer->printjob);

	if (printer->backend)
		printer->backend->ReleaseRef(printer->backend);

	free(printer->name);
	free(printer->driver);
	free(printer);
}

static void printer_win_add_ref_printer(rdpPrinter* printer)
{
	if (printer)
		printer->references++;
}

static void printer_win_release_ref_printer(rdpPrinter* printer)
{
	if (!printer)
		return;
	if (printer->references <= 1)
		printer_win_free_printer(printer);
	else
		printer->references--;
}

static rdpPrinter* printer_win_new_printer(rdpWinPrinterDriver* win_driver, const WCHAR* name,
                                           const WCHAR* drivername, BOOL is_default)
{
	rdpWinPrinter* win_printer;
	DWORD needed = 0;
	PRINTER_INFO_2* prninfo = NULL;

	if (!name)
		return NULL;

	win_printer = (rdpWinPrinter*)calloc(1, sizeof(rdpWinPrinter));
	if (!win_printer)
		return NULL;

	win_printer->printer.backend = &win_driver->driver;
	win_printer->printer.id = win_driver->id_sequence++;
	win_printer->printer.name = ConvertWCharToUtf8Alloc(name, NULL);
	if (!win_printer->printer.name)
		goto fail;

	if (!win_printer->printer.name)
		goto fail;
	win_printer->printer.is_default = is_default;

	win_printer->printer.CreatePrintJob = printer_win_create_printjob;
	win_printer->printer.FindPrintJob = printer_win_find_printjob;
	win_printer->printer.AddRef = printer_win_add_ref_printer;
	win_printer->printer.ReleaseRef = printer_win_release_ref_printer;

	if (!OpenPrinter(name, &(win_printer->hPrinter), NULL))
		goto fail;

	/* How many memory should be allocated for printer data */
	GetPrinter(win_printer->hPrinter, 2, (LPBYTE)prninfo, 0, &needed);
	if (needed == 0)
		goto fail;

	prninfo = (PRINTER_INFO_2*)GlobalAlloc(GPTR, needed);
	if (!prninfo)
		goto fail;

	if (!GetPrinter(win_printer->hPrinter, 2, (LPBYTE)prninfo, needed, &needed))
	{
		GlobalFree(prninfo);
		goto fail;
	}

	if (drivername)
		win_printer->printer.driver = ConvertWCharToUtf8Alloc(drivername, NULL);
	else
		win_printer->printer.driver = ConvertWCharToUtf8Alloc(prninfo->pDriverName, NULL);
	GlobalFree(prninfo);
	if (!win_printer->printer.driver)
		goto fail;

	win_printer->printer.AddRef(&win_printer->printer);
	win_printer->printer.backend->AddRef(win_printer->printer.backend);
	return &win_printer->printer;

fail:
	printer_win_free_printer(&win_printer->printer);
	return NULL;
}

static void printer_win_release_enum_printers(rdpPrinter** printers)
{
	rdpPrinter** cur = printers;

	while ((cur != NULL) && ((*cur) != NULL))
	{
		if ((*cur)->ReleaseRef)
			(*cur)->ReleaseRef(*cur);
		cur++;
	}
	free(printers);
}

static rdpPrinter** printer_win_enum_printers(rdpPrinterDriver* driver)
{
	rdpPrinter** printers;
	int num_printers;
	int i;
	PRINTER_INFO_2* prninfo = NULL;
	DWORD needed, returned;
	BOOL haveDefault = FALSE;
	LPWSTR defaultPrinter = NULL;

	GetDefaultPrinter(NULL, &needed);
	if (needed)
	{
		defaultPrinter = (LPWSTR)calloc(needed, sizeof(WCHAR));

		if (!defaultPrinter)
			return NULL;

		if (!GetDefaultPrinter(defaultPrinter, &needed))
			defaultPrinter[0] = '\0';
	}

	/* find required size for the buffer */
	EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed,
	             &returned);

	/* allocate array of PRINTER_INFO structures */
	prninfo = (PRINTER_INFO_2*)GlobalAlloc(GPTR, needed);
	if (!prninfo)
	{
		free(defaultPrinter);
		return NULL;
	}

	/* call again */
	if (!EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, (LPBYTE)prninfo,
	                  needed, &needed, &returned))
	{
	}

	printers = (rdpPrinter**)calloc((returned + 1), sizeof(rdpPrinter*));
	if (!printers)
	{
		GlobalFree(prninfo);
		free(defaultPrinter);
		return NULL;
	}

	num_printers = 0;

	for (i = 0; i < (int)returned; i++)
	{
		rdpPrinter* current = printers[num_printers];
		current = printer_win_new_printer((rdpWinPrinterDriver*)driver, prninfo[i].pPrinterName,
		                                  prninfo[i].pDriverName,
		                                  _wcscmp(prninfo[i].pPrinterName, defaultPrinter) == 0);
		if (!current)
		{
			printer_win_release_enum_printers(printers);
			printers = NULL;
			break;
		}
		if (current->is_default)
			haveDefault = TRUE;
		printers[num_printers++] = current;
	}

	if (!haveDefault && (returned > 0))
		printers[0]->is_default = TRUE;

	GlobalFree(prninfo);
	free(defaultPrinter);
	return printers;
}

static rdpPrinter* printer_win_get_printer(rdpPrinterDriver* driver, const char* name,
                                           const char* driverName, BOOL isDefault)
{
	WCHAR* driverNameW = NULL;
	WCHAR* nameW = NULL;
	rdpWinPrinterDriver* win_driver = (rdpWinPrinterDriver*)driver;
	rdpPrinter* myPrinter = NULL;

	if (name)
	{
		nameW = ConvertUtf8ToWCharAlloc(name, NULL);
		if (!nameW)
			return NULL;
	}
	if (driverName)
	{
		driverNameW = ConvertUtf8ToWCharAlloc(driverName, NULL);
		if (!driverNameW)
			return NULL;
	}

	myPrinter = printer_win_new_printer(win_driver, nameW, driverNameW, isDefault);
	free(driverNameW);
	free(nameW);

	return myPrinter;
}

static void printer_win_add_ref_driver(rdpPrinterDriver* driver)
{
	rdpWinPrinterDriver* win = (rdpWinPrinterDriver*)driver;
	if (win)
		win->references++;
}

/* Singleton */
static rdpWinPrinterDriver* win_driver = NULL;

static void printer_win_release_ref_driver(rdpPrinterDriver* driver)
{
	rdpWinPrinterDriver* win = (rdpWinPrinterDriver*)driver;
	if (win->references <= 1)
	{
		free(win);
		win_driver = NULL;
	}
	else
		win->references--;
}

rdpPrinterDriver* win_freerdp_printer_client_subsystem_entry(void)
{
	if (!win_driver)
	{
		win_driver = (rdpWinPrinterDriver*)calloc(1, sizeof(rdpWinPrinterDriver));

		if (!win_driver)
			return NULL;

		win_driver->driver.EnumPrinters = printer_win_enum_printers;
		win_driver->driver.ReleaseEnumPrinters = printer_win_release_enum_printers;
		win_driver->driver.GetPrinter = printer_win_get_printer;

		win_driver->driver.AddRef = printer_win_add_ref_driver;
		win_driver->driver.ReleaseRef = printer_win_release_ref_driver;

		win_driver->id_sequence = 1;
	}

	win_driver->driver.AddRef(&win_driver->driver);

	return &win_driver->driver;
}
