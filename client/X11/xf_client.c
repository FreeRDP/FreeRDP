/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

#ifdef WITH_XRENDER
#include <X11/extensions/Xrender.h>
#endif

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

#include <freerdp/utils/event.h>
#include <freerdp/utils/signal.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/client/channels.h>

#include <freerdp/rail.h>

#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/file.h>
#include <winpr/print.h>

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

static long xv_port = 0;
static const size_t password_size = 512;

void xf_transform_window(xfContext* xfc)
{
	int ret;
	int w;
	int h;
	long supplied;
	Atom hints_atom;
	XSizeHints* size_hints = NULL;

	hints_atom = XInternAtom(xfc->display, "WM_SIZE_HINTS", 1);

	ret = XGetWMSizeHints(xfc->display, xfc->window->handle, size_hints, &supplied, hints_atom);

	if(ret == 0)
		size_hints = XAllocSizeHints();

	w = (xfc->originalWidth * xfc->settings->ScalingFactor) + xfc->offset_x;
	h = (xfc->originalHeight * xfc->settings->ScalingFactor) + xfc->offset_y;

	if(w < 1)
		w = 1;

	if(h < 1)
		h = 1;

	if (size_hints)
	{
		size_hints->flags |= PMinSize | PMaxSize;
		size_hints->min_width = size_hints->max_width = w;
		size_hints->min_height = size_hints->max_height = h;
		XSetWMNormalHints(xfc->display, xfc->window->handle, size_hints);
		XResizeWindow(xfc->display, xfc->window->handle, w, h);

		XFree(size_hints);
	}
}

void xf_draw_screen_scaled(xfContext* xfc, int x, int y, int w, int h, BOOL scale)
{
#ifdef WITH_XRENDER
	XTransform transform;
	Picture windowPicture;
	Picture primaryPicture;
	XRenderPictureAttributes pa;
	XRenderPictFormat* picFormat;
	XRectangle xr;

	picFormat = XRenderFindStandardFormat(xfc->display, PictStandardRGB24);
	pa.subwindow_mode = IncludeInferiors;
	primaryPicture = XRenderCreatePicture(xfc->display, xfc->primary, picFormat, CPSubwindowMode, &pa);
	windowPicture = XRenderCreatePicture(xfc->display, xfc->window->handle, picFormat, CPSubwindowMode, &pa);

	transform.matrix[0][0] = XDoubleToFixed(1);
	transform.matrix[0][1] = XDoubleToFixed(0);
	transform.matrix[0][2] = XDoubleToFixed(0);

	transform.matrix[1][0] = XDoubleToFixed(0);
	transform.matrix[1][1] = XDoubleToFixed(1);
	transform.matrix[1][2] = XDoubleToFixed(0);

	transform.matrix[2][0] = XDoubleToFixed(0);
	transform.matrix[2][1] = XDoubleToFixed(0);
	transform.matrix[2][2] = XDoubleToFixed(xfc->settings->ScalingFactor);

	if( (w != 0) && (h != 0) )
	{

		if(scale == TRUE)
		{
			xr.x = x * xfc->settings->ScalingFactor;
			xr.y = y * xfc->settings->ScalingFactor;
			xr.width = (w+1) * xfc->settings->ScalingFactor;
			xr.height = (h+1) * xfc->settings->ScalingFactor;
		}
		else
		{
			xr.x = x;
			xr.y = y;
			xr.width = w;
			xr.height = h;
		}

		XRenderSetPictureClipRectangles(xfc->display, primaryPicture, 0, 0, &xr, 1);
	}

	XRenderSetPictureTransform(xfc->display, primaryPicture, &transform);

	XRenderComposite(xfc->display, PictOpSrc, primaryPicture, 0, windowPicture, 0, 0, 0, 0, xfc->offset_x, xfc->offset_y, xfc->currentWidth, xfc->currentHeight);

	XRenderFreePicture(xfc->display, primaryPicture);
	XRenderFreePicture(xfc->display, windowPicture);

#endif

}

void xf_sw_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void xf_sw_end_paint(rdpContext* context)
{
	rdpGdi* gdi;
	INT32 x, y;
	UINT32 w, h;
	xfContext* xfc = (xfContext*) context;

	gdi = context->gdi;

	if (!xfc->remote_app)
	{
		if (!xfc->complex_regions)
		{
			if (gdi->primary->hdc->hwnd->invalid->null)
				return;

			x = gdi->primary->hdc->hwnd->invalid->x;
			y = gdi->primary->hdc->hwnd->invalid->y;
			w = gdi->primary->hdc->hwnd->invalid->w;
			h = gdi->primary->hdc->hwnd->invalid->h;

			xf_lock_x11(xfc, FALSE);

			XPutImage(xfc->display, xfc->primary, xfc->gc, xfc->image, x, y, x, y, w, h);

			if ( (xfc->settings->ScalingFactor != 1.0) || (xfc->offset_x) || (xfc->offset_y) )
			{
				xf_draw_screen_scaled(xfc, x, y, w, h, TRUE);
			}
			else
			{
				XCopyArea(xfc->display, xfc->primary, xfc->window->handle, xfc->gc, x, y, w, h, x, y);
			}

			xf_unlock_x11(xfc, FALSE);
		}
		else
		{
			int i;
			int ninvalid;
			HGDI_RGN cinvalid;

			if (gdi->primary->hdc->hwnd->ninvalid < 1)
				return;

			ninvalid = gdi->primary->hdc->hwnd->ninvalid;
			cinvalid = gdi->primary->hdc->hwnd->cinvalid;

			xf_lock_x11(xfc, FALSE);

			for (i = 0; i < ninvalid; i++)
			{
				x = cinvalid[i].x;
				y = cinvalid[i].y;
				w = cinvalid[i].w;
				h = cinvalid[i].h;
				
				//combine xfc->primary with xfc->image
				XPutImage(xfc->display, xfc->primary, xfc->gc, xfc->image, x, y, x, y, w, h);

				if ( (xfc->settings->ScalingFactor != 1.0) || (xfc->offset_x) || (xfc->offset_y) )
				{
					xf_draw_screen_scaled(xfc, x, y, w, h, TRUE);
				}
				else
				{
					XCopyArea(xfc->display, xfc->primary, xfc->window->handle, xfc->gc, x, y, w, h, x, y);
				}
			}

			XFlush(xfc->display);

			xf_unlock_x11(xfc, FALSE);
		}
	}
	else
	{
		if (gdi->primary->hdc->hwnd->invalid->null)
			return;

		x = gdi->primary->hdc->hwnd->invalid->x;
		y = gdi->primary->hdc->hwnd->invalid->y;
		w = gdi->primary->hdc->hwnd->invalid->w;
		h = gdi->primary->hdc->hwnd->invalid->h;

		xf_lock_x11(xfc, FALSE);

		xf_rail_paint(xfc, context->rail, x, y, x + w - 1, y + h - 1);

		xf_unlock_x11(xfc, FALSE);
	}
}

void xf_sw_desktop_resize(rdpContext* context)
{
	rdpSettings* settings;
	xfContext* xfc = (xfContext*) context;

	settings = xfc->instance->settings;

	xf_lock_x11(xfc, TRUE);

	if (!xfc->fullscreen)
	{
		rdpGdi* gdi = context->gdi;
		gdi_resize(gdi, xfc->width, xfc->height);

		if (xfc->image)
		{
			xfc->image->data = NULL;
			XDestroyImage(xfc->image);
			xfc->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
					(char*) gdi->primary_buffer, gdi->width, gdi->height, xfc->scanline_pad, 0);
		}
	}

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

			if ( (xfc->settings->ScalingFactor != 1.0) || (xfc->offset_x) || (xfc->offset_y) )
			{
				xf_draw_screen_scaled(xfc, x, y, w, h, TRUE);
			}
			else
			{
				XCopyArea(xfc->display, xfc->primary, xfc->drawable, xfc->gc, x, y, w, h, x, y);
			}

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
				
				if ( (xfc->settings->ScalingFactor != 1.0) || (xfc->offset_x) || (xfc->offset_y) )
				{
					xf_draw_screen_scaled(xfc, x, y, w, h, TRUE);
				}
				else
				{
					XCopyArea(xfc->display, xfc->primary, xfc->drawable, xfc->gc, x, y, w, h, x, y);
				}
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

		xf_rail_paint(xfc, context->rail, x, y, x + w - 1, y + h - 1);

		xf_unlock_x11(xfc, FALSE);
	}
}

void xf_hw_desktop_resize(rdpContext* context)
{
	BOOL same;
	rdpSettings* settings;
	xfContext* xfc = (xfContext*) context;

	settings = xfc->instance->settings;

	xf_lock_x11(xfc, TRUE);

	if (!xfc->fullscreen)
	{
		xfc->width = settings->DesktopWidth;
		xfc->height = settings->DesktopHeight;

		if (xfc->window)
			xf_ResizeDesktopWindow(xfc, xfc->window, settings->DesktopWidth, settings->DesktopHeight);

		if (xfc->primary)
		{
			same = (xfc->primary == xfc->drawing) ? TRUE : FALSE;

			XFreePixmap(xfc->display, xfc->primary);

			xfc->primary = XCreatePixmap(xfc->display, xfc->drawable,
					xfc->width, xfc->height, xfc->depth);

			if (same)
				xfc->drawing = xfc->primary;
		}
	}
	else
	{
		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);
		XSetForeground(xfc->display, xfc->gc, 0);
		XFillRectangle(xfc->display, xfc->drawable, xfc->gc, 0, 0, xfc->width, xfc->height);
	}

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

void xf_create_window(xfContext* xfc)
{
	XEvent xevent;
	int width, height;
	char* windowTitle;

	ZeroMemory(&xevent, sizeof(xevent));

	width = xfc->width;
	height = xfc->height;

	if (!xfc->remote_app)
	{
		xfc->attribs.background_pixel = BlackPixelOfScreen(xfc->screen);
		xfc->attribs.border_pixel = WhitePixelOfScreen(xfc->screen);
		xfc->attribs.backing_store = xfc->primary ? NotUseful : Always;
		xfc->attribs.override_redirect = xfc->grab_keyboard ? xfc->fullscreen : False;
		xfc->attribs.colormap = xfc->colormap;
		xfc->attribs.bit_gravity = NorthWestGravity;
		xfc->attribs.win_gravity = NorthWestGravity;

		if (xfc->instance->settings->WindowTitle)
		{
			windowTitle = _strdup(xfc->instance->settings->WindowTitle);
		}
		else if (xfc->instance->settings->ServerPort == 3389)
		{
			windowTitle = malloc(1 + sizeof("FreeRDP: ") + strlen(xfc->instance->settings->ServerHostname));
			sprintf(windowTitle, "FreeRDP: %s", xfc->instance->settings->ServerHostname);
		}
		else
		{
			windowTitle = malloc(1 + sizeof("FreeRDP: ") + strlen(xfc->instance->settings->ServerHostname) + sizeof(":00000"));
			sprintf(windowTitle, "FreeRDP: %s:%i", xfc->instance->settings->ServerHostname, xfc->instance->settings->ServerPort);
		}

		xfc->window = xf_CreateDesktopWindow(xfc, windowTitle, width, height, xfc->settings->Decorations);
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
}

void xf_toggle_fullscreen(xfContext* xfc)
{
	Pixmap contents = 0;
	WindowStateChangeEventArgs e;

	xf_lock_x11(xfc, TRUE);

	contents = XCreatePixmap(xfc->display, xfc->window->handle, xfc->width, xfc->height, xfc->depth);
	XCopyArea(xfc->display, xfc->primary, contents, xfc->gc, 0, 0, xfc->width, xfc->height, 0, 0);

	XDestroyWindow(xfc->display, xfc->window->handle);
	xfc->fullscreen = (xfc->fullscreen) ? FALSE : TRUE;
	xf_create_window(xfc);

	XCopyArea(xfc->display, contents, xfc->primary, xfc->gc, 0, 0, xfc->width, xfc->height, 0, 0);
	XFreePixmap(xfc->display, contents);

	xf_unlock_x11(xfc, TRUE);

	EventArgsInit(&e, "xfreerdp");
	e.state = xfc->fullscreen ? FREERDP_WINDOW_STATE_FULLSCREEN : 0;
	PubSub_OnWindowStateChange(((rdpContext*) xfc)->pubSub, xfc, &e);
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

	pfs = XListPixmapFormats(xfc->display, &pf_count);

	if (pfs == NULL)
	{
		fprintf(stderr, "xf_get_pixmap_info: XListPixmapFormats failed\n");
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
		fprintf(stderr, "xf_get_pixmap_info: XGetWindowAttributes failed\n");
		return FALSE;
	}

	vis = XGetVisualInfo(xfc->display, VisualClassMask | VisualScreenMask, &template, &vi_count);

	if (vis == NULL)
	{
		fprintf(stderr, "xf_get_pixmap_info: XGetVisualInfo failed\n");
		return FALSE;
	}

	vi = NULL;
	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->visual == window_attributes.visual)
		{
			xfc->visual = vi->visual;
			break;
		}
	}

	if (vi)
	{
		/*
		 * Detect if the server visual has an inverted colormap
		 * (BGR vs RGB, or red being the least significant byte)
		 */

		if (vi->red_mask & 0xFF)
		{
			xfc->clrconv->invert = TRUE;
		}
	}

	XFree(vis);

	if ((xfc->visual == NULL) || (xfc->scanline_pad == 0))
	{
		return FALSE;
	}

	return TRUE;
}

static int (*_def_error_handler)(Display*, XErrorEvent*);

int xf_error_handler(Display* d, XErrorEvent* ev)
{
	char buf[256];
	int do_abort = TRUE;

	XGetErrorText(d, ev->error_code, buf, sizeof(buf));
	fprintf(stderr, "%s", buf);

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

static void xf_post_disconnect(freerdp *instance)
{
	xfContext* xfc = (xfContext*) instance->context;

	assert(NULL != instance);
	assert(NULL != xfc);
	assert(NULL != instance->settings);

	if (xfc->mutex)
	{
		WaitForSingleObject(xfc->mutex, INFINITE);
		CloseHandle(xfc->mutex);
		xfc->mutex = NULL;
	}

	xf_monitors_free(xfc, instance->settings);
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
	xfContext* xfc = (xfContext*) instance->context;

	xfc->settings = instance->settings;
	xfc->instance = instance;

	settings = instance->settings;
	channels = instance->context->channels;

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

	xfc->UseXThreads = TRUE;

	if (xfc->UseXThreads)
	{
		if (!XInitThreads())
		{
			fprintf(stderr, "warning: XInitThreads() failure\n");
			xfc->UseXThreads = FALSE;
		}
	}

	xfc->display = XOpenDisplay(NULL);

	if (!xfc->display)
	{
		fprintf(stderr, "xf_pre_connect: failed to open display: %s\n", XDisplayName(NULL));
		fprintf(stderr, "Please check that the $DISPLAY environment variable is properly set.\n");
		return FALSE;
	}

	if (xfc->debug)
	{
		fprintf(stderr, "Enabling X11 debug mode.\n");
		XSynchronize(xfc->display, TRUE);
		_def_error_handler = XSetErrorHandler(_xf_error_handler);
	}

	xfc->mutex = CreateMutex(NULL, FALSE, NULL);

	PubSub_SubscribeChannelConnected(instance->context->pubSub,
			(pChannelConnectedEventHandler) xf_OnChannelConnectedEventHandler);

	PubSub_SubscribeChannelDisconnected(instance->context->pubSub,
			(pChannelDisconnectedEventHandler) xf_OnChannelDisconnectedEventHandler);

	freerdp_client_load_addins(channels, instance->settings);

	freerdp_channels_pre_connect(channels, instance);

	if (settings->AuthenticationOnly)
	{
		/* Check --authonly has a username and password. */
		if (settings->Username == NULL)
		{
			fprintf(stderr, "--authonly, but no -u username. Please provide one.\n");
			return FALSE;
		}
		if (settings->Password == NULL)
		{
			fprintf(stderr, "--authonly, but no -p password. Please provide one.\n");
			return FALSE;
		}
		fprintf(stderr, "Authentication only. Don't connect to X.\n");
		/* Avoid XWindows initialization and configuration below. */
		return TRUE;
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

	xf_keyboard_init(xfc);

	xfc->clrconv = freerdp_clrconv_new(CLRCONV_ALPHA);

	instance->context->cache = cache_new(instance->settings);

	xfc->xfds = ConnectionNumber(xfc->display);
	xfc->screen_number = DefaultScreen(xfc->display);
	xfc->screen = ScreenOfDisplay(xfc->display, xfc->screen_number);
	xfc->depth = DefaultDepthOfScreen(xfc->screen);
	xfc->big_endian = (ImageByteOrder(xfc->display) == MSBFirst);

	xfc->complex_regions = TRUE;
	xfc->fullscreen = settings->Fullscreen;
	xfc->grab_keyboard = settings->GrabKeyboard;
	xfc->fullscreen_toggle = settings->ToggleFullscreen;

	xf_detect_monitors(xfc, settings);

	return TRUE;
}

/**
 * Callback given to freerdp_connect() to perform post-connection operations.
 * It will be called only if the connection was initialized properly, and will continue the initialization based on the
 * newly created connection.
 */
BOOL xf_post_connect(freerdp* instance)
{
	XGCValues gcv;
	rdpCache* cache;
	rdpChannels* channels;
	rdpSettings* settings;
	ResizeWindowEventArgs e;
	RFX_CONTEXT* rfx_context = NULL;
	NSC_CONTEXT* nsc_context = NULL;
	xfContext* xfc = (xfContext*) instance->context;

	cache = instance->context->cache;
	channels = instance->context->channels;
	settings = instance->settings;

	if (!xf_get_pixmap_info(xfc))
		return FALSE;

	xf_register_graphics(instance->context->graphics);

	if (xfc->settings->SoftwareGdi)
	{
		rdpGdi* gdi;
		UINT32 flags;

		flags = CLRCONV_ALPHA;

		if (xfc->bpp > 16)
			flags |= CLRBUF_32BPP;
		else
			flags |= CLRBUF_16BPP;

		gdi_init(instance, flags, NULL);
		gdi = instance->context->gdi;
		xfc->primary_buffer = gdi->primary_buffer;

		rfx_context = gdi->rfx_context;
	}
	else
	{
		xfc->srcBpp = instance->settings->ColorDepth;
		xf_gdi_register_update_callbacks(instance->update);

		xfc->hdc = gdi_CreateDC(xfc->clrconv, xfc->bpp);

		if (instance->settings->RemoteFxCodec)
		{
			rfx_context = (void*) rfx_context_new(FALSE);
			xfc->rfx_context = rfx_context;
		}

		if (instance->settings->NSCodec)
		{
			nsc_context = (void*) nsc_context_new();
			xfc->nsc_context = nsc_context;
		}
	}

	xfc->originalWidth = settings->DesktopWidth;
	xfc->originalHeight = settings->DesktopHeight;
	xfc->currentWidth = xfc->originalWidth;
	xfc->currentHeight = xfc->originalWidth;
	xfc->settings->ScalingFactor = 1.0;

	xfc->offset_x = 0;
	xfc->offset_y = 0;

	xfc->width = settings->DesktopWidth;
	xfc->height = settings->DesktopHeight;

	if (settings->RemoteApplicationMode)
		xfc->remote_app = TRUE;

	xf_create_window(xfc);

	ZeroMemory(&gcv, sizeof(gcv));

	if (xfc->modifierMap)
		XFreeModifiermap(xfc->modifierMap);

	xfc->modifierMap = XGetModifierMapping(xfc->display);

	xfc->gc = XCreateGC(xfc->display, xfc->drawable, GCGraphicsExposures, &gcv);
	xfc->primary = XCreatePixmap(xfc->display, xfc->drawable, xfc->width, xfc->height, xfc->depth);
	xfc->drawing = xfc->primary;

	xfc->bitmap_mono = XCreatePixmap(xfc->display, xfc->drawable, 8, 8, 1);
	xfc->gc_mono = XCreateGC(xfc->display, xfc->bitmap_mono, GCGraphicsExposures, &gcv);

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, BlackPixelOfScreen(xfc->screen));
	XFillRectangle(xfc->display, xfc->primary, xfc->gc, 0, 0, xfc->width, xfc->height);
	XFlush(xfc->display);

	xfc->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
			(char*) xfc->primary_buffer, xfc->width, xfc->height, xfc->scanline_pad, 0);

	xfc->bmp_codec_none = (BYTE*) malloc(64 * 64 * 4);

	if (xfc->settings->SoftwareGdi)
	{
		instance->update->BeginPaint = xf_sw_begin_paint;
		instance->update->EndPaint = xf_sw_end_paint;
		instance->update->DesktopResize = xf_sw_desktop_resize;
	}
	else
	{
		instance->update->BeginPaint = xf_hw_begin_paint;
		instance->update->EndPaint = xf_hw_end_paint;
		instance->update->DesktopResize = xf_hw_desktop_resize;
	}

	pointer_cache_register_callbacks(instance->update);

	if (!xfc->settings->SoftwareGdi)
	{
		glyph_cache_register_callbacks(instance->update);
		brush_cache_register_callbacks(instance->update);
		bitmap_cache_register_callbacks(instance->update);
		offscreen_cache_register_callbacks(instance->update);
		palette_cache_register_callbacks(instance->update);
	}

	instance->context->rail = rail_new(instance->settings);
	rail_register_update_callbacks(instance->context->rail, instance->update);
	xf_rail_register_callbacks(xfc, instance->context->rail);

	freerdp_channels_post_connect(channels, instance);

	xf_tsmf_init(xfc, xv_port);

	xf_cliprdr_init(xfc, channels);

	EventArgsInit(&e, "xfreerdp");
	e.width = settings->DesktopWidth;
	e.height = settings->DesktopHeight;
	PubSub_OnResizeWindow(((rdpContext*) xfc)->pubSub, xfc, &e);

	return TRUE;
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

	printf("Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fingerprint);
	printf("The above X.509 certificate could not be verified, possibly because you do not have "
		"the CA certificate in your certificate store, or the certificate has expired. "
		"Please look at the documentation on how to create local certificate store for a private CA.\n");

	while (1)
	{
		printf("Do you trust the above certificate? (Y/N) ");
		answer = fgetc(stdin);

		if (feof(stdin))
		{
			printf("\nError: Could not read answer from stdin.");
			if (instance->settings->CredentialsFromStdin)
				printf(" - Run without parameter \"--from-stdin\" to set trust.");
			printf("\n");
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
		printf("\n");
	}

	return FALSE;
}

int xf_logon_error_info(freerdp* instance, UINT32 data, UINT32 type)
{
	xfContext* xfc = (xfContext*) instance->context;

	xf_rail_disable_remoteapp_mode(xfc);

	return 1;
}

void xf_process_channel_event(rdpChannels* channels, freerdp* instance)
{
	xfContext* xfc;
	wMessage* event;

	xfc = (xfContext*) instance->context;

	event = freerdp_channels_pop_event(channels);

	if (event)
	{
		switch (GetMessageClass(event->id))
		{
			case RailChannel_Class:
				xf_process_rail_event(xfc, channels, event);
				break;

			case TsmfChannel_Class:
				xf_process_tsmf_event(xfc, event);
				break;

			case CliprdrChannel_Class:
				xf_process_cliprdr_event(xfc, event);
				break;

			case RdpeiChannel_Class:
				xf_process_rdpei_event(xfc, event);
				break;

			default:
				break;
		}

		freerdp_event_free(event);
	}
}

void xf_window_free(xfContext* xfc)
{
	rdpContext* context = (rdpContext*) xfc;

	xf_keyboard_free(xfc);

	if (xfc->gc)
	{
		XFreeGC(xfc->display, xfc->gc);
		xfc->gc = 0;
	}

	if (xfc->gc_mono)
	{
		XFreeGC(xfc->display, xfc->gc_mono);
		xfc->gc_mono = 0;
	}

	if (xfc->window)
	{
		xf_DestroyWindow(xfc, xfc->window);
		xfc->window = NULL;
	}

	if (xfc->primary)
	{
		XFreePixmap(xfc->display, xfc->primary);
		xfc->primary = 0;
	}

	if (xfc->bitmap_mono)
	{
		XFreePixmap(xfc->display, xfc->bitmap_mono);
		xfc->bitmap_mono = 0;
	}

	if (xfc->image)
	{
		xfc->image->data = NULL;
		XDestroyImage(xfc->image);
		xfc->image = NULL;
	}

	if (context->cache)
	{
		cache_free(context->cache);
		context->cache = NULL;
	}

	if (context->rail)
	{
		rail_free(context->rail);
		context->rail = NULL;
	}

	if (xfc->rfx_context)
	{
		rfx_context_free(xfc->rfx_context);
		xfc->rfx_context = NULL;
	}

	if (xfc->nsc_context)
	{
		nsc_context_free(xfc->nsc_context);
		xfc->nsc_context = NULL;
	}

	if (xfc->clrconv)
	{
		freerdp_clrconv_free(xfc->clrconv);
		xfc->clrconv = NULL;
	}

	if (xfc->hdc)
		gdi_DeleteDC(xfc->hdc);

	if (xfc->xv_context)
	{
		xf_tsmf_uninit(xfc);
		xfc->xv_context = NULL;
	}

	if (xfc->clipboard_context)
	{
		xf_cliprdr_uninit(xfc);
		xfc->clipboard_context = NULL;
	}
}

void* xf_update_thread(void* arg)
{
	int status;
	wMessage message;
	wMessageQueue* queue;
	freerdp* instance = (freerdp*) arg;

	assert( NULL != instance);

	status = 1;
	queue = freerdp_get_message_queue(instance, FREERDP_UPDATE_MESSAGE_QUEUE);

	while (MessageQueue_Wait(queue))
	{
		while (MessageQueue_Peek(queue, &message, TRUE))
		{
			status = freerdp_message_queue_process_message(instance, FREERDP_UPDATE_MESSAGE_QUEUE, &message);

			if (!status)
				break;
		}

		if (!status)
			break;
	}

	ExitThread(0);
	return NULL;
}

void* xf_input_thread(void* arg)
{
	xfContext* xfc;
	HANDLE event;
	XEvent xevent;
	wMessageQueue* queue;
	int pending_status = 1;
	int process_status = 1;
	freerdp* instance = (freerdp*) arg;
	assert(NULL != instance);

	xfc = (xfContext*) instance->context;
	assert(NULL != xfc);

	queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);
	event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, xfc->xfds);

	while (WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0)
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

	MessageQueue_PostQuit(queue, 0);
	ExitThread(0);
	return NULL;
}

void* xf_channels_thread(void* arg)
{
	int status;
	xfContext* xfc;
	HANDLE event;
	rdpChannels* channels;
	freerdp* instance = (freerdp*) arg;
	assert(NULL != instance);

	xfc = (xfContext*) instance->context;
	assert(NULL != xfc);

	channels = instance->context->channels;
	event = freerdp_channels_get_event_handle(instance);

	while (WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0)
	{
		status = freerdp_channels_process_pending_messages(instance);
		if (!status)
			break;

		xf_process_channel_event(channels, instance);
	}

	ExitThread(0);
	return NULL;
}

BOOL xf_auto_reconnect(freerdp* instance)
{
	xfContext* xfc = (xfContext*) instance->context;

	UINT32 num_retries = 0;
	UINT32 max_retries = instance->settings->AutoReconnectMaxRetries;

	/* Only auto reconnect on network disconnects. */
	if (freerdp_error_info(instance) != 0)
		return FALSE;

	/* A network disconnect was detected */
	fprintf(stderr, "Network disconnect!\n");

	if (!instance->settings->AutoReconnectionEnabled)
	{
		/* No auto-reconnect - just quit */
		return FALSE;
	}

	/* Perform an auto-reconnect. */
	for (;;)
	{
		/* Quit retrying if max retries has been exceeded */
		if (num_retries++ >= max_retries)
		{
			return FALSE;
		}

		/* Attempt the next reconnect */
		fprintf(stderr, "Attempting reconnect (%u of %u)\n", num_retries, max_retries);

		if (freerdp_reconnect(instance))
		{
			xfc->disconnect = FALSE;
			return TRUE;
		}

		sleep(5);
	}

	fprintf(stderr, "Maximum reconnect retries exceeded\n");

	return FALSE;
}

/** Main loop for the rdp connection.
 *  It will be run from the thread's entry point (thread_func()).
 *  It initiates the connection, and will continue to run until the session ends,
 *  processing events as they are received.
 *  @param instance - pointer to the rdp_freerdp structure that contains the session's settings
 *  @return A code from the enum XF_EXIT_CODE (0 if successful)
 */
void* xf_thread(void* param)
{
	int i;
	int fds;
	xfContext* xfc;
	int max_fds;
	int rcount;
	int wcount;
	BOOL status;
	int exit_code;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	freerdp* instance;
	int fd_input_event;
	HANDLE input_event;
	int select_status;
	BOOL async_update;
	BOOL async_input;
	BOOL async_channels;
	BOOL async_transport;
	HANDLE update_thread;
	HANDLE input_thread;
	HANDLE channels_thread;
	rdpChannels* channels;
	rdpSettings* settings;
	struct timeval timeout;

	exit_code = 0;
	input_event = NULL;

	instance = (freerdp*) param;
	assert(NULL != instance);

	ZeroMemory(rfds, sizeof(rfds));
	ZeroMemory(wfds, sizeof(wfds));
	ZeroMemory(&timeout, sizeof(struct timeval));

	status = freerdp_connect(instance);

	xfc = (xfContext*) instance->context;
	assert(NULL != xfc);

	/* Connection succeeded. --authonly ? */
	if (instance->settings->AuthenticationOnly)
	{
		freerdp_disconnect(instance);
		fprintf(stderr, "Authentication only, exit status %d\n", !status);
		ExitThread(exit_code);
	}

	if (!status)
	{
		if (xfc->mutex)
		{
			WaitForSingleObject(xfc->mutex, INFINITE);
			CloseHandle(xfc->mutex);
			xfc->mutex = NULL;
		}

		xf_monitors_free(xfc, instance->settings);

		exit_code = XF_EXIT_CONN_FAILED;
		ExitThread(exit_code);
	}

	channels = instance->context->channels;
	settings = instance->context->settings;

	async_update = settings->AsyncUpdate;
	async_input = settings->AsyncInput;
	async_channels = settings->AsyncChannels;
	async_transport = settings->AsyncTransport;

	if (async_update)
	{
		update_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) xf_update_thread, instance, 0, NULL);
	}

	if (async_input)
	{
		input_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) xf_input_thread, instance, 0, NULL);
	}

	if (async_channels)
	{
		channels_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) xf_channels_thread, instance, 0, NULL);
	}

	while (!xfc->disconnect && !freerdp_shall_disconnect(instance))
	{
		rcount = 0;
		wcount = 0;

		/*
		 * win8 and server 2k12 seem to have some timing issue/race condition
		 * when a initial sync request is send to sync the keyboard inidcators
		 * sending the sync event twice fixed this problem
		 */
		if (freerdp_focus_required(instance))
		{
			xf_keyboard_focus_in(xfc);
			xf_keyboard_focus_in(xfc);
		}

		if (!async_transport)
		{
			if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
			{
				fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
				exit_code = XF_EXIT_CONN_FAILED;
				break;
			}
		}

		if (!async_channels)
		{
			if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
			{
				fprintf(stderr, "Failed to get channel manager file descriptor\n");
				exit_code = XF_EXIT_CONN_FAILED;
				break;
			}
		}

		if (!async_input)
		{
			if (xf_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
			{
				fprintf(stderr, "Failed to get xfreerdp file descriptor\n");
				exit_code = XF_EXIT_CONN_FAILED;
				break;
			}
		}
		else
		{
			input_event = freerdp_get_message_queue_event_handle(instance, FREERDP_INPUT_MESSAGE_QUEUE);
			fd_input_event = GetEventFileDescriptor(input_event);
			rfds[rcount++] = (void*) (long) fd_input_event;
		}

		max_fds = 0;
		FD_ZERO(&rfds_set);
		FD_ZERO(&wfds_set);

		for (i = 0; i < rcount; i++)
		{
			fds = (int)(long)(rfds[i]);

			if (fds > max_fds)
				max_fds = fds;

			FD_SET(fds, &rfds_set);
		}

		if (max_fds == 0)
			break;

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		select_status = select(max_fds + 1, &rfds_set, NULL, NULL, &timeout);

		if (select_status == 0)
		{
			continue; /* select timeout */
		}
		else if (select_status == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) || (errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) || (errno == EINTR))) /* signal occurred */
			{
				fprintf(stderr, "xfreerdp_run: select failed\n");
				break;
			}
		}

		if (!async_transport)
		{
			if (freerdp_check_fds(instance) != TRUE)
			{
				if (xf_auto_reconnect(instance))
					continue;

				fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
				break;
			}
		}

		if (!async_channels)
		{
			if (freerdp_channels_check_fds(channels, instance) != TRUE)
			{
				fprintf(stderr, "Failed to check channel manager file descriptor\n");
				break;
			}

			xf_process_channel_event(channels, instance);
		}

		if (!async_input)
		{
			if (xf_process_x_events(instance) != TRUE)
			{
				fprintf(stderr, "Closed from X11\n");
				break;
			}
		}
		else
		{
			if (WaitForSingleObject(input_event, 0) == WAIT_OBJECT_0)
			{
				if (!freerdp_message_queue_process_pending_messages(instance, FREERDP_INPUT_MESSAGE_QUEUE))
				{
					fprintf(stderr, "User Disconnect\n");
					xfc->disconnect = TRUE;
					break;
				}
			}
		}
	}

	/* Close the channels first. This will signal the internal message pipes
	 * that the threads should quit. */
	freerdp_channels_close(channels, instance);

	if (async_update)
	{
		wMessageQueue* update_queue = freerdp_get_message_queue(instance, FREERDP_UPDATE_MESSAGE_QUEUE);
		MessageQueue_PostQuit(update_queue, 0);
		WaitForSingleObject(update_thread, INFINITE);
		CloseHandle(update_thread);
	}

	if (async_input)
	{
		wMessageQueue* input_queue = freerdp_get_message_queue(instance, FREERDP_INPUT_MESSAGE_QUEUE);
		MessageQueue_PostQuit(input_queue, 0);
		WaitForSingleObject(input_thread, INFINITE);
		CloseHandle(input_thread);
	}

	if (async_channels)
	{
		WaitForSingleObject(channels_thread, INFINITE);
		CloseHandle(channels_thread);
	}

	FILE* fin = fopen("/tmp/tsmf.tid", "rt");

	if (fin)
	{
		FILE* fin1;
		int thid = 0;
		int timeout;

		fscanf(fin, "%d", &thid);
		fclose(fin);

		pthread_kill((pthread_t) (size_t) thid, SIGUSR1);

		fin1 = fopen("/tmp/tsmf.tid", "rt");
		timeout = 5;

		while (fin1)
		{
			fclose(fin1);
			sleep(1);
			timeout--;

			if (timeout <= 0)
			{
				unlink("/tmp/tsmf.tid");
				pthread_kill((pthread_t) (size_t) thid, SIGKILL);
				break;
			}

			fin1 = fopen("/tmp/tsmf.tid", "rt");
		}
	}

	if (!exit_code)
		exit_code = freerdp_error_info(instance);

	freerdp_channels_free(channels);
	freerdp_disconnect(instance);
	gdi_free(instance);

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
	else if (!(reason <= 0xB))
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

static void xf_ScalingFactorChangeEventHandler(rdpContext* context, ScalingFactorChangeEventArgs* e)
{
	xfContext* xfc = (xfContext*) context;

	xfc->settings->ScalingFactor += e->ScalingFactor;

	if (xfc->settings->ScalingFactor > 1.2)
		xfc->settings->ScalingFactor = 1.2;
	if (xfc->settings->ScalingFactor < 0.8)
		xfc->settings->ScalingFactor = 0.8;


	xfc->currentWidth = xfc->originalWidth * xfc->settings->ScalingFactor;
	xfc->currentHeight = xfc->originalHeight * xfc->settings->ScalingFactor;

	xf_transform_window(xfc);

	{
		ResizeWindowEventArgs ev;

		EventArgsInit(&ev, "xfreerdp");
		ev.width = (int) xfc->originalWidth * xfc->settings->ScalingFactor;
		ev.height = (int) xfc->originalHeight * xfc->settings->ScalingFactor;
		PubSub_OnResizeWindow(((rdpContext*) xfc)->pubSub, xfc, &ev);
	}
	xf_draw_screen_scaled(xfc, 0, 0, 0, 0, FALSE);

}

/**
 * Client Interface
 */

static void xfreerdp_client_global_init()
{
	setlocale(LC_ALL, "");
	freerdp_handle_signals();
	freerdp_channels_global_init();
}

static void xfreerdp_client_global_uninit()
{
	freerdp_channels_global_uninit();
}

static int xfreerdp_client_start(rdpContext* context)
{
	xfContext* xfc = (xfContext*) context;

	rdpSettings* settings = context->settings;

	if (!settings->ServerHostname)
	{
		fprintf(stderr, "error: server hostname was not specified with /v:<server>[:port]\n");
		return -1;
	}

	xfc->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) xf_thread,
			context->instance, 0, NULL);

	return 0;
}

static int xfreerdp_client_stop(rdpContext* context)
{
	xfContext* xfc = (xfContext*) context;

	assert(NULL != context);

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
		CloseHandle(xfc->thread);
		xfc->thread = NULL;
	}

	return 0;
}

static int xfreerdp_client_new(freerdp* instance, rdpContext* context)
{
	xfContext* xfc;
	rdpSettings* settings;

	xfc = (xfContext*) instance->context;

	instance->PreConnect = xf_pre_connect;
	instance->PostConnect = xf_post_connect;
	instance->PostDisconnect = xf_post_disconnect;
	instance->Authenticate = xf_authenticate;
	instance->VerifyCertificate = xf_verify_certificate;
	instance->LogonErrorInfo = xf_logon_error_info;

	context->channels = freerdp_channels_new();

	settings = instance->settings;
	xfc->settings = instance->context->settings;

	PubSub_SubscribeTerminate(context->pubSub, (pTerminateEventHandler) xf_TerminateEventHandler);
	PubSub_SubscribeScalingFactorChange(context->pubSub, (pScalingFactorChangeEventHandler) xf_ScalingFactorChangeEventHandler);

	return 0;
}

static void xfreerdp_client_free(freerdp* instance, rdpContext* context)
{
	xfContext* xfc = (xfContext*) context;

	if (context)
	{
		xf_window_free(xfc);

		if (xfc->bmp_codec_none)
			free(xfc->bmp_codec_none);

		if (xfc->display)
			XCloseDisplay(xfc->display);
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
