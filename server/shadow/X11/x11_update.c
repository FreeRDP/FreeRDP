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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/codec/color.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "../shadow_surface.h"

#include "x11_shadow.h"

XImage* x11_shadow_snapshot(x11ShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	XImage* image;

	if (subsystem->use_xshm)
	{
		XCopyArea(subsystem->display, subsystem->root_window, subsystem->fb_pixmap,
				subsystem->xdamage_gc, x, y, width, height, x, y);
		image = subsystem->fb_image;
	}
	else
	{
		image = XGetImage(subsystem->display, subsystem->root_window,
				x, y, width, height, AllPlanes, ZPixmap);
	}

	return image;
}

void x11_shadow_xdamage_subtract_region(x11ShadowSubsystem* subsystem, int x, int y, int width, int height)
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

int x11_shadow_surface_copy(x11ShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	XImage* image;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	RECTANGLE_16 invalidRect;

	server = subsystem->server;
	surface = server->surface;

	printf("x11_shadow_surface_copy: x: %d y: %d width: %d height: %d\n",
			x, y, width, height);

	if (subsystem->use_xshm)
	{
		image = x11_shadow_snapshot(subsystem, x, y, width, height);

		freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32,
				surface->scanline, x, y, width, height,
				(BYTE*) image->data, PIXEL_FORMAT_XRGB32,
				image->bytes_per_line, x, y);
	}
	else
	{
		image = x11_shadow_snapshot(subsystem, x, y, width, height);

		freerdp_image_copy(surface->data, PIXEL_FORMAT_XRGB32,
				surface->scanline, x, y, width, height,
				(BYTE*) image->data, PIXEL_FORMAT_XRGB32,
				image->bytes_per_line, x, y);

		XDestroyImage(image);
	}

	invalidRect.left = x;
	invalidRect.top = y;
	invalidRect.right = width;
	invalidRect.bottom = height;

	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);

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

			if (x11_shadow_surface_copy(subsystem, x, y, width, height) > 0)
			{
				x11_shadow_xdamage_subtract_region(subsystem, x, y, width, height);
			}
		}
	}

	return 1;
}
