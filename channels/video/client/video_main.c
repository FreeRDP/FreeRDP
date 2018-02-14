/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Optimized Remoting Virtual Channel Extension
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/print.h>
#include <winpr/stream.h>
#include <winpr/cmdline.h>
#include <winpr/collections.h>
#include <winpr/interlocked.h>
#include <winpr/sysinfo.h>

#include <freerdp/addin.h>
#include <freerdp/primitives.h>
#include <freerdp/client/video.h>
#include <freerdp/channels/log.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/yuv.h>


#define TAG CHANNELS_TAG("video")

#include "video_main.h"

struct _VIDEO_CHANNEL_CALLBACK
{
	IWTSVirtualChannelCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
};
typedef struct _VIDEO_CHANNEL_CALLBACK VIDEO_CHANNEL_CALLBACK;

struct _VIDEO_LISTENER_CALLBACK
{
	IWTSListenerCallback iface;

	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	VIDEO_CHANNEL_CALLBACK* channel_callback;
};
typedef struct _VIDEO_LISTENER_CALLBACK VIDEO_LISTENER_CALLBACK;

struct _VIDEO_PLUGIN
{
	IWTSPlugin wtsPlugin;

	IWTSListener* controlListener;
	IWTSListener* dataListener;
	VIDEO_LISTENER_CALLBACK* control_callback;
	VIDEO_LISTENER_CALLBACK* data_callback;

	VideoClientContext *context;
};
typedef struct _VIDEO_PLUGIN VIDEO_PLUGIN;


#define XF_VIDEO_UNLIMITED_RATE 31

static const BYTE MFVideoFormat_H264[] = {'H', '2', '6', '4',
		0x00, 0x00,
		0x10, 0x00,
		0x80, 0x00,
		0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};

typedef struct _PresentationContext PresentationContext;
typedef struct _VideoFrame VideoFrame;


/** @brief private data for the channel */
struct _VideoClientContextPriv
{
	VideoClientContext *video;
	GeometryClientContext *geometry;
	wQueue *frames;
	CRITICAL_SECTION framesLock;
	wBufferPool *surfacePool;
	UINT32 publishedFrames;
	UINT32 droppedFrames;
	UINT32 lastSentRate;
	UINT64 nextFeedbackTime;
	PresentationContext *currentPresentation;
};

/** @brief */
struct _VideoFrame
{
	UINT64 publishTime;
	UINT64 hnsDuration;
	MAPPED_GEOMETRY *geometry;
	UINT32 w, h;
	BYTE *surfaceData;
	PresentationContext *presentation;
};

/** @brief */
struct _PresentationContext
{
	VideoClientContext *video;
	BYTE PresentationId;
	UINT32 SourceWidth, SourceHeight;
	UINT32 ScaledWidth, ScaledHeight;
	MAPPED_GEOMETRY *geometry;

	UINT64 startTimeStamp;
	UINT64 publishOffset;
	H264_CONTEXT *h264;
	YUV_CONTEXT *yuv;
	wStream *currentSample;
	UINT64 lastPublishTime, nextPublishTime;
	volatile LONG refCounter;
	BYTE *surfaceData;
	VideoSurface *surface;
};


static const char *video_command_name(BYTE cmd)
{
	switch(cmd)
	{
	case TSMM_START_PRESENTATION:
		return "start";
	case TSMM_STOP_PRESENTATION:
		return "stop";
	default:
		return "<unknown>";
	}
}

static BOOL yuv_to_rgb(PresentationContext *presentation, BYTE *dest)
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


static void video_client_context_set_geometry(VideoClientContext *video, GeometryClientContext *geometry)
{
	video->priv->geometry = geometry;
}

VideoClientContextPriv *VideoClientContextPriv_new(VideoClientContext *video)
{
	VideoClientContextPriv *ret = calloc(1, sizeof(*ret));
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

	if (!InitializeCriticalSectionAndSpinCount(&ret->framesLock, 4 * 1000))
	{
		WLog_ERR(TAG, "unable to initialize frames lock");
		goto error_spinlock;
	}

	ret->video = video;

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


static PresentationContext *PresentationContext_new(VideoClientContext *video, BYTE PresentationId,
		UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
	VideoClientContextPriv *priv = video->priv;
	PresentationContext *ret = calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	ret->video = video;
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

	ret->surfaceData = BufferPool_Take(priv->surfacePool, width * height * 4);
	if (!ret->surfaceData)
	{
		WLog_ERR(TAG, "unable to allocate surfaceData");
		goto error_surfaceData;
	}

	ret->surface = video->createSurface(video, ret->surfaceData, x, y, width, height);
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
	video->deleteSurface(video, ret->surface);
error_surface:
	BufferPool_Return(priv->surfacePool, ret->surfaceData);
error_surfaceData:
	Stream_Free(ret->currentSample, TRUE);
error_currentSample:
	h264_context_free(ret->h264);
error_h264:
	free(ret);
	return NULL;
}


static void PresentationContext_unref(PresentationContext *presentation)
{
	VideoClientContextPriv *priv;
	MAPPED_GEOMETRY *geometry;

	if (!presentation)
		return;

	if (InterlockedDecrement(&presentation->refCounter) != 0)
		return;

	geometry = presentation->geometry;
	if (geometry)
	{
		geometry->MappedGeometryUpdate = NULL;
		geometry->MappedGeometryClear = NULL;
		geometry->custom = NULL;
		mappedGeometryUnref(geometry);
	}

	priv = presentation->video->priv;

	h264_context_free(presentation->h264);
	Stream_Free(presentation->currentSample, TRUE);
	presentation->video->deleteSurface(presentation->video, presentation->surface);
	BufferPool_Return(priv->surfacePool, presentation->surfaceData);
	yuv_context_free(presentation->yuv);
	free(presentation);
}


static void VideoFrame_free(VideoFrame **pframe)
{
	VideoFrame *frame = *pframe;

	mappedGeometryUnref(frame->geometry);
	BufferPool_Return(frame->presentation->video->priv->surfacePool, frame->surfaceData);
	PresentationContext_unref(frame->presentation);
	free(frame);
	*pframe = NULL;
}


static void VideoClientContextPriv_free(VideoClientContextPriv *priv)
{
	EnterCriticalSection(&priv->framesLock);
	while (Queue_Count(priv->frames))
	{
		VideoFrame *frame = Queue_Dequeue(priv->frames);
		if (frame)
			VideoFrame_free(&frame);
	}

	Queue_Free(priv->frames);
	LeaveCriticalSection(&priv->framesLock);

	DeleteCriticalSection(&priv->framesLock);

	if (priv->currentPresentation)
		PresentationContext_unref(priv->currentPresentation);

	BufferPool_Free(priv->surfacePool);
	free(priv);
}


static UINT video_control_send_presentation_response(VideoClientContext *context, TSMM_PRESENTATION_RESPONSE *resp)
{
	BYTE buf[12];
	wStream *s;
	VIDEO_PLUGIN* video = (VIDEO_PLUGIN *)context->handle;
	IWTSVirtualChannel* channel;
	UINT ret;

	s = Stream_New(buf, 12);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	Stream_Write_UINT32(s, 12); /* cbSize */
	Stream_Write_UINT32(s, TSMM_PACKET_TYPE_PRESENTATION_RESPONSE); /* PacketType */
	Stream_Write_UINT8(s, resp->PresentationId);
	Stream_Zero(s, 3);
	Stream_SealLength(s);

	channel = video->control_callback->channel_callback->channel;
	ret = channel->Write(channel, 12, buf, NULL);
	Stream_Free(s, FALSE);

	return ret;
}

static BOOL video_onMappedGeometryUpdate(MAPPED_GEOMETRY *geometry)
{
	PresentationContext *presentation = (PresentationContext *)geometry->custom;
	RDP_RECT *r = &geometry->geometry.boundingRect;
	WLog_DBG(TAG, "geometry updated topGeom=(%d,%d-%dx%d) geom=(%d,%d-%dx%d) rects=(%d,%d-%dx%d)",
			geometry->topLevelLeft, geometry->topLevelTop,
			geometry->topLevelRight - geometry->topLevelLeft, geometry->topLevelBottom - geometry->topLevelTop,

			geometry->left, geometry->top,
			geometry->right - geometry->left, geometry->bottom - geometry->top,

			r->x, r->y, r->width, r->height
	);

	presentation->surface->x = geometry->topLevelLeft + geometry->left;
	presentation->surface->y = geometry->topLevelTop + geometry->top;

	return TRUE;
}

static BOOL video_onMappedGeometryClear(MAPPED_GEOMETRY *geometry)
{
	PresentationContext *presentation = (PresentationContext *)geometry->custom;

	mappedGeometryUnref(presentation->geometry);
	presentation->geometry = NULL;
	return TRUE;
}

static UINT video_PresentationRequest(VideoClientContext* video, TSMM_PRESENTATION_REQUEST *req)
{
	VideoClientContextPriv *priv = video->priv;
	PresentationContext *presentation;
	UINT ret = CHANNEL_RC_OK;

	presentation = priv->currentPresentation;

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
			PresentationContext_unref(presentation);
			presentation = priv->currentPresentation = NULL;
		}

		if (!priv->geometry)
		{
			WLog_ERR(TAG, "geometry channel not ready, ignoring request");
			return CHANNEL_RC_OK;
		}

		geom = HashTable_GetItemValue(priv->geometry->geometries, &(req->GeometryMappingId));
		if (!geom)
		{
			WLog_ERR(TAG, "geometry mapping 0x%"PRIx64" not registered", req->GeometryMappingId);
			return CHANNEL_RC_OK;
		}

		WLog_DBG(TAG, "creating presentation 0x%x", req->PresentationId);
		presentation = PresentationContext_new(video, req->PresentationId,
				geom->topLevelLeft + geom->left,
				geom->topLevelTop + geom->top,
				req->SourceWidth, req->SourceHeight);
		if (!presentation)
		{
			WLog_ERR(TAG, "unable to create presentation video");
			return CHANNEL_RC_NO_MEMORY;
		}

		mappedGeometryRef(geom);
		presentation->geometry = geom;

		priv->currentPresentation = presentation;
		presentation->video = video;
		presentation->SourceWidth = req->SourceWidth;
		presentation->SourceHeight = req->SourceHeight;
		presentation->ScaledWidth = req->ScaledWidth;
		presentation->ScaledHeight = req->ScaledHeight;

		geom->custom = presentation;
		geom->MappedGeometryUpdate = video_onMappedGeometryUpdate;
		geom->MappedGeometryClear = video_onMappedGeometryClear;

		/* send back response */
		resp.PresentationId = req->PresentationId;
		ret = video_control_send_presentation_response(video, &resp);
	}
	else if (req->Command == TSMM_STOP_PRESENTATION)
	{
		WLog_DBG(TAG, "stopping presentation 0x%x", req->PresentationId);
		if (!presentation)
		{
			WLog_ERR(TAG, "unknown presentation to stop %d", req->PresentationId);
			return CHANNEL_RC_OK;
		}

		priv->currentPresentation = NULL;
		priv->droppedFrames = 0;
		priv->publishedFrames = 0;
		PresentationContext_unref(presentation);
	}

	return CHANNEL_RC_OK;
}


static UINT video_read_tsmm_presentation_req(VideoClientContext *context, wStream *s)
{
	TSMM_PRESENTATION_REQUEST req;

	if (Stream_GetRemainingLength(s) < 60)
	{
		WLog_ERR(TAG, "not enough bytes for a TSMM_PRESENTATION_REQUEST");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT8(s, req.PresentationId);
	Stream_Read_UINT8(s, req.Version);
	Stream_Read_UINT8(s, req.Command);
	Stream_Read_UINT8(s, req.FrameRate); /* FrameRate - reserved and ignored */

	Stream_Seek_UINT16(s); /* AverageBitrateKbps reserved and ignored */
	Stream_Seek_UINT16(s); /* reserved */

	Stream_Read_UINT32(s, req.SourceWidth);
	Stream_Read_UINT32(s, req.SourceHeight);
	Stream_Read_UINT32(s, req.ScaledWidth);
	Stream_Read_UINT32(s, req.ScaledHeight);
	Stream_Read_UINT64(s, req.hnsTimestampOffset);
	Stream_Read_UINT64(s, req.GeometryMappingId);
	Stream_Read(s, req.VideoSubtypeId, 16);

	Stream_Read_UINT32(s, req.cbExtra);

	if (Stream_GetRemainingLength(s) < req.cbExtra)
	{
		WLog_ERR(TAG, "not enough bytes for cbExtra of TSMM_PRESENTATION_REQUEST");
		return ERROR_INVALID_DATA;
	}

	req.pExtraData = Stream_Pointer(s);

	WLog_DBG(TAG, "presentationReq: id:%"PRIu8" version:%"PRIu8" command:%s srcWidth/srcHeight=%"PRIu32"x%"PRIu32
			" scaled Width/Height=%"PRIu32"x%"PRIu32" timestamp=%"PRIu64" mappingId=%"PRIx64"",
			req.PresentationId, req.Version, video_command_name(req.Command),
			req.SourceWidth, req.SourceHeight, req.ScaledWidth, req.ScaledHeight,
			req.hnsTimestampOffset, req.GeometryMappingId);

	return video_PresentationRequest(context, &req);
}



/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_control_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *s)
{
	VIDEO_CHANNEL_CALLBACK* callback = (VIDEO_CHANNEL_CALLBACK*) pChannelCallback;
	VIDEO_PLUGIN* video;
	VideoClientContext *context;
	UINT ret = CHANNEL_RC_OK;
	UINT32 cbSize, packetType;

	video = (VIDEO_PLUGIN*) callback->plugin;
	context = (VideoClientContext *)video->wtsPlugin.pInterface;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, cbSize);
	if (cbSize < 8 || Stream_GetRemainingLength(s) < (cbSize-4))
	{
		WLog_ERR(TAG, "invalid cbSize");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, packetType);
	switch (packetType)
	{
	case TSMM_PACKET_TYPE_PRESENTATION_REQUEST:
		ret = video_read_tsmm_presentation_req(context, s);
		break;
	default:
		WLog_ERR(TAG, "not expecting packet type %"PRIu32"", packetType);
		ret = ERROR_UNSUPPORTED_TYPE;
		break;
	}

	return ret;
}

static UINT video_control_send_client_notification(VideoClientContext *context, TSMM_CLIENT_NOTIFICATION *notif)
{
	BYTE buf[100];
	wStream *s;
	VIDEO_PLUGIN* video = (VIDEO_PLUGIN *)context->handle;
	IWTSVirtualChannel* channel;
	UINT ret;
	UINT32 cbSize;

	s = Stream_New(buf, 30);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;

	cbSize = 16;
	Stream_Seek_UINT32(s); /* cbSize */
	Stream_Write_UINT32(s, TSMM_PACKET_TYPE_CLIENT_NOTIFICATION); /* PacketType */
	Stream_Write_UINT8(s, notif->PresentationId);
	Stream_Write_UINT8(s, notif->NotificationType);
	Stream_Zero(s, 2);
	if (notif->NotificationType == TSMM_CLIENT_NOTIFICATION_TYPE_FRAMERATE_OVERRIDE)
	{
		Stream_Write_UINT32(s, 16); /* cbData */

		/* TSMM_CLIENT_NOTIFICATION_FRAMERATE_OVERRIDE */
		Stream_Write_UINT32(s, notif->FramerateOverride.Flags);
		Stream_Write_UINT32(s, notif->FramerateOverride.DesiredFrameRate);
		Stream_Zero(s, 4 * 2);

		cbSize += 4 * 4;
	}
	else
	{
		Stream_Write_UINT32(s, 0); /* cbData */
	}

	Stream_SealLength(s);
	Stream_SetPosition(s, 0);
	Stream_Write_UINT32(s, cbSize);
	Stream_Free(s, FALSE);

	channel = video->control_callback->channel_callback->channel;
	ret = channel->Write(channel, cbSize, buf, NULL);

	return ret;
}

static void video_timer(VideoClientContext *video, UINT64 now)
{
	PresentationContext *presentation;
	VideoClientContextPriv *priv = video->priv;
	VideoFrame *peekFrame, *frame = NULL;

	EnterCriticalSection(&priv->framesLock);
	do
	{
		peekFrame = (VideoFrame *)Queue_Peek(priv->frames);
		if (!peekFrame)
			break;

		if (peekFrame->publishTime > now)
			break;

		if (frame)
		{
			WLog_DBG(TAG, "dropping frame @%"PRIu64, frame->publishTime);
			priv->droppedFrames++;
			VideoFrame_free(&frame);
		}
		frame = peekFrame;
		Queue_Dequeue(priv->frames);
	}
	while (1);
	LeaveCriticalSection(&priv->framesLock);

	if (!frame)
		goto treat_feedback;

	presentation = frame->presentation;

	priv->publishedFrames++;
	memcpy(presentation->surfaceData, frame->surfaceData, frame->w * frame->h * 4);

	video->showSurface(video, presentation->surface);

	VideoFrame_free(&frame);

treat_feedback:
	if (priv->nextFeedbackTime < now)
	{
		/* we can compute some feedback only if we have some published frames and
		 * a current presentation
		 */
		if (priv->publishedFrames && priv->currentPresentation)
		{
			UINT32 computedRate;

			InterlockedIncrement(&priv->currentPresentation->refCounter);

			if (priv->droppedFrames)
			{
				/**
				 * some dropped frames, looks like we're asking too many frames per seconds,
				 * try lowering rate. We go directly from unlimited rate to 24 frames/seconds
				 * otherwise we lower rate by 2 frames by seconds
				 */
				if (priv->lastSentRate == XF_VIDEO_UNLIMITED_RATE)
					computedRate = 24;
				else
				{
					computedRate = priv->lastSentRate - 2;
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
				if (priv->lastSentRate == XF_VIDEO_UNLIMITED_RATE)
					computedRate = XF_VIDEO_UNLIMITED_RATE; /* stay unlimited */
				else
				{
					computedRate = priv->lastSentRate + 2;
					if (computedRate > XF_VIDEO_UNLIMITED_RATE)
						computedRate = XF_VIDEO_UNLIMITED_RATE;
				}
			}

			if (computedRate != priv->lastSentRate)
			{
				TSMM_CLIENT_NOTIFICATION notif;
				notif.PresentationId = priv->currentPresentation->PresentationId;
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

				video_control_send_client_notification(video, &notif);
				priv->lastSentRate = computedRate;

				WLog_DBG(TAG, "server notified with rate %d published=%d dropped=%d", priv->lastSentRate,
						priv->publishedFrames, priv->droppedFrames);
			}

			PresentationContext_unref(priv->currentPresentation);
		}

		WLog_DBG(TAG, "currentRate=%d published=%d dropped=%d", priv->lastSentRate,
				priv->publishedFrames, priv->droppedFrames);

		priv->droppedFrames = 0;
		priv->publishedFrames = 0;
		priv->nextFeedbackTime = now + 1000;
	}
}


static UINT video_VideoData(VideoClientContext* context, TSMM_VIDEO_DATA *data)
{
	VideoClientContextPriv *priv = context->priv;
	PresentationContext *presentation;
	int status;

	presentation = priv->currentPresentation;
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

			context->showSurface(context, presentation->surface);

			priv->publishedFrames++;

			/* cleanup previously scheduled frames */
			EnterCriticalSection(&priv->framesLock);
			while (Queue_Count(priv->frames) > 0)
			{
				VideoFrame *frame = Queue_Dequeue(priv->frames);
				if (frame)
				{
					priv->droppedFrames++;
					VideoFrame_free(&frame);
					dropped++;
				}
			}
			LeaveCriticalSection(&priv->framesLock);

			if (dropped)
				WLog_DBG(TAG, "showing frame (%d dropped)", dropped);
		}
		else
		{
			BOOL enqueueResult;
			VideoFrame *frame = calloc(1, sizeof(*frame));
			if (!frame)
			{
				WLog_ERR(TAG, "unable to create frame");
				return CHANNEL_RC_NO_MEMORY;
			}
			mappedGeometryRef(geom);

			frame->presentation = presentation;
			frame->publishTime = presentation->lastPublishTime;
			frame->geometry = geom;
			frame->w = presentation->SourceWidth;
			frame->h = presentation->SourceHeight;

			frame->surfaceData = BufferPool_Take(priv->surfacePool, frame->w * frame->h * 4);
			if (!frame->surfaceData)
			{
				WLog_ERR(TAG, "unable to allocate frame data");
				mappedGeometryUnref(geom);
				free(frame);
				return CHANNEL_RC_NO_MEMORY;
			}

			if (!yuv_to_rgb(presentation, frame->surfaceData))
			{
				WLog_ERR(TAG, "error during YUV->RGB conversion");
				BufferPool_Return(priv->surfacePool, frame->surfaceData);
				mappedGeometryUnref(geom);
				free(frame);
				return CHANNEL_RC_NO_MEMORY;
			}

			InterlockedIncrement(&presentation->refCounter);

			EnterCriticalSection(&priv->framesLock);
			enqueueResult = Queue_Enqueue(priv->frames, frame);
			LeaveCriticalSection(&priv->framesLock);

			if (!enqueueResult)
			{
				WLog_ERR(TAG, "unable to enqueue frame");
				VideoFrame_free(&frame);
				return CHANNEL_RC_NO_MEMORY;
			}

			WLog_DBG(TAG, "scheduling frame in %"PRIu32" ms", (frame->publishTime-startTime));
		}
	}

	return CHANNEL_RC_OK;
}



static UINT video_data_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream *s)
{
	VIDEO_CHANNEL_CALLBACK* callback = (VIDEO_CHANNEL_CALLBACK*) pChannelCallback;
	VIDEO_PLUGIN* video;
	VideoClientContext *context;
	UINT32 cbSize, packetType;
	TSMM_VIDEO_DATA data;

	video = (VIDEO_PLUGIN*) callback->plugin;
	context = (VideoClientContext *)video->wtsPlugin.pInterface;

	if (Stream_GetRemainingLength(s) < 4)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, cbSize);
	if (cbSize < 8 || Stream_GetRemainingLength(s) < (cbSize-4))
	{
		WLog_ERR(TAG, "invalid cbSize");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT32(s, packetType);
	if (packetType != TSMM_PACKET_TYPE_VIDEO_DATA)
	{
		WLog_ERR(TAG, "only expecting VIDEO_DATA on the data channel");
		return ERROR_INVALID_DATA;
	}

	if (Stream_GetRemainingLength(s) < 32)
	{
		WLog_ERR(TAG, "not enough bytes for a TSMM_VIDEO_DATA");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT8(s, data.PresentationId);
	Stream_Read_UINT8(s, data.Version);
	Stream_Read_UINT8(s, data.Flags);
	Stream_Seek_UINT8(s); /* reserved */
	Stream_Read_UINT64(s, data.hnsTimestamp);
	Stream_Read_UINT64(s, data.hnsDuration);
	Stream_Read_UINT16(s, data.CurrentPacketIndex);
	Stream_Read_UINT16(s, data.PacketsInSample);
	Stream_Read_UINT32(s, data.SampleNumber);
	Stream_Read_UINT32(s, data.cbSample);
	data.pSample = Stream_Pointer(s);

/*
	WLog_DBG(TAG, "videoData: id:%"PRIu8" version:%"PRIu8" flags:0x%"PRIx8" timestamp=%"PRIu64" duration=%"PRIu64
			" curPacketIndex:%"PRIu16" packetInSample:%"PRIu16" sampleNumber:%"PRIu32" cbSample:%"PRIu32"",
			data.PresentationId, data.Version, data.Flags, data.hnsTimestamp, data.hnsDuration,
			data.CurrentPacketIndex, data.PacketsInSample, data.SampleNumber, data.cbSample);
*/

	return video_VideoData(context, &data);
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_control_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

static UINT video_data_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_control_on_new_channel_connection(IWTSListenerCallback* listenerCallback,
	IWTSVirtualChannel* channel, BYTE* Data, BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	VIDEO_CHANNEL_CALLBACK* callback;
	VIDEO_LISTENER_CALLBACK* listener_callback = (VIDEO_LISTENER_CALLBACK*) listenerCallback;

	callback = (VIDEO_CHANNEL_CALLBACK*) calloc(1, sizeof(VIDEO_CHANNEL_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = video_control_on_data_received;
	callback->iface.OnClose = video_control_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = channel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return CHANNEL_RC_OK;
}

static UINT video_data_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
	IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
	IWTSVirtualChannelCallback** ppCallback)
{
	VIDEO_CHANNEL_CALLBACK* callback;
	VIDEO_LISTENER_CALLBACK* listener_callback = (VIDEO_LISTENER_CALLBACK*) pListenerCallback;

	callback = (VIDEO_CHANNEL_CALLBACK*) calloc(1, sizeof(VIDEO_CHANNEL_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnDataReceived = video_data_on_data_received;
	callback->iface.OnClose = video_data_on_close;
	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;
	listener_callback->channel_callback = callback;

	*ppCallback = (IWTSVirtualChannelCallback*) callback;

	return CHANNEL_RC_OK;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_plugin_initialize(IWTSPlugin* plugin, IWTSVirtualChannelManager* channelMgr)
{
	UINT status;
	VIDEO_PLUGIN* video = (VIDEO_PLUGIN *)plugin;
	VIDEO_LISTENER_CALLBACK *callback;

	video->control_callback = callback = (VIDEO_LISTENER_CALLBACK*) calloc(1, sizeof(VIDEO_LISTENER_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc for control callback failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnNewChannelConnection = video_control_on_new_channel_connection;
	callback->plugin = plugin;
	callback->channel_mgr = channelMgr;

	status = channelMgr->CreateListener(channelMgr, VIDEO_CONTROL_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*)callback, &(video->controlListener));

	if (status != CHANNEL_RC_OK)
		return status;
	video->controlListener->pInterface = video->wtsPlugin.pInterface;


	video->data_callback = callback = (VIDEO_LISTENER_CALLBACK*) calloc(1, sizeof(VIDEO_LISTENER_CALLBACK));
	if (!callback)
	{
		WLog_ERR(TAG, "calloc for data callback failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	callback->iface.OnNewChannelConnection = video_data_on_new_channel_connection;
	callback->plugin = plugin;
	callback->channel_mgr = channelMgr;

	status = channelMgr->CreateListener(channelMgr, VIDEO_DATA_DVC_CHANNEL_NAME, 0,
		(IWTSListenerCallback*)callback, &(video->dataListener));

	if (status == CHANNEL_RC_OK)
		video->dataListener->pInterface = video->wtsPlugin.pInterface;

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT video_plugin_terminated(IWTSPlugin* pPlugin)
{
	VIDEO_PLUGIN* video = (VIDEO_PLUGIN*) pPlugin;

	if (video->context)
		VideoClientContextPriv_free(video->context->priv);

	free(video->control_callback);
	free(video->data_callback);
	free(video->wtsPlugin.pInterface);
	free(pPlugin);
	return CHANNEL_RC_OK;
}

/**
 * Channel Client Interface
 */


#ifdef BUILTIN_CHANNELS
#define DVCPluginEntry		video_DVCPluginEntry
#else
#define DVCPluginEntry		FREERDP_API DVCPluginEntry
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	UINT error = CHANNEL_RC_OK;
	VIDEO_PLUGIN* videoPlugin;
	VideoClientContext* videoContext;
	VideoClientContextPriv *priv;

	videoPlugin = (VIDEO_PLUGIN*) pEntryPoints->GetPlugin(pEntryPoints, "video");
	if (!videoPlugin)
	{
		videoPlugin = (VIDEO_PLUGIN*) calloc(1, sizeof(VIDEO_PLUGIN));
		if (!videoPlugin)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		videoPlugin->wtsPlugin.Initialize = video_plugin_initialize;
		videoPlugin->wtsPlugin.Connected = NULL;
		videoPlugin->wtsPlugin.Disconnected = NULL;
		videoPlugin->wtsPlugin.Terminated = video_plugin_terminated;

		videoContext = (VideoClientContext*) calloc(1, sizeof(VideoClientContext));
		if (!videoContext)
		{
			WLog_ERR(TAG, "calloc failed!");
			free(videoPlugin);
			return CHANNEL_RC_NO_MEMORY;
		}

		priv = VideoClientContextPriv_new(videoContext);
		if (!priv)
		{
			WLog_ERR(TAG, "VideoClientContextPriv_new failed!");
			free(videoContext);
			free(videoPlugin);
			return CHANNEL_RC_NO_MEMORY;
		}

		videoContext->handle = (void*) videoPlugin;
		videoContext->priv = priv;
		videoContext->timer = video_timer;
		videoContext->setGeometry = video_client_context_set_geometry;

		videoPlugin->wtsPlugin.pInterface = (void*) videoContext;
		videoPlugin->context = videoContext;

		error = pEntryPoints->RegisterPlugin(pEntryPoints, "video", (IWTSPlugin*) videoPlugin);
	}
	else
	{
		WLog_ERR(TAG, "could not get video Plugin.");
		return CHANNEL_RC_BAD_CHANNEL;
	}

	return error;
}
