/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Video Redirection
 *
 * Copyright 2010-2011 Vic Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/event.h>
#include <freerdp/plugins/tsmf.h>

#include "xf_tsmf.h"

#ifdef WITH_XV

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

typedef struct xf_xv_context xfXvContext;

struct xf_xv_context
{
	long xv_port;
	Atom xv_colorkey_atom;
	int xv_image_size;
	int xv_shmid;
	char* xv_shmaddr;
	uint32* xv_pixfmts;
};

#ifdef WITH_DEBUG_XV
#define DEBUG_XV(fmt, ...) DEBUG_CLASS(XV, fmt, ## __VA_ARGS__)
#else
#define DEBUG_XV(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

void xf_tsmf_init(xfInfo* xfi, long xv_port)
{
	int ret;
	unsigned int i;
	unsigned int version;
	unsigned int release;
	unsigned int event_base;
	unsigned int error_base;
	unsigned int request_base;
	unsigned int num_adaptors;
	xfXvContext* xv;
	XvAdaptorInfo* ai;
	XvAttribute* attr;
	XvImageFormatValues* fo;

	xv = xnew(xfXvContext);
	xfi->xv_context = xv;

	xv->xv_colorkey_atom = None;
	xv->xv_image_size = 0;
	xv->xv_port = xv_port;

	if (!XShmQueryExtension(xfi->display))
	{
		DEBUG_XV("no shmem available.");
		return;
	}

	ret = XvQueryExtension(xfi->display, &version, &release, &request_base, &event_base, &error_base);
	if (ret != Success)
	{
		DEBUG_XV("XvQueryExtension failed %d.", ret);
		return;
	}
	DEBUG_XV("version %u release %u", version, release);

	ret = XvQueryAdaptors(xfi->display, DefaultRootWindow(xfi->display),
		&num_adaptors, &ai);
	if (ret != Success)
	{
		DEBUG_XV("XvQueryAdaptors failed %d.", ret);
		return;
	}

	for (i = 0; i < num_adaptors; i++)
	{
		DEBUG_XV("adapter port %ld-%ld (%s)", ai[i].base_id,
			ai[i].base_id + ai[i].num_ports - 1, ai[i].name);
		if (xv->xv_port == 0 && i == num_adaptors - 1)
			xv->xv_port = ai[i].base_id;
	}

	if (num_adaptors > 0)
		XvFreeAdaptorInfo(ai);

	if (xv->xv_port == 0)
	{
		DEBUG_XV("no adapter selected, video frames will not be processed.");
		return;
	}
	DEBUG_XV("selected %ld", xv->xv_port);

	attr = XvQueryPortAttributes(xfi->display, xv->xv_port, &ret);
	for (i = 0; i < (unsigned int)ret; i++)
	{
		if (strcmp(attr[i].name, "XV_COLORKEY") == 0)
		{
			xv->xv_colorkey_atom = XInternAtom(xfi->display, "XV_COLORKEY", false);
			XvSetPortAttribute(xfi->display, xv->xv_port, xv->xv_colorkey_atom, attr[i].min_value + 1);
			break;
		}
	}
	XFree(attr);

#ifdef WITH_DEBUG_XV
	printf("xf_tsmf_init: pixel format ");
#endif
	fo = XvListImageFormats(xfi->display, xv->xv_port, &ret);
	if (ret > 0)
	{
		xv->xv_pixfmts = (uint32*) xzalloc((ret + 1) * sizeof(uint32));
		for (i = 0; i < ret; i++)
		{
			xv->xv_pixfmts[i] = fo[i].id;
#ifdef WITH_DEBUG_XV
			printf("%c%c%c%c ", ((char*)(xv->xv_pixfmts + i))[0], ((char*)(xv->xv_pixfmts + i))[1],
				((char*)(xv->xv_pixfmts + i))[2], ((char*)(xv->xv_pixfmts + i))[3]);
#endif
		}
		xv->xv_pixfmts[i] = 0;
	}
	XFree(fo);
#ifdef WITH_DEBUG_XV
	printf("\n");
#endif
}

void xf_tsmf_uninit(xfInfo* xfi)
{
	xfXvContext* xv = (xfXvContext*) xfi->xv_context;

	if (xv)
	{
		if (xv->xv_image_size > 0)
		{
			shmdt(xv->xv_shmaddr);
			shmctl(xv->xv_shmid, IPC_RMID, NULL);
		}
		if (xv->xv_pixfmts)
		{
			xfree(xv->xv_pixfmts);
			xv->xv_pixfmts = NULL;
		}
		xfree(xv);
		xfi->xv_context = NULL;
	}
}

static boolean
xf_tsmf_is_format_supported(xfXvContext* xv, uint32 pixfmt)
{
	int i;

	if (!xv->xv_pixfmts)
		return false;

	for (i = 0; xv->xv_pixfmts[i]; i++)
	{
		if (xv->xv_pixfmts[i] == pixfmt)
			return true;
	}

	return false;
}

static void xf_process_tsmf_video_frame_event(xfInfo* xfi, RDP_VIDEO_FRAME_EVENT* vevent)
{
	int i;
	uint8* data1;
	uint8* data2;
	uint32 pixfmt;
	XvImage * image;
	int colorkey = 0;
	XShmSegmentInfo shminfo;
	xfXvContext* xv = (xfXvContext*) xfi->xv_context;

	if (xv->xv_port == 0)
		return;

	/* In case the player is minimized */
	if (vevent->x < -2048 || vevent->y < -2048 || vevent->num_visible_rects <= 0)
		return;

	if (xv->xv_colorkey_atom != None)
	{
		XvGetPortAttribute(xfi->display, xv->xv_port, xv->xv_colorkey_atom, &colorkey);
		XSetFunction(xfi->display, xfi->gc, GXcopy);
		XSetFillStyle(xfi->display, xfi->gc, FillSolid);
		XSetForeground(xfi->display, xfi->gc, colorkey);
		for (i = 0; i < vevent->num_visible_rects; i++)
		{
			XFillRectangle(xfi->display, xfi->window->handle, xfi->gc,
				vevent->x + vevent->visible_rects[i].x,
				vevent->y + vevent->visible_rects[i].y,
				vevent->visible_rects[i].width,
				vevent->visible_rects[i].height);
		}
	}

	pixfmt = vevent->frame_pixfmt;
	image = XvShmCreateImage(xfi->display, xv->xv_port,
		pixfmt, 0, vevent->frame_width, vevent->frame_height, &shminfo);
	if (xv->xv_image_size != image->data_size)
	{
		if (xv->xv_image_size > 0)
		{
			shmdt(xv->xv_shmaddr);
			shmctl(xv->xv_shmid, IPC_RMID, NULL);
		}
		xv->xv_image_size = image->data_size;
		xv->xv_shmid = shmget(IPC_PRIVATE, image->data_size, IPC_CREAT | 0777);
		xv->xv_shmaddr = shmat(xv->xv_shmid, 0, 0);
	}
	shminfo.shmid = xv->xv_shmid;
	shminfo.shmaddr = image->data = xv->xv_shmaddr;
	shminfo.readOnly = false;

	if (!XShmAttach(xfi->display, &shminfo))
	{
		XFree(image);
		DEBUG_XV("XShmAttach failed.");
		return;
	}

	/* The video driver may align each line to a different size
	   and we need to convert our original image data. */
	switch (pixfmt)
	{
		case RDP_PIXFMT_I420:
		case RDP_PIXFMT_YV12:
			if (!xf_tsmf_is_format_supported(xv, RDP_PIXFMT_I420) &&
				!xf_tsmf_is_format_supported(xv, RDP_PIXFMT_YV12))
			{
				DEBUG_XV("pixel format 0x%X not supported by hardware.", pixfmt);
				break;
			}
			/* Y */
			if (image->pitches[0] == vevent->frame_width)
			{
				memcpy(image->data + image->offsets[0],
					vevent->frame_data,
					vevent->frame_width * vevent->frame_height);
			}
			else
			{
				for (i = 0; i < vevent->frame_height; i++)
				{
					memcpy(image->data + image->offsets[0] + i * image->pitches[0],
						vevent->frame_data + i * vevent->frame_width,
						vevent->frame_width);
				}
			}
			/* UV */
			/* Conversion between I420 and YV12 is to simply swap U and V */
			if (xf_tsmf_is_format_supported(xv, pixfmt))
			{
				data1 = vevent->frame_data + vevent->frame_width * vevent->frame_height;
				data2 = vevent->frame_data + vevent->frame_width * vevent->frame_height +
					vevent->frame_width * vevent->frame_height / 4;
			}
			else
			{
				data2 = vevent->frame_data + vevent->frame_width * vevent->frame_height;
				data1 = vevent->frame_data + vevent->frame_width * vevent->frame_height +
					vevent->frame_width * vevent->frame_height / 4;
				image->id = pixfmt == RDP_PIXFMT_I420 ? RDP_PIXFMT_YV12 : RDP_PIXFMT_I420;
			}
			if (image->pitches[1] * 2 == vevent->frame_width)
			{
				memcpy(image->data + image->offsets[1],
					data1,
					vevent->frame_width * vevent->frame_height / 4);
				memcpy(image->data + image->offsets[2],
					data2,
					vevent->frame_width * vevent->frame_height / 4);
			}
			else
			{
				for (i = 0; i < vevent->frame_height / 2; i++)
				{
					memcpy(image->data + image->offsets[1] + i * image->pitches[1],
						data1 + i * vevent->frame_width / 2,
						vevent->frame_width / 2);
					memcpy(image->data + image->offsets[2] + i * image->pitches[2],
						data2 + i * vevent->frame_width / 2,
						vevent->frame_width / 2);
				}
			}
			break;

		default:
			memcpy(image->data, vevent->frame_data, image->data_size <= vevent->frame_size ?
				image->data_size : vevent->frame_size);
			break;
	}

	XvShmPutImage(xfi->display, xv->xv_port, xfi->window->handle, xfi->gc, image,
		0, 0, image->width, image->height,
		vevent->x, vevent->y, vevent->width, vevent->height, false);
	XSync(xfi->display, false);

	XShmDetach(xfi->display, &shminfo);
	XFree(image);
}

static void xf_process_tsmf_redraw_event(xfInfo* xfi, RDP_REDRAW_EVENT* revent)
{
	XSetFunction(xfi->display, xfi->gc, GXcopy);
	XSetFillStyle(xfi->display, xfi->gc, FillSolid);
	XCopyArea(xfi->display, xfi->primary, xfi->window->handle, xfi->gc,
		revent->x, revent->y, revent->width, revent->height, revent->x, revent->y);
}

void xf_process_tsmf_event(xfInfo* xfi, RDP_EVENT* event)
{
	switch (event->event_type)
	{
		case RDP_EVENT_TYPE_TSMF_VIDEO_FRAME:
			xf_process_tsmf_video_frame_event(xfi, (RDP_VIDEO_FRAME_EVENT*) event);
			break;

		case RDP_EVENT_TYPE_TSMF_REDRAW:
			xf_process_tsmf_redraw_event(xfi, (RDP_REDRAW_EVENT*) event);
			break;

	}
}

#else /* WITH_XV */

void xf_tsmf_init(xfInfo* xfi, long xv_port)
{
}

void xf_tsmf_uninit(xfInfo* xfi)
{
}

void xf_process_tsmf_event(xfInfo* xfi, RDP_EVENT* event)
{
}

#endif /* WITH_XV */
