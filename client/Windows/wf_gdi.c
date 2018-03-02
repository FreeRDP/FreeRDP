/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

#include <freerdp/log.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/constants.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/gdi/gdi.h>

#include "wf_client.h"
#include "wf_graphics.h"
#include "wf_gdi.h"

#define TAG CLIENT_TAG("windows.gdi")

static const BYTE wf_rop2_table[] =
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

static BOOL wf_decode_color(wfContext* wfc, const UINT32 srcColor,
                            COLORREF* color, UINT32* format)
{
	rdpGdi* gdi;
	rdpSettings* settings;
	UINT32 SrcFormat, DstFormat;

	if (!wfc)
		return FALSE;

	gdi = wfc->context.gdi;
	settings = wfc->context.settings;

	if (!gdi || !settings)
		return FALSE;

	SrcFormat = gdi_get_pixel_format(gdi->context->settings->ColorDepth);

	if (format)
		*format = SrcFormat;

	switch (GetBitsPerPixel(gdi->dstFormat))
	{
		case 32:
			DstFormat = PIXEL_FORMAT_ABGR32;
			break;

		case 24:
			DstFormat = PIXEL_FORMAT_BGR24;
			break;

		case 16:
			DstFormat = PIXEL_FORMAT_RGB16;
			break;

		default:
			return FALSE;
	}

	*color = FreeRDPConvertColor(srcColor, SrcFormat,
	                      DstFormat, &gdi->palette);
	return TRUE;
}

static BOOL wf_set_rop2(HDC hdc, int rop2)
{
	if ((rop2 < 0x01) || (rop2 > 0x10))
	{
		WLog_ERR(TAG,  "Unsupported ROP2: %d", rop2);
		return FALSE;
	}

	SetROP2(hdc, wf_rop2_table[rop2 - 1]);
	return TRUE;
}

static wfBitmap* wf_glyph_new(wfContext* wfc, GLYPH_DATA* glyph)
{
	wfBitmap* glyph_bmp;
	glyph_bmp = wf_image_new(wfc, glyph->cx, glyph->cy, PIXEL_FORMAT_MONO,
	                         glyph->aj);
	return glyph_bmp;
}

static void wf_glyph_free(wfBitmap* glyph)
{
	wf_image_free(glyph);
}

static BYTE* wf_glyph_convert(wfContext* wfc, int width, int height, BYTE* data)
{
	int indexx;
	int indexy;
	BYTE* src;
	BYTE* dst;
	BYTE* cdata;
	int src_bytes_per_row;
	int dst_bytes_per_row;
	src_bytes_per_row = (width + 7) / 8;
	dst_bytes_per_row = src_bytes_per_row + (src_bytes_per_row % 2);
	cdata = (BYTE*) malloc(dst_bytes_per_row * height);
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

static HBRUSH wf_create_brush(wfContext* wfc, rdpBrush* brush, UINT32 color,
                              UINT32 bpp)
{
	UINT32 i;
	HBRUSH br;
	LOGBRUSH lbr;
	BYTE* cdata;
	BYTE ipattern[8];
	HBITMAP pattern = NULL;
	lbr.lbStyle = brush->style;

	if (lbr.lbStyle == BS_DIBPATTERN || lbr.lbStyle == BS_DIBPATTERN8X8
	    || lbr.lbStyle == BS_DIBPATTERNPT)
		lbr.lbColor = DIB_RGB_COLORS;
	else
		lbr.lbColor = color;

	if (lbr.lbStyle == BS_PATTERN || lbr.lbStyle == BS_PATTERN8X8)
	{
		if (brush->bpp > 1)
		{
			UINT32 format = gdi_get_pixel_format(bpp);
			pattern = wf_create_dib(wfc, 8, 8, format, brush->data, NULL);
			lbr.lbHatch = (ULONG_PTR) pattern;
		}
		else
		{
			for (i = 0; i != 8; i++)
				ipattern[7 - i] = brush->data[i];

			cdata = wf_glyph_convert(wfc, 8, 8, ipattern);
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
	SetBrushOrgEx(wfc->drawing->hdc, brush->x, brush->y, NULL);

	if (pattern != NULL)
		DeleteObject(pattern);

	return br;
}

static BOOL wf_scale_rect(wfContext* wfc, RECT* source)
{
	UINT32 ww, wh, dw, dh;
	rdpSettings* settings;

	if (!wfc || !source || !wfc->context.settings)
		return FALSE;

	settings = wfc->context.settings;

	if (!settings)
		return FALSE;

	dw = settings->DesktopWidth;
	dh = settings->DesktopHeight;

	if (!wfc->client_width)
		wfc->client_width = dw;

	if (!wfc->client_height)
		wfc->client_height = dh;

	ww = wfc->client_width;
	wh = wfc->client_height;

	if (!ww)
		ww = dw;

	if (!wh)
		wh = dh;

	if (wfc->context.settings->SmartSizing && (ww != dw || wh != dh))
	{
		source->bottom = source->bottom * wh / dh + 20;
		source->top = source->top * wh / dh - 20;
		source->left = source->left * ww / dw - 20;
		source->right = source->right * ww / dw + 20;
	}

	source->bottom -= wfc->yCurrentScroll;
	source->top -= wfc->yCurrentScroll;
	source->left -= wfc->xCurrentScroll;
	source->right -= wfc->xCurrentScroll;
	return TRUE;
}

void wf_invalidate_region(wfContext* wfc, UINT32 x, UINT32 y, UINT32 width,
                          UINT32 height)
{
	RECT rect;
	rdpGdi* gdi = wfc->context.gdi;
	wfc->update_rect.left = x + wfc->offset_x;
	wfc->update_rect.top = y + wfc->offset_y;
	wfc->update_rect.right = wfc->update_rect.left + width;
	wfc->update_rect.bottom = wfc->update_rect.top + height;
	wf_scale_rect(wfc, &(wfc->update_rect));
	InvalidateRect(wfc->hwnd, &(wfc->update_rect), FALSE);
	rect.left = x;
	rect.right = width;
	rect.top = y;
	rect.bottom = height;
	wf_scale_rect(wfc, &rect);
	gdi_InvalidateRegion(gdi->primary->hdc, rect.left, rect.top, rect.right,
	                     rect.bottom);
}

void wf_update_offset(wfContext* wfc)
{
	rdpSettings* settings;
	settings = wfc->context.settings;

	if (wfc->fullscreen)
	{
		if (wfc->context.settings->UseMultimon)
		{
			int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
			int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
			int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
			wfc->offset_x = (w - settings->DesktopWidth) / 2;

			if (wfc->offset_x < x)
				wfc->offset_x = x;

			wfc->offset_y = (h - settings->DesktopHeight) / 2;

			if (wfc->offset_y < y)
				wfc->offset_y = y;
		}
		else
		{
			wfc->offset_x = (GetSystemMetrics(SM_CXSCREEN) - settings->DesktopWidth) / 2;

			if (wfc->offset_x < 0)
				wfc->offset_x = 0;

			wfc->offset_y = (GetSystemMetrics(SM_CYSCREEN) - settings->DesktopHeight) / 2;

			if (wfc->offset_y < 0)
				wfc->offset_y = 0;
		}
	}
	else
	{
		wfc->offset_x = 0;
		wfc->offset_y = 0;
	}
}

void wf_resize_window(wfContext* wfc)
{
	rdpSettings* settings;
	settings = wfc->context.settings;

	if (wfc->fullscreen)
	{
		if (wfc->context.settings->UseMultimon)
		{
			int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
			int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
			int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
			SetWindowLongPtr(wfc->hwnd, GWL_STYLE, WS_POPUP);
			SetWindowPos(wfc->hwnd, HWND_TOP, x, y, w, h, SWP_FRAMECHANGED);
		}
		else
		{
			SetWindowLongPtr(wfc->hwnd, GWL_STYLE, WS_POPUP);
			SetWindowPos(wfc->hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN),
			             GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
		}
	}
	else if (!wfc->context.settings->Decorations)
	{
		SetWindowLongPtr(wfc->hwnd, GWL_STYLE, WS_CHILD);
		if (settings->EmbeddedWindow)
		{
			wf_update_canvas_diff(wfc);
		}
		else
		{
			/* Now resize to get full canvas size and room for caption and borders */
			SetWindowPos(wfc->hwnd, HWND_TOP, 0, 0, settings->DesktopWidth,
				settings->DesktopHeight, SWP_FRAMECHANGED);
			wf_update_canvas_diff(wfc);
			SetWindowPos(wfc->hwnd, HWND_TOP, -1, -1, settings->DesktopWidth + wfc->diff.x,
				settings->DesktopHeight + wfc->diff.y, SWP_NOMOVE | SWP_FRAMECHANGED);
		}
	}
	else
	{
		SetWindowLongPtr(wfc->hwnd, GWL_STYLE,
		                 WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX |
		                 WS_MAXIMIZEBOX);

		if (!wfc->client_height)
			wfc->client_height = settings->DesktopHeight;

		if (!wfc->client_width)
			wfc->client_width = settings->DesktopWidth;

		if (!wfc->client_x)
			wfc->client_x = 10;

		if (!wfc->client_y)
			wfc->client_y = 10;

		wf_update_canvas_diff(wfc);
		/* Now resize to get full canvas size and room for caption and borders */
		SetWindowPos(wfc->hwnd, HWND_TOP, wfc->client_x, wfc->client_y,
		             wfc->client_width + wfc->diff.x, wfc->client_height + wfc->diff.y,
		             0 /*SWP_FRAMECHANGED*/);
		//wf_size_scrollbars(wfc,  wfc->client_width, wfc->client_height);
	}

	wf_update_offset(wfc);
}

void wf_toggle_fullscreen(wfContext* wfc)
{
	ShowWindow(wfc->hwnd, SW_HIDE);
	wfc->fullscreen = !wfc->fullscreen;

	if (wfc->fullscreen)
	{
		wfc->disablewindowtracking = TRUE;
	}

	if (wfc->fullscreen)
		floatbar_show(wfc->floatbar);
	else
		floatbar_hide(wfc->floatbar);

	SetParent(wfc->hwnd, wfc->fullscreen ? NULL : wfc->hWndParent);
	wf_resize_window(wfc);
	ShowWindow(wfc->hwnd, SW_SHOW);
	SetForegroundWindow(wfc->hwnd);

	if (!wfc->fullscreen)
	{
		// Reenable window tracking AFTER resizing it back, otherwise it can lean to repositioning errors.
		wfc->disablewindowtracking = FALSE;
	}
}

static BOOL wf_gdi_palette_update(rdpContext* context,
                                  const PALETTE_UPDATE* palette)
{
	return TRUE;
}

void wf_set_null_clip_rgn(wfContext* wfc)
{
	SelectClipRgn(wfc->drawing->hdc, NULL);
}

void wf_set_clip_rgn(wfContext* wfc, int x, int y, int width, int height)
{
	HRGN clip;
	clip = CreateRectRgn(x, y, x + width, y + height);
	SelectClipRgn(wfc->drawing->hdc, clip);
	DeleteObject(clip);
}

static BOOL wf_gdi_set_bounds(rdpContext* context, const rdpBounds* bounds)
{
	HRGN hrgn;
	wfContext* wfc = (wfContext*)context;

	if (!context || !bounds)
		return FALSE;

	if (bounds != NULL)
	{
		hrgn = CreateRectRgn(bounds->left, bounds->top, bounds->right + 1,
		                     bounds->bottom + 1);
		SelectClipRgn(wfc->drawing->hdc, hrgn);
		DeleteObject(hrgn);
	}
	else
		SelectClipRgn(wfc->drawing->hdc, NULL);

	return TRUE;
}

static BOOL wf_gdi_dstblt(rdpContext* context, const DSTBLT_ORDER* dstblt)
{
	wfContext* wfc = (wfContext*)context;

	if (!context || !dstblt)
		return FALSE;

	if (!BitBlt(wfc->drawing->hdc, dstblt->nLeftRect, dstblt->nTopRect,
	            dstblt->nWidth, dstblt->nHeight, NULL, 0, 0, gdi_rop3_code(dstblt->bRop)))
		return FALSE;

	wf_invalidate_region(wfc, dstblt->nLeftRect, dstblt->nTopRect,
	                     dstblt->nWidth, dstblt->nHeight);
	return TRUE;
}

static BOOL wf_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	HBRUSH brush;
	HBRUSH org_brush;
	int org_bkmode;
	UINT32 fgcolor;
	UINT32 bgcolor;
	COLORREF org_bkcolor;
	COLORREF org_textcolor;
	BOOL rc;
	wfContext* wfc = (wfContext*)context;

	if (!context || !patblt)
		return FALSE;

	if (!wf_decode_color(wfc, patblt->foreColor, &fgcolor, NULL))
		return FALSE;

	if (!wf_decode_color(wfc, patblt->backColor, &bgcolor, NULL))
		return FALSE;

	brush = wf_create_brush(wfc, &patblt->brush, fgcolor,
	                        context->settings->ColorDepth);
	org_bkmode = SetBkMode(wfc->drawing->hdc, OPAQUE);
	org_bkcolor = SetBkColor(wfc->drawing->hdc, bgcolor);
	org_textcolor = SetTextColor(wfc->drawing->hdc, fgcolor);
	org_brush = (HBRUSH)SelectObject(wfc->drawing->hdc, brush);
	rc = PatBlt(wfc->drawing->hdc, patblt->nLeftRect, patblt->nTopRect,
	            patblt->nWidth, patblt->nHeight, gdi_rop3_code(patblt->bRop));
	SelectObject(wfc->drawing->hdc, org_brush);
	DeleteObject(brush);
	SetBkMode(wfc->drawing->hdc, org_bkmode);
	SetBkColor(wfc->drawing->hdc, org_bkcolor);
	SetTextColor(wfc->drawing->hdc, org_textcolor);

	if (wfc->drawing == wfc->primary)
		wf_invalidate_region(wfc, patblt->nLeftRect, patblt->nTopRect, patblt->nWidth,
		                     patblt->nHeight);

	return rc;
}

static BOOL wf_gdi_scrblt(rdpContext* context, const SCRBLT_ORDER* scrblt)
{
	wfContext* wfc = (wfContext*)context;

	if (!context || !scrblt || !wfc->drawing)
		return FALSE;

	if (!BitBlt(wfc->drawing->hdc, scrblt->nLeftRect, scrblt->nTopRect,
	            scrblt->nWidth, scrblt->nHeight, wfc->primary->hdc,
	            scrblt->nXSrc, scrblt->nYSrc, gdi_rop3_code(scrblt->bRop)))
		return FALSE;

	wf_invalidate_region(wfc, scrblt->nLeftRect, scrblt->nTopRect,
	                     scrblt->nWidth, scrblt->nHeight);
	return TRUE;
}

static BOOL wf_gdi_opaque_rect(rdpContext* context,
                               const OPAQUE_RECT_ORDER* opaque_rect)
{
	RECT rect;
	HBRUSH brush;
	UINT32 brush_color;
	wfContext* wfc = (wfContext*)context;

	if (!context || !opaque_rect)
		return FALSE;

	if (!wf_decode_color(wfc, opaque_rect->color, &brush_color, NULL))
		return FALSE;

	rect.left = opaque_rect->nLeftRect;
	rect.top = opaque_rect->nTopRect;
	rect.right = opaque_rect->nLeftRect + opaque_rect->nWidth;
	rect.bottom = opaque_rect->nTopRect + opaque_rect->nHeight;
	brush = CreateSolidBrush(brush_color);
	FillRect(wfc->drawing->hdc, &rect, brush);
	DeleteObject(brush);

	if (wfc->drawing == wfc->primary)
		wf_invalidate_region(wfc, rect.left, rect.top, rect.right - rect.left + 1,
		                     rect.bottom - rect.top + 1);

	return TRUE;
}

static BOOL wf_gdi_multi_opaque_rect(rdpContext* context,
                                     const MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	UINT32 i;
	RECT rect;
	HBRUSH brush;
	UINT32 brush_color;
	wfContext* wfc = (wfContext*)context;

	if (!context || !multi_opaque_rect)
		return FALSE;

	if (!wf_decode_color(wfc, multi_opaque_rect->color, &brush_color,
	                     NULL))
		return FALSE;

	for (i = 0; i < multi_opaque_rect->numRectangles; i++)
	{
		const DELTA_RECT* rectangle = &multi_opaque_rect->rectangles[i];
		rect.left = rectangle->left;
		rect.top = rectangle->top;
		rect.right = rectangle->left + rectangle->width;
		rect.bottom = rectangle->top + rectangle->height;
		brush = CreateSolidBrush(brush_color);
		FillRect(wfc->drawing->hdc, &rect, brush);

		if (wfc->drawing == wfc->primary)
			wf_invalidate_region(wfc, rect.left, rect.top, rect.right - rect.left + 1,
			                     rect.bottom - rect.top + 1);

		DeleteObject(brush);
	}

	return TRUE;
}

static BOOL wf_gdi_line_to(rdpContext* context, const LINE_TO_ORDER* line_to)
{
	HPEN pen;
	HPEN org_pen;
	int x, y, w, h;
	UINT32 pen_color;
	wfContext* wfc = (wfContext*)context;

	if (!context || !line_to)
		return FALSE;

	if (!wf_decode_color(wfc, line_to->penColor, &pen_color, NULL))
		return FALSE;

	pen = CreatePen(line_to->penStyle, line_to->penWidth, pen_color);
	wf_set_rop2(wfc->drawing->hdc, line_to->bRop2);
	org_pen = (HPEN) SelectObject(wfc->drawing->hdc, pen);
	MoveToEx(wfc->drawing->hdc, line_to->nXStart, line_to->nYStart, NULL);
	LineTo(wfc->drawing->hdc, line_to->nXEnd, line_to->nYEnd);
	x = (line_to->nXStart < line_to->nXEnd) ? line_to->nXStart : line_to->nXEnd;
	y = (line_to->nYStart < line_to->nYEnd) ? line_to->nYStart : line_to->nYEnd;
	w = (line_to->nXStart < line_to->nXEnd) ? (line_to->nXEnd - line_to->nXStart)
	    : (line_to->nXStart - line_to->nXEnd);
	h = (line_to->nYStart < line_to->nYEnd) ? (line_to->nYEnd - line_to->nYStart)
	    : (line_to->nYStart - line_to->nYEnd);

	if (wfc->drawing == wfc->primary)
		wf_invalidate_region(wfc, x, y, w, h);

	SelectObject(wfc->drawing->hdc, org_pen);
	DeleteObject(pen);
	return TRUE;
}

static BOOL wf_gdi_polyline(rdpContext* context, const POLYLINE_ORDER* polyline)
{
	int org_rop2;
	HPEN hpen;
	HPEN org_hpen;
	UINT32 pen_color;
	wfContext* wfc = (wfContext*)context;

	if (!context || !polyline)
		return FALSE;

	if (!wf_decode_color(wfc, polyline->penColor, &pen_color, NULL))
		return FALSE;

	hpen = CreatePen(0, 1, pen_color);
	org_rop2 = wf_set_rop2(wfc->drawing->hdc, polyline->bRop2);
	org_hpen = (HPEN) SelectObject(wfc->drawing->hdc, hpen);

	if (polyline->numDeltaEntries > 0)
	{
		POINT*  pts;
		POINT  temp;
		int    numPoints;
		int    i;
		numPoints = polyline->numDeltaEntries + 1;
		pts = (POINT*) malloc(sizeof(POINT) * numPoints);
		pts[0].x = temp.x = polyline->xStart;
		pts[0].y = temp.y = polyline->yStart;

		for (i = 0; i < (int) polyline->numDeltaEntries; i++)
		{
			temp.x += polyline->points[i].x;
			temp.y += polyline->points[i].y;
			pts[i + 1].x = temp.x;
			pts[i + 1].y = temp.y;
		}

		if (wfc->drawing == wfc->primary)
			wf_invalidate_region(wfc, wfc->client_x, wfc->client_y, wfc->client_width,
			                     wfc->client_height);

		Polyline(wfc->drawing->hdc, pts, numPoints);
		free(pts);
	}

	SelectObject(wfc->drawing->hdc, org_hpen);
	wf_set_rop2(wfc->drawing->hdc, org_rop2);
	DeleteObject(hpen);
	return TRUE;
}

static BOOL wf_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	wfBitmap* bitmap;
	wfContext* wfc = (wfContext*)context;

	if (!context || !memblt)
		return FALSE;

	bitmap = (wfBitmap*) memblt->bitmap;

	if (!bitmap || !wfc->drawing || !wfc->drawing->hdc)
		return FALSE;

	if (!BitBlt(wfc->drawing->hdc, memblt->nLeftRect, memblt->nTopRect,
	            memblt->nWidth, memblt->nHeight, bitmap->hdc,
	            memblt->nXSrc, memblt->nYSrc, gdi_rop3_code(memblt->bRop)))
		return FALSE;

	if (wfc->drawing == wfc->primary)
		wf_invalidate_region(wfc, memblt->nLeftRect, memblt->nTopRect, memblt->nWidth,
		                     memblt->nHeight);

	return TRUE;
}

static BOOL wf_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	BOOL rc = FALSE;
	HDC hdc;
	wfBitmap* bitmap;
	wfContext* wfc = (wfContext*)context;
	COLORREF fgcolor, bgcolor, orgColor;
	HBRUSH orgBrush = NULL, brush = NULL;

	if (!context || !mem3blt)
		return FALSE;

	bitmap = (wfBitmap*) mem3blt->bitmap;

	if (!bitmap || !wfc->drawing || !wfc->drawing->hdc)
		return FALSE;

	hdc = wfc->drawing->hdc;

	if (!wf_decode_color(wfc, mem3blt->foreColor, &fgcolor, NULL))
		return FALSE;

	if (!wf_decode_color(wfc, mem3blt->backColor, &bgcolor, NULL))
		return FALSE;

	orgColor = SetTextColor(hdc, fgcolor);

	switch (mem3blt->brush.style)
	{
		case GDI_BS_SOLID:
			brush = CreateSolidBrush(fgcolor);
			break;

		case GDI_BS_HATCHED:
		case GDI_BS_PATTERN:
			{
				HBITMAP bmp = CreateBitmap(8, 8, 1, mem3blt->brush.bpp, mem3blt->brush.data);
				brush = CreatePatternBrush(bmp);
			}
			break;

		default:
			goto fail;
	}

	orgBrush = SelectObject(hdc, brush);

	if (!BitBlt(hdc, mem3blt->nLeftRect, mem3blt->nTopRect,
	            mem3blt->nWidth, mem3blt->nHeight, bitmap->hdc,
	            mem3blt->nXSrc, mem3blt->nYSrc, gdi_rop3_code(mem3blt->bRop)))
		goto fail;

	if (wfc->drawing == wfc->primary)
		wf_invalidate_region(wfc, mem3blt->nLeftRect, mem3blt->nTopRect,
		                     mem3blt->nWidth,
		                     mem3blt->nHeight);

	rc = TRUE;
fail:

	if (brush)
		SelectObject(hdc, orgBrush);

	SetTextColor(hdc, orgColor);
	return rc;
}

static BOOL wf_gdi_surface_frame_marker(rdpContext* context,
                                        const SURFACE_FRAME_MARKER* surface_frame_marker)
{
	rdpSettings* settings;

	if (!context || !surface_frame_marker || !context->instance)
		return FALSE;

	settings = context->instance->settings;

	if (!settings)
		return FALSE;

	if (surface_frame_marker->frameAction == SURFACECMD_FRAMEACTION_END
	    && settings->FrameAcknowledge > 0)
	{
		IFCALL(context->instance->update->SurfaceFrameAcknowledge, context,
		       surface_frame_marker->frameId);
	}

	return TRUE;
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
	primary->MultiOpaqueRect = wf_gdi_multi_opaque_rect;
	primary->LineTo = wf_gdi_line_to;
	primary->Polyline = wf_gdi_polyline;
	primary->MemBlt = wf_gdi_memblt;
	primary->Mem3Blt = wf_gdi_mem3blt;
	update->SurfaceFrameMarker = wf_gdi_surface_frame_marker;
}

void wf_update_canvas_diff(wfContext* wfc)
{
	RECT rc_client, rc_wnd;
	int dx, dy;
	GetClientRect(wfc->hwnd, &rc_client);
	GetWindowRect(wfc->hwnd, &rc_wnd);
	dx = (rc_wnd.right - rc_wnd.left) - rc_client.right;
	dy = (rc_wnd.bottom - rc_wnd.top) - rc_client.bottom;

	if (!wfc->disablewindowtracking)
	{
		wfc->diff.x = dx;
		wfc->diff.y = dy;
	}
}
