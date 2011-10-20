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

	pixmap = XCreatePixmap(xfi->display, xfi->drawable, bitmap->width, bitmap->height, xfi->depth);

	if (bitmap->data != NULL)
	{
		data = freerdp_image_convert(bitmap->data, NULL,
				bitmap->width, bitmap->height, bitmap->bpp, xfi->bpp, xfi->clrconv);

		image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
				ZPixmap, 0, (char*) data, bitmap->width, bitmap->height, xfi->scanline_pad, 0);

		XPutImage(xfi->display, pixmap, xfi->gc, image, 0, 0, 0, 0, bitmap->width, bitmap->height);

		if (data != bitmap->data)
			xfree(data);
	}

	((xfBitmap*) bitmap)->pixmap = pixmap;
	((xfBitmap*) bitmap)->image = image;
}

void xf_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	if (((xfBitmap*) bitmap)->pixmap != 0)
		XFreePixmap(xfi->display, ((xfBitmap*) bitmap)->pixmap);
}

void xf_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap, int x, int y)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	XPutImage(xfi->display, xfi->primary, xfi->gc,
			((xfBitmap*) bitmap)->image, 0, 0, x, y, bitmap->width, bitmap->height);

	if (xfi->remote_app != True)
		XCopyArea(xfi->display, xfi->primary, xfi->drawable, xfi->gc, x, y, bitmap->width, bitmap->height, x, y);

	gdi_InvalidateRegion(xfi->hdc, x, y, bitmap->width, bitmap->height);
}

void xf_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		uint8* data, int width, int height, int bpp, int length, boolean compressed)
{
	uint16 size;

	size = width * height * (bpp / 8);

	if (bitmap->data == NULL)
		bitmap->data = (uint8*) xmalloc(size);
	else
		bitmap->data = (uint8*) xrealloc(bitmap->data, size);

	if (compressed)
	{
		boolean status;

		status = bitmap_decompress(data, bitmap->data,
				width, height, length, bpp, bpp);

		if (status != True)
		{
			printf("Bitmap Decompression Failed\n");
		}
	}
	else
	{
		freerdp_image_flip(data, data, width, height, bpp);
	}

	bitmap->width = width;
	bitmap->height = height;
	bitmap->compressed = False;
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

	if (pointer->xorBpp > 24)
	{
		freerdp_image_swap_color_order((uint8*) ci.pixels, ci.width, ci.height);
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

	if (xfi->remote_app != True)
		XDefineCursor(xfi->display, xfi->window->handle, ((xfPointer*) pointer)->cursor);
}

/* Graphics Module */

void xf_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap bitmap;
	rdpPointer pointer;

	memset(&bitmap, 0, sizeof(rdpBitmap));
	bitmap.size = sizeof(xfBitmap);

	bitmap.New = xf_Bitmap_New;
	bitmap.Free = xf_Bitmap_Free;
	bitmap.Paint = xf_Bitmap_Paint;
	bitmap.Decompress = xf_Bitmap_Decompress;
	bitmap.SetSurface = xf_Bitmap_SetSurface;

	memset(&pointer, 0, sizeof(rdpPointer));
	pointer.size = sizeof(xfPointer);
	pointer.New = xf_Pointer_New;
	pointer.Free = xf_Pointer_Free;
	pointer.Set = xf_Pointer_Set;

	graphics_register_bitmap(graphics, &bitmap);
	graphics_register_pointer(graphics, &pointer);
}
