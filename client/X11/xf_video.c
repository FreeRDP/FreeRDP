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

#include <winpr/assert.h>
#include <freerdp/client/geometry.h>
#include <freerdp/client/video.h>
#include <freerdp/gdi/video.h>

#include "xf_video.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("video")

typedef struct
{
	VideoSurface base;
	XImage* image;
} xfVideoSurface;

static VideoSurface* xfVideoCreateSurface(VideoClientContext* video, UINT32 x, UINT32 y,
                                          UINT32 width, UINT32 height)
{
	xfContext* xfc;
	xfVideoSurface* ret;

	WINPR_ASSERT(video);
	ret = (xfVideoSurface*)VideoClient_CreateCommonContext(sizeof(xfContext), x, y, width, height);
	if (!ret)
		return NULL;

	xfc = (xfContext*)video->custom;
	WINPR_ASSERT(xfc);

	ret->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
	                          (char*)ret->base.data, width, height, 8, ret->base.scanline);

	if (!ret->image)
	{
		WLog_ERR(TAG, "unable to create surface image");
		VideoClient_DestroyCommonContext(&ret->base);
		return NULL;
	}

	return &ret->base;
}

static BOOL xfVideoShowSurface(VideoClientContext* video, const VideoSurface* surface,
                               UINT32 destinationWidth, UINT32 destinationHeight)
{
	const xfVideoSurface* xfSurface = (const xfVideoSurface*)surface;
	xfContext* xfc;
	const rdpSettings* settings;

	WINPR_ASSERT(video);
	WINPR_ASSERT(xfSurface);

	xfc = video->custom;
	WINPR_ASSERT(xfc);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

#ifdef WITH_XRENDER

	if (settings->SmartSizing || settings->MultiTouchGestures)
	{
		XPutImage(xfc->display, xfc->primary, xfc->gc, xfSurface->image, 0, 0, surface->x,
		          surface->y, surface->w, surface->h);
		xf_draw_screen(xfc, surface->x, surface->y, surface->w, surface->h);
	}
	else
#endif
	{
		XPutImage(xfc->display, xfc->drawable, xfc->gc, xfSurface->image, 0, 0, surface->x,
		          surface->y, surface->w, surface->h);
	}

	return TRUE;
}

static BOOL xfVideoDeleteSurface(VideoClientContext* video, VideoSurface* surface)
{
	xfVideoSurface* xfSurface = (xfVideoSurface*)surface;

	WINPR_UNUSED(video);

	if (xfSurface)
		XFree(xfSurface->image);

	VideoClient_DestroyCommonContext(surface);
	return TRUE;
}

void xf_video_control_init(xfContext* xfc, VideoClientContext* video)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(video);

	gdi_video_control_init(xfc->common.context.gdi, video);

	/* X11 needs to be able to handle 32bpp colors directly. */
	if (xfc->depth >= 24)
	{
		video->custom = xfc;
		video->createSurface = xfVideoCreateSurface;
		video->showSurface = xfVideoShowSurface;
		video->deleteSurface = xfVideoDeleteSurface;
	}
}

void xf_video_control_uninit(xfContext* xfc, VideoClientContext* video)
{
	WINPR_ASSERT(xfc);
	gdi_video_control_uninit(xfc->common.context.gdi, video);
}
