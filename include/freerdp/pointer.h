/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_UPDATE_POINTER_H
#define FREERDP_UPDATE_POINTER_H

#include <freerdp/types.h>

#define PTR_MSG_TYPE_SYSTEM 0x0001
#define PTR_MSG_TYPE_POSITION 0x0003
#define PTR_MSG_TYPE_COLOR 0x0006
#define PTR_MSG_TYPE_CACHED 0x0007
#define PTR_MSG_TYPE_POINTER 0x0008
#define PTR_MSG_TYPE_POINTER_LARGE 0x0009

#define SYSPTR_NULL 0x00000000
#define SYSPTR_DEFAULT 0x00007F00

struct _POINTER_POSITION_UPDATE
{
	UINT32 xPos;
	UINT32 yPos;
};
typedef struct _POINTER_POSITION_UPDATE POINTER_POSITION_UPDATE;

struct _POINTER_SYSTEM_UPDATE
{
	UINT32 type;
};
typedef struct _POINTER_SYSTEM_UPDATE POINTER_SYSTEM_UPDATE;

struct _POINTER_COLOR_UPDATE
{
	UINT32 cacheIndex;
	UINT32 xPos;
	UINT32 yPos;
	UINT32 width;
	UINT32 height;
	UINT32 lengthAndMask;
	UINT32 lengthXorMask;
	BYTE* xorMaskData;
	BYTE* andMaskData;
};
typedef struct _POINTER_COLOR_UPDATE POINTER_COLOR_UPDATE;

struct _POINTER_LARGE_UPDATE
{
	UINT16 xorBpp;
	UINT16 cacheIndex;
	UINT16 hotSpotX;
	UINT16 hotSpotY;
	UINT16 width;
	UINT16 height;
	UINT32 lengthAndMask;
	UINT32 lengthXorMask;
	BYTE* xorMaskData;
	BYTE* andMaskData;
};
typedef struct _POINTER_LARGE_UPDATE POINTER_LARGE_UPDATE;

struct _POINTER_NEW_UPDATE
{
	UINT32 xorBpp;
	POINTER_COLOR_UPDATE colorPtrAttr;
};
typedef struct _POINTER_NEW_UPDATE POINTER_NEW_UPDATE;

struct _POINTER_CACHED_UPDATE
{
	UINT32 cacheIndex;
};
typedef struct _POINTER_CACHED_UPDATE POINTER_CACHED_UPDATE;

typedef BOOL (*pPointerPosition)(rdpContext* context,
                                 const POINTER_POSITION_UPDATE* pointer_position);
typedef BOOL (*pPointerSystem)(rdpContext* context, const POINTER_SYSTEM_UPDATE* pointer_system);
typedef BOOL (*pPointerColor)(rdpContext* context, const POINTER_COLOR_UPDATE* pointer_color);
typedef BOOL (*pPointerNew)(rdpContext* context, const POINTER_NEW_UPDATE* pointer_new);
typedef BOOL (*pPointerCached)(rdpContext* context, const POINTER_CACHED_UPDATE* pointer_cached);
typedef BOOL (*pPointerLarge)(rdpContext* context, const POINTER_LARGE_UPDATE* pointer_large);

struct rdp_pointer_update
{
	rdpContext* context;     /* 0 */
	UINT32 paddingA[16 - 1]; /* 1 */

	pPointerPosition PointerPosition; /* 16 */
	pPointerSystem PointerSystem;     /* 17 */
	pPointerColor PointerColor;       /* 18 */
	pPointerNew PointerNew;           /* 19 */
	pPointerCached PointerCached;     /* 20 */
	pPointerLarge PointerLarge;       /* 21 */
	UINT32 paddingB[32 - 22];         /* 22 */
};
typedef struct rdp_pointer_update rdpPointerUpdate;

#endif /* FREERDP_UPDATE_POINTER_H */
