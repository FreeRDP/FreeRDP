/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <X11/Xlib.h>

#include "xf_encode.h"

XImage* xf_snapshot(xfPeerContext* xfp, int x, int y, int width, int height)
{
	XImage* image;
	xfInfo* xfi = xfp->info;

	pthread_mutex_lock(&(xfp->mutex));

	if (xfi->use_xshm)
	{
		XCopyArea(xfi->display, xfi->root_window, xfi->fb_pixmap,
				xfi->xdamage_gc, x, y, width, height, x, y);

		XSync(xfi->display, False);

		image = xfi->fb_image;
	}
	else
	{
		image = XGetImage(xfi->display, xfi->root_window,
				x, y, width, height, AllPlanes, ZPixmap);
	}

	pthread_mutex_unlock(&(xfp->mutex));

	return image;
}
