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

#include <freerdp/utils/memory.h>

#include <freerdp/graphics.h>

/* Bitmap Class */

rdpBitmap* Bitmap_Alloc(rdpContext* context)
{
	rdpBitmap* bitmap;
	rdpGraphics* graphics;

	graphics = context->graphics;
	bitmap = (rdpBitmap*) xmalloc(graphics->Bitmap_Prototype->size);

	if (bitmap != NULL)
	{
		memcpy(bitmap, context->graphics->Bitmap_Prototype, sizeof(rdpBitmap));
		bitmap->data = NULL;
	}

	return bitmap;
}

void Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{

}

void Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	if (bitmap != NULL)
	{
		bitmap->Free(context, bitmap);

		if (bitmap->data != NULL)
			xfree(bitmap->data);

		xfree(bitmap);
	}
}

void Bitmap_SetRectangle(rdpContext* context, rdpBitmap* bitmap, uint16 left, uint16 top, uint16 right, uint16 bottom)
{
	bitmap->left = left;
	bitmap->top = top;
	bitmap->right = right;
	bitmap->bottom = bottom;
}

void Bitmap_SetDimensions(rdpContext* context, rdpBitmap* bitmap, uint16 width, uint16 height)
{
	bitmap->width = width;
	bitmap->height = height;
}

/* static method */
void Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, boolean primary)
{
	context->graphics->Bitmap_Prototype->SetSurface(context, bitmap, primary);
}

void graphics_register_bitmap(rdpGraphics* graphics, rdpBitmap* bitmap)
{
	memcpy(graphics->Bitmap_Prototype, bitmap, sizeof(rdpBitmap));
}

/* Pointer Class */

rdpPointer* Pointer_Alloc(rdpContext* context)
{
	rdpPointer* pointer;
	rdpGraphics* graphics;

	graphics = context->graphics;
	pointer = (rdpPointer*) xmalloc(graphics->Pointer_Prototype->size);

	if (pointer != NULL)
	{
		memcpy(pointer, context->graphics->Pointer_Prototype, sizeof(rdpPointer));
	}

	return pointer;
}

void Pointer_New(rdpContext* context, rdpPointer* pointer)
{

}

void Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	if (pointer != NULL)
	{
		pointer->Free(context, pointer);

		if (pointer->xorMaskData)
			xfree(pointer->xorMaskData);

		if (pointer->andMaskData)
			xfree(pointer->andMaskData);

		xfree(pointer);
	}
}

/* static method */
void Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	context->graphics->Pointer_Prototype->Set(context, pointer);
}

void graphics_register_pointer(rdpGraphics* graphics, rdpPointer* pointer)
{
	memcpy(graphics->Pointer_Prototype, pointer, sizeof(rdpPointer));
}

/* Glyph Class */

rdpGlyph* Glyph_Alloc(rdpContext* context)
{
	rdpGlyph* glyph;
	rdpGraphics* graphics;

	graphics = context->graphics;
	glyph = (rdpGlyph*) xmalloc(graphics->Glyph_Prototype->size);

	if (glyph != NULL)
	{
		memcpy(glyph, context->graphics->Glyph_Prototype, sizeof(rdpGlyph));
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

void Glyph_BeginDraw(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor)
{
	context->graphics->Glyph_Prototype->BeginDraw(context, x, y, width, height, bgcolor, fgcolor);
}

void Glyph_EndDraw(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor)
{
	context->graphics->Glyph_Prototype->EndDraw(context, x, y, width, height, bgcolor, fgcolor);
}

void graphics_register_glyph(rdpGraphics* graphics, rdpGlyph* glyph)
{
	memcpy(graphics->Glyph_Prototype, glyph, sizeof(rdpGlyph));
}

/* Graphics Module */

rdpGraphics* graphics_new(rdpContext* context)
{
	rdpGraphics* graphics;

	graphics = (rdpGraphics*) xzalloc(sizeof(rdpGraphics));

	if (graphics != NULL)
	{
		graphics->context = context;

		graphics->Bitmap_Prototype = (rdpBitmap*) xzalloc(sizeof(rdpBitmap));
		graphics->Bitmap_Prototype->size = sizeof(rdpBitmap);
		graphics->Bitmap_Prototype->New = Bitmap_New;
		graphics->Bitmap_Prototype->Free = Bitmap_Free;

		graphics->Pointer_Prototype = (rdpPointer*) xzalloc(sizeof(rdpPointer));
		graphics->Pointer_Prototype->size = sizeof(rdpPointer);
		graphics->Pointer_Prototype->New = Pointer_New;
		graphics->Pointer_Prototype->Free = Pointer_Free;

		graphics->Glyph_Prototype = (rdpGlyph*) xzalloc(sizeof(rdpGlyph));
		graphics->Glyph_Prototype->size = sizeof(rdpGlyph);
		graphics->Glyph_Prototype->New = Glyph_New;
		graphics->Glyph_Prototype->Free = Glyph_Free;
	}

	return graphics;
}

void graphics_free(rdpGraphics* graphics)
{
	if (graphics != NULL)
	{
		xfree(graphics->Bitmap_Prototype);
		xfree(graphics->Pointer_Prototype);
		xfree(graphics->Glyph_Prototype);
		xfree(graphics);
	}
}
