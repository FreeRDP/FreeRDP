/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Graphical Objects
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Thincast Technologies GmbH
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
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

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#include <winpr/crt.h>

#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/jpeg.h>

#include "xf_graphics.h"
#include "xf_gdi.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

BOOL xf_decode_color(rdpGdi* gdi, const UINT32 srcColor,
                     UINT32* color, UINT32* format)
{
	xfContext* xfc;
	UINT32 DstFormat;
	UINT32 SrcFormat;

	if (!gdi || !gdi->context || !gdi->context->settings)
		return FALSE;

	xfc = (xfContext*)gdi->context;
	SrcFormat = gdi_get_pixel_format(gdi->context->settings->ColorDepth,
	                                 FALSE);

	if (format)
		*format = SrcFormat;

	DstFormat = xf_get_local_color_format(xfc, FALSE);
	*color = ConvertColor(srcColor, SrcFormat,
	                      DstFormat, &gdi->palette);
	return TRUE;
}

/* Bitmap Class */
static BOOL xf_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	int depth;
	BYTE* data;
	Pixmap pixmap;
	XImage* image;
	UINT32 SrcFormat;
	rdpGdi* gdi;
	xfContext* xfc = (xfContext*) context;
	gdi = context->gdi;
	xf_lock_x11(xfc, FALSE);
	data = bitmap->data;
	depth = GetBitsPerPixel(bitmap->format);
	pixmap = XCreatePixmap(xfc->display, xfc->drawable, bitmap->width,
	                       bitmap->height, xfc->depth);

	if (bitmap->data)
	{
		XSetFunction(xfc->display, xfc->gc, GXcopy);

		if (depth != xfc->depth)
		{
			if (!(data = _aligned_malloc(bitmap->width * bitmap->height * 4, 16)))
			{
				xf_unlock_x11(xfc, FALSE);
				return FALSE;
			}

			SrcFormat = bitmap->format;
			freerdp_image_copy(data, gdi->dstFormat, 0, 0, 0,
			                   bitmap->width, bitmap->height,
			                   bitmap->data, SrcFormat,
			                   0, 0, 0, &context->gdi->palette);
			_aligned_free(bitmap->data);
			bitmap->data = data;
			bitmap->format = gdi->dstFormat;
		}

		image = XCreateImage(xfc->display, xfc->visual, xfc->depth,
		                     ZPixmap, 0, (char*) bitmap->data, bitmap->width, bitmap->height,
		                     xfc->scanline_pad, 0);
		XPutImage(xfc->display, pixmap, xfc->gc, image, 0, 0, 0, 0, bitmap->width,
		          bitmap->height);
		XFree(image);
	}

	((xfBitmap*) bitmap)->pixmap = pixmap;
	xf_unlock_x11(xfc, FALSE);
	return TRUE;
}

static void xf_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	xfContext* xfc = (xfContext*) context;
	xf_lock_x11(xfc, FALSE);

	if (((xfBitmap*) bitmap)->pixmap != 0)
		XFreePixmap(xfc->display, ((xfBitmap*) bitmap)->pixmap);

	xf_unlock_x11(xfc, FALSE);
}

static BOOL xf_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	XImage* image;
	int width, height;
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;
	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;
	xf_lock_x11(xfc, FALSE);
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	image = XCreateImage(xfc->display, xfc->visual, xfc->depth,
	                     ZPixmap, 0, (char*) bitmap->data, bitmap->width, bitmap->height,
	                     xfc->scanline_pad, 0);
	XPutImage(xfc->display, xfc->primary, xfc->gc,
	          image, 0, 0, bitmap->left, bitmap->top, width, height);
	XFree(image);
	ret = gdi_InvalidateRegion(xfc->hdc, bitmap->left, bitmap->top, width, height);
	xf_unlock_x11(xfc, FALSE);
	return TRUE;
}

static BOOL xf_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap,
                                 BOOL primary)
{
	xfContext* xfc = (xfContext*) context;
	xf_lock_x11(xfc, FALSE);

	if (primary)
		xfc->drawing = xfc->primary;
	else
		xfc->drawing = ((xfBitmap*) bitmap)->pixmap;

	xf_unlock_x11(xfc, FALSE);
	return TRUE;
}

/* Pointer Class */
static BOOL xf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	UINT32 CursorFormat;
	rdpGdi* gdi;
	size_t size;
	XcursorImage ci;
	xfContext* xfc = (xfContext*) context;
	xfPointer* xpointer = (xfPointer*)pointer;

	if (!context || !pointer || !context->gdi)
		return FALSE;

	if (!xfc->invert)
		CursorFormat = PIXEL_FORMAT_RGBA32;
	else
		CursorFormat = PIXEL_FORMAT_BGRA32;

	gdi = context->gdi;
	xf_lock_x11(xfc, FALSE);
	ZeroMemory(&ci, sizeof(ci));
	ci.version = XCURSOR_IMAGE_VERSION;
	ci.size = sizeof(ci);
	ci.width = pointer->width;
	ci.height = pointer->height;
	ci.xhot = pointer->xPos;
	ci.yhot = pointer->yPos;
	size = ci.height * ci.width * GetBytesPerPixel(CursorFormat);

	if (!(ci.pixels = (XcursorPixel*) _aligned_malloc(size, 16)))
	{
		xf_unlock_x11(xfc, FALSE);
		return FALSE;
	}

	if (!freerdp_image_copy_from_pointer_data(
	        (BYTE*) ci.pixels, CursorFormat,
	        0, 0, 0, pointer->width, pointer->height,
	        pointer->xorMaskData, pointer->lengthXorMask,
	        pointer->andMaskData, pointer->lengthAndMask,
	        pointer->xorBpp, &context->gdi->palette))
	{
		_aligned_free(ci.pixels);
		xf_unlock_x11(xfc, FALSE);
		return FALSE;
	}

	xpointer->cursor = XcursorImageLoadCursor(xfc->display, &ci);
	_aligned_free(ci.pixels);
	xf_unlock_x11(xfc, FALSE);
#endif
	return TRUE;
}

static void xf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*) context;
	xf_lock_x11(xfc, FALSE);

	if (((xfPointer*) pointer)->cursor)
		XFreeCursor(xfc->display, ((xfPointer*) pointer)->cursor);

	xf_unlock_x11(xfc, FALSE);
#endif
}

static BOOL xf_Pointer_Set(rdpContext* context,
                           const rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*) context;
	xf_lock_x11(xfc, FALSE);
	xfc->pointer = (xfPointer*) pointer;

	/* in RemoteApp mode, window can be null if none has had focus */

	if (xfc->window)
		XDefineCursor(xfc->display, xfc->window->handle, xfc->pointer->cursor);

	xf_unlock_x11(xfc, FALSE);
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetNull(rdpContext* context)
{
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*) context;
	static Cursor nullcursor = None;
	xf_lock_x11(xfc, FALSE);

	if (nullcursor == None)
	{
		XcursorImage ci;
		XcursorPixel xp = 0;
		ZeroMemory(&ci, sizeof(ci));
		ci.version = XCURSOR_IMAGE_VERSION;
		ci.size = sizeof(ci);
		ci.width = ci.height = 1;
		ci.xhot = ci.yhot = 0;
		ci.pixels = &xp;
		nullcursor = XcursorImageLoadCursor(xfc->display, &ci);
	}

	xfc->pointer = NULL;

	if ((xfc->window) && (nullcursor != None))
		XDefineCursor(xfc->display, xfc->window->handle, nullcursor);

	xf_unlock_x11(xfc, FALSE);
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetDefault(rdpContext* context)
{
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*) context;
	xf_lock_x11(xfc, FALSE);
	xfc->pointer = NULL;

	if (xfc->window)
		XUndefineCursor(xfc->display, xfc->window->handle);

	xf_unlock_x11(xfc, FALSE);
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	xfContext* xfc = (xfContext*) context;
	XWindowAttributes current;
	XSetWindowAttributes tmp;
	BOOL ret = FALSE;

	if (!xfc->focused || !xfc->window)
		return TRUE;

	xf_lock_x11(xfc, FALSE);

	if (XGetWindowAttributes(xfc->display, xfc->window->handle, &current) == 0)
		goto out;

	tmp.event_mask = (current.your_event_mask & ~(PointerMotionMask));

	if (XChangeWindowAttributes(xfc->display, xfc->window->handle, CWEventMask,
	                            &tmp) == 0)
		goto out;

	XWarpPointer(xfc->display, None, xfc->window->handle, 0, 0, 0, 0, x, y);
	tmp.event_mask = current.your_event_mask;
	XChangeWindowAttributes(xfc->display, xfc->window->handle, CWEventMask, &tmp);
	ret = TRUE;
out:
	xf_unlock_x11(xfc, FALSE);
	return ret;
}

/* Glyph Class */
static BOOL xf_Glyph_New(rdpContext* context, const rdpGlyph* glyph)
{
	int scanline;
	XImage* image;
	xfGlyph* xf_glyph;
	xf_glyph = (xfGlyph*) glyph;
	xfContext* xfc = (xfContext*) context;
	xf_lock_x11(xfc, FALSE);
	scanline = (glyph->cx + 7) / 8;
	xf_glyph->pixmap = XCreatePixmap(xfc->display, xfc->drawing, glyph->cx,
	                                 glyph->cy, 1);
	image = XCreateImage(xfc->display, xfc->visual, 1,
	                     ZPixmap, 0, (char*) glyph->aj, glyph->cx, glyph->cy, 8, scanline);
	image->byte_order = MSBFirst;
	image->bitmap_bit_order = MSBFirst;
	XInitImage(image);
	XPutImage(xfc->display, xf_glyph->pixmap, xfc->gc_mono, image, 0, 0, 0, 0,
	          glyph->cx, glyph->cy);
	XFree(image);
	xf_unlock_x11(xfc, FALSE);
	return TRUE;
}

static void xf_Glyph_Free(rdpContext* context, rdpGlyph* glyph)
{
	xfContext* xfc = (xfContext*) context;
	xf_lock_x11(xfc, FALSE);

	if (((xfGlyph*) glyph)->pixmap != 0)
		XFreePixmap(xfc->display, ((xfGlyph*) glyph)->pixmap);

	xf_unlock_x11(xfc, FALSE);
	free(glyph->aj);
	free(glyph);
}

static BOOL xf_Glyph_Draw(rdpContext* context, const rdpGlyph* glyph, UINT32 x,
                          UINT32 y, UINT32 w, UINT32 h, UINT32 sx, UINT32 sy,
                          BOOL fOpRedundant)
{
	xfGlyph* xf_glyph;
	xfContext* xfc = (xfContext*) context;
	xf_glyph = (xfGlyph*) glyph;
	xf_lock_x11(xfc, FALSE);

	if (!fOpRedundant)
	{
		XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
		XFillRectangle(xfc->display, xfc->drawable, xfc->gc, x, y, w, h);
	}

	XSetFillStyle(xfc->display, xfc->gc, FillStippled);
	XSetStipple(xfc->display, xfc->gc, xf_glyph->pixmap);

	if (sx || sy)
		WLog_ERR(TAG, "");

	//XSetClipOrigin(xfc->display, xfc->gc, sx, sy);
	XSetTSOrigin(xfc->display, xfc->gc, x, y);
	XFillRectangle(xfc->display, xfc->drawing, xfc->gc, x, y, w, h);
	xf_unlock_x11(xfc, FALSE);
	return TRUE;
}

static BOOL xf_Glyph_BeginDraw(rdpContext* context, UINT32 x, UINT32 y,
                               UINT32 width, UINT32 height, UINT32 bgcolor,
                               UINT32 fgcolor, BOOL fOpRedundant)
{
	xfContext* xfc = (xfContext*) context;
	XRectangle rect;

	if (!xf_decode_color(context->gdi, bgcolor, &bgcolor, NULL))
		return FALSE;

	if (!xf_decode_color(context->gdi, fgcolor, &fgcolor, NULL))
		return FALSE;

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;
	xf_lock_x11(xfc, FALSE);

	if (!fOpRedundant)
	{
		XSetForeground(xfc->display, xfc->gc, fgcolor);
		XSetBackground(xfc->display, xfc->gc, fgcolor);
		XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
		XFillRectangle(xfc->display, xfc->drawable, xfc->gc, x, y, width, height);
	}

	XSetForeground(xfc->display, xfc->gc, bgcolor);
	XSetBackground(xfc->display, xfc->gc, fgcolor);
	xf_unlock_x11(xfc, FALSE);
	return TRUE;
}

static BOOL xf_Glyph_EndDraw(rdpContext* context, UINT32 x, UINT32 y,
                             UINT32 width, UINT32 height,
                             UINT32 bgcolor, UINT32 fgcolor)
{
	xfContext* xfc = (xfContext*) context;
	BOOL ret = TRUE;

	if (!xf_decode_color(context->gdi, bgcolor, &bgcolor, NULL))
		return FALSE;

	if (!xf_decode_color(context->gdi, fgcolor, &fgcolor, NULL))
		return FALSE;

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, x, y, width, height);

	return ret;
}

/* Graphics Module */
BOOL xf_register_pointer(rdpGraphics* graphics)
{
	rdpPointer* pointer = NULL;

	if (!(pointer = (rdpPointer*) calloc(1, sizeof(rdpPointer))))
		return FALSE;

	pointer->size = sizeof(xfPointer);
	pointer->New = xf_Pointer_New;
	pointer->Free = xf_Pointer_Free;
	pointer->Set = xf_Pointer_Set;
	pointer->SetNull = xf_Pointer_SetNull;
	pointer->SetDefault = xf_Pointer_SetDefault;
	pointer->SetPosition = xf_Pointer_SetPosition;
	graphics_register_pointer(graphics, pointer);
	free(pointer);
	return TRUE;
}

BOOL xf_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap bitmap;
	rdpGlyph glyph;

	if (!graphics || !graphics->Bitmap_Prototype || !graphics->Glyph_Prototype)
		return FALSE;

	bitmap = *graphics->Bitmap_Prototype;
	glyph = *graphics->Glyph_Prototype;
	bitmap.size = sizeof(xfBitmap);
	bitmap.New = xf_Bitmap_New;
	bitmap.Free = xf_Bitmap_Free;
	bitmap.Paint = xf_Bitmap_Paint;
	bitmap.SetSurface = xf_Bitmap_SetSurface;
	graphics_register_bitmap(graphics, &bitmap);
	glyph.size = sizeof(xfGlyph);
	glyph.New = xf_Glyph_New;
	glyph.Free = xf_Glyph_Free;
	glyph.Draw = xf_Glyph_Draw;
	glyph.BeginDraw = xf_Glyph_BeginDraw;
	glyph.EndDraw = xf_Glyph_EndDraw;
	graphics_register_glyph(graphics, &glyph);
	return TRUE;
}

UINT32 xf_get_local_color_format(xfContext* xfc, BOOL aligned)
{
	UINT32 DstFormat;
	BOOL invert = !(aligned ^ xfc->invert);

	if (!xfc)
		return 0;

	if (xfc->depth == 32)
		DstFormat = (!invert) ? PIXEL_FORMAT_RGBA32 : PIXEL_FORMAT_BGRA32;
	else if (xfc->depth == 24)
	{
		if (aligned)
			DstFormat = (!invert) ? PIXEL_FORMAT_RGBX32 : PIXEL_FORMAT_BGRX32;
		else
			DstFormat = (!invert) ? PIXEL_FORMAT_RGB24 : PIXEL_FORMAT_BGR24;
	}
	else if (xfc->depth == 16)
		DstFormat = (!invert) ? PIXEL_FORMAT_RGB16 : PIXEL_FORMAT_BGR16;
	else if (xfc->depth == 15)
		DstFormat = (!invert) ? PIXEL_FORMAT_RGB16 : PIXEL_FORMAT_BGR16;
	else
		DstFormat = (!invert) ? PIXEL_FORMAT_RGBX32 : PIXEL_FORMAT_BGRX32;

	return DstFormat;
}
