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

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "x11_shadow.h"

XImage* x11_shadow_snapshot(x11ShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	XImage* image;

	if (subsystem->use_xshm)
	{
		XCopyArea(subsystem->display, subsystem->root_window, subsystem->fb_pixmap, subsystem->xdamage_gc, x, y, width, height, x, y);
		image = subsystem->fb_image;
	}
	else
	{
		image = XGetImage(subsystem->display, subsystem->root_window, x, y, width, height, AllPlanes, ZPixmap);
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

int x11_shadow_update_encode(x11ShadowSubsystem* subsystem, int x, int y, int width, int height)
{
	BYTE* data;
	XImage* image;
	RFX_RECT rect;

	if (subsystem->use_xshm)
	{
		/**
		 * Passing an offset source rectangle to rfx_compose_message()
		 * leads to protocol errors, so offset the data pointer instead.
		 */

		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;

		image = x11_shadow_snapshot(subsystem, x, y, width, height);

		data = (BYTE*) image->data;
		data = &data[(y * image->bytes_per_line) + (x * image->bits_per_pixel / 8)];
	}
	else
	{
		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;

		image = x11_shadow_snapshot(subsystem, x, y, width, height);

		data = (BYTE*) image->data;

		XDestroyImage(image);
	}

	return 0;
}

void* x11_shadow_update_thread(x11ShadowSubsystem* subsystem)
{
	HANDLE event;
	XEvent xevent;
	DWORD beg, end;
	DWORD diff, rate;
	XFixesCursorImage* ci;
	int x, y, width, height;
	XDamageNotifyEvent* notify;

	rate = 1000 / 10;

	event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, subsystem->xfds);

	while (WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0)
	{
		beg = GetTickCount();

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

				if (x11_shadow_update_encode(subsystem, x, y, width, height) >= 0)
				{
					x11_shadow_xdamage_subtract_region(subsystem, x, y, width, height);

					/* send update */
				}
			}
#ifdef WITH_XFIXES
			else if (xevent.type == subsystem->xfixes_notify_event)
			{
				ci = XFixesGetCursorImage(subsystem->display);
				XFree(ci);
			}
#endif
		}

		end = GetTickCount();
		diff = end - beg;

		if (diff < rate)
			Sleep(rate - diff);
	}

	return NULL;
}
