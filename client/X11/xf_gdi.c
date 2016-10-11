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
#include "xf_graphics.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

static const UINT8 GDI_BS_HATCHED_PATTERNS[] =
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

static BOOL xf_set_rop2(xfContext* xfc, int rop2)
{
	if ((rop2 < 0x01) || (rop2 > 0x10))
	{
		WLog_ERR(TAG,  "Unsupported ROP2: %d", rop2);
		return FALSE;
	}

	XSetFunction(xfc->display, xfc->gc, xf_rop2_table[rop2]);
	return TRUE;
}

static BOOL xf_set_rop3(xfContext* xfc, UINT32 rop3)
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

		case GDI_DSTCOPY:
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

static Pixmap xf_brush_new(xfContext* xfc, UINT32 width, UINT32 height,
                           UINT32 bpp,
                           BYTE* data)
{
	GC gc;
	Pixmap bitmap;
	BYTE* cdata;
	XImage* image;
	rdpGdi* gdi;
	UINT32 brushFormat;
	gdi = xfc->context.gdi;
	bitmap = XCreatePixmap(xfc->display, xfc->drawable, width, height, xfc->depth);

	if (data)
	{
		brushFormat = gdi_get_pixel_format(bpp, FALSE);
		cdata = (BYTE*) _aligned_malloc(width * height * 4, 16);
		freerdp_image_copy(cdata, gdi->dstFormat, 0, 0, 0,
		                   width, height, data, brushFormat, 0, 0, 0,
		                   &xfc->context.gdi->palette);
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

static Pixmap xf_mono_bitmap_new(xfContext* xfc, int width, int height,
                                 const BYTE* data)
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

static BOOL xf_gdi_set_bounds(rdpContext* context,
                              const rdpBounds* bounds)
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

static BOOL xf_gdi_dstblt(rdpContext* context, const DSTBLT_ORDER* dstblt)
{
	xfContext* xfc = (xfContext*) context;
	BOOL ret = FALSE;
	xf_lock_x11(xfc, FALSE);

	if (!xf_set_rop3(xfc, gdi_rop3_code(dstblt->bRop)))
		goto fail;

	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
	               dstblt->nLeftRect, dstblt->nTopRect,
	               dstblt->nWidth, dstblt->nHeight);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, dstblt->nLeftRect, dstblt->nTopRect,
		                           dstblt->nWidth, dstblt->nHeight);

	ret = TRUE;
fail:
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	const rdpBrush* brush;
	UINT32 foreColor;
	UINT32 backColor;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = FALSE;

	if (!xf_decode_color(context->gdi, patblt->foreColor, &foreColor, NULL))
		return FALSE;

	if (!xf_decode_color(context->gdi, patblt->backColor, &backColor, NULL))
		return FALSE;

	xf_lock_x11(xfc, FALSE);
	brush = &patblt->brush;

	if (!xf_set_rop3(xfc, gdi_rop3_code(patblt->bRop)))
		goto fail;

	switch (brush->style)
	{
		case GDI_BS_SOLID:
			XSetFillStyle(xfc->display, xfc->gc, FillSolid);
			XSetBackground(xfc->display, xfc->gc, backColor);
			XSetForeground(xfc->display, xfc->gc, foreColor);
			XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
			               patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);
			break;

		case GDI_BS_HATCHED:
			{
				Pixmap pattern = xf_mono_bitmap_new(xfc, 8, 8,
				                                    &GDI_BS_HATCHED_PATTERNS[8 * brush->hatch]);
				XSetBackground(xfc->display, xfc->gc, backColor);
				XSetForeground(xfc->display, xfc->gc, foreColor);
				XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
				XSetStipple(xfc->display, xfc->gc, pattern);
				XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);
				XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
				               patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);
				XFreePixmap(xfc->display, pattern);
			}
			break;

		case GDI_BS_PATTERN:
			if (brush->bpp > 1)
			{
				Pixmap pattern = xf_brush_new(xfc, 8, 8, brush->bpp, brush->data);
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
				Pixmap pattern = xf_mono_bitmap_new(xfc, 8, 8, brush->data);
				XSetBackground(xfc->display, xfc->gc, backColor);
				XSetForeground(xfc->display, xfc->gc, foreColor);
				XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
				XSetStipple(xfc->display, xfc->gc, pattern);
				XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);
				XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
				               patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);
				XFreePixmap(xfc->display, pattern);
			}

			break;

		default:
			WLog_ERR(TAG,  "unimplemented brush style:%d", brush->style);
			goto fail;
	}

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, patblt->nLeftRect, patblt->nTopRect,
		                           patblt->nWidth, patblt->nHeight);

	ret = TRUE;
fail:
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_scrblt(rdpContext* context, const SCRBLT_ORDER* scrblt)
{
	xfContext* xfc = (xfContext*) context;
	BOOL ret = FALSE;

	if (!xfc->display || !xfc->drawing)
		return FALSE;

	xf_lock_x11(xfc, FALSE);

	if (!xf_set_rop3(xfc, gdi_rop3_code(scrblt->bRop)))
		goto fail;

	XCopyArea(xfc->display, xfc->primary, xfc->drawing, xfc->gc, scrblt->nXSrc,
	          scrblt->nYSrc,
	          scrblt->nWidth, scrblt->nHeight, scrblt->nLeftRect, scrblt->nTopRect);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, scrblt->nLeftRect, scrblt->nTopRect,
		                           scrblt->nWidth, scrblt->nHeight);

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	ret = TRUE;
fail:
	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_opaque_rect(rdpContext* context,
                               const OPAQUE_RECT_ORDER* opaque_rect)
{
	UINT32 color;
	rdpGdi* gdi = context->gdi;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	if (!xf_decode_color(gdi, opaque_rect->color, &color, NULL))
		return FALSE;

	xf_lock_x11(xfc, FALSE);
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, color);
	XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
	               opaque_rect->nLeftRect, opaque_rect->nTopRect,
	               opaque_rect->nWidth, opaque_rect->nHeight);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, opaque_rect->nLeftRect,
		                           opaque_rect->nTopRect,
		                           opaque_rect->nWidth, opaque_rect->nHeight);

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_multi_opaque_rect(rdpContext* context,
                                     const MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	UINT32 i;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;
	rdpGdi* gdi = context->gdi;
	UINT32 color;

	if (!xf_decode_color(gdi, multi_opaque_rect->color, &color, NULL))
		return FALSE;

	xf_lock_x11(xfc, FALSE);
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, color);

	for (i = 0; i < multi_opaque_rect->numRectangles; i++)
	{
		const DELTA_RECT* rectangle = &multi_opaque_rect->rectangles[i];
		XFillRectangle(xfc->display, xfc->drawing, xfc->gc,
		               rectangle->left, rectangle->top,
		               rectangle->width, rectangle->height);

		if (xfc->drawing == xfc->primary)
		{
			if (!(ret = gdi_InvalidateRegion(xfc->hdc, rectangle->left, rectangle->top,
			                                 rectangle->width, rectangle->height)))
				break;
		}
	}

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_line_to(rdpContext* context, const LINE_TO_ORDER* line_to)
{
	UINT32 color;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	if (!xf_decode_color(context->gdi, line_to->penColor, &color, NULL))
		return FALSE;

	xf_lock_x11(xfc, FALSE);
	xf_set_rop2(xfc, line_to->bRop2);
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

static BOOL xf_gdi_invalidate_poly_region(xfContext* xfc, XPoint* points,
        int npoints)
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

static BOOL xf_gdi_polyline(rdpContext* context,
                            const POLYLINE_ORDER* polyline)
{
	int i;
	int npoints;
	UINT32 color;
	XPoint* points;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	if (!xf_decode_color(context->gdi, polyline->penColor, &color, NULL))
		return FALSE;

	xf_lock_x11(xfc, FALSE);
	xf_set_rop2(xfc, polyline->bRop2);
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

	XDrawLines(xfc->display, xfc->drawing, xfc->gc, points, npoints,
	           CoordModePrevious);

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

static BOOL xf_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	xfBitmap* bitmap;
	xfContext* xfc;
	BOOL ret = TRUE;

	if (!context || !memblt)
		return FALSE;

	bitmap = (xfBitmap*) memblt->bitmap;
	xfc = (xfContext*) context;

	if (!bitmap || !xfc || !xfc->display || !xfc->drawing)
		return FALSE;

	xf_lock_x11(xfc, FALSE);

	if (xf_set_rop3(xfc, gdi_rop3_code(memblt->bRop)))
	{
		XCopyArea(xfc->display, bitmap->pixmap, xfc->drawing, xfc->gc,
		          memblt->nXSrc, memblt->nYSrc, memblt->nWidth, memblt->nHeight,
		          memblt->nLeftRect, memblt->nTopRect);

		if (xfc->drawing == xfc->primary)
			ret = gdi_InvalidateRegion(xfc->hdc, memblt->nLeftRect,
			                           memblt->nTopRect, memblt->nWidth,
			                           memblt->nHeight);
	}

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	const rdpBrush* brush;
	xfBitmap* bitmap;
	UINT32 foreColor;
	UINT32 backColor;
	Pixmap pattern = 0;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = FALSE;

	if (!xfc->display || !xfc->drawing)
		return FALSE;

	if (!xf_decode_color(context->gdi, mem3blt->foreColor, &foreColor, NULL))
		return FALSE;

	if (!xf_decode_color(context->gdi, mem3blt->backColor, &backColor, NULL))
		return FALSE;

	xf_lock_x11(xfc, FALSE);
	brush = &mem3blt->brush;
	bitmap = (xfBitmap*) mem3blt->bitmap;

	if (!xf_set_rop3(xfc, gdi_rop3_code(mem3blt->bRop)))
		goto fail;

	switch (brush->style)
	{
		case GDI_BS_PATTERN:
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
				XSetBackground(xfc->display, xfc->gc, backColor);
				XSetForeground(xfc->display, xfc->gc, foreColor);
				XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
				XSetStipple(xfc->display, xfc->gc, pattern);
				XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);
			}

			break;

		case GDI_BS_SOLID:
			XSetFillStyle(xfc->display, xfc->gc, FillSolid);
			XSetBackground(xfc->display, xfc->gc, backColor);
			XSetForeground(xfc->display, xfc->gc, foreColor);
			XSetTSOrigin(xfc->display, xfc->gc, brush->x, brush->y);
			break;

		default:
			WLog_ERR(TAG,  "Mem3Blt unimplemented brush style:%d", brush->style);
			goto fail;
	}

	XCopyArea(xfc->display, bitmap->pixmap, xfc->drawing, xfc->gc,
	          mem3blt->nXSrc, mem3blt->nYSrc, mem3blt->nWidth, mem3blt->nHeight,
	          mem3blt->nLeftRect, mem3blt->nTopRect);

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, mem3blt->nLeftRect, mem3blt->nTopRect,
		                           mem3blt->nWidth, mem3blt->nHeight);

	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetTSOrigin(xfc->display, xfc->gc, 0, 0);

	if (pattern != 0)
		XFreePixmap(xfc->display, pattern);

	ret = TRUE;
fail:
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	xf_unlock_x11(xfc, FALSE);
	return ret;
}


static BOOL xf_gdi_polygon_sc(rdpContext* context,
                              const POLYGON_SC_ORDER* polygon_sc)
{
	int i, npoints;
	XPoint* points;
	UINT32 brush_color;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	if (!xf_decode_color(context->gdi, polygon_sc->brushColor, &brush_color, NULL))
		return FALSE;

	xf_lock_x11(xfc, FALSE);
	xf_set_rop2(xfc, polygon_sc->bRop2);
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

static BOOL xf_gdi_polygon_cb(rdpContext* context,
                              POLYGON_CB_ORDER* polygon_cb)
{
	int i, npoints;
	XPoint* points;
	Pixmap pattern;
	const rdpBrush* brush;
	UINT32 foreColor;
	UINT32 backColor;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	if (!xf_decode_color(context->gdi, polygon_cb->foreColor, &foreColor, NULL))
		return FALSE;

	if (!xf_decode_color(context->gdi, polygon_cb->backColor, &backColor, NULL))
		return FALSE;

	xf_lock_x11(xfc, FALSE);
	brush = &(polygon_cb->brush);
	xf_set_rop2(xfc, polygon_cb->bRop2);
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

static BOOL xf_gdi_surface_frame_marker(rdpContext* context,
                                        const SURFACE_FRAME_MARKER* surface_frame_marker)
{
	rdpSettings* settings;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;
	settings = xfc->context.settings;
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
				IFCALL(xfc->context.update->SurfaceFrameAcknowledge, context,
				       surface_frame_marker->frameId);
			}

			break;
	}

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

static BOOL xf_gdi_surface_update_frame(xfContext* xfc, UINT16 tx, UINT16 ty,
                                        UINT16 width, UINT16 height)
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

static BOOL xf_gdi_surface_bits(rdpContext* context,
                                const SURFACE_BITS_COMMAND* cmd)
{
	XImage* image;
	BYTE* pSrcData;
	BYTE* pDstData;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;
	rdpGdi* gdi = context->gdi;
	REGION16 invalidRegion;
	xf_lock_x11(xfc, FALSE);

	switch (cmd->codecID)
	{
		case RDP_CODEC_ID_REMOTEFX:
			if (!rfx_process_message(context->codecs->rfx, cmd->bitmapData,
			                         PIXEL_FORMAT_XRGB32, cmd->bitmapDataLength,
			                         cmd->destLeft, cmd->destTop,
			                         gdi->primary_buffer, gdi->dstFormat, gdi->stride,
			                         gdi->height, &invalidRegion))
			{
				WLog_ERR(TAG, "Failed to process RemoteFX message");
				xf_unlock_x11(xfc, FALSE);
				return FALSE;
			}

			XRectangle rect;
			rect.x = cmd->destLeft;
			rect.y = cmd->destTop;
			rect.width = cmd->width;
			rect.height = cmd->height;
			XSetFunction(xfc->display, xfc->gc, GXcopy);
			XSetFillStyle(xfc->display, xfc->gc, FillSolid);
			XSetClipRectangles(xfc->display, xfc->gc, cmd->destLeft, cmd->destTop,
			                   &rect, 1, YXBanded);

			/* Invalidate the updated region */
			if (!xf_gdi_surface_update_frame(xfc, rect.x, rect.y,
			                                 rect.width, rect.height))
				ret = FALSE;

			XSetClipMask(xfc->display, xfc->gc, None);
			break;

		case RDP_CODEC_ID_NSCODEC:
			if (!nsc_process_message(context->codecs->nsc, cmd->bpp, cmd->width,
			                         cmd->height,
			                         cmd->bitmapData, cmd->bitmapDataLength,
			                         gdi->primary_buffer, gdi->dstFormat, 0, 0, 0, cmd->width, cmd->height))
			{
				xf_unlock_x11(xfc, FALSE);
				return FALSE;
			}

			XSetFunction(xfc->display, xfc->gc, GXcopy);
			XSetFillStyle(xfc->display, xfc->gc, FillSolid);
			pDstData = gdi->primary_buffer;
			image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
			                     (char*) pDstData, cmd->width, cmd->height, xfc->scanline_pad, 0);
			XPutImage(xfc->display, xfc->primary, xfc->gc, image, 0, 0,
			          cmd->destLeft, cmd->destTop, cmd->width, cmd->height);
			XFree(image);
			ret = xf_gdi_surface_update_frame(xfc, cmd->destLeft, cmd->destTop, cmd->width,
			                                  cmd->height);
			XSetClipMask(xfc->display, xfc->gc, None);
			break;

		case RDP_CODEC_ID_NONE:
			XSetFunction(xfc->display, xfc->gc, GXcopy);
			XSetFillStyle(xfc->display, xfc->gc, FillSolid);
			pSrcData = cmd->bitmapData;
			pDstData = gdi->primary_buffer;
			freerdp_image_copy(pDstData, gdi->dstFormat, 0, 0, 0,
			                   cmd->width, cmd->height, pSrcData,
			                   PIXEL_FORMAT_BGRX32_VF, 0, 0, 0, &xfc->context.gdi->palette);
			image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
			                     (char*) pDstData, cmd->width, cmd->height, xfc->scanline_pad, 0);
			XPutImage(xfc->display, xfc->primary, xfc->gc, image, 0, 0,
			          cmd->destLeft, cmd->destTop,
			          cmd->width, cmd->height);
			XFree(image);
			ret = xf_gdi_surface_update_frame(xfc, cmd->destLeft, cmd->destTop, cmd->width,
			                                  cmd->height);
			XSetClipMask(xfc->display, xfc->gc, None);
			break;

		default:
			WLog_ERR(TAG, "Unsupported codecID %d", cmd->codecID);
	}

	xf_unlock_x11(xfc, FALSE);
	return ret;
}

void xf_gdi_register_update_callbacks(rdpUpdate* update)
{
	rdpPrimaryUpdate* primary = update->primary;
	update->SetBounds = xf_gdi_set_bounds;
	primary->DstBlt = xf_gdi_dstblt;
	primary->PatBlt = xf_gdi_patblt;
	primary->ScrBlt = xf_gdi_scrblt;
	primary->OpaqueRect = xf_gdi_opaque_rect;
	primary->MultiOpaqueRect = xf_gdi_multi_opaque_rect;
	primary->LineTo = xf_gdi_line_to;
	primary->Polyline = xf_gdi_polyline;
	primary->MemBlt = xf_gdi_memblt;
	primary->Mem3Blt = xf_gdi_mem3blt;
	primary->PolygonSC = xf_gdi_polygon_sc;
	primary->PolygonCB = xf_gdi_polygon_cb;
	update->SurfaceBits = xf_gdi_surface_bits;
	update->SurfaceFrameMarker = xf_gdi_surface_frame_marker;
}

