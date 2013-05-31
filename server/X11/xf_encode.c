/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 RemoteFX Encoder
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <sys/select.h>
#include <sys/signal.h>

#include <winpr/crt.h>

#include "xf_encode.h"

XImage* xf_snapshot(xfPeerContext* xfp, int x, int y, int width, int height)
{
	XImage* image;
	xfInfo* xfi = xfp->info;

	if (xfi->use_xshm)
	{
		XCopyArea(xfi->display, xfi->root_window, xfi->fb_pixmap, xfi->xdamage_gc, x, y, width, height, x, y);
		image = xfi->fb_image;
	}
	else
	{
		image = XGetImage(xfi->display, xfi->root_window, x, y, width, height, AllPlanes, ZPixmap);
	}

	return image;
}

void xf_xdamage_subtract_region(xfPeerContext* xfp, int x, int y, int width, int height)
{
	XRectangle region;
	xfInfo* xfi = xfp->info;

	region.x = x;
	region.y = y;
	region.width = width;
	region.height = height;

#ifdef WITH_XFIXES
	XFixesSetRegion(xfi->display, xfi->xdamage_region, &region, 1);
	XDamageSubtract(xfi->display, xfi->xdamage, xfi->xdamage_region, None);
#endif
}

int xf_update_encode(freerdp_peer* client, int x, int y, int width, int height)
{
	wStream* s;
	BYTE* data;
	xfInfo* xfi;
	RFX_RECT rect;
	XImage* image;
	rdpUpdate* update;
	xfPeerContext* xfp;
	SURFACE_BITS_COMMAND* cmd;

	update = client->update;
	xfp = (xfPeerContext*) client->context;
	cmd = &update->surface_bits_command;
	xfi = xfp->info;

	if (width * height <= 0)
	{
		cmd->bitmapDataLength = 0;
		return -1;
	}

	s = xfp->s;
	Stream_Clear(s);
	Stream_SetPosition(s, 0);

	if (xfi->use_xshm)
	{
		/**
		 * Passing an offset source rectangle to rfx_compose_message()
		 * leads to protocol errors, so offset the data pointer instead.
		 */

		rect.x = 0;
		rect.y = 0;
		rect.width = width;
		rect.height = height;

		image = xf_snapshot(xfp, x, y, width, height);

		data = (BYTE*) image->data;
		data = &data[(y * image->bytes_per_line) + (x * image->bits_per_pixel / 8)];

		rfx_compose_message(xfp->rfx_context, s, &rect, 1, data,
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

		image = xf_snapshot(xfp, x, y, width, height);

		data = (BYTE*) image->data;

		rfx_compose_message(xfp->rfx_context, s, &rect, 1, data,
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
