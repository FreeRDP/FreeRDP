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
typedef struct rdp_pointer rdpPointer;
typedef struct rdp_glyph rdpGlyph;

#include <stdlib.h>
#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>

/* Bitmap Class */

typedef void (*pBitmap_New)(rdpContext* context, rdpBitmap* bitmap);
typedef void (*pBitmap_Free)(rdpContext* context, rdpBitmap* bitmap);
typedef void (*pBitmap_Paint)(rdpContext* context, rdpBitmap* bitmap);
typedef void (*pBitmap_Decompress)(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed);
typedef void (*pBitmap_SetSurface)(rdpContext* context, rdpBitmap* bitmap, boolean primary);

struct rdp_bitmap
{
	size_t size; /* 0 */
	pBitmap_New New; /* 1 */
	pBitmap_Free Free; /* 2 */
	pBitmap_Paint Paint; /* 3 */
	pBitmap_Decompress Decompress; /* 4 */
	pBitmap_SetSurface SetSurface; /* 5 */
	uint32 paddingA[16 - 6];  /* 6 */

	uint32 left; /* 16 */
	uint32 top; /* 17 */
	uint32 right; /* 18 */
	uint32 bottom; /* 19 */
	uint32 width; /* 20 */
	uint32 height; /* 21 */
	uint32 bpp; /* 22 */
	uint32 flags; /* 23 */
	uint32 length; /* 24 */
	uint8* data; /* 25 */
	uint32 paddingB[32 - 26]; /* 26 */

	boolean compressed; /* 32 */
	boolean ephemeral; /* 33 */
	uint32 paddingC[64 - 34]; /* 34 */
};

FREERDP_API rdpBitmap* Bitmap_Alloc(rdpContext* context);
FREERDP_API void Bitmap_New(rdpContext* context, rdpBitmap* bitmap);
FREERDP_API void Bitmap_Free(rdpContext* context, rdpBitmap* bitmap);
FREERDP_API void Bitmap_Register(rdpContext* context, rdpBitmap* bitmap);
FREERDP_API void Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed);
FREERDP_API void Bitmap_SetRectangle(rdpContext* context, rdpBitmap* bitmap,
		uint16 left, uint16 top, uint16 right, uint16 bottom);
FREERDP_API void Bitmap_SetDimensions(rdpContext* context, rdpBitmap* bitmap, uint16 width, uint16 height);
FREERDP_API void Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, boolean primary);

/* Pointer Class */

typedef void (*pPointer_New)(rdpContext* context, rdpPointer* pointer);
typedef void (*pPointer_Free)(rdpContext* context, rdpPointer* pointer);
typedef void (*pPointer_Set)(rdpContext* context, rdpPointer* pointer);

struct rdp_pointer
{
	size_t size; /* 0 */
	pPointer_New New; /* 1 */
	pPointer_Free Free; /* 2 */
	pPointer_Set Set; /* 3 */
	uint32 paddingA[16 - 4]; /* 4 */

	uint32 xPos; /* 16 */
	uint32 yPos; /* 17 */
	uint32 width; /* 18 */
	uint32 height; /* 19 */
	uint32 xorBpp; /* 20 */
	uint32 lengthAndMask; /* 21 */
	uint32 lengthXorMask; /* 22 */
	uint8* xorMaskData; /* 23 */
	uint8* andMaskData; /* 24 */
	uint32 paddingB[32 - 25]; /* 25 */
};

FREERDP_API rdpPointer* Pointer_Alloc(rdpContext* context);
FREERDP_API void Pointer_New(rdpContext* context, rdpPointer* pointer);
FREERDP_API void Pointer_Free(rdpContext* context, rdpPointer* pointer);
FREERDP_API void Pointer_Set(rdpContext* context, rdpPointer* pointer);

/* Glyph Class */

typedef void (*pGlyph_New)(rdpContext* context, rdpGlyph* glyph);
typedef void (*pGlyph_Free)(rdpContext* context, rdpGlyph* glyph);
typedef void (*pGlyph_Draw)(rdpContext* context, rdpGlyph* glyph, int x, int y);
typedef void (*pGlyph_BeginDraw)(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor);
typedef void (*pGlyph_EndDraw)(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor);

struct rdp_glyph
{
	size_t size; /* 0 */
	pGlyph_New New; /* 1 */
	pGlyph_Free Free; /* 2 */
	pGlyph_Draw Draw; /* 3 */
	pGlyph_BeginDraw BeginDraw; /* 4 */
	pGlyph_EndDraw EndDraw; /* 5 */
	uint32 paddingA[16 - 6]; /* 6 */

	sint32 x; /* 16 */
	sint32 y; /* 17 */
	uint32 cx; /* 18 */
	uint32 cy; /* 19 */
	uint32 cb; /* 20 */
	uint8* aj; /* 21 */
	uint32 paddingB[32 - 22]; /* 22 */
};

FREERDP_API rdpGlyph* Glyph_Alloc(rdpContext* context);
FREERDP_API void Glyph_New(rdpContext* context, rdpGlyph* glyph);
FREERDP_API void Glyph_Free(rdpContext* context, rdpGlyph* glyph);
FREERDP_API void Glyph_Draw(rdpContext* context, rdpGlyph* glyph, int x, int y);
FREERDP_API void Glyph_BeginDraw(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor);
FREERDP_API void Glyph_EndDraw(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor);

/* Graphics Module */

struct rdp_graphics
{
	rdpContext* context; /* 0 */
	rdpBitmap* Bitmap_Prototype; /* 1 */
	rdpPointer* Pointer_Prototype; /* 2 */
	rdpGlyph* Glyph_Prototype; /* 3 */
	uint32 paddingA[16 - 4]; /* 4 */
};

FREERDP_API void graphics_register_bitmap(rdpGraphics* graphics, rdpBitmap* bitmap);
FREERDP_API void graphics_register_pointer(rdpGraphics* graphics, rdpPointer* pointer);
FREERDP_API void graphics_register_glyph(rdpGraphics* graphics, rdpGlyph* glyph);

FREERDP_API rdpGraphics* graphics_new(rdpContext* context);
FREERDP_API void graphics_free(rdpGraphics* graphics);

#endif /* __GRAPHICS_H */
