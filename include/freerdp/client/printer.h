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

#ifndef FREERDP_CHANNEL_PRINTER_CLIENT_PRINTER_H
#define FREERDP_CHANNEL_PRINTER_CLIENT_PRINTER_H

#include <freerdp/channels/rdpdr.h>

typedef struct rdp_printer_driver rdpPrinterDriver;
typedef struct rdp_printer rdpPrinter;
typedef struct rdp_print_job rdpPrintJob;

typedef void (*pcReferencePrinterDriver)(rdpPrinterDriver* driver);
typedef rdpPrinter** (*pcEnumPrinters)(rdpPrinterDriver* driver);
typedef void (*pcReleaseEnumPrinters)(rdpPrinter** printers);

typedef rdpPrinter* (*pcGetPrinter)(rdpPrinterDriver* driver, const char* name,
                                    const char* driverName, BOOL isDefault);
typedef void (*pcReferencePrinter)(rdpPrinter* printer);

struct rdp_printer_driver
{
	pcEnumPrinters EnumPrinters;
	pcReleaseEnumPrinters ReleaseEnumPrinters;
	pcGetPrinter GetPrinter;

	pcReferencePrinterDriver AddRef;
	pcReferencePrinterDriver ReleaseRef;
};

typedef rdpPrintJob* (*pcCreatePrintJob)(rdpPrinter* printer, UINT32 id);
typedef rdpPrintJob* (*pcFindPrintJob)(rdpPrinter* printer, UINT32 id);

struct rdp_printer
{
	size_t id;
	char* name;
	char* driver;
	BOOL is_default;

	size_t references;
	rdpPrinterDriver* backend;
	pcCreatePrintJob CreatePrintJob;
	pcFindPrintJob FindPrintJob;
	pcReferencePrinter AddRef;
	pcReferencePrinter ReleaseRef;
};

typedef UINT (*pcWritePrintJob)(rdpPrintJob* printjob, const BYTE* data, size_t size);
typedef void (*pcClosePrintJob)(rdpPrintJob* printjob);

struct rdp_print_job
{
	UINT32 id;
	rdpPrinter* printer;

	pcWritePrintJob Write;
	pcClosePrintJob Close;
};

#endif /* FREERDP_CHANNEL_PRINTER_CLIENT_PRINTER_H */
