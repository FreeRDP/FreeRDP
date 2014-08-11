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

#include "../shadow_screen.h"
#include "../shadow_capture.h"
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

	if (!subsystem->use_xfixes || !subsystem->use_xdamage)
		return;

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
	RECTANGLE_16 invalidRect;

	server = subsystem->server;

	invalidRect.left = x;
	invalidRect.top = y;
	invalidRect.right = x + width;
	invalidRect.bottom = y + height;

	region16_union_rect(&(subsystem->invalidRegion), &(subsystem->invalidRegion), &invalidRect);

	return 1;
}

int x11_shadow_screen_grab(x11ShadowSubsystem* subsystem)
{
	int count;
	int status;
	int x, y;
	int width, height;
	XImage* image;
	rdpShadowScreen* screen;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 invalidRect;

	server = subsystem->server;
	surface = server->surface;
	screen = server->screen;

	if (subsystem->use_xshm)
	{
		XLockDisplay(subsystem->display);

		XCopyArea(subsystem->display, subsystem->root_window, subsystem->fb_pixmap,
				subsystem->xshm_gc, 0, 0, subsystem->width, subsystem->height, 0, 0);

		XSync(subsystem->display, False);

		XUnlockDisplay(subsystem->display);

		image = subsystem->fb_image;

		status = shadow_capture_compare(surface->data, surface->scanline, surface->width, surface->height,
				(BYTE*) image->data, image->bytes_per_line, &invalidRect);

		if (status > 0)
		{
			x = invalidRect.left;
			y = invalidRect.top;
			width = invalidRect.right - invalidRect.left;
			height = invalidRect.bottom - invalidRect.top;

			freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32,
					surface->scanline, x - surface->x, y - surface->y, width, height,
					(BYTE*) image->data, PIXEL_FORMAT_XRGB32,
					image->bytes_per_line, x, y);

			region16_union_rect(&(subsystem->invalidRegion), &(subsystem->invalidRegion), &invalidRect);

			count = ArrayList_Count(server->clients);

			InitializeSynchronizationBarrier(&(subsystem->barrier), count + 1, -1);

			SetEvent(subsystem->updateEvent);

			EnterSynchronizationBarrier(&(subsystem->barrier), 0);

			DeleteSynchronizationBarrier(&(subsystem->barrier));

			ResetEvent(subsystem->updateEvent);

			region16_clear(&(subsystem->invalidRegion));
		}
	}
	else
	{
		XLockDisplay(subsystem->display);

		image = XGetImage(subsystem->display, subsystem->root_window,
				0, 0, subsystem->width, subsystem->height, AllPlanes, ZPixmap);

		XUnlockDisplay(subsystem->display);

		status = shadow_capture_compare(surface->data, surface->scanline, surface->width, surface->height,
				(BYTE*) image->data, image->bytes_per_line, &invalidRect);

		if (status > 0)
		{
			x = invalidRect.left;
			y = invalidRect.top;
			width = invalidRect.right - invalidRect.left;
			height = invalidRect.bottom - invalidRect.top;

			freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32,
					surface->scanline, x - surface->x, y - surface->y, width, height,
					(BYTE*) image->data, PIXEL_FORMAT_XRGB32,
					image->bytes_per_line, x, y);

			region16_union_rect(&(subsystem->invalidRegion), &(subsystem->invalidRegion), &invalidRect);

			count = ArrayList_Count(server->clients);

			InitializeSynchronizationBarrier(&(subsystem->barrier), count + 1, -1);

			SetEvent(subsystem->updateEvent);

			EnterSynchronizationBarrier(&(subsystem->barrier), 0);

			DeleteSynchronizationBarrier(&(subsystem->barrier));

			ResetEvent(subsystem->updateEvent);

			region16_clear(&(subsystem->invalidRegion));
		}

		XDestroyImage(image);
	}

	return 1;
}

void* x11_shadow_subsystem_thread(x11ShadowSubsystem* subsystem)
{
	int fps;
	DWORD status;
	DWORD nCount;
	UINT64 cTime;
	DWORD dwTimeout;
	DWORD dwInterval;
	UINT64 frameTime;
	HANDLE events[32];
	HANDLE StopEvent;

	StopEvent = subsystem->server->StopEvent;

	nCount = 0;
	events[nCount++] = StopEvent;
	events[nCount++] = subsystem->event;

	fps = 16;
	dwInterval = 1000 / fps;
	frameTime = GetTickCount64() + dwInterval;

	while (1)
	{
		cTime = GetTickCount64();
		dwTimeout = (cTime > frameTime) ? 0 : frameTime - cTime;

		status = WaitForMultipleObjects(nCount, events, FALSE, dwTimeout);

		if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
		{
			break;
		}

		if ((status == WAIT_TIMEOUT) || (GetTickCount64() > frameTime))
		{
			x11_shadow_screen_grab(subsystem);

			dwInterval = 1000 / fps;
			frameTime += dwInterval;
		}
	}

	ExitThread(0);
	return NULL;
}

int x11_shadow_xfixes_init(x11ShadowSubsystem* subsystem)
{
#ifdef WITH_XFIXES
	int xfixes_event;
	int xfixes_error;
	int major, minor;

	if (!XFixesQueryExtension(subsystem->display, &xfixes_event, &xfixes_error))
		return -1;

	if (!XFixesQueryVersion(subsystem->display, &major, &minor))
		return -1;

	subsystem->xfixes_notify_event = xfixes_event + XFixesCursorNotify;

	XFixesSelectCursorInput(subsystem->display, DefaultRootWindow(subsystem->display), XFixesDisplayCursorNotifyMask);

	return 1;
#else
	return -1;
#endif
}

int x11_shadow_xinerama_init(x11ShadowSubsystem* subsystem)
{
#ifdef WITH_XINERAMA
	int index;
	int numMonitors;
	int major, minor;
	int xinerama_event;
	int xinerama_error;
	MONITOR_DEF* monitor;
	XineramaScreenInfo* screen;
	XineramaScreenInfo* screens;

	if (!XineramaQueryExtension(subsystem->display, &xinerama_event, &xinerama_error))
		return -1;

	if (!XDamageQueryVersion(subsystem->display, &major, &minor))
		return -1;

	if (!XineramaIsActive(subsystem->display))
		return -1;

	screens = XineramaQueryScreens(subsystem->display, &numMonitors);

	if (numMonitors > 16)
		numMonitors = 16;

	if (!screens || (numMonitors < 1))
		return -1;

	subsystem->monitorCount = numMonitors;

	for (index = 0; index < numMonitors; index++)
	{
		screen = &screens[index];
		monitor = &(subsystem->monitors[index]);

		monitor->left = screen->x_org;
		monitor->top = screen->y_org;
		monitor->right = monitor->left + screen->width;
		monitor->bottom = monitor->top + screen->height;
		monitor->flags = (index == 0) ? 1 : 0;
	}

	XFree(screens);
#endif

	return 1;
}

int x11_shadow_xdamage_init(x11ShadowSubsystem* subsystem)
{
#ifdef WITH_XDAMAGE
	int major, minor;
	int damage_event;
	int damage_error;

	if (!subsystem->use_xfixes)
		return -1;

	if (!XDamageQueryExtension(subsystem->display, &damage_event, &damage_error))
		return -1;

	if (!XDamageQueryVersion(subsystem->display, &major, &minor))
		return -1;

	if (major < 1)
		return -1;

	subsystem->xdamage_notify_event = damage_event + XDamageNotify;
	subsystem->xdamage = XDamageCreate(subsystem->display, subsystem->root_window, XDamageReportDeltaRectangles);

	if (!subsystem->xdamage)
		return -1;

#ifdef WITH_XFIXES
	subsystem->xdamage_region = XFixesCreateRegion(subsystem->display, NULL, 0);

	if (!subsystem->xdamage_region)
		return -1;
#endif

	return 1;
#else
	return -1;
#endif
}

int x11_shadow_xshm_init(x11ShadowSubsystem* subsystem)
{
	Bool pixmaps;
	int major, minor;
	XGCValues values;

	if (!XShmQueryExtension(subsystem->display))
		return -1;

	if (!XShmQueryVersion(subsystem->display, &major, &minor, &pixmaps))
		return -1;

	if (!pixmaps)
		return -1;

	subsystem->fb_shm_info.shmid = -1;
	subsystem->fb_shm_info.shmaddr = (char*) -1;
	subsystem->fb_shm_info.readOnly = False;

	subsystem->fb_image = XShmCreateImage(subsystem->display, subsystem->visual, subsystem->depth,
			ZPixmap, NULL, &(subsystem->fb_shm_info), subsystem->width, subsystem->height);

	if (!subsystem->fb_image)
	{
		fprintf(stderr, "XShmCreateImage failed\n");
		return -1;
	}

	subsystem->fb_shm_info.shmid = shmget(IPC_PRIVATE,
			subsystem->fb_image->bytes_per_line * subsystem->fb_image->height, IPC_CREAT | 0600);

	if (subsystem->fb_shm_info.shmid == -1)
	{
		fprintf(stderr, "shmget failed\n");
		return -1;
	}

	subsystem->fb_shm_info.shmaddr = shmat(subsystem->fb_shm_info.shmid, 0, 0);
	subsystem->fb_image->data = subsystem->fb_shm_info.shmaddr;

	if (subsystem->fb_shm_info.shmaddr == ((char*) -1))
	{
		fprintf(stderr, "shmat failed\n");
		return -1;
	}

	if (!XShmAttach(subsystem->display, &(subsystem->fb_shm_info)))
		return -1;

	XSync(subsystem->display, False);

	shmctl(subsystem->fb_shm_info.shmid, IPC_RMID, 0);

	subsystem->fb_pixmap = XShmCreatePixmap(subsystem->display,
			subsystem->root_window, subsystem->fb_image->data, &(subsystem->fb_shm_info),
			subsystem->fb_image->width, subsystem->fb_image->height, subsystem->fb_image->depth);

	XSync(subsystem->display, False);

	if (!subsystem->fb_pixmap)
		return -1;

	values.subwindow_mode = IncludeInferiors;
	values.graphics_exposures = False;

	subsystem->xshm_gc = XCreateGC(subsystem->display, subsystem->root_window,
			GCSubwindowMode | GCGraphicsExposures, &values);

	XSetFunction(subsystem->display, subsystem->xshm_gc, GXcopy);
	XSync(subsystem->display, False);

	return 1;
}

int x11_shadow_subsystem_init(x11ShadowSubsystem* subsystem)
{
	int i;
	int pf_count;
	int vi_count;
	int nextensions;
	char** extensions;
	XVisualInfo* vi;
	XVisualInfo* vis;
	XVisualInfo template;
	XPixmapFormatValues* pf;
	XPixmapFormatValues* pfs;
	MONITOR_DEF* virtualScreen;

	/**
	 * To see if your X11 server supports shared pixmaps, use:
	 * xdpyinfo -ext MIT-SHM | grep "shared pixmaps"
	 */

	if (!getenv("DISPLAY"))
	{
		/* Set DISPLAY variable if not already set */
		setenv("DISPLAY", ":0", 1);
	}

	if (!XInitThreads())
		return -1;

	subsystem->display = XOpenDisplay(NULL);

	if (!subsystem->display)
	{
		fprintf(stderr, "failed to open display: %s\n", XDisplayName(NULL));
		return -1;
	}

	extensions = XListExtensions(subsystem->display, &nextensions);

	if (!extensions || (nextensions < 0))
		return -1;

	for (i = 0; i < nextensions; i++)
	{
		if (strcmp(extensions[i], "Composite") == 0)
			subsystem->composite = TRUE;
	}

	XFreeExtensionList(extensions);

	if (subsystem->composite)
		subsystem->use_xdamage = FALSE;

	if (!subsystem->use_xdamage)
		subsystem->use_xfixes = FALSE;

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

	if (subsystem->use_xfixes)
	{
		if (x11_shadow_xfixes_init(subsystem) < 0)
			subsystem->use_xfixes = FALSE;
	}

	if (subsystem->use_xinerama)
	{
		if (x11_shadow_xinerama_init(subsystem) < 0)
			subsystem->use_xinerama = FALSE;
	}

	if (subsystem->use_xshm)
	{
		if (x11_shadow_xshm_init(subsystem) < 0)
			subsystem->use_xshm = FALSE;
	}

	if (subsystem->use_xdamage)
	{
		if (x11_shadow_xdamage_init(subsystem) < 0)
			subsystem->use_xdamage = FALSE;
	}

	subsystem->event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, subsystem->xfds);

	virtualScreen = &(subsystem->virtualScreen);

	virtualScreen->left = 0;
	virtualScreen->top = 0;
	virtualScreen->right = subsystem->width;
	virtualScreen->bottom = subsystem->height;
	virtualScreen->flags = 1;

	if (subsystem->monitorCount < 1)
	{
		subsystem->monitorCount = 1;
		subsystem->monitors[0].left = virtualScreen->left;
		subsystem->monitors[0].top = virtualScreen->top;
		subsystem->monitors[0].right = virtualScreen->right;
		subsystem->monitors[0].bottom = virtualScreen->bottom;
		subsystem->monitors[0].flags = 1;
	}

	printf("X11 Extensions: XFixes: %d Xinerama: %d XDamage: %d XShm: %d\n",
			subsystem->use_xfixes, subsystem->use_xinerama, subsystem->use_xdamage, subsystem->use_xshm);

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

int x11_shadow_subsystem_start(x11ShadowSubsystem* subsystem)
{
	HANDLE thread;

	if (!subsystem)
		return -1;

	thread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) x11_shadow_subsystem_thread,
			(void*) subsystem, 0, NULL);

	return 1;
}

int x11_shadow_subsystem_stop(x11ShadowSubsystem* subsystem)
{
	if (!subsystem)
		return -1;

	return 1;
}

void x11_shadow_subsystem_free(x11ShadowSubsystem* subsystem)
{
	if (!subsystem)
		return;

	x11_shadow_subsystem_uninit(subsystem);

	region16_uninit(&(subsystem->invalidRegion));

	CloseHandle(subsystem->updateEvent);

	free(subsystem);
}

x11ShadowSubsystem* x11_shadow_subsystem_new(rdpShadowServer* server)
{
	x11ShadowSubsystem* subsystem;

	subsystem = (x11ShadowSubsystem*) calloc(1, sizeof(x11ShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->server = server;

	subsystem->updateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	region16_init(&(subsystem->invalidRegion));

	subsystem->Init = (pfnShadowSubsystemInit) x11_shadow_subsystem_init;
	subsystem->Uninit = (pfnShadowSubsystemInit) x11_shadow_subsystem_uninit;
	subsystem->Start = (pfnShadowSubsystemStart) x11_shadow_subsystem_start;
	subsystem->Stop = (pfnShadowSubsystemStop) x11_shadow_subsystem_stop;
	subsystem->Free = (pfnShadowSubsystemFree) x11_shadow_subsystem_free;

	subsystem->SynchronizeEvent = (pfnShadowSynchronizeEvent) x11_shadow_input_synchronize_event;
	subsystem->KeyboardEvent = (pfnShadowKeyboardEvent) x11_shadow_input_keyboard_event;
	subsystem->UnicodeKeyboardEvent = (pfnShadowUnicodeKeyboardEvent) x11_shadow_input_unicode_keyboard_event;
	subsystem->MouseEvent = (pfnShadowMouseEvent) x11_shadow_input_mouse_event;
	subsystem->ExtendedMouseEvent = (pfnShadowExtendedMouseEvent) x11_shadow_input_extended_mouse_event;

	subsystem->composite = FALSE;
	subsystem->use_xshm = TRUE;
	subsystem->use_xfixes = TRUE;
	subsystem->use_xdamage = FALSE;
	subsystem->use_xinerama = TRUE;

	return subsystem;
}

rdpShadowSubsystem* X11_ShadowCreateSubsystem(rdpShadowServer* server)
{
	return (rdpShadowSubsystem*) x11_shadow_subsystem_new(server);
}
