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

#include <sys/select.h>
#include <sys/signal.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

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

void* xf_frame_rate_thread(void* param)
{
	xfInfo* xfi;
	HGDI_RGN region;
	xfPeerContext* xfp;
	freerdp_peer* client;
	UINT32 wait_interval;

	client = (freerdp_peer*) param;
	xfp = (xfPeerContext*) client->context;
	xfi = xfp->info;

	region = xfp->hdc->hwnd->invalid;
	wait_interval = 1000000 / xfp->fps;

	while (1)
	{
		/* check if we should terminate */
		pthread_testcancel();

		if (!region->null)
		{
			UINT32 xy, wh;

			pthread_mutex_lock(&(xfp->mutex));

			xy = (region->x << 16) | region->y;
			wh = (region->w << 16) | region->h;
			region->null = 1;

			pthread_mutex_unlock(&(xfp->mutex));

			MessageQueue_Post(xfp->queue, (void*) xfp,
					MakeMessageId(PeerEvent, EncodeRegion),
					(void*) (size_t) xy, (void*) (size_t) wh);
		}

		USleep(wait_interval);
	}

	return NULL;
}

void* xf_monitor_updates(void* param)
{
	int fds;
	xfInfo* xfi;
	XEvent xevent;
	fd_set rfds_set;
	int select_status;
	xfPeerContext* xfp;
	freerdp_peer* client;
	UINT32 wait_interval;
	struct timeval timeout;
	int x, y, width, height;
	XDamageNotifyEvent* notify;

	client = (freerdp_peer*) param;
	xfp = (xfPeerContext*) client->context;
	xfi = xfp->info;

	fds = xfi->xfds;
	wait_interval = 1000000 / xfp->fps;
	ZeroMemory(&timeout, sizeof(struct timeval));

	pthread_create(&(xfp->frame_rate_thread), 0, xf_frame_rate_thread, (void*) client);

	while (1)
	{
		/* check if we should terminate */
		pthread_testcancel();

		FD_ZERO(&rfds_set);
		FD_SET(fds, &rfds_set);

		timeout.tv_sec = 0;
		timeout.tv_usec = wait_interval;
		select_status = select(fds + 1, &rfds_set, NULL, NULL, &timeout);

		if (select_status == -1)
		{
			fprintf(stderr, "select failed\n");
		}
		else if (select_status == 0)
		{

		}

		while (XPending(xfi->display) > 0)
		{
			ZeroMemory(&xevent, sizeof(xevent));
			XNextEvent(xfi->display, &xevent);

			if (xevent.type == xfi->xdamage_notify_event)
			{
				notify = (XDamageNotifyEvent*) &xevent;

				x = notify->area.x;
				y = notify->area.y;
				width = notify->area.width;
				height = notify->area.height;

				pthread_mutex_lock(&(xfp->mutex));
				gdi_InvalidateRegion(xfp->hdc, x, y, width, height);
				pthread_mutex_unlock(&(xfp->mutex));

				xf_xdamage_subtract_region(xfp, x, y, width, height);
			}
		}
	}

	return NULL;
}
