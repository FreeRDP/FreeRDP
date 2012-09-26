/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Print Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
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

#ifndef __PRINTER_MAIN_H
#define __PRINTER_MAIN_H

#include "rdpdr_types.h"

/* SERVER_PRINTER_CACHE_EVENT.cachedata */
#define RDPDR_ADD_PRINTER_EVENT             0x00000001
#define RDPDR_UPDATE_PRINTER_EVENT          0x00000002
#define RDPDR_DELETE_PRINTER_EVENT          0x00000003
#define RDPDR_RENAME_PRINTER_EVENT          0x00000004

/* DR_PRN_DEVICE_ANNOUNCE.Flags */
#define RDPDR_PRINTER_ANNOUNCE_FLAG_ASCII           0x00000001
#define RDPDR_PRINTER_ANNOUNCE_FLAG_DEFAULTPRINTER  0x00000002
#define RDPDR_PRINTER_ANNOUNCE_FLAG_NETWORKPRINTER  0x00000004
#define RDPDR_PRINTER_ANNOUNCE_FLAG_TSPRINTER       0x00000008
#define RDPDR_PRINTER_ANNOUNCE_FLAG_XPSFORMAT       0x00000010

typedef struct rdp_printer_driver rdpPrinterDriver;
typedef struct rdp_printer rdpPrinter;
typedef struct rdp_print_job rdpPrintJob;

typedef rdpPrinter** (*pcEnumPrinters) (rdpPrinterDriver* driver);
typedef rdpPrinter* (*pcGetPrinter) (rdpPrinterDriver* driver, const char* name);

struct rdp_printer_driver
{
	pcEnumPrinters EnumPrinters;
	pcGetPrinter GetPrinter;
};

typedef rdpPrintJob* (*pcCreatePrintJob) (rdpPrinter* printer, uint32 id);
typedef rdpPrintJob* (*pcFindPrintJob) (rdpPrinter* printer, uint32 id);
typedef void (*pcFreePrinter) (rdpPrinter* printer);

struct rdp_printer
{
	int id;
	char* name;
	char* driver;
	boolean is_default;

	pcCreatePrintJob CreatePrintJob;
	pcFindPrintJob FindPrintJob;
	pcFreePrinter Free;
};

typedef void (*pcWritePrintJob) (rdpPrintJob* printjob, uint8* data, int size);
typedef void (*pcClosePrintJob) (rdpPrintJob* printjob);

struct rdp_print_job
{
	uint32 id;
	rdpPrinter* printer;

	pcWritePrintJob Write;
	pcClosePrintJob Close;
};

#endif
