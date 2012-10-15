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

extern const STATIC_ENTRY_TABLE CLIENT_STATIC_ENTRY_TABLES[];

void* freerdp_channels_find_static_entry_in_table(const STATIC_ENTRY_TABLE* table, const char* entry)
{
	int index = 0;
	STATIC_ENTRY* pEntry;

	pEntry = (STATIC_ENTRY*) &table->table[index++];

	while (pEntry->entry != NULL)
	{
		if (strcmp(pEntry->name, entry) == 0)
		{
			return (void*) pEntry->entry;
		}

		pEntry = (STATIC_ENTRY*) &table->table[index++];
	}

	return NULL;
}

void* freerdp_channels_find_static_entry(const char* name, const char* entry)
{
	int index = 0;
	STATIC_ENTRY_TABLE* pEntry;

	pEntry = (STATIC_ENTRY_TABLE*) &CLIENT_STATIC_ENTRY_TABLES[index++];

	while (pEntry->table != NULL)
	{
		if (strcmp(pEntry->name, name) == 0)
		{
			return freerdp_channels_find_static_entry_in_table(pEntry, entry);
		}

		pEntry = (STATIC_ENTRY_TABLE*) &CLIENT_STATIC_ENTRY_TABLES[index++];
	}

	return NULL;
}

void* freerdp_channels_find_static_virtual_channel_entry(const char* name)
{
	return freerdp_channels_find_static_entry("VirtualChannelEntry", name);
}

void* freerdp_channels_find_static_device_service_entry(const char* name)
{
	return freerdp_channels_find_static_entry("DeviceServiceEntry", name);
}
