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

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct rdp_printer_driver rdpPrinterDriver;
	typedef struct rdp_printer rdpPrinter;
	typedef struct rdp_print_job rdpPrintJob;

	typedef void (*pcSetDeviceForPrinterDriver)(rdpPrinterDriver* driver,
	                                            const RDPDR_PRINTER* device);
	typedef void (*pcReferencePrinterDriver)(rdpPrinterDriver* driver);
	typedef rdpPrinter** (*pcEnumPrinters)(rdpPrinterDriver* driver);
	typedef void (*pcReleaseEnumPrinters)(rdpPrinter** printers);

	typedef rdpPrinter* (*pcGetPrinter)(rdpPrinterDriver* driver, const char* name,
	                                    const char* driverName, BOOL isDefault);
	typedef void (*pcReferencePrinter)(rdpPrinter* printer);

	struct rdp_printer_driver
	{
		ALIGN64 WINPR_ATTR_NODISCARD pcEnumPrinters EnumPrinters;
		ALIGN64 pcReleaseEnumPrinters ReleaseEnumPrinters;
		ALIGN64 WINPR_ATTR_NODISCARD pcGetPrinter GetPrinter;

		ALIGN64 pcReferencePrinterDriver AddRef;
		ALIGN64 pcReferencePrinterDriver ReleaseRef;

		/** @brief Function allowing to set a context for a backend.
		 *  @since version 3.27.0
		 */
		ALIGN64 pcSetDeviceForPrinterDriver SetDeviceContext;

		UINT64 reserved[58];
	};

	typedef rdpPrintJob* (*pcCreatePrintJob)(rdpPrinter* printer, UINT32 id);
	typedef rdpPrintJob* (*pcFindPrintJob)(rdpPrinter* printer, UINT32 id);

	struct rdp_printer
	{
		ALIGN64 size_t id;
		ALIGN64 char* name;
		ALIGN64 char* driver;
		ALIGN64 BOOL is_default;

		ALIGN64 size_t references;
		ALIGN64 rdpPrinterDriver* backend;
		ALIGN64 WINPR_ATTR_NODISCARD pcCreatePrintJob CreatePrintJob;
		ALIGN64 WINPR_ATTR_NODISCARD pcFindPrintJob FindPrintJob;
		ALIGN64 pcReferencePrinter AddRef;
		ALIGN64 pcReferencePrinter ReleaseRef;
		UINT64 reserved[54];
	};

	typedef UINT (*pcWritePrintJob)(rdpPrintJob* printjob, const BYTE* data, size_t size);
	typedef void (*pcClosePrintJob)(rdpPrintJob* printjob);

	struct rdp_print_job
	{
		ALIGN64 UINT32 id;
		ALIGN64 rdpPrinter* printer;

		ALIGN64 WINPR_ATTR_NODISCARD pcWritePrintJob Write;
		ALIGN64 pcClosePrintJob Close;

		UINT64 reserved[60];
	};

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_PRINTER_CLIENT_PRINTER_H */
