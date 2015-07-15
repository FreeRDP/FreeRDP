/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 GDI
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/constants.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>

#include "xf_gdi.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

static UINT8 GDI_BS_HATCHED_PATTERNS[] =
{
	0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, /* HS_HORIZONTAL */
	0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, 0xF7, /* HS_VERTICAL */
	0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F, /* HS_FDIAGONAL */
	0x7F, 0xBF, 0xDF, 0xEF, 0xF7, 0xFB, 0xFD, 0xFE, /* HS_BDIAGONAL */
	0xF7, 0xF7, 0xF7, 0x00, 0xF7, 0xF7, 0xF7, 0xF7, /* HS_CROSS */
	0x7E, 0xBD, 0xDB, 0xE7, 0xE7, 0xDB, 0xBD, 0x7E /* HS_DIACROSS */
};

static const BYTE xf_rop2_table[] =
{
	0,
	GXclear,        /* 0 */
	GXnor,          /* DPon */
	GXandInverted,  /* DPna */
	GXcopyInverted, /* Pn */
	GXandReverse,   /* PDna */
	GXinvert,       /* Dn */
	GXxor,          /* DPx */
	GXnand,         /* DPan */
	GXand,          /* DPa */
	GXequiv,        /* DPxn */
	GXnoop,         /* D */
	GXorInverted,   /* DPno */
	GXcopy,         /* P */
	GXorReverse,    /* PDno */
	GXor,           /* DPo */
	GXset           /* 1 */
};

BOOL xf_set_rop2(xfContext* xfc, int rop2)
{
	if ((rop2 < 0x01) || (rop2 > 0x10))
	{
		WLog_ERR(TAG,  "Unsupported ROP2: %d", rop2);
		return FALSE;
	}

	XSetFunction(xfc->display, xfc->gc, xf_rop2_table[rop2]);
	return TRUE;
}

BOOL xf_set_rop3(xfContext* xfc, int rop3)
{
	int function = -1;

	switch (rop3)
	{
		case GDI_BLACKNESS:
			function = GXclear;
			break;

		case GDI_DPon:
			function = GXnor;
			break;

		case GDI_DPna:
			function = GXandInverted;
			break;

		case GDI_Pn:
			function = GXcopyInverted;
			break;

		case GDI_NOTSRCERASE:
			function = GXnor;
			break;

		case GDI_DSna:
			function = GXandInverted;
			break;

		case GDI_NOTSRCCOPY:
			function = GXcopyInverted;
			break;

		case GDI_SRCERASE:
			function = GXandReverse;
			break;

		case GDI_PDna:
			function = GXandReverse;
			break;

		case GDI_DSTINVERT:
			function = GXinvert;
			break;

		case GDI_PATINVERT:
			function = GXxor;
			break;

		case GDI_DPan:
			function = GXnand;
			break;

		case GDI_SRCINVERT:
			function = GXxor;
			break;

		case GDI_DSan:
			function = GXnand;
			break;

		case GDI_SRCAND:
			function = GXand;
			break;

		case GDI_DSxn:
			function = GXequiv;
			break;

		case GDI_DPa:
			function = GXand;
			break;

		case GDI_PDxn:
			function = GXequiv;
			break;

		case GDI_D:
			function = GXnoop;
			break;

		case GDI_DPno:
			function = GXorInverted;
			break;

		case GDI_MERGEPAINT:
			function = GXorInverted;
			break;

		case GDI_SRCCOPY:
			function = GXcopy;
			break;

		case GDI_SDno:
			function = GXorReverse;
			break;

		case GDI_SRCPAINT:
			function = GXor;
			break;

		case GDI_PATCOPY:
			function = GXcopy;
			break;

		case GDI_PDno:
			function = GXorReverse;
			break;

		case GDI_DPo:
			function = GXor;
			break;

		case GDI_WHITENESS:
			function = GXset;
			break;

		case GDI_PSDPxax:
			function = GXand;
			break;

		default:
			break;
	}

	if (function < 0)
	{
		WLog_ERR(TAG,  "Unsupported ROP3: 0x%08X", rop3);
		XSetFunction(xfc->display, xfc->gc, GXclear);
		return FALSE;
	}

	XSetFunction(xfc->display, xfc->gc, function);

	return TRUE;
}

UINT32 xf_convert_rdp_order_color(xfContext* xfc, UINT32 color)
{
	UINT32 r = 0, g = 0, b = 0;

	switch (xfc->srcBpp)
	{
	case 32:
	case 24:
		if (xfc->visual->red_mask == 0xFF0000 &&
		    xfc->visual->green_mask == 0xFF00 &&
		    xfc->visual->blue_mask == 0xFF)
		{
			return color;
		}
		GetRGB32(r, g, b, color);
		break;

	case 16:
		color = (color & 0xFF00) | ((color >> 16) & 0xFF);
		if (xfc->visual->red_mask == 0xF800 &&
		    xfc->visual->green_mask == 0x07E0 &&
		    xfc->visual->blue_mask == 0x001F)
		{
			return color;
		}
		GetRGB16(r, g, b, color);
		break;

	case 15:
		color = (color & 0xFF00) | ((color >> 16) & 0xFF);
		GetRGB15(r, g, b, color);
		break;

	case 8:
		color = (color >> 16) & (UINT32) 0xFF;
		if (xfc->palette)
		{
			r = xfc->palette[(color * 4) + 2];
			g = xfc->palette[(color * 4) + 1];
			b = xfc->palette[(color * 4) + 0];
		}
		break;

	default:
		return color;
	}

	return ( ((r >> xfc->red_shift_r) << xfc->red_shift_l) |
	         ((g >> xfc->green_shift_r) << xfc->green_shift_l) |
	         ((b >> xfc->blue_shift_r) << xfc->blue_shift_l) );
}

Pixmap xf_brush_new(xfContext* xfc, int width, int height, int bpp, BYTE* data)
{
	GC gc;
	Pixmap bitmap;
	BYTE* cdata;
	XImage* image;
	UINT32 brushFormat;

	bitmap = XCreatePixmap(xfc->display, xfc->drawable, width, height, xfc->depth);

	if (data)
	{
		brushFormat = gdi_get_pixel_format(bpp, FALSE);

		cdata = (BYTE*) _aligned_malloc(width * height * 4, 16);

		freerdp_image_copy(cdata, xfc->format, -1, 0, 0,
				width, height, data, brushFormat, -1, 0, 0, xfc->palette);

		image = XCreateImage(xfc->display, xfc->visual, xfc->depth,
				ZPixmap, 0, (char*) cdata, width, height, xfc->scanline_pad, 0);

		gc = XCreateGC(xfc->display, xfc->drawable, 0, NULL);
		XPutImage(xfc->display, bitmap, gc, image, 0, 0, 0, 0, width, height);
		XFree(image);

		if (cdata != data)
			_aligned_free(cdata);

		XFreeGC(xfc->display, gc);
	}

	return bitmap;
}

Pixmap xf_mono_bitmap_new(xfContext* xfc, int width, int height, BYTE* data)
{
	int scanline;
	XImage* image;
	Pixmap bitmap;

	scanline = (width + 7) / 8;

	bitmap = XCreatePixmap(xfc->display, xfc->drawable, width, height, 1);

	image = XCreateImage(xfc->display, xfc->visual, 1,
			ZPixmap, 0, (char*) data, width, height, 8, scanline);

	XPutImage(xfc->display, bitmap, xfc->gc_mono, image, 0, 0, 0, 0, width, height);
	XFree(image);

	return bitmap;
}

BOOL xf_gdi_bitmap_update(rdpContext* context, BITMAP_UPDATE* bitmapUpdate)
{
	int status;
	int nXDst;
	int nYDst;
	int nXSrc;
	int nYSrc;
	int nWidth;
	int nHeight;
	UINT32 index;
	XImage* image;
	BYTE* pSrcData;
	BYTE* pDstData;
	UINT32 SrcSize;
	BOOL compressed;
	UINT32 SrcFormat;
	UINT32 bitsPerPixel;
	UINT32 bytesPerPixel;
	BITMAP_DATA* bitmap;
	rdpCodecs* codecs = context->codecs;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	for (index = 0; index < bitmapUpdate->number; index++)
	{
		bitmap = &(bitmapUpdate->rectangles[index]);

		nXSrc = 0;
		nYSrc = 0;

		nXDst = bitmap->destLeft;
		nYDst = bitmap->destTop;

		nWidth = bitmap->width;
		nHeight = bitmap->height;

		pSrcData = bitmap->bitmapDataStream;
		SrcSize = bitmap->bitmapLength;

		compressed = bitmap->compressed;
		bitsPerPixel = bitmap->bitsPerPixel;
		bytesPerPixel = (bitsPerPixel + 7) / 8;

		SrcFormat = gdi_get_pixel_format(bitsPerPixel, TRUE);

		if (xfc->bitmap_size < (nWidth * nHeight * 4))
		{
			xfc->bitmap_size = nWidth * nHeight * 4;
			xfc->bitmap_buffer = (BYTE*) _aligned_realloc(xfc->bitmap_buffer, xfc->bitmap_size, 16);

			if (!xfc->bitmap_buffer)
				return FALSE;
		}

		if (compressed)
		{
			pDstData = xfc->bitmap_buffer;

			if (bitsPerPixel < 32)
			{
				if (!freerdp_client_codecs_prepare(codecs, FREERDP_CODEC_INTERLEAVED))
					return FALSE;

				status = interleaved_decompress(codecs->interleaved, pSrcData, SrcSize, bitsPerPixel,
						&pDstData, xfc->format, -1, 0, 0, nWidth, nHeight, xfc->palette);
			}
			else
			{
				if (!freerdp_client_codecs_prepare(codecs, FREERDP_CODEC_PLANAR))
					return FALSE;

				status = planar_decompress(codecs->planar, pSrcData, SrcSize, &pDstData,
						xfc->format, -1, 0, 0, nWidth, nHeight, TRUE);
			}

			if (status < 0)
			{
				WLog_ERR(TAG, "bitmap decompression failure");
				return FALSE;
			}

			pSrcData = xfc->bitmap_buffer;
		}
		else
		{
			pDstData = xfc->bitmap_buffer;

			status = freerdp_image_copy(pDstData, xfc->format, -1, 0, 0,
						nWidth, nHeight, pSrcData, SrcFormat, -1, 0, 0, xfc->palette);

			pSrcData = xfc->bitmap_buffer;
		}

		xf_lock_x11(xfc, FALSE);

		XSetFunction(xfc->display, xfc->gc, GXcopy);

		image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
				(char*) pSrcData, nWidth, nHeight, xfc->scanline_pad, 0);
		if (!image)
		{
			xf_unlock_x11(xfc, FALSE);
			return FALSE;
		}

		nWidth = bitmap->destRight - bitmap->destLeft + 1; /* clip width */
		nHeight = bitmap->destBottom - bitmap->destTop + 1; /* clip height */

		XPutImage(xfc->display, xfc->primary, xfc->gc,
				image, 0, 0, nXDst, nYDst, nWidth, nHeight);

		XFree(image);

		ret = gdi_InvalidateRegion(xfc->hdc, nXDst, nYDst, nWidth, nHeight);

		xf_unlock_x11(xfc, FALSE);

		if (!ret)
			break;
	}
	return ret;
}

static BOOL xf_gdi_palette_update(rdpContext* context, PALETTE_UPDATE* palette)
{
	int index;
	PALETTE_ENTRY* pe;
	UINT32* palette32;
	xfContext* xfc = (xfContext*) context;

	xf_lock_x11(xfc, FALSE);

	palette32 = (UINT32*) xfc->palette;

	for (index = 0; index < palette->number; index++)
	{
		pe = &(palette->entries[index]);
		palette32[index] = RGB32(pe->red, pe->green, pe->blue);
	}

	xf_unlock_x11(xfc, FALSE);
	return TRUE;
}

static BOOL xf_gdi_set_bounds(rdpContext* context, rdpBounds* bounds)
{
	XRectangle clip;
	xfContext* xfc = (xfContext*) context;

	xf_lock_x11(xfc, FALSE);

	if (bounds)
	{
		clip.x = bounds->left;
		clip.y = bounds->top;
		clip.width = bounds->right - bounds->left + 1;
		clip.height = bounds->bottom - bounds->top + 1;
		XSetClipRectangles(xfc->display, xfc->gc, 0, 0, &clip, 1, YXBanded);
	}
	else
	{
		XSetClipMask(xfc->display, xfc->gc, None);
	}

	xf_unlock_x11(xfc, FALSE);
	return TRUE;
}

static BOOL xf_gdi_dstblt(rdpContext* context, DSTBLT_ORDER* dstblt)
{
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	xf_set_rop3(xfc, gdi_rop3_code(dstblt->bRop));

	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
			dstblt->nLeftRect, dstblt->nTopRect,
			dstblt->nWidth, dstblt->nHeight);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, dstblt->nLeftRect, dstblt->nTopRect, dstblt->nWidth, dstblt->nHeight);

	XSetFunction(xfc->display, xfc->gc, GXcopy);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	Pixmap pattern;
	rdpBrush* brush;
	UINT32 foreColor;
	UINT32 backColor;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	brush = &patblt->brush;
	xf_set_rop3(xfc, gdi_rop3_code(patblt->bRop));

	foreColor = xf_convert_rdp_order_color(xfc, patblt->foreColor);
	backColor = xf_convert_rdp_order_color(xfc, patblt->backColor);

	if (brush->style == GDI_BS_SOLID)
	{
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);
		XSetForeground(xfc->display, xfc->gc, foreColor);

		XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
				patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);
	}
	else if (brush->style == GDI_BS_HATCHED)
	{
		pattern = xf_mono_bitmap_new(xfc, 8, 8, GDI_BS_HATCHED_PATTERNS + 8 * brush->hatch);

		XSetForeground(xfc->display, xfc->gc, backColor);
		XSetBackground(xfc->display, xfc->gc, foreColor);
		XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
		XSetStipple(xfc->display, xfc->gc, pattern);
		XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);

		XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
		patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);

		XFreePixmap(xfc->display, pattern);
	}
	else if (brush->style == GDI_BS_PATTERN)
	{
		if (brush->bpp > 1)
		{
			pattern = xf_brush_new(xfc, 8, 8, brush->bpp, brush->data);

			XSetFillStyle(xfc->display, xfc->gc, FillTiled);
			XSetTile(xfc->display, xfc->gc, pattern);
			XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);

			XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
					patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);

			XSetTile(xfc->display, xfc->gc, xfc->primary);

			XFreePixmap(xfc->display, pattern);
		}
		else
		{
			pattern = xf_mono_bitmap_new(xfc, 8, 8, brush->data);

			XSetForeground(xfc->display, xfc->gc, backColor);
			XSetBackground(xfc->display, xfc->gc, foreColor);
			XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
			XSetStipple(xfc->display, xfc->gc, pattern);
			XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);

			XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
					patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);

			XFreePixmap(xfc->display, pattern);
		}
	}
	else
	{
		WLog_ERR(TAG,  "unimplemented brush style:%d", brush->style);
	}

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);

	XSetFunction(xfc->display, xfc->gc, GXcopy);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_scrblt(rdpContext* context, SCRBLT_ORDER* scrblt)
{
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	xf_set_rop3(xfc, gdi_rop3_code(scrblt->bRop));

	XCopyArea(xfc->display, xfc->primary, xfc->drawing, xfc->gc, scrblt->nXSrc, scrblt->nYSrc,
			scrblt->nWidth, scrblt->nHeight, scrblt->nLeftRect, scrblt->nTopRect);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, scrblt->nLeftRect, scrblt->nTopRect, scrblt->nWidth, scrblt->nHeight);

	XSetFunction(xfc->display, xfc->gc, GXcopy);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

BOOL xf_gdi_opaque_rect(rdpContext* context, OPAQUE_RECT_ORDER* opaque_rect)
{
	UINT32 color;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	color = xf_convert_rdp_order_color(xfc, opaque_rect->color);

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, color);

	XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
			opaque_rect->nLeftRect, opaque_rect->nTopRect,
			opaque_rect->nWidth, opaque_rect->nHeight);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, opaque_rect->nLeftRect, opaque_rect->nTopRect,
				opaque_rect->nWidth, opaque_rect->nHeight);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

BOOL xf_gdi_multi_opaque_rect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	int i;
	UINT32 color;
	DELTA_RECT* rectangle;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	color = xf_convert_rdp_order_color(xfc, multi_opaque_rect->color);

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, color);

	for (i = 1; i < multi_opaque_rect->numRectangles + 1; i++)
	{
		rectangle = &multi_opaque_rect->rectangles[i];

		XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
				rectangle->left, rectangle->top,
				rectangle->width, rectangle->height);

		if (xfc->drawing == xfc->primary)
		{
			if (!(ret = gdi_InvalidateRegion(xfc->hdc, rectangle->left, rectangle->top, rectangle->width, rectangle->height)))
				break;
		}
	}

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

void xf_gdi_draw_nine_grid(rdpContext* context, DRAW_NINE_GRID_ORDER* draw_nine_grid)
{
	WLog_ERR(TAG,  "DrawNineGrid");
}

BOOL xf_gdi_line_to(rdpContext* context, LINE_TO_ORDER* line_to)
{
	UINT32 color;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	xf_set_rop2(xfc, line_to->bRop2);
	color = xf_convert_rdp_order_color(xfc, line_to->penColor);

	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, color);

	XDrawLine(xfc->display, xfc->drawing, xfc->gc,
			line_to->nXStart, line_to->nYStart, line_to->nXEnd, line_to->nYEnd);

	if (xfc->drawing == xfc->primary)
	{
		int x, y, w, h;

		x = MIN(line_to->nXStart, line_to->nXEnd);
		y = MIN(line_to->nYStart, line_to->nYEnd);

		w = abs(line_to->nXEnd - line_to->nXStart) + 1;
		h = abs(line_to->nYEnd - line_to->nYStart) + 1;

		ret = gdi_InvalidateRegion(xfc->hdc, x, y, w, h);
	}

	XSetFunction(xfc->display, xfc->gc, GXcopy);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_invalidate_poly_region(xfContext* xfc, XPoint* points, int npoints)
{
	int x, y, x1, y1, x2, y2;

	if (npoints < 2)
		return FALSE;

	x = x1 = x2 = points->x;
	y = y1 = y2 = points->y;

	while (--npoints)
	{
		points++;
		x += points->x;
		y += points->y;

		if (x > x2)
			x2 = x;
		if (x < x1)
			x1 = x;
		if (y > y2)
			y2 = y;
		if (y < y1)
			y1 = y;
	}

	x2++;
	y2++;

	return gdi_InvalidateRegion(xfc->hdc, x1, y1, x2 - x1, y2 - y1);
}

BOOL xf_gdi_polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	int i;
	int npoints;
	UINT32 color;
	XPoint* points;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	xf_set_rop2(xfc, polyline->bRop2);
	color = xf_convert_rdp_order_color(xfc, polyline->penColor);

	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, color);

	npoints = polyline->numDeltaEntries + 1;
	points = malloc(sizeof(XPoint) * npoints);
	if (!points)
	{
		xf_unlock_x11(xfc, FALSE);
		return FALSE;
	}

	points[0].x = polyline->xStart;
	points[0].y = polyline->yStart;

	for (i = 0; i < polyline->numDeltaEntries; i++)
	{
		points[i + 1].x = polyline->points[i].x;
		points[i + 1].y = polyline->points[i].y;
	}

	XDrawLines(xfc->display, xfc->drawing, xfc->gc, points, npoints, CoordModePrevious);

	if (xfc->drawing == xfc->primary)
	{
		if (!xf_gdi_invalidate_poly_region(xfc, points, npoints))
			ret = FALSE;
	}

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	free(points);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

BOOL xf_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	xfBitmap* bitmap;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	bitmap = (xfBitmap*) memblt->bitmap;
	xf_set_rop3(xfc, gdi_rop3_code(memblt->bRop));

	XCopyArea(xfc->display, bitmap->pixmap, xfc->drawing, xfc->gc,
			memblt->nXSrc, memblt->nYSrc, memblt->nWidth, memblt->nHeight,
			memblt->nLeftRect, memblt->nTopRect);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, memblt->nLeftRect, memblt->nTopRect, memblt->nWidth, memblt->nHeight);

	XSetFunction(xfc->display, xfc->gc, GXcopy);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

BOOL xf_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	rdpBrush* brush;
	xfBitmap* bitmap;
	UINT32 foreColor;
	UINT32 backColor;
	Pixmap pattern = 0;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	brush = &mem3blt->brush;
	bitmap = (xfBitmap*) mem3blt->bitmap;
	xf_set_rop3(xfc, gdi_rop3_code(mem3blt->bRop));
	foreColor = xf_convert_rdp_order_color(xfc, mem3blt->foreColor);
	backColor = xf_convert_rdp_order_color(xfc, mem3blt->backColor);

	if (brush->style == GDI_BS_PATTERN)
	{
		if (brush->bpp > 1)
		{
			pattern = xf_brush_new(xfc, 8, 8, brush->bpp, brush->data);

			XSetFillStyle(xfc->display, xfc->gc, FillTiled);
			XSetTile(xfc->display, xfc->gc, pattern);
			XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);
		}
		else
		{
			pattern = xf_mono_bitmap_new(xfc, 8, 8, brush->data);

			XSetForeground(xfc->display, xfc->gc, backColor);
			XSetBackground(xfc->display, xfc->gc, foreColor);
			XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
			XSetStipple(xfc->display, xfc->gc, pattern);
			XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);
		}
	}
	else if (brush->style == GDI_BS_SOLID)
	{
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);
		XSetForeground(xfc->display, xfc->gc, backColor);
		XSetBackground(xfc->display, xfc->gc, foreColor);

		XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);
	}
	else
	{
		WLog_ERR(TAG,  "Mem3Blt unimplemented brush style:%d", brush->style);
	}

	XCopyArea(xfc->display, bitmap->pixmap, xfc->drawing, xfc->gc,
			mem3blt->nXSrc, mem3blt->nYSrc, mem3blt->nWidth, mem3blt->nHeight,
			mem3blt->nLeftRect, mem3blt->nTopRect);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, mem3blt->nLeftRect, mem3blt->nTopRect, mem3blt->nWidth, mem3blt->nHeight);

	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetTSOrigin(xfc->display, xfc->gc, 0, 0);

	if (pattern != 0)
		XFreePixmap(xfc->display, pattern);

	XSetFunction(xfc->display, xfc->gc, GXcopy);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}


BOOL xf_gdi_polygon_sc(rdpContext* context, POLYGON_SC_ORDER* polygon_sc)
{
	int i, npoints;
	XPoint* points;
	UINT32 brush_color;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	xf_set_rop2(xfc, polygon_sc->bRop2);
	brush_color = xf_convert_rdp_order_color(xfc, polygon_sc->brushColor);

	npoints = polygon_sc->numPoints + 1;
	points = malloc(sizeof(XPoint) * npoints);
	if (!points)
	{
		xf_unlock_x11(xfc, FALSE);
		return FALSE;
	}

	points[0].x = polygon_sc->xStart;
	points[0].y = polygon_sc->yStart;

	for (i = 0; i < polygon_sc->numPoints; i++)
	{
		points[i + 1].x = polygon_sc->points[i].x;
		points[i + 1].y = polygon_sc->points[i].y;
	}

	switch (polygon_sc->fillMode)
	{
		case 1: /* alternate */
			XSetFillRule(xfc->display, xfc->gc, EvenOddRule);
			break;

		case 2: /* winding */
			XSetFillRule(xfc->display, xfc->gc, WindingRule);
			break;

		default:
			WLog_ERR(TAG,  "PolygonSC unknown fillMode: %d", polygon_sc->fillMode);
			break;
	}

	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, brush_color);

	XFillPolygon(xfc->display, xfc->drawing, xfc->gc,
			points, npoints, Complex, CoordModePrevious);

	if (xfc->drawing == xfc->primary)
	{
		if (!xf_gdi_invalidate_poly_region(xfc, points, npoints))
			ret = FALSE;
	}

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	free(points);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

BOOL xf_gdi_polygon_cb(rdpContext* context, POLYGON_CB_ORDER* polygon_cb)
{
	int i, npoints;
	XPoint* points;
	Pixmap pattern;
	rdpBrush* brush;
	UINT32 foreColor;
	UINT32 backColor;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	brush = &(polygon_cb->brush);
	xf_set_rop2(xfc, polygon_cb->bRop2);
	foreColor = xf_convert_rdp_order_color(xfc, polygon_cb->foreColor);
	backColor = xf_convert_rdp_order_color(xfc, polygon_cb->backColor);

	npoints = polygon_cb->numPoints + 1;
	points = malloc(sizeof(XPoint) * npoints);
	if (!points)
	{
		xf_unlock_x11(xfc, FALSE);
		return FALSE;
	}

	points[0].x = polygon_cb->xStart;
	points[0].y = polygon_cb->yStart;

	for (i = 0; i < polygon_cb->numPoints; i++)
	{
		points[i + 1].x = polygon_cb->points[i].x;
		points[i + 1].y = polygon_cb->points[i].y;
	}

	switch (polygon_cb->fillMode)
	{
		case GDI_FILL_ALTERNATE: /* alternate */
			XSetFillRule(xfc->display, xfc->gc, EvenOddRule);
			break;

		case GDI_FILL_WINDING: /* winding */
			XSetFillRule(xfc->display, xfc->gc, WindingRule);
			break;

		default:
			WLog_ERR(TAG, "PolygonCB unknown fillMode: %d", polygon_cb->fillMode);
			break;
	}

	if (brush->style == GDI_BS_PATTERN)
	{
		if (brush->bpp > 1)
		{
			pattern = xf_brush_new(xfc, 8, 8, brush->bpp, brush->data);

			XSetFillStyle(xfc->display, xfc->gc, FillTiled);
			XSetTile(xfc->display, xfc->gc, pattern);
		}
		else
		{
			pattern = xf_mono_bitmap_new(xfc, 8, 8, brush->data);

			XSetForeground(xfc->display, xfc->gc, backColor);
			XSetBackground(xfc->display, xfc->gc, foreColor);

			if (polygon_cb->backMode == BACKMODE_TRANSPARENT)
				XSetFillStyle(xfc->display, xfc->gc, FillStippled);
			else if (polygon_cb->backMode == BACKMODE_OPAQUE)
				XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);

			XSetStipple(xfc->display, xfc->gc, pattern);
		}

		XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);

		XFillPolygon(xfc->display, xfc->drawing, xfc->gc,
				points, npoints, Complex, CoordModePrevious);

		XSetFillStyle(xfc->display, xfc->gc, FillSolid);
		XSetTSOrigin(xfc->display, xfc->gc, 0, 0);
		XFreePixmap(xfc->display, pattern);

		if (xfc->drawing == xfc->primary)
		{
			if (!xf_gdi_invalidate_poly_region(xfc, points, npoints))
				ret = FALSE;
		}
	}
	else
	{
		WLog_ERR(TAG,  "PolygonCB unimplemented brush style:%d", brush->style);
	}

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	free(points);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

BOOL xf_gdi_ellipse_sc(rdpContext* context, ELLIPSE_SC_ORDER* ellipse_sc)
{
	WLog_ERR(TAG,  "Not implemented: EllipseSC");
	return TRUE;
}

BOOL xf_gdi_ellipse_cb(rdpContext* context, ELLIPSE_CB_ORDER* ellipse_cb)
{
	WLog_ERR(TAG,  "Not implemented: EllipseCB");
	return TRUE;
}

BOOL xf_gdi_frame_marker(rdpContext* context, FRAME_MARKER_ORDER* frameMarker)
{
	return TRUE;
}

BOOL xf_gdi_surface_frame_marker(rdpContext* context, SURFACE_FRAME_MARKER* surface_frame_marker)
{
	rdpSettings* settings;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	settings = xfc->instance->settings;


	xf_lock_x11(xfc, FALSE);

	switch (surface_frame_marker->frameAction)
	{
		case SURFACECMD_FRAMEACTION_BEGIN:
			xfc->frame_begin = TRUE;
			xfc->frame_x1 = 0;
			xfc->frame_y1 = 0;
			xfc->frame_x2 = 0;
			xfc->frame_y2 = 0;
			break;

		case SURFACECMD_FRAMEACTION_END:
			xfc->frame_begin = FALSE;
			if ((xfc->frame_x2 > xfc->frame_x1) && (xfc->frame_y2 > xfc->frame_y1))
				ret = gdi_InvalidateRegion(xfc->hdc, xfc->frame_x1, xfc->frame_y1,
					xfc->frame_x2 - xfc->frame_x1, xfc->frame_y2 - xfc->frame_y1);
			if (settings->FrameAcknowledge > 0)
			{
				IFCALL(xfc->instance->update->SurfaceFrameAcknowledge, context, surface_frame_marker->frameId);
			}
			break;
	}

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_surface_update_frame(xfContext* xfc, UINT16 tx, UINT16 ty, UINT16 width, UINT16 height)
{
	BOOL ret = TRUE;
	if (!xfc->remote_app)
	{
		if (xfc->frame_begin)
		{
			if (xfc->frame_x2 > xfc->frame_x1 && xfc->frame_y2 > xfc->frame_y1)
			{
				xfc->frame_x1 = MIN(xfc->frame_x1, tx);
				xfc->frame_y1 = MIN(xfc->frame_y1, ty);
				xfc->frame_x2 = MAX(xfc->frame_x2, tx + width);
				xfc->frame_y2 = MAX(xfc->frame_y2, ty + height);
			}
			else
			{
				xfc->frame_x1 = tx;
				xfc->frame_y1 = ty;
				xfc->frame_x2 = tx + width;
				xfc->frame_y2 = ty + height;
			}
		}
		else
		{
			ret = gdi_InvalidateRegion(xfc->hdc, tx, ty, width, height);
		}
	}
	else
	{
		ret = gdi_InvalidateRegion(xfc->hdc, tx, ty, width, height);
	}
	return ret;
}

BOOL xf_gdi_surface_bits(rdpContext* context, SURFACE_BITS_COMMAND* cmd)
{
	int i, tx, ty;
	XImage* image;
	BYTE* pSrcData;
	BYTE* pDstData;
	RFX_MESSAGE* message;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	xf_lock_x11(xfc, FALSE);

	if (cmd->codecID == RDP_CODEC_ID_REMOTEFX)
	{
		if (!freerdp_client_codecs_prepare(xfc->codecs, FREERDP_CODEC_REMOTEFX))
		{
			xf_unlock_x11(xfc, FALSE);
			return FALSE;
		}

		if (!(message = rfx_process_message(xfc->codecs->rfx, cmd->bitmapData, cmd->bitmapDataLength)))
		{
			WLog_ERR(TAG, "Failed to process RemoteFX message");
			xf_unlock_x11(xfc, FALSE);
			return FALSE;
		}

		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);

		XSetClipRectangles(xfc->display, xfc->gc, cmd->destLeft, cmd->destTop,
				(XRectangle*) message->rects, message->numRects, YXBanded);

		if (xfc->bitmap_size < (64 * 64 * 4))
		{
			xfc->bitmap_size = 64 * 64 * 4;
			xfc->bitmap_buffer = (BYTE*) _aligned_realloc(xfc->bitmap_buffer, xfc->bitmap_size, 16);

			if (!xfc->bitmap_buffer)
			{
				rfx_message_free(xfc->codecs->rfx, message);
				XSetClipMask(xfc->display, xfc->gc, None);
				xf_unlock_x11(xfc, FALSE);
				return FALSE;
			}
		}

		/* Draw the tiles to primary surface, each is 64x64. */
		for (i = 0; i < message->numTiles; i++)
		{
			pSrcData = message->tiles[i]->data;
			pDstData = pSrcData;

			if ((xfc->depth != 24) || (xfc->depth != 32))
			{
				pDstData = xfc->bitmap_buffer;

				freerdp_image_copy(pDstData, xfc->format, -1, 0, 0,
						64, 64, pSrcData, PIXEL_FORMAT_XRGB32, -1, 0, 0, xfc->palette);
			}

			image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
				(char*) pDstData, 64, 64, xfc->scanline_pad, 0);

			tx = message->tiles[i]->x + cmd->destLeft;
			ty = message->tiles[i]->y + cmd->destTop;

			XPutImage(xfc->display, xfc->primary, xfc->gc, image, 0, 0, tx, ty, 64, 64);
			XFree(image);
		}

		/* Invalidate the updated region */
		for (i = 0; i < message->numRects; i++)
		{
			tx = message->rects[i].x + cmd->destLeft;
			ty = message->rects[i].y + cmd->destTop;

			if (!xf_gdi_surface_update_frame(xfc, tx, ty, message->rects[i].width, message->rects[i].height))
			{
				ret = FALSE;
				break;
			}
		}

		XSetClipMask(xfc->display, xfc->gc, None);
		rfx_message_free(xfc->codecs->rfx, message);
	}
	else if (cmd->codecID == RDP_CODEC_ID_NSCODEC)
	{
		if (!freerdp_client_codecs_prepare(xfc->codecs, FREERDP_CODEC_NSCODEC))
		{
			xf_unlock_x11(xfc, FALSE);
			return FALSE;
		}

		nsc_process_message(xfc->codecs->nsc, cmd->bpp, cmd->width, cmd->height, cmd->bitmapData, cmd->bitmapDataLength);

		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);

		if (xfc->bitmap_size < (cmd->width * cmd->height * 4))
		{
			xfc->bitmap_size = cmd->width * cmd->height * 4;
			xfc->bitmap_buffer = (BYTE*) _aligned_realloc(xfc->bitmap_buffer, xfc->bitmap_size, 16);

			if (!xfc->bitmap_buffer)
			{
				xf_unlock_x11(xfc, FALSE);
				return FALSE;
			}
		}

		pSrcData = xfc->codecs->nsc->BitmapData;
		pDstData = xfc->bitmap_buffer;

		freerdp_image_copy(pDstData, xfc->format, -1, 0, 0,
					cmd->width, cmd->height, pSrcData, PIXEL_FORMAT_XRGB32_VF, -1, 0, 0, xfc->palette);

		image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
				(char*) pDstData, cmd->width, cmd->height, xfc->scanline_pad, 0);

		XPutImage(xfc->display, xfc->primary, xfc->gc, image, 0, 0,
				cmd->destLeft, cmd->destTop, cmd->width, cmd->height);

		XFree(image);

		ret = xf_gdi_surface_update_frame(xfc, cmd->destLeft, cmd->destTop, cmd->width, cmd->height);

		XSetClipMask(xfc->display, xfc->gc, None);
	}
	else if (cmd->codecID == RDP_CODEC_ID_NONE)
	{
		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);

		if (xfc->bitmap_size < (cmd->width * cmd->height * 4))
		{
			xfc->bitmap_size = cmd->width * cmd->height * 4;
			xfc->bitmap_buffer = (BYTE*) _aligned_realloc(xfc->bitmap_buffer, xfc->bitmap_size, 16);

			if (!xfc->bitmap_buffer)
			{
				xf_unlock_x11(xfc, FALSE);
				return FALSE;
			}
		}

		pSrcData = cmd->bitmapData;
		pDstData = xfc->bitmap_buffer;

		freerdp_image_copy(pDstData, xfc->format, -1, 0, 0,
				cmd->width, cmd->height, pSrcData, PIXEL_FORMAT_XRGB32_VF, -1, 0, 0, xfc->palette);

		image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
			(char*) pDstData, cmd->width, cmd->height, xfc->scanline_pad, 0);

		XPutImage(xfc->display, xfc->primary, xfc->gc, image, 0, 0,
				cmd->destLeft, cmd->destTop,
				cmd->width, cmd->height);
		XFree(image);

		ret = xf_gdi_surface_update_frame(xfc, cmd->destLeft, cmd->destTop, cmd->width, cmd->height);

		XSetClipMask(xfc->display, xfc->gc, None);
	}
	else
	{
		WLog_ERR(TAG, "Unsupported codecID %d", cmd->codecID);
	}

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

void xf_gdi_register_update_callbacks(rdpUpdate* update)
{
	rdpPrimaryUpdate* primary = update->primary;

	update->Palette = xf_gdi_palette_update;
	update->SetBounds = xf_gdi_set_bounds;

	primary->DstBlt = xf_gdi_dstblt;
	primary->PatBlt = xf_gdi_patblt;
	primary->ScrBlt = xf_gdi_scrblt;
	primary->OpaqueRect = xf_gdi_opaque_rect;
	primary->DrawNineGrid = NULL;
	primary->MultiDstBlt = NULL;
	primary->MultiPatBlt = NULL;
	primary->MultiScrBlt = NULL;
	primary->MultiOpaqueRect = xf_gdi_multi_opaque_rect;
	primary->MultiDrawNineGrid = NULL;
	primary->LineTo = xf_gdi_line_to;
	primary->Polyline = xf_gdi_polyline;
	primary->MemBlt = xf_gdi_memblt;
	primary->Mem3Blt = xf_gdi_mem3blt;
	primary->SaveBitmap = NULL;
	primary->GlyphIndex = NULL;
	primary->FastIndex = NULL;
	primary->FastGlyph = NULL;
	primary->PolygonSC = xf_gdi_polygon_sc;
	primary->PolygonCB = xf_gdi_polygon_cb;
	primary->EllipseSC = xf_gdi_ellipse_sc;
	primary->EllipseCB = xf_gdi_ellipse_cb;

	update->SurfaceBits = xf_gdi_surface_bits;
	update->SurfaceFrameMarker = xf_gdi_surface_frame_marker;

	update->altsec->FrameMarker = xf_gdi_frame_marker;
}

