/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>

#include <winpr/crt.h>

#include <freerdp/utils/event.h>
#include <freerdp/client/tsmf.h>

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
	UINT32* xv_pixfmts;
};

#ifdef WITH_DEBUG_XV
#define DEBUG_XV(fmt, ...) DEBUG_CLASS(XV, fmt, ## __VA_ARGS__)
#else
#define DEBUG_XV(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

void xf_tsmf_init(xfContext* xfc, long xv_port)
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

	xv = (xfXvContext*) malloc(sizeof(xfXvContext));
	ZeroMemory(xv, sizeof(xfXvContext));

	xfc->xv_context = xv;

	xv->xv_colorkey_atom = None;
	xv->xv_image_size = 0;
	xv->xv_port = xv_port;

	if (!XShmQueryExtension(xfc->display))
	{
		DEBUG_XV("no shmem available.");
		return;
	}

	ret = XvQueryExtension(xfc->display, &version, &release, &request_base, &event_base, &error_base);
	if (ret != Success)
	{
		DEBUG_XV("XvQueryExtension failed %d.", ret);
		return;
	}
	DEBUG_XV("version %u release %u", version, release);

	ret = XvQueryAdaptors(xfc->display, DefaultRootWindow(xfc->display),
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

	attr = XvQueryPortAttributes(xfc->display, xv->xv_port, &ret);
	for (i = 0; i < (unsigned int)ret; i++)
	{
		if (strcmp(attr[i].name, "XV_COLORKEY") == 0)
		{
			xv->xv_colorkey_atom = XInternAtom(xfc->display, "XV_COLORKEY", FALSE);
			XvSetPortAttribute(xfc->display, xv->xv_port, xv->xv_colorkey_atom, attr[i].min_value + 1);
			break;
		}
	}
	XFree(attr);

#ifdef WITH_DEBUG_XV
	fprintf(stderr, "xf_tsmf_init: pixel format ");
#endif
	fo = XvListImageFormats(xfc->display, xv->xv_port, &ret);
	if (ret > 0)
	{
		xv->xv_pixfmts = (UINT32*) malloc((ret + 1) * sizeof(UINT32));
		ZeroMemory(xv->xv_pixfmts, (ret + 1) * sizeof(UINT32));

		for (i = 0; i < ret; i++)
		{
			xv->xv_pixfmts[i] = fo[i].id;
#ifdef WITH_DEBUG_XV
			fprintf(stderr, "%c%c%c%c ", ((char*)(xv->xv_pixfmts + i))[0], ((char*)(xv->xv_pixfmts + i))[1],
				((char*)(xv->xv_pixfmts + i))[2], ((char*)(xv->xv_pixfmts + i))[3]);
#endif
		}
		xv->xv_pixfmts[i] = 0;
	}
	XFree(fo);
#ifdef WITH_DEBUG_XV
	fprintf(stderr, "\n");
#endif
}

void xf_tsmf_uninit(xfContext* xfc)
{
	xfXvContext* xv = (xfXvContext*) xfc->xv_context;

	if (xv)
	{
		if (xv->xv_image_size > 0)
		{
			shmdt(xv->xv_shmaddr);
			shmctl(xv->xv_shmid, IPC_RMID, NULL);
		}
		if (xv->xv_pixfmts)
		{
			free(xv->xv_pixfmts);
			xv->xv_pixfmts = NULL;
		}
		free(xv);
		xfc->xv_context = NULL;
	}
}

static BOOL
xf_tsmf_is_format_supported(xfXvContext* xv, UINT32 pixfmt)
{
	int i;

	if (!xv->xv_pixfmts)
		return FALSE;

	for (i = 0; xv->xv_pixfmts[i]; i++)
	{
		if (xv->xv_pixfmts[i] == pixfmt)
			return TRUE;
	}

	return FALSE;
}

static void xf_process_tsmf_video_frame_event(xfContext* xfc, RDP_VIDEO_FRAME_EVENT* vevent)
{
	int i;
	BYTE* data1;
	BYTE* data2;
	UINT32 pixfmt;
	UINT32 xvpixfmt;
	BOOL converti420yv12 = FALSE;
	XvImage * image;
	int colorkey = 0;
	XShmSegmentInfo shminfo;
	xfXvContext* xv = (xfXvContext*) xfc->xv_context;

	if (xv->xv_port == 0)
		return;

	/* In case the player is minimized */
	if (vevent->x < -2048 || vevent->y < -2048 || vevent->num_visible_rects <= 0)
		return;

	if (xv->xv_colorkey_atom != None)
	{
		XvGetPortAttribute(xfc->display, xv->xv_port, xv->xv_colorkey_atom, &colorkey);
		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);
		XSetForeground(xfc->display, xfc->gc, colorkey);
		for (i = 0; i < vevent->num_visible_rects; i++)
		{
			XFillRectangle(xfc->display, xfc->window->handle, xfc->gc,
				vevent->x + vevent->visible_rects[i].x,
				vevent->y + vevent->visible_rects[i].y,
				vevent->visible_rects[i].width,
				vevent->visible_rects[i].height);
		}
	}
	else
	{
		XSetClipRectangles(xfc->display, xfc->gc, vevent->x, vevent->y,
			(XRectangle*) vevent->visible_rects, vevent->num_visible_rects, YXBanded);
	}

	pixfmt = vevent->frame_pixfmt;

	if (xf_tsmf_is_format_supported(xv, pixfmt))
	{
		xvpixfmt = pixfmt;
	}
	else if (pixfmt == RDP_PIXFMT_I420 && xf_tsmf_is_format_supported(xv, RDP_PIXFMT_YV12))
	{
		xvpixfmt = RDP_PIXFMT_YV12;
		converti420yv12 = TRUE;
	}
	else if (pixfmt == RDP_PIXFMT_YV12 && xf_tsmf_is_format_supported(xv, RDP_PIXFMT_I420))
	{
		xvpixfmt = RDP_PIXFMT_I420;
		converti420yv12 = TRUE;
	}
	else
	{
		DEBUG_XV("pixel format 0x%X not supported by hardware.", pixfmt);
		return;
	}

	image = XvShmCreateImage(xfc->display, xv->xv_port,
		xvpixfmt, 0, vevent->frame_width, vevent->frame_height, &shminfo);

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
	shminfo.readOnly = FALSE;

	if (!XShmAttach(xfc->display, &shminfo))
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
			if (converti420yv12 == FALSE)
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

	XvShmPutImage(xfc->display, xv->xv_port, xfc->window->handle, xfc->gc, image,
		0, 0, image->width, image->height,
		vevent->x, vevent->y, vevent->width, vevent->height, FALSE);
	if (xv->xv_colorkey_atom == None)
		XSetClipMask(xfc->display, xfc->gc, None);
	XSync(xfc->display, FALSE);

	XShmDetach(xfc->display, &shminfo);
	XFree(image);
}

static void xf_process_tsmf_redraw_event(xfContext* xfc, RDP_REDRAW_EVENT* revent)
{
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XCopyArea(xfc->display, xfc->primary, xfc->window->handle, xfc->gc,
		revent->x, revent->y, revent->width, revent->height, revent->x, revent->y);
}

void xf_process_tsmf_event(xfContext* xfc, wMessage* event)
{
	switch (GetMessageType(event->id))
	{
		case TsmfChannel_VideoFrame:
			xf_process_tsmf_video_frame_event(xfc, (RDP_VIDEO_FRAME_EVENT*) event);
			break;

		case TsmfChannel_Redraw:
			xf_process_tsmf_redraw_event(xfc, (RDP_REDRAW_EVENT*) event);
			break;

	}
}

#else /* WITH_XV */

void xf_tsmf_init(xfContext* xfc, long xv_port)
{
}

void xf_tsmf_uninit(xfContext* xfc)
{
}

void xf_process_tsmf_event(xfContext* xfc, wMessage* event)
{
}

#endif /* WITH_XV */
