/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphical Objects
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#include <freerdp/log.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/shape.h>
#include <freerdp/gdi/region.h>
#include <freerdp/gdi/bitmap.h>

#include "clipping.h"
#include "drawing.h"
#include "brush.h"
#include "graphics.h"

#define TAG FREERDP_TAG("gdi")
/* Bitmap Class */

HGDI_BITMAP gdi_create_bitmap(rdpGdi* gdi, UINT32 nWidth, UINT32 nHeight,
                              UINT32 SrcFormat, BYTE* data)
{
	UINT32 nSrcStep;
	UINT32 nDstStep;
	BYTE* pSrcData;
	BYTE* pDstData;
	HGDI_BITMAP bitmap;

	if (!gdi)
		return NULL;

	nDstStep = nWidth * GetBytesPerPixel(gdi->dstFormat);
	pDstData = _aligned_malloc(nHeight * nDstStep, 16);

	if (!pDstData)
		return NULL;

	pSrcData = data;
	nSrcStep = nWidth * GetBytesPerPixel(SrcFormat);

	if (!freerdp_image_copy(pDstData, gdi->dstFormat, nDstStep, 0, 0,
	                        nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, 0, 0,
	                        &gdi->palette))
	{
		_aligned_free(pDstData);
		return NULL;
	}

	bitmap = gdi_CreateBitmap(nWidth, nHeight, gdi->dstFormat, pDstData);
	return bitmap;
}

static BOOL gdi_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	gdiBitmap* gdi_bitmap;
	rdpGdi* gdi = context->gdi;
	gdi_bitmap = (gdiBitmap*) bitmap;
	gdi_bitmap->hdc = gdi_CreateCompatibleDC(gdi->hdc);

	if (!gdi_bitmap->hdc)
		return FALSE;

	if (!bitmap->data)
		gdi_bitmap->bitmap = gdi_CreateCompatibleBitmap(
		                         gdi->hdc, bitmap->width,
		                         bitmap->height);
	else
	{
		UINT32 format = bitmap->format;
		gdi_bitmap->bitmap = gdi_create_bitmap(gdi, bitmap->width,
		                                       bitmap->height,
		                                       format, bitmap->data);
	}

	if (!gdi_bitmap->bitmap)
	{
		gdi_DeleteDC(gdi_bitmap->hdc);
		return FALSE;
	}

	gdi_bitmap->hdc->format = gdi_bitmap->bitmap->format;
	gdi_SelectObject(gdi_bitmap->hdc, (HGDIOBJECT) gdi_bitmap->bitmap);
	gdi_bitmap->org_bitmap = NULL;
	return TRUE;
}

static void gdi_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	gdiBitmap* gdi_bitmap = (gdiBitmap*) bitmap;

	if (gdi_bitmap)
	{
		gdi_SelectObject(gdi_bitmap->hdc, (HGDIOBJECT) gdi_bitmap->org_bitmap);
		gdi_DeleteObject((HGDIOBJECT) gdi_bitmap->bitmap);
		gdi_DeleteDC(gdi_bitmap->hdc);
	}
}

static BOOL gdi_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	gdiBitmap* gdi_bitmap = (gdiBitmap*) bitmap;
	UINT32 width = bitmap->right - bitmap->left + 1;
	UINT32 height = bitmap->bottom - bitmap->top + 1;
	return gdi_BitBlt(context->gdi->primary->hdc,
	                  bitmap->left, bitmap->top,
	                  width, height, gdi_bitmap->hdc,
	                  0, 0, GDI_SRCCOPY, &context->gdi->palette);
}

static BOOL gdi_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
                                  const BYTE* pSrcData, UINT32 width, UINT32 height,
                                  UINT32 bpp, UINT32 length, BOOL compressed,
                                  UINT32 codecId)
{
	UINT16 size;
	UINT32 SrcSize = length;
	UINT32 SrcFormat;
	UINT32 bytesPerPixel;
	UINT32 DstWidth = width;
	UINT32 DstHeight = height;
	rdpGdi* gdi = context->gdi;
	bytesPerPixel = (bpp + 7) / 8;
	size = width * height * GetBytesPerPixel(gdi->dstFormat);
	bitmap->data = (BYTE*) _aligned_malloc(size, 16);

	if (!bitmap->data)
		return FALSE;

	bitmap->compressed = FALSE;
	bitmap->length = size;
	bitmap->format = gdi->dstFormat;

	if (compressed)
	{
		if (bpp < 32)
		{
			if (!interleaved_decompress(context->codecs->interleaved,
			                            pSrcData, SrcSize,
			                            width, height,
			                            bpp,
			                            bitmap->data, bitmap->format,
			                            0, 0, 0, DstWidth, DstHeight,
			                            &gdi->palette))
				return FALSE;
		}
		else
		{
			if (!planar_decompress(context->codecs->planar, pSrcData, SrcSize,
			                       width, height,
			                       bitmap->data, bitmap->format, 0, 0, 0,
			                       width, height, TRUE))
				return FALSE;
		}
	}
	else
	{
		SrcFormat = gdi_get_pixel_format(bpp, TRUE);

		if (!freerdp_image_copy(bitmap->data, bitmap->format, 0, 0, 0,
		                        DstWidth, DstHeight, pSrcData, SrcFormat,
		                        0, 0, 0, &gdi->palette))
			return FALSE;
	}

	return TRUE;
}

static BOOL gdi_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap,
                                  BOOL primary)
{
	rdpGdi* gdi = context->gdi;

	if (primary)
		gdi->drawing = gdi->primary;
	else
		gdi->drawing = (gdiBitmap*) bitmap;

	return TRUE;
}

/* Glyph Class */
static BOOL gdi_Glyph_New(rdpContext* context, const rdpGlyph* glyph)
{
	BYTE* data;
	gdiGlyph* gdi_glyph;

	if (!context || !glyph)
		return FALSE;

	gdi_glyph = (gdiGlyph*) glyph;
	gdi_glyph->hdc = gdi_GetDC();

	if (!gdi_glyph->hdc)
		return FALSE;

	gdi_glyph->hdc->format = PIXEL_FORMAT_MONO;
	data = freerdp_glyph_convert(glyph->cx, glyph->cy, glyph->aj);

	if (!data)
	{
		gdi_DeleteDC(gdi_glyph->hdc);
		return FALSE;
	}

	gdi_glyph->bitmap = gdi_CreateBitmap(glyph->cx, glyph->cy, PIXEL_FORMAT_MONO,
	                                     data);

	if (!gdi_glyph->bitmap)
	{
		gdi_DeleteDC(gdi_glyph->hdc);
		_aligned_free(data);
		return FALSE;
	}

	gdi_SelectObject(gdi_glyph->hdc, (HGDIOBJECT) gdi_glyph->bitmap);
	gdi_glyph->org_bitmap = NULL;
	return TRUE;
}

static void gdi_Glyph_Free(rdpContext* context, rdpGlyph* glyph)
{
	gdiGlyph* gdi_glyph;
	gdi_glyph = (gdiGlyph*) glyph;

	if (gdi_glyph)
	{
		gdi_SelectObject(gdi_glyph->hdc, (HGDIOBJECT) gdi_glyph->org_bitmap);
		gdi_DeleteObject((HGDIOBJECT) gdi_glyph->bitmap);
		gdi_DeleteDC(gdi_glyph->hdc);
		free(glyph->aj);
		free(glyph);
	}
}

static BOOL gdi_Glyph_Draw(rdpContext* context, const rdpGlyph* glyph, UINT32 x,
                           UINT32 y)
{
	gdiGlyph* gdi_glyph;
	rdpGdi* gdi;

	if (!context || !glyph)
		return FALSE;

	gdi = context->gdi;
	gdi_glyph = (gdiGlyph*) glyph;
	return gdi_BitBlt(gdi->drawing->hdc, x, y, gdi_glyph->bitmap->width,
	                  gdi_glyph->bitmap->height, gdi_glyph->hdc, 0, 0,
	                  GDI_GLYPH_ORDER, &context->gdi->palette);
}

static BOOL gdi_Glyph_BeginDraw(rdpContext* context, UINT32 x, UINT32 y,
                                UINT32 width, UINT32 height, UINT32 bgcolor,
                                UINT32 fgcolor, BOOL fOpRedundant)
{
	rdpGdi* gdi;

	if (!context || !context->gdi)
		return FALSE;

	gdi = context->gdi;

	if (!gdi->drawing || !gdi->drawing->hdc)
		return FALSE;

	if (!gdi_decode_color(gdi, bgcolor, &bgcolor, NULL))
		return FALSE;

	if (!gdi_decode_color(gdi, fgcolor, &fgcolor, NULL))
		return FALSE;

	gdi->drawing->hdc->brush = gdi_CreateSolidBrush(bgcolor);

	if (!gdi->drawing->hdc->brush)
		return FALSE;

	gdi_SetTextColor(gdi->drawing->hdc, bgcolor);
	gdi_SetBkColor(gdi->drawing->hdc, fgcolor);
	return gdi_SetClipRgn(gdi->drawing->hdc, x, y, width, height);
}

static BOOL gdi_Glyph_EndDraw(rdpContext* context, UINT32 x, UINT32 y,
                              UINT32 width, UINT32 height, UINT32 bgcolor, UINT32 fgcolor)
{
	rdpGdi* gdi;

	if (!context || !context->gdi)
		return FALSE;

	gdi = context->gdi;

	if (!gdi->drawing || !gdi->drawing->hdc)
		return FALSE;

	gdi_DeleteObject((HGDIOBJECT)gdi->drawing->hdc->brush);
	gdi_SetNullClipRgn(gdi->drawing->hdc);
	return TRUE;
}

/* Graphics Module */
BOOL gdi_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap bitmap;
	rdpGlyph glyph;
	bitmap.size = sizeof(gdiBitmap);
	bitmap.New = gdi_Bitmap_New;
	bitmap.Free = gdi_Bitmap_Free;
	bitmap.Paint = gdi_Bitmap_Paint;
	bitmap.Decompress = gdi_Bitmap_Decompress;
	bitmap.SetSurface = gdi_Bitmap_SetSurface;
	graphics_register_bitmap(graphics, &bitmap);
	glyph.size = sizeof(gdiGlyph);
	glyph.New = gdi_Glyph_New;
	glyph.Free = gdi_Glyph_Free;
	glyph.Draw = gdi_Glyph_Draw;
	glyph.BeginDraw = gdi_Glyph_BeginDraw;
	glyph.EndDraw = gdi_Glyph_EndDraw;
	graphics_register_glyph(graphics, &glyph);
	return TRUE;
}
