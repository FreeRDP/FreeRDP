/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Print Virtual Channel - Android backend
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <freerdp/client/printer.h>
#include <freerdp/channels/log.h>

#define TAG CHANNELS_TAG("printer.client.android")

#define PRINT_OUTPUT_DIR "/sdcard/Download/"
#define PRINT_OUTPUT_PREFIX "rdp_print_"
#define ANDROID_PRINTER_NAME "aFreeRDP Print"
#define ANDROID_PRINTER_DRIVER "Microsoft Print to PDF"

typedef struct
{
	rdpPrinterDriver driver;
	size_t id_sequence;
	size_t references;
} rdpAndroidPrinterDriver;

typedef struct
{
	rdpPrintJob printjob;
	FILE* file;
	char path[256];
} rdpAndroidPrintJob;

typedef struct
{
	rdpPrinter printer;
	rdpAndroidPrintJob* printjob;
} rdpAndroidPrinter;

static UINT printer_android_write_printjob(rdpPrintJob* printjob, const BYTE* data, size_t size)
{
	rdpAndroidPrintJob* aj = (rdpAndroidPrintJob*)printjob;
	if (aj->file)
	{
		if (fwrite(data, 1, size, aj->file) != size)
			WLog_WARN(TAG, "Short write on print job file");
	}
	return CHANNEL_RC_OK;
}

static void printer_android_close_printjob(rdpPrintJob* printjob)
{
	rdpAndroidPrintJob* aj = (rdpAndroidPrintJob*)printjob;
	rdpAndroidPrinter* ap = (rdpAndroidPrinter*)printjob->printer;

	if (aj->file)
	{
		fclose(aj->file);
		aj->file = nullptr;
		WLog_INFO(TAG, "Print job complete, saved to %s", aj->path);
	}

	ap->printjob = nullptr;
	free(aj);
}

static rdpPrintJob* printer_android_create_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpAndroidPrinter* ap = (rdpAndroidPrinter*)printer;

	WINPR_ASSERT(ap);

	if (ap->printjob != nullptr)
	{
		WLog_WARN(TAG, "printjob [printer '%s'] already existing, abort!", printer->name);
		return nullptr;
	}

	rdpAndroidPrintJob* aj = calloc(1, sizeof(*aj));
	if (!aj)
		return nullptr;

	aj->printjob.id = id;
	aj->printjob.printer = printer;
	aj->printjob.Write = printer_android_write_printjob;
	aj->printjob.Close = printer_android_close_printjob;

	time_t t = time(NULL);
	snprintf(aj->path, sizeof(aj->path), PRINT_OUTPUT_DIR PRINT_OUTPUT_PREFIX "%lld.pdf",
	         (long long)t);

	aj->file = fopen(aj->path, "wb");
	if (!aj->file)
	{
		WLog_ERR(TAG, "Failed to open dump file %s", aj->path);
		free(aj);
		return NULL;
	}

	ap->printjob = aj;
	return &aj->printjob;
}

static rdpPrintJob* printer_android_find_printjob(rdpPrinter* printer, UINT32 id)
{
	rdpAndroidPrinter* ap = (rdpAndroidPrinter*)printer;

	WINPR_ASSERT(ap);

	if (ap->printjob == NULL)
		return NULL;
	if (ap->printjob->printjob.id != id)
		return NULL;

	return &ap->printjob->printjob;
}

static void printer_android_free_printer(rdpPrinter* printer)
{
	rdpAndroidPrinter* ap = (rdpAndroidPrinter*)printer;

	WINPR_ASSERT(ap);

	if (ap->printjob)
	{
		WINPR_ASSERT(ap->printjob->printjob.Close);
		ap->printjob->printjob.Close(&ap->printjob->printjob);
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

static void printer_android_add_ref_printer(rdpPrinter* printer)
{
	if (printer)
		printer->references++;
}

static void printer_android_release_ref_printer(rdpPrinter* printer)
{
	if (!printer)
		return;
	if (printer->references <= 1)
		printer_android_free_printer(printer);
	else
		printer->references--;
}

static rdpPrinter* printer_android_new_printer(rdpAndroidPrinterDriver* drv, const char* name,
                                               const char* driverName, BOOL isDefault)
{
	rdpAndroidPrinter* ap = calloc(1, sizeof(*ap));
	if (!ap)
		return NULL;

	ap->printer.backend = &drv->driver;
	ap->printer.id = drv->id_sequence++;
	ap->printer.name = _strdup(name ? name : ANDROID_PRINTER_NAME);
	if (!ap->printer.name)
		goto fail;

	ap->printer.driver = _strdup(driverName ? driverName : ANDROID_PRINTER_DRIVER);
	if (!ap->printer.driver)
		goto fail;

	ap->printer.is_default = isDefault;
	ap->printer.CreatePrintJob = printer_android_create_printjob;
	ap->printer.FindPrintJob = printer_android_find_printjob;
	ap->printer.AddRef = printer_android_add_ref_printer;
	ap->printer.ReleaseRef = printer_android_release_ref_printer;

	WINPR_ASSERT(ap->printer.AddRef);
	ap->printer.AddRef(&ap->printer);

	WINPR_ASSERT(ap->printer.backend->AddRef);
	ap->printer.backend->AddRef(ap->printer.backend);

	return &ap->printer;

fail:
	printer_android_free_printer(&ap->printer);
	return NULL;
}

static rdpPrinter** printer_android_enum_printers(rdpPrinterDriver* driver)
{
	rdpAndroidPrinterDriver* ad = (rdpAndroidPrinterDriver*)driver;
	rdpPrinter** list = calloc(2, sizeof(rdpPrinter*));
	if (!list)
		return NULL;

	list[0] = printer_android_new_printer(ad, ANDROID_PRINTER_NAME, ANDROID_PRINTER_DRIVER, TRUE);
	list[1] = NULL;
	return list;
}

static void printer_android_release_enum_printers(rdpPrinter** printers)
{
	if (!printers)
		return;
	for (rdpPrinter** p = printers; *p; p++)
	{
		if ((*p)->ReleaseRef)
			(*p)->ReleaseRef(*p);
	}
	free(printers);
}

static rdpPrinter* printer_android_get_printer(rdpPrinterDriver* driver, const char* name,
                                               const char* driverName, BOOL isDefault)
{
	return printer_android_new_printer((rdpAndroidPrinterDriver*)driver, name, driverName,
	                                   isDefault);
}

static void printer_android_add_ref_driver(rdpPrinterDriver* driver)
{
	rdpAndroidPrinterDriver* ad = (rdpAndroidPrinterDriver*)driver;
	if (ad)
		ad->references++;
}

static void printer_android_release_ref_driver(rdpPrinterDriver* driver)
{
	rdpAndroidPrinterDriver* ad = (rdpAndroidPrinterDriver*)driver;

	WINPR_ASSERT(ad);
	if (ad->references <= 1)
		free(ad);
	else
		ad->references--;
}

FREERDP_ENTRY_POINT(UINT VCAPITYPE android_freerdp_printer_client_subsystem_entry(void* arg))
{
	rdpPrinterDriver** ppDriver = (rdpPrinterDriver**)arg;

	if (!ppDriver)
		return ERROR_INVALID_PARAMETER;

	rdpAndroidPrinterDriver* drv = calloc(1, sizeof(*drv));
	if (!drv)
		return ERROR_OUTOFMEMORY;

	drv->driver.EnumPrinters = printer_android_enum_printers;
	drv->driver.ReleaseEnumPrinters = printer_android_release_enum_printers;
	drv->driver.GetPrinter = printer_android_get_printer;
	drv->driver.AddRef = printer_android_add_ref_driver;
	drv->driver.ReleaseRef = printer_android_release_ref_driver;
	drv->id_sequence = 1;

	WINPR_ASSERT(drv->driver.AddRef);
	drv->driver.AddRef(&drv->driver);

	*ppDriver = &drv->driver;
	return CHANNEL_RC_OK;
}
