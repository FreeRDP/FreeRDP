/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Windows GDI
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>

#include "wfreerdp.h"

const uint8 wf_rop2_table[] =
{
	R2_BLACK,       /* 0 */
	R2_NOTMERGEPEN, /* DPon */
	R2_MASKNOTPEN,  /* DPna */
	R2_NOTCOPYPEN,  /* Pn */
	R2_MASKPENNOT,  /* PDna */
	R2_NOT,         /* Dn */
	R2_XORPEN,      /* DPx */
	R2_NOTMASKPEN,  /* DPan */
	R2_MASKPEN,     /* DPa */
	R2_NOTXORPEN,   /* DPxn */
	R2_NOP,         /* D */
	R2_MERGENOTPEN, /* DPno */
	R2_COPYPEN,     /* P */
	R2_MERGEPENNOT, /* PDno */
	R2_MERGEPEN,    /* PDo */
	R2_WHITE,       /* 1 */
};

boolean wf_set_rop2(HDC hdc, int rop2)
{
	if ((rop2 < 0x01) || (rop2 > 0x10))
	{
		printf("Unsupported ROP2: %d\n", rop2);
		return False;
	}

	SetROP2(hdc, wf_rop2_table[rop2 - 1]);

	return True;
}

HBITMAP wf_create_dib(wfInfo* wfi, int width, int height, int bpp, uint8* data)
{
	HDC hdc;
	int negHeight;
	HBITMAP bitmap;
	BITMAPINFO bmi;
	uint8* cdata = NULL;

	/**
	 * See: http://msdn.microsoft.com/en-us/library/dd183376
	 * if biHeight is positive, the bitmap is bottom-up
	 * if biHeight is negative, the bitmap is top-down
	 * Since we get top-down bitmaps, let's keep it that way
	 */

	negHeight = (height < 0) ? height : height * (-1);

	hdc = GetDC(NULL);
	bmi.bmiHeader.biSize = sizeof(BITMAPINFO);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = negHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 24;
	bmi.bmiHeader.biCompression = BI_RGB;
	bitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**) &cdata, NULL, 0);

	if (data != NULL)
		freerdp_image_convert(data, cdata, width, height, bpp, 24, wfi->clrconv);

	ReleaseDC(NULL, hdc);
	GdiFlush();

	return bitmap;
}

WF_IMAGE* wf_image_new(wfInfo* wfi, int width, int height, int bpp, uint8* data)
{
	HDC hdc;
	WF_IMAGE* image;

	hdc = GetDC(NULL);
	image = (WF_IMAGE*) malloc(sizeof(WF_IMAGE));
	image->hdc = CreateCompatibleDC(hdc);

	if (data == NULL)
		image->bitmap = CreateCompatibleBitmap(hdc, width, height);
	else
		image->bitmap = wf_create_dib(wfi, width, height, bpp, data);

	image->org_bitmap = (HBITMAP) SelectObject(image->hdc, image->bitmap);
	ReleaseDC(NULL, hdc);
	
	return image;
}

void wf_image_free(WF_IMAGE* image)
{
	if (image != 0)
	{
		SelectObject(image->hdc, image->org_bitmap);
		DeleteObject(image->bitmap);
		DeleteDC(image->hdc);
		free(image);
	}
}

WF_IMAGE* wf_glyph_new(wfInfo* wfi, GLYPH_DATA* glyph)
{
	WF_IMAGE* glyph_bmp;
	glyph_bmp = wf_image_new(wfi, glyph->cx, glyph->cy, 1, glyph->aj);
	return glyph_bmp;
}

void wf_glyph_free(WF_IMAGE* glyph)
{
	wf_image_free(glyph);
}

void wf_invalidate_region(wfInfo* wfi, int x, int y, int width, int height)
{
	RECT update_rect;
	update_rect.left = x;
	update_rect.top = y;
	update_rect.right = x + width;
	update_rect.bottom = y + height;
	InvalidateRect(wfi->hwnd, &update_rect, FALSE);

	gdi_InvalidateRegion(wfi->hdc, x, y, width, height);
}

void wf_toggle_fullscreen(wfInfo* wfi)
{
	ShowWindow(wfi->hwnd, SW_HIDE);
	wfi->fullscreen = !wfi->fullscreen;
	SetForegroundWindow(wfi->hwnd);
}

void wf_gdi_bitmap_update(rdpUpdate* update, BITMAP_UPDATE* bitmap)
{
	int i;
	BITMAP_DATA* bmp;
	WF_IMAGE* wf_bmp;

	wfInfo* wfi = GET_WFI(update);

	for (i = 0; i < bitmap->number; i++)
	{
		bmp = &bitmap->bitmaps[i];

		wf_bmp = wf_image_new(wfi, bmp->width, bmp->height, wfi->dstBpp, bmp->dstData);

		BitBlt(wfi->primary->hdc,
				bmp->left, bmp->top, bmp->right - bmp->left + 1,
				bmp->bottom - bmp->top + 1, wf_bmp->hdc, 0, 0, GDI_SRCCOPY);

		wf_invalidate_region(wfi, bmp->left, bmp->top, bmp->right - bmp->left + 1, bmp->bottom - bmp->top + 1);

		wf_image_free(wf_bmp);
	}
}

void wf_gdi_palette_update(rdpUpdate* update, PALETTE_UPDATE* palette)
{

}

void wf_gdi_set_bounds(rdpUpdate* update, BOUNDS* bounds)
{
	HRGN hrgn;
	wfInfo* wfi = GET_WFI(update);

	if (bounds != NULL)
	{
		hrgn = CreateRectRgn(bounds->left, bounds->top, bounds->right, bounds->bottom);
		SelectClipRgn(wfi->drawing->hdc, hrgn);
		DeleteObject(hrgn);
	}
	else
	{
		SelectClipRgn(wfi->drawing->hdc, NULL);
	}
}

void wf_gdi_dstblt(rdpUpdate* update, DSTBLT_ORDER* dstblt)
{
	wfInfo* wfi = GET_WFI(update);

	BitBlt(wfi->drawing->hdc, dstblt->nLeftRect, dstblt->nTopRect,
			dstblt->nWidth, dstblt->nHeight, NULL, 0, 0, gdi_rop3_code(dstblt->bRop));

	wf_invalidate_region(wfi, dstblt->nLeftRect, dstblt->nTopRect,
			dstblt->nWidth, dstblt->nHeight);
}

void wf_gdi_patblt(rdpUpdate* update, PATBLT_ORDER* patblt)
{

}

void wf_gdi_scrblt(rdpUpdate* update, SCRBLT_ORDER* scrblt)
{
	wfInfo* wfi = GET_WFI(update);

	BitBlt(wfi->drawing->hdc, scrblt->nLeftRect, scrblt->nTopRect,
			scrblt->nWidth, scrblt->nHeight, wfi->primary->hdc,
			scrblt->nXSrc, scrblt->nYSrc, gdi_rop3_code(scrblt->bRop));

	wf_invalidate_region(wfi, scrblt->nLeftRect, scrblt->nTopRect,
			scrblt->nWidth, scrblt->nHeight);
}

void wf_gdi_opaque_rect(rdpUpdate* update, OPAQUE_RECT_ORDER* opaque_rect)
{
	RECT rect;
	HBRUSH brush;
	uint32 brush_color;
	wfInfo* wfi = GET_WFI(update);

	brush_color = freerdp_color_convert(opaque_rect->color, wfi->dstBpp, 24, wfi->clrconv);

	rect.left = opaque_rect->nLeftRect;
	rect.top = opaque_rect->nTopRect;
	rect.right = opaque_rect->nLeftRect + opaque_rect->nWidth;
	rect.bottom = opaque_rect->nTopRect + opaque_rect->nHeight;
	brush = CreateSolidBrush(brush_color);
	FillRect(wfi->drawing->hdc, &rect, brush);
	DeleteObject(brush);

	if (wfi->drawing == wfi->primary)
		wf_invalidate_region(wfi, rect.left, rect.top, rect.right, rect.bottom);
}

void wf_gdi_multi_opaque_rect(rdpUpdate* update, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	int i;
	RECT rect;
	HBRUSH brush;
	uint32 brush_color;
	DELTA_RECT* rectangle;
	wfInfo* wfi = GET_WFI(update);

	for (i = 1; i < multi_opaque_rect->numRectangles + 1; i++)
	{
		rectangle = &multi_opaque_rect->rectangles[i];

		brush_color = freerdp_color_convert(multi_opaque_rect->color, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);

		rect.left = rectangle->left;
		rect.top = rectangle->top;
		rect.right = rectangle->left + rectangle->width;
		rect.bottom = rectangle->top + rectangle->height;
		brush = CreateSolidBrush(brush_color);

		brush = CreateSolidBrush(brush_color);
		FillRect(wfi->drawing->hdc, &rect, brush);

		DeleteObject(brush);
	}
}

void wf_gdi_line_to(rdpUpdate* update, LINE_TO_ORDER* line_to)
{
	HPEN pen;
	HPEN org_pen;
	uint32 pen_color;
	wfInfo* wfi = GET_WFI(update);

	pen_color = freerdp_color_convert(line_to->penColor, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);

	pen = CreatePen(line_to->penStyle, line_to->penWidth, pen_color);

	wf_set_rop2(wfi->drawing->hdc, line_to->bRop2);
	org_pen = (HPEN) SelectObject(wfi->drawing->hdc, pen);
	
	MoveToEx(wfi->drawing->hdc, line_to->nXStart, line_to->nYStart, NULL);
	LineTo(wfi->drawing->hdc, line_to->nXEnd, line_to->nYEnd);
	SelectObject(wfi->drawing->hdc, org_pen);

	DeleteObject(pen);
}

void wf_gdi_polyline(rdpUpdate* update, POLYLINE_ORDER* polyline)
{

}

void wf_gdi_fast_index(rdpUpdate* update, FAST_INDEX_ORDER* fast_index)
{

}

void wf_gdi_create_offscreen_bitmap(rdpUpdate* update, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	WF_IMAGE* wf_bmp;
	wfInfo* wfi = GET_WFI(update);

	wf_bmp = wf_image_new(wfi, create_offscreen_bitmap->cx, create_offscreen_bitmap->cy, wfi->dstBpp, NULL);
	offscreen_put(wfi->cache->offscreen, create_offscreen_bitmap->id, (void*) wf_bmp);
}

void wf_gdi_switch_surface(rdpUpdate* update, SWITCH_SURFACE_ORDER* switch_surface)
{
	WF_IMAGE* wf_bmp;
	wfInfo* wfi = GET_WFI(update);

	if (switch_surface->bitmapId == SCREEN_BITMAP_SURFACE)
	{
		wfi->drawing = (WF_IMAGE*) wfi->primary;
	}
	else
	{
		wf_bmp = (WF_IMAGE*) offscreen_get(wfi->cache->offscreen, switch_surface->bitmapId);
		wfi->drawing = wf_bmp;
	}
}

void wf_gdi_cache_bitmap_v2(rdpUpdate* update, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2)
{
	wfInfo* wfi = GET_WFI(update);

	bitmap_v2_put(wfi->cache->bitmap_v2, cache_bitmap_v2->cacheId,
			cache_bitmap_v2->cacheIndex, cache_bitmap_v2->bitmapDataStream);
}

void wf_gdi_cache_color_table(rdpUpdate* update, CACHE_COLOR_TABLE_ORDER* cache_color_table)
{
	wfInfo* wfi = GET_WFI(update);
	color_table_put(wfi->cache->color_table, cache_color_table->cacheIndex, (void*) cache_color_table->colorTable);
}

void wf_gdi_cache_glyph(rdpUpdate* update, CACHE_GLYPH_ORDER* cache_glyph)
{
	int i;
	WF_IMAGE* wf_bmp;
	GLYPH_DATA* glyph;
	wfInfo* wfi = GET_WFI(update);

	for (i = 0; i < cache_glyph->cGlyphs; i++)
	{
		glyph = cache_glyph->glyphData[i];
		wf_bmp = wf_glyph_new(wfi, glyph);
		glyph_put(wfi->cache->glyph, cache_glyph->cacheId, glyph->cacheIndex, glyph, (void*) wf_bmp);
	}
}

void wf_gdi_cache_glyph_v2(rdpUpdate* update, CACHE_GLYPH_V2_ORDER* cache_glyph_v2)
{

}

void wf_gdi_cache_brush(rdpUpdate* update, CACHE_BRUSH_ORDER* cache_brush)
{
	//wfInfo* wfi = GET_WFI(update);
	//brush_put(wfi->cache->brush, cache_brush->index, cache_brush->data, cache_brush->bpp);
}

void wf_gdi_surface_bits(rdpUpdate* update, SURFACE_BITS_COMMAND* surface_bits_command)
{

}

void wf_gdi_bitmap_decompress(rdpUpdate* update, BITMAP_DATA* bitmap_data)
{
	uint16 dstSize;

	dstSize = bitmap_data->width * bitmap_data->height * (bitmap_data->bpp / 8);

	if (bitmap_data->dstData == NULL)
		bitmap_data->dstData = (uint8*) xmalloc(dstSize);
	else
		bitmap_data->dstData = (uint8*) xrealloc(bitmap_data->dstData, dstSize);

	if (bitmap_data->compressed)
	{
		bitmap_decompress(bitmap_data->srcData, bitmap_data->dstData,
				bitmap_data->width, bitmap_data->height, bitmap_data->length,
				bitmap_data->bpp, bitmap_data->bpp);
	}
	else
	{
		freerdp_image_invert(bitmap_data->srcData, bitmap_data->dstData,
				bitmap_data->width, bitmap_data->height, bitmap_data->bpp);
	}

	bitmap_data->compressed = False;
}

void wf_gdi_register_update_callbacks(rdpUpdate* update)
{
	update->Bitmap = wf_gdi_bitmap_update;
	update->Palette = wf_gdi_palette_update;
	update->SetBounds = wf_gdi_set_bounds;
	update->DstBlt = wf_gdi_dstblt;
	update->PatBlt = wf_gdi_patblt;
	update->ScrBlt = wf_gdi_scrblt;
	update->OpaqueRect = wf_gdi_opaque_rect;
	update->DrawNineGrid = NULL;
	update->MultiDstBlt = NULL;
	update->MultiPatBlt = NULL;
	update->MultiScrBlt = NULL;
	update->MultiOpaqueRect = wf_gdi_multi_opaque_rect;
	update->MultiDrawNineGrid = NULL;
	update->LineTo = wf_gdi_line_to;
	update->Polyline = NULL;
	update->MemBlt = NULL;
	update->Mem3Blt = NULL;
	update->SaveBitmap = NULL;
	update->GlyphIndex = NULL;
	update->FastIndex = wf_gdi_fast_index;
	update->FastGlyph = NULL;
	update->PolygonSC = NULL;
	update->PolygonCB = NULL;
	update->EllipseSC = NULL;
	update->EllipseCB = NULL;
	update->CreateOffscreenBitmap = wf_gdi_create_offscreen_bitmap;
	update->SwitchSurface = wf_gdi_switch_surface;

	update->CacheBitmapV2 = wf_gdi_cache_bitmap_v2;
	update->CacheColorTable = wf_gdi_cache_color_table;
	update->CacheGlyph = wf_gdi_cache_glyph;
	update->CacheGlyphV2 = wf_gdi_cache_glyph_v2;
	update->CacheBrush = wf_gdi_cache_brush;

	update->SurfaceBits = wf_gdi_surface_bits;

	update->BitmapDecompress = wf_gdi_bitmap_decompress;
}
