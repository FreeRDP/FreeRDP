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

#include <winpr/assert.h>

#include <freerdp/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <time.h>
#include <cups/cups.h>

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/string.h>

#include <freerdp/channels/rdpdr.h>

#include <freerdp/client/printer.h>

typedef struct
{
	rdpPrinterDriver driver;

	int id_sequence;
	size_t references;
} rdpCupsPrinterDriver;

typedef struct
{
	rdpPrintJob printjob;

	http_t* printjob_object;
	int printjob_id;
} rdpCupsPrintJob;

typedef struct
{
	rdpPrinter printer;

	rdpCupsPrintJob* printjob;
} rdpCupsPrinter;

static void printer_cups_get_printjob_name(char* buf, size_t size, size_t id)
{
	struct tm tres;
	const time_t tt = time(NULL);
	const struct tm* t = localtime_r(&tt, &tres);

	WINPR_ASSERT(buf);
	WINPR_ASSERT(size > 0);

	sprintf_s(buf, size - 1, "FreeRDP Print %04d-%02d-%02d %02d-%02d-%02d - Job %" PRIdz,
	          t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, id);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT printer_cups_write_printjob(rdpPrintJob* printjob, const BYTE* data, size_t size)
{
	rdpCupsPrintJob* cups_printjob = (rdpCupsPrintJob*)printjob;

	WINPR_ASSERT(cups_printjob);

#ifndef _CUPS_API_1_4

	{
		FILE* fp;

		fp = winpr_fopen((const char*)cups_printjob->printjob_object, "a+b");

		if (!fp)
			return ERROR_INTERNAL_ERROR;

		if (fwrite(data, 1, size, fp) < size)
		{
			fclose(fp);
			return ERROR_INTERNAL_ERROR;
			// FIXME once this function doesn't return void anymore!
		}

		fclose(fp);
	}

#else

	cupsWriteRequestData(cups_printjob->printjob_object, (const char*)data, size);

#endif

	return CHANNEL_RC_OK;
}

static void printer_cups_close_printjob(rdpPrintJob* printjob)
{
	rdpCupsPrintJob* cups_printjob = (rdpCupsPrintJob*)printjob;
	rdpCupsPrinter* cups_printer;

	WINPR_ASSERT(cups_printjob);

#ifndef _CUPS_API_1_4

	{
		char buf[100];

		printer_cups_get_printjob_name(buf, sizeof(buf), printjob->id);

		if (cupsPrintFile(printjob->printer->name, (const char*)cups_printjob->printjob_object, buf,
		                  0, NULL) == 0)
		{
		}

		unlink(cups_printjob->printjob_object);
		free(cups_printjob->printjob_object);
	}

#else

	cupsFinishDocument(cups_printjob->printjob_object, printjob->printer->name);
	cups_printjob->printjob_id = 0;
	httpClose(cups_printjob->printjob_object);

#endif

	cups_printer = (rdpCupsPrinter*)printjob->printer;
	WINPR_ASSERT(cups_printer);

	cups_printer->printjob = NULL;
	free(cups_printjob);
}

static rdpPrintJob* printer_cups_create_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpCupsPrinter* cups_printer = (rdpCupsPrinter*)printer;
	rdpCupsPrintJob* cups_printjob;

	WINPR_ASSERT(cups_printer);

	if (cups_printer->printjob != NULL)
		return NULL;

	cups_printjob = (rdpCupsPrintJob*)calloc(1, sizeof(rdpCupsPrintJob));
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

#if !defined(_CUPS_API_1_7)
		cups_printjob->printjob_object =
		    httpConnectEncrypt(cupsServer(), ippPort(), HTTP_ENCRYPT_IF_REQUESTED);
#else
		cups_printjob->printjob_object = httpConnect2(cupsServer(), ippPort(), NULL, AF_UNSPEC,
		                                              HTTP_ENCRYPT_IF_REQUESTED, 1, 10000, NULL);
#endif
		if (!cups_printjob->printjob_object)
		{
			free(cups_printjob);
			return NULL;
		}

		printer_cups_get_printjob_name(buf, sizeof(buf), cups_printjob->printjob.id);

		cups_printjob->printjob_id =
		    cupsCreateJob(cups_printjob->printjob_object, printer->name, buf, 0, NULL);

		if (!cups_printjob->printjob_id)
		{
			httpClose(cups_printjob->printjob_object);
			free(cups_printjob);
			return NULL;
		}

		cupsStartDocument(cups_printjob->printjob_object, printer->name, cups_printjob->printjob_id,
		                  buf, CUPS_FORMAT_AUTO, 1);
	}

#endif

	cups_printer->printjob = cups_printjob;

	return &cups_printjob->printjob;
}

static rdpPrintJob* printer_cups_find_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpCupsPrinter* cups_printer = (rdpCupsPrinter*)printer;

	WINPR_ASSERT(cups_printer);

	if (cups_printer->printjob == NULL)
		return NULL;
	if (cups_printer->printjob->printjob.id != id)
		return NULL;

	return &cups_printer->printjob->printjob;
}

static void printer_cups_free_printer(rdpPrinter* printer)
{
	rdpCupsPrinter* cups_printer = (rdpCupsPrinter*)printer;

	WINPR_ASSERT(cups_printer);

	if (cups_printer->printjob)
	{
		WINPR_ASSERT(cups_printer->printjob->printjob.Close);
		cups_printer->printjob->printjob.Close(&cups_printer->printjob->printjob);
	}

	if (printer->backend)
	{
		WINPR_ASSERT(printer->backend->ReleaseRef);
		printer->backend->ReleaseRef(printer->backend);
	}
	free(printer->name);
	free(printer->driver);
	free(printer);
}

static void printer_cups_add_ref_printer(rdpPrinter* printer)
{
	if (printer)
		printer->references++;
}

static void printer_cups_release_ref_printer(rdpPrinter* printer)
{
	if (!printer)
		return;
	if (printer->references <= 1)
		printer_cups_free_printer(printer);
	else
		printer->references--;
}

static rdpPrinter* printer_cups_new_printer(rdpCupsPrinterDriver* cups_driver, const char* name,
                                            const char* driverName, BOOL is_default)
{
	rdpCupsPrinter* cups_printer;

	cups_printer = (rdpCupsPrinter*)calloc(1, sizeof(rdpCupsPrinter));
	if (!cups_printer)
		return NULL;

	cups_printer->printer.backend = &cups_driver->driver;

	cups_printer->printer.id = cups_driver->id_sequence++;
	cups_printer->printer.name = _strdup(name);
	if (!cups_printer->printer.name)
		goto fail;

	if (driverName)
		cups_printer->printer.driver = _strdup(driverName);
	else
		cups_printer->printer.driver = _strdup("MS Publisher Imagesetter");
	if (!cups_printer->printer.driver)
		goto fail;

	cups_printer->printer.is_default = is_default;

	cups_printer->printer.CreatePrintJob = printer_cups_create_printjob;
	cups_printer->printer.FindPrintJob = printer_cups_find_printjob;
	cups_printer->printer.AddRef = printer_cups_add_ref_printer;
	cups_printer->printer.ReleaseRef = printer_cups_release_ref_printer;

	WINPR_ASSERT(cups_printer->printer.AddRef);
	cups_printer->printer.AddRef(&cups_printer->printer);

	WINPR_ASSERT(cups_printer->printer.backend->AddRef);
	cups_printer->printer.backend->AddRef(cups_printer->printer.backend);

	return &cups_printer->printer;

fail:
	printer_cups_free_printer(&cups_printer->printer);
	return NULL;
}

static void printer_cups_release_enum_printers(rdpPrinter** printers)
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

static rdpPrinter** printer_cups_enum_printers(rdpPrinterDriver* driver)
{
	rdpPrinter** printers;
	int num_printers = 0;
	cups_dest_t* dests = NULL;
	cups_dest_t* dest = NULL;
	int i;
	BOOL haveDefault = FALSE;
	const int num_dests = cupsGetDests(&dests);

	WINPR_ASSERT(driver);

	printers = (rdpPrinter**)calloc(num_dests + 1, sizeof(rdpPrinter*));
	if (!printers)
		return NULL;

	for (i = 0, dest = dests; i < num_dests; i++, dest++)
	{
		if (dest->instance == NULL)
		{
			rdpPrinter* current = printer_cups_new_printer((rdpCupsPrinterDriver*)driver,
			                                               dest->name, NULL, dest->is_default);
			if (!current)
			{
				printer_cups_release_enum_printers(printers);
				printers = NULL;
				break;
			}

			if (current->is_default)
				haveDefault = TRUE;

			printers[num_printers++] = current;
		}
	}
	cupsFreeDests(num_dests, dests);

	if (!haveDefault && (num_dests > 0) && printers)
	{
		if (printers[0])
			printers[0]->is_default = TRUE;
	}

	return printers;
}

static rdpPrinter* printer_cups_get_printer(rdpPrinterDriver* driver, const char* name,
                                            const char* driverName, BOOL isDefault)
{
	rdpCupsPrinterDriver* cups_driver = (rdpCupsPrinterDriver*)driver;

	WINPR_ASSERT(cups_driver);
	return printer_cups_new_printer(cups_driver, name, driverName, isDefault);
}

static void printer_cups_add_ref_driver(rdpPrinterDriver* driver)
{
	rdpCupsPrinterDriver* cups_driver = (rdpCupsPrinterDriver*)driver;
	if (cups_driver)
		cups_driver->references++;
}

/* Singleton */
static rdpCupsPrinterDriver* uniq_cups_driver = NULL;

static void printer_cups_release_ref_driver(rdpPrinterDriver* driver)
{
	rdpCupsPrinterDriver* cups_driver = (rdpCupsPrinterDriver*)driver;

	WINPR_ASSERT(cups_driver);

	if (cups_driver->references <= 1)
	{
		if (uniq_cups_driver == cups_driver)
			uniq_cups_driver = NULL;
		free(cups_driver);
	}
	else
		cups_driver->references--;
}

rdpPrinterDriver* cups_freerdp_printer_client_subsystem_entry(void)
{
	if (!uniq_cups_driver)
	{
		uniq_cups_driver = (rdpCupsPrinterDriver*)calloc(1, sizeof(rdpCupsPrinterDriver));

		if (!uniq_cups_driver)
			return NULL;

		uniq_cups_driver->driver.EnumPrinters = printer_cups_enum_printers;
		uniq_cups_driver->driver.ReleaseEnumPrinters = printer_cups_release_enum_printers;
		uniq_cups_driver->driver.GetPrinter = printer_cups_get_printer;

		uniq_cups_driver->driver.AddRef = printer_cups_add_ref_driver;
		uniq_cups_driver->driver.ReleaseRef = printer_cups_release_ref_driver;

		uniq_cups_driver->id_sequence = 1;
	}

	WINPR_ASSERT(uniq_cups_driver->driver.AddRef);
	uniq_cups_driver->driver.AddRef(&uniq_cups_driver->driver);

	return &uniq_cups_driver->driver;
}
