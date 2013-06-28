/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Server Graphical Updates
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include "xf_peer.h"
#include "xf_encode.h"

#include "xf_update.h"

void* xf_update_thread(void* param)
{
	xfInfo* xfi;
	HANDLE event;
	XEvent xevent;
	DWORD beg, end;
	DWORD diff, rate;
	xfPeerContext* xfp;
	freerdp_peer* client;
	int x, y, width, height;
	XDamageNotifyEvent* notify;

	client = (freerdp_peer*) param;
	xfp = (xfPeerContext*) client->context;
	xfi = xfp->info;

	rate = 1000 / xfp->fps;

	event = CreateFileDescriptorEvent(NULL, FALSE, FALSE, xfi->xfds);

	while (WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0)
	{
		beg = GetTickCount();

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

				if (xf_update_encode(client, x, y, width, height) >= 0)
				{
					xf_xdamage_subtract_region(xfp, x, y, width, height);

					SetEvent(xfp->updateReadyEvent);

					WaitForSingleObject(xfp->updateSentEvent, INFINITE);
					ResetEvent(xfp->updateSentEvent);
				}
			}
#ifdef WITH_XFIXES
			else if (xevent.type == xfi->xfixes_notify_event)
			{
				XFixesCursorImage* ci = XFixesGetCursorImage(xfi->display);
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
