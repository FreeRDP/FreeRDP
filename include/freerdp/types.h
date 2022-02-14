/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Type Definitions
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_TYPES_H
#define FREERDP_TYPES_H

#include <winpr/wtypes.h>
#include <winpr/wtsapi.h>

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

typedef struct
{
	BYTE red;
	BYTE green;
	BYTE blue;
} PALETTE_ENTRY;

typedef struct
{
	UINT32 count;
	PALETTE_ENTRY entries[256];
} rdpPalette;

#include <freerdp/settings.h>

typedef struct
{
	DWORD size;
	void* data[4];
} RDP_PLUGIN_DATA;

typedef struct
{
	INT16 x;
	INT16 y;
	INT16 width;
	INT16 height;
} RDP_RECT;

typedef struct
{
	UINT16 left;
	UINT16 top;
	UINT16 right;
	UINT16 bottom;
} RECTANGLE_16;

typedef struct
{
	UINT32 left;
	UINT32 top;
	UINT32 width;
	UINT32 height;
} RECTANGLE_32;

/* Plugin events */

#include <freerdp/message.h>
#include <winpr/collections.h>

#endif /* __RDP_TYPES_H */
