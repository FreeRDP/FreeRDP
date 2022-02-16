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

#include <freerdp/config.h>

#include <winpr/crt.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>

#include <freerdp/log.h>
#include <freerdp/client/tsmf.h>

#include "xf_tsmf.h"

#ifdef WITH_XV

#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>

static long xv_port = 0;

struct xf_xv_context
{
	long xv_port;
	Atom xv_colorkey_atom;
	int xv_image_size;
	int xv_shmid;
	char* xv_shmaddr;
	UINT32* xv_pixfmts;
};
typedef struct xf_xv_context xfXvContext;

#define TAG CLIENT_TAG("x11")

static BOOL xf_tsmf_is_format_supported(xfXvContext* xv, UINT32 pixfmt)
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

static int xf_tsmf_xv_video_frame_event(TsmfClientContext* tsmf, TSMF_VIDEO_FRAME_EVENT* event)
{
	int i;
	int x, y;
	UINT32 width;
	UINT32 height;
	BYTE* data1;
	BYTE* data2;
	UINT32 pixfmt;
	UINT32 xvpixfmt;
	XvImage* image;
	int colorkey = 0;
	int numRects = 0;
	xfContext* xfc;
	xfXvContext* xv;
	XRectangle* xrects = NULL;
	XShmSegmentInfo shminfo;
	BOOL converti420yv12 = FALSE;

	if (!tsmf)
		return -1;

	xfc = (xfContext*)tsmf->custom;

	if (!xfc)
		return -1;

	xv = (xfXvContext*)xfc->xv_context;

	if (!xv)
		return -1;

	if (xv->xv_port == 0)
		return -1001;

	/* In case the player is minimized */
	if (event->x < -2048 || event->y < -2048 || event->numVisibleRects == 0)
	{
		return -1002;
	}

	xrects = NULL;
	numRects = event->numVisibleRects;

	if (numRects > 0)
	{
		xrects = (XRectangle*)calloc(numRects, sizeof(XRectangle));

		if (!xrects)
			return -1;

		for (i = 0; i < numRects; i++)
		{
			x = event->x + event->visibleRects[i].left;
			y = event->y + event->visibleRects[i].top;
			width = event->visibleRects[i].right - event->visibleRects[i].left;
			height = event->visibleRects[i].bottom - event->visibleRects[i].top;

			xrects[i].x = x;
			xrects[i].y = y;
			xrects[i].width = width;
			xrects[i].height = height;
		}
	}

	if (xv->xv_colorkey_atom != None)
	{
		XvGetPortAttribute(xfc->display, xv->xv_port, xv->xv_colorkey_atom, &colorkey);
		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);
		XSetForeground(xfc->display, xfc->gc, colorkey);

		if (event->numVisibleRects < 1)
		{
			XSetClipMask(xfc->display, xfc->gc, None);
		}
		else
		{
			XFillRectangles(xfc->display, xfc->window->handle, xfc->gc, xrects, numRects);
		}
	}
	else
	{
		XSetFunction(xfc->display, xfc->gc, GXcopy);
		XSetFillStyle(xfc->display, xfc->gc, FillSolid);

		if (event->numVisibleRects < 1)
		{
			XSetClipMask(xfc->display, xfc->gc, None);
		}
		else
		{
			XSetClipRectangles(xfc->display, xfc->gc, 0, 0, xrects, numRects, YXBanded);
		}
	}

	pixfmt = event->framePixFmt;

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
		WLog_DBG(TAG, "pixel format 0x%" PRIX32 " not supported by hardware.", pixfmt);
		free(xrects);
		return -1003;
	}

	image = XvShmCreateImage(xfc->display, xv->xv_port, xvpixfmt, 0, event->frameWidth,
	                         event->frameHeight, &shminfo);

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
		free(xrects);
		WLog_DBG(TAG, "XShmAttach failed.");
		return -1004;
	}

	/* The video driver may align each line to a different size
	   and we need to convert our original image data. */
	switch (pixfmt)
	{
		case RDP_PIXFMT_I420:
		case RDP_PIXFMT_YV12:
			/* Y */
			if (image->pitches[0] == event->frameWidth)
			{
				CopyMemory(image->data + image->offsets[0], event->frameData,
				           event->frameWidth * event->frameHeight);
			}
			else
			{
				for (i = 0; i < event->frameHeight; i++)
				{
					CopyMemory(image->data + image->offsets[0] + i * image->pitches[0],
					           event->frameData + i * event->frameWidth, event->frameWidth);
				}
			}
			/* UV */
			/* Conversion between I420 and YV12 is to simply swap U and V */
			if (!converti420yv12)
			{
				data1 = event->frameData + event->frameWidth * event->frameHeight;
				data2 = event->frameData + event->frameWidth * event->frameHeight +
				        event->frameWidth * event->frameHeight / 4;
			}
			else
			{
				data2 = event->frameData + event->frameWidth * event->frameHeight;
				data1 = event->frameData + event->frameWidth * event->frameHeight +
				        event->frameWidth * event->frameHeight / 4;
				image->id = pixfmt == RDP_PIXFMT_I420 ? RDP_PIXFMT_YV12 : RDP_PIXFMT_I420;
			}

			if (image->pitches[1] * 2 == event->frameWidth)
			{
				CopyMemory(image->data + image->offsets[1], data1,
				           event->frameWidth * event->frameHeight / 4);
				CopyMemory(image->data + image->offsets[2], data2,
				           event->frameWidth * event->frameHeight / 4);
			}
			else
			{
				for (i = 0; i < event->frameHeight / 2; i++)
				{
					CopyMemory(image->data + image->offsets[1] + i * image->pitches[1],
					           data1 + i * event->frameWidth / 2, event->frameWidth / 2);
					CopyMemory(image->data + image->offsets[2] + i * image->pitches[2],
					           data2 + i * event->frameWidth / 2, event->frameWidth / 2);
				}
			}
			break;

		default:
			if (image->data_size < 0)
			{
				free(xrects);
				return -2000;
			}
			else
			{
				const size_t size = ((UINT32)image->data_size <= event->frameSize)
				                        ? (UINT32)image->data_size
				                        : event->frameSize;
				CopyMemory(image->data, event->frameData, size);
			}
			break;
	}

	XvShmPutImage(xfc->display, xv->xv_port, xfc->window->handle, xfc->gc, image, 0, 0,
	              image->width, image->height, event->x, event->y, event->width, event->height,
	              FALSE);

	if (xv->xv_colorkey_atom == None)
		XSetClipMask(xfc->display, xfc->gc, None);

	XSync(xfc->display, FALSE);

	XShmDetach(xfc->display, &shminfo);
	XFree(image);

	free(xrects);

	return 1;
}

static int xf_tsmf_xv_init(xfContext* xfc, TsmfClientContext* tsmf)
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

	if (xfc->xv_context)
		return 1; /* context already created */

	xv = (xfXvContext*)calloc(1, sizeof(xfXvContext));

	if (!xv)
		return -1;

	xfc->xv_context = xv;

	xv->xv_colorkey_atom = None;
	xv->xv_image_size = 0;
	xv->xv_port = xv_port;

	if (!XShmQueryExtension(xfc->display))
	{
		WLog_DBG(TAG, "no xshm available.");
		return -1;
	}

	ret =
	    XvQueryExtension(xfc->display, &version, &release, &request_base, &event_base, &error_base);

	if (ret != Success)
	{
		WLog_DBG(TAG, "XvQueryExtension failed %d.", ret);
		return -1;
	}

	WLog_DBG(TAG, "version %u release %u", version, release);

	ret = XvQueryAdaptors(xfc->display, DefaultRootWindow(xfc->display), &num_adaptors, &ai);

	if (ret != Success)
	{
		WLog_DBG(TAG, "XvQueryAdaptors failed %d.", ret);
		return -1;
	}

	for (i = 0; i < num_adaptors; i++)
	{
		WLog_DBG(TAG, "adapter port %lu-%lu (%s)", ai[i].base_id,
		         ai[i].base_id + ai[i].num_ports - 1, ai[i].name);

		if (xv->xv_port == 0 && i == num_adaptors - 1)
			xv->xv_port = ai[i].base_id;
	}

	if (num_adaptors > 0)
		XvFreeAdaptorInfo(ai);

	if (xv->xv_port == 0)
	{
		WLog_DBG(TAG, "no adapter selected, video frames will not be processed.");
		return -1;
	}
	WLog_DBG(TAG, "selected %ld", xv->xv_port);

	attr = XvQueryPortAttributes(xfc->display, xv->xv_port, &ret);

	for (i = 0; i < (unsigned int)ret; i++)
	{
		if (strcmp(attr[i].name, "XV_COLORKEY") == 0)
		{
			xv->xv_colorkey_atom = XInternAtom(xfc->display, "XV_COLORKEY", FALSE);
			XvSetPortAttribute(xfc->display, xv->xv_port, xv->xv_colorkey_atom,
			                   attr[i].min_value + 1);
			break;
		}
	}
	XFree(attr);

	WLog_DBG(TAG, "xf_tsmf_init: pixel format ");

	fo = XvListImageFormats(xfc->display, xv->xv_port, &ret);

	if (ret > 0)
	{
		xv->xv_pixfmts = (UINT32*)calloc((ret + 1), sizeof(UINT32));

		for (i = 0; i < (unsigned int)ret; i++)
		{
			xv->xv_pixfmts[i] = fo[i].id;
			WLog_DBG(TAG, "%c%c%c%c ", ((char*)(xv->xv_pixfmts + i))[0],
			         ((char*)(xv->xv_pixfmts + i))[1], ((char*)(xv->xv_pixfmts + i))[2],
			         ((char*)(xv->xv_pixfmts + i))[3]);
		}
		xv->xv_pixfmts[i] = 0;
	}
	XFree(fo);

	if (tsmf)
	{
		xfc->tsmf = tsmf;
		tsmf->custom = (void*)xfc;

		tsmf->FrameEvent = xf_tsmf_xv_video_frame_event;
	}

	return 1;
}

static int xf_tsmf_xv_uninit(xfContext* xfc, TsmfClientContext* tsmf)
{
	xfXvContext* xv = (xfXvContext*)xfc->xv_context;

	WINPR_UNUSED(tsmf);
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

	if (xfc->tsmf)
	{
		xfc->tsmf->custom = NULL;
		xfc->tsmf = NULL;
	}

	return 1;
}

#endif

int xf_tsmf_init(xfContext* xfc, TsmfClientContext* tsmf)
{
#ifdef WITH_XV
	return xf_tsmf_xv_init(xfc, tsmf);
#endif

	return 1;
}

int xf_tsmf_uninit(xfContext* xfc, TsmfClientContext* tsmf)
{
#ifdef WITH_XV
	return xf_tsmf_xv_uninit(xfc, tsmf);
#endif

	return 1;
}
