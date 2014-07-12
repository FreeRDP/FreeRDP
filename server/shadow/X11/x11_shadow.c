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

#include "x11_shadow.h"

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

void x11_shadow_xdamage_init(x11ShadowSubsystem* subsystem)
{
	int damage_event;
	int damage_error;
	int major, minor;
	XGCValues values;

	if (XDamageQueryExtension(subsystem->display, &damage_event, &damage_error) == 0)
	{
		fprintf(stderr, "XDamageQueryExtension failed\n");
		return;
	}

	XDamageQueryVersion(subsystem->display, &major, &minor);

	if (XDamageQueryVersion(subsystem->display, &major, &minor) == 0)
	{
		fprintf(stderr, "XDamageQueryVersion failed\n");
		return;
	}
	else if (major < 1)
	{
		fprintf(stderr, "XDamageQueryVersion failed: major:%d minor:%d\n", major, minor);
		return;
	}

	subsystem->xdamage_notify_event = damage_event + XDamageNotify;
	subsystem->xdamage = XDamageCreate(subsystem->display, subsystem->root_window, XDamageReportDeltaRectangles);

	if (subsystem->xdamage == None)
	{
		fprintf(stderr, "XDamageCreate failed\n");
		return;
	}

#ifdef WITH_XFIXES
	subsystem->xdamage_region = XFixesCreateRegion(subsystem->display, NULL, 0);

	if (subsystem->xdamage_region == None)
	{
		fprintf(stderr, "XFixesCreateRegion failed\n");
		XDamageDestroy(subsystem->display, subsystem->xdamage);
		subsystem->xdamage = None;
		return;
	}
#endif

	values.subwindow_mode = IncludeInferiors;
	subsystem->xdamage_gc = XCreateGC(subsystem->display, subsystem->root_window, GCSubwindowMode, &values);
	XSetFunction(subsystem->display, subsystem->xdamage_gc, GXcopy);
}

#endif

int x11_shadow_xshm_init(x11ShadowSubsystem* server)
{
	Bool pixmaps;
	int major, minor;

	if (XShmQueryExtension(server->display) != False)
	{
		XShmQueryVersion(server->display, &major, &minor, &pixmaps);

		if (pixmaps != True)
		{
			fprintf(stderr, "XShmQueryVersion failed\n");
			return -1;
		}
	}
	else
	{
		fprintf(stderr, "XShmQueryExtension failed\n");
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

	XShmAttach(server->display, &(server->fb_shm_info));
	XSync(server->display, False);

	shmctl(server->fb_shm_info.shmid, IPC_RMID, 0);

	fprintf(stderr, "display: %p root_window: %p width: %d height: %d depth: %d\n",
			server->display, (void*) server->root_window, server->fb_image->width, server->fb_image->height, server->fb_image->depth);

	server->fb_pixmap = XShmCreatePixmap(server->display,
			server->root_window, server->fb_image->data, &(server->fb_shm_info),
			server->fb_image->width, server->fb_image->height, server->fb_image->depth);

	return 0;
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
	subsystem->use_xshm = TRUE;

	setenv("DISPLAY", ":0", 1); /* Set DISPLAY variable if not already set */

	if (!XInitThreads())
		fprintf(stderr, "warning: XInitThreads() failure\n");

	subsystem->display = XOpenDisplay(NULL);

	if (!subsystem->display)
	{
		fprintf(stderr, "failed to open display: %s\n", XDisplayName(NULL));
		exit(1);
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
		exit(1);
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
		exit(1);
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

