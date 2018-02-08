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

#include "xf_video.h"

#define TAG CLIENT_TAG("video")

typedef struct
{
	VideoSurface base;
	XImage* image;
} xfVideoSurface;


void xf_video_geometry_init(xfContext* xfc, GeometryClientContext* geom)
{
	xfc->geometry = geom;

	if (xfc->video)
	{
		VideoClientContext* video = xfc->video;
		video->setGeometry(video, xfc->geometry);
	}
}

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


static BOOL xfVideoShowSurface(VideoClientContext* video, xfVideoSurface* surface)
{
	xfContext* xfc = video->custom;
#ifdef WITH_XRENDER

	if (xfc->context.settings->SmartSizing
	    || xfc->context.settings->MultiTouchGestures)
	{
		XPutImage(xfc->display, xfc->primary, xfc->gc, surface->image,
		          0, 0, surface->base.x, surface->base.y, surface->base.w, surface->base.h);
		xf_draw_screen(xfc, surface->base.x, surface->base.y, surface->base.w, surface->base.h);
	}
	else
#endif
	{
		XPutImage(xfc->display, xfc->drawable, xfc->gc, surface->image,
		          0, 0,
		          surface->base.x, surface->base.y, surface->base.w, surface->base.h);
	}

	return TRUE;
}

static BOOL xfVideoDeleteSurface(VideoClientContext* video, xfVideoSurface* surface)
{
	XFree(surface->image);
	free(surface);
	return TRUE;
}
void xf_video_control_init(xfContext* xfc, VideoClientContext* video)
{
	xfc->video = video;
	video->custom = xfc;
	video->createSurface = xfVideoCreateSurface;
	video->showSurface = (pcVideoShowSurface)xfVideoShowSurface;
	video->deleteSurface = (pcVideoDeleteSurface)xfVideoDeleteSurface;
	video->setGeometry(video, xfc->geometry);
}


void xf_video_control_uninit(xfContext* xfc, VideoClientContext* video)
{
}

static void xf_video_timer(xfContext* xfc, TimerEventArgs* timer)
{
	xfc->video->timer(xfc->video, timer->now);
}

void xf_video_data_init(xfContext* xfc, VideoClientContext* video)
{
	PubSub_SubscribeTimer(xfc->context.pubSub, (pTimerEventHandler)xf_video_timer);
}

void xf_video_data_uninit(xfContext* xfc, VideoClientContext* context)
{
	PubSub_UnsubscribeTimer(xfc->context.pubSub, (pTimerEventHandler)xf_video_timer);
}
