/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Client
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
#include <freerdp/utils/args.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/event.h>
#include <freerdp/utils/signal.h>
#include <freerdp/utils/passphrase.h>
#include <freerdp/plugins/cliprdr.h>
#include <freerdp/rail.h>

#include "xf_gdi.h"
#include "xf_rail.h"
#include "xf_tsmf.h"
#include "xf_event.h"
#include "xf_cliprdr.h"
#include "xf_monitor.h"
#include "xf_graphics.h"
#include "xf_keyboard.h"

#include "xfreerdp.h"

static freerdp_sem g_sem;
static int g_thread_count = 0;
static uint8 g_disconnect_reason = 0;

static long xv_port = 0;
static const size_t password_size = 512;

struct thread_data
{
	freerdp* instance;
};

int xf_process_client_args(rdpSettings* settings, const char* opt, const char* val, void* user_data);
int xf_process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data);

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
	sint32 x, y;
	uint32 w, h;

	xfi = ((xfContext*) context)->xfi;
	gdi = context->gdi;

	if (xfi->remote_app != true)
	{
		if (xfi->complex_regions != true)
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

	if (xfi->fullscreen != true)
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
	sint32 x, y;
	uint32 w, h;

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
	boolean same;
	rdpSettings* settings;

	xfi = ((xfContext*) context)->xfi;
	settings = xfi->instance->settings;

	if (xfi->fullscreen != true)
	{
		xfi->width = settings->width;
		xfi->height = settings->height;

		if (xfi->window)
			xf_ResizeDesktopWindow(xfi, xfi->window, settings->width, settings->height);

		if (xfi->primary)
		{
			same = (xfi->primary == xfi->drawing) ? true : false;

			XFreePixmap(xfi->display, xfi->primary);

			xfi->primary = XCreatePixmap(xfi->display, xfi->drawable,
					xfi->width, xfi->height, xfi->depth);

			if (same)
				xfi->drawing = xfi->primary;
		}
	}
}

boolean xf_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	xfInfo* xfi = ((xfContext*) instance->context)->xfi;

	rfds[*rcount] = (void*)(long)(xfi->xfds);
	(*rcount)++;

	return true;
}

boolean xf_check_fds(freerdp* instance, fd_set* set)
{
	XEvent xevent;
	xfInfo* xfi = ((xfContext*) instance->context)->xfi;

	while (XPending(xfi->display))
	{
		memset(&xevent, 0, sizeof(xevent));
		XNextEvent(xfi->display, &xevent);

		if (xf_event_process(instance, &xevent) != true)
			return false;
	}

	return true;
}

void xf_create_window(xfInfo* xfi)
{
	XEvent xevent;
	char* win_title;
	int width, height;

	width = xfi->width;
	height = xfi->height;

	xfi->attribs.background_pixel = BlackPixelOfScreen(xfi->screen);
	xfi->attribs.border_pixel = WhitePixelOfScreen(xfi->screen);
	xfi->attribs.backing_store = xfi->primary ? NotUseful : Always;
	xfi->attribs.override_redirect = xfi->fullscreen;
	xfi->attribs.colormap = xfi->colormap;
	xfi->attribs.bit_gravity = ForgetGravity;
	xfi->attribs.win_gravity = StaticGravity;

	if (xfi->instance->settings->window_title != NULL)
	{
		win_title = xstrdup(xfi->instance->settings->window_title);
	}
	else if (xfi->instance->settings->port == 3389)
	{
		win_title = xmalloc(1 + sizeof("FreeRDP: ") + strlen(xfi->instance->settings->hostname));
		sprintf(win_title, "FreeRDP: %s", xfi->instance->settings->hostname);
	}
	else
	{
		win_title = xmalloc(1 + sizeof("FreeRDP: ") + strlen(xfi->instance->settings->hostname) + sizeof(":00000"));
		sprintf(win_title, "FreeRDP: %s:%i", xfi->instance->settings->hostname, xfi->instance->settings->port);
	}

	xfi->window = xf_CreateDesktopWindow(xfi, win_title, width, height, xfi->decorations);
	xfree(win_title);

	if (xfi->parent_window)
		XReparentWindow(xfi->display, xfi->window->handle, xfi->parent_window, 0, 0);

	if (xfi->fullscreen)
		xf_SetWindowFullscreen(xfi, xfi->window, xfi->fullscreen);

	/* wait for VisibilityNotify */
	do
	{
		XMaskEvent(xfi->display, VisibilityChangeMask, &xevent);
	}
	while (xevent.type != VisibilityNotify);

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
	xfi->fullscreen = (xfi->fullscreen) ? false : true;
	xf_create_window(xfi);

	XCopyArea(xfi->display, contents, xfi->primary, xfi->gc, 0, 0, xfi->width, xfi->height, 0, 0);
	XFreePixmap(xfi->display, contents);
}

boolean xf_get_pixmap_info(xfInfo* xfi)
{
	int i;
	int vi_count;
	int pf_count;
	XVisualInfo* vi;
	XVisualInfo* vis;
	XVisualInfo template;
	XPixmapFormatValues* pf;
	XPixmapFormatValues* pfs;

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

	vis = XGetVisualInfo(xfi->display, VisualClassMask | VisualScreenMask, &template, &vi_count);

	if (vis == NULL)
	{
		printf("xf_get_pixmap_info: XGetVisualInfo failed\n");
		return false;
	}

	vi = NULL;
	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->depth == xfi->depth)
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
			xfi->clrconv->invert = true;
		}
	}

	XFree(vis);

	if ((xfi->visual == NULL) || (xfi->scanline_pad == 0))
	{
		return false;
	}

	return true;
}

static int (*_def_error_handler)(Display*, XErrorEvent*);
int xf_error_handler(Display* d, XErrorEvent* ev)
{
	char buf[256];
	int do_abort = true;

	XGetErrorText(d, ev->error_code, buf, sizeof(buf));
	printf("%s", buf);

	if (do_abort)
		abort();

	_def_error_handler(d, ev);

	return false;
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

boolean xf_pre_connect(freerdp* instance)
{
	xfInfo* xfi;
	boolean bitmap_cache;
	rdpSettings* settings;
	int arg_parse_result;
	
	xfi = (xfInfo*) xzalloc(sizeof(xfInfo));
	((xfContext*) instance->context)->xfi = xfi;

	xfi->_context = instance->context;
	xfi->context = (xfContext*) instance->context;
	xfi->context->settings = instance->settings;
	xfi->instance = instance;
	
	arg_parse_result = freerdp_parse_args(instance->settings, instance->context->argc,instance->context->argv,
				xf_process_plugin_args, instance->context->channels, xf_process_client_args, xfi);
	
	if (arg_parse_result < 0)
	{
		if (arg_parse_result == FREERDP_ARGS_PARSE_FAILURE)
			printf("failed to parse arguments.\n");
		
		exit(XF_EXIT_PARSE_ARGUMENTS);
	}

	settings = instance->settings;
	bitmap_cache = settings->bitmap_cache;

	settings->os_major_type = OSMAJORTYPE_UNIX;
	settings->os_minor_type = OSMINORTYPE_NATIVE_XSERVER;
	settings->order_support[NEG_DSTBLT_INDEX] = true;
	settings->order_support[NEG_PATBLT_INDEX] = true;
	settings->order_support[NEG_SCRBLT_INDEX] = true;
	settings->order_support[NEG_OPAQUE_RECT_INDEX] = true;
	settings->order_support[NEG_DRAWNINEGRID_INDEX] = false;
	settings->order_support[NEG_MULTIDSTBLT_INDEX] = false;
	settings->order_support[NEG_MULTIPATBLT_INDEX] = false;
	settings->order_support[NEG_MULTISCRBLT_INDEX] = false;
	settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = true;
	settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = false;
	settings->order_support[NEG_LINETO_INDEX] = true;
	settings->order_support[NEG_POLYLINE_INDEX] = true;
	settings->order_support[NEG_MEMBLT_INDEX] = bitmap_cache;
	settings->order_support[NEG_MEM3BLT_INDEX] = false;
	settings->order_support[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->order_support[NEG_MEM3BLT_V2_INDEX] = false;
	settings->order_support[NEG_SAVEBITMAP_INDEX] = false;
	settings->order_support[NEG_GLYPH_INDEX_INDEX] = true;
	settings->order_support[NEG_FAST_INDEX_INDEX] = true;
	settings->order_support[NEG_FAST_GLYPH_INDEX] = true;
	settings->order_support[NEG_POLYGON_SC_INDEX] = false;
	settings->order_support[NEG_POLYGON_CB_INDEX] = false;
	settings->order_support[NEG_ELLIPSE_SC_INDEX] = false;
	settings->order_support[NEG_ELLIPSE_CB_INDEX] = false;

	freerdp_channels_pre_connect(xfi->_context->channels, instance);

	xfi->display = XOpenDisplay(NULL);

	if (xfi->display == NULL)
	{
		printf("xf_pre_connect: failed to open display: %s\n", XDisplayName(NULL));
		printf("Please check that the $DISPLAY environment variable is properly set.\n");
		return false;
	}

	if (xfi->debug)
	{
		printf("Enabling X11 debug mode.\n");
		XSynchronize(xfi->display, true);
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

	xf_kbd_init(xfi);

	xfi->clrconv = freerdp_clrconv_new(CLRCONV_ALPHA);

	instance->context->cache = cache_new(instance->settings);

	xfi->xfds = ConnectionNumber(xfi->display);
	xfi->screen_number = DefaultScreen(xfi->display);
	xfi->screen = ScreenOfDisplay(xfi->display, xfi->screen_number);
	xfi->depth = DefaultDepthOfScreen(xfi->screen);
	xfi->big_endian = (ImageByteOrder(xfi->display) == MSBFirst);

	xfi->mouse_motion = settings->mouse_motion;
	xfi->complex_regions = true;
	xfi->decorations = settings->decorations;
	xfi->fullscreen = settings->fullscreen;
	xfi->grab_keyboard = settings->grab_keyboard;
	xfi->fullscreen_toggle = true;
	xfi->sw_gdi = settings->sw_gdi;
	xfi->parent_window = (Window) settings->parent_window_xid;

	xf_detect_monitors(xfi, settings);

	return true;
}

void cpuid(unsigned info, unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx)
{
#ifdef __GNUC__
#if defined(__i386__) || defined(__x86_64__)
	*eax = info;
	__asm volatile
		("mov %%ebx, %%edi;" /* 32bit PIC: don't clobber ebx */
		 "cpuid;"
		 "mov %%ebx, %%esi;"
		 "mov %%edi, %%ebx;"
		 :"+a" (*eax), "=S" (*ebx), "=c" (*ecx), "=d" (*edx)
		 : :"edi");
#endif
#endif
}
 
uint32 xf_detect_cpu()
{
	unsigned int eax, ebx, ecx, edx = 0;
	uint32 cpu_opt = 0;

	cpuid(1, &eax, &ebx, &ecx, &edx);

	if (edx & (1<<26)) 
	{
		DEBUG("SSE2 detected");
		cpu_opt |= CPU_SSE2;
	}

	return cpu_opt;
}

boolean xf_post_connect(freerdp* instance)
{
	xfInfo* xfi;
	XGCValues gcv;
	rdpCache* cache;
	rdpChannels* channels;
	RFX_CONTEXT* rfx_context = NULL;

	xfi = ((xfContext*) instance->context)->xfi;
	cache = instance->context->cache;
	channels = xfi->_context->channels;

	if (xf_get_pixmap_info(xfi) != true)
		return false;

	xf_register_graphics(instance->context->graphics);

	if (xfi->sw_gdi)
	{
		rdpGdi* gdi;
		uint32 flags;

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
		xfi->srcBpp = instance->settings->color_depth;
		xf_gdi_register_update_callbacks(instance->update);

		xfi->hdc = gdi_CreateDC(xfi->clrconv, xfi->bpp);

		if (instance->settings->rfx_codec)
		{
			rfx_context = (void*) rfx_context_new();
			xfi->rfx_context = rfx_context;
		}

		if (instance->settings->ns_codec)
			xfi->nsc_context = (void*) nsc_context_new();
	}

	if (rfx_context)
	{
#ifdef WITH_SSE2
		/* detect only if needed */
		rfx_context_set_cpu_opt(rfx_context, xf_detect_cpu());
#endif
	}

	xfi->width = instance->settings->width;
	xfi->height = instance->settings->height;

	xf_create_window(xfi);

	memset(&gcv, 0, sizeof(gcv));
	xfi->modifier_map = XGetModifierMapping(xfi->display);

	xfi->gc = XCreateGC(xfi->display, xfi->drawable, GCGraphicsExposures, &gcv);
	xfi->primary = XCreatePixmap(xfi->display, xfi->drawable, xfi->width, xfi->height, xfi->depth);
	xfi->drawing = xfi->primary;

	xfi->bitmap_mono = XCreatePixmap(xfi->display, xfi->drawable, 8, 8, 1);
	xfi->gc_mono = XCreateGC(xfi->display, xfi->bitmap_mono, GCGraphicsExposures, &gcv);

	XSetForeground(xfi->display, xfi->gc, BlackPixelOfScreen(xfi->screen));
	XFillRectangle(xfi->display, xfi->primary, xfi->gc, 0, 0, xfi->width, xfi->height);

	xfi->image = XCreateImage(xfi->display, xfi->visual, xfi->depth, ZPixmap, 0,
			(char*) xfi->primary_buffer, xfi->width, xfi->height, xfi->scanline_pad, 0);

	xfi->bmp_codec_none = (uint8*) xmalloc(64 * 64 * 4);

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

	if (xfi->sw_gdi != true)
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

	return true;
}

boolean xf_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	*password = xmalloc(password_size * sizeof(char));

	if (freerdp_passphrase_read("Password: ", *password, password_size) == NULL)
		return false;

	return true;
}

boolean xf_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
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

		if (answer == 'y' || answer == 'Y')
		{
			return true;
		}
		else if (answer == 'n' || answer == 'N')
		{
			break;
		}
		printf("\n");
	}

	return false;
}

int xf_process_client_args(rdpSettings* settings, const char* opt, const char* val, void* user_data)
{
	int argc = 0;
	xfInfo* xfi = (xfInfo*) user_data;

	if (strcmp("--kbd-list", opt) == 0)
	{
		int i;
		rdpKeyboardLayout* layouts;

		layouts = freerdp_kbd_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_STANDARD);
		printf("\nKeyboard Layouts\n");
		for (i = 0; layouts[i].code; i++)
			printf("0x%08X\t%s\n", layouts[i].code, layouts[i].name);
		free(layouts);

		layouts = freerdp_kbd_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_VARIANT);
		printf("\nKeyboard Layout Variants\n");
		for (i = 0; layouts[i].code; i++)
			printf("0x%08X\t%s\n", layouts[i].code, layouts[i].name);
		free(layouts);

		layouts = freerdp_kbd_get_layouts(RDP_KEYBOARD_LAYOUT_TYPE_IME);
		printf("\nKeyboard Input Method Editors (IMEs)\n");
		for (i = 0; layouts[i].code; i++)
			printf("0x%08X\t%s\n", layouts[i].code, layouts[i].name);
		free(layouts);

		exit(0);
	}
	else if (strcmp("--xv-port", opt) == 0)
	{
		xv_port = atoi(val);
		argc = 2;
	}
	else if (strcmp("--dbg-x11", opt) == 0)
	{
		xfi->debug = true;
		argc = 1;
	}

	return argc;
}

int xf_process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChannels* channels = (rdpChannels*) user_data;

	printf("loading plugin %s\n", name);
	freerdp_channels_load_plugin(channels, settings, name, plugin_data);

	return 1;
}

int xf_receive_channel_data(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size)
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

	XFreeGC(xfi->display, xfi->gc);
	xfi->gc = 0;

	XFreeGC(xfi->display, xfi->gc_mono);
	xfi->gc_mono = 0;

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

	freerdp_clrconv_free(xfi->clrconv);

	if (xfi->hdc)
		gdi_DeleteDC(xfi->hdc);

	xf_tsmf_uninit(xfi);
	xf_cliprdr_uninit(xfi);
}

void xf_free(xfInfo* xfi)
{
	xf_window_free(xfi);

	xfree(xfi->bmp_codec_none);

	XCloseDisplay(xfi->display);

	xfree(xfi);
}

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

	if (!freerdp_connect(instance))
		return XF_EXIT_CONN_FAILED;

	xfi = ((xfContext*) instance->context)->xfi;
	channels = instance->context->channels;

	while (!xfi->disconnect && !freerdp_shall_disconnect(instance))
	{
		rcount = 0;
		wcount = 0;

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != true)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			ret = XF_EXIT_CONN_FAILED;
			break;
		}
		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != true)
		{
			printf("Failed to get channel manager file descriptor\n");
			ret = XF_EXIT_CONN_FAILED;
			break;
		}
		if (xf_get_fds(instance, rfds, &rcount, wfds, &wcount) != true)
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

		timeout.tv_sec = 5;
		select_status = select(max_fds + 1, &rfds_set, &wfds_set, NULL, &timeout);

		if (select_status == 0)
		{
			//freerdp_send_keep_alive(instance);
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

		if (freerdp_check_fds(instance) != true)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (xf_check_fds(instance, &rfds_set) != true)
		{
			printf("Failed to check xfreerdp file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(channels, instance) != true)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		xf_process_channel_event(channels, instance);
	}

	if (!ret)
		ret = freerdp_error_info(instance);

	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	freerdp_disconnect(instance);
	gdi_free(instance);
	xf_free(xfi);

	freerdp_free(instance);

	return ret;
}

void* thread_func(void* param)
{
	struct thread_data* data;
	data = (struct thread_data*) param;

	g_disconnect_reason = xfreerdp_run(data->instance);

	xfree(data);

	pthread_detach(pthread_self());

	g_thread_count--;

        if (g_thread_count < 1)
                freerdp_sem_signal(g_sem);

	pthread_exit(NULL);
}

static uint8 exit_code_from_disconnect_reason(uint32 reason)
{
	if (reason == 0 ||
	   (reason >= XF_EXIT_PARSE_ARGUMENTS && reason <= XF_EXIT_CONN_FAILED))
		 return reason;

	/* Licence error set */
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

	g_sem = freerdp_sem_new(1);

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
	instance->settings->sw_gdi = false;

	data = (struct thread_data*) xzalloc(sizeof(struct thread_data));
	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
                freerdp_sem_wait(g_sem);
	}

	freerdp_channels_global_uninit();

	return exit_code_from_disconnect_reason(g_disconnect_reason);
}
