/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Graphical Objects
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

#ifndef __GRAPHICS_H
#define __GRAPHICS_H

typedef struct rdp_bitmap rdpBitmap;

#include <stdlib.h>
#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>

typedef void (*pBitmap_New)(rdpContext* context, rdpBitmap* bitmap);
typedef void (*pBitmap_Free)(rdpContext* context, rdpBitmap* bitmap);
typedef void (*pBitmap_Paint)(rdpContext* context, rdpBitmap* bitmap, int x, int y);
typedef void (*pBitmap_Decompress)(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed);

struct rdp_bitmap
{
	size_t size;

	pBitmap_New New;
	pBitmap_Free Free;
	pBitmap_Paint Paint;
	pBitmap_Decompress Decompress;

	uint16 left;
	uint16 top;
	uint16 right;
	uint16 bottom;
	uint16 width;
	uint16 height;
	uint16 bpp;
	uint16 flags;
	uint32 length;
	uint8* data;

	boolean compressed;
};

FREERDP_API rdpBitmap* Bitmap_Alloc(rdpContext* context);
FREERDP_API void Bitmap_New(rdpContext* context, rdpBitmap* bitmap);
FREERDP_API void Bitmap_Free(rdpContext* context, rdpBitmap* bitmap);
FREERDP_API void Bitmap_Register(rdpContext* context, rdpBitmap* bitmap);
FREERDP_API void Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed);

struct rdp_graphics
{
	rdpContext* context;
	rdpBitmap* Bitmap_Prototype;
};

FREERDP_API void graphics_register_bitmap(rdpGraphics* graphics, rdpBitmap* bitmap);

FREERDP_API rdpGraphics* graphics_new(rdpContext* context);
FREERDP_API void graphics_free(rdpGraphics* graphics);

#endif /* __GRAPHICS_H */
