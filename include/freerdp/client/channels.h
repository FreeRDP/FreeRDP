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

#ifndef FREERDP_CHANNELS_CLIENT
#define FREERDP_CHANNELS_CLIENT

#include <freerdp/api.h>
#include <freerdp/channels/channels.h>

#define FREERDP_ADDIN_CLIENT			0x00000001
#define FREERDP_ADDIN_SERVER			0x00000002

#define FREERDP_ADDIN_STATIC			0x00000010
#define FREERDP_ADDIN_DYNAMIC			0x00000020

#define FREERDP_ADDIN_NAME			0x00000100
#define FREERDP_ADDIN_SUBSYSTEM			0x00000200
#define FREERDP_ADDIN_TYPE			0x00000400

struct _FREERDP_ADDIN
{
	DWORD dwFlags;
	CHAR cName[16];
	CHAR cType[16];
	CHAR cSubsystem[16];
};
typedef struct _FREERDP_ADDIN FREERDP_ADDIN;

FREERDP_API void* freerdp_channels_client_find_static_entry(const char* name, const char* identifier);
FREERDP_API void* freerdp_channels_client_find_dynamic_entry(const char* name, const char* identifier);
FREERDP_API void* freerdp_channels_client_find_entry(const char* name, const char* identifier);

FREERDP_API FREERDP_ADDIN** freerdp_channels_list_client_addins(LPSTR lpName, LPSTR lpSubsystem, LPSTR lpType, DWORD dwFlags);

#endif /* FREERDP_CHANNELS_CLIENT */

