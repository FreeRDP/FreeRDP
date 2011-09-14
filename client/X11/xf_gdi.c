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

#include <freerdp/gdi/gdi.h>
#include <freerdp/rfx/rfx.h>
#include <freerdp/constants.h>
#include <freerdp/utils/memory.h>

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
		return False;
	}

	XSetFunction(xfi->display, xfi->gc, xf_rop2_table[rop2]);
	return True;
}

boolean xf_set_rop3(xfInfo* xfi, int rop3)
{
	int function = -1;

	switch (rop3)
	{
		case GDI_BLACKNESS:
			function = GXclear;
			break;

		case 0x000500A9:
			function = GXnor;
			break;

		case 0x000A0329:
			function = GXandInverted;
			break;

		case 0x000F0001:
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

		case 0x00500325:
			function = GXandReverse;
			break;

		case GDI_DSTINVERT:
			function = GXinvert;
			break;

		case GDI_PATINVERT:
			function = GXxor;
			break;

		case 0x005F00E9:
			function = GXnand;
			break;

		case GDI_SRCINVERT:
			function = GXxor;
			break;

		case 0x007700E6:
			function = GXnand;
			break;

		case GDI_SRCAND:
			function = GXand;
			break;

		case 0x00990066:
			function = GXequiv;
			break;

		case 0x00A000C9:
			function = GXand;
			break;

		case GDI_PDxn:
			function = GXequiv;
			break;

		case 0x00AA0029:
			function = GXnoop;
			break;

		case 0x00AF0229:
			function = GXorInverted;
			break;

		case GDI_MERGEPAINT:
			function = GXorInverted;
			break;

		case GDI_SRCCOPY:
			function = GXcopy;
			break;

		case 0x00DD0228:
			function = GXorReverse;
			break;

		case GDI_SRCPAINT:
			function = GXor;
			break;

		case GDI_PATCOPY:
			function = GXcopy;
			break;

		case 0x00F50225:
			function = GXorReverse;
			break;

		case 0x00FA0089:
			function = GXor;
			break;

		case GDI_WHITENESS:
			function = GXset;
			break;

		default:
			break;
	}

	if (function < 0)
	{
		printf("Unsupported ROP3: 0x%08X\n", rop3);
		XSetFunction(xfi->display, xfi->gc, GXclear);
		return False;
	}

	XSetFunction(xfi->display, xfi->gc, function);

	return True;
}

Pixmap xf_bitmap_new(xfInfo* xfi, int width, int height, int bpp, uint8* data)
{
	Pixmap bitmap;
	uint8* cdata;
	XImage* image;

	bitmap = XCreatePixmap(xfi->display, xfi->window->handle, width, height, xfi->bpp);

	cdata = freerdp_image_convert(data, NULL, width, height, bpp, xfi->bpp, xfi->clrconv);

	image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
			ZPixmap, 0, (char *) cdata, width, height, xfi->scanline_pad, 0);

	XPutImage(xfi->display, bitmap, xfi->gc, image, 0, 0, 0, 0, width, height);
	XFree(image);

	if (cdata != data)
		xfree(cdata);

	return bitmap;
}

Pixmap xf_mono_bitmap_new(xfInfo* xfi, int width, int height, uint8* data)
{
	int scanline;
	XImage* image;
	Pixmap bitmap;

	scanline = (width + 7) / 8;

	bitmap = XCreatePixmap(xfi->display, xfi->window->handle, width, height, 1);

	image = XCreateImage(xfi->display, xfi->visual, 1,
			ZPixmap, 0, (char*) data, width, height, 8, scanline);

	XPutImage(xfi->display, bitmap, xfi->gc_mono, image, 0, 0, 0, 0, width, height);
	XFree(image);

	return bitmap;
}

void xf_gdi_bitmap_update(rdpUpdate* update, BITMAP_UPDATE* bitmap)
{
	int i;
	int x, y;
	int w, h;
	uint8* data;
	XImage* image;
	BITMAP_DATA* bmp;
	xfInfo* xfi = GET_XFI(update);

	for (i = 0; i < bitmap->number; i++)
	{
		bmp = &bitmap->bitmaps[i];

		data = freerdp_image_convert(bmp->data, NULL, bmp->width, bmp->height, bmp->bpp, xfi->bpp, xfi->clrconv);

		image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
				ZPixmap, 0, (char*) data, bmp->width, bmp->height, xfi->scanline_pad, 0);

		x = bmp->left;
		y = bmp->top;
		w = bmp->right - bmp->left + 1;
		h = bmp->bottom - bmp->top + 1;

		XPutImage(xfi->display, xfi->primary, xfi->gc, image, 0, 0, x, y, w, h);
		XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc, x, y, w, h, x, y);
	}
}

void xf_gdi_palette_update(rdpUpdate* update, PALETTE_UPDATE* palette)
{

}

void xf_gdi_set_bounds(rdpUpdate* update, BOUNDS* bounds)
{
	XRectangle clip;
	xfInfo* xfi = GET_XFI(update);

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

void xf_gdi_dstblt(rdpUpdate* update, DSTBLT_ORDER* dstblt)
{
	xfInfo* xfi = GET_XFI(update);

	xf_set_rop3(xfi, gdi_rop3_code(dstblt->bRop));

	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
			dstblt->nLeftRect, dstblt->nTopRect,
			dstblt->nWidth, dstblt->nHeight);

	if (xfi->drawing == xfi->primary)
	{
		XFillRectangle(xfi->display, xfi->window->handle, xfi->gc,
				dstblt->nLeftRect, dstblt->nTopRect, dstblt->nWidth, dstblt->nHeight);
	}
}

void xf_gdi_patblt(rdpUpdate* update, PATBLT_ORDER* patblt)
{
	BRUSH* brush;
	Pixmap pattern;
	uint32 foreColor;
	uint32 backColor;
	xfInfo* xfi = GET_XFI(update);

	brush = &patblt->brush;
	xf_set_rop3(xfi, gdi_rop3_code(patblt->bRop));

	foreColor = freerdp_color_convert(patblt->foreColor, xfi->srcBpp, 32, xfi->clrconv);
	backColor = freerdp_color_convert(patblt->backColor, xfi->srcBpp, 32, xfi->clrconv);

	if (brush->style & CACHED_BRUSH)
	{
		printf("cached brush bpp:%d\n", brush->bpp);
		brush->data = brush_get(xfi->cache->brush, brush->index, &brush->bpp);
		brush->style = GDI_BS_PATTERN;
	}

	if (brush->style == GDI_BS_SOLID)
	{
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);
		XSetForeground(xfi->display, xfi->gc, patblt->foreColor);

		XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
				patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);
	}
	else if (brush->style == GDI_BS_PATTERN)
	{
		if (brush->bpp > 1)
		{
			pattern = xf_bitmap_new(xfi, 8, 8, brush->bpp, brush->data);

			XSetFillStyle(xfi->display, xfi->gc, FillTiled);
			XSetTile(xfi->display, xfi->gc, pattern);
			XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);

			XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
					patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);

			XSetTile(xfi->display, xfi->gc, xfi->primary);
		}
		else
		{
			pattern = xf_mono_bitmap_new(xfi, 8, 8, brush->data);

			XSetForeground(xfi->display, xfi->gc, backColor);
			XSetBackground(xfi->display, xfi->gc, foreColor);
			XSetFillStyle(xfi->display, xfi->gc, FillOpaqueStippled);
			XSetTSOrigin(xfi->display, xfi->gc, brush->x, brush->y);

			XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
					patblt->nLeftRect, patblt->nTopRect, patblt->nWidth, patblt->nHeight);

			XSetStipple(xfi->display, xfi->gc_mono, pattern);
		}
	}
	else
	{
		printf("unimplemented brush style:%d\n", brush->style);
	}

	if (xfi->drawing == xfi->primary)
	{
		XSetFunction(xfi->display, xfi->gc, GXcopy);
		XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc, patblt->nLeftRect, patblt->nTopRect,
				patblt->nWidth, patblt->nHeight, patblt->nLeftRect, patblt->nTopRect);
	}
}

void xf_gdi_scrblt(rdpUpdate* update, SCRBLT_ORDER* scrblt)
{
	xfInfo* xfi = GET_XFI(update);

	xf_set_rop3(xfi, gdi_rop3_code(scrblt->bRop));
	XCopyArea(xfi->display, xfi->primary, xfi->drawing, xfi->gc, scrblt->nXSrc, scrblt->nYSrc,
			scrblt->nWidth, scrblt->nHeight, scrblt->nLeftRect, scrblt->nTopRect);

	if (xfi->drawing == xfi->primary)
	{
		if (xfi->unobscured)
		{
			XCopyArea(xfi->display, xfi->window->handle, xfi->window->handle, xfi->gc,
					scrblt->nXSrc, scrblt->nYSrc, scrblt->nWidth, scrblt->nHeight,
					scrblt->nLeftRect, scrblt->nTopRect);
		}
		else
		{
			XSetFunction(xfi->display, xfi->gc, GXcopy);
			XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc,
					scrblt->nLeftRect, scrblt->nTopRect, scrblt->nWidth, scrblt->nHeight,
					scrblt->nLeftRect, scrblt->nTopRect);
		}
	}
}

void xf_gdi_opaque_rect(rdpUpdate* update, OPAQUE_RECT_ORDER* opaque_rect)
{
	uint32 color;
	xfInfo* xfi = GET_XFI(update);

	color = freerdp_color_convert(opaque_rect->color, xfi->srcBpp, xfi->bpp, xfi->clrconv);

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, color);
	XFillRectangle(xfi->display, xfi->drawing, xfi->gc,
			opaque_rect->nLeftRect, opaque_rect->nTopRect,
			opaque_rect->nWidth, opaque_rect->nHeight);

	if (xfi->drawing == xfi->primary)
	{
		XFillRectangle(xfi->display, xfi->window->handle, xfi->gc,
				opaque_rect->nLeftRect, opaque_rect->nTopRect, opaque_rect->nWidth, opaque_rect->nHeight);
	}
}

void xf_gdi_multi_opaque_rect(rdpUpdate* update, MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	int i;
	uint32 color;
	DELTA_RECT* rectangle;
	xfInfo* xfi = GET_XFI(update);

	color = freerdp_color_convert(multi_opaque_rect->color, xfi->srcBpp, xfi->bpp, xfi->clrconv);

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
			XFillRectangle(xfi->display, xfi->window->handle, xfi->gc,
					rectangle->left, rectangle->top, rectangle->width, rectangle->height);
		}
	}
}

void xf_gdi_line_to(rdpUpdate* update, LINE_TO_ORDER* line_to)
{
	uint32 color;
	xfInfo* xfi = GET_XFI(update);

	xf_set_rop2(xfi, line_to->bRop2);
	color = freerdp_color_convert(line_to->penColor, xfi->srcBpp, 32, xfi->clrconv);

	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, color);

	XDrawLine(xfi->display, xfi->drawing, xfi->gc,
			line_to->nXStart, line_to->nYStart, line_to->nXEnd, line_to->nYEnd);

	if (xfi->drawing == xfi->primary)
	{
		XDrawLine(xfi->display, xfi->window->handle, xfi->gc,
				line_to->nXStart, line_to->nYStart, line_to->nXEnd, line_to->nYEnd);
	}
}

void xf_gdi_polyline(rdpUpdate* update, POLYLINE_ORDER* polyline)
{
	int i;
	uint32 color;
	XPoint* points;
	xfInfo* xfi = GET_XFI(update);

	xf_set_rop2(xfi, polyline->bRop2);
	color = freerdp_color_convert(polyline->penColor, xfi->srcBpp, 32, xfi->clrconv);

	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, color);

	points = xmalloc(sizeof(XPoint) * polyline->numPoints);

	for (i = 0; i < polyline->numPoints; i++)
	{
		points[i].x = polyline->points[i].x;
		points[i].y = polyline->points[i].y;
	}

	XDrawLines(xfi->display, xfi->drawing, xfi->gc, points, polyline->numPoints, CoordModePrevious);

	if (xfi->drawing == xfi->primary)
	{
		XDrawLines(xfi->display, xfi->window->handle, xfi->gc, points, polyline->numPoints, CoordModePrevious);
	}

	xfree(points);
}

void xf_gdi_fast_index(rdpUpdate* update, FAST_INDEX_ORDER* fast_index)
{

}

void xf_gdi_create_offscreen_bitmap(rdpUpdate* update, CREATE_OFFSCREEN_BITMAP_ORDER* create_offscreen_bitmap)
{
	Pixmap surface;
	xfInfo* xfi = GET_XFI(update);

	surface = xf_bitmap_new(xfi, create_offscreen_bitmap->cx, create_offscreen_bitmap->cy, xfi->bpp, NULL);

	offscreen_put(xfi->cache->offscreen, create_offscreen_bitmap->id, (void*) surface);
}

void xf_gdi_switch_surface(rdpUpdate* update, SWITCH_SURFACE_ORDER* switch_surface)
{
	Pixmap surface;
	xfInfo* xfi = GET_XFI(update);

	if (switch_surface->bitmapId == SCREEN_BITMAP_SURFACE)
	{
		xfi->drawing = xfi->primary;
	}
	else
	{
		surface = (Pixmap) offscreen_get(xfi->cache->offscreen, switch_surface->bitmapId);
		xfi->drawing = surface;
	}
}

void xf_gdi_cache_bitmap_v2(rdpUpdate* update, CACHE_BITMAP_V2_ORDER* cache_bitmap_v2)
{
	xfInfo* xfi = GET_XFI(update);

	bitmap_v2_put(xfi->cache->bitmap_v2, cache_bitmap_v2->cacheId,
			cache_bitmap_v2->cacheIndex, cache_bitmap_v2->data);
}

void xf_gdi_cache_color_table(rdpUpdate* update, CACHE_COLOR_TABLE_ORDER* cache_color_table)
{

}

void xf_gdi_cache_glyph(rdpUpdate* update, CACHE_GLYPH_ORDER* cache_glyph)
{

}

void xf_gdi_cache_glyph_v2(rdpUpdate* update, CACHE_GLYPH_V2_ORDER* cache_glyph_v2)
{

}

void xf_gdi_cache_brush(rdpUpdate* update, CACHE_BRUSH_ORDER* cache_brush)
{
	xfInfo* xfi = GET_XFI(update);
	brush_put(xfi->cache->brush, cache_brush->index, cache_brush->data, cache_brush->bpp);
}

void xf_gdi_surface_bits(rdpUpdate* update, SURFACE_BITS_COMMAND* surface_bits_command)
{
	STREAM* s;
	XImage* image;
	int i, tx, ty;
	RFX_MESSAGE* message;
	xfInfo* xfi = GET_XFI(update);
	RFX_CONTEXT* context = (RFX_CONTEXT*) xfi->rfx_context;

	if (surface_bits_command->codecID == CODEC_ID_REMOTEFX)
	{
		s = stream_new(0);
		stream_attach(s, surface_bits_command->bitmapData, surface_bits_command->bitmapDataLength);

		message = rfx_process_message(context, s);

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

			XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc,
				tx, ty, message->rects[i].width, message->rects[i].height, tx, ty);
		}

		XSetClipMask(xfi->display, xfi->gc, None);
		rfx_message_free(context, message);
		stream_detach(s);
		stream_free(s);
	}
	else if (surface_bits_command->codecID == CODEC_ID_NONE)
	{
		XSetFunction(xfi->display, xfi->gc, GXcopy);
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);

		xfi->bmp_codec_none = (uint8*) xrealloc(xfi->bmp_codec_none,
				surface_bits_command->width * surface_bits_command->height * 4);

		freerdp_image_invert(surface_bits_command->bitmapData, xfi->bmp_codec_none,
				surface_bits_command->width, surface_bits_command->height, 32);

		image = XCreateImage(xfi->display, xfi->visual, 24, ZPixmap, 0,
			(char*) xfi->bmp_codec_none, surface_bits_command->width, surface_bits_command->height, 32, 0);

		XPutImage(xfi->display, xfi->primary, xfi->gc, image, 0, 0,
				surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height);

		XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc,
				surface_bits_command->destLeft, surface_bits_command->destTop,
				surface_bits_command->width, surface_bits_command->height,
				surface_bits_command->destLeft, surface_bits_command->destTop);

		XSetClipMask(xfi->display, xfi->gc, None);
	}
	else
	{
		printf("Unsupported codecID %d\n", surface_bits_command->codecID);
	}
}

void xf_gdi_register_update_callbacks(rdpUpdate* update)
{
	update->Bitmap = xf_gdi_bitmap_update;
	update->Palette = xf_gdi_palette_update;
	update->SetBounds = xf_gdi_set_bounds;
	update->DstBlt = xf_gdi_dstblt;
	update->PatBlt = xf_gdi_patblt;
	update->ScrBlt = xf_gdi_scrblt;
	update->OpaqueRect = xf_gdi_opaque_rect;
	update->DrawNineGrid = NULL;
	update->MultiDstBlt = NULL;
	update->MultiPatBlt = NULL;
	update->MultiScrBlt = NULL;
	update->MultiOpaqueRect = xf_gdi_multi_opaque_rect;
	update->MultiDrawNineGrid = NULL;
	update->LineTo = xf_gdi_line_to;
	update->Polyline = NULL;
	update->MemBlt = NULL;
	update->Mem3Blt = NULL;
	update->SaveBitmap = NULL;
	update->GlyphIndex = NULL;
	update->FastIndex = xf_gdi_fast_index;
	update->FastGlyph = NULL;
	update->PolygonSC = NULL;
	update->PolygonCB = NULL;
	update->EllipseSC = NULL;
	update->EllipseCB = NULL;
	update->CreateOffscreenBitmap = xf_gdi_create_offscreen_bitmap;
	update->SwitchSurface = xf_gdi_switch_surface;

	update->CacheBitmapV2 = xf_gdi_cache_bitmap_v2;
	update->CacheColorTable = xf_gdi_cache_color_table;
	update->CacheGlyph = xf_gdi_cache_glyph;
	update->CacheGlyphV2 = xf_gdi_cache_glyph_v2;
	update->CacheBrush = xf_gdi_cache_brush;

	update->SurfaceBits = xf_gdi_surface_bits;
}

