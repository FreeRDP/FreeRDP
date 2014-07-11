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

XImage* x11_shadow_snapshot(x11ShadowClient* context, int x, int y, int width, int height)
{
	XImage* image;
	x11ShadowServer* server = context->server;

	if (server->use_xshm)
	{
		XCopyArea(server->display, server->root_window, server->fb_pixmap, server->xdamage_gc, x, y, width, height, x, y);
		image = server->fb_image;
	}
	else
	{
		image = XGetImage(server->display, server->root_window, x, y, width, height, AllPlanes, ZPixmap);
	}

	return image;
}

void x11_shadow_xdamage_subtract_region(x11ShadowClient* context, int x, int y, int width, int height)
{
	XRectangle region;
	x11ShadowServer* server = context->server;

	region.x = x;
	region.y = y;
	region.width = width;
	region.height = height;

#ifdef WITH_XFIXES
	XFixesSetRegion(server->display, server->xdamage_region, &region, 1);
	XDamageSubtract(server->display, server->xdamage, server->xdamage_region, None);
#endif
}

int x11_shadow_update_encode(freerdp_peer* client, int x, int y, int width, int height)
{
	wStream* s;
	BYTE* data;
	RFX_RECT rect;
	XImage* image;
	rdpUpdate* update;
	x11ShadowClient* context;
	x11ShadowServer* server;
	SURFACE_BITS_COMMAND* cmd;

	context = (x11ShadowClient*) client->context;
	server = context->server;

	update = client->update;
	cmd = &update->surface_bits_command;

	if (width * height <= 0)
	{
		cmd->bitmapDataLength = 0;
		return -1;
	}

	s = context->s;
	Stream_Clear(s);
	Stream_SetPosition(s, 0);

	if (server->use_xshm)
	{
		/**
		 * Passing an offset source rectangle to rfx_compose_message()
		 * leads to protocol errors, so offset the data pointer instead.
		 */

		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;

		image = x11_shadow_snapshot(context, x, y, width, height);

		data = (BYTE*) image->data;
		data = &data[(y * image->bytes_per_line) + (x * image->bits_per_pixel / 8)];

		rfx_compose_message(context->rfx_context, s, &rect, 1, data,
				width, height, image->bytes_per_line);

		cmd->destLeft = x;
		cmd->destTop = y;
		cmd->destRight = x + width;
		cmd->destBottom = y + height;
	}
	else
	{
		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;

		image = x11_shadow_snapshot(context, x, y, width, height);

		data = (BYTE*) image->data;

		rfx_compose_message(context->rfx_context, s, &rect, 1, data,
				width, height, image->bytes_per_line);

		cmd->destLeft = x;
		cmd->destTop = y;
		cmd->destRight = x + width;
		cmd->destBottom = y + height;

		XDestroyImage(image);
	}

	cmd->bpp = 32;
	cmd->codecID = client->settings->RemoteFxCodecId;
	cmd->width = width;
	cmd->height = height;
	cmd->bitmapDataLength = Stream_GetPosition(s);
	cmd->bitmapData = Stream_Buffer(s);

	return 0;
}

void x11_shadow_client_send_update(freerdp_peer* client)
{
	rdpUpdate* update;
	SURFACE_BITS_COMMAND* cmd;

	update = client->update;
	cmd = &update->surface_bits_command;

	if (cmd->bitmapDataLength)
		update->SurfaceBits(update->context, cmd);
}

void* x11_shadow_update_thread(void* param)
{
	HANDLE event;
	XEvent xevent;
	DWORD beg, end;
	DWORD diff, rate;
	x11ShadowClient* context;
	x11ShadowServer* server;
	freerdp_peer* client;
	int x, y, width, height;
	XDamageNotifyEvent* notify;

	client = (freerdp_peer*) param;
	context = (x11ShadowClient*) client->context;
	server = context->server;

	rate = 1000 / 10;

	event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, server->xfds);

	while (WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0)
	{
		beg = GetTickCount();

		while (XPending(server->display) > 0)
		{
			ZeroMemory(&xevent, sizeof(xevent));
			XNextEvent(server->display, &xevent);

			if (xevent.type == server->xdamage_notify_event)
			{
				notify = (XDamageNotifyEvent*) &xevent;

				x = notify->area.x;
				y = notify->area.y;
				width = notify->area.width;
				height = notify->area.height;

				if (x11_shadow_update_encode(client, x, y, width, height) >= 0)
				{
					x11_shadow_xdamage_subtract_region(context, x, y, width, height);

					if (context->activated)
						x11_shadow_client_send_update(client);
				}
			}
#ifdef WITH_XFIXES
			else if (xevent.type == server->xfixes_notify_event)
			{
				XFixesCursorImage* ci = XFixesGetCursorImage(server->display);
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
