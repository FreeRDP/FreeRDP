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
#include <freerdp/utils/memory.h>

#include "wfreerdp.h"
#include "wf_graphics.h"

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

wfBitmap* wf_glyph_new(wfInfo* wfi, GLYPH_DATA* glyph)
{
	wfBitmap* glyph_bmp;
	glyph_bmp = wf_image_new(wfi, glyph->cx, glyph->cy, 1, glyph->aj);
	return glyph_bmp;
}

void wf_glyph_free(wfBitmap* glyph)
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

#if 0
void wf_gdi_bitmap_update(rdpUpdate* update, BITMAP_UPDATE* bitmap)
{
	int i;
	rdpBitmap* bmp;
	wfBitmap* wf_bmp;

	wfInfo* wfi = ((wfContext*) update->context)->wfi;

	for (i = 0; i < bitmap->number; i++)
	{
		bmp = &bitmap->rectangles[i];

		wf_bmp = wf_image_new(wfi, bmp->width, bmp->height, wfi->srcBpp, bmp->dstData);

		BitBlt(wfi->primary->hdc,
				bmp->left, bmp->top, bmp->right - bmp->left + 1,
				bmp->bottom - bmp->top + 1, wf_bmp->hdc, 0, 0, GDI_SRCCOPY);

		wf_invalidate_region(wfi, bmp->left, bmp->top, bmp->right - bmp->left + 1, bmp->bottom - bmp->top + 1);

		wf_image_free(wf_bmp);
	}
}
#endif

void wf_gdi_palette_update(rdpUpdate* update, PALETTE_UPDATE* palette)
{

}

void wf_gdi_set_bounds(rdpUpdate* update, BOUNDS* bounds)
{
	HRGN hrgn;
	wfInfo* wfi = ((wfContext*) update->context)->wfi;

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
	wfInfo* wfi = ((wfContext*) update->context)->wfi;

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
	wfInfo* wfi = ((wfContext*) update->context)->wfi;

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
	wfInfo* wfi = ((wfContext*) update->context)->wfi;

	brush_color = freerdp_color_convert(opaque_rect->color, wfi->srcBpp, 24, wfi->clrconv);

	rect.left = opaque_rect->nLeftRect;
	rect.top = opaque_rect->nTopRect;
	rect.right = opaque_rect->nLeftRect + opaque_rect->nWidth;
	rect.bottom = opaque_rect->nTopRect + opaque_rect->nHeight;
	brush = CreateSolidBrush(brush_color);
	FillRect(wfi->drawing->hdc, &rect, brush);
	DeleteObject(brush);

	if (wfi->drawing == wfi->primary)
		wf_invalidate_region(wfi, rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1);
}

void wf_gdi_multi_opaque_rect(rdpUpdate* update, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	int i;
	RECT rect;
	HBRUSH brush;
	uint32 brush_color;
	DELTA_RECT* rectangle;
	wfInfo* wfi = ((wfContext*) update->context)->wfi;

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
	wfInfo* wfi = ((wfContext*) update->context)->wfi;

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

void wf_gdi_memblt(rdpUpdate* update, MEMBLT_ORDER* memblt)
{
	wfBitmap* bitmap;
	wfInfo* wfi = ((wfContext*) update->context)->wfi;

	bitmap = (wfBitmap*) memblt->bitmap;

	BitBlt(wfi->drawing->hdc, memblt->nLeftRect, memblt->nTopRect,
			memblt->nWidth, memblt->nHeight, bitmap->hdc,
			memblt->nXSrc, memblt->nYSrc, gdi_rop3_code(memblt->bRop));

	if (wfi->drawing == wfi->primary)
		wf_invalidate_region(wfi, memblt->nLeftRect, memblt->nTopRect, memblt->nWidth, memblt->nHeight);
}

void wf_gdi_mem3blt(rdpUpdate* update, MEM3BLT_ORDER* mem3blt)
{

}

void wf_gdi_fast_index(rdpUpdate* update, FAST_INDEX_ORDER* fast_index)
{

}

void wf_gdi_cache_color_table(rdpUpdate* update, CACHE_COLOR_TABLE_ORDER* cache_color_table)
{
	color_table_put(update->context->cache->color_table, cache_color_table->cacheIndex, (void*) cache_color_table->colorTable);
}

void wf_gdi_cache_glyph(rdpUpdate* update, CACHE_GLYPH_ORDER* cache_glyph)
{
	int i;
	wfBitmap* wf_bmp;
	GLYPH_DATA* glyph;
	wfInfo* wfi = ((wfContext*) update->context)->wfi;

	for (i = 0; i < cache_glyph->cGlyphs; i++)
	{
		glyph = cache_glyph->glyphData[i];
		wf_bmp = wf_glyph_new(wfi, glyph);
		glyph_put(update->context->cache->glyph, cache_glyph->cacheId, glyph->cacheIndex, glyph, (void*) wf_bmp);
	}
}

void wf_gdi_cache_glyph_v2(rdpUpdate* update, CACHE_GLYPH_V2_ORDER* cache_glyph_v2)
{

}

void wf_gdi_cache_brush(rdpUpdate* update, CACHE_BRUSH_ORDER* cache_brush)
{

}

void wf_gdi_surface_bits(rdpUpdate* update, SURFACE_BITS_COMMAND* surface_bits_command)
{

}

void wf_gdi_register_update_callbacks(rdpUpdate* update)
{
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
	update->MemBlt = wf_gdi_memblt;
	update->Mem3Blt = wf_gdi_mem3blt;
	update->SaveBitmap = NULL;
	update->GlyphIndex = NULL;
	update->FastIndex = wf_gdi_fast_index;
	update->FastGlyph = NULL;
	update->PolygonSC = NULL;
	update->PolygonCB = NULL;
	update->EllipseSC = NULL;
	update->EllipseCB = NULL;

	update->CacheColorTable = wf_gdi_cache_color_table;
	update->CacheGlyph = wf_gdi_cache_glyph;
	update->CacheGlyphV2 = wf_gdi_cache_glyph_v2;
	update->CacheBrush = wf_gdi_cache_brush;

	update->SurfaceBits = wf_gdi_surface_bits;
}
