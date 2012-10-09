/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/client/channels.h>

extern const VIRTUAL_CHANNEL_ENTRY VIRTUAL_CHANNEL_TABLE[];
extern const DEVICE_SERVICE_ENTRY DEVICE_SERVICE_TABLE[];

void* freerdp_channels_find_static_virtual_channel_entry(const char* name)
{
	int index = 0;
	VIRTUAL_CHANNEL_ENTRY* pEntry;

	pEntry = (VIRTUAL_CHANNEL_ENTRY*) &VIRTUAL_CHANNEL_TABLE[index++];

	while (pEntry->entry != NULL)
	{
		if (strcmp(pEntry->name, name) == 0)
		{
			return (void*) pEntry->entry;
		}

		pEntry = (VIRTUAL_CHANNEL_ENTRY*) &VIRTUAL_CHANNEL_TABLE[index++];
	}

	return NULL;
}

void* freerdp_channels_find_static_device_service_entry(const char* name)
{
	int index = 0;
	DEVICE_SERVICE_ENTRY* pEntry;

	pEntry = (DEVICE_SERVICE_ENTRY*) &DEVICE_SERVICE_TABLE[index++];

	while (pEntry->entry != NULL)
	{
		if (strcmp(pEntry->name, name) == 0)
		{
			return (void*) pEntry->entry;
		}

		pEntry = (DEVICE_SERVICE_ENTRY*) &DEVICE_SERVICE_TABLE[index++];		
	}

	return NULL;	
}

void* freerdp_channels_find_static_entry(const char* name, const char* entry)
{
	if (strcmp(entry, "VirtualChannelEntry") == 0)
	{
		return freerdp_channels_find_static_virtual_channel_entry(name);
	}
	else if (strcmp(entry, "DeviceServiceEntry") == 0)
	{
		return freerdp_channels_find_static_device_service_entry(name);
	}

	return NULL;
}

