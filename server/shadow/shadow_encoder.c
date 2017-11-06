/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "shadow.h"

#include "shadow_encoder.h"

int shadow_encoder_preferred_fps(rdpShadowEncoder* encoder)
{
	/* Return preferred fps calculated according to the last
	 * sent frame id and last client-acknowledged frame id.
	 */
	return encoder->fps;
}

UINT32 shadow_encoder_inflight_frames(rdpShadowEncoder* encoder)
{
	/* Return inflight frame count.
	 * If queueDepth is SUSPEND_FRAME_ACKNOWLEDGEMENT, count = 0
	 * Otherwise, calculate count =
	 * <last sent frame id> - <last client-acknowledged frame id>
	 * Note: This function is exported so that subsystem could
	 * implement its own strategy to tune fps.
	 */
	return (encoder->queueDepth == SUSPEND_FRAME_ACKNOWLEDGEMENT) ? 0 : encoder->frameId -
	       encoder->lastAckframeId;
}

UINT32 shadow_encoder_create_frame_id(rdpShadowEncoder* encoder)
{
	UINT32 frameId;
	int inFlightFrames;
	inFlightFrames = shadow_encoder_inflight_frames(encoder);

	/*
	 * Calculate preferred fps according to how much frames are
	 * in-progress. Note that it only works when subsytem implementation
	 * calls shadow_encoder_preferred_fps and takes the suggestion.
	 */
	if (inFlightFrames > 1)
	{
		encoder->fps = (100 / (inFlightFrames + 1) * encoder->maxFps) / 100;
	}
	else
	{
		encoder->fps += 2;

		if (encoder->fps > encoder->maxFps)
			encoder->fps = encoder->maxFps;
	}

	if (encoder->fps < 1)
		encoder->fps = 1;

	frameId = ++encoder->frameId;
	return frameId;
}

static int shadow_encoder_init_grid(rdpShadowEncoder* encoder)
{
	int i, j, k;
	int tileSize;
	int tileCount;
	encoder->gridWidth = ((encoder->width + (encoder->maxTileWidth - 1)) /
	                      encoder->maxTileWidth);
	encoder->gridHeight = ((encoder->height + (encoder->maxTileHeight - 1)) /
	                       encoder->maxTileHeight);
	tileSize = encoder->maxTileWidth * encoder->maxTileHeight * 4;
	tileCount = encoder->gridWidth * encoder->gridHeight;
	encoder->gridBuffer = (BYTE*) calloc(tileSize, tileCount);

	if (!encoder->gridBuffer)
		return -1;

	encoder->grid = (BYTE**) calloc(tileCount, sizeof(BYTE*));

	if (!encoder->grid)
		return -1;

	for (i = 0; i < encoder->gridHeight; i++)
	{
		for (j = 0; j < encoder->gridWidth; j++)
		{
			k = (i * encoder->gridWidth) + j;
			encoder->grid[k] = &(encoder->gridBuffer[k * tileSize]);
		}
	}

	return 0;
}

static int shadow_encoder_uninit_grid(rdpShadowEncoder* encoder)
{
	if (encoder->gridBuffer)
	{
		free(encoder->gridBuffer);
		encoder->gridBuffer = NULL;
	}

	if (encoder->grid)
	{
		free(encoder->grid);
		encoder->grid = NULL;
	}

	encoder->gridWidth = 0;
	encoder->gridHeight = 0;
	return 0;
}

static int shadow_encoder_init_rfx(rdpShadowEncoder* encoder)
{
	if (!encoder->rfx)
		encoder->rfx = rfx_context_new(TRUE);

	if (!encoder->rfx)
		goto fail;

	if (!rfx_context_reset(encoder->rfx, encoder->width, encoder->height))
		goto fail;

	encoder->rfx->mode = encoder->server->rfxMode;
	rfx_context_set_pixel_format(encoder->rfx, PIXEL_FORMAT_BGRX32);
	encoder->codecs |= FREERDP_CODEC_REMOTEFX;
	return 1;
fail:
	rfx_context_free(encoder->rfx);
	return -1;
}

static int shadow_encoder_init_nsc(rdpShadowEncoder* encoder)
{
	rdpContext* context = (rdpContext*) encoder->client;
	rdpSettings* settings = context->settings;

	if (!encoder->nsc)
		encoder->nsc = nsc_context_new();

	if (!encoder->nsc)
		goto fail;

	if (!nsc_context_reset(encoder->nsc, encoder->width, encoder->height))
		goto fail;

	encoder->nsc->ColorLossLevel = settings->NSCodecColorLossLevel;
	encoder->nsc->ChromaSubsamplingLevel = settings->NSCodecAllowSubsampling ? 1 :
	                                       0;
	encoder->nsc->DynamicColorFidelity = settings->NSCodecAllowDynamicColorFidelity;
	nsc_context_set_pixel_format(encoder->nsc, PIXEL_FORMAT_BGRX32);
	encoder->codecs |= FREERDP_CODEC_NSCODEC;
	return 1;
fail:
	nsc_context_free(encoder->nsc);
	return -1;
}

static int shadow_encoder_init_planar(rdpShadowEncoder* encoder)
{
	DWORD planarFlags = 0;
	rdpContext* context = (rdpContext*) encoder->client;
	rdpSettings* settings = context->settings;

	if (settings->DrawAllowSkipAlpha)
		planarFlags |= PLANAR_FORMAT_HEADER_NA;

	planarFlags |= PLANAR_FORMAT_HEADER_RLE;

	if (!encoder->planar)
	{
		encoder->planar = freerdp_bitmap_planar_context_new(planarFlags,
		                  encoder->maxTileWidth, encoder->maxTileHeight);
	}

	if (!encoder->planar)
		goto fail;

	if (!freerdp_bitmap_planar_context_reset(encoder->planar,
	        encoder->maxTileWidth,
	        encoder->maxTileHeight))
		goto fail;

	encoder->codecs |= FREERDP_CODEC_PLANAR;
	return 1;
fail:
	freerdp_bitmap_planar_context_free(encoder->planar);
	return -1;
}

static int shadow_encoder_init_interleaved(rdpShadowEncoder* encoder)
{
	if (!encoder->interleaved)
		encoder->interleaved = bitmap_interleaved_context_new(TRUE);

	if (!encoder->interleaved)
		goto fail;

	if (!bitmap_interleaved_context_reset(encoder->interleaved))
		goto fail;

	encoder->codecs |= FREERDP_CODEC_INTERLEAVED;
	return 1;
fail:
	bitmap_interleaved_context_free(encoder->interleaved);
	return -1;
}

static int shadow_encoder_init_h264(rdpShadowEncoder* encoder)
{
	if (!encoder->h264)
		encoder->h264 = h264_context_new(TRUE);

	if (!encoder->h264)
		goto fail;

	if (!h264_context_reset(encoder->h264, encoder->width, encoder->height))
		goto fail;

	encoder->h264->RateControlMode = encoder->server->h264RateControlMode;
	encoder->h264->BitRate = encoder->server->h264BitRate;
	encoder->h264->FrameRate = encoder->server->h264FrameRate;
	encoder->h264->QP = encoder->server->h264QP;
	encoder->codecs |= FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444;
	return 1;
fail:
	h264_context_free(encoder->h264);
	return -1;
}

static int shadow_encoder_init(rdpShadowEncoder* encoder)
{
	encoder->width = encoder->server->screen->width;
	encoder->height = encoder->server->screen->height;
	encoder->maxTileWidth = 64;
	encoder->maxTileHeight = 64;
	shadow_encoder_init_grid(encoder);

	if (!encoder->bs)
		encoder->bs = Stream_New(NULL,
		                         encoder->maxTileWidth * encoder->maxTileHeight * 4);

	if (!encoder->bs)
		return -1;

	return 1;
}

static int shadow_encoder_uninit_rfx(rdpShadowEncoder* encoder)
{
	if (encoder->rfx)
	{
		rfx_context_free(encoder->rfx);
		encoder->rfx = NULL;
	}

	encoder->codecs &= ~FREERDP_CODEC_REMOTEFX;
	return 1;
}

static int shadow_encoder_uninit_nsc(rdpShadowEncoder* encoder)
{
	if (encoder->nsc)
	{
		nsc_context_free(encoder->nsc);
		encoder->nsc = NULL;
	}

	encoder->codecs &= ~FREERDP_CODEC_NSCODEC;
	return 1;
}

static int shadow_encoder_uninit_planar(rdpShadowEncoder* encoder)
{
	if (encoder->planar)
	{
		freerdp_bitmap_planar_context_free(encoder->planar);
		encoder->planar = NULL;
	}

	encoder->codecs &= ~FREERDP_CODEC_PLANAR;
	return 1;
}

static int shadow_encoder_uninit_interleaved(rdpShadowEncoder* encoder)
{
	if (encoder->interleaved)
	{
		bitmap_interleaved_context_free(encoder->interleaved);
		encoder->interleaved = NULL;
	}

	encoder->codecs &= ~FREERDP_CODEC_INTERLEAVED;
	return 1;
}

static int shadow_encoder_uninit_h264(rdpShadowEncoder* encoder)
{
	if (encoder->h264)
	{
		h264_context_free(encoder->h264);
		encoder->h264 = NULL;
	}

	encoder->codecs &= ~(FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444);
	return 1;
}

static int shadow_encoder_uninit(rdpShadowEncoder* encoder)
{
	shadow_encoder_uninit_grid(encoder);

	if (encoder->bs)
	{
		Stream_Free(encoder->bs, TRUE);
		encoder->bs = NULL;
	}

	if (encoder->codecs & FREERDP_CODEC_REMOTEFX)
	{
		shadow_encoder_uninit_rfx(encoder);
	}

	if (encoder->codecs & FREERDP_CODEC_NSCODEC)
	{
		shadow_encoder_uninit_nsc(encoder);
	}

	if (encoder->codecs & FREERDP_CODEC_PLANAR)
	{
		shadow_encoder_uninit_planar(encoder);
	}

	if (encoder->codecs & FREERDP_CODEC_INTERLEAVED)
	{
		shadow_encoder_uninit_interleaved(encoder);
	}

	if (encoder->codecs & (FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444))
	{
		shadow_encoder_uninit_h264(encoder);
	}

	return 1;
}

int shadow_encoder_reset(rdpShadowEncoder* encoder)
{
	int status;
	UINT32 codecs = encoder->codecs;
	rdpContext* context = (rdpContext*) encoder->client;
	rdpSettings* settings = context->settings;
	status = shadow_encoder_uninit(encoder);

	if (status < 0)
		return -1;

	status = shadow_encoder_init(encoder);

	if (status < 0)
		return -1;

	status = shadow_encoder_prepare(encoder, codecs);

	if (status < 0)
		return -1;

	encoder->fps = 16;
	encoder->maxFps = 32;
	encoder->frameId = 0;
	encoder->lastAckframeId = 0;
	encoder->frameAck = settings->SurfaceFrameMarkerEnabled;
	return 1;
}

int shadow_encoder_prepare(rdpShadowEncoder* encoder, UINT32 codecs)
{
	int status;

	if ((codecs & FREERDP_CODEC_REMOTEFX)
	    && !(encoder->codecs & FREERDP_CODEC_REMOTEFX))
	{
		status = shadow_encoder_init_rfx(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & FREERDP_CODEC_NSCODEC)
	    && !(encoder->codecs & FREERDP_CODEC_NSCODEC))
	{
		status = shadow_encoder_init_nsc(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & FREERDP_CODEC_PLANAR)
	    && !(encoder->codecs & FREERDP_CODEC_PLANAR))
	{
		status = shadow_encoder_init_planar(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & FREERDP_CODEC_INTERLEAVED)
	    && !(encoder->codecs & FREERDP_CODEC_INTERLEAVED))
	{
		status = shadow_encoder_init_interleaved(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & (FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444))
	    && !(encoder->codecs & (FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444)))
	{
		status = shadow_encoder_init_h264(encoder);

		if (status < 0)
			return -1;
	}

	return 1;
}

rdpShadowEncoder* shadow_encoder_new(rdpShadowClient* client)
{
	rdpShadowEncoder* encoder;
	rdpShadowServer* server = client->server;
	encoder = (rdpShadowEncoder*) calloc(1, sizeof(rdpShadowEncoder));

	if (!encoder)
		return NULL;

	encoder->client = client;
	encoder->server = server;
	encoder->fps = 16;
	encoder->maxFps = 32;

	if (shadow_encoder_init(encoder) < 0)
	{
		free(encoder);
		return NULL;
	}

	return encoder;
}

void shadow_encoder_free(rdpShadowEncoder* encoder)
{
	if (!encoder)
		return;

	shadow_encoder_uninit(encoder);
	free(encoder);
}
