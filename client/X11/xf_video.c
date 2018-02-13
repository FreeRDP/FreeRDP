/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Optimized Remoting Virtual Channel Extension for X11
 *
 * Copyright 2017 David Fort <contact@hardening-consulting.com>
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


#include <freerdp/client/geometry.h>
#include <freerdp/client/video.h>
#include <freerdp/gdi/video.h>

#include "xf_video.h"

#define TAG CLIENT_TAG("video")

typedef struct
{
	VideoSurface base;
	XImage* image;
} xfVideoSurface;

static VideoSurface* xfVideoCreateSurface(VideoClientContext* video, BYTE* data, UINT32 x, UINT32 y,
        UINT32 width, UINT32 height)
{
	xfContext* xfc = (xfContext*)video->custom;
	xfVideoSurface* ret = calloc(1, sizeof(*ret));

	if (!ret)
		return NULL;

	ret->base.data = data;
	ret->base.x = x;
	ret->base.y = y;
	ret->base.w = width;
	ret->base.h = height;
	ret->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
	                          (char*)data, width, height, 8, width * 4);

	if (!ret->image)
	{
		WLog_ERR(TAG, "unable to create surface image");
		free(ret);
		return NULL;
	}

	return &ret->base;
}


static BOOL xfVideoShowSurface(VideoClientContext* video, VideoSurface* surface)
{
	xfVideoSurface* xfSurface = (xfVideoSurface*)surface;
	xfContext* xfc = video->custom;
#ifdef WITH_XRENDER

	if (xfc->context.settings->SmartSizing
	    || xfc->context.settings->MultiTouchGestures)
	{
		XPutImage(xfc->display, xfc->primary, xfc->gc, xfSurface->image,
		          0, 0, surface->x, surface->y, surface->w, surface->h);
		xf_draw_screen(xfc, surface->x, surface->y, surface->w, surface->h);
	}
	else
#endif
	{
		XPutImage(xfc->display, xfc->drawable, xfc->gc, xfSurface->image,
		          0, 0,
		          surface->x, surface->y, surface->w, surface->h);
	}

	return TRUE;
}

static BOOL xfVideoDeleteSurface(VideoClientContext* video, VideoSurface* surface)
{
	xfVideoSurface* xfSurface = (xfVideoSurface*)surface;

	if (xfSurface)
		XFree(xfSurface->image);

	free(surface);
	return TRUE;
}
void xf_video_control_init(xfContext* xfc, VideoClientContext* video)
{
	gdi_video_control_init(xfc->context.gdi, video);
	video->custom = xfc;
	video->createSurface = xfVideoCreateSurface;
	video->showSurface = xfVideoShowSurface;
	video->deleteSurface = xfVideoDeleteSurface;
}


void xf_video_control_uninit(xfContext* xfc, VideoClientContext* video)
{
	gdi_video_control_uninit(xfc->context.gdi, video);
}
