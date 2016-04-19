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
	nDstStep = nWidth * GetBytesPerPixel(gdi->dstFormat);
	pDstData = _aligned_malloc(nHeight * nDstStep, 16);

	if (!pDstData)
		return NULL;

	pSrcData = data;
	nSrcStep = nWidth * GetBytesPerPixel(SrcFormat);
	freerdp_image_copy(pDstData, gdi->dstFormat, nDstStep, 0, 0,
					   nWidth, nHeight, pSrcData, SrcFormat, nSrcStep, 0, 0, gdi->palette);
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
	int width, height;
	gdiBitmap* gdi_bitmap = (gdiBitmap*) bitmap;
	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;
	return gdi_BitBlt(context->gdi->primary->hdc, bitmap->left, bitmap->top,
					  width, height, gdi_bitmap->hdc, 0, 0, GDI_SRCCOPY);
}

static BOOL gdi_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
								  const BYTE* data, UINT32 width, UINT32 height,
								  UINT32 bpp, UINT32 length, BOOL compressed,
								  UINT32 codecId)
{
	int status;
	UINT16 size;
	const BYTE* pSrcData;
	BYTE* pDstData;
	UINT32 SrcSize;
	UINT32 SrcFormat;
	UINT32 bytesPerPixel;
	rdpGdi* gdi = context->gdi;
	bytesPerPixel = (bpp + 7) / 8;
	size = width * height * 4;
	bitmap->data = (BYTE*) _aligned_malloc(size, 16);
	pSrcData = data;
	SrcSize = (UINT32) length;
	pDstData = bitmap->data;

	if (compressed)
	{
		if (bpp < 32)
		{
			status = interleaved_decompress(gdi->codecs->interleaved,
											pSrcData, SrcSize,
											bpp,
											pDstData, gdi->dstFormat,
											0, 0, 0, width, height,
											gdi->palette);
		}
		else
		{
			status = planar_decompress(gdi->codecs->planar, pSrcData, SrcSize,
									   pDstData, gdi->dstFormat, 0, 0, 0,
									   width, height, TRUE);
		}

		if (status < 0)
		{
			WLog_ERR(TAG, "Bitmap Decompression Failed");
			return FALSE;
		}
	}
	else
	{
		SrcFormat = gdi_get_pixel_format(bpp, FALSE);
		status = freerdp_image_copy(pDstData, gdi->dstFormat, 0, 0, 0,
									width, height, pSrcData, SrcFormat,
									0, 0, 0, gdi->palette);
	}

	bitmap->compressed = FALSE;
	bitmap->length = size;
	bitmap->format = gdi->dstFormat;
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
static BOOL gdi_Glyph_New(rdpContext* context, rdpGlyph* glyph)
{
	BYTE* data;
	gdiGlyph* gdi_glyph;
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

	gdi_glyph->bitmap->format = PIXEL_FORMAT_MONO;
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
	}
}

static BOOL gdi_Glyph_Draw(rdpContext* context, rdpGlyph* glyph, UINT32 x,
						   UINT32 y)
{
	gdiGlyph* gdi_glyph;
	rdpGdi* gdi = context->gdi;
	gdi_glyph = (gdiGlyph*) glyph;
	return gdi_BitBlt(gdi->drawing->hdc, x, y, gdi_glyph->bitmap->width,
					  gdi_glyph->bitmap->height, gdi_glyph->hdc, 0, 0, GDI_DSPDxax);
}

static BOOL gdi_Glyph_BeginDraw(rdpContext* context, UINT32 x, UINT32 y,
								UINT32 width, UINT32 height, UINT32 bgcolor,
								UINT32 fgcolor, BOOL fOpRedundant)
{
	GDI_RECT rect;
	HGDI_BRUSH brush;
	rdpGdi* gdi = context->gdi;
	BOOL ret = FALSE;
	UINT32 SrcFormat = gdi_get_pixel_format(context->settings->ColorDepth, FALSE);
	/* TODO: handle fOpRedundant! See xf_Glyph_BeginDraw() */
	bgcolor = ConvertColor(bgcolor, SrcFormat, gdi->dstFormat, gdi->palette);
	fgcolor = ConvertColor(fgcolor, SrcFormat, gdi->dstFormat, gdi->palette);

	if (!(brush = gdi_CreateSolidBrush(fgcolor)))
		goto out;

	gdi_CRgnToRect(x, y, width, height, &rect);
	ret = gdi_FillRect(gdi->drawing->hdc, &rect, brush);
	gdi_DeleteObject((HGDIOBJECT) brush);
out:
	gdi->textColor = gdi_SetTextColor(gdi->drawing->hdc, bgcolor);
	return ret;
}

static BOOL gdi_Glyph_EndDraw(rdpContext* context, UINT32 x, UINT32 y,
							  UINT32 width, UINT32 height, UINT32 bgcolor, UINT32 fgcolor)
{
	rdpGdi* gdi = context->gdi;
	UINT32 SrcFormat = gdi_get_pixel_format(context->settings->ColorDepth, FALSE);
	bgcolor = ConvertColor(bgcolor, SrcFormat,
						   gdi->dstFormat, gdi->palette);
	gdi->textColor = gdi_SetTextColor(gdi->drawing->hdc, bgcolor);
	return TRUE;
}

/* Graphics Module */
BOOL gdi_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap* bitmap;
	rdpGlyph* glyph;
	bitmap = (rdpBitmap*) calloc(1, sizeof(rdpBitmap));

	if (!bitmap)
		return FALSE;

	bitmap->size = sizeof(gdiBitmap);
	bitmap->New = gdi_Bitmap_New;
	bitmap->Free = gdi_Bitmap_Free;
	bitmap->Paint = gdi_Bitmap_Paint;
	bitmap->Decompress = gdi_Bitmap_Decompress;
	bitmap->SetSurface = gdi_Bitmap_SetSurface;
	graphics_register_bitmap(graphics, bitmap);
	free(bitmap);
	glyph = (rdpGlyph*) calloc(1, sizeof(rdpGlyph));

	if (!glyph)
		return FALSE;

	glyph->size = sizeof(gdiGlyph);
	glyph->New = gdi_Glyph_New;
	glyph->Free = gdi_Glyph_Free;
	glyph->Draw = gdi_Glyph_Draw;
	glyph->BeginDraw = gdi_Glyph_BeginDraw;
	glyph->EndDraw = gdi_Glyph_EndDraw;
	graphics_register_glyph(graphics, glyph);
	free(glyph);
	return TRUE;
}
