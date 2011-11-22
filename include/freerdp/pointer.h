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
	uint32 xPos;
	uint32 yPos;
};
typedef struct _POINTER_POSITION_UPDATE POINTER_POSITION_UPDATE;

struct _POINTER_SYSTEM_UPDATE
{
	uint32 type;
};
typedef struct _POINTER_SYSTEM_UPDATE POINTER_SYSTEM_UPDATE;

struct _POINTER_COLOR_UPDATE
{
	uint32 cacheIndex;
	uint32 xPos;
	uint32 yPos;
	uint32 width;
	uint32 height;
	uint32 lengthAndMask;
	uint32 lengthXorMask;
	uint8* xorMaskData;
	uint8* andMaskData;
};
typedef struct _POINTER_COLOR_UPDATE POINTER_COLOR_UPDATE;

struct _POINTER_NEW_UPDATE
{
	uint32 xorBpp;
	POINTER_COLOR_UPDATE colorPtrAttr;
};
typedef struct _POINTER_NEW_UPDATE POINTER_NEW_UPDATE;

struct _POINTER_CACHED_UPDATE
{
	uint32 cacheIndex;
};
typedef struct _POINTER_CACHED_UPDATE POINTER_CACHED_UPDATE;

typedef void (*pPointerPosition)(rdpContext* context, POINTER_POSITION_UPDATE* pointer_position);
typedef void (*pPointerSystem)(rdpContext* context, POINTER_SYSTEM_UPDATE* pointer_system);
typedef void (*pPointerColor)(rdpContext* context, POINTER_COLOR_UPDATE* pointer_color);
typedef void (*pPointerNew)(rdpContext* context, POINTER_NEW_UPDATE* pointer_new);
typedef void (*pPointerCached)(rdpContext* context, POINTER_CACHED_UPDATE* pointer_cached);

struct rdp_pointer_update
{
	rdpContext* context; /* 0 */
	uint32 paddingA[16 - 1]; /* 1 */

	pPointerPosition PointerPosition; /* 16 */
	pPointerSystem PointerSystem; /* 17 */
	pPointerColor PointerColor; /* 18 */
	pPointerNew PointerNew; /* 19 */
	pPointerCached PointerCached; /* 20 */
	uint32 paddingB[32 - 21]; /* 21 */

	/* internal */

	POINTER_POSITION_UPDATE pointer_position;
	POINTER_SYSTEM_UPDATE pointer_system;
	POINTER_COLOR_UPDATE pointer_color;
	POINTER_NEW_UPDATE pointer_new;
	POINTER_CACHED_UPDATE pointer_cached;
};
typedef struct rdp_pointer_update rdpPointerUpdate;

#endif /* __UPDATE_POINTER_H */
