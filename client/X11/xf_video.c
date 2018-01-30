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
#include <winpr/sysinfo.h>
#include <winpr/interlocked.h>

#include <freerdp/client/geometry.h>
#include <freerdp/client/video.h>
#include <freerdp/primitives.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/yuv.h>

#include "xf_video.h"

#define TAG CLIENT_TAG("video")
#define XF_VIDEO_UNLIMITED_RATE 31


BYTE MFVideoFormat_H264[] = {'H', '2', '6', '4',
		0x00, 0x00,
		0x10, 0x00,
		0x80, 0x00,
		0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

typedef struct _xfPresentationContext xfPresentationContext;

struct _xfVideoContext
{
	xfContext *xfc;
	wQueue *frames;
	CRITICAL_SECTION framesLock;
	wBufferPool *surfacePool;
	UINT32 publishedFrames;
	UINT32 droppedFrames;
	UINT32 lastSentRate;
	UINT64 nextFeedbackTime;
	xfPresentationContext *currentPresentation;
};

struct _xfVideoFrame
{
	UINT64 publishTime;
	UINT64 hnsDuration;
	UINT32 x, y, w, h;
	BYTE *surfaceData;
	xfPresentationContext *presentation;
};
typedef struct _xfVideoFrame xfVideoFrame;

struct _xfPresentationContext
{
	xfContext *xfc;
	xfVideoContext *xfVideo;
	BYTE PresentationId;
	UINT32 SourceWidth, SourceHeight;
	UINT32 ScaledWidth, ScaledHeight;
	MAPPED_GEOMETRY *geometry;

	UINT64 startTimeStamp;
	UINT64 publishOffset;
	H264_CONTEXT *h264;
	YUV_CONTEXT *yuv;
	wStream *currentSample;
	BYTE *surfaceData;
	XImage *surface;
	UINT64 lastPublishTime, nextPublishTime;
	volatile LONG refCounter;
};



static void xfPresentationContext_unref(xfPresentationContext *c)
{
	xfVideoContext *xfVideo;

	if (!c)
		return;

	if (InterlockedDecrement(&c->refCounter) != 0)
		return;

	xfVideo = c->xfVideo;
	if (c->geometry)
	{
		c->geometry->MappedGeometryUpdate = NULL;
		c->geometry->MappedGeometryClear = NULL;
		c->geometry->custom = NULL;
	}

	h264_context_free(c->h264);
	Stream_Free(c->currentSample, TRUE);
	XFree(c->surface);
	BufferPool_Return(xfVideo->surfacePool, c->surfaceData);
	yuv_context_free(c->yuv);
	free(c);
}


static xfPresentationContext *xfPresentationContext_new(xfContext *xfc, BYTE PresentationId, UINT32 width, UINT32 height)
{
	xfVideoContext *xfVideo = xfc->xfVideo;
	xfPresentationContext *ret = calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	ret->xfc = xfc;
	ret->PresentationId = PresentationId;

	ret->h264 = h264_context_new(FALSE);
	if (!ret->h264)
	{
		WLog_ERR(TAG, "unable to create a h264 context");
		goto error_h264;
	}
	h264_context_reset(ret->h264, width, height);

	ret->currentSample = Stream_New(NULL, 4096);
	if (!ret->currentSample)
	{
		WLog_ERR(TAG, "unable to create current packet stream");
		goto error_currentSample;
	}

	ret->surfaceData = BufferPool_Take(xfVideo->surfacePool, width * height * 4);
	if (!ret->surfaceData)
	{
		WLog_ERR(TAG, "unable to allocate surfaceData");
		goto error_surfaceData;
	}

	ret->surface = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
			(char *)ret->surfaceData, width, height, 8, width * 4);
	if (!ret->surface)
	{
		WLog_ERR(TAG, "unable to create surface");
		goto error_surface;
	}

	ret->yuv = yuv_context_new(FALSE);
	if (!ret->yuv)
	{
		WLog_ERR(TAG, "unable to create YUV decoder");
		goto error_yuv;
	}

	yuv_context_reset(ret->yuv, width, height);
	ret->refCounter = 1;
	return ret;

error_yuv:
	XFree(ret->surface);
error_surface:
	BufferPool_Return(xfVideo->surfacePool, ret->surfaceData);
error_surfaceData:
	Stream_Free(ret->currentSample, TRUE);
error_currentSample:
	h264_context_free(ret->h264);
error_h264:
	free(ret);
	return NULL;
}

xfVideoContext *xf_video_new(xfContext *xfc)
{
	xfVideoContext *ret = calloc(1, sizeof(xfVideoContext));
	if (!ret)
		return NULL;

	ret->frames = Queue_New(TRUE, 10, 2);
	if (!ret->frames)
	{
		WLog_ERR(TAG, "unable to allocate frames queue");
		goto error_frames;
	}

	ret->surfacePool = BufferPool_New(FALSE, 0, 16);
	if (!ret->surfacePool)
	{
		WLog_ERR(TAG, "unable to create surface pool");
		goto error_surfacePool;
	}

	if (!InitializeCriticalSectionAndSpinCount(&ret->framesLock, 4000))
	{
		WLog_ERR(TAG, "unable to initialize frames lock");
		goto error_spinlock;
	}

	ret->xfc = xfc;

	 /* don't set to unlimited so that we have the chance to send a feedback in
	  * the first second (for servers that want feedback directly)
	  */
	ret->lastSentRate = 30;
	return ret;

error_spinlock:
	BufferPool_Free(ret->surfacePool);
error_surfacePool:
	Queue_Free(ret->frames);
error_frames:
	free(ret);
	return NULL;
}

void xf_video_geometry_init(xfContext *xfc, GeometryClientContext *geom)
{
	xfc->geometry = geom;
}

static BOOL xf_video_onMappedGeometryUpdate(MAPPED_GEOMETRY *geometry)
{
	return TRUE;
}

static BOOL xf_video_onMappedGeometryClear(MAPPED_GEOMETRY *geometry)
{
	xfPresentationContext *presentation = (xfPresentationContext *)geometry->custom;

	presentation->geometry = NULL;
	return TRUE;
}


static UINT xf_video_PresentationRequest(VideoClientContext* context, TSMM_PRESENTATION_REQUEST *req)
{
	xfContext *xfc = context->custom;
	xfVideoContext *xfVideo = xfc->xfVideo;
	xfPresentationContext *presentation;
	UINT ret = CHANNEL_RC_OK;

	presentation = xfVideo->currentPresentation;

	if (req->Command == TSMM_START_PRESENTATION)
	{
		MAPPED_GEOMETRY *geom;
		TSMM_PRESENTATION_RESPONSE resp;

		if (memcmp(req->VideoSubtypeId, MFVideoFormat_H264, 16) != 0)
		{
			WLog_ERR(TAG, "not a H264 video, ignoring request");
			return CHANNEL_RC_OK;
		}

		if (presentation)
		{
			if (presentation->PresentationId == req->PresentationId)
			{
				WLog_ERR(TAG, "ignoring start request for existing presentation %d", req->PresentationId);
				return CHANNEL_RC_OK;
			}

			WLog_ERR(TAG, "releasing current presentation %d", req->PresentationId);
			xfPresentationContext_unref(presentation);
			presentation = xfVideo->currentPresentation = NULL;
		}

		if (!xfc->geometry)
		{
			WLog_ERR(TAG, "geometry channel not ready, ignoring request");
			return CHANNEL_RC_OK;
		}

		geom = HashTable_GetItemValue(xfc->geometry->geometries, &(req->GeometryMappingId));
		if (!geom)
		{
			WLog_ERR(TAG, "geometry mapping 0x%"PRIx64" not registered", req->GeometryMappingId);
			return CHANNEL_RC_OK;
		}

		WLog_DBG(TAG, "creating presentation 0x%x", req->PresentationId);
		presentation = xfPresentationContext_new(xfc, req->PresentationId, req->SourceWidth, req->SourceHeight);
		if (!presentation)
		{
			WLog_ERR(TAG, "unable to create presentation context");
			return CHANNEL_RC_NO_MEMORY;
		}

		xfVideo->currentPresentation = presentation;
		presentation->xfVideo = xfVideo;
		presentation->geometry = geom;
		presentation->SourceWidth = req->SourceWidth;
		presentation->SourceHeight = req->SourceHeight;
		presentation->ScaledWidth = req->ScaledWidth;
		presentation->ScaledHeight = req->ScaledHeight;

		geom->custom = presentation;
		geom->MappedGeometryUpdate = xf_video_onMappedGeometryUpdate;
		geom->MappedGeometryClear = xf_video_onMappedGeometryClear;

		/* send back response */
		resp.PresentationId = req->PresentationId;
		ret = context->PresentationResponse(context, &resp);
	}
	else if (req->Command == TSMM_STOP_PRESENTATION)
	{
		WLog_DBG(TAG, "stopping presentation 0x%x", req->PresentationId);
		if (!presentation)
		{
			WLog_ERR(TAG, "unknown presentation to stop %d", req->PresentationId);
			return CHANNEL_RC_OK;
		}

		xfVideo->currentPresentation = NULL;
		xfVideo->droppedFrames = 0;
		xfVideo->publishedFrames = 0;
		xfPresentationContext_unref(presentation);
	}

	return CHANNEL_RC_OK;
}


static BOOL yuv_to_rgb(xfPresentationContext *presentation, BYTE *dest)
{
	const BYTE* pYUVPoint[3];
	H264_CONTEXT *h264 = presentation->h264;

	BYTE** ppYUVData;
	ppYUVData = h264->pYUVData;

	pYUVPoint[0] = ppYUVData[0];
	pYUVPoint[1] = ppYUVData[1];
	pYUVPoint[2] = ppYUVData[2];

	if (!yuv_context_decode(presentation->yuv, pYUVPoint, h264->iStride, PIXEL_FORMAT_BGRX32, dest, h264->width * 4))
	{
		WLog_ERR(TAG, "error in yuv_to_rgb conversion");
		return FALSE;
	}

	return TRUE;
}

static void xf_video_frame_free(xfVideoFrame **pframe)
{
	xfVideoFrame *frame = *pframe;

	xfPresentationContext_unref(frame->presentation);
	BufferPool_Return(frame->presentation->xfVideo->surfacePool, frame->surfaceData);
	free(frame);
	*pframe = NULL;
}

static void xf_video_timer(xfContext *xfc, TimerEventArgs *timer)
{
	xfVideoContext *xfVideo = xfc->xfVideo;
	xfPresentationContext *presentation;
	xfVideoFrame *peekFrame, *frame = NULL;

	EnterCriticalSection(&xfVideo->framesLock);
	do
	{
		peekFrame = (xfVideoFrame *)Queue_Peek(xfVideo->frames);
		if (!peekFrame)
			break;

		if (peekFrame->publishTime > timer->now)
			break;

		if (frame)
		{
			/* free skipped frame */
			WLog_DBG(TAG, "dropping frame @%"PRIu64, frame->publishTime);
			xfVideo->droppedFrames++;
			xf_video_frame_free(&frame);
		}
		frame = peekFrame;
		Queue_Dequeue(xfVideo->frames);
	}
	while (1);
	LeaveCriticalSection(&xfVideo->framesLock);

	if (!frame)
		goto treat_feedback;

	presentation = frame->presentation;

	xfVideo->publishedFrames++;
	memcpy(presentation->surfaceData, frame->surfaceData, frame->w * frame->h * 4);

	XPutImage(xfc->display, xfc->drawable, xfc->gc, presentation->surface,
		0, 0,
		frame->x, frame->y, frame->w, frame->h);

	xfPresentationContext_unref(presentation);
	BufferPool_Return(xfVideo->surfacePool, frame->surfaceData);
	free(frame);

treat_feedback:
	if (xfVideo->nextFeedbackTime < timer->now)
	{
		/* we can compute some feedback only if we have some published frames and
		 * a current presentation
		 */
		if (xfVideo->publishedFrames && xfVideo->currentPresentation)
		{
			UINT32 computedRate;

			InterlockedIncrement(&xfVideo->currentPresentation->refCounter);

			if (xfVideo->droppedFrames)
			{
				/**
				 * some dropped frames, looks like we're asking too many frames per seconds,
				 * try lowering rate. We go directly from unlimited rate to 24 frames/seconds
				 * otherwise we lower rate by 2 frames by seconds
				 */
				if (xfVideo->lastSentRate == XF_VIDEO_UNLIMITED_RATE)
					computedRate = 24;
				else
				{
					computedRate = xfVideo->lastSentRate - 2;
					if (!computedRate)
						computedRate = 2;
				}
			}
			else
			{
				/**
				 * we treat all frames ok, so either ask the server to send more,
				 * or stay unlimited
				 */
				if (xfVideo->lastSentRate == XF_VIDEO_UNLIMITED_RATE)
					computedRate = XF_VIDEO_UNLIMITED_RATE; /* stay unlimited */
				else
				{
					computedRate = xfVideo->lastSentRate + 2;
					if (computedRate > XF_VIDEO_UNLIMITED_RATE)
						computedRate = XF_VIDEO_UNLIMITED_RATE;
				}
			}

			if (computedRate != xfVideo->lastSentRate)
			{
				TSMM_CLIENT_NOTIFICATION notif;
				notif.PresentationId = xfVideo->currentPresentation->PresentationId;
				notif.NotificationType = TSMM_CLIENT_NOTIFICATION_TYPE_FRAMERATE_OVERRIDE;
				if (computedRate == XF_VIDEO_UNLIMITED_RATE)
				{
					notif.FramerateOverride.Flags = 0x01;
					notif.FramerateOverride.DesiredFrameRate = 0x00;
				}
				else
				{
					notif.FramerateOverride.Flags = 0x02;
					notif.FramerateOverride.DesiredFrameRate = computedRate;
				}

				xfVideo->xfc->video->ClientNotification(xfVideo->xfc->video, &notif);
				xfVideo->lastSentRate = computedRate;

				WLog_DBG(TAG, "server notified with rate %d published=%d dropped=%d", xfVideo->lastSentRate,
						xfVideo->publishedFrames, xfVideo->droppedFrames);
			}

			xfPresentationContext_unref(xfVideo->currentPresentation);
		}

		WLog_DBG(TAG, "currentRate=%d published=%d dropped=%d", xfVideo->lastSentRate,
				xfVideo->publishedFrames, xfVideo->droppedFrames);

		xfVideo->droppedFrames = 0;
		xfVideo->publishedFrames = 0;
		xfVideo->nextFeedbackTime = timer->now + 1000;
	}
}

static UINT xf_video_VideoData(VideoClientContext* context, TSMM_VIDEO_DATA *data)
{
	xfContext *xfc = context->custom;
	xfVideoContext *xfVideo = xfc->xfVideo;
	xfPresentationContext *presentation;
	int status;

	presentation = xfVideo->currentPresentation;
	if (!presentation)
	{
		WLog_ERR(TAG, "no current presentation");
		return CHANNEL_RC_OK;
	}

	if (presentation->PresentationId != data->PresentationId)
	{
		WLog_ERR(TAG, "current presentation id=%d doesn't match data id=%d", presentation->PresentationId,
				data->PresentationId);
		return CHANNEL_RC_OK;
	}

	if (!Stream_EnsureRemainingCapacity(presentation->currentSample, data->cbSample))
	{
		WLog_ERR(TAG, "unable to expand the current packet");
		return CHANNEL_RC_NO_MEMORY;
	}

	Stream_Write(presentation->currentSample, data->pSample, data->cbSample);

	if (data->CurrentPacketIndex == data->PacketsInSample)
	{
		H264_CONTEXT *h264 = presentation->h264;
		UINT64 startTime = GetTickCount64(), timeAfterH264;
		MAPPED_GEOMETRY *geom = presentation->geometry;

		Stream_SealLength(presentation->currentSample);
		Stream_SetPosition(presentation->currentSample, 0);

		status = h264->subsystem->Decompress(h264, Stream_Pointer(presentation->currentSample),
				Stream_Length(presentation->currentSample));
		if (status == 0)
			return CHANNEL_RC_OK;

		if (status < 0)
			return CHANNEL_RC_OK;

		timeAfterH264 = GetTickCount64();
		if (data->SampleNumber == 1)
		{
			presentation->lastPublishTime = startTime;
		}

		presentation->lastPublishTime += (data->hnsDuration / 10000);
		if (presentation->lastPublishTime <= timeAfterH264 + 10)
		{
			int dropped = 0;

			/* if the frame is to be published in less than 10 ms, let's consider it's now */
			yuv_to_rgb(presentation, presentation->surfaceData);

			XPutImage(xfc->display, xfc->drawable, xfc->gc, presentation->surface,
				0, 0,
				geom->topLevelLeft + geom->left + geom->geometry.boundingRect.x,
				geom->topLevelTop + geom->top + geom->geometry.boundingRect.y,
				presentation->SourceWidth, presentation->SourceHeight);

			xfVideo->publishedFrames++;

			/* cleanup previously scheduled frames */
			EnterCriticalSection(&xfVideo->framesLock);
			while (Queue_Count(xfVideo->frames) > 0)
			{
				xfVideoFrame *frame = Queue_Dequeue(xfVideo->frames);
				if (frame)
				{
					xfVideo->droppedFrames++;
					xf_video_frame_free(&frame);
					dropped++;
				}
			}
			LeaveCriticalSection(&xfVideo->framesLock);

			if (dropped)
				WLog_DBG(TAG, "showing frame (%d dropped)", dropped);
		}
		else
		{
			BOOL enqueueResult;
			xfVideoFrame *frame = calloc(1, sizeof(*frame));
			if (!frame)
			{
				WLog_ERR(TAG, "unable to create frame");
				return CHANNEL_RC_NO_MEMORY;
			}

			frame->presentation = presentation;
			frame->publishTime = presentation->lastPublishTime;
			frame->x = geom->topLevelLeft + geom->left + geom->geometry.boundingRect.x;
			frame->y = geom->topLevelTop + geom->top + geom->geometry.boundingRect.y;
			frame->w = presentation->SourceWidth;
			frame->h = presentation->SourceHeight;

			frame->surfaceData = BufferPool_Take(xfVideo->surfacePool, frame->w * frame->h * 4);
			if (!frame->surfaceData)
			{
				WLog_ERR(TAG, "unable to allocate frame data");
				free(frame);
				return CHANNEL_RC_NO_MEMORY;
			}

			if (!yuv_to_rgb(presentation, frame->surfaceData))
			{
				WLog_ERR(TAG, "error during YUV->RGB conversion");
				BufferPool_Return(xfVideo->surfacePool, frame->surfaceData);
				free(frame);
				return CHANNEL_RC_NO_MEMORY;
			}

			InterlockedIncrement(&presentation->refCounter);

			EnterCriticalSection(&xfVideo->framesLock);
			enqueueResult = Queue_Enqueue(xfVideo->frames, frame);
			LeaveCriticalSection(&xfVideo->framesLock);

			if (!enqueueResult)
			{
				WLog_ERR(TAG, "unable to enqueue frame");
				xf_video_frame_free(&frame);
				return CHANNEL_RC_NO_MEMORY;
			}

			WLog_DBG(TAG, "scheduling frame in %"PRIu32" ms", (frame->publishTime-startTime));
		}
	}

	return CHANNEL_RC_OK;
}

void xf_video_free(xfVideoContext *xfVideo)
{
	EnterCriticalSection(&xfVideo->framesLock);
	while (Queue_Count(xfVideo->frames))
	{
		xfVideoFrame *frame = Queue_Dequeue(xfVideo->frames);
		if (frame)
			xf_video_frame_free(&frame);
	}

	Queue_Free(xfVideo->frames);
	LeaveCriticalSection(&xfVideo->framesLock);

	DeleteCriticalSection(&xfVideo->framesLock);

	if (xfVideo->currentPresentation)
		xfPresentationContext_unref(xfVideo->currentPresentation);

	BufferPool_Free(xfVideo->surfacePool);
	free(xfVideo);
}


void xf_video_control_init(xfContext *xfc, VideoClientContext *video)
{
	xfc->video = video;
	video->custom = xfc;
	video->PresentationRequest = xf_video_PresentationRequest;
}

void xf_video_control_uninit(xfContext *xfc, VideoClientContext *video)
{
	video->VideoData = NULL;
}


void xf_video_data_init(xfContext *xfc, VideoClientContext *video)
{
	video->VideoData = xf_video_VideoData;
	PubSub_SubscribeTimer(xfc->context.pubSub, (pTimerEventHandler)xf_video_timer);
}

void xf_video_data_uninit(xfContext *xfc, VideoClientContext *context)
{
	PubSub_UnsubscribeTimer(xfc->context.pubSub, (pTimerEventHandler)xf_video_timer);
}
