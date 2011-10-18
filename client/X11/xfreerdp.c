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
#include "xf_keyboard.h"

#include "xfreerdp.h"

freerdp_sem g_sem;
static int g_thread_count = 0;

static long xv_port = 0;
const size_t password_size = 512;

struct thread_data
{
	freerdp* instance;
};

int xf_process_client_args(rdpSettings* settings, const char* opt, const char* val, void* user_data);
int xf_process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data);

void xf_context_size(freerdp* instance, uint32* size)
{
	*size = sizeof(xfContext);
}

void xf_context_new(freerdp* instance, xfContext* context)
{
	rdpContext* _context = &context->_p;
	_context->channels = freerdp_channels_new();
}

void xf_context_free(freerdp* instance, xfContext* context)
{

}

void xf_sw_begin_paint(rdpUpdate* update)
{
	rdpGdi* gdi = update->context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void xf_sw_end_paint(rdpUpdate* update)
{
	rdpGdi* gdi;
	xfInfo* xfi;
	sint32 x, y;
	uint32 w, h;
	xfContext* context;

	context = (xfContext*) update->context;
	gdi = update->context->gdi;
	xfi = context->xfi;

	if (xfi->remote_app != True)
	{
		if (xfi->complex_regions != True)
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

		xf_rail_paint(xfi, update->context->rail, x, y, x + w - 1, y + h - 1);
	}
}

void xf_sw_desktop_resize(rdpUpdate* update)
{
	xfInfo* xfi;
	rdpSettings* settings;

	xfi = ((xfContext*) update->context)->xfi;
	settings = xfi->instance->settings;

	if (xfi->fullscreen != True)
	{
		rdpGdi* gdi = update->context->gdi;
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

void xf_hw_begin_paint(rdpUpdate* update)
{
	xfInfo* xfi;
	xfi = ((xfContext*) update->context)->xfi;
	xfi->hdc->hwnd->invalid->null = 1;
	xfi->hdc->hwnd->ninvalid = 0;
}

void xf_hw_end_paint(rdpUpdate* update)
{
	xfInfo* xfi;
	sint32 x, y;
	uint32 w, h;

	xfi = ((xfContext*) update->context)->xfi;

	if (xfi->remote_app)
	{
		if (xfi->hdc->hwnd->invalid->null)
			return;

		x = xfi->hdc->hwnd->invalid->x;
		y = xfi->hdc->hwnd->invalid->y;
		w = xfi->hdc->hwnd->invalid->w;
		h = xfi->hdc->hwnd->invalid->h;

		xf_rail_paint(xfi, update->context->rail, x, y, x + w - 1, y + h - 1);
	}
}

void xf_hw_desktop_resize(rdpUpdate* update)
{
	xfInfo* xfi;
	boolean same;
	rdpSettings* settings;

	xfi = ((xfContext*) update->context)->xfi;
	settings = xfi->instance->settings;

	if (xfi->fullscreen != True)
	{
		xfi->width = settings->width;
		xfi->height = settings->height;

		if (xfi->window)
			xf_ResizeDesktopWindow(xfi, xfi->window, settings->width, settings->height);

		if (xfi->primary)
		{
			same = (xfi->primary == xfi->drawing) ? True : False;

			XFreePixmap(xfi->display, xfi->primary);

			xfi->primary = XCreatePixmap(xfi->display, xfi->drawable,
					xfi->width, xfi->height, xfi->depth);

			if (same)
				xfi->drawing = xfi->primary;
		}
	}
}

void xf_bitmap_size(rdpUpdate* update, uint32* size)
{
	*size = sizeof(xfBitmap);
}

void xf_bitmap_new(rdpUpdate* update, xfBitmap* bitmap)
{
	uint8* cdata;
	XImage* image;
	rdpBitmap* _bitmap;
	xfInfo* xfi = ((xfContext*) update->context)->xfi;

	_bitmap = (rdpBitmap*) &bitmap->bitmap;
	bitmap->pixmap = XCreatePixmap(xfi->display, xfi->drawable, _bitmap->width, _bitmap->height, xfi->depth);

	if (_bitmap->dstData != NULL)
	{
		cdata = freerdp_image_convert(_bitmap->dstData, NULL,
				_bitmap->width, _bitmap->height, _bitmap->bpp, xfi->bpp, xfi->clrconv);

		image = XCreateImage(xfi->display, xfi->visual, xfi->depth,
				ZPixmap, 0, (char*) cdata, _bitmap->width, _bitmap->height, xfi->scanline_pad, 0);

		XPutImage(xfi->display, bitmap->pixmap, xfi->gc, image, 0, 0, 0, 0, _bitmap->width, _bitmap->height);
		XFree(image);

		if (cdata != _bitmap->dstData)
			xfree(cdata);
	}
}

void xf_offscreen_bitmap_new(rdpUpdate* update, xfBitmap* bitmap)
{
	rdpBitmap* _bitmap;
	xfInfo* xfi = ((xfContext*) update->context)->xfi;

	_bitmap = (rdpBitmap*) &bitmap->bitmap;
	bitmap->pixmap = XCreatePixmap(xfi->display, xfi->drawable, _bitmap->width, _bitmap->height, xfi->depth);
}

void xf_set_surface(rdpUpdate* update, xfBitmap* bitmap, boolean primary)
{
	xfInfo* xfi = ((xfContext*) update->context)->xfi;

	if (primary)
		xfi->drawing = xfi->primary;
	else
		xfi->drawing = bitmap->pixmap;
}

void xf_bitmap_free(rdpUpdate* update, xfBitmap* bitmap)
{
	xfInfo* xfi = ((xfContext*) update->context)->xfi;

	if (bitmap->pixmap != 0)
		XFreePixmap(xfi->display, bitmap->pixmap);
}

void xf_pointer_size(rdpUpdate* update, uint32* size)
{
	*size = sizeof(xfPointer);
}

void xf_pointer_set(rdpUpdate* update, xfPointer* pointer)
{
	xfInfo* xfi = ((xfContext*) update->context)->xfi;

	if (xfi->remote_app != True)
		XDefineCursor(xfi->display, xfi->window->handle, pointer->cursor);
}

void xf_pointer_new(rdpUpdate* update, xfPointer* pointer)
{
	xfInfo* xfi;
	XcursorImage ci;
	rdpPointer* _pointer;

	xfi = ((xfContext*) update->context)->xfi;
	_pointer = (rdpPointer*) &pointer->pointer;

	memset(&ci, 0, sizeof(ci));
	ci.version = XCURSOR_IMAGE_VERSION;
	ci.size = sizeof(ci);
	ci.width = _pointer->width;
	ci.height = _pointer->height;
	ci.xhot = _pointer->xPos;
	ci.yhot = _pointer->yPos;
	ci.pixels = (XcursorPixel*) malloc(ci.width * ci.height * 4);
	memset(ci.pixels, 0, ci.width * ci.height * 4);

	if ((_pointer->andMaskData != 0) && (_pointer->xorMaskData != 0))
	{
		freerdp_alpha_cursor_convert((uint8*) (ci.pixels), _pointer->xorMaskData, _pointer->andMaskData,
				_pointer->width, _pointer->height, _pointer->xorBpp, xfi->clrconv);
	}

	if (_pointer->xorBpp > 24)
	{
		freerdp_image_swap_color_order((uint8*) ci.pixels, ci.width, ci.height);
	}

	pointer->cursor = XcursorImageLoadCursor(xfi->display, &ci);
	xfree(ci.pixels);
}

void xf_pointer_free(rdpUpdate* update, xfPointer* pointer)
{
	xfInfo* xfi = ((xfContext*) update->context)->xfi;

	if (pointer->cursor != 0)
		XFreeCursor(xfi->display, pointer->cursor);
}

boolean xf_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	xfInfo* xfi = ((xfContext*) instance->context)->xfi;

	rfds[*rcount] = (void*)(long)(xfi->xfds);
	(*rcount)++;

	return True;
}

boolean xf_check_fds(freerdp* instance, fd_set* set)
{
	XEvent xevent;
	xfInfo* xfi = ((xfContext*) instance->context)->xfi;

	while (XPending(xfi->display))
	{
		memset(&xevent, 0, sizeof(xevent));
		XNextEvent(xfi->display, &xevent);

		if (xf_event_process(instance, &xevent) != True)
			return False;
	}

	return True;
}

void xf_create_window(xfInfo* xfi)
{
	XEvent xevent;
	char* title;
	char* hostname;
	int width, height;

	width = xfi->width;
	height = xfi->height;

	xfi->attribs.background_pixel = BlackPixelOfScreen(xfi->screen);
	xfi->attribs.border_pixel = WhitePixelOfScreen(xfi->screen);
	xfi->attribs.backing_store = xfi->primary ? NotUseful : Always;
	xfi->attribs.override_redirect = xfi->fullscreen;
	xfi->attribs.colormap = xfi->colormap;

	if (xfi->remote_app != True)
	{
		if (xfi->fullscreen)
		{
			width = xfi->fullscreen ? WidthOfScreen(xfi->screen) : xfi->width;
			height = xfi->fullscreen ? HeightOfScreen(xfi->screen) : xfi->height;
		}

		hostname = xfi->instance->settings->hostname;
		title = xmalloc(sizeof("FreeRDP: ") + strlen(hostname));
		sprintf(title, "FreeRDP: %s", hostname);

		xfi->window = xf_CreateDesktopWindow(xfi, title, width, height, xfi->decorations);
		xfree(title);

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
	else
	{
		xfi->drawable = DefaultRootWindow(xfi->display);
	}
}

void xf_toggle_fullscreen(xfInfo* xfi)
{
	Pixmap contents = 0;

	contents = XCreatePixmap(xfi->display, xfi->window->handle, xfi->width, xfi->height, xfi->depth);
	XCopyArea(xfi->display, xfi->primary, contents, xfi->gc, 0, 0, xfi->width, xfi->height, 0, 0);

	XDestroyWindow(xfi->display, xfi->window->handle);
	xfi->fullscreen = (xfi->fullscreen) ? False : True;
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
		return False;
	}

	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->depth == xfi->depth)
		{
			xfi->visual = vi->visual;
			break;
		}
	}
	XFree(vis);

	if ((xfi->visual == NULL) || (xfi->scanline_pad == 0))
	{
		return False;
	}

	return True;
}

boolean xf_pre_connect(freerdp* instance)
{
	xfInfo* xfi;
	boolean bitmap_cache;
	rdpSettings* settings;

	xfi = (xfInfo*) xzalloc(sizeof(xfInfo));
	((xfContext*) instance->context)->xfi = xfi;

	xfi->_context = instance->context;
	xfi->context = (xfContext*) instance->context;
	xfi->context->settings = instance->settings;
	xfi->instance = instance;

	if (freerdp_parse_args(instance->settings, instance->context->argc, instance->context->argv,
			xf_process_plugin_args, instance->context->channels, xf_process_client_args, xfi) < 0)
	{
		printf("failed to parse arguments.\n");
		exit(0);
	}

	settings = instance->settings;
	bitmap_cache = settings->bitmap_cache;

	settings->order_support[NEG_DSTBLT_INDEX] = True;
	settings->order_support[NEG_PATBLT_INDEX] = True;
	settings->order_support[NEG_SCRBLT_INDEX] = True;
	settings->order_support[NEG_OPAQUE_RECT_INDEX] = True;
	settings->order_support[NEG_DRAWNINEGRID_INDEX] = False;
	settings->order_support[NEG_MULTIDSTBLT_INDEX] = False;
	settings->order_support[NEG_MULTIPATBLT_INDEX] = False;
	settings->order_support[NEG_MULTISCRBLT_INDEX] = False;
	settings->order_support[NEG_MULTIOPAQUERECT_INDEX] = True;
	settings->order_support[NEG_MULTI_DRAWNINEGRID_INDEX] = False;
	settings->order_support[NEG_LINETO_INDEX] = True;
	settings->order_support[NEG_POLYLINE_INDEX] = True;
	settings->order_support[NEG_MEMBLT_INDEX] = bitmap_cache;
	settings->order_support[NEG_MEM3BLT_INDEX] = False;
	settings->order_support[NEG_MEMBLT_V2_INDEX] = bitmap_cache;
	settings->order_support[NEG_MEM3BLT_V2_INDEX] = False;
	settings->order_support[NEG_SAVEBITMAP_INDEX] = True;
	settings->order_support[NEG_GLYPH_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_GLYPH_INDEX] = True;
	settings->order_support[NEG_POLYGON_SC_INDEX] = False;
	settings->order_support[NEG_POLYGON_CB_INDEX] = False;
	settings->order_support[NEG_ELLIPSE_SC_INDEX] = False;
	settings->order_support[NEG_ELLIPSE_CB_INDEX] = False;

	freerdp_channels_pre_connect(xfi->_context->channels, instance);

	xfi->display = XOpenDisplay(NULL);

	if (xfi->display == NULL)
	{
		printf("xf_pre_connect: failed to open display: %s\n", XDisplayName(NULL));
		return False;
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
	xfi->_NET_WM_WINDOW_TYPE_UTILITY = XInternAtom(xfi->display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
	xfi->_NET_WM_STATE_SKIP_TASKBAR = XInternAtom(xfi->display, "_NET_WM_STATE_SKIP_TASKBAR", False);
	xfi->_NET_WM_STATE_SKIP_PAGER = XInternAtom(xfi->display, "_NET_WM_STATE_SKIP_PAGER", False);

	xfi->_NET_WM_MOVERESIZE = XInternAtom(xfi->display, "_NET_WM_MOVERESIZE", False);

	xfi->WM_PROTOCOLS = XInternAtom(xfi->display, "WM_PROTOCOLS", False);
	xfi->WM_DELETE_WINDOW = XInternAtom(xfi->display, "WM_DELETE_WINDOW", False);

	xf_kbd_init(xfi);

	xfi->clrconv = xnew(CLRCONV);
	xfi->clrconv->palette = NULL;
	xfi->clrconv->alpha = 1;
	xfi->clrconv->invert = 0;
	xfi->clrconv->rgb555 = 0;

	instance->context->cache = cache_new(instance->settings);

	xfi->xfds = ConnectionNumber(xfi->display);
	xfi->screen_number = DefaultScreen(xfi->display);
	xfi->screen = ScreenOfDisplay(xfi->display, xfi->screen_number);
	xfi->depth = DefaultDepthOfScreen(xfi->screen);
	xfi->big_endian = (ImageByteOrder(xfi->display) == MSBFirst);

	xfi->mouse_motion = settings->mouse_motion;
	xfi->complex_regions = True;
	xfi->decorations = settings->decorations;
	xfi->remote_app = settings->remote_app;
	xfi->fullscreen = settings->fullscreen;
	xfi->grab_keyboard = settings->grab_keyboard;
	xfi->fullscreen_toggle = True;
	xfi->sw_gdi = settings->sw_gdi;

	xf_detect_monitors(xfi, settings);

	return True;
}

boolean xf_post_connect(freerdp* instance)
{
	xfInfo* xfi;
	XGCValues gcv;
	rdpCache* cache;
	rdpChannels* channels;

	xfi = ((xfContext*) instance->context)->xfi;
	cache = instance->context->cache;
	channels = xfi->_context->channels;

	if (xf_get_pixmap_info(xfi) != True)
		return False;

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
	}
	else
	{
		xfi->srcBpp = instance->settings->color_depth;
		xf_gdi_register_update_callbacks(instance->update);

		xfi->hdc = gdi_GetDC();
		xfi->hdc->bitsPerPixel = xfi->bpp;
		xfi->hdc->bytesPerPixel = xfi->bpp / 8;

		xfi->hdc->alpha = xfi->clrconv->alpha;
		xfi->hdc->invert = xfi->clrconv->invert;
		xfi->hdc->rgb555 = xfi->clrconv->rgb555;

		xfi->hdc->hwnd = (HGDI_WND) malloc(sizeof(GDI_WND));
		xfi->hdc->hwnd->invalid = gdi_CreateRectRgn(0, 0, 0, 0);
		xfi->hdc->hwnd->invalid->null = 1;

		xfi->hdc->hwnd->count = 32;
		xfi->hdc->hwnd->cinvalid = (HGDI_RGN) malloc(sizeof(GDI_RGN) * xfi->hdc->hwnd->count);
		xfi->hdc->hwnd->ninvalid = 0;

		xfi->primary_buffer = (uint8*) xzalloc(xfi->width * xfi->height * xfi->bpp);

		if (instance->settings->rfx_codec)
			xfi->rfx_context = (void*) rfx_context_new();

		if (instance->settings->ns_codec)
			xfi->nsc_context = (void*) nsc_context_new();
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
	cache->pointer->PointerSize = (cbPointerSize) xf_pointer_size;
	cache->pointer->PointerSet = (cbPointerSet) xf_pointer_set;
	cache->pointer->PointerNew = (cbPointerNew) xf_pointer_new;
	cache->pointer->PointerFree = (cbPointerFree) xf_pointer_free;

	if (xfi->sw_gdi != True)
	{
		bitmap_cache_register_callbacks(instance->update);
		cache->bitmap->BitmapSize = (cbBitmapSize) xf_bitmap_size;
		cache->bitmap->BitmapNew = (cbBitmapNew) xf_bitmap_new;
		cache->bitmap->BitmapFree = (cbBitmapFree) xf_bitmap_free;

		offscreen_cache_register_callbacks(instance->update);
		cache->offscreen->BitmapSize = (cbBitmapSize) xf_bitmap_size;
		cache->offscreen->BitmapNew = (cbBitmapNew) xf_offscreen_bitmap_new;
		cache->offscreen->BitmapFree = (cbBitmapFree) xf_bitmap_free;
		cache->offscreen->SetSurface = (cbSetSurface) xf_set_surface;
	}

	instance->context->rail = rail_new(instance->settings);
	rail_register_update_callbacks(instance->context->rail, instance->update);
	xf_rail_register_callbacks(xfi, instance->context->rail);

	freerdp_channels_post_connect(channels, instance);

	xf_tsmf_init(xfi, xv_port);

	if (xfi->remote_app != True)
		xf_cliprdr_init(xfi, channels);

	return True;
}

boolean xf_authenticate(freerdp* instance, char** username, char** password, char** domain)
{
	*password = xmalloc(password_size * sizeof(char));

	if (freerdp_passphrase_read("Password: ", *password, password_size) == NULL)
		return False;

	return True;
}

boolean xf_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint)
{
	printf("Certificate details:\n");
	printf("\tSubject: %s\n", subject);
	printf("\tIssuer: %s\n", issuer);
	printf("\tThumbprint: %s\n", fingerprint);
	printf("The above X.509 certificate could not be verified, possibly because you do not have "
		"the CA certificate in your certificate store, or the certificate has expired."
		"Please look at the documentation on how to create local certificate store for a private CA.\n");

	char answer;
	while (1)
	{
		printf("Do you trust the above certificate? (Y/N) ");
		answer = fgetc(stdin);

		if (answer == 'y' || answer == 'Y')
		{
			return True;
		}
		else if (answer == 'n' || answer == 'N')
		{
			break;
		}
	}

	return False;
}


int xf_process_client_args(rdpSettings* settings, const char* opt, const char* val, void* user_data)
{
	int argc = 0;

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

	xf_DestroyWindow(xfi, xfi->window);
	xfi->window = NULL;

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
		if (context->cache != NULL)
		{
			cache_free(context->cache);
			context->cache = NULL;
		}
		if (context->rail != NULL)
		{
			rail_free(context->rail);
			context->rail = NULL;
		}
	}

	if (xfi->rfx_context) 
	{
		rfx_context_free(xfi->rfx_context);
		xfi->rfx_context = NULL;
	}
	
	xfree(xfi->clrconv);

	xf_tsmf_uninit(xfi);
	xf_cliprdr_uninit(xfi);
}

void xf_free(xfInfo* xfi)
{
	xf_window_free(xfi);
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
	void* rfds[32];
	void* wfds[32];
	fd_set rfds_set;
	fd_set wfds_set;
	rdpChannels* channels;

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	if (!freerdp_connect(instance))
		return 0;

	xfi = ((xfContext*) instance->context)->xfi;
	channels = instance->context->channels;

	while (1)
	{
		rcount = 0;
		wcount = 0;

		if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get channel manager file descriptor\n");
			break;
		}
		if (xf_get_fds(instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get xfreerdp file descriptor\n");
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

		if (select(max_fds + 1, &rfds_set, &wfds_set, NULL, NULL) == -1)
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

		if (freerdp_check_fds(instance) != True)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (xf_check_fds(instance, &rfds_set) != True)
		{
			printf("Failed to check xfreerdp file descriptor\n");
			break;
		}
		if (freerdp_channels_check_fds(channels, instance) != True)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		xf_process_channel_event(channels, instance);
	}

	freerdp_channels_close(channels, instance);
	freerdp_channels_free(channels);
	freerdp_disconnect(instance);
	gdi_free(instance);
	freerdp_free(instance);
	xf_free(xfi);

	return 0;
}

void* thread_func(void* param)
{
	struct thread_data* data;
	data = (struct thread_data*) param;

	xfreerdp_run(data->instance);

	xfree(data);

	pthread_detach(pthread_self());

	g_thread_count--;

        if (g_thread_count < 1)
                freerdp_sem_signal(g_sem);

	return NULL;
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

	instance->ContextSize = (pcContextSize) xf_context_size;
	instance->ContextNew = (pcContextNew) xf_context_new;
	instance->ContextFree = (pcContextFree) xf_context_free;
	freerdp_context_new(instance);

	instance->context->argc = argc;
	instance->context->argv = argv;
	instance->settings->sw_gdi = False;

	data = (struct thread_data*) xzalloc(sizeof(struct thread_data));
	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
                freerdp_sem_wait(g_sem);
	}

	freerdp_channels_global_uninit();

	return 0;
}
