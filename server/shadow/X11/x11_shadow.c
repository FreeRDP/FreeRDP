/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2011-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/select.h>
#include <sys/signal.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>

#include "../shadow_surface.h"

#include "x11_shadow.h"

void x11_shadow_input_synchronize_event(x11ShadowSubsystem* subsystem, UINT32 flags)
{

}

void x11_shadow_input_keyboard_event(x11ShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{
#ifdef WITH_XTEST
	DWORD vkcode;
	DWORD keycode;
	BOOL extended = FALSE;

	if (flags & KBD_FLAGS_EXTENDED)
		extended = TRUE;

	if (extended)
		code |= KBDEXT;

	vkcode = GetVirtualKeyCodeFromVirtualScanCode(code, 4);
	keycode = GetKeycodeFromVirtualKeyCode(vkcode, KEYCODE_TYPE_EVDEV);

	if (keycode != 0)
	{
		XTestGrabControl(subsystem->display, True);

		if (flags & KBD_FLAGS_DOWN)
			XTestFakeKeyEvent(subsystem->display, keycode, True, 0);
		else if (flags & KBD_FLAGS_RELEASE)
			XTestFakeKeyEvent(subsystem->display, keycode, False, 0);

		XTestGrabControl(subsystem->display, False);
	}
#endif
}

void x11_shadow_input_unicode_keyboard_event(x11ShadowSubsystem* subsystem, UINT16 flags, UINT16 code)
{

}

void x11_shadow_input_mouse_event(x11ShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	int button = 0;
	BOOL down = FALSE;

	XTestGrabControl(subsystem->display, True);

	if (flags & PTR_FLAGS_WHEEL)
	{
		BOOL negative = FALSE;

		if (flags & PTR_FLAGS_WHEEL_NEGATIVE)
			negative = TRUE;

		button = (negative) ? 5 : 4;

		XTestFakeButtonEvent(subsystem->display, button, True, 0);
		XTestFakeButtonEvent(subsystem->display, button, False, 0);
	}
	else
	{
		if (flags & PTR_FLAGS_MOVE)
			XTestFakeMotionEvent(subsystem->display, 0, x, y, 0);

		if (flags & PTR_FLAGS_BUTTON1)
			button = 1;
		else if (flags & PTR_FLAGS_BUTTON2)
			button = 3;
		else if (flags & PTR_FLAGS_BUTTON3)
			button = 2;

		if (flags & PTR_FLAGS_DOWN)
			down = TRUE;

		if (button != 0)
			XTestFakeButtonEvent(subsystem->display, button, down, 0);
	}

	XTestGrabControl(subsystem->display, False);
#endif
}

void x11_shadow_input_extended_mouse_event(x11ShadowSubsystem* subsystem, UINT16 flags, UINT16 x, UINT16 y)
{
#ifdef WITH_XTEST
	int button = 0;
	BOOL down = FALSE;

	XTestGrabControl(subsystem->display, True);
	XTestFakeMotionEvent(subsystem->display, 0, x, y, CurrentTime);

	if (flags & PTR_XFLAGS_BUTTON1)
		button = 8;
	else if (flags & PTR_XFLAGS_BUTTON2)
		button = 9;

	if (flags & PTR_XFLAGS_DOWN)
		down = TRUE;

	if (button != 0)
		XTestFakeButtonEvent(subsystem->display, button, down, 0);

	XTestGrabControl(subsystem->display, False);
#endif
}

void x11_shadow_validate_region(x11ShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	XRectangle region;

	region.x = x;
	region.y = y;
	region.width = width;
	region.height = height;

#ifdef WITH_XFIXES
	XFixesSetRegion(subsystem->display, subsystem->xdamage_region, &region, 1);
	XDamageSubtract(subsystem->display, subsystem->xdamage, subsystem->xdamage_region, None);
#endif
}

int x11_shadow_invalidate_region(x11ShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 invalidRect;

	server = subsystem->server;
	surface = server->surface;

	invalidRect.left = x;
	invalidRect.top = y;
	invalidRect.right = x + width;
	invalidRect.bottom = y + height;

	printf("x11_shadow_invalidate_region: x: %d y: %d width: %d height: %d\n",
			x, y, width, height);

	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);

	return 1;
}

int x11_shadow_surface_copy(x11ShadowSubsystem* subsystem)
{
	int x, y;
	int width;
	int height;
	XImage* image;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	const RECTANGLE_16* extents;

	server = subsystem->server;
	surface = server->surface;

	if (region16_is_empty(&(surface->invalidRegion)))
		return 1;

	extents = region16_extents(&(surface->invalidRegion));

	x = extents->left;
	y = extents->top;
	width = extents->right - extents->left;
	height = extents->bottom - extents->top;

	printf("x11_shadow_surface_copy: x: %d y: %d width: %d height: %d\n",
			x, y, width, height);

	if (subsystem->use_xshm)
	{
		XCopyArea(subsystem->display, subsystem->root_window, subsystem->fb_pixmap,
				subsystem->xdamage_gc, x, y, width, height, x, y);

		image = subsystem->fb_image;

		freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32,
				surface->scanline, x, y, width, height,
				(BYTE*) image->data, PIXEL_FORMAT_XRGB32,
				image->bytes_per_line, x, y);
	}
	else
	{
		image = XGetImage(subsystem->display, subsystem->root_window,
						x, y, width, height, AllPlanes, ZPixmap);

		freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32,
				surface->scanline, x, y, width, height,
				(BYTE*) image->data, PIXEL_FORMAT_XRGB32,
				image->bytes_per_line, 0, 0);

		XDestroyImage(image);
	}

	x11_shadow_validate_region(subsystem, x, y, width, height);

	return 1;
}

int x11_shadow_check_event(x11ShadowSubsystem* subsystem)
{
	XEvent xevent;
	int x, y, width, height;
	XDamageNotifyEvent* notify;

	while (XPending(subsystem->display) > 0)
	{
		ZeroMemory(&xevent, sizeof(xevent));
		XNextEvent(subsystem->display, &xevent);

		if (xevent.type == subsystem->xdamage_notify_event)
		{
			notify = (XDamageNotifyEvent*) &xevent;

			x = notify->area.x;
			y = notify->area.y;
			width = notify->area.width;
			height = notify->area.height;

			x11_shadow_invalidate_region(subsystem, x, y, width, height);
		}
	}

	return 1;
}

int x11_shadow_cursor_init(x11ShadowSubsystem* subsystem)
{
#ifdef WITH_XFIXES
	int event;
	int error;

	if (!XFixesQueryExtension(subsystem->display, &event, &error))
	{
		fprintf(stderr, "XFixesQueryExtension failed\n");
		return -1;
	}

	subsystem->xfixes_notify_event = event + XFixesCursorNotify;

	XFixesSelectCursorInput(subsystem->display, DefaultRootWindow(subsystem->display), XFixesDisplayCursorNotifyMask);
#endif

	return 0;
}

#ifdef WITH_XDAMAGE

int x11_shadow_xdamage_init(x11ShadowSubsystem* subsystem)
{
	int damage_event;
	int damage_error;
	int major, minor;
	XGCValues values;

	if (!XDamageQueryExtension(subsystem->display, &damage_event, &damage_error))
		return -1;

	if (!XDamageQueryVersion(subsystem->display, &major, &minor))
		return -1;

	if (major < 1)
	{
		fprintf(stderr, "XDamageQueryVersion failed: major:%d minor:%d\n", major, minor);
		return -1;
	}

	subsystem->xdamage_notify_event = damage_event + XDamageNotify;
	subsystem->xdamage = XDamageCreate(subsystem->display, subsystem->root_window, XDamageReportDeltaRectangles);

	if (!subsystem->xdamage)
		return -1;

#ifdef WITH_XFIXES
	subsystem->xdamage_region = XFixesCreateRegion(subsystem->display, NULL, 0);

	if (!subsystem->xdamage_region)
	{
		fprintf(stderr, "XFixesCreateRegion failed\n");
		XDamageDestroy(subsystem->display, subsystem->xdamage);
		subsystem->xdamage = None;
		return -1;
	}
#endif

	values.subwindow_mode = IncludeInferiors;
	subsystem->xdamage_gc = XCreateGC(subsystem->display,
			subsystem->root_window, GCSubwindowMode, &values);
	XSetFunction(subsystem->display, subsystem->xdamage_gc, GXcopy);

	return 1;
}

#endif

int x11_shadow_xshm_init(x11ShadowSubsystem* server)
{
	Bool pixmaps;
	int major, minor;

	if (!XShmQueryExtension(server->display))
		return -1;

	if (!XShmQueryVersion(server->display, &major, &minor, &pixmaps))
		return -1;

	if (!pixmaps)
	{
		fprintf(stderr, "XShmQueryVersion failed\n");
		return -1;
	}

	server->fb_shm_info.shmid = -1;
	server->fb_shm_info.shmaddr = (char*) -1;

	server->fb_image = XShmCreateImage(server->display, server->visual, server->depth,
			ZPixmap, NULL, &(server->fb_shm_info), server->width, server->height);

	if (!server->fb_image)
	{
		fprintf(stderr, "XShmCreateImage failed\n");
		return -1;
	}

	server->fb_shm_info.shmid = shmget(IPC_PRIVATE,
			server->fb_image->bytes_per_line * server->fb_image->height, IPC_CREAT | 0600);

	if (server->fb_shm_info.shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		return -1;
	}

	server->fb_shm_info.readOnly = False;
	server->fb_shm_info.shmaddr = shmat(server->fb_shm_info.shmid, 0, 0);
	server->fb_image->data = server->fb_shm_info.shmaddr;

	if (server->fb_shm_info.shmaddr == ((char*) -1))
	{
		fprintf(stderr, "shmat failed\n");
		return -1;
	}

	if (!XShmAttach(server->display, &(server->fb_shm_info)))
		return -1;

	XSync(server->display, False);

	shmctl(server->fb_shm_info.shmid, IPC_RMID, 0);

	fprintf(stderr, "display: %p root_window: %p width: %d height: %d depth: %d\n",
			server->display, (void*) server->root_window, server->fb_image->width, server->fb_image->height, server->fb_image->depth);

	server->fb_pixmap = XShmCreatePixmap(server->display,
			server->root_window, server->fb_image->data, &(server->fb_shm_info),
			server->fb_image->width, server->fb_image->height, server->fb_image->depth);

	if (!server->fb_pixmap)
		return -1;

	return 1;
}

int x11_shadow_subsystem_init(x11ShadowSubsystem* subsystem)
{
	int i;
	int pf_count;
	int vi_count;
	XVisualInfo* vi;
	XVisualInfo* vis;
	XVisualInfo template;
	XPixmapFormatValues* pf;
	XPixmapFormatValues* pfs;

	/**
	 * Recent X11 servers drop support for shared pixmaps
	 * To see if your X11 server supports shared pixmaps, use:
	 * xdpyinfo -ext MIT-SHM | grep "shared pixmaps"
	 */
	subsystem->use_xshm = FALSE;

	if (!getenv("DISPLAY"))
	{
		/* Set DISPLAY variable if not already set */
		setenv("DISPLAY", ":0", 1);
	}

	if (!XInitThreads())
	{
		fprintf(stderr, "warning: XInitThreads() failure\n");
	}

	subsystem->display = XOpenDisplay(NULL);

	if (!subsystem->display)
	{
		fprintf(stderr, "failed to open display: %s\n", XDisplayName(NULL));
		return -1;
	}

	subsystem->xfds = ConnectionNumber(subsystem->display);
	subsystem->number = DefaultScreen(subsystem->display);
	subsystem->screen = ScreenOfDisplay(subsystem->display, subsystem->number);
	subsystem->depth = DefaultDepthOfScreen(subsystem->screen);
	subsystem->width = WidthOfScreen(subsystem->screen);
	subsystem->height = HeightOfScreen(subsystem->screen);
	subsystem->root_window = DefaultRootWindow(subsystem->display);

	pfs = XListPixmapFormats(subsystem->display, &pf_count);

	if (!pfs)
	{
		fprintf(stderr, "XListPixmapFormats failed\n");
		return -1;
	}

	for (i = 0; i < pf_count; i++)
	{
		pf = pfs + i;

		if (pf->depth == subsystem->depth)
		{
			subsystem->bpp = pf->bits_per_pixel;
			subsystem->scanline_pad = pf->scanline_pad;
			break;
		}
	}
	XFree(pfs);

	ZeroMemory(&template, sizeof(template));
	template.class = TrueColor;
	template.screen = subsystem->number;

	vis = XGetVisualInfo(subsystem->display, VisualClassMask | VisualScreenMask, &template, &vi_count);

	if (!vis)
	{
		fprintf(stderr, "XGetVisualInfo failed\n");
		return -1;
	}

	for (i = 0; i < vi_count; i++)
	{
		vi = vis + i;

		if (vi->depth == subsystem->depth)
		{
			subsystem->visual = vi->visual;
			break;
		}
	}
	XFree(vis);

	XSelectInput(subsystem->display, subsystem->root_window, SubstructureNotifyMask);

	if (subsystem->use_xshm)
	{
		if (x11_shadow_xshm_init(subsystem) < 0)
			subsystem->use_xshm = FALSE;
	}

	if (subsystem->use_xshm)
		printf("Using X Shared Memory Extension (XShm)\n");

#ifdef WITH_XDAMAGE
	x11_shadow_xdamage_init(subsystem);
#endif

	x11_shadow_cursor_init(subsystem);

	subsystem->event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, subsystem->xfds);

	subsystem->monitorCount = 1;
	subsystem->monitors[0].left = 0;
	subsystem->monitors[0].top = 0;
	subsystem->monitors[0].right = subsystem->width;
	subsystem->monitors[0].bottom = subsystem->height;
	subsystem->monitors[0].flags = 1;

	return 1;
}

int x11_shadow_subsystem_uninit(x11ShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	if (subsystem->display)
	{
		XCloseDisplay(subsystem->display);
		subsystem->display = NULL;
	}

	if (subsystem->event)
	{
		CloseHandle(subsystem->event);
		subsystem->event = NULL;
	}

	return 1;
}

rdpShadowSubsystem* x11_shadow_subsystem_new(rdpShadowServer* server)
{
	x11ShadowSubsystem* subsystem;

	subsystem = (x11ShadowSubsystem*) calloc(1, sizeof(x11ShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->server = server;

	x11_shadow_subsystem_init(subsystem);

	return (rdpShadowSubsystem*) subsystem;
}

void x11_shadow_subsystem_free(rdpShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	x11_shadow_subsystem_uninit((x11ShadowSubsystem*) subsystem);

	free(subsystem);
}

