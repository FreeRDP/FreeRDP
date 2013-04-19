/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

/* Bitmap Class */

void xf_Bitmap_New(rdpContext* context, rdpBitmap* bitmap)
{
	BYTE* data;
	Pixmap pixmap;
	XImage* image;
	xfContext* context_ = (xfContext*) context;
	xfInfo* xfi = context_->xfi;

	xf_lock_x11(xfi, FALSE);

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	pixmap = XCreatePixmap(xfi->display, xfi->drawable, bitmap->width, bitmap->height, xfi->depth);

	if (bitmap->data != NULL)
	{
		data = freerdp_image_convert(bitmap->data, NULL,
				bitmap->width, bitmap->height, context_->settings->ColorDepth, xfi->bpp, xfi->clrconv);

		if (bitmap->ephemeral != TRUE)
		{
			image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
				ZPixmap, 0, (char*) data, bitmap->width, bitmap->height, xfi->scanline_pad, 0);

			XPutImage(xfi->display, pixmap, xfi->gc, image, 0, 0, 0, 0, bitmap->width, bitmap->height);
			XFree(image);

			if (data != bitmap->data)
				free(data);
		}
		else
		{
			if (data != bitmap->data)
				free(bitmap->data);

			bitmap->data = data;
		}
	}

	((xfBitmap*) bitmap)->pixmap = pixmap;

	xf_unlock_x11(xfi, FALSE);
}

void xf_Bitmap_Free(rdpContext* context, rdpBitmap* bitmap)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_lock_x11(xfi, FALSE);

	if (((xfBitmap*) bitmap)->pixmap != 0)
		XFreePixmap(xfi->display, ((xfBitmap*) bitmap)->pixmap);

	xf_unlock_x11(xfi, FALSE);
}

void xf_Bitmap_Paint(rdpContext* context, rdpBitmap* bitmap)
{
	XImage* image;
	int width, height;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	width = bitmap->right - bitmap->left + 1;
	height = bitmap->bottom - bitmap->top + 1;

	xf_lock_x11(xfi, FALSE);

	XSetFunction(xfi->display, xfi->gc, GXcopy);

	image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
			ZPixmap, 0, (char*) bitmap->data, bitmap->width, bitmap->height, xfi->scanline_pad, 0);

	XPutImage(xfi->display, xfi->primary, xfi->gc,
			image, 0, 0, bitmap->left, bitmap->top, width, height);

	XFree(image);

	gdi_InvalidateRegion(xfi->hdc, bitmap->left, bitmap->top, width, height);

	xf_unlock_x11(xfi, FALSE);
}

void xf_Bitmap_Decompress(rdpContext* context, rdpBitmap* bitmap,
		BYTE* data, int width, int height, int bpp, int length,
		BOOL compressed, int codec_id)
{
	UINT16 size;
	BYTE* src;
	BYTE* dst;
	int yindex;
	int xindex;
	xfInfo* xfi;
	BOOL status;
	RFX_MESSAGE* msg;

	xfi = ((xfContext*) context)->xfi;

	size = width * height * (bpp + 7) / 8;

	if (bitmap->data == NULL)
		bitmap->data = (BYTE*) malloc(size);
	else
		bitmap->data = (BYTE*) realloc(bitmap->data, size);

	switch (codec_id)
	{
		case RDP_CODEC_ID_NSCODEC:
			fprintf(stderr, "xf_Bitmap_Decompress: nsc not done\n");
			break;

		case RDP_CODEC_ID_REMOTEFX:
			rfx_context_set_pixel_format(xfi->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);
			msg = rfx_process_message(xfi->rfx_context, data, length);

			if (msg == NULL)
			{
				fprintf(stderr, "xf_Bitmap_Decompress: rfx Decompression Failed\n");
			}
			else
			{
				for (yindex = 0; yindex < height; yindex++)
				{
					src = msg->tiles[0]->data + yindex * 64 * 4;
					dst = bitmap->data + yindex * width * 3;
					for (xindex = 0; xindex < width; xindex++)
					{
						*(dst++) = *(src++);
						*(dst++) = *(src++);
						*(dst++) = *(src++);
						src++;
					}
				}
				rfx_message_free(xfi->rfx_context, msg);
			}
			break;

		case RDP_CODEC_ID_JPEG:
			if (!jpeg_decompress(data, bitmap->data, width, height, length, bpp))
			{
				fprintf(stderr, "xf_Bitmap_Decompress: jpeg Decompression Failed\n");
			}
			break;

		default:
			if (compressed)
			{
				status = bitmap_decompress(data, bitmap->data, width, height, length, bpp, bpp);

				if (status == FALSE)
				{
					fprintf(stderr, "xf_Bitmap_Decompress: Bitmap Decompression Failed\n");
				}
			}
			else
			{
				freerdp_image_flip(data, bitmap->data, width, height, bpp);
			}
			break;
	}

	bitmap->compressed = FALSE;
	bitmap->length = size;
	bitmap->bpp = bpp;
}

void xf_Bitmap_SetSurface(rdpContext* context, rdpBitmap* bitmap, BOOL primary)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_lock_x11(xfi, FALSE);

	if (primary)
		xfi->drawing = xfi->primary;
	else
		xfi->drawing = ((xfBitmap*) bitmap)->pixmap;

	xf_unlock_x11(xfi, FALSE);
}

/* Pointer Class */

void xf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	XcursorImage ci;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_lock_x11(xfi, FALSE);

	ZeroMemory(&ci, sizeof(ci));
	ci.version = XCURSOR_IMAGE_VERSION;
	ci.size = sizeof(ci);
	ci.width = pointer->width;
	ci.height = pointer->height;
	ci.xhot = pointer->xPos;
	ci.yhot = pointer->yPos;

	ci.pixels = (XcursorPixel*) malloc(ci.width * ci.height * 4);
	ZeroMemory(ci.pixels, ci.width * ci.height * 4);

	if ((pointer->andMaskData != 0) && (pointer->xorMaskData != 0))
	{
		freerdp_alpha_cursor_convert((BYTE*) (ci.pixels), pointer->xorMaskData, pointer->andMaskData,
				pointer->width, pointer->height, pointer->xorBpp, xfi->clrconv);
	}

	((xfPointer*) pointer)->cursor = XcursorImageLoadCursor(xfi->display, &ci);

	free(ci.pixels);

	xf_unlock_x11(xfi, FALSE);
#endif
}

void xf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_lock_x11(xfi, FALSE);

	if (((xfPointer*) pointer)->cursor != 0)
		XFreeCursor(xfi->display, ((xfPointer*) pointer)->cursor);

	xf_unlock_x11(xfi, FALSE);
#endif
}

void xf_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
#ifdef WITH_XCURSOR
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_lock_x11(xfi, FALSE);

	/* in RemoteApp mode, window can be null if none has had focus */

	if (xfi->window != NULL)
		XDefineCursor(xfi->display, xfi->window->handle, ((xfPointer*) pointer)->cursor);

	xf_unlock_x11(xfi, FALSE);
#endif
}

void xf_Pointer_SetNull(rdpContext* context)
{
#ifdef WITH_XCURSOR
	xfInfo* xfi = ((xfContext*) context)->xfi;
	static Cursor nullcursor = None;

	xf_lock_x11(xfi, FALSE);

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
		nullcursor = XcursorImageLoadCursor(xfi->display, &ci);
	}

	if (xfi->window != NULL && nullcursor != None)
		XDefineCursor(xfi->display, xfi->window->handle, nullcursor);

	xf_unlock_x11(xfi, FALSE);
#endif
}

void xf_Pointer_SetDefault(rdpContext* context)
{
#ifdef WITH_XCURSOR
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_lock_x11(xfi, FALSE);

	if (xfi->window != NULL)
		XUndefineCursor(xfi->display, xfi->window->handle);

	xf_unlock_x11(xfi, FALSE);
#endif
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

	xf_lock_x11(xfi, FALSE);

	scanline = (glyph->cx + 7) / 8;

	xf_glyph->pixmap = XCreatePixmap(xfi->display, xfi->drawing, glyph->cx, glyph->cy, 1);

	image = XCreateImage(xfi->display, xfi->visual, 1,
			ZPixmap, 0, (char*) glyph->aj, glyph->cx, glyph->cy, 8, scanline);

	image->byte_order = MSBFirst;
	image->bitmap_bit_order = MSBFirst;

	XInitImage(image);
	XPutImage(xfi->display, xf_glyph->pixmap, xfi->gc_mono, image, 0, 0, 0, 0, glyph->cx, glyph->cy);
	XFree(image);

	xf_unlock_x11(xfi, FALSE);
}

void xf_Glyph_Free(rdpContext* context, rdpGlyph* glyph)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_lock_x11(xfi, FALSE);

	if (((xfGlyph*) glyph)->pixmap != 0)
		XFreePixmap(xfi->display, ((xfGlyph*) glyph)->pixmap);

	xf_unlock_x11(xfi, FALSE);
}

void xf_Glyph_Draw(rdpContext* context, rdpGlyph* glyph, int x, int y)
{
	xfGlyph* xf_glyph;
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_glyph = (xfGlyph*) glyph;

	xf_lock_x11(xfi, FALSE);

	XSetStipple(xfi->display, xfi->gc, xf_glyph->pixmap);
	XSetTSOrigin(xfi->display, xfi->gc, x, y);
	XFillRectangle(xfi->display, xfi->drawing, xfi->gc, x, y, glyph->cx, glyph->cy);
	XSetStipple(xfi->display, xfi->gc, xfi->bitmap_mono);

	xf_unlock_x11(xfi, FALSE);
}

void xf_Glyph_BeginDraw(rdpContext* context, int x, int y, int width, int height, UINT32 bgcolor, UINT32 fgcolor)
{
	xfContext* context_ = (xfContext*) context;
	xfInfo* xfi = context_->xfi;

	bgcolor = (xfi->clrconv->invert)?
		freerdp_color_convert_var_bgr(bgcolor, context_->settings->ColorDepth, xfi->bpp, xfi->clrconv):
		freerdp_color_convert_var_rgb(bgcolor, context_->settings->ColorDepth, xfi->bpp, xfi->clrconv);

	fgcolor = (xfi->clrconv->invert)?
		freerdp_color_convert_var_bgr(fgcolor, context_->settings->ColorDepth, xfi->bpp, xfi->clrconv):
		freerdp_color_convert_var_rgb(fgcolor, context_->settings->ColorDepth, xfi->bpp, xfi->clrconv);

	xf_lock_x11(xfi, FALSE);

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, fgcolor);
	XFillRectangle(xfi->display, xfi->drawing, xfi->gc, x, y, width, height);

	XSetForeground(xfi->display, xfi->gc, bgcolor);
	XSetBackground(xfi->display, xfi->gc, fgcolor);
	XSetFillStyle(xfi->display, xfi->gc, FillStippled);

	xf_unlock_x11(xfi, FALSE);
}

void xf_Glyph_EndDraw(rdpContext* context, int x, int y, int width, int height, UINT32 bgcolor, UINT32 fgcolor)
{
	xfInfo* xfi = ((xfContext*) context)->xfi;

	xf_lock_x11(xfi, FALSE);

	if (xfi->drawing == xfi->primary)
	{
		gdi_InvalidateRegion(xfi->hdc, x, y, width, height);
	}

	xf_unlock_x11(xfi, FALSE);
}

/* Graphics Module */

void xf_register_graphics(rdpGraphics* graphics)
{
	rdpBitmap* bitmap;
	rdpPointer* pointer;
	rdpGlyph* glyph;

	bitmap = (rdpBitmap*) malloc(sizeof(rdpBitmap));
	ZeroMemory(bitmap, sizeof(rdpBitmap));

	bitmap->size = sizeof(xfBitmap);

	bitmap->New = xf_Bitmap_New;
	bitmap->Free = xf_Bitmap_Free;
	bitmap->Paint = xf_Bitmap_Paint;
	bitmap->Decompress = xf_Bitmap_Decompress;
	bitmap->SetSurface = xf_Bitmap_SetSurface;

	graphics_register_bitmap(graphics, bitmap);
	free(bitmap);

	pointer = (rdpPointer*) malloc(sizeof(rdpPointer));
	ZeroMemory(pointer, sizeof(rdpPointer));

	pointer->size = sizeof(xfPointer);

	pointer->New = xf_Pointer_New;
	pointer->Free = xf_Pointer_Free;
	pointer->Set = xf_Pointer_Set;
	pointer->SetNull = xf_Pointer_SetNull;
	pointer->SetDefault = xf_Pointer_SetDefault;

	graphics_register_pointer(graphics, pointer);
	free(pointer);

	glyph = (rdpGlyph*) malloc(sizeof(rdpGlyph));
	ZeroMemory(glyph, sizeof(rdpGlyph));

	glyph->size = sizeof(xfGlyph);

	glyph->New = xf_Glyph_New;
	glyph->Free = xf_Glyph_Free;
	glyph->Draw = xf_Glyph_Draw;
	glyph->BeginDraw = xf_Glyph_BeginDraw;
	glyph->EndDraw = xf_Glyph_EndDraw;

	graphics_register_glyph(graphics, glyph);
	free(glyph);
}
