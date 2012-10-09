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

#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/client/channels.h>

#include "rdpdr_main.h"
#include "devman.h"

DEVMAN* devman_new(rdpSvcPlugin* plugin)
{
	DEVMAN* devman;

	devman = xnew(DEVMAN);
	devman->plugin = plugin;
	devman->id_sequence = 1;
	devman->devices = list_new();

	return devman;
}

void devman_free(DEVMAN* devman)
{
	DEVICE* device;

	while ((device = (DEVICE*) list_dequeue(devman->devices)) != NULL)
		IFCALL(device->Free, device);

	list_free(devman->devices);

	free(devman);
}

static void devman_register_device(DEVMAN* devman, DEVICE* device)
{
	device->id = devman->id_sequence++;
	list_add(devman->devices, device);

	DEBUG_SVC("device %d.%s registered", device->id, device->name);
}

BOOL devman_load_device_service(DEVMAN* devman, RDP_PLUGIN_DATA* plugin_data)
{
	char* name;
	DEVICE_SERVICE_ENTRY_POINTS ep;
	PDEVICE_SERVICE_ENTRY entry = NULL;

	name = (char*) plugin_data->data[0];
	entry = (PDEVICE_SERVICE_ENTRY) freerdp_channels_find_static_device_service_entry(name);

	if (!entry)
	{
		printf("loading device service %s (plugin)\n", name);
		entry = freerdp_load_plugin(name, "DeviceServiceEntry");
	}
	else
	{
		printf("loading device service %s (static)\n", name);
	}

	if (entry == NULL)
		return FALSE;

	ep.devman = devman;
	ep.RegisterDevice = devman_register_device;
	ep.plugin_data = plugin_data;

	entry(&ep);

	return TRUE;
}

DEVICE* devman_get_device_by_id(DEVMAN* devman, UINT32 id)
{
	LIST_ITEM* item;
	DEVICE* device;

	for (item = devman->devices->head; item; item = item->next)
	{
		device = (DEVICE*) item->data;

		if (device->id == id)
			return device;
	}

	return NULL;
}
