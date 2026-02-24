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
#include <winpr/wtsapi.h>
#include <winpr/string.h>
#include <winpr/windows.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winspool.h>

#include <freerdp/client/printer.h>
#include <freerdp/utils/helpers.h>

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

WINPR_ATTR_MALLOC(free, 1)
static WCHAR* printer_win_get_printjob_name(size_t id)
{
	struct tm tres = WINPR_C_ARRAY_INIT;
	WCHAR* str = nullptr;
	size_t len = 0;

	const time_t tt = time(nullptr);
	const errno_t err = localtime_s(&tres, &tt);

	do
	{
		if (len > 0)
		{
			str = calloc(len + 1, sizeof(WCHAR));
			if (!str)
				return nullptr;
		}

		const int rc = swprintf_s(
		    str, len,
		    WIDEN("%s Print %04d-%02d-%02d %02d-%02d-%02d - Job %") WIDEN(PRIuz) WIDEN("\0"),
		    freerdp_getApplicationDetailsStringW(), tres.tm_year + 1900, tres.tm_mon + 1,
		    tres.tm_mday, tres.tm_hour, tres.tm_min, tres.tm_sec, id);
		if (rc <= 0)
		{
			free(str);
			return nullptr;
		}
		len = WINPR_ASSERTING_INT_CAST(size_t, rc) + 1ull;
	} while (!str);

	return str;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_win_write_printjob(rdpPrintJob* printjob, const BYTE* data, size_t size)
{
	LPCVOID pBuf = data;
	DWORD pcWritten = 0;

	if (size > UINT32_MAX)
		return ERROR_BAD_ARGUMENTS;

	if (!printjob || !data)
		return ERROR_BAD_ARGUMENTS;

	rdpWinPrinter* printer = (rdpWinPrinter*)printjob->printer;
	if (!printer)
		return ERROR_BAD_ARGUMENTS;

	DWORD cbBuf = WINPR_ASSERTING_INT_CAST(uint32_t, size);
	if (!WritePrinter(printer->hPrinter, WINPR_CAST_CONST_PTR_AWAY(pBuf, void*), cbBuf, &pcWritten))
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

	if (!EndDocPrinter(win_printer->hPrinter))
	{
	}

	win_printer->printjob = nullptr;

	free(win_printjob->di.pDocName);
	free(win_printjob);
}

static rdpPrintJob* printer_win_create_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*)printer;
	rdpWinPrintJob* win_printjob;

	if (win_printer->printjob != nullptr)
		return nullptr;

	win_printjob = (rdpWinPrintJob*)calloc(1, sizeof(rdpWinPrintJob));
	if (!win_printjob)
		return nullptr;

	win_printjob->printjob.id = id;
	win_printjob->printjob.printer = printer;
	win_printjob->di.pDocName = printer_win_get_printjob_name(id);
	win_printjob->di.pDatatype = nullptr;
	win_printjob->di.pOutputFile = nullptr;

	win_printjob->handle = StartDocPrinter(win_printer->hPrinter, 1, (LPBYTE) & (win_printjob->di));

	if (!win_printjob->handle)
	{
		free(win_printjob->di.pDocName);
		free(win_printjob);
		return nullptr;
	}

	if (!StartPagePrinter(win_printer->hPrinter))
	{
		free(win_printjob->di.pDocName);
		free(win_printjob);
		return nullptr;
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
		return nullptr;

	if (win_printer->printjob->printjob.id != id)
		return nullptr;

	return (rdpPrintJob*)win_printer->printjob;
}

static void printer_win_free_printer(rdpPrinter* printer)
{
	rdpWinPrinter* win_printer = (rdpWinPrinter*)printer;

	if (win_printer->printjob)
		win_printer->printjob->printjob.Close((rdpPrintJob*)win_printer->printjob);

	if (win_printer->hPrinter)
		ClosePrinter(win_printer->hPrinter);

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
	PRINTER_INFO_2* prninfo = nullptr;

	if (!name)
		return nullptr;

	win_printer = (rdpWinPrinter*)calloc(1, sizeof(rdpWinPrinter));
	if (!win_printer)
		return nullptr;

	win_printer->printer.backend = &win_driver->driver;
	win_printer->printer.id = win_driver->id_sequence++;
	win_printer->printer.name = ConvertWCharToUtf8Alloc(name, nullptr);
	if (!win_printer->printer.name)
		goto fail;

	if (!win_printer->printer.name)
		goto fail;
	win_printer->printer.is_default = is_default;

	win_printer->printer.CreatePrintJob = printer_win_create_printjob;
	win_printer->printer.FindPrintJob = printer_win_find_printjob;
	win_printer->printer.AddRef = printer_win_add_ref_printer;
	win_printer->printer.ReleaseRef = printer_win_release_ref_printer;

	if (!OpenPrinter(WINPR_CAST_CONST_PTR_AWAY(name, WCHAR*), &(win_printer->hPrinter), nullptr))
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
		win_printer->printer.driver = ConvertWCharToUtf8Alloc(drivername, nullptr);
	else
		win_printer->printer.driver = ConvertWCharToUtf8Alloc(prninfo->pDriverName, nullptr);
	GlobalFree(prninfo);
	if (!win_printer->printer.driver)
		goto fail;

	win_printer->printer.AddRef(&win_printer->printer);
	win_printer->printer.backend->AddRef(win_printer->printer.backend);
	return &win_printer->printer;

fail:
	printer_win_free_printer(&win_printer->printer);
	return nullptr;
}

static void printer_win_release_enum_printers(rdpPrinter** printers)
{
	rdpPrinter** cur = printers;

	while ((cur != nullptr) && ((*cur) != nullptr))
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
	PRINTER_INFO_2* prninfo = nullptr;
	DWORD needed, returned;
	BOOL haveDefault = FALSE;
	LPWSTR defaultPrinter = nullptr;

	GetDefaultPrinter(nullptr, &needed);
	if (needed)
	{
		defaultPrinter = (LPWSTR)calloc(needed, sizeof(WCHAR));

		if (!defaultPrinter)
			return nullptr;

		if (!GetDefaultPrinter(defaultPrinter, &needed))
			defaultPrinter[0] = '\0';
	}

	/* find required size for the buffer */
	EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 2, nullptr, 0, &needed,
	             &returned);

	/* allocate array of PRINTER_INFO structures */
	prninfo = (PRINTER_INFO_2*)GlobalAlloc(GPTR, needed);
	if (!prninfo)
	{
		free(defaultPrinter);
		return nullptr;
	}

	/* call again */
	if (!EnumPrinters(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, nullptr, 2, (LPBYTE)prninfo,
	                  needed, &needed, &returned))
	{
	}

	printers = (rdpPrinter**)calloc((returned + 1), sizeof(rdpPrinter*));
	if (!printers)
	{
		GlobalFree(prninfo);
		free(defaultPrinter);
		return nullptr;
	}

	num_printers = 0;

	for (int i = 0; i < (int)returned; i++)
	{
		rdpPrinter* current = printers[num_printers];
		current = printer_win_new_printer((rdpWinPrinterDriver*)driver, prninfo[i].pPrinterName,
		                                  prninfo[i].pDriverName,
		                                  _wcscmp(prninfo[i].pPrinterName, defaultPrinter) == 0);
		if (!current)
		{
			printer_win_release_enum_printers(printers);
			printers = nullptr;
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
	WCHAR* driverNameW = nullptr;
	WCHAR* nameW = nullptr;
	rdpWinPrinterDriver* win_driver = (rdpWinPrinterDriver*)driver;
	rdpPrinter* myPrinter = nullptr;

	if (name)
	{
		nameW = ConvertUtf8ToWCharAlloc(name, nullptr);
		if (!nameW)
			return nullptr;
	}
	if (driverName)
	{
		driverNameW = ConvertUtf8ToWCharAlloc(driverName, nullptr);
		if (!driverNameW)
			return nullptr;
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
static rdpWinPrinterDriver* win_driver = nullptr;

static void printer_win_release_ref_driver(rdpPrinterDriver* driver)
{
	rdpWinPrinterDriver* win = (rdpWinPrinterDriver*)driver;
	if (win->references <= 1)
	{
		free(win);
		win_driver = nullptr;
	}
	else
		win->references--;
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE win_freerdp_printer_client_subsystem_entry(void* arg))
{
	rdpPrinterDriver** ppPrinter = (rdpPrinterDriver**)arg;
	if (!ppPrinter)
		return ERROR_INVALID_PARAMETER;

	if (!win_driver)
	{
		win_driver = (rdpWinPrinterDriver*)calloc(1, sizeof(rdpWinPrinterDriver));

		if (!win_driver)
			return ERROR_OUTOFMEMORY;

		win_driver->driver.EnumPrinters = printer_win_enum_printers;
		win_driver->driver.ReleaseEnumPrinters = printer_win_release_enum_printers;
		win_driver->driver.GetPrinter = printer_win_get_printer;

		win_driver->driver.AddRef = printer_win_add_ref_driver;
		win_driver->driver.ReleaseRef = printer_win_release_ref_driver;

		win_driver->id_sequence = 1;
	}

	win_driver->driver.AddRef(&win_driver->driver);

	*ppPrinter = &win_driver->driver;
	return CHANNEL_RC_OK;
}
