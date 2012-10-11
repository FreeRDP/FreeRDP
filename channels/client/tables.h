/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Static Entry Point Tables
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

#include <freerdp/svc.h>

#ifndef PDEVICE_SERVICE_ENTRY_POINTS
#define PDEVICE_SERVICE_ENTRY_POINTS void*
#endif

typedef int (*VIRTUAL_CHANNEL_ENTRY_FN)(PCHANNEL_ENTRY_POINTS pEntryPoints);
typedef int (*DEVICE_SERVICE_ENTRY_FN)(PDEVICE_SERVICE_ENTRY_POINTS pEntryPoints);

struct _VIRTUAL_CHANNEL_ENTRY
{
	const char* name;
	const VIRTUAL_CHANNEL_ENTRY_FN entry;
};
typedef struct _VIRTUAL_CHANNEL_ENTRY VIRTUAL_CHANNEL_ENTRY;

struct _DEVICE_SERVICE_ENTRY
{
	const char* name;
	const DEVICE_SERVICE_ENTRY_FN entry;
};
typedef struct _DEVICE_SERVICE_ENTRY DEVICE_SERVICE_ENTRY;

