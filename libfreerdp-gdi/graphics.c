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

#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/utils/memory.h>
#include <freerdp/codec/bitmap.h>

#include "graphics.h"

HGDI_BITMAP gdi_create_bitmap(rdpGdi* gdi, int width, int height, int bpp, uint8* data)
{
	uint8* bmpData;
	HGDI_BITMAP bitmap;

	bmpData = freerdp_image_convert(data, NULL, width, height, gdi->srcBpp, bpp, gdi->clrconv);
	bitmap = gdi_CreateBitmap(width, height, gdi->dstBpp, bmpData);

	return bitmap;
}

void gdi_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	gdiBitmap* gdi_bitmap;
	rdpGdi* gdi = context->gdi;

	gdi_bitmap = (gdiBitmap*) bitmap;
	gdi_bitmap->hdc = gdi_CreateCompatibleDC(gdi->hdc);

	if (bitmap->data == NULL)
		gdi_bitmap->bitmap = gdi_CreateCompatibleBitmap(gdi->hdc, bitmap->width, bitmap->height);
	else
		gdi_bitmap->bitmap = gdi_create_bitmap(gdi, bitmap->width, bitmap->height, gdi->dstBpp, bitmap->data);

	gdi_SelectObject(gdi_bitmap->hdc, (HGDIOBJECT) gdi_bitmap->bitmap);
	gdi_bitmap->org_bitmap = NULL;
}

void gdi_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	gdiBitmap* gdi_bitmap = (gdiBitmap*) bitmap;

	if (gdi_bitmap != NULL)
	{
		gdi_SelectObject(gdi_bitmap->hdc, (HGDIOBJECT) gdi_bitmap->org_bitmap);
		gdi_DeleteObject((HGDIOBJECT) gdi_bitmap->bitmap);
		gdi_DeleteDC(gdi_bitmap->hdc);
	}
}

void gdi_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap, int x, int y)
{
	gdiBitmap* gdi_bitmap = (gdiBitmap*) bitmap;

	gdi_BitBlt(context->gdi->primary->hdc, x, y,
			bitmap->width, bitmap->height,
			gdi_bitmap->hdc, 0, 0, GDI_SRCCOPY);
}

void gdi_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed)
{
	uint16 size;

	size = width * height * (bpp / 8);

	if (bitmap->data == NULL)
		bitmap->data = (uint8*) xmalloc(size);
	else
		bitmap->data = (uint8*) xrealloc(bitmap->data, size);

	if (compressed)
	{
		boolean status;

		status = bitmap_decompress(data, bitmap->data, width, height, length, bpp, bpp);

		if (status != True)
		{
			printf("Bitmap Decompression Failed\n");
		}
	}
	else
	{
		freerdp_image_flip(data, bitmap->data, width, height, bpp);

	}

	bitmap->width = width;
	bitmap->height = height;
	bitmap->compressed = False;
	bitmap->length = size;
	bitmap->bpp = bpp;
}

void gdi_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, boolean primary)
{
	rdpGdi* gdi = context->gdi;

	if (primary)
		gdi->drawing = gdi->primary;
	else
		gdi->drawing = (gdiBitmap*) bitmap;
}

void gdi_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap bitmap;

	memset(&bitmap, 0, sizeof(rdpBitmap));
	bitmap.size = sizeof(gdiBitmap);

	bitmap.New = gdi_Bitmap_New;
	bitmap.Free = gdi_Bitmap_Free;
	bitmap.Paint = gdi_Bitmap_Paint;
	bitmap.Decompress = gdi_Bitmap_Decompress;
	bitmap.SetSurface = gdi_Bitmap_SetSurface;

	graphics_register_bitmap(graphics, &bitmap);
}

