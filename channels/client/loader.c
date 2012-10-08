/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channel Loader
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

extern const VIRTUAL_CHANNEL_ENTRY VIRTUAL_CHANNEL_TABLE[];
extern const DEVICE_SERVICE_ENTRY DEVICE_SERVICE_TABLE[];

void* freerdp_channels_find_static_entry(const char* name, const char* entry)
{
	if (strcmp(entry, "VirtualChannelEntry") == 0)
	{
		VIRTUAL_CHANNEL_ENTRY* pEntry;

		pEntry = (VIRTUAL_CHANNEL_ENTRY*) &VIRTUAL_CHANNEL_TABLE[0];

		while (pEntry != NULL)
		{
			if (strcmp(pEntry->name, name) == 0)
			{
				return (void*) pEntry->entry;
			}

			return NULL;
		}
	}
	else if (strcmp(entry, "DeviceServiceEntry") == 0)
	{
		DEVICE_SERVICE_ENTRY* pEntry;

		pEntry = (DEVICE_SERVICE_ENTRY*) &DEVICE_SERVICE_TABLE[0];

		while (pEntry != NULL)
		{
			if (strcmp(pEntry->name, name) == 0)
			{
				return (void*) pEntry->entry;
			}

			return NULL;
		}
	}

	return NULL;
}
