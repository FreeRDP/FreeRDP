/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Print Virtual Channel - CUPS driver
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
#include <pthread.h>

#include <time.h>
#include <cups/cups.h>

#include <winpr/crt.h>
#include <winpr/string.h>

#include <freerdp/channels/rdpdr.h>

#include "printer_main.h"

#include "printer_cups.h"

typedef struct rdp_cups_printer_driver rdpCupsPrinterDriver;
typedef struct rdp_cups_printer rdpCupsPrinter;
typedef struct rdp_cups_print_job rdpCupsPrintJob;

struct rdp_cups_printer_driver
{
	rdpPrinterDriver driver;

	int id_sequence;
};

struct rdp_cups_printer
{
	rdpPrinter printer;

	rdpCupsPrintJob* printjob;
};

struct rdp_cups_print_job
{
	rdpPrintJob printjob;

	void* printjob_object;
	int printjob_id;
};

static void printer_cups_get_printjob_name(char* buf, int size)
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
static UINT printer_cups_write_printjob(rdpPrintJob* printjob, BYTE* data, int size)
{
	rdpCupsPrintJob* cups_printjob = (rdpCupsPrintJob*) printjob;

#ifndef _CUPS_API_1_4

	{
		FILE* fp;

		fp = fopen((const char*) cups_printjob->printjob_object, "a+b");

		if (!fp)
			return ERROR_INTERNAL_ERROR;

		if (fwrite(data, 1, size, fp) < size)
		{
			return ERROR_INTERNAL_ERROR;
			// FIXME once this function doesn't return void anymore!
		}

		fclose(fp);
	}

#else

	cupsWriteRequestData((http_t*) cups_printjob->printjob_object, (const char*) data, size);

#endif

	return CHANNEL_RC_OK;
}

static void printer_cups_close_printjob(rdpPrintJob* printjob)
{
	rdpCupsPrintJob* cups_printjob = (rdpCupsPrintJob*) printjob;

#ifndef _CUPS_API_1_4

	{
		char buf[100];

		printer_cups_get_printjob_name(buf, sizeof(buf));

		if (cupsPrintFile(printjob->printer->name, (const char*) cups_printjob->printjob_object, buf, 0, NULL) == 0)
		{

		}

		unlink(cups_printjob->printjob_object);
		free(cups_printjob->printjob_object);
	}

#else

	cupsFinishDocument((http_t*) cups_printjob->printjob_object, printjob->printer->name);
	cups_printjob->printjob_id = 0;
	httpClose((http_t*) cups_printjob->printjob_object);

#endif

	((rdpCupsPrinter*) printjob->printer)->printjob = NULL;
	free(cups_printjob) ;
}

static rdpPrintJob* printer_cups_create_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpCupsPrinter* cups_printer = (rdpCupsPrinter*) printer;
	rdpCupsPrintJob* cups_printjob;

	if (cups_printer->printjob != NULL)
		return NULL;

	cups_printjob = (rdpCupsPrintJob*) calloc(1, sizeof(rdpCupsPrintJob));
	if (!cups_printjob)
		return NULL;

	cups_printjob->printjob.id = id;
	cups_printjob->printjob.printer = printer;

	cups_printjob->printjob.Write = printer_cups_write_printjob;
	cups_printjob->printjob.Close = printer_cups_close_printjob;

#ifndef _CUPS_API_1_4

	cups_printjob->printjob_object = _strdup(tmpnam(NULL));
	if (!cups_printjob->printjob_object)
	{
		free(cups_printjob);
		return NULL;
	}

#else
	{
		char buf[100];

		cups_printjob->printjob_object = httpConnectEncrypt(cupsServer(), ippPort(), HTTP_ENCRYPT_IF_REQUESTED);

		if (!cups_printjob->printjob_object)
		{
			free(cups_printjob);
			return NULL;
		}

		printer_cups_get_printjob_name(buf, sizeof(buf));

		cups_printjob->printjob_id = cupsCreateJob((http_t*) cups_printjob->printjob_object,
			printer->name, buf, 0, NULL);

		if (!cups_printjob->printjob_id)
		{
			httpClose((http_t*) cups_printjob->printjob_object);
			free(cups_printjob);
			return NULL;
		}

		cupsStartDocument((http_t*) cups_printjob->printjob_object,
			printer->name, cups_printjob->printjob_id, buf, CUPS_FORMAT_AUTO, 1);
	}

#endif

	cups_printer->printjob = cups_printjob;
	
	return (rdpPrintJob*)cups_printjob;
}

static rdpPrintJob* printer_cups_find_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpCupsPrinter* cups_printer = (rdpCupsPrinter*)printer;

	if (cups_printer->printjob == NULL)
		return NULL;
	if (cups_printer->printjob->printjob.id != id)
		return NULL;

	return (rdpPrintJob*)cups_printer->printjob;
}

static void printer_cups_free_printer(rdpPrinter* printer)
{
	rdpCupsPrinter* cups_printer = (rdpCupsPrinter*) printer;

	if (cups_printer->printjob)
		cups_printer->printjob->printjob.Close((rdpPrintJob*) cups_printer->printjob);

	free(printer->name);
	free(printer->driver);
	free(printer);
}

static rdpPrinter* printer_cups_new_printer(rdpCupsPrinterDriver* cups_driver,
	const char* name, const char* driverName, BOOL is_default)
{
	rdpCupsPrinter* cups_printer;

	cups_printer = (rdpCupsPrinter*) calloc(1, sizeof(rdpCupsPrinter));
	if (!cups_printer)
		return NULL;

	cups_printer->printer.id = cups_driver->id_sequence++;
	cups_printer->printer.name = _strdup(name);
	if (!cups_printer->printer.name)
	{
		free(cups_printer);
		return NULL;
	}

	if (driverName)
		cups_printer->printer.driver = _strdup(driverName);
	else
		cups_printer->printer.driver = _strdup("MS Publisher Imagesetter");
	if (!cups_printer->printer.driver)
	{
		free(cups_printer->printer.name);
		free(cups_printer);
		return NULL;
	}
	cups_printer->printer.is_default = is_default;

	cups_printer->printer.CreatePrintJob = printer_cups_create_printjob;
	cups_printer->printer.FindPrintJob = printer_cups_find_printjob;
	cups_printer->printer.Free = printer_cups_free_printer;

	return (rdpPrinter*) cups_printer;
}

static rdpPrinter** printer_cups_enum_printers(rdpPrinterDriver* driver)
{
	rdpPrinter** printers;
	int num_printers;
	cups_dest_t *dests;
	cups_dest_t *dest;
	int num_dests;
	int i;

	num_dests = cupsGetDests(&dests);
	printers = (rdpPrinter**) calloc(1, sizeof(rdpPrinter*) * (num_dests + 1));
	if (!printers)
		return NULL;

	num_printers = 0;

	for (i = 0, dest = dests; i < num_dests; i++, dest++)
	{
		if (dest->instance == NULL)
		{
			printers[num_printers++] = printer_cups_new_printer((rdpCupsPrinterDriver*) driver,
				dest->name, NULL, dest->is_default);
		}
	}
	cupsFreeDests(num_dests, dests);

	return printers;
}

static rdpPrinter* printer_cups_get_printer(rdpPrinterDriver* driver,
        const char* name, const char* driverName)
{
	rdpCupsPrinterDriver* cups_driver = (rdpCupsPrinterDriver*) driver;

	return printer_cups_new_printer(cups_driver, name, driverName,
            cups_driver->id_sequence == 1 ? TRUE : FALSE);
}

static rdpCupsPrinterDriver* cups_driver = NULL;

rdpPrinterDriver* printer_cups_get_driver(void)
{
	if (cups_driver == NULL)
	{
		cups_driver = (rdpCupsPrinterDriver*) calloc(1, sizeof(rdpCupsPrinterDriver));

		if (!cups_driver)
			return NULL;

		cups_driver->driver.EnumPrinters = printer_cups_enum_printers;
		cups_driver->driver.GetPrinter = printer_cups_get_printer;

		cups_driver->id_sequence = 1;
	}

	return (rdpPrinterDriver*) cups_driver;
}

