/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <freerdp/graphics.h>

#include "graphics.h"

/* Bitmap Class */

rdpBitmap* Bitmap_Alloc(rdpContext* context)
{
	rdpBitmap* bitmap;
	rdpGraphics* graphics;
	graphics = context->graphics;
	bitmap = (rdpBitmap*) calloc(1, graphics->Bitmap_Prototype->size);

	if (bitmap)
	{
		CopyMemory(bitmap, graphics->Bitmap_Prototype, sizeof(rdpBitmap));
		bitmap->data = NULL;
	}

	return bitmap;
}

static BOOL Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	if (!bitmap || !context)
		return FALSE;

	*bitmap = *context->graphics->Bitmap_Prototype;
	return TRUE;
}

void Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	if (bitmap)
		bitmap->Free(context, bitmap);
}

BOOL Bitmap_SetRectangle(rdpBitmap* bitmap,
                         UINT16 left,
                         UINT16 top, UINT16 right, UINT16 bottom)
{
	if (!bitmap)
		return FALSE;

	bitmap->left = left;
	bitmap->top = top;
	bitmap->right = right;
	bitmap->bottom = bottom;
	return TRUE;
}

BOOL Bitmap_SetDimensions(rdpBitmap* bitmap,
                          UINT16 width,
                          UINT16 height)
{
	if (!bitmap)
		return FALSE;

	bitmap->right = bitmap->left + width - 1;
	bitmap->bottom = bitmap->top + height - 1;
	bitmap->width = width;
	bitmap->height = height;
	return TRUE;
}

void graphics_register_bitmap(rdpGraphics* graphics, rdpBitmap* bitmap)
{
	CopyMemory(graphics->Bitmap_Prototype, bitmap, sizeof(rdpBitmap));
}

/* Pointer Class */
rdpPointer* Pointer_Alloc(rdpContext* context)
{
	rdpPointer* pointer;
	rdpGraphics* graphics;
	graphics = context->graphics;
	pointer = (rdpPointer*) calloc(1, graphics->Pointer_Prototype->size);

	if (pointer)
	{
		CopyMemory(pointer, graphics->Pointer_Prototype, sizeof(rdpPointer));
	}

	return pointer;
}

static BOOL Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	rdpPointer* proto;

	if (!context || !context->graphics || !context->graphics->Pointer_Prototype)
		return FALSE;

	proto = context->graphics->Pointer_Prototype;
	*pointer = *proto;
	return TRUE;
}

/* static method */
void graphics_register_pointer(rdpGraphics* graphics, rdpPointer* pointer)
{
	CopyMemory(graphics->Pointer_Prototype, pointer, sizeof(rdpPointer));
}

/* Glyph Class */

rdpGlyph* Glyph_Alloc(rdpContext* context, INT32 x, INT32 y,
                      UINT32 cx, UINT32 cy, UINT32 cb, const BYTE* aj)
{
	rdpGlyph* glyph;
	rdpGraphics* graphics;

	if (!context || !context->graphics)
		return NULL;

	graphics = context->graphics;

	if (!graphics->Glyph_Prototype)
		return NULL;

	glyph = (rdpGlyph*) calloc(1, graphics->Glyph_Prototype->size);

	if (!glyph)
		return NULL;

	*glyph = *graphics->Glyph_Prototype;
	glyph->cb = cb;
	glyph->cx = cx;
	glyph->cy = cy;
	glyph->x = x;
	glyph->y = y;
	glyph->aj = malloc(glyph->cb);

	if (!glyph->aj)
	{
		free(glyph);
		return NULL;
	}

	CopyMemory(glyph->aj, aj, cb);

	if (!glyph->New(context, glyph))
	{
		free(glyph->aj);
		free(glyph);
		return NULL;
	}

	return glyph;
}

void graphics_register_glyph(rdpGraphics* graphics, rdpGlyph* glyph)
{
	CopyMemory(graphics->Glyph_Prototype, glyph, sizeof(rdpGlyph));
}

/* Graphics Module */

rdpGraphics* graphics_new(rdpContext* context)
{
	rdpGraphics* graphics;
	graphics = (rdpGraphics*) calloc(1, sizeof(rdpGraphics));

	if (graphics)
	{
		graphics->context = context;
		graphics->Bitmap_Prototype = (rdpBitmap*) calloc(1, sizeof(rdpBitmap));

		if (!graphics->Bitmap_Prototype)
		{
			free(graphics);
			return NULL;
		}

		graphics->Bitmap_Prototype->size = sizeof(rdpBitmap);
		graphics->Bitmap_Prototype->New = Bitmap_New;
		graphics->Bitmap_Prototype->Free = NULL;
		graphics->Pointer_Prototype = (rdpPointer*) calloc(1, sizeof(rdpPointer));

		if (!graphics->Pointer_Prototype)
		{
			free(graphics->Bitmap_Prototype);
			free(graphics);
			return NULL;
		}

		graphics->Pointer_Prototype->size = sizeof(rdpPointer);
		graphics->Pointer_Prototype->New = Pointer_New;
		graphics->Pointer_Prototype->Free = NULL;
		graphics->Glyph_Prototype = (rdpGlyph*) calloc(1, sizeof(rdpGlyph));

		if (!graphics->Glyph_Prototype)
		{
			free(graphics->Pointer_Prototype);
			free(graphics->Bitmap_Prototype);
			free(graphics);
			return NULL;
		}

		graphics->Glyph_Prototype->size = sizeof(rdpGlyph);
	}

	return graphics;
}

void graphics_free(rdpGraphics* graphics)
{
	if (graphics)
	{
		free(graphics->Bitmap_Prototype);
		free(graphics->Pointer_Prototype);
		free(graphics->Glyph_Prototype);
		free(graphics);
	}
}
