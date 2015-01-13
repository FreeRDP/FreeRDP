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

void Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{

}

void Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	if (bitmap)
	{
		bitmap->Free(context, bitmap);

		if (bitmap->data)
			_aligned_free(bitmap->data);

		free(bitmap);
	}
}

void Bitmap_SetRectangle(rdpContext* context, rdpBitmap* bitmap, UINT16 left, UINT16 top, UINT16 right, UINT16 bottom)
{
	bitmap->left = left;
	bitmap->top = top;
	bitmap->right = right;
	bitmap->bottom = bottom;
}

void Bitmap_SetDimensions(rdpContext* context, rdpBitmap* bitmap, UINT16 width, UINT16 height)
{
	bitmap->width = width;
	bitmap->height = height;
}

/* static method */
void Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, BOOL primary)
{
	context->graphics->Bitmap_Prototype->SetSurface(context, bitmap, primary);
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

void Pointer_New(rdpContext* context, rdpPointer* pointer)
{

}

void Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	if (pointer)
	{
		pointer->Free(context, pointer);

		if (pointer->xorMaskData)
		{
			free(pointer->xorMaskData);
			pointer->xorMaskData = NULL;
		}

		if (pointer->andMaskData)
		{
			free(pointer->andMaskData);
			pointer->andMaskData = NULL;
		}

		free(pointer);
	}
}

/* static method */
void Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	context->graphics->Pointer_Prototype->Set(context, pointer);
}

void Pointer_SetNull(rdpContext* context)
{
	context->graphics->Pointer_Prototype->SetNull(context);
}

void Pointer_SetDefault(rdpContext* context)
{
	context->graphics->Pointer_Prototype->SetDefault(context);
}

void Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	IFCALL(context->graphics->Pointer_Prototype->SetPosition, context, x, y);
}

void graphics_register_pointer(rdpGraphics* graphics, rdpPointer* pointer)
{
	CopyMemory(graphics->Pointer_Prototype, pointer, sizeof(rdpPointer));
}

/* Glyph Class */

rdpGlyph* Glyph_Alloc(rdpContext* context)
{
	rdpGlyph* glyph;
	rdpGraphics* graphics;

	graphics = context->graphics;
	glyph = (rdpGlyph*) calloc(1, graphics->Glyph_Prototype->size);

	if (glyph)
	{
		CopyMemory(glyph, graphics->Glyph_Prototype, sizeof(rdpGlyph));
	}

	return glyph;
}

void Glyph_New(rdpContext* context, rdpGlyph* glyph)
{
	context->graphics->Glyph_Prototype->New(context, glyph);
}

void Glyph_Free(rdpContext* context, rdpGlyph* glyph)
{
	context->graphics->Glyph_Prototype->Free(context, glyph);
}

void Glyph_Draw(rdpContext* context, rdpGlyph* glyph, int x, int y)
{
	context->graphics->Glyph_Prototype->Draw(context, glyph, x, y);
}

void Glyph_BeginDraw(rdpContext* context, int x, int y, int width, int height, UINT32 bgcolor, UINT32 fgcolor, BOOL fOpRedundant)
{
	context->graphics->Glyph_Prototype->BeginDraw(context, x, y, width, height, bgcolor, fgcolor, fOpRedundant);
}

void Glyph_EndDraw(rdpContext* context, int x, int y, int width, int height, UINT32 bgcolor, UINT32 fgcolor)
{
	context->graphics->Glyph_Prototype->EndDraw(context, x, y, width, height, bgcolor, fgcolor);
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
			free (graphics);
			return NULL;
		}

		graphics->Bitmap_Prototype->size = sizeof(rdpBitmap);
		graphics->Bitmap_Prototype->New = Bitmap_New;
		graphics->Bitmap_Prototype->Free = Bitmap_Free;

		graphics->Pointer_Prototype = (rdpPointer*) calloc(1, sizeof(rdpPointer));

		if (!graphics->Pointer_Prototype)
		{
			free (graphics->Bitmap_Prototype);
			free (graphics);
			return NULL;
		}

		graphics->Pointer_Prototype->size = sizeof(rdpPointer);
		graphics->Pointer_Prototype->New = Pointer_New;
		graphics->Pointer_Prototype->Free = Pointer_Free;

		graphics->Glyph_Prototype = (rdpGlyph*) calloc(1, sizeof(rdpGlyph));

		if (!graphics->Glyph_Prototype)
		{
			free (graphics->Pointer_Prototype);
			free (graphics->Bitmap_Prototype);
			free (graphics);
			return NULL;
		}

		graphics->Glyph_Prototype->size = sizeof(rdpGlyph);
		graphics->Glyph_Prototype->New = Glyph_New;
		graphics->Glyph_Prototype->Free = Glyph_Free;
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
