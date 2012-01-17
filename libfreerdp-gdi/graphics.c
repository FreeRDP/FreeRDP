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
#include <freerdp/gdi/brush.h>
#include <freerdp/gdi/shape.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>
#include <freerdp/gdi/drawing.h>
#include <freerdp/gdi/clipping.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/utils/memory.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/cache/glyph.h>

#include "graphics.h"

/* Bitmap Class */

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

void gdi_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	int width, height;
	gdiBitmap* gdi_bitmap = (gdiBitmap*) bitmap;

	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;

	gdi_BitBlt(context->gdi->primary->hdc, bitmap->left, bitmap->top,
			width, height, gdi_bitmap->hdc, 0, 0, GDI_SRCCOPY);
}

void gdi_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed)
{
	uint16 size;

	size = width * height * (bpp + 7) / 8;

	if (bitmap->data == NULL)
		bitmap->data = (uint8*) xmalloc(size);
	else
		bitmap->data = (uint8*) xrealloc(bitmap->data, size);

	if (compressed)
	{
		boolean status;

		status = bitmap_decompress(data, bitmap->data, width, height, length, bpp, bpp);

		if (status != true)
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
	bitmap->compressed = false;
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

/* Glyph Class */

void gdi_Glyph_New(rdpContext* context, rdpGlyph* glyph)
{
	uint8* data;
	gdiGlyph* gdi_glyph;

	gdi_glyph = (gdiGlyph*) glyph;

	gdi_glyph->hdc = gdi_GetDC();
	gdi_glyph->hdc->bytesPerPixel = 1;
	gdi_glyph->hdc->bitsPerPixel = 1;

	data = freerdp_glyph_convert(glyph->cx, glyph->cy, glyph->aj);
	gdi_glyph->bitmap = gdi_CreateBitmap(glyph->cx, glyph->cy, 1, data);
	gdi_glyph->bitmap->bytesPerPixel = 1;
	gdi_glyph->bitmap->bitsPerPixel = 1;

	gdi_SelectObject(gdi_glyph->hdc, (HGDIOBJECT) gdi_glyph->bitmap);
	gdi_glyph->org_bitmap = NULL;
}

void gdi_Glyph_Free(rdpContext* context, rdpGlyph* glyph)
{
	gdiGlyph* gdi_glyph;

	gdi_glyph = (gdiGlyph*) glyph;

	if (gdi_glyph != 0)
	{
		gdi_SelectObject(gdi_glyph->hdc, (HGDIOBJECT) gdi_glyph->org_bitmap);
		gdi_DeleteObject((HGDIOBJECT) gdi_glyph->bitmap);
		gdi_DeleteDC(gdi_glyph->hdc);
	}
}

void gdi_Glyph_Draw(rdpContext* context, rdpGlyph* glyph, int x, int y)
{
	gdiGlyph* gdi_glyph;
	rdpGdi* gdi = context->gdi;

	gdi_glyph = (gdiGlyph*) glyph;

	gdi_BitBlt(gdi->drawing->hdc, x, y, gdi_glyph->bitmap->width,
			gdi_glyph->bitmap->height, gdi_glyph->hdc, 0, 0, GDI_DSPDxax);
}

void gdi_Glyph_BeginDraw(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor)
{
	GDI_RECT rect;
	HGDI_BRUSH brush;
	rdpGdi* gdi = context->gdi;

	bgcolor = freerdp_color_convert_var_bgr(bgcolor, gdi->srcBpp, 32, gdi->clrconv);
	fgcolor = freerdp_color_convert_var_bgr(fgcolor, gdi->srcBpp, 32, gdi->clrconv);

	gdi_CRgnToRect(x, y, width, height, &rect);

	brush = gdi_CreateSolidBrush(fgcolor);

	gdi_FillRect(gdi->drawing->hdc, &rect, brush);

	gdi->textColor = gdi_SetTextColor(gdi->drawing->hdc, bgcolor);
}

void gdi_Glyph_EndDraw(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor)
{
	rdpGdi* gdi = context->gdi;

	bgcolor = freerdp_color_convert_var_bgr(bgcolor, gdi->srcBpp, 32, gdi->clrconv);
	gdi->textColor = gdi_SetTextColor(gdi->drawing->hdc, bgcolor);
}

/* Graphics Module */

void gdi_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap* bitmap;
	rdpGlyph* glyph;

	bitmap = xnew(rdpBitmap);
	bitmap->size = sizeof(gdiBitmap);

	bitmap->New = gdi_Bitmap_New;
	bitmap->Free = gdi_Bitmap_Free;
	bitmap->Paint = gdi_Bitmap_Paint;
	bitmap->Decompress = gdi_Bitmap_Decompress;
	bitmap->SetSurface = gdi_Bitmap_SetSurface;

	graphics_register_bitmap(graphics, bitmap);
	xfree(bitmap);

	glyph = xnew(rdpGlyph);
	glyph->size = sizeof(gdiGlyph);

	glyph->New = gdi_Glyph_New;
	glyph->Free = gdi_Glyph_Free;
	glyph->Draw = gdi_Glyph_Draw;
	glyph->BeginDraw = gdi_Glyph_BeginDraw;
	glyph->EndDraw = gdi_Glyph_EndDraw;

	graphics_register_glyph(graphics, glyph);
	xfree(glyph);
}

