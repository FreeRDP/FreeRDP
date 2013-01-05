/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012 HP Development Company, LLC
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

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
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

#include "xf_gdi.h"
#include "xf_rail.h"
#include "xf_tsmf.h"
#include "xf_event.h"
#include "xf_cliprdr.h"
#include "xf_monitor.h"
#include "xf_graphics.h"
#include "xf_keyboard.h"

#include "xfreerdp.h"

static HANDLE g_sem;
static int g_thread_count = 0;
static BYTE g_disconnect_reason = 0;

static long xv_port = 0;
static const size_t password_size = 512;

struct thread_data
{
	freerdp* instance;
};

void xf_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
}

void xf_context_free(freerdp* instance, rdpContext* context)
{

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
	xfInfo* xfi;
	INT32 x, y;
	UINT32 w, h;

	xfi = ((xfContext*) context)->xfi;
	gdi = context->gdi;

	if (xfi->remote_app != TRUE)
	{
		if (xfi->complex_regions != TRUE)
		{
			if (gdi->primary->hdc->hwnd->invalid->null)
				return;

			x = gdi->primary->hdc->hwnd->invalid->x;
			y = gdi->primary->hdc->hwnd->invalid->y;
			w = gdi->primary->hdc->hwnd->invalid->w;
			h = gdi->primary->hdc->hwnd->invalid->h;

			XPutImage(xfi->display, xfi->primary, xfi->gc, xfi->image, x, y, x, y, w, h);
			XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc, x, y, w, h, x, y);
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

			for (i = 0; i < ninvalid; i++)
			{
				x = cinvalid[i].x;
				y = cinvalid[i].y;
				w = cinvalid[i].w;
				h = cinvalid[i].h;

				XPutImage(xfi->display, xfi->primary, xfi->gc, xfi->image, x, y, x, y, w, h);
				XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc, x, y, w, h, x, y);
			}

			XFlush(xfi->display);
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

		xf_rail_paint(xfi, context->rail, x, y, x + w - 1, y + h - 1);
	}
}

void xf_sw_desktop_resize(rdpContext* context)
{
	xfInfo* xfi;
	rdpSettings* settings;

	xfi = ((xfContext*) context)->xfi;
	settings = xfi->instance->settings;

	if (xfi->fullscreen != TRUE)
	{
		rdpGdi* gdi = context->gdi;
		gdi_resize(gdi, xfi->width, xfi->height);

		if (xfi->image)
		{
			xfi->image->data = NULL;
			XDestroyImage(xfi->image);
			xfi->image = XCreateImage(xfi->display, xfi->visual, xfi->depth, ZPixmap, 0,
					(char*) gdi->primary_buffer, gdi->width, gdi->height, xfi->scanline_pad, 0);
		}
	}
}

void xf_hw_begin_paint(rdpContext* context)
{
	xfInfo* xfi;
	xfi = ((xfContext*) context)->xfi;
	xfi->hdc->hwnd->invalid->null = 1;
	xfi->hdc->hwnd->ninvalid = 0;
}

void xf_hw_end_paint(rdpContext* context)
{
	xfInfo* xfi;
	INT32 x, y;
	UINT32 w, h;

	xfi = ((xfContext*) context)->xfi;

	if (xfi->remote_app)
	{
		if (xfi->hdc->hwnd->invalid->null)
			return;

		x = xfi->hdc->hwnd->invalid->x;
		y = xfi->hdc->hwnd->invalid->y;
		w = xfi->hdc->hwnd->invalid->w;
		h = xfi->hdc->hwnd->invalid->h;

		xf_rail_paint(xfi, context->rail, x, y, x + w - 1, y + h - 1);
	}
}

void xf_hw_desktop_resize(rdpContext* context)
{
	xfInfo* xfi;
	BOOL same;
	rdpSettings* settings;

	xfi = ((xfContext*) context)->xfi;
	settings = xfi->instance->settings;

	if (xfi->fullscreen != TRUE)
	{
		xfi->width = settings->DesktopWidth;
		xfi->height = settings->DesktopHeight;

		if (xfi->window)
			xf_ResizeDesktopWindow(xfi, xfi->window, settings->DesktopWidth, settings->DesktopHeight);

		if (xfi->primary)
		{
			same = (xfi->primary == xfi->drawing) ? TRUE : FALSE;

			XFreePixmap(xfi->display, xfi->primary);

			xfi->primary = XCreatePixmap(xfi->display, xfi->drawable,
					xfi->width, xfi->height, xfi->depth);

			if (same)
				xfi->drawing = xfi->primary;
		}
	}
	else
	{
		XSetFunction(xfi->display, xfi->gc, GXcopy);
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);
		XSetForeground(xfi->display, xfi->gc, 0);
		XFillRectangle(xfi->display, xfi->drawable, xfi->gc,
			0, 0, xfi->width, xfi->height);
	}
}

BOOL xf_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	xfInfo* xfi = ((xfContext*) instance->context)->xfi;

	rfds[*rcount] = (void*)(long)(xfi->xfds);
	(*rcount)++;

	return TRUE;
}

BOOL xf_process_x_events(freerdp* instance)
{
	XEvent xevent;
	xfInfo* xfi = ((xfContext*) instance->context)->xfi;

	while (XPending(xfi->display))
	{
		memset(&xevent, 0, sizeof(xevent));
		XNextEvent(xfi->display, &xevent);

		if (xf_event_process(instance, &xevent) != TRUE)
			return FALSE;
	}

	return TRUE;
}

void xf_create_window(xfInfo* xfi)
{
	XEvent xevent;
	char* win_title;
	int width, height;

	memset(&xevent, 0x00, sizeof(xevent));

	width = xfi->width;
	height = xfi->height;

	xfi->attribs.background_pixel = BlackPixelOfScreen(xfi->screen);
	xfi->attribs.border_pixel = WhitePixelOfScreen(xfi->screen);
	xfi->attribs.backing_store = xfi->primary ? NotUseful : Always;
	xfi->attribs.override_redirect = xfi->fullscreen;
	xfi->attribs.colormap = xfi->colormap;
	xfi->attribs.bit_gravity = NorthWestGravity;
	xfi->attribs.win_gravity = NorthWestGravity;
	
	xfi->us_mouse_event_delay = 1000000 / xfi->instance->settings->MouseEventRate;

	if (xfi->instance->settings->WindowTitle != NULL)
	{
		win_title = _strdup(xfi->instance->settings->WindowTitle);
	}
	else if (xfi->instance->settings->ServerPort == 3389)
	{
		win_title = malloc(1 + sizeof("FreeRDP: ") + strlen(xfi->instance->settings->ServerHostname));
		sprintf(win_title, "FreeRDP: %s", xfi->instance->settings->ServerHostname);
	}
	else
	{
		win_title = malloc(1 + sizeof("FreeRDP: ") + strlen(xfi->instance->settings->ServerHostname) + sizeof(":00000"));
		sprintf(win_title, "FreeRDP: %s:%i", xfi->instance->settings->ServerHostname, xfi->instance->settings->ServerPort);
	}

	xfi->window = xf_CreateDesktopWindow(xfi, win_title, width, height, xfi->decorations);
	free(win_title);

	if (xfi->fullscreen)
		xf_SetWindowFullscreen(xfi, xfi->window, xfi->fullscreen);

	xfi->unobscured = (xevent.xvisibility.state == VisibilityUnobscured);

	XSetWMProtocols(xfi->display, xfi->window->handle, &(xfi->WM_DELETE_WINDOW), 1);
	xfi->drawable = xfi->window->handle;
}

void xf_toggle_fullscreen(xfInfo* xfi)
{
	Pixmap contents = 0;

	contents = XCreatePixmap(xfi->display, xfi->window->handle, xfi->width, xfi->height, xfi->depth);
	XCopyArea(xfi->display, xfi->primary, contents, xfi->gc, 0, 0, xfi->width, xfi->height, 0, 0);

	XDestroyWindow(xfi->display, xfi->window->handle);
	xfi->fullscreen = (xfi->fullscreen) ? FALSE : TRUE;
	xf_create_window(xfi);

	XCopyArea(xfi->display, contents, xfi->primary, xfi->gc, 0, 0, xfi->width, xfi->height, 0, 0);
	XFreePixmap(xfi->display, contents);
}

BOOL xf_get_pixmap_info(xfInfo* xfi)
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

	pfs = XListPixmapFormats(xfi->display, &pf_count);

	if (pfs == NULL)
	{
		printf("xf_get_pixmap_info: XListPixmapFormats failed\n");
		return 1;
	}

	for (i = 0; i < pf_count; i++)
	{
		pf = pfs + i;

		if (pf->depth == xfi->depth)
		{
			xfi->bpp = pf->bits_per_pixel;
			xfi->scanline_pad = pf->scanline_pad;
			break;
		}
	}
	XFree(pfs);

	memset(&template, 0, sizeof(template));
	template.class = TrueColor;
	template.screen = xfi->screen_number;

	if (XGetWindowAttributes(xfi->display, RootWindowOfScreen(xfi->screen), &window_attributes) == 0)
	{
		printf("xf_get_pixmap_info: XGetWindowAttributes failed\n");
		return FALSE;
	}

	vis = XGetVisualInfo(xfi->display, VisualClassMask | VisualScreenMask, &template, &vi_count);

	if (vis == NULL)
	{
		printf("xf_get_pixmap_info: XGetVisualInfo failed\n");
		return FALSE;
	}

	vi = NULL;
	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->visual == window_attributes.visual)
		{
			xfi->visual = vi->visual;
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
			xfi->clrconv->invert = TRUE;
		}
	}

	XFree(vis);

	if ((xfi->visual == NULL) || (xfi->scanline_pad == 0))
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
	printf("%s", buf);

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
	int status;
	xfInfo* xfi;
	rdpFile* file;
	BOOL bitmap_cache;
	rdpSettings* settings;
	
	xfi = (xfInfo*) malloc(sizeof(xfInfo));
	ZeroMemory(xfi, sizeof(xfInfo));

	((xfContext*) instance->context)->xfi = xfi;

	xfi->_context = instance->context;
	xfi->context = (xfContext*) instance->context;
	xfi->context->settings = instance->settings;
	xfi->instance = instance;
	
	status = freerdp_client_parse_command_line_arguments(instance->context->argc,
				instance->context->argv, instance->settings);

	if (status < 0)
		exit(XF_EXIT_PARSE_ARGUMENTS);

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	settings = instance->settings;

	if (settings->ConnectionFile)
	{
		file = freerdp_client_rdp_file_new();

		printf("Using connection file: %s\n", settings->ConnectionFile);

		freerdp_client_parse_rdp_file(file, settings->ConnectionFile);
		freerdp_client_populate_settings_from_rdp_file(file, settings);
	}

	bitmap_cache = settings->BitmapCacheEnabled;

	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_NATIVE_XSERVER;

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
	settings->OrderSupport[NEG_MEMBLT_INDEX] = bitmap_cache;

	settings->OrderSupport[NEG_MEM3BLT_INDEX] = (settings->SoftwareGdi) ? TRUE : FALSE;

	settings->OrderSupport[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->OrderSupport[NEG_MEM3BLT_V2_INDEX] = FALSE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = FALSE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;

	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = (settings->SoftwareGdi) ? FALSE : TRUE;

	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = FALSE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = FALSE;

	freerdp_channels_pre_connect(xfi->_context->channels, instance);

	if (settings->AuthenticationOnly)
	{
		/* Check --authonly has a username and password. */
		if (settings->Username == NULL )
		{
			fprintf(stderr, "--authonly, but no -u username. Please provide one.\n");
			exit(1);
		}
		if (settings->Password == NULL )
		{
			fprintf(stderr, "--authonly, but no -p password. Please provide one.\n");
			exit(1);
		}
		fprintf(stderr, "%s:%d: Authentication only. Don't connect to X.\n", __FILE__, __LINE__);
		/* Avoid XWindows initialization and configuration below. */
		return TRUE;
	}

	xfi->display = XOpenDisplay(NULL);

	if (xfi->display == NULL)
	{
		printf("xf_pre_connect: failed to open display: %s\n", XDisplayName(NULL));
		printf("Please check that the $DISPLAY environment variable is properly set.\n");
		return FALSE;
	}

	if (xfi->debug)
	{
		printf("Enabling X11 debug mode.\n");
		XSynchronize(xfi->display, TRUE);
		_def_error_handler = XSetErrorHandler(_xf_error_handler);
	}

	xfi->_NET_WM_ICON = XInternAtom(xfi->display, "_NET_WM_ICON", False);
	xfi->_MOTIF_WM_HINTS = XInternAtom(xfi->display, "_MOTIF_WM_HINTS", False);
	xfi->_NET_CURRENT_DESKTOP = XInternAtom(xfi->display, "_NET_CURRENT_DESKTOP", False);
	xfi->_NET_WORKAREA = XInternAtom(xfi->display, "_NET_WORKAREA", False);
	xfi->_NET_WM_STATE = XInternAtom(xfi->display, "_NET_WM_STATE", False);
	xfi->_NET_WM_STATE_FULLSCREEN = XInternAtom(xfi->display, "_NET_WM_STATE_FULLSCREEN", False);
	xfi->_NET_WM_WINDOW_TYPE = XInternAtom(xfi->display, "_NET_WM_WINDOW_TYPE", False);

	xfi->_NET_WM_WINDOW_TYPE_NORMAL = XInternAtom(xfi->display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	xfi->_NET_WM_WINDOW_TYPE_DIALOG = XInternAtom(xfi->display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
	xfi->_NET_WM_WINDOW_TYPE_POPUP= XInternAtom(xfi->display, "_NET_WM_WINDOW_TYPE_POPUP", False);
	xfi->_NET_WM_WINDOW_TYPE_UTILITY = XInternAtom(xfi->display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	xfi->_NET_WM_WINDOW_TYPE_DROPDOWN_MENU = XInternAtom(xfi->display, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", False);
	xfi->_NET_WM_STATE_SKIP_TASKBAR = XInternAtom(xfi->display, "_NET_WM_STATE_SKIP_TASKBAR", False);
	xfi->_NET_WM_STATE_SKIP_PAGER = XInternAtom(xfi->display, "_NET_WM_STATE_SKIP_PAGER", False);
	xfi->_NET_WM_MOVERESIZE = XInternAtom(xfi->display, "_NET_WM_MOVERESIZE", False);
	xfi->_NET_MOVERESIZE_WINDOW = XInternAtom(xfi->display, "_NET_MOVERESIZE_WINDOW", False);

	xfi->WM_PROTOCOLS = XInternAtom(xfi->display, "WM_PROTOCOLS", False);
	xfi->WM_DELETE_WINDOW = XInternAtom(xfi->display, "WM_DELETE_WINDOW", False);
	xfi->WM_STATE = XInternAtom(xfi->display, "WM_STATE", False);

	xf_kbd_init(xfi);

	xfi->clrconv = freerdp_clrconv_new(CLRCONV_ALPHA);

	instance->context->cache = cache_new(instance->settings);

	xfi->xfds = ConnectionNumber(xfi->display);
	xfi->screen_number = DefaultScreen(xfi->display);
	xfi->screen = ScreenOfDisplay(xfi->display, xfi->screen_number);
	xfi->depth = DefaultDepthOfScreen(xfi->screen);
	xfi->big_endian = (ImageByteOrder(xfi->display) == MSBFirst);

	xfi->mouse_motion = settings->MouseMotion;
	xfi->complex_regions = TRUE;
	xfi->decorations = settings->Decorations;
	xfi->fullscreen = settings->Fullscreen;
	xfi->grab_keyboard = settings->GrabKeyboard;
	xfi->fullscreen_toggle = TRUE;
	xfi->sw_gdi = settings->SoftwareGdi;
	xfi->parent_window = (Window) settings->ParentWindowId;

	xf_detect_monitors(xfi, settings);

	return TRUE;
}

void cpuid(unsigned info, unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx)
{
#ifdef __GNUC__
#if defined(__i386__) || defined(__x86_64__)
	__asm volatile
	(
		/* The EBX (or RBX register on x86_64) is used for the PIC base address
		   and must not be corrupted by our inline assembly. */
#if defined(__i386__)
		"mov %%ebx, %%esi;"
		"cpuid;"
		"xchg %%ebx, %%esi;"
#else
		"mov %%rbx, %%rsi;"
		"cpuid;"
		"xchg %%rbx, %%rsi;"
#endif
		: "=a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
		: "0" (info)
	);
#endif
#endif
}

UINT32 xf_detect_cpu()
{
	unsigned int eax, ebx, ecx, edx = 0;
	UINT32 cpu_opt = 0;

	cpuid(1, &eax, &ebx, &ecx, &edx);

	if (edx & (1<<26)) 
	{
		DEBUG("SSE2 detected");
		cpu_opt |= CPU_SSE2;
	}

	return cpu_opt;
}

/**
 * Callback given to freerdp_connect() to perform post-connection operations.
 * It will be called only if the connection was initialized properly, and will continue the initialization based on the
 * newly created connection.
 */
BOOL xf_post_connect(freerdp* instance)
{
#ifdef WITH_SSE2
	UINT32 cpu;
#endif
	xfInfo* xfi;
	XGCValues gcv;
	rdpCache* cache;
	rdpChannels* channels;
	RFX_CONTEXT* rfx_context = NULL;
	NSC_CONTEXT* nsc_context = NULL;

	xfi = ((xfContext*) instance->context)->xfi;
	cache = instance->context->cache;
	channels = xfi->_context->channels;

	if (xf_get_pixmap_info(xfi) != TRUE)
		return FALSE;

	xf_register_graphics(instance->context->graphics);

	if (xfi->sw_gdi)
	{
		rdpGdi* gdi;
		UINT32 flags;

		flags = CLRCONV_ALPHA;

		if (xfi->bpp > 16)
			flags |= CLRBUF_32BPP;
		else
			flags |= CLRBUF_16BPP;

		gdi_init(instance, flags, NULL);
		gdi = instance->context->gdi;
		xfi->primary_buffer = gdi->primary_buffer;

		rfx_context = gdi->rfx_context;
	}
	else
	{
		xfi->srcBpp = instance->settings->ColorDepth;
		xf_gdi_register_update_callbacks(instance->update);

		xfi->hdc = gdi_CreateDC(xfi->clrconv, xfi->bpp);

		if (instance->settings->RemoteFxCodec)
		{
			rfx_context = (void*) rfx_context_new();
			xfi->rfx_context = rfx_context;
		}

		if (instance->settings->NSCodec)
		{
			nsc_context = (void*) nsc_context_new();
			xfi->nsc_context = nsc_context;
		}
	}

#ifdef WITH_SSE2
	/* detect only if needed */
	cpu = xf_detect_cpu();
	if (rfx_context)
		rfx_context_set_cpu_opt(rfx_context, cpu);
	if (nsc_context)
		nsc_context_set_cpu_opt(nsc_context, cpu);
#endif

	xfi->width = instance->settings->DesktopWidth;
	xfi->height = instance->settings->DesktopHeight;

	xf_create_window(xfi);

	memset(&gcv, 0, sizeof(gcv));
	xfi->modifier_map = XGetModifierMapping(xfi->display);

	xfi->gc = XCreateGC(xfi->display, xfi->drawable, GCGraphicsExposures, &gcv);
	xfi->primary = XCreatePixmap(xfi->display, xfi->drawable, xfi->width, xfi->height, xfi->depth);
	xfi->drawing = xfi->primary;

	xfi->bitmap_mono = XCreatePixmap(xfi->display, xfi->drawable, 8, 8, 1);
	xfi->gc_mono = XCreateGC(xfi->display, xfi->bitmap_mono, GCGraphicsExposures, &gcv);

	XSetFunction(xfi->display, xfi->gc, GXcopy);
	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XSetForeground(xfi->display, xfi->gc, BlackPixelOfScreen(xfi->screen));
	XFillRectangle(xfi->display, xfi->primary, xfi->gc, 0, 0, xfi->width, xfi->height);
	XFlush(xfi->display);

	xfi->image = XCreateImage(xfi->display, xfi->visual, xfi->depth, ZPixmap, 0,
			(char*) xfi->primary_buffer, xfi->width, xfi->height, xfi->scanline_pad, 0);

	xfi->bmp_codec_none = (BYTE*) malloc(64 * 64 * 4);

	if (xfi->sw_gdi)
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

	if (xfi->sw_gdi != TRUE)
	{
		glyph_cache_register_callbacks(instance->update);
		brush_cache_register_callbacks(instance->update);
		bitmap_cache_register_callbacks(instance->update);
		offscreen_cache_register_callbacks(instance->update);
		palette_cache_register_callbacks(instance->update);
	}

	instance->context->rail = rail_new(instance->settings);
	rail_register_update_callbacks(instance->context->rail, instance->update);
	xf_rail_register_callbacks(xfi, instance->context->rail);

	freerdp_channels_post_connect(channels, instance);

	xf_tsmf_init(xfi, xv_port);

	xf_cliprdr_init(xfi, channels);

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

int xf_receive_channel_data(freerdp* instance, int channelId, BYTE* data, int size, int flags, int total_size)
{
	return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

void xf_process_channel_event(rdpChannels* chanman, freerdp* instance)
{
	xfInfo* xfi;
	RDP_EVENT* event;

	xfi = ((xfContext*) instance->context)->xfi;

	event = freerdp_channels_pop_event(chanman);

	if (event)
	{
		switch (event->event_class)
		{
			case RDP_EVENT_CLASS_RAIL:
				xf_process_rail_event(xfi, chanman, event);
				break;

			case RDP_EVENT_CLASS_TSMF:
				xf_process_tsmf_event(xfi, event);
				break;

			case RDP_EVENT_CLASS_CLIPRDR:
				xf_process_cliprdr_event(xfi, event);
				break;

			default:
				break;
		}

		freerdp_event_free(event);
	}
}

void xf_window_free(xfInfo* xfi)
{
	rdpContext* context = xfi->instance->context;

	XFreeModifiermap(xfi->modifier_map);
	xfi->modifier_map = 0;

	if (xfi->gc != NULL)
	{
		XFreeGC(xfi->display, xfi->gc);
		xfi->gc = 0;
	}

	if (xfi->gc_mono != NULL)
	{
		XFreeGC(xfi->display, xfi->gc_mono);
		xfi->gc_mono = 0;
	}

	if (xfi->window != NULL)
	{
		xf_DestroyWindow(xfi, xfi->window);
		xfi->window = NULL;
	}

	if (xfi->primary)
	{
		XFreePixmap(xfi->display, xfi->primary);
		xfi->primary = 0;
	}

	if (xfi->bitmap_mono)
	{
		XFreePixmap(xfi->display, xfi->bitmap_mono);
		xfi->bitmap_mono = 0;
	}

	if (xfi->image)
	{
		xfi->image->data = NULL;
		XDestroyImage(xfi->image);
		xfi->image = NULL;
	}

	if (context != NULL)
	{
			cache_free(context->cache);
			context->cache = NULL;

			rail_free(context->rail);
			context->rail = NULL;
	}

	if (xfi->rfx_context) 
	{
		rfx_context_free(xfi->rfx_context);
		xfi->rfx_context = NULL;
	}

	if (xfi->nsc_context)
	{
		nsc_context_free(xfi->nsc_context);
		xfi->nsc_context = NULL;
	}

	freerdp_clrconv_free(xfi->clrconv);

	if (xfi->hdc)
		gdi_DeleteDC(xfi->hdc);

	xf_tsmf_uninit(xfi);
	xf_cliprdr_uninit(xfi);
}

void xf_free(xfInfo* xfi)
{
	xf_window_free(xfi);

	free(xfi->bmp_codec_none);

	XCloseDisplay(xfi->display);

	free(xfi);
}

/** Main loop for the rdp connection.
 *  It will be run from the thread's entry point (thread_func()).
 *  It initiates the connection, and will continue to run until the session ends,
 *  processing events as they are received.
 *  @param instance - pointer to the rdp_freerdp structure that contains the session's settings
 *  @return A code from the enum XF_EXIT_CODE (0 if successfull)
 */
int xfreerdp_run(freerdp* instance)
{
	int i;
	int fds;
	xfInfo* xfi;
	int max_fds;
	int rcount;
	int wcount;
	int ret = 0;
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	int select_status;
	rdpChannels* channels;
	struct timeval timeout;

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));
	memset(&timeout, 0, sizeof(struct timeval));

	BOOL status = freerdp_connect(instance);
	/* Connection succeeded. --authonly ? */
	if (instance->settings->AuthenticationOnly)
	{
		freerdp_disconnect(instance);
		fprintf(stderr, "%s:%d: Authentication only, exit status %d\n", __FILE__, __LINE__, !status);
		exit(!status);
	}

	if (!status)
	{
		xf_free(((xfContext*) instance->context)->xfi);
		return XF_EXIT_CONN_FAILED;
	}

	xfi = ((xfContext*) instance->context)->xfi;
	channels = instance->context->channels;

	while (!xfi->disconnect && !freerdp_shall_disconnect(instance))
	{
		rcount = 0;
		wcount = 0;

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			ret = XF_EXIT_CONN_FAILED;
			break;
		}
		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get channel manager file descriptor\n");
			ret = XF_EXIT_CONN_FAILED;
			break;
		}
		if (xf_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE)
		{
			printf("Failed to get xfreerdp file descriptor\n");
			ret = XF_EXIT_CONN_FAILED;
			break;
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

		select_status = select(max_fds + 1, &rfds_set, &wfds_set, NULL, &timeout);

		if (select_status == 0)
		{
			continue;
		}
		else if (select_status == -1)
		{
			/* these are not really errors */
			if (!((errno == EAGAIN) ||
				(errno == EWOULDBLOCK) ||
				(errno == EINPROGRESS) ||
				(errno == EINTR))) /* signal occurred */
			{
				printf("xfreerdp_run: select failed\n");
				break;
			}
		}

		if (freerdp_check_fds(instance) != TRUE)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (xf_process_x_events(instance) != TRUE)
		{
			printf("Closed from X\n");
			break;
		}
		if (freerdp_channels_check_fds(channels, instance) != TRUE)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		xf_process_channel_event(channels, instance);
	}

	FILE *fin = fopen("/tmp/tsmf.tid", "rt");
	if(fin)
	{
		int thid = 0;
		fscanf(fin, "%d", &thid);
		fclose(fin);
		pthread_kill((pthread_t) (size_t) thid, SIGUSR1);

		FILE *fin1 = fopen("/tmp/tsmf.tid", "rt");
		int timeout = 5;
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

	if (!ret)
		ret = freerdp_error_info(instance);

	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	freerdp_disconnect(instance);
	gdi_free(instance);
	xf_free(xfi);

	return ret;
}

/** Entry point for the thread that will deal with the session.
 *  It just calls xfreerdp_run() using the given instance as parameter.
 *  @param param - pointer to a thread_data structure that contains the initialized connection.
 */
void* thread_func(void* param)
{
	struct thread_data* data;
	data = (struct thread_data*) param;

	g_disconnect_reason = xfreerdp_run(data->instance);

	free(data);

	g_thread_count--;

	if (g_thread_count < 1)
		ReleaseSemaphore(g_sem, 1, NULL);

	pthread_exit(NULL);
}

static BYTE exit_code_from_disconnect_reason(UINT32 reason)
{
	if (reason == 0 ||
	   (reason >= XF_EXIT_PARSE_ARGUMENTS && reason <= XF_EXIT_CONN_FAILED))
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

int main(int argc, char* argv[])
{
	pthread_t thread;
	freerdp* instance;
	struct thread_data* data;

	freerdp_handle_signals();

	setlocale(LC_ALL, "");

	freerdp_channels_global_init();

	g_sem = CreateSemaphore(NULL, 0, 1, NULL);

	instance = freerdp_new();
	instance->PreConnect = xf_pre_connect;
	instance->PostConnect = xf_post_connect;
	instance->Authenticate = xf_authenticate;
	instance->VerifyCertificate = xf_verify_certificate;
	instance->ReceiveChannelData = xf_receive_channel_data;

	instance->context_size = sizeof(xfContext);
	instance->ContextNew = (pContextNew) xf_context_new;
	instance->ContextFree = (pContextFree) xf_context_free;
	freerdp_context_new(instance);

	instance->context->argc = argc;
	instance->context->argv = argv;
	instance->settings->SoftwareGdi = FALSE;

	data = (struct thread_data*) malloc(sizeof(struct thread_data));
	ZeroMemory(data, sizeof(struct thread_data));

	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
		WaitForSingleObject(g_sem, INFINITE);
	}

	pthread_join(thread, NULL);
	pthread_detach(thread);

	freerdp_context_free(instance);
	freerdp_free(instance);

	freerdp_channels_global_uninit();

	return exit_code_from_disconnect_reason(g_disconnect_reason);
}
