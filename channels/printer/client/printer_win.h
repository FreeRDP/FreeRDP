/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Print Virtual Channel - win driver
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

#ifndef __PRINTER_WIN_H
#define __PRINTER_WIN_H

#include <freerdp/channels/log.h>

rdpPrinterDriver* printer_win_get_driver(void);

#define PRINTER_TAG CHANNELS_TAG("printer.client")
#ifdef WITH_DEBUG_WINPR
#define DEBUG_WINPR(fmt, ...) WLog_DBG(PRINTER_TAG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_WINPR(fmt, ...) do { } while (0)
#endif

#endif

