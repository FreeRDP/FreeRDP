/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#include <assert.h>
#include <float.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#ifdef WITH_XRENDER
#include <X11/extensions/Xrender.h>
#include <math.h>
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#endif

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include <X11/XKBlib.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>

#include <freerdp/utils/signal.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/channels.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/file.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>
#include <X11/XKBlib.h>

#include "xf_gdi.h"
#include "xf_rail.h"
#if defined(CHANNEL_TSMF_CLIENT)
#include "xf_tsmf.h"
#endif
#include "xf_event.h"
#include "xf_input.h"
#include "xf_cliprdr.h"
#include "xf_disp.h"
#include "xf_video.h"
#include "xf_monitor.h"
#include "xf_graphics.h"
#include "xf_keyboard.h"
#include "xf_input.h"
#include "xf_channels.h"
#include "xfreerdp.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

#define MIN_PIXEL_DIFF 0.001

static int (*_def_error_handler)(Display*, XErrorEvent*);
static int _xf_error_handler(Display* d, XErrorEvent* ev);
static void xf_check_extensions(xfContext* context);
static void xf_window_free(xfContext* xfc);
static BOOL xf_get_pixmap_info(xfContext* xfc);

#ifdef WITH_XRENDER
static void xf_draw_screen_scaled(xfContext* xfc, int x, int y, int w, int h)
{
	XTransform transform;
	Picture windowPicture;
	Picture primaryPicture;
	XRenderPictureAttributes pa;
	XRenderPictFormat* picFormat;
	double xScalingFactor;
	double yScalingFactor;
	int x2;
	int y2;
	const char* filter;
	rdpSettings* settings = xfc->context.settings;

	if (xfc->scaledWidth <= 0 || xfc->scaledHeight <= 0)
	{
		WLog_ERR(TAG, "the current window dimensions are invalid");
		return;
	}

	if (settings->DesktopWidth <= 0 || settings->DesktopHeight <= 0)
	{
		WLog_ERR(TAG, "the window dimensions are invalid");
		return;
	}

	xScalingFactor = settings->DesktopWidth / (double)xfc->scaledWidth;
	yScalingFactor = settings->DesktopHeight / (double)xfc->scaledHeight;
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, 0);
	/* Black out possible space between desktop and window borders */
	{
		XRectangle box1 = { 0, 0, xfc->window->width, xfc->window->height };
		XRectangle box2 = { xfc->offset_x, xfc->offset_y, xfc->scaledWidth, xfc->scaledHeight };
		Region reg1 = XCreateRegion();
		Region reg2 = XCreateRegion();
		XUnionRectWithRegion(&box1, reg1, reg1);
		XUnionRectWithRegion(&box2, reg2, reg2);

		if (XSubtractRegion(reg1, reg2, reg1) && !XEmptyRegion(reg1))
		{
			XSetRegion(xfc->display, xfc->gc, reg1);
			XFillRectangle(xfc->display, xfc->window->handle, xfc->gc, 0, 0, xfc->window->width,
			               xfc->window->height);
			XSetClipMask(xfc->display, xfc->gc, None);
		}

		XDestroyRegion(reg1);
		XDestroyRegion(reg2);
	}
	picFormat = XRenderFindVisualFormat(xfc->display, xfc->visual);
	pa.subwindow_mode = IncludeInferiors;
	primaryPicture =
	    XRenderCreatePicture(xfc->display, xfc->primary, picFormat, CPSubwindowMode, &pa);
	windowPicture =
	    XRenderCreatePicture(xfc->display, xfc->window->handle, picFormat, CPSubwindowMode, &pa);
	/* avoid blurry filter when scaling factor is 2x, 3x, etc
	 * useful when the client has high-dpi monitor */
	filter = FilterBilinear;
	if (fabs(xScalingFactor - yScalingFactor) < MIN_PIXEL_DIFF)
	{
		const double inverseX = 1.0 / xScalingFactor;
		const double inverseRoundedX = round(inverseX);
		const double absInverse = fabs(inverseX - inverseRoundedX);

		if (absInverse < MIN_PIXEL_DIFF)
			filter = FilterNearest;
	}
	XRenderSetPictureFilter(xfc->display, primaryPicture, filter, 0, 0);
	transform.matrix[0][0] = XDoubleToFixed(xScalingFactor);
	transform.matrix[0][1] = XDoubleToFixed(0.0);
	transform.matrix[0][2] = XDoubleToFixed(0.0);
	transform.matrix[1][0] = XDoubleToFixed(0.0);
	transform.matrix[1][1] = XDoubleToFixed(yScalingFactor);
	transform.matrix[1][2] = XDoubleToFixed(0.0);
	transform.matrix[2][0] = XDoubleToFixed(0.0);
	transform.matrix[2][1] = XDoubleToFixed(0.0);
	transform.matrix[2][2] = XDoubleToFixed(1.0);
	/* calculate and fix up scaled coordinates */
	x2 = x + w;
	y2 = y + h;
	x = floor(x / xScalingFactor) - 1;
	y = floor(y / yScalingFactor) - 1;
	w = ceil(x2 / xScalingFactor) + 1 - x;
	h = ceil(y2 / yScalingFactor) + 1 - y;
	XRenderSetPictureTransform(xfc->display, primaryPicture, &transform);
	XRenderComposite(xfc->display, PictOpSrc, primaryPicture, 0, windowPicture, x, y, 0, 0,
	                 xfc->offset_x + x, xfc->offset_y + y, w, h);
	XRenderFreePicture(xfc->display, primaryPicture);
	XRenderFreePicture(xfc->display, windowPicture);
}

BOOL xf_picture_transform_required(xfContext* xfc)
{
	rdpSettings* settings = xfc->context.settings;

	if ((xfc->offset_x != 0) || (xfc->offset_y != 0) ||
	    (xfc->scaledWidth != (INT64)settings->DesktopWidth) ||
	    (xfc->scaledHeight != (INT64)settings->DesktopHeight))
	{
		return TRUE;
	}

	return FALSE;
}
#endif /* WITH_XRENDER defined */

void xf_draw_screen_(xfContext* xfc, int x, int y, int w, int h, const char* fkt, const char* file,
                     int line)
{
	if (!xfc)
	{
		WLog_DBG(TAG, "[%s] called from [%s] xfc=%p", __FUNCTION__, fkt, xfc);
		return;
	}

	if (w == 0 || h == 0)
	{
		WLog_WARN(TAG, "invalid width and/or height specified: w=%d h=%d", w, h);
		return;
	}

#ifdef WITH_XRENDER

	if (xf_picture_transform_required(xfc))
	{
		xf_draw_screen_scaled(xfc, x, y, w, h);
		return;
	}

#endif
	XCopyArea(xfc->display, xfc->primary, xfc->window->handle, xfc->gc, x, y, w, h, x, y);
}

static BOOL xf_desktop_resize(rdpContext* context)
{
	rdpSettings* settings;
	xfContext* xfc = (xfContext*)context;
	settings = context->settings;

	if (xfc->primary)
	{
		BOOL same = (xfc->primary == xfc->drawing) ? TRUE : FALSE;
		XFreePixmap(xfc->display, xfc->primary);

		if (!(xfc->primary = XCreatePixmap(xfc->display, xfc->drawable, settings->DesktopWidth,
		                                   settings->DesktopHeight, xfc->depth)))
			return FALSE;

		if (same)
			xfc->drawing = xfc->primary;
	}

#ifdef WITH_XRENDER

	if (!xfc->context.settings->SmartSizing)
	{
		xfc->scaledWidth = settings->DesktopWidth;
		xfc->scaledHeight = settings->DesktopHeight;
	}

#endif

	if (!xfc->fullscreen)
	{
		xf_ResizeDesktopWindow(xfc, xfc->window, settings->DesktopWidth, settings->DesktopHeight);
	}
	else
	{
#ifdef WITH_XRENDER

		if (!xfc->context.settings->SmartSizing)
#endif
		{
			/* Update the saved width and height values the window will be
			 * resized to when toggling out of fullscreen */
			xfc->savedWidth = settings->DesktopWidth;
			xfc->savedHeight = settings->DesktopHeight;
		}

		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);
		XSetForeground(xfc->display, xfc->gc, 0);
		XFillRectangle(xfc->display, xfc->drawable, xfc->gc, 0, 0, settings->DesktopWidth,
		               settings->DesktopHeight);
	}

	return TRUE;
}

static BOOL xf_sw_end_paint(rdpContext* context)
{
	int i;
	INT32 x, y;
	UINT32 w, h;
	int ninvalid;
	HGDI_RGN cinvalid;
	xfContext* xfc = (xfContext*)context;
	rdpGdi* gdi = context->gdi;

	if (gdi->suppressOutput)
		return TRUE;

	x = gdi->primary->hdc->hwnd->invalid->x;
	y = gdi->primary->hdc->hwnd->invalid->y;
	w = gdi->primary->hdc->hwnd->invalid->w;
	h = gdi->primary->hdc->hwnd->invalid->h;
	ninvalid = gdi->primary->hdc->hwnd->ninvalid;
	cinvalid = gdi->primary->hdc->hwnd->cinvalid;

	if (!xfc->remote_app)
	{
		if (!xfc->complex_regions)
		{
			if (gdi->primary->hdc->hwnd->invalid->null)
				return TRUE;

			xf_lock_x11(xfc);
			XPutImage(xfc->display, xfc->primary, xfc->gc, xfc->image, x, y, x, y, w, h);
			xf_draw_screen(xfc, x, y, w, h);
			xf_unlock_x11(xfc);
		}
		else
		{
			if (gdi->primary->hdc->hwnd->ninvalid < 1)
				return TRUE;

			xf_lock_x11(xfc);

			for (i = 0; i < ninvalid; i++)
			{
				x = cinvalid[i].x;
				y = cinvalid[i].y;
				w = cinvalid[i].w;
				h = cinvalid[i].h;
				XPutImage(xfc->display, xfc->primary, xfc->gc, xfc->image, x, y, x, y, w, h);
				xf_draw_screen(xfc, x, y, w, h);
			}

			XFlush(xfc->display);
			xf_unlock_x11(xfc);
		}
	}
	else
	{
		if (gdi->primary->hdc->hwnd->invalid->null)
			return TRUE;

		xf_lock_x11(xfc);
		xf_rail_paint(xfc, x, y, x + w, y + h);
		xf_unlock_x11(xfc);
	}

	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	gdi->primary->hdc->hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL xf_sw_desktop_resize(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	xfContext* xfc = (xfContext*)context;
	rdpSettings* settings = context->settings;
	BOOL ret = FALSE;
	xf_lock_x11(xfc);

	if (!gdi_resize(gdi, settings->DesktopWidth, settings->DesktopHeight))
		goto out;

	if (xfc->image)
	{
		xfc->image->data = NULL;
		XDestroyImage(xfc->image);
	}

	if (!(xfc->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
	                                (char*)gdi->primary_buffer, gdi->width, gdi->height,
	                                xfc->scanline_pad, gdi->stride)))
	{
		goto out;
	}

	xfc->image->byte_order = LSBFirst;
	xfc->image->bitmap_bit_order = LSBFirst;
	ret = xf_desktop_resize(context);
out:
	xf_unlock_x11(xfc);
	return ret;
}

static BOOL xf_hw_end_paint(rdpContext* context)
{
	INT32 x, y;
	UINT32 w, h;
	xfContext* xfc = (xfContext*)context;

	if (xfc->context.gdi->suppressOutput)
		return TRUE;

	if (!xfc->remote_app)
	{
		if (!xfc->complex_regions)
		{
			if (xfc->hdc->hwnd->invalid->null)
				return TRUE;

			x = xfc->hdc->hwnd->invalid->x;
			y = xfc->hdc->hwnd->invalid->y;
			w = xfc->hdc->hwnd->invalid->w;
			h = xfc->hdc->hwnd->invalid->h;
			xf_lock_x11(xfc);
			xf_draw_screen(xfc, x, y, w, h);
			xf_unlock_x11(xfc);
		}
		else
		{
			int i;
			int ninvalid;
			HGDI_RGN cinvalid;

			if (xfc->hdc->hwnd->ninvalid < 1)
				return TRUE;

			ninvalid = xfc->hdc->hwnd->ninvalid;
			cinvalid = xfc->hdc->hwnd->cinvalid;
			xf_lock_x11(xfc);

			for (i = 0; i < ninvalid; i++)
			{
				x = cinvalid[i].x;
				y = cinvalid[i].y;
				w = cinvalid[i].w;
				h = cinvalid[i].h;
				xf_draw_screen(xfc, x, y, w, h);
			}

			XFlush(xfc->display);
			xf_unlock_x11(xfc);
		}
	}
	else
	{
		if (xfc->hdc->hwnd->invalid->null)
			return TRUE;

		x = xfc->hdc->hwnd->invalid->x;
		y = xfc->hdc->hwnd->invalid->y;
		w = xfc->hdc->hwnd->invalid->w;
		h = xfc->hdc->hwnd->invalid->h;
		xf_lock_x11(xfc);
		xf_rail_paint(xfc, x, y, x + w, y + h);
		xf_unlock_x11(xfc);
	}

	xfc->hdc->hwnd->invalid->null = TRUE;
	xfc->hdc->hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL xf_hw_desktop_resize(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	xfContext* xfc = (xfContext*)context;
	rdpSettings* settings = context->settings;
	BOOL ret = FALSE;
	xf_lock_x11(xfc);

	if (!gdi_resize(gdi, settings->DesktopWidth, settings->DesktopHeight))
		goto out;

	ret = xf_desktop_resize(context);
out:
	xf_unlock_x11(xfc);
	return ret;
}

static BOOL xf_process_x_events(freerdp* instance)
{
	BOOL status;
	XEvent xevent;
	int pending_status;
	xfContext* xfc = (xfContext*)instance->context;
	status = TRUE;
	pending_status = TRUE;

	while (pending_status)
	{
		xf_lock_x11(xfc);
		pending_status = XPending(xfc->display);

		if (pending_status)
		{
			ZeroMemory(&xevent, sizeof(xevent));
			XNextEvent(xfc->display, &xevent);
			status = xf_event_process(instance, &xevent);
		}
		xf_unlock_x11(xfc);
		if (!status)
			break;
	}

	return status;
}

static char* xf_window_get_title(rdpSettings* settings)
{
	BOOL port;
	char* windowTitle;
	size_t size;
	char* name;
	const char* prefix = "FreeRDP:";

	if (!settings)
		return NULL;

	name = settings->ServerHostname;

	if (settings->WindowTitle)
		return _strdup(settings->WindowTitle);

	port = (settings->ServerPort != 3389);
	/* Just assume a window title is never longer than a filename... */
	size = strnlen(name, MAX_PATH) + 16;
	windowTitle = calloc(size, sizeof(char));

	if (!windowTitle)
		return NULL;

	if (!port)
		sprintf_s(windowTitle, size, "%s %s", prefix, name);
	else
		sprintf_s(windowTitle, size, "%s %s:%i", prefix, name, settings->ServerPort);

	return windowTitle;
}

BOOL xf_create_window(xfContext* xfc)
{
	XGCValues gcv;
	XEvent xevent;
	int width, height;
	char* windowTitle;
	rdpGdi* gdi;
	rdpSettings* settings;
	settings = xfc->context.settings;
	gdi = xfc->context.gdi;
	ZeroMemory(&xevent, sizeof(xevent));
	width = settings->DesktopWidth;
	height = settings->DesktopHeight;

	if (!xfc->hdc)
		if (!(xfc->hdc = gdi_CreateDC(gdi->dstFormat)))
			return FALSE;

	if (!xfc->remote_app)
	{
		xfc->attribs.background_pixel = BlackPixelOfScreen(xfc->screen);
		xfc->attribs.border_pixel = WhitePixelOfScreen(xfc->screen);
		xfc->attribs.backing_store = xfc->primary ? NotUseful : Always;
		xfc->attribs.override_redirect = False;
		xfc->attribs.colormap = xfc->colormap;
		xfc->attribs.bit_gravity = NorthWestGravity;
		xfc->attribs.win_gravity = NorthWestGravity;
#ifdef WITH_XRENDER
		xfc->offset_x = 0;
		xfc->offset_y = 0;
#endif
		windowTitle = xf_window_get_title(settings);

		if (!windowTitle)
			return FALSE;

#ifdef WITH_XRENDER

		if (settings->SmartSizing && !xfc->fullscreen)
		{
			if (settings->SmartSizingWidth)
				width = settings->SmartSizingWidth;

			if (settings->SmartSizingHeight)
				height = settings->SmartSizingHeight;

			xfc->scaledWidth = width;
			xfc->scaledHeight = height;
		}

#endif
		xfc->window = xf_CreateDesktopWindow(xfc, windowTitle, width, height);
		free(windowTitle);

		if (xfc->fullscreen)
			xf_SetWindowFullscreen(xfc, xfc->window, xfc->fullscreen);

		xfc->unobscured = (xevent.xvisibility.state == VisibilityUnobscured);
		XSetWMProtocols(xfc->display, xfc->window->handle, &(xfc->WM_DELETE_WINDOW), 1);
		xfc->drawable = xfc->window->handle;
	}
	else
	{
		xfc->drawable = xf_CreateDummyWindow(xfc);
	}

	ZeroMemory(&gcv, sizeof(gcv));

	if (xfc->modifierMap)
		XFreeModifiermap(xfc->modifierMap);

	xfc->modifierMap = XGetModifierMapping(xfc->display);

	if (!xfc->gc)
		xfc->gc = XCreateGC(xfc->display, xfc->drawable, GCGraphicsExposures, &gcv);

	if (!xfc->primary)
		xfc->primary = XCreatePixmap(xfc->display, xfc->drawable, settings->DesktopWidth,
		                             settings->DesktopHeight, xfc->depth);

	xfc->drawing = xfc->primary;

	if (!xfc->bitmap_mono)
		xfc->bitmap_mono = XCreatePixmap(xfc->display, xfc->drawable, 8, 8, 1);

	if (!xfc->gc_mono)
		xfc->gc_mono = XCreateGC(xfc->display, xfc->bitmap_mono, GCGraphicsExposures, &gcv);

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, BlackPixelOfScreen(xfc->screen));
	XFillRectangle(xfc->display, xfc->primary, xfc->gc, 0, 0, settings->DesktopWidth,
	               settings->DesktopHeight);
	XFlush(xfc->display);

	if (!xfc->image)
	{
		rdpGdi* gdi = xfc->context.gdi;
		xfc->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
		                          (char*)gdi->primary_buffer, settings->DesktopWidth,
		                          settings->DesktopHeight, xfc->scanline_pad, gdi->stride);
		xfc->image->byte_order = LSBFirst;
		xfc->image->bitmap_bit_order = LSBFirst;
	}

	return TRUE;
}

static void xf_window_free(xfContext* xfc)
{
	if (xfc->window)
	{
		xf_DestroyDesktopWindow(xfc, xfc->window);
		xfc->window = NULL;
	}

	if (xfc->hdc)
	{
		gdi_DeleteDC(xfc->hdc);
		xfc->hdc = NULL;
	}

#if defined(CHANNEL_TSMF_CLIENT)
	if (xfc->xv_context)
	{
		xf_tsmf_uninit(xfc, NULL);
		xfc->xv_context = NULL;
	}
#endif

	if (xfc->image)
	{
		xfc->image->data = NULL;
		XDestroyImage(xfc->image);
		xfc->image = NULL;
	}

	if (xfc->bitmap_mono)
	{
		XFreePixmap(xfc->display, xfc->bitmap_mono);
		xfc->bitmap_mono = 0;
	}

	if (xfc->gc_mono)
	{
		XFreeGC(xfc->display, xfc->gc_mono);
		xfc->gc_mono = 0;
	}

	if (xfc->primary)
	{
		XFreePixmap(xfc->display, xfc->primary);
		xfc->primary = 0;
	}

	if (xfc->gc)
	{
		XFreeGC(xfc->display, xfc->gc);
		xfc->gc = 0;
	}

	if (xfc->modifierMap)
	{
		XFreeModifiermap(xfc->modifierMap);
		xfc->modifierMap = NULL;
	}
}

void xf_toggle_fullscreen(xfContext* xfc)
{
	WindowStateChangeEventArgs e;
	rdpContext* context = (rdpContext*)xfc;
	rdpSettings* settings = context->settings;

	/*
	  when debugging, ungrab keyboard when toggling fullscreen
	  to allow keyboard usage on the debugger
	*/
	if (xfc->debug)
	{
		XUngrabKeyboard(xfc->display, CurrentTime);
	}

	xfc->fullscreen = (xfc->fullscreen) ? FALSE : TRUE;
	xfc->decorations = (xfc->fullscreen) ? FALSE : settings->Decorations;
	xf_SetWindowFullscreen(xfc, xfc->window, xfc->fullscreen);
	EventArgsInit(&e, "xfreerdp");
	e.state = xfc->fullscreen ? FREERDP_WINDOW_STATE_FULLSCREEN : 0;
	PubSub_OnWindowStateChange(context->pubSub, context, &e);
}

BOOL xf_toggle_control(xfContext* xfc)
{
	EncomspClientContext* encomsp;
	ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU pdu;
	encomsp = xfc->encomsp;

	if (!encomsp)
		return FALSE;

	pdu.ParticipantId = 0;
	pdu.Flags = ENCOMSP_REQUEST_VIEW;

	if (!xfc->controlToggle)
		pdu.Flags |= ENCOMSP_REQUEST_INTERACT;

	encomsp->ChangeParticipantControlLevel(encomsp, &pdu);
	xfc->controlToggle = !xfc->controlToggle;
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
xf_encomsp_participant_created(EncomspClientContext* context,
                               const ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated)
{
	xfContext* xfc;
	rdpSettings* settings;
	BOOL request;

	if (!context || !context->custom || !participantCreated)
		return ERROR_INVALID_PARAMETER;

	xfc = context->custom;
	settings = xfc->context.settings;

	if (!settings)
		return ERROR_INVALID_PARAMETER;

	request = freerdp_settings_get_bool(settings, FreeRDP_RemoteAssistanceRequestControl);
	if (request && (participantCreated->Flags & ENCOMSP_MAY_VIEW) &&
	    !(participantCreated->Flags & ENCOMSP_MAY_INTERACT))
		xf_toggle_control(xfc);

	return CHANNEL_RC_OK;
}

void xf_encomsp_init(xfContext* xfc, EncomspClientContext* encomsp)
{
	xfc->encomsp = encomsp;
	encomsp->custom = (void*)xfc;
	encomsp->ParticipantCreated = xf_encomsp_participant_created;
}

void xf_encomsp_uninit(xfContext* xfc, EncomspClientContext* encomsp)
{
	WINPR_UNUSED(encomsp);
	xfc->encomsp = NULL;
}

void xf_lock_x11_(xfContext* xfc, const char* fkt)
{

	if (!xfc->UseXThreads)
		WaitForSingleObject(xfc->mutex, INFINITE);
	else
		XLockDisplay(xfc->display);

	if (xfc->locked)
		WLog_WARN(TAG, "%s:\t[%" PRIu32 "] recursive lock from %s", __FUNCTION__, xfc->locked, fkt);
	xfc->locked++;
	WLog_VRB(TAG, "%s:\t[%" PRIu32 "] from %s", __FUNCTION__, xfc->locked, fkt);
}

void xf_unlock_x11_(xfContext* xfc, const char* fkt)
{
	if (xfc->locked == 0)
		WLog_WARN(TAG, "X11: trying to unlock although not locked!");

	WLog_VRB(TAG, "%s:\t[%" PRIu32 "] from %s", __FUNCTION__, xfc->locked - 1, fkt);
	if (!xfc->UseXThreads)
		ReleaseMutex(xfc->mutex);
	else
		XUnlockDisplay(xfc->display);
	xfc->locked--;
}

static BOOL xf_get_pixmap_info(xfContext* xfc)
{
	int i;
	int vi_count;
	int pf_count;
	XVisualInfo* vi;
	XVisualInfo* vis;
	XVisualInfo tpl;
	XPixmapFormatValues* pf;
	XPixmapFormatValues* pfs;
	XWindowAttributes window_attributes;
	assert(xfc->display);
	pfs = XListPixmapFormats(xfc->display, &pf_count);

	if (!pfs)
	{
		WLog_ERR(TAG, "XListPixmapFormats failed");
		return 1;
	}

	for (i = 0; i < pf_count; i++)
	{
		pf = pfs + i;

		if (pf->depth == xfc->depth)
		{
			xfc->scanline_pad = pf->scanline_pad;
			break;
		}
	}

	XFree(pfs);
	ZeroMemory(&tpl, sizeof(tpl));
	tpl.class = TrueColor;
	tpl.screen = xfc->screen_number;

	if (XGetWindowAttributes(xfc->display, RootWindowOfScreen(xfc->screen), &window_attributes) ==
	    0)
	{
		WLog_ERR(TAG, "XGetWindowAttributes failed");
		return FALSE;
	}

	vis = XGetVisualInfo(xfc->display, VisualClassMask | VisualScreenMask, &tpl, &vi_count);

	if (!vis)
	{
		WLog_ERR(TAG, "XGetVisualInfo failed");
		return FALSE;
	}

	vi = vis;

	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->visual == window_attributes.visual)
		{
			xfc->visual = vi->visual;
			break;
		}
	}

	if (xfc->visual)
	{
		/*
		 * Detect if the server visual has an inverted colormap
		 * (BGR vs RGB, or red being the least significant byte)
		 */
		if (vi->red_mask & 0xFF)
		{
			xfc->invert = FALSE;
		}
	}

	XFree(vis);

	if ((xfc->visual == NULL) || (xfc->scanline_pad == 0))
	{
		return FALSE;
	}

	return TRUE;
}

static int xf_error_handler(Display* d, XErrorEvent* ev)
{
	char buf[256];
	int do_abort = TRUE;
	XGetErrorText(d, ev->error_code, buf, sizeof(buf));
	WLog_ERR(TAG, "%s", buf);

	if (do_abort)
		abort();

	_def_error_handler(d, ev);
	return FALSE;
}

static int _xf_error_handler(Display* d, XErrorEvent* ev)
{
	/*
	 * ungrab the keyboard, in case a debugger is running in
	 * another window. This make xf_error_handler() a potential
	 * debugger breakpoint.
	 */

	XUngrabKeyboard(d, CurrentTime);
	return xf_error_handler(d, ev);
}

static BOOL xf_play_sound(rdpContext* context, const PLAY_SOUND_UPDATE* play_sound)
{
	xfContext* xfc = (xfContext*)context;
	WINPR_UNUSED(play_sound);
	XkbBell(xfc->display, None, 100, 0);
	return TRUE;
}

static void xf_check_extensions(xfContext* context)
{
	int xkb_opcode, xkb_event, xkb_error;
	int xkb_major = XkbMajorVersion;
	int xkb_minor = XkbMinorVersion;

	if (XkbLibraryVersion(&xkb_major, &xkb_minor) &&
	    XkbQueryExtension(context->display, &xkb_opcode, &xkb_event, &xkb_error, &xkb_major,
	                      &xkb_minor))
	{
		context->xkbAvailable = TRUE;
	}

#ifdef WITH_XRENDER
	{
		int xrender_event_base;
		int xrender_error_base;

		if (XRenderQueryExtension(context->display, &xrender_event_base, &xrender_error_base))
		{
			context->xrenderAvailable = TRUE;
		}
	}
#endif
}

#ifdef WITH_XI
/* Input device which does NOT have the correct mapping. We must disregard */
/* this device when trying to find the input device which is the pointer.  */
static const char TEST_PTR_STR[] = "Virtual core XTEST pointer";
static const size_t TEST_PTR_LEN = sizeof(TEST_PTR_STR) / sizeof(char);
#endif /* WITH_XI */

static void xf_get_x11_button_map(xfContext* xfc, unsigned char* x11_map)
{
#ifdef WITH_XI
	int opcode, event, error;
	XDevice* ptr_dev;
	XExtensionVersion* version;
	XDeviceInfo* devices1;
	XIDeviceInfo* devices2;
	int i, num_devices;

	if (XQueryExtension(xfc->display, "XInputExtension", &opcode, &event, &error))
	{
		WLog_DBG(TAG, "Searching for XInput pointer device");
		ptr_dev = NULL;
		/* loop through every device, looking for a pointer */
		version = XGetExtensionVersion(xfc->display, INAME);

		if (version->major_version >= 2)
		{
			/* XID of pointer device using XInput version 2 */
			devices2 = XIQueryDevice(xfc->display, XIAllDevices, &num_devices);

			if (devices2)
			{
				for (i = 0; i < num_devices; ++i)
				{
					if ((devices2[i].use == XISlavePointer) &&
					    (strncmp(devices2[i].name, TEST_PTR_STR, TEST_PTR_LEN) != 0))
					{
						ptr_dev = XOpenDevice(xfc->display, devices2[i].deviceid);
						if (ptr_dev)
							break;
					}
				}

				XIFreeDeviceInfo(devices2);
			}
		}
		else
		{
			/* XID of pointer device using XInput version 1 */
			devices1 = XListInputDevices(xfc->display, &num_devices);

			if (devices1)
			{
				for (i = 0; i < num_devices; ++i)
				{
					if ((devices1[i].use == IsXExtensionPointer) &&
					    (strncmp(devices1[i].name, TEST_PTR_STR, TEST_PTR_LEN) != 0))
					{
						ptr_dev = XOpenDevice(xfc->display, devices1[i].id);
						if (ptr_dev)
							break;
					}
				}

				XFreeDeviceList(devices1);
			}
		}

		XFree(version);

		/* get button mapping from input extension if there is a pointer device; */
		/* otherwise leave unchanged.                                            */
		if (ptr_dev)
		{
			WLog_DBG(TAG, "Pointer device: %d", ptr_dev->device_id);
			XGetDeviceButtonMapping(xfc->display, ptr_dev, x11_map, NUM_BUTTONS_MAPPED);
			XCloseDevice(xfc->display, ptr_dev);
		}
		else
		{
			WLog_DBG(TAG, "No pointer device found!");
		}
	}
	else
#endif /* WITH_XI */
	{
		WLog_DBG(TAG, "Get global pointer mapping (no XInput)");
		XGetPointerMapping(xfc->display, x11_map, NUM_BUTTONS_MAPPED);
	}
}

/* Assignment of physical (not logical) mouse buttons to wire flags. */
/* Notice that the middle button is 2 in X11, but 3 in RDP.          */
static const button_map xf_button_flags[NUM_BUTTONS_MAPPED] = {
	{ Button1, PTR_FLAGS_BUTTON1 },
	{ Button2, PTR_FLAGS_BUTTON3 },
	{ Button3, PTR_FLAGS_BUTTON2 },
	{ Button4, PTR_FLAGS_WHEEL | 0x78 },
	/* Negative value is 9bit twos complement */
	{ Button5, PTR_FLAGS_WHEEL | PTR_FLAGS_WHEEL_NEGATIVE | (0x100 - 0x78) },
	{ 6, PTR_FLAGS_HWHEEL | PTR_FLAGS_WHEEL_NEGATIVE | (0x100 - 0x78) },
	{ 7, PTR_FLAGS_HWHEEL | 0x78 },
	{ 8, PTR_XFLAGS_BUTTON1 },
	{ 9, PTR_XFLAGS_BUTTON2 },
	{ 97, PTR_XFLAGS_BUTTON1 },
	{ 112, PTR_XFLAGS_BUTTON2 }
};

static UINT16 get_flags_for_button(int button)
{
	size_t x;

	for (x = 0; x < ARRAYSIZE(xf_button_flags); x++)
	{
		const button_map* map = &xf_button_flags[x];

		if (map->button == button)
			return map->flags;
	}

	return 0;
}

static void xf_button_map_init(xfContext* xfc)
{
	size_t pos = 0;
	/* loop counter for array initialization */
	size_t physical;
	/* logical mouse button which is used for each physical mouse  */
	/* button (indexed from zero). This is the default map.        */
	unsigned char x11_map[112] = { 0 };
	x11_map[0] = Button1;
	x11_map[1] = Button2;
	x11_map[2] = Button3;
	x11_map[3] = Button4;
	x11_map[4] = Button5;
	x11_map[5] = 6;
	x11_map[6] = 7;
	x11_map[7] = 8;
	x11_map[8] = 9;
	x11_map[96] = 97;
	x11_map[111] = 112;

	/* query system for actual remapping */
	if (xfc->context.settings->UnmapButtons)
	{
		xf_get_x11_button_map(xfc, x11_map);
	}

	/* iterate over all (mapped) physical buttons; for each of them */
	/* find the logical button in X11, and assign to this the       */
	/* appropriate value to send over the RDP wire.                 */
	for (physical = 0; physical < ARRAYSIZE(x11_map); ++physical)
	{
		const unsigned char logical = x11_map[physical];
		const UINT16 flags = get_flags_for_button(logical);

		if ((logical != 0) && (flags != 0))
		{
			if (pos >= NUM_BUTTONS_MAPPED)
			{
				WLog_ERR(TAG, "Failed to map mouse button to RDP button, no space");
			}
			else
			{
				button_map* map = &xfc->button_map[pos++];
				map->button = logical;
				map->flags = get_flags_for_button(physical + Button1);
			}
		}
	}
}

/**
 * Callback given to freerdp_connect() to process the pre-connect operations.
 * It will fill the rdp_freerdp structure (instance) with the appropriate options to use for the
 * connection.
 *
 * @param instance - pointer to the rdp_freerdp structure that contains the connection's parameters,
 * and will be filled with the appropriate informations.
 *
 * @return TRUE if successful. FALSE otherwise.
 * Can exit with error code XF_EXIT_PARSE_ARGUMENTS if there is an error in the parameters.
 */
static BOOL xf_pre_connect(freerdp* instance)
{
	rdpChannels* channels;
	rdpSettings* settings;
	rdpContext* context = instance->context;
	xfContext* xfc = (xfContext*)instance->context;
	UINT32 maxWidth = 0;
	UINT32 maxHeight = 0;
	settings = instance->settings;
	channels = context->channels;
	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_XSERVER;
	PubSub_SubscribeChannelConnected(instance->context->pubSub, xf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
	                                    xf_OnChannelDisconnectedEventHandler);

	if (!freerdp_client_load_addins(channels, instance->settings))
		return FALSE;

	if (!settings->Username && !settings->CredentialsFromStdin && !settings->SmartcardLogon)
	{
		int rc;
		char login_name[MAX_PATH] = { 0 };

#ifdef HAVE_GETLOGIN_R
		rc = getlogin_r(login_name, sizeof(login_name));
#else
		strncpy(login_name, getlogin(), sizeof(login_name));
		rc = 0;
#endif
		if (rc == 0)
		{
			settings->Username = _strdup(login_name);

			if (!settings->Username)
				return FALSE;

			WLog_INFO(TAG, "No user name set. - Using login name: %s", settings->Username);
		}
	}

	if (settings->AuthenticationOnly)
	{
		/* Check +auth-only has a username and password. */
		if (!settings->Password)
		{
			WLog_INFO(TAG, "auth-only, but no password set. Please provide one.");
			return FALSE;
		}

		WLog_INFO(TAG, "Authentication only. Don't connect to X.");
	}

	if (!xf_keyboard_init(xfc))
		return FALSE;

	xf_detect_monitors(xfc, &maxWidth, &maxHeight);

	if (maxWidth && maxHeight)
	{
		settings->DesktopWidth = maxWidth;
		settings->DesktopHeight = maxHeight;
	}

#ifdef WITH_XRENDER

	/**
	 * If /f is specified in combination with /smart-sizing:widthxheight then
	 * we run the session in the /smart-sizing dimensions scaled to full screen
	 */
	if (settings->Fullscreen && settings->SmartSizing && settings->SmartSizingWidth &&
	    settings->SmartSizingHeight)
	{
		settings->DesktopWidth = settings->SmartSizingWidth;
		settings->DesktopHeight = settings->SmartSizingHeight;
	}

#endif
	xfc->fullscreen = settings->Fullscreen;
	xfc->decorations = settings->Decorations;
	xfc->grab_keyboard = settings->GrabKeyboard;
	xfc->fullscreen_toggle = settings->ToggleFullscreen;
	xf_button_map_init(xfc);
	return TRUE;
}

/**
 * Callback given to freerdp_connect() to perform post-connection operations.
 * It will be called only if the connection was initialized properly, and will continue the
 * initialization based on the newly created connection.
 */
static BOOL xf_post_connect(freerdp* instance)
{
	rdpUpdate* update;
	rdpContext* context;
	rdpSettings* settings;
	ResizeWindowEventArgs e;
	xfContext* xfc = (xfContext*)instance->context;
	context = instance->context;
	settings = instance->settings;
	update = context->update;

	if (!gdi_init(instance, xf_get_local_color_format(xfc, TRUE)))
		return FALSE;

	if (!xf_register_pointer(context->graphics))
		return FALSE;

	if (!settings->SoftwareGdi)
	{
		if (!xf_register_graphics(context->graphics))
		{
			WLog_ERR(TAG, "failed to register graphics");
			return FALSE;
		}

		xf_gdi_register_update_callbacks(update);
		brush_cache_register_callbacks(instance->update);
		glyph_cache_register_callbacks(instance->update);
		bitmap_cache_register_callbacks(instance->update);
		offscreen_cache_register_callbacks(instance->update);
		palette_cache_register_callbacks(instance->update);
	}

#ifdef WITH_XRENDER
	xfc->scaledWidth = settings->DesktopWidth;
	xfc->scaledHeight = settings->DesktopHeight;
	xfc->offset_x = 0;
	xfc->offset_y = 0;
#endif

	if (!xfc->xrenderAvailable)
	{
		if (settings->SmartSizing)
		{
			WLog_ERR(TAG, "XRender not available: disabling smart-sizing");
			settings->SmartSizing = FALSE;
		}

		if (settings->MultiTouchGestures)
		{
			WLog_ERR(TAG, "XRender not available: disabling local multi-touch gestures");
			settings->MultiTouchGestures = FALSE;
		}
	}

	if (settings->RemoteApplicationMode)
		xfc->remote_app = TRUE;

	if (!xf_create_window(xfc))
	{
		WLog_ERR(TAG, "xf_create_window failed");
		return FALSE;
	}

	if (settings->SoftwareGdi)
	{
		update->EndPaint = xf_sw_end_paint;
		update->DesktopResize = xf_sw_desktop_resize;
	}
	else
	{
		update->EndPaint = xf_hw_end_paint;
		update->DesktopResize = xf_hw_desktop_resize;
	}

	update->PlaySound = xf_play_sound;
	update->SetKeyboardIndicators = xf_keyboard_set_indicators;
	update->SetKeyboardImeStatus = xf_keyboard_set_ime_status;

	if (settings->RedirectClipboard && !(xfc->clipboard = xf_clipboard_new(xfc)))
		return FALSE;

	if (!(xfc->xfDisp = xf_disp_new(xfc)))
	{
		xf_clipboard_free(xfc->clipboard);
		return FALSE;
	}

	EventArgsInit(&e, "xfreerdp");
	e.width = settings->DesktopWidth;
	e.height = settings->DesktopHeight;
	PubSub_OnResizeWindow(context->pubSub, xfc, &e);
	return TRUE;
}

static void xf_post_disconnect(freerdp* instance)
{
	xfContext* xfc;
	rdpContext* context;

	if (!instance || !instance->context)
		return;

	context = instance->context;
	xfc = (xfContext*)context;
	PubSub_UnsubscribeChannelConnected(instance->context->pubSub,
	                                   xf_OnChannelConnectedEventHandler);
	PubSub_UnsubscribeChannelDisconnected(instance->context->pubSub,
	                                      xf_OnChannelDisconnectedEventHandler);
	gdi_free(instance);

	if (xfc->clipboard)
	{
		xf_clipboard_free(xfc->clipboard);
		xfc->clipboard = NULL;
	}

	if (xfc->xfDisp)
	{
		xf_disp_free(xfc->xfDisp);
		xfc->xfDisp = NULL;
	}

	if ((xfc->window != NULL) && (xfc->drawable == xfc->window->handle))
		xfc->drawable = 0;
	else
		xf_DestroyDummyWindow(xfc, xfc->drawable);

	xf_window_free(xfc);
	xf_keyboard_free(xfc);
}

static int xf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	xfContext* xfc = (xfContext*)instance->context;
	const char* str_data = freerdp_get_logon_error_info_data(data);
	const char* str_type = freerdp_get_logon_error_info_type(type);
	WLog_INFO(TAG, "Logon Error Info %s [%s]", str_data, str_type);
	xf_rail_disable_remoteapp_mode(xfc);
	return 1;
}

static DWORD WINAPI xf_input_thread(LPVOID arg)
{
	BOOL running = TRUE;
	DWORD status;
	DWORD nCount;
	HANDLE events[3];
	wMessage msg;
	wMessageQueue* queue;
	freerdp* instance = (freerdp*)arg;
	xfContext* xfc = (xfContext*)instance->context;
	queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);
	nCount = 0;
	events[nCount++] = MessageQueue_Event(queue);
	events[nCount++] = xfc->x11event;
	events[nCount++] = instance->context->abortEvent;

	while (running)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		switch (status)
		{
			case WAIT_OBJECT_0:
			case WAIT_OBJECT_0 + 1:
			case WAIT_OBJECT_0 + 2:
				if (WaitForSingleObject(events[0], 0) == WAIT_OBJECT_0)
				{
					if (MessageQueue_Peek(queue, &msg, FALSE))
					{
						if (msg.id == WMQ_QUIT)
							running = FALSE;
					}
				}

				if (WaitForSingleObject(events[1], 0) == WAIT_OBJECT_0)
				{
					if (!xf_process_x_events(xfc->context.instance))
					{
						running = FALSE;
						break;
					}
				}

				if (WaitForSingleObject(events[2], 0) == WAIT_OBJECT_0)
					running = FALSE;

				break;

			default:
				running = FALSE;
				break;
		}
	}

	MessageQueue_PostQuit(queue, 0);
	freerdp_abort_connect(xfc->context.instance);
	ExitThread(0);
	return 0;
}

static BOOL handle_window_events(freerdp* instance)
{
	rdpSettings* settings;

	if (!instance || !instance->settings)
		return FALSE;

	settings = instance->settings;

	if (!settings->AsyncInput)
	{
		if (!xf_process_x_events(instance))
		{
			WLog_INFO(TAG, "Closed from X11");
			return FALSE;
		}
	}

	return TRUE;
}

/** Main loop for the rdp connection.
 *  It will be run from the thread's entry point (thread_func()).
 *  It initiates the connection, and will continue to run until the session ends,
 *  processing events as they are received.
 *  @param instance - pointer to the rdp_freerdp structure that contains the session's settings
 *  @return A code from the enum XF_EXIT_CODE (0 if successful)
 */
static DWORD WINAPI xf_client_thread(LPVOID param)
{
	BOOL status;
	DWORD exit_code = 0;
	DWORD nCount;
	DWORD waitStatus;
	HANDLE handles[64];
	xfContext* xfc;
	freerdp* instance;
	rdpContext* context;
	HANDLE inputEvent = NULL;
	HANDLE inputThread = NULL;
	HANDLE timer = NULL;
	LARGE_INTEGER due;
	rdpSettings* settings;
	TimerEventArgs timerEvent;
	EventArgsInit(&timerEvent, "xfreerdp");
	instance = (freerdp*)param;
	context = instance->context;
	status = freerdp_connect(instance);
	xfc = (xfContext*)instance->context;

	if (!status)
	{
		if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_AUTHENTICATION_FAILED)
			exit_code = XF_EXIT_AUTH_FAILURE;
		else if (freerdp_get_last_error(instance->context) ==
		         FREERDP_ERROR_SECURITY_NEGO_CONNECT_FAILED)
			exit_code = XF_EXIT_NEGO_FAILURE;
		else
			exit_code = XF_EXIT_CONN_FAILED;
	}
	else
		exit_code = XF_EXIT_SUCCESS;

	if (!status)
		goto end;

	/* --authonly ? */
	if (instance->settings->AuthenticationOnly)
	{
		WLog_ERR(TAG, "Authentication only, exit status %" PRId32 "", !status);
		goto disconnect;
	}

	if (!status)
	{
		WLog_ERR(TAG, "Freerdp connect error exit status %" PRId32 "", !status);
		exit_code = freerdp_error_info(instance);

		if (freerdp_get_last_error(instance->context) == FREERDP_ERROR_AUTHENTICATION_FAILED)
			exit_code = XF_EXIT_AUTH_FAILURE;
		else if (exit_code == ERRINFO_SUCCESS)
			exit_code = XF_EXIT_CONN_FAILED;

		goto disconnect;
	}

	settings = context->settings;
	timer = CreateWaitableTimerA(NULL, FALSE, "mainloop-periodic-timer");

	if (!timer)
	{
		WLog_ERR(TAG, "failed to create timer");
		goto disconnect;
	}

	due.QuadPart = 0;

	if (!SetWaitableTimer(timer, &due, 20, NULL, NULL, FALSE))
	{
		goto disconnect;
	}

	if (!settings->AsyncInput)
	{
		inputEvent = xfc->x11event;
	}
	else
	{
		if (!(inputThread = CreateThread(NULL, 0, xf_input_thread, instance, 0, NULL)))
		{
			WLog_ERR(TAG, "async input: failed to create input thread");
			exit_code = XF_EXIT_UNKNOWN;
			goto disconnect;
		}
	}

	while (!freerdp_shall_disconnect(instance))
	{
		nCount = 0;
		handles[nCount++] = timer;

		if (!settings->AsyncInput)
			handles[nCount++] = inputEvent;

		/*
		 * win8 and server 2k12 seem to have some timing issue/race condition
		 * when a initial sync request is send to sync the keyboard indicators
		 * sending the sync event twice fixed this problem
		 */
		if (freerdp_focus_required(instance))
		{
			xf_keyboard_focus_in(xfc);
			xf_keyboard_focus_in(xfc);
		}

		{
			DWORD tmp =
			    freerdp_get_event_handles(context, &handles[nCount], ARRAYSIZE(handles) - nCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "freerdp_get_event_handles failed");
				break;
			}

			nCount += tmp;
		}

		if (xfc->window)
			xf_floatbar_hide_and_show(xfc->window->floatbar);

		waitStatus = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);

		if (waitStatus == WAIT_FAILED)
			break;

		{
			if (!freerdp_check_event_handles(context))
			{
				if (client_auto_reconnect_ex(instance, handle_window_events))
					continue;
				else
				{
					/*
					 * Indicate an unsuccessful connection attempt if reconnect
					 * did not succeed and no other error was specified.
					 */
					if (freerdp_error_info(instance) == 0)
						exit_code = XF_EXIT_CONN_FAILED;
				}

				if (freerdp_get_last_error(context) == FREERDP_ERROR_SUCCESS)
					WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");

				break;
			}
		}

		if (!handle_window_events(instance))
			break;

		if ((status != WAIT_TIMEOUT) && (waitStatus == WAIT_OBJECT_0))
		{
			timerEvent.now = GetTickCount64();
			PubSub_OnTimer(context->pubSub, context, &timerEvent);
		}
	}

	if (settings->AsyncInput)
	{
		WaitForSingleObject(inputThread, INFINITE);
		CloseHandle(inputThread);
	}

	if (!exit_code)
	{
		exit_code = freerdp_error_info(instance);

		if (exit_code == XF_EXIT_DISCONNECT &&
		    freerdp_get_disconnect_ultimatum(context) == Disconnect_Ultimatum_user_requested)
		{
			/* This situation might be limited to Windows XP. */
			WLog_INFO(TAG, "Error info says user did not initiate but disconnect ultimatum says "
			               "they did; treat this as a user logoff");
			exit_code = XF_EXIT_LOGOFF;
		}
	}

disconnect:

	if (timer)
		CloseHandle(timer);

	freerdp_disconnect(instance);
end:
	ExitThread(exit_code);
	return exit_code;
}

DWORD xf_exit_code_from_disconnect_reason(DWORD reason)
{
	if (reason == 0 || (reason >= XF_EXIT_PARSE_ARGUMENTS && reason <= XF_EXIT_NEGO_FAILURE))
		return reason;
	/* License error set */
	else if (reason >= 0x100 && reason <= 0x10A)
		reason -= 0x100 + XF_EXIT_LICENSE_INTERNAL;
	/* RDP protocol error set */
	else if (reason >= 0x10c9 && reason <= 0x1193)
		reason = XF_EXIT_RDP;
	/* There's no need to test protocol-independent codes: they match */
	else if (!(reason <= 0xC))
		reason = XF_EXIT_UNKNOWN;

	return reason;
}

static void xf_TerminateEventHandler(void* context, TerminateEventArgs* e)
{
	rdpContext* ctx = (rdpContext*)context;
	WINPR_UNUSED(e);
	freerdp_abort_connect(ctx->instance);
}

#ifdef WITH_XRENDER
static void xf_ZoomingChangeEventHandler(void* context, ZoomingChangeEventArgs* e)
{
	xfContext* xfc = (xfContext*)context;
	rdpSettings* settings = xfc->context.settings;
	int w = xfc->scaledWidth + e->dx;
	int h = xfc->scaledHeight + e->dy;

	if (e->dx == 0 && e->dy == 0)
		return;

	if (w < 10)
		w = 10;

	if (h < 10)
		h = 10;

	if (w == xfc->scaledWidth && h == xfc->scaledHeight)
		return;

	xfc->scaledWidth = w;
	xfc->scaledHeight = h;
	xf_draw_screen(xfc, 0, 0, settings->DesktopWidth, settings->DesktopHeight);
}

static void xf_PanningChangeEventHandler(void* context, PanningChangeEventArgs* e)
{
	xfContext* xfc = (xfContext*)context;
	rdpSettings* settings = xfc->context.settings;

	if (e->dx == 0 && e->dy == 0)
		return;

	xfc->offset_x += e->dx;
	xfc->offset_y += e->dy;
	xf_draw_screen(xfc, 0, 0, settings->DesktopWidth, settings->DesktopHeight);
}
#endif

/**
 * Client Interface
 */

static BOOL xfreerdp_client_global_init()
{
	setlocale(LC_ALL, "");

	if (freerdp_handle_signals() != 0)
		return FALSE;

	return TRUE;
}

static void xfreerdp_client_global_uninit()
{
}

static int xfreerdp_client_start(rdpContext* context)
{
	xfContext* xfc = (xfContext*)context;
	rdpSettings* settings = context->settings;

	if (!settings->ServerHostname)
	{
		WLog_ERR(TAG, "error: server hostname was not specified with /v:<server>[:port]");
		return -1;
	}

	if (!(xfc->thread = CreateThread(NULL, 0, xf_client_thread, context->instance, 0, NULL)))
	{
		WLog_ERR(TAG, "failed to create client thread");
		return -1;
	}

	return 0;
}

static int xfreerdp_client_stop(rdpContext* context)
{
	xfContext* xfc = (xfContext*)context;
	freerdp_abort_connect(context->instance);

	if (xfc->thread)
	{
		WaitForSingleObject(xfc->thread, INFINITE);
		CloseHandle(xfc->thread);
		xfc->thread = NULL;
	}

	return 0;
}

static Atom get_supported_atom(xfContext* xfc, const char* atomName)
{
	unsigned long i;
	const Atom atom = XInternAtom(xfc->display, atomName, False);

	for (i = 0; i < xfc->supportedAtomCount; i++)
	{
		if (xfc->supportedAtoms[i] == atom)
			return atom;
	}

	return None;
}
static BOOL xfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	xfContext* xfc = (xfContext*)instance->context;
	assert(context);
	assert(xfc);
	assert(!xfc->display);
	assert(!xfc->mutex);
	assert(!xfc->x11event);
	instance->PreConnect = xf_pre_connect;
	instance->PostConnect = xf_post_connect;
	instance->PostDisconnect = xf_post_disconnect;
	instance->Authenticate = client_cli_authenticate;
	instance->GatewayAuthenticate = client_cli_gw_authenticate;
	instance->VerifyCertificateEx = client_cli_verify_certificate_ex;
	instance->VerifyChangedCertificateEx = client_cli_verify_changed_certificate_ex;
	instance->PresentGatewayMessage = client_cli_present_gateway_message;
	instance->LogonErrorInfo = xf_logon_error_info;
	PubSub_SubscribeTerminate(context->pubSub, xf_TerminateEventHandler);
#ifdef WITH_XRENDER
	PubSub_SubscribeZoomingChange(context->pubSub, xf_ZoomingChangeEventHandler);
	PubSub_SubscribePanningChange(context->pubSub, xf_PanningChangeEventHandler);
#endif
	xfc->UseXThreads = TRUE;
	/* uncomment below if debugging to prevent keyboard grap */
	/* xfc->debug = TRUE; */

	if (xfc->UseXThreads)
	{
		if (!XInitThreads())
		{
			WLog_WARN(TAG, "XInitThreads() failure");
			xfc->UseXThreads = FALSE;
		}
	}

	xfc->display = XOpenDisplay(NULL);

	if (!xfc->display)
	{
		WLog_ERR(TAG, "failed to open display: %s", XDisplayName(NULL));
		WLog_ERR(TAG, "Please check that the $DISPLAY environment variable is properly set.");
		goto fail_open_display;
	}

	xfc->mutex = CreateMutex(NULL, FALSE, NULL);

	if (!xfc->mutex)
	{
		WLog_ERR(TAG, "Could not create mutex!");
		goto fail_create_mutex;
	}

	xfc->xfds = ConnectionNumber(xfc->display);
	xfc->screen_number = DefaultScreen(xfc->display);
	xfc->screen = ScreenOfDisplay(xfc->display, xfc->screen_number);
	xfc->depth = DefaultDepthOfScreen(xfc->screen);
	xfc->big_endian = (ImageByteOrder(xfc->display) == MSBFirst);
	xfc->invert = TRUE;
	xfc->complex_regions = TRUE;
	xfc->_NET_SUPPORTED = XInternAtom(xfc->display, "_NET_SUPPORTED", True);
	xfc->_NET_SUPPORTING_WM_CHECK = XInternAtom(xfc->display, "_NET_SUPPORTING_WM_CHECK", True);

	if ((xfc->_NET_SUPPORTED != None) && (xfc->_NET_SUPPORTING_WM_CHECK != None))
	{
		Atom actual_type = 0;
		int actual_format = 0;
		unsigned long nitems = 0, after = 0;
		unsigned char* data = NULL;
		int status = XGetWindowProperty(xfc->display, RootWindowOfScreen(xfc->screen),
		                                xfc->_NET_SUPPORTED, 0, 1024, False, XA_ATOM, &actual_type,
		                                &actual_format, &nitems, &after, &data);

		if ((status == Success) && (actual_type == XA_ATOM) && (actual_format == 32))
		{
			xfc->supportedAtomCount = nitems;
			xfc->supportedAtoms = calloc(nitems, sizeof(Atom));
			memcpy(xfc->supportedAtoms, data, nitems * sizeof(Atom));
		}

		if (data)
			XFree(data);
	}

	xfc->_XWAYLAND_MAY_GRAB_KEYBOARD =
	    XInternAtom(xfc->display, "_XWAYLAND_MAY_GRAB_KEYBOARD", False);
	xfc->_NET_WM_ICON = XInternAtom(xfc->display, "_NET_WM_ICON", False);
	xfc->_MOTIF_WM_HINTS = XInternAtom(xfc->display, "_MOTIF_WM_HINTS", False);
	xfc->_NET_CURRENT_DESKTOP = XInternAtom(xfc->display, "_NET_CURRENT_DESKTOP", False);
	xfc->_NET_WORKAREA = XInternAtom(xfc->display, "_NET_WORKAREA", False);
	xfc->_NET_WM_STATE = get_supported_atom(xfc, "_NET_WM_STATE");
	xfc->_NET_WM_STATE_FULLSCREEN = get_supported_atom(xfc, "_NET_WM_STATE_FULLSCREEN");
	xfc->_NET_WM_STATE_MAXIMIZED_HORZ =
	    XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
	xfc->_NET_WM_STATE_MAXIMIZED_VERT =
	    XInternAtom(xfc->display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
	xfc->_NET_WM_FULLSCREEN_MONITORS = get_supported_atom(xfc, "_NET_WM_FULLSCREEN_MONITORS");
	xfc->_NET_WM_NAME = XInternAtom(xfc->display, "_NET_WM_NAME", False);
	xfc->_NET_WM_PID = XInternAtom(xfc->display, "_NET_WM_PID", False);
	xfc->_NET_WM_WINDOW_TYPE = XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE", False);
	xfc->_NET_WM_WINDOW_TYPE_NORMAL =
	    XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	xfc->_NET_WM_WINDOW_TYPE_DIALOG =
	    XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	xfc->_NET_WM_WINDOW_TYPE_POPUP = XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_POPUP", False);
	xfc->_NET_WM_WINDOW_TYPE_POPUP_MENU =
	    XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
	xfc->_NET_WM_WINDOW_TYPE_UTILITY =
	    XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	xfc->_NET_WM_WINDOW_TYPE_DROPDOWN_MENU =
	    XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
	xfc->_NET_WM_STATE_SKIP_TASKBAR =
	    XInternAtom(xfc->display, "_NET_WM_STATE_SKIP_TASKBAR", False);
	xfc->_NET_WM_STATE_SKIP_PAGER = XInternAtom(xfc->display, "_NET_WM_STATE_SKIP_PAGER", False);
	xfc->_NET_WM_MOVERESIZE = XInternAtom(xfc->display, "_NET_WM_MOVERESIZE", False);
	xfc->_NET_MOVERESIZE_WINDOW = XInternAtom(xfc->display, "_NET_MOVERESIZE_WINDOW", False);
	xfc->UTF8_STRING = XInternAtom(xfc->display, "UTF8_STRING", FALSE);
	xfc->WM_PROTOCOLS = XInternAtom(xfc->display, "WM_PROTOCOLS", False);
	xfc->WM_DELETE_WINDOW = XInternAtom(xfc->display, "WM_DELETE_WINDOW", False);
	xfc->WM_STATE = XInternAtom(xfc->display, "WM_STATE", False);
	xfc->x11event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, xfc->xfds, WINPR_FD_READ);

	if (!xfc->x11event)
	{
		WLog_ERR(TAG, "Could not create xfds event");
		goto fail_xfds_event;
	}

	xfc->colormap = DefaultColormap(xfc->display, xfc->screen_number);

	if (xfc->debug)
	{
		WLog_INFO(TAG, "Enabling X11 debug mode.");
		XSynchronize(xfc->display, TRUE);
		_def_error_handler = XSetErrorHandler(_xf_error_handler);
	}

	xf_check_extensions(xfc);

	if (!xf_get_pixmap_info(xfc))
	{
		WLog_ERR(TAG, "Failed to get pixmap info");
		goto fail_pixmap_info;
	}

	xfc->vscreen.monitors = calloc(16, sizeof(MONITOR_INFO));

	if (!xfc->vscreen.monitors)
		goto fail_vscreen_monitors;

	return TRUE;
fail_vscreen_monitors:
fail_pixmap_info:
	CloseHandle(xfc->x11event);
	xfc->x11event = NULL;
fail_xfds_event:
	CloseHandle(xfc->mutex);
	xfc->mutex = NULL;
fail_create_mutex:
	XCloseDisplay(xfc->display);
	xfc->display = NULL;
fail_open_display:
	return FALSE;
}

static void xfreerdp_client_free(freerdp* instance, rdpContext* context)
{
	xfContext* xfc = (xfContext*)instance->context;

	if (!context)
		return;

	PubSub_UnsubscribeTerminate(context->pubSub, xf_TerminateEventHandler);
#ifdef WITH_XRENDER
	PubSub_UnsubscribeZoomingChange(context->pubSub, xf_ZoomingChangeEventHandler);
	PubSub_UnsubscribePanningChange(context->pubSub, xf_PanningChangeEventHandler);
#endif

	if (xfc->display)
	{
		XCloseDisplay(xfc->display);
		xfc->display = NULL;
	}

	if (xfc->x11event)
	{
		CloseHandle(xfc->x11event);
		xfc->x11event = NULL;
	}

	if (xfc->mutex)
	{
		CloseHandle(xfc->mutex);
		xfc->mutex = NULL;
	}

	if (xfc->vscreen.monitors)
	{
		free(xfc->vscreen.monitors);
		xfc->vscreen.monitors = NULL;
	}

	free(xfc->supportedAtoms);
}

int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->Version = 1;
	pEntryPoints->Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	pEntryPoints->GlobalInit = xfreerdp_client_global_init;
	pEntryPoints->GlobalUninit = xfreerdp_client_global_uninit;
	pEntryPoints->ContextSize = sizeof(xfContext);
	pEntryPoints->ClientNew = xfreerdp_client_new;
	pEntryPoints->ClientFree = xfreerdp_client_free;
	pEntryPoints->ClientStart = xfreerdp_client_start;
	pEntryPoints->ClientStop = xfreerdp_client_stop;
	return 0;
}
