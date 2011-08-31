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

#ifdef WITH_XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <locale.h>
#include <sys/select.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/event.h>
#include <freerdp/constants.h>
#include <freerdp/plugins/cliprdr.h>
#include <freerdp/rail.h>

#include "xf_gdi.h"
#include "xf_rail.h"
#include "xf_event.h"
#include "xf_monitor.h"
#include "xf_keyboard.h"

#include "xfreerdp.h"

freerdp_sem g_sem;
static int g_thread_count = 0;

struct thread_data
{
	freerdp* instance;
};

void xf_begin_paint(rdpUpdate* update)
{
	GDI* gdi;
	gdi = GET_GDI(update);
	gdi->primary->hdc->hwnd->invalid->null = 1;
	gdi->primary->hdc->hwnd->ninvalid = 0;
}

void xf_end_paint(rdpUpdate* update)
{
	GDI* gdi;
	xfInfo* xfi;
	sint32 x, y;
	uint32 w, h;

	gdi = GET_GDI(update);
	xfi = GET_XFI(update);

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

		xf_rail_paint(xfi, update->rail, x, y, x + w - 1, y + h - 1);
	}
}

boolean xf_get_fds(freerdp* instance, void** rfds, int* rcount, void** wfds, int* wcount)
{
	xfInfo* xfi = GET_XFI(instance);

	rfds[*rcount] = (void*)(long)(xfi->xfds);
	(*rcount)++;

	return True;
}

boolean xf_check_fds(freerdp* instance, fd_set* set)
{
	XEvent xevent;
	xfInfo* xfi = GET_XFI(instance);

	while (XPending(xfi->display))
	{
		memset(&xevent, 0, sizeof(xevent));
		XNextEvent(xfi->display, &xevent);

		if (xf_event_process(instance, &xevent) != True)
			return False;
	}

	return True;
}

void xf_toggle_fullscreen(xfInfo* xfi)
{
	Pixmap contents = 0;

	contents = XCreatePixmap(xfi->display, xfi->window->handle, xfi->width, xfi->height, xfi->depth);
	XCopyArea(xfi->display, xfi->primary, contents, xfi->gc, 0, 0, xfi->width, xfi->height, 0, 0);

	xfi->fullscreen = (xfi->fullscreen) ? False : True;

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
	rdpSettings* settings;

	xfi = (xfInfo*) xzalloc(sizeof(xfInfo));
	SET_XFI(instance, xfi);

	xfi->instance = instance;

	settings = instance->settings;

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
	settings->order_support[NEG_MEMBLT_INDEX] = True;
	settings->order_support[NEG_MEM3BLT_INDEX] = False;
	settings->order_support[NEG_SAVEBITMAP_INDEX] = True;
	settings->order_support[NEG_GLYPH_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_INDEX_INDEX] = True;
	settings->order_support[NEG_FAST_GLYPH_INDEX] = True;
	settings->order_support[NEG_POLYGON_SC_INDEX] = False;
	settings->order_support[NEG_POLYGON_CB_INDEX] = False;
	settings->order_support[NEG_ELLIPSE_SC_INDEX] = False;
	settings->order_support[NEG_ELLIPSE_CB_INDEX] = False;

	freerdp_chanman_pre_connect(GET_CHANMAN(instance), instance);

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

	xf_kbd_init(xfi);

	xfi->clrconv = (HCLRCONV) malloc(sizeof(CLRCONV));
	xfi->clrconv->palette = NULL;
	xfi->clrconv->alpha = 1;
	xfi->clrconv->invert = 0;
	xfi->clrconv->rgb555 = 0;

	xfi->cache = cache_new(instance->settings);

	xfi->xfds = ConnectionNumber(xfi->display);
	xfi->screen_number = DefaultScreen(xfi->display);
	xfi->screen = ScreenOfDisplay(xfi->display, xfi->screen_number);
	xfi->depth = DefaultDepthOfScreen(xfi->screen);
	xfi->big_endian = (ImageByteOrder(xfi->display) == MSBFirst);

	xfi->mouse_motion = True;
	xfi->complex_regions = True;
	xfi->decoration = settings->decorations;
	xfi->remote_app = settings->remote_app;
	xfi->fullscreen = settings->fullscreen;

	xf_detect_monitors(xfi, settings);

	return True;
}

boolean xf_post_connect(freerdp* instance)
{
	GDI* gdi;
	xfInfo* xfi;
	XEvent xevent;
	XGCValues gcv;
	Atom kill_atom;
	Atom protocol_atom;

	xfi = GET_XFI(instance);
	SET_XFI(instance->update, xfi);

	if (xf_get_pixmap_info(xfi) != True)
		return False;

	gdi_init(instance, CLRCONV_ALPHA | CLRBUF_32BPP);
	gdi = GET_GDI(instance->update);

	if (instance->settings->sw_gdi != True)
	{
		xfi->srcBpp = instance->settings->color_depth;
		xf_gdi_register_update_callbacks(instance->update);
	}

	if (xfi->fullscreen)
		xfi->decoration = False;

	xfi->width = xfi->fullscreen ? WidthOfScreen(xfi->screen) : instance->settings->width;
	xfi->height = xfi->fullscreen ? HeightOfScreen(xfi->screen) : instance->settings->height;

	xfi->attribs.background_pixel = BlackPixelOfScreen(xfi->screen);
	xfi->attribs.border_pixel = WhitePixelOfScreen(xfi->screen);
	xfi->attribs.backing_store = xfi->primary ? NotUseful : Always;
	xfi->attribs.override_redirect = xfi->fullscreen;
	xfi->attribs.colormap = xfi->colormap;

	if (xfi->remote_app != True)
	{
		xfi->window = xf_CreateDesktopWindow(xfi, "xfreerdp", xfi->width, xfi->height);

		xf_SetWindowDecorations(xfi, xfi->window, xfi->decoration);

		if (xfi->fullscreen)
			xf_SetWindowFullscreen(xfi, xfi->window, xfi->fullscreen);

		/* wait for VisibilityNotify */
		do
		{
			XMaskEvent(xfi->display, VisibilityChangeMask, &xevent);
		}
		while (xevent.type != VisibilityNotify);

		xfi->unobscured = (xevent.xvisibility.state == VisibilityUnobscured);

		protocol_atom = XInternAtom(xfi->display, "WM_PROTOCOLS", True);
		kill_atom = XInternAtom(xfi->display, "WM_DELETE_WINDOW", True);
		XSetWMProtocols(xfi->display, xfi->window->handle, &kill_atom, 1);
	}

	memset(&gcv, 0, sizeof(gcv));
	xfi->modifier_map = XGetModifierMapping(xfi->display);

	xfi->gc = XCreateGC(xfi->display, DefaultRootWindow(xfi->display), GCGraphicsExposures, &gcv);
	xfi->primary = XCreatePixmap(xfi->display, DefaultRootWindow(xfi->display), xfi->width, xfi->height, xfi->depth);
	xfi->drawing = xfi->primary;

	XSetForeground(xfi->display, xfi->gc, BlackPixelOfScreen(xfi->screen));
	XFillRectangle(xfi->display, xfi->primary, xfi->gc, 0, 0, xfi->width, xfi->height);

	xfi->image = XCreateImage(xfi->display, xfi->visual, xfi->depth, ZPixmap, 0,
			(char*) gdi->primary_buffer, gdi->width, gdi->height, xfi->scanline_pad, 0);

	instance->update->BeginPaint = xf_begin_paint;
	instance->update->EndPaint = xf_end_paint;

	xfi->rail = rail_new(instance->settings);
	instance->update->rail = (void*) xfi->rail;
	rail_register_update_callbacks(xfi->rail, instance->update);
	xf_rail_register_callbacks(xfi, xfi->rail);

	freerdp_chanman_post_connect(GET_CHANMAN(instance), instance);

	return True;
}

int xf_process_ui_args(rdpSettings* settings, const char* opt, const char* val, void* user_data)
{
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

	return 1;
}

int xf_process_plugin_args(rdpSettings* settings, const char* name, RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	rdpChanMan* chanman = (rdpChanMan*) user_data;

	printf("Load plugin %s\n", name);
	freerdp_chanman_load_plugin(chanman, settings, name, plugin_data);

	return 1;
}

int xf_receive_channel_data(freerdp* instance, int channelId, uint8* data, int size, int flags, int total_size)
{
	return freerdp_chanman_data(instance, channelId, data, size, flags, total_size);
}

void xf_process_cb_sync_event(rdpChanMan* chanman, freerdp* instance)
{
	RDP_EVENT* event;
	RDP_CB_FORMAT_LIST_EVENT* format_list_event;

	event = freerdp_event_new(RDP_EVENT_CLASS_CLIPRDR, RDP_EVENT_TYPE_CB_FORMAT_LIST, NULL, NULL);

	format_list_event = (RDP_CB_FORMAT_LIST_EVENT*) event;
	format_list_event->num_formats = 0;

	freerdp_chanman_send_event(chanman, event);
}

void xf_process_channel_event(rdpChanMan* chanman, freerdp* instance)
{
	xfInfo* xfi;
	RDP_EVENT* event;

	xfi = GET_XFI(instance);

	event = freerdp_chanman_pop_event(chanman);

	if (event)
	{
		switch (event->event_class)
		{
			case RDP_EVENT_CLASS_RAIL:
				xf_process_rail_event(xfi, chanman, event);
				break;

			default:
				break;
		}

		switch (event->event_type)
		{
			case RDP_EVENT_TYPE_CB_SYNC:
				xf_process_cb_sync_event(chanman, instance);
				break;
			default:
				break;
		}
		freerdp_event_free(event);
	}
}

void xf_window_free(xfInfo* xfi)
{
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
	rdpChanMan* chanman;

	memset(rfds, 0, sizeof(rfds));
	memset(wfds, 0, sizeof(wfds));

	if (!instance->Connect(instance))
		return 0;

	xfi = GET_XFI(instance);
	chanman = GET_CHANMAN(instance);

	while (1)
	{
		rcount = 0;
		wcount = 0;

		if (instance->GetFileDescriptor(instance, rfds, &rcount, wfds, &wcount) != True)
		{
			printf("Failed to get FreeRDP file descriptor\n");
			break;
		}
		if (freerdp_chanman_get_fds(chanman, instance, rfds, &rcount, wfds, &wcount) != True)
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

		if (instance->CheckFileDescriptor(instance) != True)
		{
			printf("Failed to check FreeRDP file descriptor\n");
			break;
		}
		if (xf_check_fds(instance, &rfds_set) != True)
		{
			printf("Failed to check xfreerdp file descriptor\n");
			break;
		}
		if (freerdp_chanman_check_fds(chanman, instance) != True)
		{
			printf("Failed to check channel manager file descriptor\n");
			break;
		}
		xf_process_channel_event(chanman, instance);
	}

	freerdp_chanman_close(chanman, instance);
	freerdp_chanman_free(chanman);
	instance->Disconnect(instance);
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
	rdpChanMan* chanman;

	setlocale(LC_ALL, "");

	freerdp_chanman_global_init();

	g_sem = freerdp_sem_new(1);

	instance = freerdp_new();
	instance->PreConnect = xf_pre_connect;
	instance->PostConnect = xf_post_connect;
	instance->ReceiveChannelData = xf_receive_channel_data;

	chanman = freerdp_chanman_new();
	SET_CHANMAN(instance, chanman);

	instance->settings->sw_gdi = True;

	if (freerdp_parse_args(instance->settings, argc, argv,
			xf_process_plugin_args, chanman, xf_process_ui_args, NULL) < 0)
		return 1;

	data = (struct thread_data*) xzalloc(sizeof(struct thread_data));
	data->instance = instance;

	g_thread_count++;
	pthread_create(&thread, 0, thread_func, data);

	while (g_thread_count > 0)
	{
                freerdp_sem_wait(g_sem);
	}

	freerdp_chanman_global_uninit();

	return 0;
}
