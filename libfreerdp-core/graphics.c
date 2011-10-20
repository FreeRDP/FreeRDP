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

rdpBitmap* Bitmap_Alloc(rdpContext* context)
{
	rdpBitmap* bitmap;
	rdpGraphics* graphics;

	graphics = context->graphics;
	bitmap = (rdpBitmap*) xmalloc(graphics->Bitmap_Prototype->size);

	if (bitmap != NULL)
	{
		memcpy(bitmap, context->graphics->Bitmap_Prototype, sizeof(rdpBitmap));
	}

	return bitmap;
}

void Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{

}

void Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed)
{
	bitmap->Decompress(context, bitmap, data, width, height, bpp, length, compressed);
}

void Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap, int x, int y)
{
	bitmap->Paint(context, bitmap, x, y);
}

void Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	if (bitmap != NULL)
	{
		if (bitmap->data != NULL)
			xfree(bitmap->data);

		xfree(bitmap);
	}
}

void graphics_register_bitmap(rdpGraphics* graphics, rdpBitmap* bitmap)
{
	memcpy(graphics->Bitmap_Prototype, bitmap, sizeof(rdpBitmap));
}

rdpGraphics* graphics_new(rdpContext* context)
{
	rdpGraphics* graphics;

	graphics = (rdpGraphics*) xzalloc(sizeof(rdpGraphics));

	if (graphics != NULL)
	{
		graphics->Bitmap_Prototype = (rdpBitmap*) xmalloc(sizeof(rdpBitmap));
		graphics->Bitmap_Prototype->size = sizeof(rdpBitmap);
		graphics->Bitmap_Prototype->New = Bitmap_New;
		graphics->Bitmap_Prototype->Free = Bitmap_Free;
	}

	return graphics;
}

void graphics_free(rdpGraphics* graphics)
{
	if (graphics != NULL)
	{
		xfree(graphics->Bitmap_Prototype);
		xfree(graphics);
	}
}
