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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/windows.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winspool.h>

#include "printer_main.h"

#include "printer_win.h"

typedef struct rdp_win_printer_driver rdpWinPrinterDriver;
typedef struct rdp_win_printer rdpWinPrinter;
typedef struct rdp_win_print_job rdpWinPrintJob;

struct rdp_win_printer_driver
{
	rdpPrinterDriver driver;

	int id_sequence;
};

struct rdp_win_printer
{
	rdpPrinter printer;
	HANDLE hPrinter;
	rdpWinPrintJob* printjob;
};

struct rdp_win_print_job
{
	rdpPrintJob printjob;
	DOC_INFO_1 di;
	DWORD handle;

	void* printjob_object;
	int printjob_id;
};

static void printer_win_get_printjob_name(char* buf, int size)
{
	time_t tt;
	struct tm* t;

	tt = time(NULL);
	t = localtime(&tt);
	sprintf_s(buf, size - 1, "FreeRDP Print Job %d%02d%02d%02d%02d%02d",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		t->tm_hour, t->tm_min, t->tm_sec);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_win_write_printjob(rdpPrintJob* printjob, BYTE* data, int size)
{
	rdpWinPrintJob* win_printjob = (rdpWinPrintJob*) printjob;

	LPVOID pBuf = data;
	DWORD cbBuf = size;
	DWORD pcWritten;

	if(!WritePrinter(((rdpWinPrinter*)printjob->printer)->hPrinter, pBuf, cbBuf, &pcWritten))
		return ERROR_INTERNAL_ERROR;
	return CHANNEL_RC_OK;
}

static void printer_win_close_printjob(rdpPrintJob* printjob)
{
	rdpWinPrintJob* win_printjob = (rdpWinPrintJob*) printjob;

	if (!EndPagePrinter(((rdpWinPrinter*) printjob->printer)->hPrinter))
	{

	}

	if (!ClosePrinter(((rdpWinPrinter*) printjob->printer)->hPrinter))
	{

	}

	((rdpWinPrinter*) printjob->printer)->printjob = NULL;

	free(win_printjob);
}

static rdpPrintJob* printer_win_create_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*)printer;
	rdpWinPrintJob* win_printjob;

	if (win_printer->printjob != NULL)
		return NULL;

	win_printjob = (rdpWinPrintJob*) calloc(1, sizeof(rdpWinPrintJob));
	if (!win_printjob)
		return NULL;

	win_printjob->printjob.id = id;
	win_printjob->printjob.printer = printer;
	win_printjob->di.pDocName = L"FREERDPjob";
	win_printjob->di.pDatatype= NULL;
	win_printjob->di.pOutputFile = NULL;

	win_printjob->handle = StartDocPrinter(win_printer->hPrinter, 1, (LPBYTE) &(win_printjob->di));

	if (!win_printjob->handle)
	{
		free(win_printjob);
		return NULL;
	}

	if (!StartPagePrinter(win_printer->hPrinter))
	{
		free(win_printjob);
		return NULL;
	}

	win_printjob->printjob.Write = printer_win_write_printjob;
	win_printjob->printjob.Close = printer_win_close_printjob;

	win_printer->printjob = win_printjob;
	
	return (rdpPrintJob*) win_printjob;
}

static rdpPrintJob* printer_win_find_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*) printer;

	if (!win_printer->printjob)
		return NULL;

	if (win_printer->printjob->printjob.id != id)
		return NULL;

	return (rdpPrintJob*) win_printer->printjob;
}

static void printer_win_free_printer(rdpPrinter* printer)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*) printer;

	if (win_printer->printjob)
		win_printer->printjob->printjob.Close((rdpPrintJob*) win_printer->printjob);

	free(printer->name);
	free(printer->driver);
	free(printer);
}

static rdpPrinter* printer_win_new_printer(rdpWinPrinterDriver* win_driver,
	const char* name, const WCHAR* drivername, BOOL is_default)
{
	rdpWinPrinter* win_printer;
	wchar_t wname[256];
	DWORD needed;
	PRINTER_INFO_2 *prninfo=NULL;
	size_t charsConverted;

	win_printer = (rdpWinPrinter*) calloc(1, sizeof(rdpWinPrinter));
	if (!win_printer)
		return NULL;

	win_printer->printer.id = win_driver->id_sequence++;
	win_printer->printer.name = _strdup(name);
	if (!win_printer->printer.name)
	{
		free(win_printer);
		return NULL;
	}
	win_printer->printer.is_default = is_default;

	win_printer->printer.CreatePrintJob = printer_win_create_printjob;
	win_printer->printer.FindPrintJob = printer_win_find_printjob;
	win_printer->printer.Free = printer_win_free_printer;

	swprintf(wname, 256, L"%hs", name);
	OpenPrinter(wname, &(win_printer->hPrinter), NULL);

	GetPrinter(win_printer->hPrinter, 2, (LPBYTE) prninfo, 0, &needed);
	prninfo = (PRINTER_INFO_2*) GlobalAlloc(GPTR,needed);
	if (!prninfo)
	{
		free(win_printer->printer.name);
		free(win_printer);
		return NULL;
	}
	GetPrinter(win_printer->hPrinter, 2, (LPBYTE) prninfo, needed, &needed);

	if (drivername)
		win_printer->printer.driver = _wcsdup(drivername);
	else
		win_printer->printer.driver = _wcsdup(prninfo->pDriverName);
	if (!win_printer->printer.driver)
	{
		GlobalFree(prninfo);
		free(win_printer->printer.name);
		free(win_printer);
		return NULL;
	}

	return (rdpPrinter*)win_printer;
}

static rdpPrinter** printer_win_enum_printers(rdpPrinterDriver* driver)
{
	rdpPrinter** printers;
	int num_printers;
	int i;
	char pname[1000];
	size_t charsConverted;
	PRINTER_INFO_2* prninfo = NULL;
	DWORD needed, returned;

	/* find required size for the buffer */
	EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed, &returned);


	/* allocate array of PRINTER_INFO structures */
	prninfo = (PRINTER_INFO_2*) GlobalAlloc(GPTR,needed);
	if (!prninfo)
		return NULL;
 
	/* call again */
	if (!EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2, (LPBYTE) prninfo, needed, &needed, &returned))
	{

	}

	printers = (rdpPrinter**) calloc((returned + 1), sizeof(rdpPrinter*));
	if (!printers)
	{
		GlobalFree(prninfo);
		return NULL;
	}

	num_printers = 0;

	for (i = 0; i < (int) returned; i++)
	{
		wcstombs_s(&charsConverted, pname, 1000, prninfo[i].pPrinterName, _TRUNCATE);
		printers[num_printers++] = printer_win_new_printer((rdpWinPrinterDriver*)driver,
			pname, prninfo[i].pDriverName, 0);
	}

	GlobalFree(prninfo);
	return printers;
}

static rdpPrinter* printer_win_get_printer(rdpPrinterDriver* driver,
	const char* name, const char* driverName)
{
	WCHAR* driverNameW = NULL;
	rdpWinPrinterDriver* win_driver = (rdpWinPrinterDriver*)driver;
	rdpPrinter *myPrinter = NULL;
	
	if (driverName)
	{
		ConvertToUnicode(CP_UTF8, 0, driverName, -1, &driverNameW, 0);
		if (!driverNameW)
			return NULL;
	}

	myPrinter = printer_win_new_printer(win_driver, name, driverNameW,
	win_driver->id_sequence == 1 ? TRUE : FALSE);
	free(driverNameW);

	return myPrinter;
}

static rdpWinPrinterDriver* win_driver = NULL;

rdpPrinterDriver* printer_win_get_driver(void)
{
	if (!win_driver)
	{
		win_driver = (rdpWinPrinterDriver*) calloc(1, sizeof(rdpWinPrinterDriver));
		if (!win_driver)
			return NULL;

		win_driver->driver.EnumPrinters = printer_win_enum_printers;
		win_driver->driver.GetPrinter = printer_win_get_printer;

		win_driver->id_sequence = 1;
	}

	return (rdpPrinterDriver*) win_driver;
}

