/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Pointer Updates Interface API
 *
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

#ifndef __UPDATE_POINTER_H
#define __UPDATE_POINTER_H

#include <freerdp/types.h>

#define PTR_MSG_TYPE_SYSTEM		0x0001
#define PTR_MSG_TYPE_POSITION		0x0003
#define PTR_MSG_TYPE_COLOR		0x0006
#define PTR_MSG_TYPE_CACHED		0x0007
#define PTR_MSG_TYPE_POINTER		0x0008

#define SYSPTR_NULL			0x00000000
#define SYSPTR_DEFAULT			0x00007F00

struct _POINTER_POSITION_UPDATE
{
	uint16 xPos;
	uint16 yPos;
};
typedef struct _POINTER_POSITION_UPDATE POINTER_POSITION_UPDATE;

struct _POINTER_SYSTEM_UPDATE
{
	uint32 type;
};
typedef struct _POINTER_SYSTEM_UPDATE POINTER_SYSTEM_UPDATE;

struct _POINTER_COLOR_UPDATE
{
	uint16 cacheIndex;
	uint16 xPos;
	uint16 yPos;
	uint16 width;
	uint16 height;
	uint16 lengthAndMask;
	uint16 lengthXorMask;
	uint8* xorMaskData;
	uint8* andMaskData;
};
typedef struct _POINTER_COLOR_UPDATE POINTER_COLOR_UPDATE;

struct _POINTER_NEW_UPDATE
{
	uint16 xorBpp;
	POINTER_COLOR_UPDATE colorPtrAttr;
};
typedef struct _POINTER_NEW_UPDATE POINTER_NEW_UPDATE;

struct _POINTER_CACHED_UPDATE
{
	uint16 cacheIndex;
};
typedef struct _POINTER_CACHED_UPDATE POINTER_CACHED_UPDATE;

typedef void (*pPointerPosition)(rdpUpdate* update, POINTER_POSITION_UPDATE* pointer_position);
typedef void (*pPointerSystem)(rdpUpdate* update, POINTER_SYSTEM_UPDATE* pointer_system);
typedef void (*pPointerColor)(rdpUpdate* update, POINTER_COLOR_UPDATE* pointer_color);
typedef void (*pPointerNew)(rdpUpdate* update, POINTER_NEW_UPDATE* pointer_new);
typedef void (*pPointerCached)(rdpUpdate* update, POINTER_CACHED_UPDATE* pointer_cached);

#endif /* __UPDATE_POINTER_H */
