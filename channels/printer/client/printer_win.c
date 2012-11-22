/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Print Virtual Channel - WIN driver
 *
 * Copyright 2012 Gerald Richter
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
#include <winpr/windows.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winspool.h>

#include <freerdp/utils/svc_plugin.h>

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

	DEBUG_WINPR("");

	tt = time(NULL);
	t = localtime(&tt);
	snprintf(buf, size - 1, "FreeRDP Print Job %d%02d%02d%02d%02d%02d",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
		t->tm_hour, t->tm_min, t->tm_sec);

	DEBUG_WINPR("buf: %s", buf);

}

static void printer_win_write_printjob(rdpPrintJob* printjob, BYTE* data, int size)
{
	rdpWinPrintJob* win_printjob = (rdpWinPrintJob*)printjob;

	LPVOID pBuf = data;
	DWORD cbBuf = size;
	DWORD pcWritten;

	if( ! WritePrinter( ((rdpWinPrinter*)printjob->printer)->hPrinter, pBuf, cbBuf, &pcWritten ) ) 
			DEBUG_WINPR("WritePrinter failed");
;
 
}

static void printer_win_close_printjob(rdpPrintJob* printjob)
{
	rdpWinPrintJob* win_printjob = (rdpWinPrintJob*)printjob;

	DEBUG_WINPR("");

	if ( ! EndPagePrinter( ((rdpWinPrinter*)printjob->printer)->hPrinter ) )
		DEBUG_WINPR("EndPagePrinter failed");;
	if ( ! ClosePrinter( ((rdpWinPrinter*)printjob->printer)->hPrinter ) ) 
		DEBUG_WINPR("ClosePrinter failed");;

	((rdpWinPrinter*)printjob->printer)->printjob = NULL;
	free(win_printjob) ;
}

static rdpPrintJob* printer_win_create_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*)printer;
	rdpWinPrintJob* win_printjob;

	DEBUG_WINPR("");

	if (win_printer->printjob != NULL)
		return NULL;

	win_printjob = (rdpWinPrintJob*) malloc(sizeof(rdpWinPrintJob));
	ZeroMemory(win_printjob, sizeof(rdpWinPrintJob));

	win_printjob->printjob.id = id;
	win_printjob->printjob.printer = printer;
	win_printjob->di.pDocName = L"FREERDPjob";
	win_printjob->di.pDatatype= NULL;
	win_printjob->di.pOutputFile = NULL;

	win_printjob->handle = StartDocPrinter(win_printer->hPrinter, 1, (LPBYTE) &(win_printjob->di));

	if (!win_printjob->handle)
		DEBUG_WINPR("StartDocPrinter failed");

	if (!StartPagePrinter(win_printer->hPrinter))
		DEBUG_WINPR("ClosePrinter failed");

	win_printjob->printjob.Write = printer_win_write_printjob;
	win_printjob->printjob.Close = printer_win_close_printjob;

	win_printer->printjob = win_printjob;
	
	return (rdpPrintJob*) win_printjob;
}

static rdpPrintJob* printer_win_find_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*) printer;

	DEBUG_WINPR("");

	if (win_printer->printjob == NULL)
		return NULL;

	if (win_printer->printjob->printjob.id != id)
		return NULL;

	return (rdpPrintJob*) win_printer->printjob;
}

static void printer_win_free_printer(rdpPrinter* printer)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*) printer;

	DEBUG_WINPR("");

	if (win_printer->printjob)
		win_printer->printjob->printjob.Close((rdpPrintJob*) win_printer->printjob);

	free(printer->name);
	free(printer);
}

static rdpPrinter* printer_win_new_printer(rdpWinPrinterDriver* win_driver, const char* name, const wchar_t* drivername, BOOL is_default)
{
	rdpWinPrinter* win_printer;
	wchar_t wname[256];
	DWORD needed;
	PRINTER_INFO_2 *prninfo=NULL;
	size_t charsConverted;
	DEBUG_WINPR("");

	win_printer = (rdpWinPrinter*) malloc(sizeof(rdpWinPrinter));
	ZeroMemory(win_printer, sizeof(rdpWinPrinter));

	win_printer->printer.id = win_driver->id_sequence++;
	win_printer->printer.name = _strdup(name);
	win_printer->printer.is_default = is_default;

	win_printer->printer.CreatePrintJob = printer_win_create_printjob;
	win_printer->printer.FindPrintJob = printer_win_find_printjob;
	win_printer->printer.Free = printer_win_free_printer;

	swprintf(wname, 256, L"%hs", name);
	OpenPrinter(wname, &(win_printer->hPrinter), NULL);
	DEBUG_WINPR("handle: 0x%08X", win_printer->hPrinter);

	GetPrinter(win_printer->hPrinter, 2, (LPBYTE) prninfo, 0, &needed);
	prninfo = (PRINTER_INFO_2*) GlobalAlloc(GPTR,needed);
	GetPrinter(win_printer->hPrinter, 2, (LPBYTE) prninfo, needed, &needed);

	win_printer->printer.driver = malloc(1000);
	wcstombs_s(&charsConverted, win_printer->printer.driver, 1000, prninfo->pDriverName, _TRUNCATE);

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
 
	DEBUG_WINPR("");

	/* find required size for the buffer */
	EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed, &returned);


	/* allocate array of PRINTER_INFO structures */
	prninfo = (PRINTER_INFO_2*) GlobalAlloc(GPTR,needed);
 
	/* call again */
	if ( !EnumPrinters(PRINTER_ENUM_LOCAL|PRINTER_ENUM_CONNECTIONS, NULL, 2, (LPBYTE) prninfo, needed, &needed, &returned) )
	{
		DEBUG_WINPR("EnumPrinters failed");
	} ; /* ERROR... */

	DEBUG_WINPR("printers found: %d", returned);

	printers = (rdpPrinter**) malloc(sizeof(rdpPrinter*) * (returned + 1));
	ZeroMemory(printers, sizeof(rdpPrinter*) * (returned + 1));

	num_printers = 0;

	for (i = 0; i < (int)returned; i++)
	{
		wcstombs_s(&charsConverted, pname, 1000, prninfo[i].pPrinterName, _TRUNCATE);
		printers[num_printers++] = printer_win_new_printer((rdpWinPrinterDriver*)driver,
			pname, prninfo[i].pDriverName, 0);
	}

	GlobalFree(prninfo);
	return printers;
}

static rdpPrinter* printer_win_get_printer(rdpPrinterDriver* driver, const char* name)
{
	rdpWinPrinterDriver* win_driver = (rdpWinPrinterDriver*)driver;
	rdpPrinter *myPrinter = NULL;

	DEBUG_WINPR("printer %s", name);
	
	myPrinter = printer_win_new_printer(win_driver, name, L"", win_driver->id_sequence == 1 ? TRUE : FALSE);
	
	return myPrinter;
}

static rdpWinPrinterDriver* win_driver = NULL;

rdpPrinterDriver* printer_win_get_driver(void)
{
	DEBUG_WINPR("");

	if (win_driver == NULL)
	{
		win_driver = (rdpWinPrinterDriver*) malloc(sizeof(rdpWinPrinterDriver));
		ZeroMemory(win_driver, sizeof(rdpWinPrinterDriver));

		win_driver->driver.EnumPrinters = printer_win_enum_printers;
		win_driver->driver.GetPrinter = printer_win_get_printer;

		win_driver->id_sequence = 1;
	}

	return (rdpPrinterDriver*) win_driver;
}

