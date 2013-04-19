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

#include <freerdp/gdi/gdi.h>
#include <freerdp/constants.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>

#include "wf_interface.h"
#include "wf_graphics.h"

const BYTE wf_rop2_table[] =
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

BOOL wf_set_rop2(HDC hdc, int rop2)
{
	if ((rop2 < 0x01) || (rop2 > 0x10))
	{
		fprintf(stderr, "Unsupported ROP2: %d\n", rop2);
		return FALSE;
	}

	SetROP2(hdc, wf_rop2_table[rop2 - 1]);

	return TRUE;
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

BYTE* wf_glyph_convert(wfInfo* wfi, int width, int height, BYTE* data)
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
	cdata = (BYTE *) malloc(dst_bytes_per_row * height);

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

HBRUSH wf_create_brush(wfInfo * wfi, rdpBrush* brush, UINT32 color, int bpp)
{
	int i;
	HBRUSH br;
	LOGBRUSH lbr;
	BYTE* cdata;
	BYTE ipattern[8];
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

void wf_scale_rect(wfInfo* wfi, RECT* source)
{
	int ww, wh, dw, dh;

	if (!wfi->client_width)
		wfi->client_width = wfi->width;

	if (!wfi->client_height)
		wfi->client_height = wfi->height;

	ww = wfi->client_width;
	wh = wfi->client_height;
	dw = wfi->instance->settings->DesktopWidth;
	dh = wfi->instance->settings->DesktopHeight;

	if (!ww)
		ww = dw;

	if (!wh)
		wh = dh;

	if (wfi->instance->settings->SmartSizing && (ww != dw || wh != dh))
	{
		source->bottom = MIN(dh, MAX(0, source->bottom * wh / dh + 2));
		source->top = MIN(dh, MAX(0, source->top * wh / dh - 2));
		source->left = MIN(dw, MAX(0, source->left * ww / dw - 2));
		source->right = MIN(dw, MAX(0, source->right * ww / dw + 2));
	}
}

void wf_invalidate_region(wfInfo* wfi, int x, int y, int width, int height)
{
	RECT rect;

	wfi->update_rect.left = x + wfi->offset_x;
	wfi->update_rect.top = y + wfi->offset_y;
	wfi->update_rect.right = wfi->update_rect.left + width;
	wfi->update_rect.bottom = wfi->update_rect.top + height;

	wf_scale_rect(wfi, &(wfi->update_rect));
	InvalidateRect(wfi->hwnd, &(wfi->update_rect), FALSE);

	rect.left = x;
	rect.right = width;
	rect.top = y;
	rect.bottom = height;
	wf_scale_rect(wfi, &rect);
	gdi_InvalidateRegion(wfi->hdc, rect.left, rect.top, rect.right, rect.bottom);
}

void wf_update_offset(wfInfo* wfi)
{
	if (wfi->fullscreen)
	{
		wfi->offset_x = (GetSystemMetrics(SM_CXSCREEN) - wfi->width) / 2;
		if (wfi->offset_x < 0)
			wfi->offset_x = 0;
		wfi->offset_y = (GetSystemMetrics(SM_CYSCREEN) - wfi->height) / 2;
		if (wfi->offset_y < 0)
			wfi->offset_y = 0;
	}
	else
	{
		wfi->offset_x = 0;
		wfi->offset_y = 0;
	}
}

void wf_resize_window(wfInfo* wfi)
{
	if (wfi->fullscreen)
	{
		SetWindowLongPtr(wfi->hwnd, GWL_STYLE, WS_POPUP);
		SetWindowPos(wfi->hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
	}
	else if (!wfi->instance->settings->Decorations)
	{
		RECT rc_wnd;
		RECT rc_client;
		
		SetWindowLongPtr(wfi->hwnd, GWL_STYLE, WS_CHILD);

		/* Now resize to get full canvas size and room for caption and borders */
		SetWindowPos(wfi->hwnd, HWND_TOP, 0, 0, wfi->width, wfi->height, SWP_FRAMECHANGED);
		GetClientRect(wfi->hwnd, &rc_client);
		GetWindowRect(wfi->hwnd, &rc_wnd);
		
		wfi->diff.x = (rc_wnd.right - rc_wnd.left) - rc_client.right;
		wfi->diff.y = (rc_wnd.bottom - rc_wnd.top) - rc_client.bottom;
		
		SetWindowPos(wfi->hwnd, HWND_TOP, -1, -1, wfi->width + wfi->diff.x, wfi->height + wfi->diff.y, SWP_NOMOVE | SWP_FRAMECHANGED);
	}
	else
	{
		RECT rc_wnd;
		RECT rc_client;

		SetWindowLongPtr(wfi->hwnd, GWL_STYLE, WS_CAPTION | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX);

		/* Now resize to get full canvas size and room for caption and borders */
		SetWindowPos(wfi->hwnd, HWND_TOP, 10, 10, wfi->width, wfi->height, SWP_FRAMECHANGED);
		GetClientRect(wfi->hwnd, &rc_client);
		GetWindowRect(wfi->hwnd, &rc_wnd);

		wfi->diff.x = (rc_wnd.right - rc_wnd.left) - rc_client.right;
		wfi->diff.y = (rc_wnd.bottom - rc_wnd.top) - rc_client.bottom;
		
		SetWindowPos(wfi->hwnd, HWND_TOP, -1, -1, wfi->width + wfi->diff.x, wfi->height + wfi->diff.y, SWP_NOMOVE | SWP_FRAMECHANGED);
	}
	wf_update_offset(wfi);
}

void wf_toggle_fullscreen(wfInfo* wfi)
{
	ShowWindow(wfi->hwnd, SW_HIDE);
	wfi->fullscreen = !wfi->fullscreen;
	wf_resize_window(wfi);
	ShowWindow(wfi->hwnd, SW_SHOW);
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
	UINT32 fgcolor;
	UINT32 bgcolor;
	COLORREF org_bkcolor;
	COLORREF org_textcolor;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	fgcolor = freerdp_color_convert_bgr(patblt->foreColor, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);
	bgcolor = freerdp_color_convert_bgr(patblt->backColor, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);

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
	UINT32 brush_color;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	brush_color = freerdp_color_convert_var_bgr(opaque_rect->color, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);

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
	UINT32 brush_color;
	DELTA_RECT* rectangle;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	for (i = 1; i < (int) multi_opaque_rect->numRectangles + 1; i++)
	{
		rectangle = &multi_opaque_rect->rectangles[i];

		brush_color = freerdp_color_convert_var_bgr(multi_opaque_rect->color, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);

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
	UINT32 pen_color;
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
	UINT32 pen_color;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	pen_color = freerdp_color_convert_bgr(polyline->penColor, wfi->srcBpp, wfi->dstBpp, wfi->clrconv);

	hpen = CreatePen(0, 1, pen_color);
	org_rop2 = wf_set_rop2(wfi->drawing->hdc, polyline->bRop2);
	org_hpen = (HPEN) SelectObject(wfi->drawing->hdc, hpen);

	if (polyline->numPoints > 0)
	{
		pts = (POINT*) malloc(sizeof(POINT) * polyline->numPoints);

		for (i = 0; i < (int) polyline->numPoints; i++)
		{
			pts[i].x = polyline->points[i].x;
			pts[i].y = polyline->points[i].y;

			if (wfi->drawing == wfi->primary)
				wf_invalidate_region(wfi, pts[i].x, pts[i].y, pts[i].x + 1, pts[i].y + 1);
		}

		Polyline(wfi->drawing->hdc, pts, polyline->numPoints);
		free(pts);
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
	BITMAPINFO bitmap_info;
	wfInfo* wfi = ((wfContext*) context)->wfi;

	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*) wfi->rfx_context;
	NSC_CONTEXT* nsc_context = (NSC_CONTEXT*) wfi->nsc_context;

	tile_bitmap = (char*) malloc(32);
	ZeroMemory(tile_bitmap, 32);

	if (surface_bits_command->codecID == RDP_CODEC_ID_REMOTEFX)
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
	else if (surface_bits_command->codecID == RDP_CODEC_ID_NSCODEC)
	{
		nsc_process_message(nsc_context, surface_bits_command->bpp, surface_bits_command->width, surface_bits_command->height,
			surface_bits_command->bitmapData, surface_bits_command->bitmapDataLength);
		ZeroMemory(&bitmap_info, sizeof(bitmap_info));
		bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmap_info.bmiHeader.biWidth = surface_bits_command->width;
		bitmap_info.bmiHeader.biHeight = surface_bits_command->height;
		bitmap_info.bmiHeader.biPlanes = 1;
		bitmap_info.bmiHeader.biBitCount = surface_bits_command->bpp;
		bitmap_info.bmiHeader.biCompression = BI_RGB;
		SetDIBitsToDevice(wfi->primary->hdc, surface_bits_command->destLeft, surface_bits_command->destTop,
			surface_bits_command->width, surface_bits_command->height, 0, 0, 0, surface_bits_command->height,
			nsc_context->bmpdata, &bitmap_info, DIB_RGB_COLORS);
		wf_invalidate_region(wfi, surface_bits_command->destLeft, surface_bits_command->destTop,
			surface_bits_command->width, surface_bits_command->height);
	}
	else if (surface_bits_command->codecID == RDP_CODEC_ID_NONE)
	{
		ZeroMemory(&bitmap_info, sizeof(bitmap_info));
		bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmap_info.bmiHeader.biWidth = surface_bits_command->width;
		bitmap_info.bmiHeader.biHeight = surface_bits_command->height;
		bitmap_info.bmiHeader.biPlanes = 1;
		bitmap_info.bmiHeader.biBitCount = surface_bits_command->bpp;
		bitmap_info.bmiHeader.biCompression = BI_RGB;
		SetDIBitsToDevice(wfi->primary->hdc, surface_bits_command->destLeft, surface_bits_command->destTop,
			surface_bits_command->width, surface_bits_command->height, 0, 0, 0, surface_bits_command->height,
			surface_bits_command->bitmapData, &bitmap_info, DIB_RGB_COLORS);
		wf_invalidate_region(wfi, surface_bits_command->destLeft, surface_bits_command->destTop,
			surface_bits_command->width, surface_bits_command->height);
	}
	else
	{
		fprintf(stderr, "Unsupported codecID %d\n", surface_bits_command->codecID);
	}

	if (tile_bitmap != NULL)
		free(tile_bitmap);
}

void wf_gdi_surface_frame_marker(rdpContext* context, SURFACE_FRAME_MARKER* surface_frame_marker)
{
	wfInfo* wfi;
	rdpSettings* settings;

	wfi = ((wfContext*) context)->wfi;
	settings = wfi->instance->settings;
	if (surface_frame_marker->frameAction == SURFACECMD_FRAMEACTION_END && settings->FrameAcknowledge > 0)
	{
		IFCALL(wfi->instance->update->SurfaceFrameAcknowledge, context, surface_frame_marker->frameId);
	}
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
	update->SurfaceFrameMarker = wf_gdi_surface_frame_marker;
}
