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

#include "xf_graphics.h"
#include "xf_gdi.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

BOOL xf_decode_color(xfContext* xfc, const UINT32 srcColor, XColor* color)
{
	rdpGdi* gdi;
	rdpSettings* settings;
	UINT32 SrcFormat;
	BYTE r, g, b, a;

	if (!xfc || !color)
		return FALSE;

	gdi = xfc->context.gdi;

	if (!gdi)
		return FALSE;

	settings = xfc->context.settings;

	if (!settings)
		return FALSE;

	switch (settings->ColorDepth)
	{
		case 32:
		case 24:
			SrcFormat = PIXEL_FORMAT_BGR24;
			break;

		case 16:
			SrcFormat = PIXEL_FORMAT_RGB16;
			break;

		case 15:
			SrcFormat = PIXEL_FORMAT_RGB15;
			break;

		case 8:
			SrcFormat = PIXEL_FORMAT_RGB8;
			break;

		default:
			return FALSE;
	}

	SplitColor(srcColor, SrcFormat, &r, &g, &b, &a, &gdi->palette);
	color->blue = (unsigned short)(b << 8);
	color->green = (unsigned short)(g << 8);
	color->red = (unsigned short)(r << 8);
	color->flags = DoRed | DoGreen | DoBlue;

	if (XAllocColor(xfc->display, xfc->colormap, color) == 0)
		return FALSE;

	return TRUE;
}

/* Bitmap Class */
static BOOL xf_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	BOOL rc = FALSE;
	UINT32 depth;
	BYTE* data;
	rdpGdi* gdi;
	xfBitmap* xbitmap = (xfBitmap*)bitmap;
	xfContext* xfc = (xfContext*)context;

	if (!context || !bitmap || !context->gdi)
		return FALSE;

	gdi = context->gdi;
	xf_lock_x11(xfc);
	depth = GetBitsPerPixel(bitmap->format);
	xbitmap->pixmap =
	    XCreatePixmap(xfc->display, xfc->drawable, bitmap->width, bitmap->height, xfc->depth);

	if (!xbitmap->pixmap)
		goto unlock;

	if (bitmap->data)
	{
		XSetFunction(xfc->display, xfc->gc, GXcopy);

		if ((INT64)depth != xfc->depth)
		{
			if (!(data = _aligned_malloc(bitmap->width * bitmap->height * 4, 16)))
				goto unlock;

			if (!freerdp_image_copy(data, gdi->dstFormat, 0, 0, 0, bitmap->width, bitmap->height,
			                        bitmap->data, bitmap->format, 0, 0, 0, &context->gdi->palette,
			                        FREERDP_FLIP_NONE))
			{
				_aligned_free(data);
				goto unlock;
			}

			_aligned_free(bitmap->data);
			bitmap->data = data;
			bitmap->format = gdi->dstFormat;
		}

		xbitmap->image =
		    XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0, (char*)bitmap->data,
		                 bitmap->width, bitmap->height, xfc->scanline_pad, 0);

		if (!xbitmap->image)
			goto unlock;

		xbitmap->image->byte_order = LSBFirst;
		xbitmap->image->bitmap_bit_order = LSBFirst;
		XPutImage(xfc->display, xbitmap->pixmap, xfc->gc, xbitmap->image, 0, 0, 0, 0, bitmap->width,
		          bitmap->height);
	}

	rc = TRUE;
unlock:
	xf_unlock_x11(xfc);
	return rc;
}

static void xf_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	xfContext* xfc = (xfContext*)context;
	xfBitmap* xbitmap = (xfBitmap*)bitmap;

	if (!xfc || !xbitmap)
		return;

	xf_lock_x11(xfc);

	if (xbitmap->pixmap != 0)
	{
		XFreePixmap(xfc->display, xbitmap->pixmap);
		xbitmap->pixmap = 0;
	}

	if (xbitmap->image)
	{
		xbitmap->image->data = NULL;
		XDestroyImage(xbitmap->image);
		xbitmap->image = NULL;
	}

	xf_unlock_x11(xfc);
	_aligned_free(bitmap->data);
	free(xbitmap);
}

static BOOL xf_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	int width, height;
	xfContext* xfc = (xfContext*)context;
	xfBitmap* xbitmap = (xfBitmap*)bitmap;
	BOOL ret;

	if (!context || !xbitmap)
		return FALSE;

	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;
	xf_lock_x11(xfc);
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XPutImage(xfc->display, xfc->primary, xfc->gc, xbitmap->image, 0, 0, bitmap->left, bitmap->top,
	          width, height);
	ret = gdi_InvalidateRegion(xfc->hdc, bitmap->left, bitmap->top, width, height);
	xf_unlock_x11(xfc);
	return ret;
}

static BOOL xf_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, BOOL primary)
{
	xfContext* xfc = (xfContext*)context;

	if (!context || (!bitmap && !primary))
		return FALSE;

	xf_lock_x11(xfc);

	if (primary)
		xfc->drawing = xfc->primary;
	else
		xfc->drawing = ((xfBitmap*)bitmap)->pixmap;

	xf_unlock_x11(xfc);
	return TRUE;
}

static BOOL _xf_Pointer_GetCursorForCurrentScale(rdpContext* context, const rdpPointer* pointer,
                                                 Cursor* cursor)
{
#ifdef WITH_XCURSOR
	UINT32 CursorFormat;
	xfContext* xfc = (xfContext*)context;
	xfPointer* xpointer = (xfPointer*)pointer;
	XcursorImage ci = { 0 };
	rdpSettings* settings;
	UINT32 xTargetSize;
	UINT32 yTargetSize;
	double xscale;
	double yscale;
	size_t size;
	int cursorIndex = -1;
	UINT32 i;
	void* tmp;

	if (!context || !pointer || !context->gdi)
		return FALSE;

	settings = xfc->context.settings;

	if (!settings)
		return FALSE;

	xscale = (settings->SmartSizing ? xfc->scaledWidth / (double)settings->DesktopWidth : 1);
	yscale = (settings->SmartSizing ? xfc->scaledHeight / (double)settings->DesktopHeight : 1);
	xTargetSize = pointer->width * xscale;
	yTargetSize = pointer->height * yscale;

	for (i = 0; i < xpointer->nCursors; i++)
	{
		if (xpointer->cursorWidths[i] == xTargetSize && xpointer->cursorHeights[i] == yTargetSize)
		{
			cursorIndex = i;
		}
	}

	if (cursorIndex == -1)
	{
		xf_lock_x11(xfc);

		if (!xfc->invert)
			CursorFormat = (!xfc->big_endian) ? PIXEL_FORMAT_RGBA32 : PIXEL_FORMAT_ABGR32;
		else
			CursorFormat = (!xfc->big_endian) ? PIXEL_FORMAT_BGRA32 : PIXEL_FORMAT_ARGB32;

		if (xpointer->nCursors == xpointer->mCursors)
		{
			void* tmp2;
			xpointer->mCursors = (xpointer->mCursors == 0 ? 1 : xpointer->mCursors * 2);

			tmp2 = realloc(xpointer->cursorWidths, sizeof(UINT32) * xpointer->mCursors);
			if (!tmp2)
			{
				xf_unlock_x11(xfc);
				return FALSE;
			}
			xpointer->cursorWidths = tmp2;

			tmp2 = realloc(xpointer->cursorHeights, sizeof(UINT32) * xpointer->mCursors);
			if (!tmp2)
			{
				xf_unlock_x11(xfc);
				return FALSE;
			}
			xpointer->cursorHeights = (UINT32*)tmp2;

			tmp2 = realloc(xpointer->cursors, sizeof(Cursor) * xpointer->mCursors);
			if (!tmp2)
			{
				xf_unlock_x11(xfc);
				return FALSE;
			}
			xpointer->cursors = (Cursor*)tmp2;
		}

		ci.version = XCURSOR_IMAGE_VERSION;
		ci.size = sizeof(ci);
		ci.width = xTargetSize;
		ci.height = yTargetSize;
		ci.xhot = pointer->xPos * xscale;
		ci.yhot = pointer->yPos * yscale;
		size = ci.height * ci.width * GetBytesPerPixel(CursorFormat);

		tmp = _aligned_malloc(size, 16);
		if (!tmp)
		{
			xf_unlock_x11(xfc);
			return FALSE;
		}
		ci.pixels = (XcursorPixel*)tmp;

		if (xscale != 1 || yscale != 1)
		{
			if (!freerdp_image_scale((BYTE*)ci.pixels, CursorFormat, 0, 0, 0, ci.width, ci.height,
			                         (BYTE*)xpointer->cursorPixels, CursorFormat, 0, 0, 0,
			                         pointer->width, pointer->height))
			{
				_aligned_free(tmp);
				xf_unlock_x11(xfc);
				return FALSE;
			}
		}
		else
		{
			ci.pixels = xpointer->cursorPixels;
		}

		cursorIndex = xpointer->nCursors;
		xpointer->cursorWidths[cursorIndex] = ci.width;
		xpointer->cursorHeights[cursorIndex] = ci.height;
		xpointer->cursors[cursorIndex] = XcursorImageLoadCursor(xfc->display, &ci);
		xpointer->nCursors += 1;

		_aligned_free(tmp);

		xf_unlock_x11(xfc);
	}

	cursor[0] = xpointer->cursors[cursorIndex];
#endif
	return TRUE;
}

/* Pointer Class */
static BOOL xf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	UINT32 CursorFormat;
	size_t size;
	xfContext* xfc = (xfContext*)context;
	xfPointer* xpointer = (xfPointer*)pointer;

	if (!context || !pointer || !context->gdi)
		return FALSE;

	if (!xfc->invert)
		CursorFormat = (!xfc->big_endian) ? PIXEL_FORMAT_RGBA32 : PIXEL_FORMAT_ABGR32;
	else
		CursorFormat = (!xfc->big_endian) ? PIXEL_FORMAT_BGRA32 : PIXEL_FORMAT_ARGB32;

	xpointer->nCursors = 0;
	xpointer->mCursors = 0;

	size = pointer->height * pointer->width * GetBytesPerPixel(CursorFormat);

	if (!(xpointer->cursorPixels = (XcursorPixel*)_aligned_malloc(size, 16)))
		return FALSE;

	if (!freerdp_image_copy_from_pointer_data(
	        (BYTE*)xpointer->cursorPixels, CursorFormat, 0, 0, 0, pointer->width, pointer->height,
	        pointer->xorMaskData, pointer->lengthXorMask, pointer->andMaskData,
	        pointer->lengthAndMask, pointer->xorBpp, &context->gdi->palette))
	{
		_aligned_free(xpointer->cursorPixels);
		return FALSE;
	}

	if (!_xf_Pointer_GetCursorForCurrentScale(context, pointer, &(xpointer->cursor)))
		return FALSE;
#endif
	return TRUE;
}

static void xf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	UINT32 i;
	xfContext* xfc = (xfContext*)context;
	xfPointer* xpointer = (xfPointer*)pointer;

	xf_lock_x11(xfc);

	_aligned_free(xpointer->cursorPixels);
	free(xpointer->cursorWidths);
	free(xpointer->cursorHeights);

	for (i = 0; i < xpointer->nCursors; i++)
	{
		XFreeCursor(xfc->display, xpointer->cursors[i]);
	}

	free(xpointer->cursors);
	xpointer->nCursors = 0;
	xpointer->mCursors = 0;

	xf_unlock_x11(xfc);
#endif
}

static BOOL xf_Pointer_Set(rdpContext* context, const rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*)context;
	xfc->pointer = (xfPointer*)pointer;

	/* in RemoteApp mode, window can be null if none has had focus */

	if (xfc->window)
	{
		if (!_xf_Pointer_GetCursorForCurrentScale(context, pointer, &(xfc->pointer->cursor)))
			return FALSE;
		xf_lock_x11(xfc);
		XDefineCursor(xfc->display, xfc->window->handle, xfc->pointer->cursor);
		xf_unlock_x11(xfc);
	}
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetNull(rdpContext* context)
{
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*)context;
	static Cursor nullcursor = None;
	xf_lock_x11(xfc);

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

	xf_unlock_x11(xfc);
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetDefault(rdpContext* context)
{
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*)context;
	xf_lock_x11(xfc);
	xfc->pointer = NULL;

	if (xfc->window)
		XUndefineCursor(xfc->display, xfc->window->handle);

	xf_unlock_x11(xfc);
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	xfContext* xfc = (xfContext*)context;
	XWindowAttributes current;
	XSetWindowAttributes tmp;
	BOOL ret = FALSE;

	if (!xfc->focused || !xfc->window)
		return TRUE;

	xf_lock_x11(xfc);

	if (XGetWindowAttributes(xfc->display, xfc->window->handle, &current) == 0)
		goto out;

	tmp.event_mask = (current.your_event_mask & ~(PointerMotionMask));

	if (XChangeWindowAttributes(xfc->display, xfc->window->handle, CWEventMask, &tmp) == 0)
		goto out;

	XWarpPointer(xfc->display, None, xfc->window->handle, 0, 0, 0, 0, x, y);
	tmp.event_mask = current.your_event_mask;
	XChangeWindowAttributes(xfc->display, xfc->window->handle, CWEventMask, &tmp);
	ret = TRUE;
out:
	xf_unlock_x11(xfc);
	return ret;
}

/* Glyph Class */
static BOOL xf_Glyph_New(rdpContext* context, const rdpGlyph* glyph)
{
	int scanline;
	XImage* image;
	xfGlyph* xf_glyph;
	xf_glyph = (xfGlyph*)glyph;
	xfContext* xfc = (xfContext*)context;
	xf_lock_x11(xfc);
	scanline = (glyph->cx + 7) / 8;
	xf_glyph->pixmap = XCreatePixmap(xfc->display, xfc->drawing, glyph->cx, glyph->cy, 1);
	image = XCreateImage(xfc->display, xfc->visual, 1, ZPixmap, 0, (char*)glyph->aj, glyph->cx,
	                     glyph->cy, 8, scanline);
	image->byte_order = MSBFirst;
	image->bitmap_bit_order = MSBFirst;
	XInitImage(image);
	XPutImage(xfc->display, xf_glyph->pixmap, xfc->gc_mono, image, 0, 0, 0, 0, glyph->cx,
	          glyph->cy);
	image->data = NULL;
	XDestroyImage(image);
	xf_unlock_x11(xfc);
	return TRUE;
}

static void xf_Glyph_Free(rdpContext* context, rdpGlyph* glyph)
{
	xfContext* xfc = (xfContext*)context;
	xf_lock_x11(xfc);

	if (((xfGlyph*)glyph)->pixmap != 0)
		XFreePixmap(xfc->display, ((xfGlyph*)glyph)->pixmap);

	xf_unlock_x11(xfc);
	free(glyph->aj);
	free(glyph);
}

static BOOL xf_Glyph_Draw(rdpContext* context, const rdpGlyph* glyph, INT32 x, INT32 y, INT32 w,
                          INT32 h, INT32 sx, INT32 sy, BOOL fOpRedundant)
{
	xfGlyph* xf_glyph;
	xfContext* xfc = (xfContext*)context;
	xf_glyph = (xfGlyph*)glyph;
	xf_lock_x11(xfc);

	if (!fOpRedundant)
	{
		XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
		XFillRectangle(xfc->display, xfc->drawable, xfc->gc, x, y, w, h);
	}

	XSetFillStyle(xfc->display, xfc->gc, FillStippled);
	XSetStipple(xfc->display, xfc->gc, xf_glyph->pixmap);

	if (sx || sy)
		WLog_ERR(TAG, "");

	// XSetClipOrigin(xfc->display, xfc->gc, sx, sy);
	XSetTSOrigin(xfc->display, xfc->gc, x, y);
	XFillRectangle(xfc->display, xfc->drawing, xfc->gc, x, y, w, h);
	xf_unlock_x11(xfc);
	return TRUE;
}

static BOOL xf_Glyph_BeginDraw(rdpContext* context, INT32 x, INT32 y, INT32 width, INT32 height,
                               UINT32 bgcolor, UINT32 fgcolor, BOOL fOpRedundant)
{
	xfContext* xfc = (xfContext*)context;
	XRectangle rect;
	XColor xbgcolor, xfgcolor;

	if (!xf_decode_color(xfc, bgcolor, &xbgcolor))
		return FALSE;

	if (!xf_decode_color(xfc, fgcolor, &xfgcolor))
		return FALSE;

	rect.x = x;
	rect.y = y;
	rect.width = width;
	rect.height = height;
	xf_lock_x11(xfc);

	if (!fOpRedundant)
	{
		XSetForeground(xfc->display, xfc->gc, xfgcolor.pixel);
		XSetBackground(xfc->display, xfc->gc, xfgcolor.pixel);
		XSetFillStyle(xfc->display, xfc->gc, FillOpaqueStippled);
		XFillRectangle(xfc->display, xfc->drawable, xfc->gc, x, y, width, height);
	}

	XSetForeground(xfc->display, xfc->gc, xbgcolor.pixel);
	XSetBackground(xfc->display, xfc->gc, xfgcolor.pixel);
	xf_unlock_x11(xfc);
	return TRUE;
}

static BOOL xf_Glyph_EndDraw(rdpContext* context, INT32 x, INT32 y, INT32 width, INT32 height,
                             UINT32 bgcolor, UINT32 fgcolor)
{
	xfContext* xfc = (xfContext*)context;
	BOOL ret = TRUE;
	XColor xfgcolor, xbgcolor;

	if (!xf_decode_color(xfc, bgcolor, &xbgcolor))
		return FALSE;

	if (!xf_decode_color(xfc, fgcolor, &xfgcolor))
		return FALSE;

	if (xfc->drawing == xfc->primary)
		ret = gdi_InvalidateRegion(xfc->hdc, x, y, width, height);

	return ret;
}

/* Graphics Module */
BOOL xf_register_pointer(rdpGraphics* graphics)
{
	rdpPointer* pointer = NULL;

	if (!(pointer = (rdpPointer*)calloc(1, sizeof(rdpPointer))))
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
	BOOL invert = FALSE;

	if (!xfc)
		return 0;

	invert = xfc->invert;

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
		DstFormat = PIXEL_FORMAT_RGB16;
	else if (xfc->depth == 15)
		DstFormat = PIXEL_FORMAT_RGB15;
	else
		DstFormat = (!invert) ? PIXEL_FORMAT_RGBX32 : PIXEL_FORMAT_BGRX32;

	return DstFormat;
}
