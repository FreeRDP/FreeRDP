/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/client/channels.h>

#include "rdpdr_main.h"

#include "devman.h"

void devman_device_free(DEVICE* device)
{
	IFCALL(device->Free, device);
}

DEVMAN* devman_new(rdpdrPlugin* rdpdr)
{
	DEVMAN* devman;

	devman = (DEVMAN*) malloc(sizeof(DEVMAN));
	ZeroMemory(devman, sizeof(DEVMAN));

	devman->plugin = (void*) rdpdr;
	devman->id_sequence = 1;

	devman->devices = ListDictionary_New(TRUE);

	ListDictionary_ValueObject(devman->devices)->fnObjectFree =
			(OBJECT_FREE_FN) devman_device_free;

	return devman;
}

void devman_free(DEVMAN* devman)
{
	ListDictionary_Free(devman->devices);
	free(devman);
}

void devman_unregister_device(DEVMAN* devman, void* key)
{
	DEVICE* device;

	device = (DEVICE*) ListDictionary_Remove(devman->devices, key);

	if (device)
		devman_device_free(device);
}

static void devman_register_device(DEVMAN* devman, DEVICE* device)
{
	void* key = NULL;

	device->id = devman->id_sequence++;
	key = (void*) (size_t) device->id;

	ListDictionary_Add(devman->devices, key, device);
}

DEVICE* devman_get_device_by_id(DEVMAN* devman, UINT32 id)
{
	DEVICE* device = NULL;
	void* key = (void*) (size_t) id;

	device = (DEVICE*) ListDictionary_GetItemValue(devman->devices, key);

	return device;
}

static char DRIVE_SERVICE_NAME[] = "drive";
static char PRINTER_SERVICE_NAME[] = "printer";
static char SMARTCARD_SERVICE_NAME[] = "smartcard";
static char SERIAL_SERVICE_NAME[] = "serial";
static char PARALLEL_SERVICE_NAME[] = "parallel";

BOOL devman_load_device_service(DEVMAN* devman, RDPDR_DEVICE* device)
{
	char* ServiceName = NULL;
	DEVICE_SERVICE_ENTRY_POINTS ep;
	PDEVICE_SERVICE_ENTRY entry = NULL;

	if (device->Type == RDPDR_DTYP_FILESYSTEM)
		ServiceName = DRIVE_SERVICE_NAME;
	else if (device->Type == RDPDR_DTYP_PRINT)
		ServiceName = PRINTER_SERVICE_NAME;
	else if (device->Type == RDPDR_DTYP_SMARTCARD)
		ServiceName = SMARTCARD_SERVICE_NAME;
	else if (device->Type == RDPDR_DTYP_SERIAL)
		ServiceName = SERIAL_SERVICE_NAME;
	else if (device->Type == RDPDR_DTYP_PARALLEL)
		ServiceName = PARALLEL_SERVICE_NAME;

	if (!ServiceName)
		return FALSE;

	fprintf(stderr, "Loading device service %s (static)\n", ServiceName);
	entry = (PDEVICE_SERVICE_ENTRY) freerdp_load_channel_addin_entry(ServiceName, NULL, "DeviceServiceEntry", 0);

	if (!entry)
		return FALSE;

	ep.devman = devman;
	ep.RegisterDevice = devman_register_device;
	ep.device = device;

	entry(&ep);

	return TRUE;
}
