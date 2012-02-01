/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Graphical Objects
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

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#include <freerdp/codec/bitmap.h>

#include "xf_graphics.h"

/* Bitmap Class */

void xf_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	uint8* data;
	Pixmap pixmap;
	XImage* image;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	pixmap = XCreatePixmap(xfi->display, xfi->drawable, bitmap->width, bitmap->height, xfi->depth);

	if (bitmap->data != NULL)
	{
		data = freerdp_image_convert(bitmap->data, NULL,
				bitmap->width, bitmap->height, bitmap->bpp, xfi->bpp, xfi->clrconv);

		if (bitmap->ephemeral != true)
		{
			image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
				ZPixmap, 0, (char*) data, bitmap->width, bitmap->height, xfi->scanline_pad, 0);

			XPutImage(xfi->display, pixmap, xfi->gc, image, 0, 0, 0, 0, bitmap->width, bitmap->height);
			XFree(image);

			if (data != bitmap->data)
				xfree(data);
		}
		else
		{
			if (data != bitmap->data)
				xfree(bitmap->data);
			
			bitmap->data = data;
		}
	}

	((xfBitmap*) bitmap)->pixmap = pixmap;
}

void xf_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	if (((xfBitmap*) bitmap)->pixmap != 0)
		XFreePixmap(xfi->display, ((xfBitmap*) bitmap)->pixmap);
}

void xf_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	XImage* image;
	int width, height;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;

	XSetFunction(xfi->display, xfi->gc, GXcopy);

	image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
			ZPixmap, 0, (char*) bitmap->data, bitmap->width, bitmap->height, xfi->scanline_pad, 0);

	XPutImage(xfi->display, xfi->primary, xfi->gc,
			image, 0, 0, bitmap->left, bitmap->top, width, height);

	XFree(image);

	if (xfi->remote_app != true)
	{
		XCopyArea(xfi->display, xfi->primary, xfi->drawable, xfi->gc,
				bitmap->left, bitmap->top, width, height, bitmap->left, bitmap->top);
	}

	gdi_InvalidateRegion(xfi->hdc, bitmap->left, bitmap->top, width, height);
}

void xf_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed)
{
	uint16 size;

	size = width * height * (bpp + 7) / 8;

	if (bitmap->data == NULL)
		bitmap->data = (uint8*) xmalloc(size);
	else
		bitmap->data = (uint8*) xrealloc(bitmap->data, size);

	if (compressed)
	{
		boolean status;

		status = bitmap_decompress(data, bitmap->data, width, height, length, bpp, bpp);

		if (status != true)
		{
			printf("Bitmap Decompression Failed\n");
		}
	}
	else
	{
		freerdp_image_flip(data, bitmap->data, width, height, bpp);
	}

	bitmap->compressed = false;
	bitmap->length = size;
	bitmap->bpp = bpp;
}

void xf_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, boolean primary)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	if (primary)
		xfi->drawing = xfi->primary;
	else
		xfi->drawing = ((xfBitmap*) bitmap)->pixmap;
}

/* Pointer Class */

void xf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	XcursorImage ci;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	memset(&ci, 0, sizeof(ci));
	ci.version = XCURSOR_IMAGE_VERSION;
	ci.size = sizeof(ci);
	ci.width = pointer->width;
	ci.height = pointer->height;
	ci.xhot = pointer->xPos;
	ci.yhot = pointer->yPos;
	ci.pixels = (XcursorPixel*) malloc(ci.width * ci.height * 4);
	memset(ci.pixels, 0, ci.width * ci.height * 4);

	if ((pointer->andMaskData != 0) && (pointer->xorMaskData != 0))
	{
		freerdp_alpha_cursor_convert((uint8*) (ci.pixels), pointer->xorMaskData, pointer->andMaskData,
				pointer->width, pointer->height, pointer->xorBpp, xfi->clrconv);
	}

	((xfPointer*) pointer)->cursor = XcursorImageLoadCursor(xfi->display, &ci);
	xfree(ci.pixels);
}

void xf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	if (((xfPointer*) pointer)->cursor != 0)
		XFreeCursor(xfi->display, ((xfPointer*) pointer)->cursor);
}

void xf_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	/* in RemoteApp mode, window can be null if none has had focus */

	if (xfi->window != NULL)
		XDefineCursor(xfi->display, xfi->window->handle, ((xfPointer*) pointer)->cursor);
}
/* Glyph Class */

void xf_Glyph_New(rdpContext* context, rdpGlyph* glyph)
{
	xfInfo* xfi;
	int scanline;
	XImage* image;
	xfGlyph* xf_glyph;

	xf_glyph = (xfGlyph*) glyph;
	xfi = ((xfContext*) context)->xfi;

	scanline = (glyph->cx + 7) / 8;

	xf_glyph->pixmap = XCreatePixmap(xfi->display, xfi->drawing, glyph->cx, glyph->cy, 1);

	image = XCreateImage(xfi->display, xfi->visual, 1,
			ZPixmap, 0, (char*) glyph->aj, glyph->cx, glyph->cy, 8, scanline);

	image->byte_order = MSBFirst;
	image->bitmap_bit_order = MSBFirst;

	XInitImage(image);
	XPutImage(xfi->display, xf_glyph->pixmap, xfi->gc_mono, image, 0, 0, 0, 0, glyph->cx, glyph->cy);
	XFree(image);
}

void xf_Glyph_Free(rdpContext* context, rdpGlyph* glyph)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	if (((xfGlyph*) glyph)->pixmap != 0)
		XFreePixmap(xfi->display, ((xfGlyph*) glyph)->pixmap);
}

void xf_Glyph_Draw(rdpContext* context, rdpGlyph* glyph, int x, int y)
{
	xfGlyph* xf_glyph;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_glyph = (xfGlyph*) glyph;

	XSetStipple(xfi->display, xfi->gc, xf_glyph->pixmap);
	XSetTSOrigin(xfi->display, xfi->gc, x, y);
	XFillRectangle(xfi->display, xfi->drawing, xfi->gc, x, y, glyph->cx, glyph->cy);
	XSetStipple(xfi->display, xfi->gc, xfi->bitmap_mono);
}

void xf_Glyph_BeginDraw(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	bgcolor = (xfi->clrconv->invert)?
		freerdp_color_convert_var_bgr(bgcolor, xfi->srcBpp, 32, xfi->clrconv):
		freerdp_color_convert_var_rgb(bgcolor, xfi->srcBpp, 32, xfi->clrconv);

	fgcolor = (xfi->clrconv->invert)?
		freerdp_color_convert_var_bgr(fgcolor, xfi->srcBpp, 32, xfi->clrconv):
		freerdp_color_convert_var_rgb(fgcolor, xfi->srcBpp, 32, xfi->clrconv);

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, fgcolor);
	XFillRectangle(xfi->display, xfi->drawing, xfi->gc, x, y, width, height);

	XSetForeground(xfi->display, xfi->gc, bgcolor);
	XSetBackground(xfi->display, xfi->gc, fgcolor);
	XSetFillStyle(xfi->display, xfi->gc, FillStippled);
}

void xf_Glyph_EndDraw(rdpContext* context, int x, int y, int width, int height, uint32 bgcolor, uint32 fgcolor)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	if (xfi->drawing == xfi->primary)
	{
		if (xfi->remote_app != true)
		{
			XCopyArea(xfi->display, xfi->primary, xfi->drawable, xfi->gc, x, y, width, height, x, y);
		}

		gdi_InvalidateRegion(xfi->hdc, x, y, width, height);
	}
}

/* Graphics Module */

void xf_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap* bitmap;
	rdpPointer* pointer;
	rdpGlyph* glyph;

	bitmap = xnew(rdpBitmap);
	bitmap->size = sizeof(xfBitmap);

	bitmap->New = xf_Bitmap_New;
	bitmap->Free = xf_Bitmap_Free;
	bitmap->Paint = xf_Bitmap_Paint;
	bitmap->Decompress = xf_Bitmap_Decompress;
	bitmap->SetSurface = xf_Bitmap_SetSurface;

	graphics_register_bitmap(graphics, bitmap);
	xfree(bitmap);

	pointer = xnew(rdpPointer);
	pointer->size = sizeof(xfPointer);

	pointer->New = xf_Pointer_New;
	pointer->Free = xf_Pointer_Free;
	pointer->Set = xf_Pointer_Set;

	graphics_register_pointer(graphics, pointer);
	xfree(pointer);

	glyph = xnew(rdpGlyph);
	glyph->size = sizeof(xfGlyph);

	glyph->New = xf_Glyph_New;
	glyph->Free = xf_Glyph_Free;
	glyph->Draw = xf_Glyph_Draw;
	glyph->BeginDraw = xf_Glyph_BeginDraw;
	glyph->EndDraw = xf_Glyph_EndDraw;

	graphics_register_glyph(graphics, glyph);
	xfree(glyph);
}
