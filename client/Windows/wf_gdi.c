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
#include <freerdp/constants.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/utils/memory.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>

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
		return false;
	}

	SetROP2(hdc, wf_rop2_table[rop2 - 1]);

	return true;
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

uint8* wf_glyph_convert(wfInfo* wfi, int width, int height, uint8* data)
{
	int indexx;
	int indexy;
	uint8* src;
	uint8* dst;
	uint8* cdata;
	int src_bytes_per_row;
	int dst_bytes_per_row;

	src_bytes_per_row = (width + 7) / 8;
	dst_bytes_per_row = src_bytes_per_row + (src_bytes_per_row % 2);
	cdata = (uint8 *) malloc(dst_bytes_per_row * height);

	src = data;
	for (indexy = 0; indexy < height; indexy++)
	{
		dst = cdata + indexy * dst_bytes_per_row;

		for (indexx = 0; indexx < dst_bytes_per_row; indexx++)
		{
			if (indexx < src_bytes_per_row)
				*dst++ = *src++;
			else
				*dst++ = 0;
		}
	}

	return cdata;
}

HBRUSH wf_create_brush(wfInfo * wfi, rdpBrush* brush, uint32 color, int bpp)
{
	int i;
	HBRUSH br;
	LOGBRUSH lbr;
	uint8* cdata;
	uint8 ipattern[8];
	HBITMAP pattern = NULL;

	lbr.lbStyle = brush->style;

	if (lbr.lbStyle == BS_DIBPATTERN || lbr.lbStyle == BS_DIBPATTERN8X8 || lbr.lbStyle == BS_DIBPATTERNPT)
		lbr.lbColor = DIB_RGB_COLORS;
	else
		lbr.lbColor = color;

	if (lbr.lbStyle == BS_PATTERN || lbr.lbStyle == BS_PATTERN8X8)
	{
		if (brush->bpp > 1)
		{
			pattern = wf_create_dib(wfi, 8, 8, bpp, brush->data, NULL);
			lbr.lbHatch = (ULONG_PTR) pattern;
		}
		else
		{
			for (i = 0; i != 8; i++)
				ipattern[7 - i] = brush->data[i];
	
			cdata = wf_glyph_convert(wfi, 8, 8, ipattern);
			pattern = CreateBitmap(8, 8, 1, 1, cdata);
			lbr.lbHatch = (ULONG_PTR) pattern;
			free(cdata);
		}
	}
	else if (lbr.lbStyle == BS_HATCHED)
	{
		lbr.lbHatch = brush->hatch;
	}
	else
	{
		lbr.lbHatch = 0;
	}

	br = CreateBrushIndirect(&lbr);
	SetBrushOrgEx(wfi->drawing->hdc, brush->x, brush->y, NULL);

	if (pattern != NULL)
		DeleteObject(pattern);

	return br;
}

void wf_invalidate_region(wfInfo* wfi, int x, int y, int width, int height)
{
	wfi->update_rect.left = x;
	wfi->update_rect.top = y;
	wfi->update_rect.right = x + width;
	wfi->update_rect.bottom = y + height;
	InvalidateRect(wfi->hwnd, &(wfi->update_rect), FALSE);
	gdi_InvalidateRegion(wfi->hdc, x, y, width, height);
}

void wf_toggle_fullscreen(wfInfo* wfi)
{
	ShowWindow(wfi->hwnd, SW_HIDE);
	wfi->fullscreen = !wfi->fullscreen;
	SetForegroundWindow(wfi->hwnd);
}

void wf_gdi_palette_update(rdpContext* context, PALETTE_UPDATE* palette)
{

}

void wf_set_null_clip_rgn(wfInfo* wfi)
{
	SelectClipRgn(wfi->drawing->hdc, NULL);
}

void wf_set_clip_rgn(wfInfo* wfi, int x, int y, int width, int height)
{
	HRGN clip;
	clip = CreateRectRgn(x, y, x + width, y + height);
	SelectClipRgn(wfi->drawing->hdc, clip);
	DeleteObject(clip);
}

void wf_gdi_set_bounds(rdpContext* context, rdpBounds* bounds)
{
	HRGN hrgn;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	if (bounds != NULL)
	{
		hrgn = CreateRectRgn(bounds->left, bounds->top, bounds->right + 1, bounds->bottom + 1);
		SelectClipRgn(wfi->drawing->hdc, hrgn);
		DeleteObject(hrgn);
	}
	else
	{
		SelectClipRgn(wfi->drawing->hdc, NULL);
	}
}

void wf_gdi_dstblt(rdpContext* context, DSTBLT_ORDER* dstblt)
{
	wfInfo* wfi = ((wfContext*) context)->wfi;

	BitBlt(wfi->drawing->hdc, dstblt->nLeftRect, dstblt->nTopRect,
			dstblt->nWidth, dstblt->nHeight, NULL, 0, 0, gdi_rop3_code(dstblt->bRop));

	wf_invalidate_region(wfi, dstblt->nLeftRect, dstblt->nTopRect,
			dstblt->nWidth, dstblt->nHeight);
}

void wf_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	HBRUSH brush;
	HBRUSH org_brush;
	int org_bkmode;
	uint32 fgcolor;
	uint32 bgcolor;
	COLORREF org_bkcolor;
	COLORREF org_textcolor;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	fgcolor = freerdp_color_convert_bgr(patblt->foreColor, wfi->srcBpp, 32, wfi->clrconv);
	bgcolor = freerdp_color_convert_bgr(patblt->backColor, wfi->srcBpp, 32, wfi->clrconv);

	brush = wf_create_brush(wfi, &patblt->brush, fgcolor, wfi->srcBpp);
	org_bkmode = SetBkMode(wfi->drawing->hdc, OPAQUE);
	org_bkcolor = SetBkColor(wfi->drawing->hdc, bgcolor);
	org_textcolor = SetTextColor(wfi->drawing->hdc, fgcolor);
	org_brush = (HBRUSH)SelectObject(wfi->drawing->hdc, brush);

	PatBlt(wfi->drawing->hdc, patblt->nLeftRect, patblt->nTopRect,
		patblt->nWidth, patblt->nHeight, gdi_rop3_code(patblt->bRop));

	SelectObject(wfi->drawing->hdc, org_brush);
	DeleteObject(brush);

	SetBkMode(wfi->drawing->hdc, org_bkmode);
	SetBkColor(wfi->drawing->hdc, org_bkcolor);
	SetTextColor(wfi->drawing->hdc, org_textcolor);

	if (wfi->drawing == wfi->primary)
		wf_invalidate_region(wfi, patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);
}

void wf_gdi_scrblt(rdpContext* context, SCRBLT_ORDER* scrblt)
{
	wfInfo* wfi = ((wfContext*) context)->wfi;

	BitBlt(wfi->drawing->hdc, scrblt->nLeftRect, scrblt->nTopRect,
			scrblt->nWidth, scrblt->nHeight, wfi->primary->hdc,
			scrblt->nXSrc, scrblt->nYSrc, gdi_rop3_code(scrblt->bRop));

	wf_invalidate_region(wfi, scrblt->nLeftRect, scrblt->nTopRect,
			scrblt->nWidth, scrblt->nHeight);
}

void wf_gdi_opaque_rect(rdpContext* context, OPAQUE_RECT_ORDER* opaque_rect)
{
	RECT rect;
	HBRUSH brush;
	uint32 brush_color;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	brush_color = freerdp_color_convert_var_bgr(opaque_rect->color, wfi->srcBpp, 24, wfi->clrconv);

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

void wf_gdi_multi_opaque_rect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	int i;
	RECT rect;
	HBRUSH brush;
	uint32 brush_color;
	DELTA_RECT* rectangle;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	for (i = 1; i < (int) multi_opaque_rect->numRectangles + 1; i++)
	{
		rectangle = &multi_opaque_rect->rectangles[i];

		brush_color = freerdp_color_convert_var_bgr(multi_opaque_rect->color, wfi->srcBpp, 32, wfi->clrconv);

		rect.left = rectangle->left;
		rect.top = rectangle->top;
		rect.right = rectangle->left + rectangle->width;
		rect.bottom = rectangle->top + rectangle->height;
		brush = CreateSolidBrush(brush_color);

		brush = CreateSolidBrush(brush_color);
		FillRect(wfi->drawing->hdc, &rect, brush);

		if (wfi->drawing == wfi->primary)
			wf_invalidate_region(wfi, rect.left, rect.top, rect.right - rect.left + 1, rect.bottom - rect.top + 1);

		DeleteObject(brush);
	}
}

void wf_gdi_line_to(rdpContext* context, LINE_TO_ORDER* line_to)
{
	HPEN pen;
	HPEN org_pen;
	int x, y, w, h;
	uint32 pen_color;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	pen_color = freerdp_color_convert_bgr(line_to->penColor, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);

	pen = CreatePen(line_to->penStyle, line_to->penWidth, pen_color);

	wf_set_rop2(wfi->drawing->hdc, line_to->bRop2);
	org_pen = (HPEN) SelectObject(wfi->drawing->hdc, pen);
	
	MoveToEx(wfi->drawing->hdc, line_to->nXStart, line_to->nYStart, NULL);
	LineTo(wfi->drawing->hdc, line_to->nXEnd, line_to->nYEnd);

	x = (line_to->nXStart < line_to->nXEnd) ? line_to->nXStart : line_to->nXEnd;
	y = (line_to->nYStart < line_to->nYEnd) ? line_to->nYStart : line_to->nYEnd;
	w = (line_to->nXStart < line_to->nXEnd) ? (line_to->nXEnd - line_to->nXStart) : (line_to->nXStart - line_to->nXEnd);
	h = (line_to->nYStart < line_to->nYEnd) ? (line_to->nYEnd - line_to->nYStart) : (line_to->nYStart - line_to->nYEnd);

	if (wfi->drawing == wfi->primary)
		wf_invalidate_region(wfi, x, y, w, h);

	SelectObject(wfi->drawing->hdc, org_pen);
	DeleteObject(pen);
}

void wf_gdi_polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	int i;
	POINT* pts;
	int org_rop2;
	HPEN hpen;
	HPEN org_hpen;
	uint32 pen_color;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	pen_color = freerdp_color_convert_bgr(polyline->penColor, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);

	hpen = CreatePen(0, 1, pen_color);
	org_rop2 = wf_set_rop2(wfi->drawing->hdc, polyline->bRop2);
	org_hpen = (HPEN) SelectObject(wfi->drawing->hdc, hpen);

	if (polyline->numPoints > 0)
	{
		pts = (POINT*) xmalloc(sizeof(POINT) * polyline->numPoints);

		for (i = 0; i < (int) polyline->numPoints; i++)
		{
			pts[i].x = polyline->points[i].x;
			pts[i].y = polyline->points[i].y;

			if (wfi->drawing == wfi->primary)
				wf_invalidate_region(wfi, pts[i].x, pts[i].y, pts[i].x + 1, pts[i].y + 1);
		}

		Polyline(wfi->drawing->hdc, pts, polyline->numPoints);
		xfree(pts);
	}

	SelectObject(wfi->drawing->hdc, org_hpen);
	wf_set_rop2(wfi->drawing->hdc, org_rop2);
	DeleteObject(hpen);
}

void wf_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	wfBitmap* bitmap;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	bitmap = (wfBitmap*) memblt->bitmap;

	BitBlt(wfi->drawing->hdc, memblt->nLeftRect, memblt->nTopRect,
			memblt->nWidth, memblt->nHeight, bitmap->hdc,
			memblt->nXSrc, memblt->nYSrc, gdi_rop3_code(memblt->bRop));

	if (wfi->drawing == wfi->primary)
		wf_invalidate_region(wfi, memblt->nLeftRect, memblt->nTopRect, memblt->nWidth, memblt->nHeight);
}

void wf_gdi_surface_bits(rdpContext* context, SURFACE_BITS_COMMAND* surface_bits_command)
{
	int i, j;
	int tx, ty;
	char* tile_bitmap;
	RFX_MESSAGE* message;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*) wfi->rfx_context;
	NSC_CONTEXT* nsc_context = (NSC_CONTEXT*) wfi->nsc_context;

	tile_bitmap = (char*) xzalloc(32);

	if (surface_bits_command->codecID == CODEC_ID_REMOTEFX)
	{
		message = rfx_process_message(rfx_context, surface_bits_command->bitmapData, surface_bits_command->bitmapDataLength);

		/* blit each tile */
		for (i = 0; i < message->num_tiles; i++)
		{
			tx = message->tiles[i]->x + surface_bits_command->destLeft;
			ty = message->tiles[i]->y + surface_bits_command->destTop;

			freerdp_image_convert(message->tiles[i]->data, wfi->tile->pdata, 64, 64, 32, 32, wfi->clrconv);

			for (j = 0; j < message->num_rects; j++)
			{
				wf_set_clip_rgn(wfi,
					surface_bits_command->destLeft + message->rects[j].x,
					surface_bits_command->destTop + message->rects[j].y,
					message->rects[j].width, message->rects[j].height);

				BitBlt(wfi->primary->hdc, tx, ty, 64, 64, wfi->tile->hdc, 0, 0, SRCCOPY);
			}
		}

		wf_set_null_clip_rgn(wfi);

		/* invalidate regions */
		for (i = 0; i < message->num_rects; i++)
		{
			tx = surface_bits_command->destLeft + message->rects[i].x;
			ty = surface_bits_command->destTop + message->rects[i].y;
			wf_invalidate_region(wfi, tx, ty, message->rects[i].width, message->rects[i].height);
		}

		rfx_message_free(rfx_context, message);
	}
	else if (surface_bits_command->codecID == CODEC_ID_NSCODEC)
	{
		nsc_context->width = surface_bits_command->width;
		nsc_context->height = surface_bits_command->height;
		nsc_process_message(nsc_context, surface_bits_command->bitmapData, surface_bits_command->bitmapDataLength);
		wfi->image->_bitmap.width = surface_bits_command->width;
		wfi->image->_bitmap.height = surface_bits_command->height;
		wfi->image->_bitmap.bpp = surface_bits_command->bpp;
		wfi->image->_bitmap.data = (uint8*) xrealloc(wfi->image->_bitmap.data, wfi->image->_bitmap.width * wfi->image->_bitmap.height * 4);
		freerdp_image_flip(nsc_context->bmpdata, wfi->image->_bitmap.data, wfi->image->_bitmap.width, wfi->image->_bitmap.height, 32);
		BitBlt(wfi->primary->hdc, surface_bits_command->destLeft, surface_bits_command->destTop, surface_bits_command->width, surface_bits_command->height, wfi->image->hdc, 0, 0, GDI_SRCCOPY);
		nsc_context_destroy(nsc_context);
	} 
	else if (surface_bits_command->codecID == CODEC_ID_NONE)
	{
		wfi->image->_bitmap.width = surface_bits_command->width;
		wfi->image->_bitmap.height = surface_bits_command->height;
		wfi->image->_bitmap.bpp = surface_bits_command->bpp;

		wfi->image->_bitmap.data = (uint8*) xrealloc(wfi->image->_bitmap.data,
				wfi->image->_bitmap.width * wfi->image->_bitmap.height * 4);

		if ((surface_bits_command->bpp != 32) || (wfi->clrconv->alpha == true))
		{
			uint8* temp_image;

			freerdp_image_convert(surface_bits_command->bitmapData, wfi->image->_bitmap.data,
				wfi->image->_bitmap.width, wfi->image->_bitmap.height,
				wfi->image->_bitmap.bpp, 32, wfi->clrconv);

			surface_bits_command->bpp = 32;
			surface_bits_command->bitmapData = wfi->image->_bitmap.data;

			temp_image = (uint8*) xmalloc(wfi->image->_bitmap.width * wfi->image->_bitmap.height * 4);
			freerdp_image_flip(wfi->image->_bitmap.data, temp_image, wfi->image->_bitmap.width, wfi->image->_bitmap.height, 32);
			xfree(wfi->image->_bitmap.data);
			wfi->image->_bitmap.data = temp_image;
		}
		else
		{
			freerdp_image_flip(surface_bits_command->bitmapData, wfi->image->_bitmap.data,
					wfi->image->_bitmap.width, wfi->image->_bitmap.height, 32);
		}

		BitBlt(wfi->primary->hdc, surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height, wfi->image->hdc, 0, 0, SRCCOPY);
	}
	else
	{
		printf("Unsupported codecID %d\n", surface_bits_command->codecID);
	}

	if (tile_bitmap != NULL)
		xfree(tile_bitmap);
}

void wf_gdi_register_update_callbacks(rdpUpdate* update)
{
	rdpPrimaryUpdate* primary = update->primary;

	update->Palette = wf_gdi_palette_update;
	update->SetBounds = wf_gdi_set_bounds;

	primary->DstBlt = wf_gdi_dstblt;
	primary->PatBlt = wf_gdi_patblt;
	primary->ScrBlt = wf_gdi_scrblt;
	primary->OpaqueRect = wf_gdi_opaque_rect;
	primary->DrawNineGrid = NULL;
	primary->MultiDstBlt = NULL;
	primary->MultiPatBlt = NULL;
	primary->MultiScrBlt = NULL;
	primary->MultiOpaqueRect = wf_gdi_multi_opaque_rect;
	primary->MultiDrawNineGrid = NULL;
	primary->LineTo = wf_gdi_line_to;
	primary->Polyline = wf_gdi_polyline;
	primary->MemBlt = wf_gdi_memblt;
	primary->Mem3Blt = NULL;
	primary->SaveBitmap = NULL;
	primary->GlyphIndex = NULL;
	primary->FastIndex = NULL;
	primary->FastGlyph = NULL;
	primary->PolygonSC = NULL;
	primary->PolygonCB = NULL;
	primary->EllipseSC = NULL;
	primary->EllipseCB = NULL;

	update->SurfaceBits = wf_gdi_surface_bits;
}
