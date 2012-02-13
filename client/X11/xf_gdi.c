/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 GDI
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>

#include "xf_gdi.h"

static const uint8 xf_rop2_table[] =
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

boolean xf_set_rop2(xfInfo* xfi, int rop2)
{
	if ((rop2 < 0x01) || (rop2 > 0x10))
	{
		printf("Unsupported ROP2: %d\n", rop2);
		return false;
	}

	XSetFunction(xfi->display, xfi->gc, xf_rop2_table[rop2]);
	return true;
}

boolean xf_set_rop3(xfInfo* xfi, int rop3)
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
		printf("Unsupported ROP3: 0x%08X\n", rop3);
		XSetFunction(xfi->display, xfi->gc, GXclear);
		return false;
	}

	XSetFunction(xfi->display, xfi->gc, function);

	return true;
}

Pixmap xf_brush_new(xfInfo* xfi, int width, int height, int bpp, uint8* data)
{
	Pixmap bitmap;
	uint8* cdata;
	XImage* image;

	bitmap = XCreatePixmap(xfi->display, xfi->drawable, width, height, xfi->depth);

	if (data != NULL)
	{
		GC gc;

		cdata = freerdp_image_convert(data, NULL, width, height, bpp, xfi->bpp, xfi->clrconv);

		image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
						ZPixmap, 0, (char*) cdata, width, height, xfi->scanline_pad, 0);

		gc = XCreateGC(xfi->display, xfi->drawable, 0, NULL);
		XPutImage(xfi->display, bitmap, gc, image, 0, 0, 0, 0, width, height);
		XFree(image);

		if (cdata != data)
			xfree(cdata);

		XFreeGC(xfi->display, gc);
	}

	return bitmap;
}

Pixmap xf_mono_bitmap_new(xfInfo* xfi, int width, int height, uint8* data)
{
	int scanline;
	XImage* image;
	Pixmap bitmap;

	scanline = (width + 7) / 8;

	bitmap = XCreatePixmap(xfi->display, xfi->drawable, width, height, 1);

	image = XCreateImage(xfi->display, xfi->visual, 1,
			ZPixmap, 0, (char*) data, width, height, 8, scanline);

	XPutImage(xfi->display, bitmap, xfi->gc_mono, image, 0, 0, 0, 0, width, height);
	XFree(image);

	return bitmap;
}

Pixmap xf_glyph_new(xfInfo* xfi, int width, int height, uint8* data)
{
	int scanline;
	Pixmap bitmap;
	XImage* image;

	scanline = (width + 7) / 8;

	bitmap = XCreatePixmap(xfi->display, xfi->drawable, width, height, 1);

	image = XCreateImage(xfi->display, xfi->visual, 1,
			ZPixmap, 0, (char*) data, width, height, 8, scanline);

	image->byte_order = MSBFirst;
	image->bitmap_bit_order = MSBFirst;

	XInitImage(image);
	XPutImage(xfi->display, bitmap, xfi->gc_mono, image, 0, 0, 0, 0, width, height);
	XFree(image);

	return bitmap;
}

void xf_gdi_palette_update(rdpContext* context, PALETTE_UPDATE* palette)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;
	xfi->clrconv->palette->count = palette->number;
	xfi->clrconv->palette->entries = palette->entries;
}

void xf_gdi_set_bounds(rdpContext* context, rdpBounds* bounds)
{
	XRectangle clip;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	if (bounds != NULL)
	{
		clip.x = bounds->left;
		clip.y = bounds->top;
		clip.width = bounds->right - bounds->left + 1;
		clip.height = bounds->bottom - bounds->top + 1;
		XSetClipRectangles(xfi->display, xfi->gc, 0, 0, &clip, 1, YXBanded);
	}
	else
	{
		XSetClipMask(xfi->display, xfi->gc, None);
	}
}

void xf_gdi_dstblt(rdpContext* context, DSTBLT_ORDER* dstblt)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_set_rop3(xfi, gdi_rop3_code(dstblt->bRop));

	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
			dstblt->nLeftRect, dstblt->nTopRect,
			dstblt->nWidth, dstblt->nHeight);

	if (xfi->drawing == xfi->primary)
	{
		if (xfi->remote_app != true)
		{
			XFillRectangle(xfi->display, xfi->drawable, xfi->gc,
				dstblt->nLeftRect, dstblt->nTopRect, dstblt->nWidth, dstblt->nHeight);
		}

		gdi_InvalidateRegion(xfi->hdc, dstblt->nLeftRect, dstblt->nTopRect, dstblt->nWidth, dstblt->nHeight);
	}
	XSetFunction(xfi->display, xfi->gc, GXcopy);
}

void xf_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	Pixmap pattern;
	rdpBrush* brush;
	uint32 foreColor;
	uint32 backColor;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	brush = &patblt->brush;
	xf_set_rop3(xfi, gdi_rop3_code(patblt->bRop));

	foreColor = freerdp_color_convert_rgb(patblt->foreColor, xfi->srcBpp, 32, xfi->clrconv);
	backColor = freerdp_color_convert_rgb(patblt->backColor, xfi->srcBpp, 32, xfi->clrconv);

	if (brush->style == GDI_BS_SOLID)
	{
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);
		XSetForeground(xfi->display, xfi->gc, foreColor);

		XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
				patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);
	}
	else if (brush->style == GDI_BS_PATTERN)
	{
		if (brush->bpp > 1)
		{
			pattern = xf_brush_new(xfi, 8, 8, brush->bpp, brush->data);

			XSetFillStyle(xfi->display, xfi->gc, FillTiled);
			XSetTile(xfi->display, xfi->gc, pattern);
			XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);

			XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
					patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);

			XSetTile(xfi->display, xfi->gc, xfi->primary);

			XFreePixmap(xfi->display, pattern);
		}
		else
		{
			pattern = xf_mono_bitmap_new(xfi, 8, 8, brush->data);

			XSetForeground(xfi->display, xfi->gc, backColor);
			XSetBackground(xfi->display, xfi->gc, foreColor);
			XSetFillStyle(xfi->display, xfi->gc, FillOpaqueStippled);
			XSetStipple(xfi->display, xfi->gc, pattern);
			XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);

			XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
					patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);

			XFreePixmap(xfi->display, pattern);
		}
	}
	else
	{
		printf("unimplemented brush style:%d\n", brush->style);
	}

	if (xfi->drawing == xfi->primary)
	{
		XSetFunction(xfi->display, xfi->gc, GXcopy);

		if (xfi->remote_app != true)
		{
			XCopyArea(xfi->display, xfi->primary, xfi->drawable, xfi->gc, patblt->nLeftRect, patblt->nTopRect,
				patblt->nWidth, patblt->nHeight, patblt->nLeftRect, patblt->nTopRect);
		}

		gdi_InvalidateRegion(xfi->hdc, patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);
	}

	XSetFunction(xfi->display, xfi->gc, GXcopy);
}

void xf_gdi_scrblt(rdpContext* context, SCRBLT_ORDER* scrblt)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_set_rop3(xfi, gdi_rop3_code(scrblt->bRop));

	XCopyArea(xfi->display, xfi->primary, xfi->drawing, xfi->gc, scrblt->nXSrc, scrblt->nYSrc,
			scrblt->nWidth, scrblt->nHeight, scrblt->nLeftRect, scrblt->nTopRect);

	if (xfi->drawing == xfi->primary)
	{
		if (xfi->remote_app != true)
		{
			if (xfi->unobscured)
			{
				XCopyArea(xfi->display, xfi->drawable, xfi->drawable, xfi->gc,
						scrblt->nXSrc, scrblt->nYSrc, scrblt->nWidth, scrblt->nHeight,
						scrblt->nLeftRect, scrblt->nTopRect);
			}
			else
			{
				XSetFunction(xfi->display, xfi->gc, GXcopy);
				XCopyArea(xfi->display, xfi->primary, xfi->drawable, xfi->gc,
						scrblt->nLeftRect, scrblt->nTopRect, scrblt->nWidth, scrblt->nHeight,
						scrblt->nLeftRect, scrblt->nTopRect);
			}
		}

		gdi_InvalidateRegion(xfi->hdc, scrblt->nLeftRect, scrblt->nTopRect, scrblt->nWidth, scrblt->nHeight);
	}

	XSetFunction(xfi->display, xfi->gc, GXcopy);
}

void xf_gdi_opaque_rect(rdpContext* context, OPAQUE_RECT_ORDER* opaque_rect)
{
	uint32 color;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	color = freerdp_color_convert_var(opaque_rect->color, xfi->srcBpp, 32, xfi->clrconv);

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, color);

	XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
			opaque_rect->nLeftRect, opaque_rect->nTopRect,
			opaque_rect->nWidth, opaque_rect->nHeight);

	if (xfi->drawing == xfi->primary)
	{
		if (xfi->remote_app != true)
		{
			XFillRectangle(xfi->display, xfi->drawable, xfi->gc,
				opaque_rect->nLeftRect, opaque_rect->nTopRect, opaque_rect->nWidth, opaque_rect->nHeight);
		}

		gdi_InvalidateRegion(xfi->hdc, opaque_rect->nLeftRect, opaque_rect->nTopRect,
				opaque_rect->nWidth, opaque_rect->nHeight);
	}
}

void xf_gdi_multi_opaque_rect(rdpContext* context, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	int i;
	uint32 color;
	DELTA_RECT* rectangle;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	color = freerdp_color_convert_var(multi_opaque_rect->color, xfi->srcBpp, 32, xfi->clrconv);

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, color);

	for (i = 1; i < multi_opaque_rect->numRectangles + 1; i++)
	{
		rectangle = &multi_opaque_rect->rectangles[i];

		XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
				rectangle->left, rectangle->top,
				rectangle->width, rectangle->height);

		if (xfi->drawing == xfi->primary)
		{
			if (xfi->remote_app != true)
			{
				XFillRectangle(xfi->display, xfi->drawable, xfi->gc,
					rectangle->left, rectangle->top, rectangle->width, rectangle->height);
			}

			gdi_InvalidateRegion(xfi->hdc, rectangle->left, rectangle->top, rectangle->width, rectangle->height);
		}
	}
}

void xf_gdi_draw_nine_grid(rdpContext* context, DRAW_NINE_GRID_ORDER* draw_nine_grid)
{
	printf("DrawNineGrid\n");
}

void xf_gdi_line_to(rdpContext* context, LINE_TO_ORDER* line_to)
{
	uint32 color;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_set_rop2(xfi, line_to->bRop2);
	color = freerdp_color_convert_rgb(line_to->penColor, xfi->srcBpp, 32, xfi->clrconv);

	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, color);

	XDrawLine(xfi->display, xfi->drawing, xfi->gc,
			line_to->nXStart, line_to->nYStart, line_to->nXEnd, line_to->nYEnd);

	if (xfi->drawing == xfi->primary)
	{
		int width, height;

		if (xfi->remote_app != true)
		{
			XDrawLine(xfi->display, xfi->drawable, xfi->gc,
				line_to->nXStart, line_to->nYStart, line_to->nXEnd, line_to->nYEnd);
		}

		width = line_to->nXStart - line_to->nXEnd;
		height = line_to->nYStart - line_to->nYEnd;

		if (width < 0)
			width *= (-1);

		if (height < 0)
			height *= (-1);

		gdi_InvalidateRegion(xfi->hdc, line_to->nXStart, line_to->nYStart, width, height);

	}

	XSetFunction(xfi->display, xfi->gc, GXcopy);
}

void xf_gdi_polyline(rdpContext* context, POLYLINE_ORDER* polyline)
{
	int i;
	int x, y;
	int x1, y1;
	int x2, y2;
	int npoints;
	uint32 color;
	XPoint* points;
	int width, height;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_set_rop2(xfi, polyline->bRop2);
	color = freerdp_color_convert_var(polyline->penColor, xfi->srcBpp, 32, xfi->clrconv);

	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, color);

	npoints = polyline->numPoints + 1;
	points = xmalloc(sizeof(XPoint) * npoints);

	points[0].x = polyline->xStart;
	points[0].y = polyline->yStart;

	for (i = 0; i < polyline->numPoints; i++)
	{
		points[i + 1].x = polyline->points[i].x;
		points[i + 1].y = polyline->points[i].y;
	}

	XDrawLines(xfi->display, xfi->drawing, xfi->gc, points, npoints, CoordModePrevious);

	if (xfi->drawing == xfi->primary)
	{
		if (xfi->remote_app != true)
			XDrawLines(xfi->display, xfi->drawable, xfi->gc, points, npoints, CoordModePrevious);

		x1 = points[0].x;
		y1 = points[0].y;

		for (i = 1; i < npoints; i++)
		{
			x2 = points[i].x + x1;
			y2 = points[i].y + y1;

			x = (x2 < x1) ? x2 : x1;
			width = (x2 > x1) ? x2 - x1 : x1 - x2;

			y = (y2 < y1) ? y2 : y1;
			height = (y2 > y1) ? y2 - y1 : y1 - y2;

			x1 = x2;
			y1 = y2;

			gdi_InvalidateRegion(xfi->hdc, x, y, width, height);
		}
	}

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	xfree(points);
}

void xf_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	xfBitmap* bitmap;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	bitmap = (xfBitmap*) memblt->bitmap;
	xf_set_rop3(xfi, gdi_rop3_code(memblt->bRop));

	XCopyArea(xfi->display, bitmap->pixmap, xfi->drawing, xfi->gc,
			memblt->nXSrc, memblt->nYSrc, memblt->nWidth, memblt->nHeight,
			memblt->nLeftRect, memblt->nTopRect);

	if (xfi->drawing == xfi->primary)
	{
		if (xfi->remote_app != true)
		{
			XCopyArea(xfi->display, bitmap->pixmap, xfi->drawable, xfi->gc,
				memblt->nXSrc, memblt->nYSrc, memblt->nWidth, memblt->nHeight,
				memblt->nLeftRect, memblt->nTopRect);
		}

		gdi_InvalidateRegion(xfi->hdc, memblt->nLeftRect, memblt->nTopRect, memblt->nWidth, memblt->nHeight);
	}

	XSetFunction(xfi->display, xfi->gc, GXcopy);
}

void xf_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	rdpBrush* brush;
	xfBitmap* bitmap;
	uint32 foreColor;
	uint32 backColor;
	Pixmap pattern = 0;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	brush = &mem3blt->brush;
	bitmap = (xfBitmap*) mem3blt->bitmap;
	xf_set_rop3(xfi, gdi_rop3_code(mem3blt->bRop));
	foreColor = freerdp_color_convert_rgb(mem3blt->foreColor, xfi->srcBpp, 32, xfi->clrconv);
	backColor = freerdp_color_convert_rgb(mem3blt->backColor, xfi->srcBpp, 32, xfi->clrconv);

	if (brush->style == GDI_BS_PATTERN)
	{
		if (brush->bpp > 1)
		{
			pattern = xf_brush_new(xfi, 8, 8, brush->bpp, brush->data);

			XSetFillStyle(xfi->display, xfi->gc, FillTiled);
			XSetTile(xfi->display, xfi->gc, pattern);
			XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);
		}
		else
		{
			pattern = xf_mono_bitmap_new(xfi, 8, 8, brush->data);

			XSetForeground(xfi->display, xfi->gc, backColor);
			XSetBackground(xfi->display, xfi->gc, foreColor);
			XSetFillStyle(xfi->display, xfi->gc, FillOpaqueStippled);
			XSetStipple(xfi->display, xfi->gc, pattern);
			XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);
		}
	}
	else if (brush->style == GDI_BS_SOLID)
	{
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);
		XSetForeground(xfi->display, xfi->gc, backColor);
		XSetBackground(xfi->display, xfi->gc, foreColor);

		XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);
	}
	else
	{
		printf("Mem3Blt unimplemented brush style:%d\n", brush->style);
	}

	XCopyArea(xfi->display, bitmap->pixmap, xfi->drawing, xfi->gc,
			mem3blt->nXSrc, mem3blt->nYSrc, mem3blt->nWidth, mem3blt->nHeight,
			mem3blt->nLeftRect, mem3blt->nTopRect);

	if (xfi->drawing == xfi->primary)
	{
		if (xfi->remote_app != true)
		{
			XCopyArea(xfi->display, bitmap->pixmap, xfi->drawable, xfi->gc,
				mem3blt->nXSrc, mem3blt->nYSrc, mem3blt->nWidth, mem3blt->nHeight,
				mem3blt->nLeftRect, mem3blt->nTopRect);
		}

		gdi_InvalidateRegion(xfi->hdc, mem3blt->nLeftRect, mem3blt->nTopRect, mem3blt->nWidth, mem3blt->nHeight);
	}

	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetTSOrigin(xfi->display, xfi->gc, 0, 0);

	if (pattern != 0)
		XFreePixmap(xfi->display, pattern);

	XSetFunction(xfi->display, xfi->gc, GXcopy);
}

void xf_gdi_polygon_sc(rdpContext* context, POLYGON_SC_ORDER* polygon_sc)
{
	int i, npoints;
	XPoint* points;
	uint32 brush_color;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_set_rop2(xfi, polygon_sc->bRop2);
	brush_color = freerdp_color_convert_var(polygon_sc->brushColor, xfi->srcBpp, 32, xfi->clrconv);

	npoints = polygon_sc->numPoints + 1;
	points = xmalloc(sizeof(XPoint) * npoints);

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
			XSetFillRule(xfi->display, xfi->gc, EvenOddRule);
			break;

		case 2: /* winding */
			XSetFillRule(xfi->display, xfi->gc, WindingRule);
			break;

		default:
			printf("PolygonSC unknown fillMode: %d\n", polygon_sc->fillMode);
			break;
	}

	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, brush_color);

	XFillPolygon(xfi->display, xfi->drawing, xfi->gc,
			points, npoints, Complex, CoordModePrevious);

	if (xfi->drawing == xfi->primary)
	{
		XFillPolygon(xfi->display, xfi->drawable, xfi->gc,
				points, npoints, Complex, CoordModePrevious);
	}

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	xfree(points);
}

void xf_gdi_polygon_cb(rdpContext* context, POLYGON_CB_ORDER* polygon_cb)
{
	int i, npoints;
	XPoint* points;
	Pixmap pattern;
	rdpBrush* brush;
	uint32 foreColor;
	uint32 backColor;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	brush = &(polygon_cb->brush);
	xf_set_rop2(xfi, polygon_cb->bRop2);
	foreColor = freerdp_color_convert_rgb(polygon_cb->foreColor, xfi->srcBpp, 32, xfi->clrconv);
	backColor = freerdp_color_convert_rgb(polygon_cb->backColor, xfi->srcBpp, 32, xfi->clrconv);

	npoints = polygon_cb->numPoints + 1;
	points = xmalloc(sizeof(XPoint) * npoints);

	points[0].x = polygon_cb->xStart;
	points[0].y = polygon_cb->yStart;

	for (i = 0; i < polygon_cb->numPoints; i++)
	{
		points[i + 1].x = polygon_cb->points[i].x;
		points[i + 1].y = polygon_cb->points[i].y;
	}

	switch (polygon_cb->fillMode)
	{
		case 1: /* alternate */
			XSetFillRule(xfi->display, xfi->gc, EvenOddRule);
			break;

		case 2: /* winding */
			XSetFillRule(xfi->display, xfi->gc, WindingRule);
			break;

		default:
			printf("PolygonCB unknown fillMode: %d\n", polygon_cb->fillMode);
			break;
	}

	if (brush->style == GDI_BS_PATTERN)
	{
		if (brush->bpp > 1)
		{
			pattern = xf_brush_new(xfi, 8, 8, brush->bpp, brush->data);

			XSetFillStyle(xfi->display, xfi->gc, FillTiled);
			XSetTile(xfi->display, xfi->gc, pattern);
			XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);

			XFillPolygon(xfi->display, xfi->drawing, xfi->gc,
					points, npoints, Complex, CoordModePrevious);

			if (xfi->drawing == xfi->primary)
			{
				XFillPolygon(xfi->display, xfi->drawable, xfi->gc,
						points, npoints, Complex, CoordModePrevious);
			}

			XSetFillStyle(xfi->display, xfi->gc, FillSolid);
			XSetTSOrigin(xfi->display, xfi->gc, 0, 0);
			XFreePixmap(xfi->display, pattern);
		}
		else
		{
			pattern = xf_mono_bitmap_new(xfi, 8, 8, brush->data);

			XSetForeground(xfi->display, xfi->gc, backColor);
			XSetBackground(xfi->display, xfi->gc, foreColor);

			if (polygon_cb->backMode == BACKMODE_TRANSPARENT)
				XSetFillStyle(xfi->display, xfi->gc, FillStippled);
			else if (polygon_cb->backMode == BACKMODE_OPAQUE)
				XSetFillStyle(xfi->display, xfi->gc, FillOpaqueStippled);

			XSetStipple(xfi->display, xfi->gc, pattern);
			XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);

			XFillPolygon(xfi->display, xfi->drawing, xfi->gc,
					points, npoints, Complex, CoordModePrevious);

			if (xfi->drawing == xfi->primary)
			{
				XFillPolygon(xfi->display, xfi->drawable, xfi->gc,
						points, npoints, Complex, CoordModePrevious);
			}

			XSetFillStyle(xfi->display, xfi->gc, FillSolid);
			XSetTSOrigin(xfi->display, xfi->gc, 0, 0);
			XFreePixmap(xfi->display, pattern);
		}
	}
	else
	{
		printf("PolygonCB unimplemented brush style:%d\n", brush->style);
	}

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	xfree(points);
}

void xf_gdi_ellipse_sc(rdpContext* context, ELLIPSE_SC_ORDER* ellipse_sc)
{
	printf("EllipseSC\n");
}

void xf_gdi_ellipse_cb(rdpContext* context, ELLIPSE_CB_ORDER* ellipse_cb)
{
	printf("EllipseCB\n");
}

void xf_gdi_surface_frame_marker(rdpContext* context, SURFACE_FRAME_MARKER* surface_frame_marker)
{

}

void xf_gdi_surface_bits(rdpContext* context, SURFACE_BITS_COMMAND* surface_bits_command)
{
	int i, tx, ty;
	XImage* image;
	RFX_MESSAGE* message;
	xfInfo* xfi = ((xfContext*) context)->xfi;
	RFX_CONTEXT* rfx_context = (RFX_CONTEXT*) xfi->rfx_context;
	NSC_CONTEXT* nsc_context = (NSC_CONTEXT*) xfi->nsc_context;

	if (surface_bits_command->codecID == CODEC_ID_REMOTEFX)
	{
		message = rfx_process_message(rfx_context,
				surface_bits_command->bitmapData, surface_bits_command->bitmapDataLength);

		XSetFunction(xfi->display, xfi->gc, GXcopy);
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);

		XSetClipRectangles(xfi->display, xfi->gc,
				surface_bits_command->destLeft, surface_bits_command->destTop,
				(XRectangle*) message->rects, message->num_rects, YXBanded);

		/* Draw the tiles to primary surface, each is 64x64. */
		for (i = 0; i < message->num_tiles; i++)
		{
			image = XCreateImage(xfi->display, xfi->visual, 24, ZPixmap, 0,
				(char*) message->tiles[i]->data, 64, 64, 32, 0);

			tx = message->tiles[i]->x + surface_bits_command->destLeft;
			ty = message->tiles[i]->y + surface_bits_command->destTop;

			XPutImage(xfi->display, xfi->primary, xfi->gc, image, 0, 0, tx, ty, 64, 64);
			XFree(image);
		}

		/* Copy the updated region from backstore to the window. */
		for (i = 0; i < message->num_rects; i++)
		{
			tx = message->rects[i].x + surface_bits_command->destLeft;
			ty = message->rects[i].y + surface_bits_command->destTop;

			if (xfi->remote_app != true)
			{
				XCopyArea(xfi->display, xfi->primary, xfi->drawable, xfi->gc,
						tx, ty, message->rects[i].width, message->rects[i].height, tx, ty);
			}

			gdi_InvalidateRegion(xfi->hdc, tx, ty, message->rects[i].width, message->rects[i].height);
		}

		XSetClipMask(xfi->display, xfi->gc, None);
		rfx_message_free(rfx_context, message);
	}
	else if (surface_bits_command->codecID == CODEC_ID_NSCODEC)
	{
		nsc_context->width = surface_bits_command->width;
		nsc_context->height = surface_bits_command->height;
		nsc_process_message(nsc_context, surface_bits_command->bitmapData, surface_bits_command->bitmapDataLength);
		XSetFunction(xfi->display, xfi->gc, GXcopy);
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);

		xfi->bmp_codec_nsc = (uint8*) xrealloc(xfi->bmp_codec_nsc,
				surface_bits_command->width * surface_bits_command->height * 4);

		freerdp_image_flip(nsc_context->bmpdata, xfi->bmp_codec_nsc,
				surface_bits_command->width, surface_bits_command->height, 32);

		image = XCreateImage(xfi->display, xfi->visual, 24, ZPixmap, 0,
			(char*) xfi->bmp_codec_nsc, surface_bits_command->width, surface_bits_command->height, 32, 0);

		XPutImage(xfi->display, xfi->primary, xfi->gc, image, 0, 0,
				surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height);

		if (xfi->remote_app != true)
		{
			XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc,
				surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height,
				surface_bits_command->destLeft, surface_bits_command->destTop);
		}

		gdi_InvalidateRegion(xfi->hdc, surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height);

		XSetClipMask(xfi->display, xfi->gc, None);
		nsc_context_destroy(nsc_context);
	}
	else if (surface_bits_command->codecID == CODEC_ID_NONE)
	{
		XSetFunction(xfi->display, xfi->gc, GXcopy);
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);

		xfi->bmp_codec_none = (uint8*) xrealloc(xfi->bmp_codec_none,
				surface_bits_command->width * surface_bits_command->height * 4);

		freerdp_image_flip(surface_bits_command->bitmapData, xfi->bmp_codec_none,
				surface_bits_command->width, surface_bits_command->height, 32);

		image = XCreateImage(xfi->display, xfi->visual, 24, ZPixmap, 0,
			(char*) xfi->bmp_codec_none, surface_bits_command->width, surface_bits_command->height, 32, 0);

		XPutImage(xfi->display, xfi->primary, xfi->gc, image, 0, 0,
				surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height);

		if (xfi->remote_app != true)
		{
			XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc,
				surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height,
				surface_bits_command->destLeft, surface_bits_command->destTop);
		}

		gdi_InvalidateRegion(xfi->hdc, surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height);

		XSetClipMask(xfi->display, xfi->gc, None);
	}
	else
	{
		printf("Unsupported codecID %d\n", surface_bits_command->codecID);
	}
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
}

