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

#include <freerdp/config.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#include <float.h>
#include <math.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/rfx.h>

#include "xf_graphics.h"
#include "xf_event.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

static BOOL xf_Pointer_Set(rdpContext* context, rdpPointer* pointer);

BOOL xf_decode_color(xfContext* xfc, const UINT32 srcColor, XColor* color)
{
	UINT32 SrcFormat = 0;
	BYTE r = 0, g = 0, b = 0, a = 0;

	if (!xfc || !color)
		return FALSE;

	rdpGdi* gdi = xfc->common.context.gdi;

	if (!gdi)
		return FALSE;

	rdpSettings* settings = xfc->common.context.settings;

	if (!settings)
		return FALSE;

	switch (freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth))
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

	FreeRDPSplitColor(srcColor, SrcFormat, &r, &g, &b, &a, &gdi->palette);
	color->blue = (unsigned short)(b << 8);
	color->green = (unsigned short)(g << 8);
	color->red = (unsigned short)(r << 8);
	color->flags = DoRed | DoGreen | DoBlue;

	if (XAllocColor(xfc->display, xfc->colormap, color) == 0)
		return FALSE;

	return TRUE;
}

static BOOL xf_Pointer_GetCursorForCurrentScale(rdpContext* context, rdpPointer* pointer,
                                                Cursor* cursor)
{
#if defined(WITH_XCURSOR) && defined(WITH_XRENDER)
	xfContext* xfc = (xfContext*)context;
	xfPointer* xpointer = (xfPointer*)pointer;
	XcursorImage ci = { 0 };
	int cursorIndex = -1;

	if (!context || !pointer || !context->gdi)
		return FALSE;

	rdpSettings* settings = xfc->common.context.settings;

	if (!settings)
		return FALSE;

	const double xscale = (freerdp_settings_get_bool(settings, FreeRDP_SmartSizing)
	                           ? xfc->scaledWidth / (double)freerdp_settings_get_uint32(
	                                                    settings, FreeRDP_DesktopWidth)
	                           : 1);
	const double yscale = (freerdp_settings_get_bool(settings, FreeRDP_SmartSizing)
	                           ? xfc->scaledHeight / (double)freerdp_settings_get_uint32(
	                                                     settings, FreeRDP_DesktopHeight)
	                           : 1);
	const UINT32 xTargetSize = MAX(1, pointer->width * xscale);
	const UINT32 yTargetSize = MAX(1, pointer->height * yscale);

	WLog_DBG(TAG, "scaled: %" PRIu32 "x%" PRIu32 ", desktop: %" PRIu32 "x%" PRIu32,
	         xfc->scaledWidth, xfc->scaledHeight,
	         freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
	         freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight));
	for (UINT32 i = 0; i < xpointer->nCursors; i++)
	{
		if ((xpointer->cursorWidths[i] == xTargetSize) &&
		    (xpointer->cursorHeights[i] == yTargetSize))
		{
			cursorIndex = i;
		}
	}

	if (cursorIndex == -1)
	{
		UINT32 CursorFormat = 0;
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
		const size_t size = 1ull * ci.height * ci.width * FreeRDPGetBytesPerPixel(CursorFormat);

		void* tmp = winpr_aligned_malloc(size, 16);
		if (!tmp)
		{
			xf_unlock_x11(xfc);
			return FALSE;
		}
		ci.pixels = (XcursorPixel*)tmp;

		const double xs = fabs(fabs(xscale) - 1.0);
		const double ys = fabs(fabs(yscale) - 1.0);

		WLog_DBG(TAG,
		         "cursorIndex %" PRId32 " scaling pointer %" PRIu32 "x%" PRIu32 " --> %" PRIu32
		         "x%" PRIu32 " [%lfx%lf]",
		         cursorIndex, pointer->width, pointer->height, ci.width, ci.height, xscale, yscale);
		if ((xs > DBL_EPSILON) || (ys > DBL_EPSILON))
		{
			if (!freerdp_image_scale((BYTE*)ci.pixels, CursorFormat, 0, 0, 0, ci.width, ci.height,
			                         (BYTE*)xpointer->cursorPixels, CursorFormat, 0, 0, 0,
			                         pointer->width, pointer->height))
			{
				winpr_aligned_free(tmp);
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

		winpr_aligned_free(tmp);

		xf_unlock_x11(xfc);
	}
	else
	{
		WLog_DBG(TAG, "using cached cursor %" PRId32, cursorIndex);
	}

	cursor[0] = xpointer->cursors[cursorIndex];
#endif
	return TRUE;
}

/* Pointer Class */
static Window xf_Pointer_get_window(xfContext* xfc)
{
	if (!xfc)
	{
		WLog_WARN(TAG, "xf_Pointer: Invalid context");
		return 0;
	}
	if (xfc->remote_app)
	{
		if (!xfc->appWindow)
		{
			WLog_WARN(TAG, "xf_Pointer: Invalid appWindow");
			return 0;
		}
		return xfc->appWindow->handle;
	}
	else
	{
		if (!xfc->window)
		{
			WLog_WARN(TAG, "xf_Pointer: Invalid window");
			return 0;
		}
		return xfc->window->handle;
	}
}

BOOL xf_pointer_update_scale(xfContext* xfc)
{
	xfPointer* pointer;
	WINPR_ASSERT(xfc);

	pointer = xfc->pointer;
	if (!pointer)
		return TRUE;

	return xf_Pointer_Set(&xfc->common.context, &xfc->pointer->pointer);
}

static BOOL xf_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	BOOL rc = FALSE;

#ifdef WITH_XCURSOR
	UINT32 CursorFormat;
	size_t size;
	xfContext* xfc = (xfContext*)context;
	xfPointer* xpointer = (xfPointer*)pointer;

	if (!context || !pointer || !context->gdi)
		goto fail;

	if (!xfc->invert)
		CursorFormat = (!xfc->big_endian) ? PIXEL_FORMAT_RGBA32 : PIXEL_FORMAT_ABGR32;
	else
		CursorFormat = (!xfc->big_endian) ? PIXEL_FORMAT_BGRA32 : PIXEL_FORMAT_ARGB32;

	xpointer->nCursors = 0;
	xpointer->mCursors = 0;

	size = 1ull * pointer->height * pointer->width * FreeRDPGetBytesPerPixel(CursorFormat);

	if (!(xpointer->cursorPixels = (XcursorPixel*)winpr_aligned_malloc(size, 16)))
		goto fail;

	if (!freerdp_image_copy_from_pointer_data(
	        (BYTE*)xpointer->cursorPixels, CursorFormat, 0, 0, 0, pointer->width, pointer->height,
	        pointer->xorMaskData, pointer->lengthXorMask, pointer->andMaskData,
	        pointer->lengthAndMask, pointer->xorBpp, &context->gdi->palette))
	{
		winpr_aligned_free(xpointer->cursorPixels);
		goto fail;
	}

#endif

	rc = TRUE;

fail:
	WLog_DBG(TAG, "%p", rc ? pointer : NULL);
	return rc;
}

static void xf_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	WLog_DBG(TAG, "%p", pointer);

#ifdef WITH_XCURSOR
	UINT32 i;
	xfContext* xfc = (xfContext*)context;
	xfPointer* xpointer = (xfPointer*)pointer;

	xf_lock_x11(xfc);

	winpr_aligned_free(xpointer->cursorPixels);
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

static BOOL xf_Pointer_Set(rdpContext* context, rdpPointer* pointer)
{
	WLog_DBG(TAG, "%p", pointer);
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*)context;
	Window handle = xf_Pointer_get_window(xfc);

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(pointer);

	xfc->pointer = (xfPointer*)pointer;

	/* in RemoteApp mode, window can be null if none has had focus */

	if (handle)
	{
		if (!xf_Pointer_GetCursorForCurrentScale(context, pointer, &(xfc->pointer->cursor)))
			return FALSE;
		xf_lock_x11(xfc);
		XDefineCursor(xfc->display, handle, xfc->pointer->cursor);
		xf_unlock_x11(xfc);
	}
	else
	{
		WLog_WARN(TAG, "handle=%ld", handle);
	}
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetNull(rdpContext* context)
{
	WLog_DBG(TAG, "called");
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*)context;
	static Cursor nullcursor = None;
	Window handle = xf_Pointer_get_window(xfc);
	xf_lock_x11(xfc);

	if (nullcursor == None)
	{
		XcursorImage ci = { 0 };
		XcursorPixel xp = 0;

		ci.version = XCURSOR_IMAGE_VERSION;
		ci.size = sizeof(ci);
		ci.width = ci.height = 1;
		ci.xhot = ci.yhot = 0;
		ci.pixels = &xp;
		nullcursor = XcursorImageLoadCursor(xfc->display, &ci);
	}

	xfc->pointer = NULL;

	if ((handle) && (nullcursor != None))
		XDefineCursor(xfc->display, handle, nullcursor);

	xf_unlock_x11(xfc);
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetDefault(rdpContext* context)
{
	WLog_DBG(TAG, "called");
#ifdef WITH_XCURSOR
	xfContext* xfc = (xfContext*)context;
	Window handle = xf_Pointer_get_window(xfc);
	xf_lock_x11(xfc);
	xfc->pointer = NULL;

	if (handle)
		XUndefineCursor(xfc->display, handle);

	xf_unlock_x11(xfc);
#endif
	return TRUE;
}

static BOOL xf_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
	xfContext* xfc = (xfContext*)context;
	XWindowAttributes current = { 0 };
	XSetWindowAttributes tmp = { 0 };
	BOOL ret = FALSE;
	Status rc = 0;
	Window handle = xf_Pointer_get_window(xfc);

	if (!handle)
	{
		WLog_WARN(TAG, "focus %d, handle%lu", xfc->focused, handle);
		return TRUE;
	}

	WLog_DBG(TAG, "%" PRIu32 "x%" PRIu32, x, y);
	if (!xfc->focused)
		return TRUE;

	xf_adjust_coordinates_to_screen(xfc, &x, &y);

	xf_lock_x11(xfc);

	rc = XGetWindowAttributes(xfc->display, handle, &current);
	if (rc == 0)
	{
		WLog_WARN(TAG, "XGetWindowAttributes==%d", rc);
		goto out;
	}

	tmp.event_mask = (current.your_event_mask & ~(PointerMotionMask));

	rc = XChangeWindowAttributes(xfc->display, handle, CWEventMask, &tmp);
	if (rc == 0)
	{
		WLog_WARN(TAG, "XChangeWindowAttributes==%d", rc);
		goto out;
	}

	rc = XWarpPointer(xfc->display, handle, handle, 0, 0, 0, 0, x, y);
	if (rc == 0)
		WLog_WARN(TAG, "XWarpPointer==%d", rc);
	tmp.event_mask = current.your_event_mask;
	rc = XChangeWindowAttributes(xfc->display, handle, CWEventMask, &tmp);
	if (rc == 0)
		WLog_WARN(TAG, "2.try XChangeWindowAttributes==%d", rc);
	ret = TRUE;
out:
	xf_unlock_x11(xfc);
	return ret;
}

/* Graphics Module */
BOOL xf_register_pointer(rdpGraphics* graphics)
{
	rdpPointer pointer = { 0 };

	pointer.size = sizeof(xfPointer);
	pointer.New = xf_Pointer_New;
	pointer.Free = xf_Pointer_Free;
	pointer.Set = xf_Pointer_Set;
	pointer.SetNull = xf_Pointer_SetNull;
	pointer.SetDefault = xf_Pointer_SetDefault;
	pointer.SetPosition = xf_Pointer_SetPosition;
	graphics_register_pointer(graphics, &pointer);
	return TRUE;
}

UINT32 xf_get_local_color_format(xfContext* xfc, BOOL aligned)
{
	UINT32 DstFormat;
	BOOL invert = FALSE;

	if (!xfc)
		return 0;

	invert = xfc->invert;

	WINPR_ASSERT(xfc->depth != 0);
	if (xfc->depth == 32)
		DstFormat = (!invert) ? PIXEL_FORMAT_RGBA32 : PIXEL_FORMAT_BGRA32;
	else if (xfc->depth == 30)
		DstFormat = (!invert) ? PIXEL_FORMAT_RGBX32_DEPTH30 : PIXEL_FORMAT_BGRX32_DEPTH30;
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
