/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

#include <assert.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef WITH_XRENDER
#include <X11/extensions/Xrender.h>
#include <math.h>
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
#endif

#ifdef WITH_XCURSOR
#include <X11/Xcursor/Xcursor.h>
#endif

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#ifdef WITH_XI
#include <X11/extensions/XInput2.h>
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
#include <X11/XKBlib.h>

#include "xf_gdi.h"
#include "xf_rail.h"
#include "xf_tsmf.h"
#include "xf_event.h"
#include "xf_input.h"
#include "xf_cliprdr.h"
#include "xf_monitor.h"
#include "xf_graphics.h"
#include "xf_keyboard.h"
#include "xf_input.h"
#include "xf_channels.h"
#include "xfreerdp.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("x11")

static const size_t password_size = 512;

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

	if (xfc->scaledWidth <= 0 || xfc->scaledHeight <= 0)
	{
		WLog_ERR(TAG, "the current window dimensions are invalid");
		return;
	}

	if (xfc->width <= 0 || xfc->height <= 0)
	{
		WLog_ERR(TAG, "the window dimensions are invalid");
		return;
	}

	xScalingFactor = xfc->width / (double)xfc->scaledWidth;
	yScalingFactor = xfc->height / (double)xfc->scaledHeight;

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
			XFillRectangle(xfc->display, xfc->window->handle, xfc->gc, 0, 0, xfc->window->width, xfc->window->height);
			XSetClipMask(xfc->display, xfc->gc, None);
		}

		XDestroyRegion(reg1);
		XDestroyRegion(reg2);
	}

	picFormat = XRenderFindVisualFormat(xfc->display, xfc->visual);

	pa.subwindow_mode = IncludeInferiors;
	primaryPicture = XRenderCreatePicture(xfc->display, xfc->primary, picFormat, CPSubwindowMode, &pa);
	windowPicture = XRenderCreatePicture(xfc->display, xfc->window->handle, picFormat, CPSubwindowMode, &pa);

	XRenderSetPictureFilter(xfc->display, primaryPicture, FilterBilinear, 0, 0);

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
	XRenderComposite(xfc->display, PictOpSrc, primaryPicture, 0, windowPicture, x, y, 0, 0, xfc->offset_x + x, xfc->offset_y + y, w, h);
	XRenderFreePicture(xfc->display, primaryPicture);
	XRenderFreePicture(xfc->display, windowPicture);
}

BOOL xf_picture_transform_required(xfContext* xfc)
{
	if (xfc->offset_x || xfc->offset_y ||
	    xfc->scaledWidth != xfc->width ||
	    xfc->scaledHeight != xfc->height)
	{
		return TRUE;
	}

	return FALSE;
}
#endif /* WITH_XRENDER defined */

void xf_draw_screen(xfContext* xfc, int x, int y, int w, int h)
{
	if (w == 0 || h == 0)
	{
		WLog_WARN(TAG, "invalid width and/or height specified: w=%d h=%d", w, h);
		return;
	}

#ifdef WITH_XRENDER
	if (xf_picture_transform_required(xfc)) {
		xf_draw_screen_scaled(xfc, x, y, w, h);
		return;
	}
#endif
	XCopyArea(xfc->display, xfc->primary, xfc->window->handle, xfc->gc, x, y, w, h, x, y);
}

static void xf_desktop_resize(rdpContext* context)
{
	rdpSettings* settings;
	xfContext* xfc = (xfContext*) context;
	settings = xfc->settings;

	if (xfc->primary)
	{
		BOOL same = (xfc->primary == xfc->drawing) ? TRUE : FALSE;
		XFreePixmap(xfc->display, xfc->primary);
		xfc->primary = XCreatePixmap(xfc->display, xfc->drawable, xfc->width, xfc->height, xfc->depth);
		if (same)
			xfc->drawing = xfc->primary;
	}

#ifdef WITH_XRENDER
	if (!xfc->settings->SmartSizing)
	{
		xfc->scaledWidth = xfc->width;
		xfc->scaledHeight = xfc->height;
	}
#endif

	if (!xfc->fullscreen)
	{
		if (xfc->window)
			xf_ResizeDesktopWindow(xfc, xfc->window, settings->DesktopWidth, settings->DesktopHeight);
	}
	else
	{
		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);
		XSetForeground(xfc->display, xfc->gc, 0);
		XFillRectangle(xfc->display, xfc->drawable, xfc->gc, 0, 0, xfc->window->width, xfc->window->height);
	}
}

void xf_sw_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void xf_sw_end_paint(rdpContext* context)
{
	int i;
	INT32 x, y;
	UINT32 w, h;
	int ninvalid;
	HGDI_RGN cinvalid;
	xfContext* xfc = (xfContext*) context;
	rdpGdi* gdi = context->gdi;

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
				return;

			xf_lock_x11(xfc, FALSE);

			XPutImage(xfc->display, xfc->primary, xfc->gc, xfc->image, x, y, x, y, w, h);

			xf_draw_screen(xfc, x, y, w, h);

			xf_unlock_x11(xfc, FALSE);
		}
		else
		{
			if (gdi->primary->hdc->hwnd->ninvalid < 1)
				return;

			xf_lock_x11(xfc, FALSE);

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

			xf_unlock_x11(xfc, FALSE);
		}
	}
	else
	{
		if (gdi->primary->hdc->hwnd->invalid->null)
			return;

		xf_lock_x11(xfc, FALSE);

		xf_rail_paint(xfc, x, y, x + w - 1, y + h - 1);

		xf_unlock_x11(xfc, FALSE);
	}
}

void xf_sw_desktop_resize(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	xfContext* xfc = (xfContext*) context;

	xf_lock_x11(xfc, TRUE);

	xfc->width = context->settings->DesktopWidth;
	xfc->height = context->settings->DesktopHeight;

	gdi_resize(gdi, xfc->width, xfc->height);

	if (xfc->image)
	{
		xfc->image->data = NULL;
		XDestroyImage(xfc->image);

		xfc->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
				(char*) gdi->primary_buffer, gdi->width, gdi->height, xfc->scanline_pad, 0);
	}

	xf_desktop_resize(context);

	xf_unlock_x11(xfc, TRUE);
}

void xf_hw_begin_paint(rdpContext* context)
{
	xfContext* xfc = (xfContext*) context;
	xfc->hdc->hwnd->invalid->null = 1;
	xfc->hdc->hwnd->ninvalid = 0;
}

void xf_hw_end_paint(rdpContext* context)
{
	INT32 x, y;
	UINT32 w, h;
	xfContext* xfc = (xfContext*) context;

	if (!xfc->remote_app)
	{
		if (!xfc->complex_regions)
		{
			if (xfc->hdc->hwnd->invalid->null)
				return;

			x = xfc->hdc->hwnd->invalid->x;
			y = xfc->hdc->hwnd->invalid->y;
			w = xfc->hdc->hwnd->invalid->w;
			h = xfc->hdc->hwnd->invalid->h;

			xf_lock_x11(xfc, FALSE);

			xf_draw_screen(xfc, x, y, w, h);

			xf_unlock_x11(xfc, FALSE);
		}
		else
		{
			int i;
			int ninvalid;
			HGDI_RGN cinvalid;

			if (xfc->hdc->hwnd->ninvalid < 1)
				return;

			ninvalid = xfc->hdc->hwnd->ninvalid;
			cinvalid = xfc->hdc->hwnd->cinvalid;

			xf_lock_x11(xfc, FALSE);

			for (i = 0; i < ninvalid; i++)
			{
				x = cinvalid[i].x;
				y = cinvalid[i].y;
				w = cinvalid[i].w;
				h = cinvalid[i].h;

				xf_draw_screen(xfc, x, y, w, h);
			}

			XFlush(xfc->display);

			xf_unlock_x11(xfc, FALSE);
		}
	}
	else
	{
		if (xfc->hdc->hwnd->invalid->null)
			return;

		x = xfc->hdc->hwnd->invalid->x;
		y = xfc->hdc->hwnd->invalid->y;
		w = xfc->hdc->hwnd->invalid->w;
		h = xfc->hdc->hwnd->invalid->h;

		xf_lock_x11(xfc, FALSE);

		xf_rail_paint(xfc, x, y, x + w - 1, y + h - 1);

		xf_unlock_x11(xfc, FALSE);
	}
}

void xf_hw_desktop_resize(rdpContext* context)
{
	xfContext* xfc = (xfContext*) context;
	rdpSettings* settings = xfc->settings;

	xf_lock_x11(xfc, TRUE);

	xfc->width = settings->DesktopWidth;
	xfc->height = settings->DesktopHeight;

	xf_desktop_resize(context);

	xf_unlock_x11(xfc, TRUE);
}

BOOL xf_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	xfContext* xfc = (xfContext*) instance->context;
	rfds[*rcount] = (void*)(long)(xfc->xfds);
	(*rcount)++;
	return TRUE;
}

BOOL xf_process_x_events(freerdp* instance)
{
	BOOL status;
	XEvent xevent;
	int pending_status;
	xfContext* xfc = (xfContext*) instance->context;

	status = TRUE;
	pending_status = TRUE;

	while (pending_status)
	{
		xf_lock_x11(xfc, FALSE);
		pending_status = XPending(xfc->display);
		xf_unlock_x11(xfc, FALSE);

		if (pending_status)
		{
			ZeroMemory(&xevent, sizeof(xevent));
			XNextEvent(xfc->display, &xevent);

			status = xf_event_process(instance, &xevent);

			if (!status)
				return status;
		}
	}
	return status;
}

BOOL xf_create_window(xfContext* xfc)
{
	XGCValues gcv;
	XEvent xevent;
	int width, height;
	char* windowTitle;
	rdpSettings* settings = xfc->settings;

	ZeroMemory(&xevent, sizeof(xevent));

	xf_detect_monitors(xfc);

	width = xfc->width;
	height = xfc->height;

	if (!xfc->hdc)
		xfc->hdc = gdi_CreateDC(CLRBUF_32BPP, xfc->bpp);

	if (!xfc->remote_app)
	{
		xfc->attribs.background_pixel = BlackPixelOfScreen(xfc->screen);
		xfc->attribs.border_pixel = WhitePixelOfScreen(xfc->screen);
		xfc->attribs.backing_store = xfc->primary ? NotUseful : Always;
		xfc->attribs.override_redirect = xfc->grab_keyboard ? xfc->fullscreen : False;
		xfc->attribs.colormap = xfc->colormap;
		xfc->attribs.bit_gravity = NorthWestGravity;
		xfc->attribs.win_gravity = NorthWestGravity;

#ifdef WITH_XRENDER
		xfc->offset_x = 0;
		xfc->offset_y = 0;
#endif

		if (settings->WindowTitle)
		{
			windowTitle = _strdup(settings->WindowTitle);
		}
		else if (settings->ServerPort == 3389)
		{
			windowTitle = malloc(1 + sizeof("FreeRDP: ") + strlen(settings->ServerHostname));
			sprintf(windowTitle, "FreeRDP: %s", settings->ServerHostname);
		}
		else
		{
			windowTitle = malloc(1 + sizeof("FreeRDP: ") + strlen(settings->ServerHostname) + sizeof(":00000"));
			sprintf(windowTitle, "FreeRDP: %s:%i", settings->ServerHostname, settings->ServerPort);
		}

		if (xfc->fullscreen)
		{
			width = xfc->desktopWidth;
			height = xfc->desktopHeight;
		}

#ifdef WITH_XRENDER
		if (settings->SmartSizing)
		{
			if (xfc->fullscreen)
			{
				if (xfc->window)
				{
					settings->SmartSizingWidth = xfc->window->width;
					settings->SmartSizingHeight = xfc->window->height;
				}
			}
			else
			{
				if (settings->SmartSizingWidth)
					width = settings->SmartSizingWidth;
				if (settings->SmartSizingHeight)
					height = settings->SmartSizingHeight;
			}

			xfc->scaledWidth = width;
			xfc->scaledHeight = height;
		}
#endif

		xfc->window = xf_CreateDesktopWindow(xfc, windowTitle, width, height, xfc->decorations);

		free(windowTitle);

		if (xfc->fullscreen)
			xf_SetWindowFullscreen(xfc, xfc->window, xfc->fullscreen);

		xfc->unobscured = (xevent.xvisibility.state == VisibilityUnobscured);
		XSetWMProtocols(xfc->display, xfc->window->handle, &(xfc->WM_DELETE_WINDOW), 1);
		xfc->drawable = xfc->window->handle;
	}
	else
	{
		xfc->drawable = DefaultRootWindow(xfc->display);
	}

	ZeroMemory(&gcv, sizeof(gcv));

	if (xfc->modifierMap)
		XFreeModifiermap(xfc->modifierMap);

	xfc->modifierMap = XGetModifierMapping(xfc->display);
	assert(!xfc->gc);
	xfc->gc = XCreateGC(xfc->display, xfc->drawable, GCGraphicsExposures, &gcv);
	assert(!xfc->primary);
	xfc->primary = XCreatePixmap(xfc->display, xfc->drawable, xfc->width, xfc->height, xfc->depth);
	xfc->drawing = xfc->primary;
	assert(!xfc->bitmap_mono);
	xfc->bitmap_mono = XCreatePixmap(xfc->display, xfc->drawable, 8, 8, 1);
	assert(!xfc->gc_mono);
	xfc->gc_mono = XCreateGC(xfc->display, xfc->bitmap_mono, GCGraphicsExposures, &gcv);
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, BlackPixelOfScreen(xfc->screen));
	XFillRectangle(xfc->display, xfc->primary, xfc->gc, 0, 0, xfc->width, xfc->height);
	XFlush(xfc->display);
	assert(!xfc->image);

	xfc->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
			(char*) xfc->primary_buffer, xfc->width, xfc->height, xfc->scanline_pad, 0);

	return TRUE;
}

void xf_window_free(xfContext* xfc)
{
	if (xfc->gc_mono)
	{
		XFreeGC(xfc->display, xfc->gc_mono);
		xfc->gc_mono = 0;
	}

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

	if (xfc->xv_context)
	{
		xf_tsmf_uninit(xfc, NULL);
		xfc->xv_context = NULL;
	}

	if (xfc->bitmap_buffer)
	{
		_aligned_free(xfc->bitmap_buffer);
		xfc->bitmap_buffer = NULL;
		xfc->bitmap_size = 0;
	}

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
	rdpContext* context = (rdpContext*) xfc;
	rdpSettings* settings = context->settings;

	xf_lock_x11(xfc, TRUE);

	xf_window_free(xfc);

	xfc->fullscreen = (xfc->fullscreen) ? FALSE : TRUE;
	xfc->decorations = (xfc->fullscreen) ? FALSE : settings->Decorations;

	xf_create_window(xfc);

	xf_unlock_x11(xfc, TRUE);

	EventArgsInit(&e, "xfreerdp");
	e.state = xfc->fullscreen ? FREERDP_WINDOW_STATE_FULLSCREEN : 0;
	PubSub_OnWindowStateChange(context->pubSub, context, &e);
}

void xf_toggle_control(xfContext* xfc)
{
	EncomspClientContext* encomsp;
	ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU pdu;

	encomsp = xfc->encomsp;

	if (!encomsp)
		return;

	pdu.ParticipantId = 0;
	pdu.Flags = ENCOMSP_REQUEST_VIEW;

	if (!xfc->controlToggle)
		pdu.Flags |= ENCOMSP_REQUEST_INTERACT;

	encomsp->ChangeParticipantControlLevel(encomsp, &pdu);

	xfc->controlToggle = !xfc->controlToggle;
}

int xf_encomsp_participant_created(EncomspClientContext* context, ENCOMSP_PARTICIPANT_CREATED_PDU* participantCreated)
{
	return 1;
}

void xf_encomsp_init(xfContext* xfc, EncomspClientContext* encomsp)
{
	xfc->encomsp = encomsp;
	encomsp->custom = (void*) xfc;

	encomsp->ParticipantCreated = xf_encomsp_participant_created;
}

void xf_encomsp_uninit(xfContext* xfc, EncomspClientContext* encomsp)
{
	xfc->encomsp = NULL;
}

void xf_lock_x11(xfContext* xfc, BOOL display)
{
	if (!xfc->UseXThreads)
	{
		WaitForSingleObject(xfc->mutex, INFINITE);
	}
	else
	{
		if (display)
			XLockDisplay(xfc->display);
	}
}

void xf_unlock_x11(xfContext* xfc, BOOL display)
{
	if (!xfc->UseXThreads)
	{
		ReleaseMutex(xfc->mutex);
	}
	else
	{
		if (display)
			XUnlockDisplay(xfc->display);
	}
}

static void xf_calculate_color_shifts(UINT32 mask, UINT8* rsh, UINT8* lsh)
{
	for (*lsh = 0; !(mask & 1); mask >>= 1)
		(*lsh)++;

	for (*rsh = 8; mask; mask >>= 1)
		(*rsh)--;
}

BOOL xf_get_pixmap_info(xfContext* xfc)
{
	int i;
	int vi_count;
	int pf_count;
	XVisualInfo* vi;
	XVisualInfo* vis;
	XVisualInfo template;
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
			xfc->bpp = pf->bits_per_pixel;
			xfc->scanline_pad = pf->scanline_pad;
			break;
		}
	}

	XFree(pfs);

	ZeroMemory(&template, sizeof(template));
	template.class = TrueColor;
	template.screen = xfc->screen_number;

	if (XGetWindowAttributes(xfc->display, RootWindowOfScreen(xfc->screen), &window_attributes) == 0)
	{
		WLog_ERR(TAG, "XGetWindowAttributes failed");
		return FALSE;
	}

	vis = XGetVisualInfo(xfc->display, VisualClassMask | VisualScreenMask, &template, &vi_count);

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
			xfc->invert = TRUE;
		}

		/* calculate color shifts required for rdp order color conversion */
		xf_calculate_color_shifts(vi->red_mask, &xfc->red_shift_r, &xfc->red_shift_l);
		xf_calculate_color_shifts(vi->green_mask, &xfc->green_shift_r, &xfc->green_shift_l);
		xf_calculate_color_shifts(vi->blue_mask, &xfc->blue_shift_r, &xfc->blue_shift_l);
	}

	XFree(vis);

	if ((xfc->visual == NULL) || (xfc->scanline_pad == 0))
	{
		return FALSE;
	}

	return TRUE;
}

int xf_error_handler(Display* d, XErrorEvent* ev)
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

int _xf_error_handler(Display* d, XErrorEvent* ev)
{
	/*
	 * ungrab the keyboard, in case a debugger is running in
	 * another window. This make xf_error_handler() a potential
	 * debugger breakpoint.
	 */
	XUngrabKeyboard(d, CurrentTime);
	return xf_error_handler(d, ev);
}

static void xf_play_sound(rdpContext* context, PLAY_SOUND_UPDATE* play_sound)
{
	xfContext* xfc = (xfContext*) context;
	XkbBell(xfc->display, None, 100, 0);
}

void xf_check_extensions(xfContext* context)
{
	int xkb_opcode, xkb_event, xkb_error;
	int xkb_major = XkbMajorVersion;
	int xkb_minor = XkbMinorVersion;

	if (XkbLibraryVersion(&xkb_major, &xkb_minor) && XkbQueryExtension(context->display, &xkb_opcode, &xkb_event,
			&xkb_error, &xkb_major, &xkb_minor))
	{
		context->xkbAvailable = TRUE;
	}

#ifdef WITH_XRENDER
	{
		int xrender_event_base;
		int xrender_error_base;

		if (XRenderQueryExtension(context->display, &xrender_event_base,
								  &xrender_error_base))
		{
			context->xrenderAvailable = TRUE;
		}
	}
#endif
}

/**
 * Callback given to freerdp_connect() to process the pre-connect operations.
 * It will fill the rdp_freerdp structure (instance) with the appropriate options to use for the connection.
 *
 * @param instance - pointer to the rdp_freerdp structure that contains the connection's parameters, and will
 * be filled with the appropriate informations.
 *
 * @return TRUE if successful. FALSE otherwise.
 * Can exit with error code XF_EXIT_PARSE_ARGUMENTS if there is an error in the parameters.
 */
BOOL xf_pre_connect(freerdp* instance)
{
	rdpChannels* channels;
	rdpSettings* settings;
	rdpContext* context = instance->context;
	xfContext* xfc = (xfContext*) instance->context;

	xfc->codecs = context->codecs;
	xfc->settings = instance->settings;
	xfc->instance = instance;

	settings = instance->settings;
	channels = context->channels;

	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_XSERVER;

	ZeroMemory(settings->OrderSupport, 32);
	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = FALSE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = FALSE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = settings->BitmapCacheEnabled;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = (settings->SoftwareGdi) ? TRUE : FALSE;
	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = settings->BitmapCacheEnabled;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

	PubSub_SubscribeChannelConnected(instance->context->pubSub,
			(pChannelConnectedEventHandler) xf_OnChannelConnectedEventHandler);
	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
			(pChannelDisconnectedEventHandler) xf_OnChannelDisconnectedEventHandler);

	freerdp_client_load_addins(channels, instance->settings);
	freerdp_channels_pre_connect(channels, instance);

	if (!settings->Username)
	{
		char* login_name = getlogin();

		if (login_name)
		{
			settings->Username = _strdup(login_name);
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

	if (!context->cache)
		context->cache = cache_new(settings);

	xf_keyboard_init(xfc);

	xf_detect_monitors(xfc);
	settings->DesktopWidth = xfc->desktopWidth;
	settings->DesktopHeight = xfc->desktopHeight;

	xfc->fullscreen = settings->Fullscreen;
	xfc->decorations = settings->Decorations;
	xfc->grab_keyboard = settings->GrabKeyboard;
	xfc->fullscreen_toggle = settings->ToggleFullscreen;

	return TRUE;
}

/**
 * Callback given to freerdp_connect() to perform post-connection operations.
 * It will be called only if the connection was initialized properly, and will continue the initialization based on the
 * newly created connection.
 */
BOOL xf_post_connect(freerdp* instance)
{
	UINT32 flags;
	rdpCache* cache;
	rdpUpdate* update;
	rdpContext* context;
	rdpChannels* channels;
	rdpSettings* settings;
	ResizeWindowEventArgs e;
	xfContext* xfc = (xfContext*) instance->context;

	context = instance->context;
	cache = context->cache;
	channels = context->channels;
	settings = instance->settings;
	update = context->update;

	xf_register_graphics(context->graphics);

	flags = CLRCONV_ALPHA;

	if (xfc->bpp > 16)
		flags |= CLRBUF_32BPP;
	else
		flags |= CLRBUF_16BPP;

	if (settings->SoftwareGdi)
	{
		rdpGdi* gdi;

		gdi_init(instance, flags, NULL);

		gdi = context->gdi;
		xfc->primary_buffer = gdi->primary_buffer;
	}
	else
	{
		xfc->srcBpp = settings->ColorDepth;
		xf_gdi_register_update_callbacks(update);
	}

	xfc->width = settings->DesktopWidth;
	xfc->height = settings->DesktopHeight;

#ifdef WITH_XRENDER
	xfc->scaledWidth = xfc->width;
	xfc->scaledHeight = xfc->height;
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

	xf_create_window(xfc);

	if (settings->SoftwareGdi)
	{
		update->BeginPaint = xf_sw_begin_paint;
		update->EndPaint = xf_sw_end_paint;
		update->DesktopResize = xf_sw_desktop_resize;
	}
	else
	{
		update->BeginPaint = xf_hw_begin_paint;
		update->EndPaint = xf_hw_end_paint;
		update->DesktopResize = xf_hw_desktop_resize;
	}

	pointer_cache_register_callbacks(update);

	if (!settings->SoftwareGdi)
	{
		glyph_cache_register_callbacks(update);
		brush_cache_register_callbacks(update);
		bitmap_cache_register_callbacks(update);
		offscreen_cache_register_callbacks(update);
		palette_cache_register_callbacks(update);
		update->BitmapUpdate = xf_gdi_bitmap_update;
	}

	update->PlaySound = xf_play_sound;
	update->SetKeyboardIndicators = xf_keyboard_set_indicators;

	xfc->clipboard = xf_clipboard_new(xfc);
	freerdp_channels_post_connect(channels, instance);

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
	xfc = (xfContext*) context;

	gdi_free(instance);

	if (xfc->clipboard)
	{
		xf_clipboard_free(xfc->clipboard);
		xfc->clipboard = NULL;
	}

	xf_window_free(xfc);

	if (context->cache)
	{
		cache_free(context->cache);
		context->cache = NULL;
	}

	xf_keyboard_free(xfc);

	freerdp_channels_disconnect(context->channels, instance);
}

/** Callback set in the rdp_freerdp structure, and used to get the user's password,
 *  if required to establish the connection.
 *  This function is actually called in credssp_ntlmssp_client_init()
 *  @see rdp_server_accept_nego() and rdp_check_fds()
 *  @param instance - pointer to the rdp_freerdp structure that contains the connection settings
 *  @param username - unused
 *  @param password - on return: pointer to a character string that will be filled by the password entered by the user.
 *  				  Note that this character string will be allocated inside the function, and needs to be deallocated by the caller
 *  				  using free(), even in case this function fails.
 *  @param domain - unused
 *  @return TRUE if a password was successfully entered. See freerdp_passphrase_read() for more details.
 */
BOOL xf_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	// FIXME: seems this callback may be called when 'username' is not known.
	// But it doesn't do anything to fix it...
	*password = malloc(password_size * sizeof(char));

	if (freerdp_passphrase_read("Password: ", *password, password_size, instance->settings->CredentialsFromStdin) == NULL)
		return FALSE;

	return TRUE;
}

/** Callback set in the rdp_freerdp structure, and used to make a certificate validation
 *  when the connection requires it.
 *  This function will actually be called by tls_verify_certificate().
 *  @see rdp_client_connect() and tls_connect()
 *  @param instance - pointer to the rdp_freerdp structure that contains the connection settings
 *  @param subject
 *  @param issuer
 *  @param fingerprint
 *  @return TRUE if the certificate is trusted. FALSE otherwise.
 */
BOOL xf_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
	char answer;

	WLog_INFO(TAG, "Certificate details:");
	WLog_INFO(TAG, "\tSubject: %s", subject);
	WLog_INFO(TAG, "\tIssuer: %s", issuer);
	WLog_INFO(TAG, "\tThumbprint: %s", fingerprint);
	WLog_INFO(TAG, "The above X.509 certificate could not be verified, possibly because you do not have "
			  "the CA certificate in your certificate store, or the certificate has expired. "
			  "Please look at the documentation on how to create local certificate store for a private CA.");

	while (1)
	{
		WLog_INFO(TAG, "Do you trust the above certificate? (Y/N) ");
		answer = fgetc(stdin);

		if (feof(stdin))
		{
			WLog_INFO(TAG, "Error: Could not read answer from stdin.");
			if (instance->settings->CredentialsFromStdin)
				WLog_INFO(TAG, " - Run without parameter \"--from-stdin\" to set trust.");
			WLog_INFO(TAG, "");
			return FALSE;
		}

		if (answer == 'y' || answer == 'Y')
		{
			return TRUE;
		}
		else if (answer == 'n' || answer == 'N')
		{
			break;
		}

		WLog_INFO(TAG, "");
	}

	return FALSE;
}

int xf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	xfContext* xfc = (xfContext*) instance->context;
	xf_rail_disable_remoteapp_mode(xfc);
	return 1;
}

void* xf_input_thread(void* arg)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[2];
	XEvent xevent;
	wMessage msg;
	wMessageQueue* queue;
	int pending_status = 1;
	int process_status = 1;
	freerdp* instance = (freerdp*) arg;
	xfContext* xfc = (xfContext*) instance->context;

	queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);

	nCount = 0;
	events[nCount++] = MessageQueue_Event(queue);
	events[nCount++] = xfc->x11event;

	while (1)
	{
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(events[0], 0) == WAIT_OBJECT_0)
		{
			if (MessageQueue_Peek(queue, &msg, FALSE))
			{
				if (msg.id == WMQ_QUIT)
					break;
			}
		}

		if (WaitForSingleObject(events[1], 0) == WAIT_OBJECT_0)
		{
			do
			{
				xf_lock_x11(xfc, FALSE);
				pending_status = XPending(xfc->display);
				xf_unlock_x11(xfc, FALSE);

				if (pending_status)
				{
					xf_lock_x11(xfc, FALSE);

					ZeroMemory(&xevent, sizeof(xevent));
					XNextEvent(xfc->display, &xevent);

					process_status = xf_event_process(instance, &xevent);

					xf_unlock_x11(xfc, FALSE);

					if (!process_status)
						break;
				}
			}
			while (pending_status);

			if (!process_status)
				break;

		}
	}

	MessageQueue_PostQuit(queue, 0);
	ExitThread(0);
	return NULL;
}

BOOL xf_auto_reconnect(freerdp* instance)
{
	UINT32 maxRetries;
	UINT32 numRetries = 0;
	xfContext* xfc = (xfContext*) instance->context;
	rdpSettings* settings = xfc->settings;

	maxRetries = settings->AutoReconnectMaxRetries;

	/* Only auto reconnect on network disconnects. */
	if (freerdp_error_info(instance) != 0)
		return FALSE;

	/* A network disconnect was detected */
	WLog_INFO(TAG, "Network disconnect!");

	if (!settings->AutoReconnectionEnabled)
	{
		/* No auto-reconnect - just quit */
		return FALSE;
	}

	/* Perform an auto-reconnect. */
	while (TRUE)
	{
		/* Quit retrying if max retries has been exceeded */
		if (numRetries++ >= maxRetries)
		{
			return FALSE;
		}

		/* Attempt the next reconnect */
		WLog_INFO(TAG, "Attempting reconnect (%u of %u)", numRetries, maxRetries);

		if (freerdp_reconnect(instance))
		{
			xfc->disconnect = FALSE;
			return TRUE;
		}

		sleep(5);
	}

	WLog_ERR(TAG, "Maximum reconnect retries exceeded");

	return FALSE;
}

/** Main loop for the rdp connection.
 *  It will be run from the thread's entry point (thread_func()).
 *  It initiates the connection, and will continue to run until the session ends,
 *  processing events as they are received.
 *  @param instance - pointer to the rdp_freerdp structure that contains the session's settings
 *  @return A code from the enum XF_EXIT_CODE (0 if successful)
 */
void* xf_client_thread(void* param)
{
	BOOL status;
	int exit_code;
	DWORD nCount;
	DWORD waitStatus;
	HANDLE handles[64];
	xfContext* xfc;
	freerdp* instance;
	rdpContext* context;
	HANDLE inputEvent;
	HANDLE inputThread;
	rdpChannels* channels;
	rdpSettings* settings;

	exit_code = 0;
	instance = (freerdp*) param;
	context = instance->context;

	status = freerdp_connect(instance);

	xfc = (xfContext*) instance->context;

	/* Connection succeeded. --authonly ? */
	if (instance->settings->AuthenticationOnly || !status)
	{
		freerdp_disconnect(instance);
		WLog_ERR(TAG, "Authentication only, exit status %d", !status);
		exit_code = XF_EXIT_CONN_FAILED;
		ExitThread(exit_code);
	}

	channels = context->channels;
	settings = context->settings;

	if (!settings->AsyncInput)
	{
		inputEvent = xfc->x11event;
	}
	else
	{
		inputThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) xf_input_thread, instance, 0, NULL);
		inputEvent = freerdp_get_message_queue_event_handle(instance, FREERDP_INPUT_MESSAGE_QUEUE);
	}

	while (!xfc->disconnect && !freerdp_shall_disconnect(instance))
	{
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

		nCount = 0;
		handles[nCount++] = inputEvent;

		if (!settings->AsyncTransport)
		{
			nCount += freerdp_get_event_handles(context, &handles[nCount]);
		}

		waitStatus = WaitForMultipleObjects(nCount, handles, FALSE, 100);

		if (!settings->AsyncTransport)
		{
			if (!freerdp_check_event_handles(context))
			{
				if (xf_auto_reconnect(instance))
					continue;

				WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
				break;
			}
		}

		if (!settings->AsyncInput)
		{
			if (!xf_process_x_events(instance))
			{
				WLog_INFO(TAG, "Closed from X11");
				break;
			}
		}
		else
		{
			if (WaitForSingleObject(inputEvent, 0) == WAIT_OBJECT_0)
			{
				if (!freerdp_message_queue_process_pending_messages(instance, FREERDP_INPUT_MESSAGE_QUEUE))
				{
					WLog_INFO(TAG, "User Disconnect");
					xfc->disconnect = TRUE;
					break;
				}
			}
		}
	}

	if (settings->AsyncInput)
	{
		wMessageQueue* inputQueue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);
		MessageQueue_PostQuit(inputQueue, 0);
		WaitForSingleObject(inputThread, INFINITE);
		CloseHandle(inputThread);
	}

	if (!exit_code)
		exit_code = freerdp_error_info(instance);

	freerdp_disconnect(instance);

	ExitThread(exit_code);
	return NULL;
}

DWORD xf_exit_code_from_disconnect_reason(DWORD reason)
{
	if (reason == 0 || (reason >= XF_EXIT_PARSE_ARGUMENTS && reason <= XF_EXIT_CONN_FAILED))
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

void xf_TerminateEventHandler(rdpContext* context, TerminateEventArgs* e)
{
	wMessageQueue* queue;

	xfContext* xfc = (xfContext*) context;

	if (context->settings->AsyncInput)
	{
		queue = freerdp_get_message_queue(context->instance, FREERDP_INPUT_MESSAGE_QUEUE);

		if (queue)
			MessageQueue_PostQuit(queue, 0);
	}
	else
	{
		xfc->disconnect = TRUE;
	}
}

#ifdef WITH_XRENDER
static void xf_ZoomingChangeEventHandler(rdpContext* context, ZoomingChangeEventArgs* e)
{
	xfContext* xfc = (xfContext*) context;
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

	xf_draw_screen(xfc, 0, 0, xfc->width, xfc->height);
}

static void xf_PanningChangeEventHandler(rdpContext* context, PanningChangeEventArgs* e)
{
	xfContext* xfc = (xfContext*) context;

	if (e->dx == 0 && e->dy == 0)
		return;

	xfc->offset_x += e->dx;
	xfc->offset_y += e->dy;

	xf_draw_screen(xfc, 0, 0, xfc->width, xfc->height);
}
#endif

/**
 * Client Interface
 */

static void xfreerdp_client_global_init()
{
	setlocale(LC_ALL, "");
	freerdp_handle_signals();
}

static void xfreerdp_client_global_uninit()
{

}

static int xfreerdp_client_start(rdpContext* context)
{
	xfContext* xfc = (xfContext*) context;
	rdpSettings* settings = context->settings;

	if (!settings->ServerHostname)
	{
		WLog_ERR(TAG, "error: server hostname was not specified with /v:<server>[:port]");
		return -1;
	}

	xfc->disconnect = FALSE;

	xfc->thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) xf_client_thread,
			context->instance, 0, NULL);

	return 0;
}

static int xfreerdp_client_stop(rdpContext* context)
{
	xfContext* xfc = (xfContext*) context;

	if (context->settings->AsyncInput)
	{
		wMessageQueue* queue;

		queue = freerdp_get_message_queue(context->instance, FREERDP_INPUT_MESSAGE_QUEUE);

		if (queue)
			MessageQueue_PostQuit(queue, 0);
	}
	else
	{
		xfc->disconnect = TRUE;
	}

	if (xfc->thread)
	{
		WaitForSingleObject(xfc->thread, INFINITE);
		CloseHandle(xfc->thread);
		xfc->thread = NULL;
	}

	return 0;
}

static int xfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	rdpSettings* settings;
	xfContext* xfc = (xfContext*) instance->context;

	instance->PreConnect = xf_pre_connect;
	instance->PostConnect = xf_post_connect;
	instance->PostDisconnect = xf_post_disconnect;
	instance->Authenticate = xf_authenticate;
	instance->VerifyCertificate = xf_verify_certificate;
	instance->LogonErrorInfo = xf_logon_error_info;
	assert(!context->channels);
	context->channels = freerdp_channels_new();

	settings = instance->settings;
	xfc->settings = instance->context->settings;

	PubSub_SubscribeTerminate(context->pubSub, (pTerminateEventHandler) xf_TerminateEventHandler);

#ifdef WITH_XRENDER
	PubSub_SubscribeZoomingChange(context->pubSub, (pZoomingChangeEventHandler) xf_ZoomingChangeEventHandler);
	PubSub_SubscribePanningChange(context->pubSub, (pPanningChangeEventHandler) xf_PanningChangeEventHandler);
#endif

	xfc->UseXThreads = TRUE;

	if (xfc->UseXThreads)
	{
		if (!XInitThreads())
		{
			WLog_WARN(TAG, "XInitThreads() failure");
			xfc->UseXThreads = FALSE;
		}
	}

	assert(!xfc->display);
	xfc->display = XOpenDisplay(NULL);

	if (!xfc->display)
	{
		WLog_ERR(TAG, "failed to open display: %s", XDisplayName(NULL));
		WLog_ERR(TAG, "Please check that the $DISPLAY environment variable is properly set.");
		return -1;
	}

	assert(!xfc->mutex);
	xfc->mutex = CreateMutex(NULL, FALSE, NULL);

	if (!xfc->mutex)
	{
		WLog_ERR(TAG, "Could not create mutex!");
		return -1;
	}

	xfc->_NET_WM_ICON = XInternAtom(xfc->display, "_NET_WM_ICON", False);
	xfc->_MOTIF_WM_HINTS = XInternAtom(xfc->display, "_MOTIF_WM_HINTS", False);
	xfc->_NET_CURRENT_DESKTOP = XInternAtom(xfc->display, "_NET_CURRENT_DESKTOP", False);
	xfc->_NET_WORKAREA = XInternAtom(xfc->display, "_NET_WORKAREA", False);
	xfc->_NET_WM_STATE = XInternAtom(xfc->display, "_NET_WM_STATE", False);
	xfc->_NET_WM_STATE_FULLSCREEN = XInternAtom(xfc->display, "_NET_WM_STATE_FULLSCREEN", False);
	xfc->_NET_WM_WINDOW_TYPE = XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE", False);
	xfc->_NET_WM_WINDOW_TYPE_NORMAL = XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	xfc->_NET_WM_WINDOW_TYPE_DIALOG = XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	xfc->_NET_WM_WINDOW_TYPE_POPUP = XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_POPUP", False);
	xfc->_NET_WM_WINDOW_TYPE_UTILITY = XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	xfc->_NET_WM_WINDOW_TYPE_DROPDOWN_MENU = XInternAtom(xfc->display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
	xfc->_NET_WM_STATE_SKIP_TASKBAR = XInternAtom(xfc->display, "_NET_WM_STATE_SKIP_TASKBAR", False);
	xfc->_NET_WM_STATE_SKIP_PAGER = XInternAtom(xfc->display, "_NET_WM_STATE_SKIP_PAGER", False);
	xfc->_NET_WM_MOVERESIZE = XInternAtom(xfc->display, "_NET_WM_MOVERESIZE", False);
	xfc->_NET_MOVERESIZE_WINDOW = XInternAtom(xfc->display, "_NET_MOVERESIZE_WINDOW", False);
	xfc->WM_PROTOCOLS = XInternAtom(xfc->display, "WM_PROTOCOLS", False);
	xfc->WM_DELETE_WINDOW = XInternAtom(xfc->display, "WM_DELETE_WINDOW", False);
	xfc->WM_STATE = XInternAtom(xfc->display, "WM_STATE", False);

	xfc->xfds = ConnectionNumber(xfc->display);
	xfc->screen_number = DefaultScreen(xfc->display);
	xfc->screen = ScreenOfDisplay(xfc->display, xfc->screen_number);
	xfc->depth = DefaultDepthOfScreen(xfc->screen);
	xfc->big_endian = (ImageByteOrder(xfc->display) == MSBFirst);
	xfc->invert = (ImageByteOrder(xfc->display) == MSBFirst) ? TRUE : FALSE;
	xfc->complex_regions = TRUE;

	assert(!xfc->x11event);
	xfc->x11event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, xfc->xfds);
	xfc->colormap = DefaultColormap(xfc->display, xfc->screen_number);
	xfc->format = PIXEL_FORMAT_XRGB32;

	if (xfc->depth == 32)
		xfc->format = (!xfc->invert) ? PIXEL_FORMAT_XRGB32 : PIXEL_FORMAT_XBGR32;
	else if (xfc->depth == 24)
		xfc->format = (!xfc->invert) ? PIXEL_FORMAT_XRGB32 : PIXEL_FORMAT_XBGR32;
	else if (xfc->depth == 16)
		xfc->format = (!xfc->invert) ? PIXEL_FORMAT_RGB565 : PIXEL_FORMAT_BGR565;
	else if (xfc->depth == 15)
		xfc->format = (!xfc->invert) ? PIXEL_FORMAT_RGB555 : PIXEL_FORMAT_BGR555;
	else
		xfc->format = PIXEL_FORMAT_XRGB32;

	if (xfc->debug)
	{
		WLog_INFO(TAG, "Enabling X11 debug mode.");
		XSynchronize(xfc->display, TRUE);
		_def_error_handler = XSetErrorHandler(_xf_error_handler);
	}

	xf_check_extensions(xfc);

	if (!xf_get_pixmap_info(xfc))
		return -1;

	xfc->vscreen.monitors = calloc(16, sizeof(MONITOR_INFO));

	if (!xfc->vscreen.monitors)
		return -1;

	return 0;
}

static void xfreerdp_client_free(freerdp* instance, rdpContext* context)
{
	xfContext* xfc = (xfContext*) instance->context;

	if (!context)
		return;

	if (context->channels)
	{
		freerdp_channels_close(context->channels, instance);
		freerdp_channels_free(context->channels);
		context->channels = NULL;
	}

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
		WaitForSingleObject(xfc->mutex, INFINITE);
		CloseHandle(xfc->mutex);
		xfc->mutex = NULL;
	}

	if (xfc->vscreen.monitors)
	{
		free(xfc->vscreen.monitors);
		xfc->vscreen.monitors = NULL;
	}
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
