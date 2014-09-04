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

int shadow_encoder_create_frame_id(rdpShadowEncoder* encoder)
{
	UINT32 frameId;
	int inFlightFrames;
	SURFACE_FRAME* frame;

	inFlightFrames = ListDictionary_Count(encoder->frameList);

	if (inFlightFrames > encoder->frameAck)
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

	frame = (SURFACE_FRAME*) malloc(sizeof(SURFACE_FRAME));

	if (!frame)
		return -1;

	frameId = frame->frameId = ++encoder->frameId;
	ListDictionary_Add(encoder->frameList, (void*) (size_t) frame->frameId, frame);

	return (int) frame->frameId;
}

int shadow_encoder_init_grid(rdpShadowEncoder* encoder)
{
	int i, j, k;
	int tileSize;
	int tileCount;

	encoder->gridWidth = ((encoder->width + (encoder->maxTileWidth - 1)) / encoder->maxTileWidth);
	encoder->gridHeight = ((encoder->height + (encoder->maxTileHeight - 1)) / encoder->maxTileHeight);

	tileSize = encoder->maxTileWidth * encoder->maxTileHeight * 4;
	tileCount = encoder->gridWidth * encoder->gridHeight;

	encoder->gridBuffer = (BYTE*) malloc(tileSize * tileCount);

	if (!encoder->gridBuffer)
		return -1;

	encoder->grid = (BYTE**) malloc(tileCount * sizeof(BYTE*));

	if (!encoder->grid)
		return -1;

	for (i = 0; i < encoder->gridHeight; i++)
	{
		for (j = 0; j < encoder->gridWidth; j++)
		{
			k = (i * encoder->gridHeight) + j;
			encoder->grid[k] = &(encoder->gridBuffer[k * tileSize]);
		}
	}

	return 0;
}

int shadow_encoder_uninit_grid(rdpShadowEncoder* encoder)
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

int shadow_encoder_init_rfx(rdpShadowEncoder* encoder)
{
	if (!encoder->rfx)
		encoder->rfx = rfx_context_new(TRUE);

	if (!encoder->rfx)
		return -1;

	encoder->rfx->mode = RLGR3;
	encoder->rfx->width = encoder->width;
	encoder->rfx->height = encoder->height;

	rfx_context_set_pixel_format(encoder->rfx, RDP_PIXEL_FORMAT_B8G8R8A8);

	if (!encoder->frameList)
	{
		encoder->fps = 16;
		encoder->maxFps = 32;
		encoder->frameId = 0;
		encoder->frameAck = TRUE;
		encoder->frameList = ListDictionary_New(TRUE);
	}

	encoder->codecs |= SHADOW_CODEC_REMOTEFX;

	return 1;
}

int shadow_encoder_init_nsc(rdpShadowEncoder* encoder)
{
	if (!encoder->nsc)
		encoder->nsc = nsc_context_new();

	if (!encoder->nsc)
		return -1;

	nsc_context_set_pixel_format(encoder->nsc, RDP_PIXEL_FORMAT_B8G8R8A8);

	if (!encoder->frameList)
	{
		encoder->fps = 16;
		encoder->maxFps = 32;
		encoder->frameId = 0;
		encoder->frameAck = TRUE;
		encoder->frameList = ListDictionary_New(TRUE);
	}

	encoder->codecs |= SHADOW_CODEC_NSCODEC;

	return 1;
}

int shadow_encoder_init_bitmap(rdpShadowEncoder* encoder)
{
	DWORD planarFlags;

	planarFlags = PLANAR_FORMAT_HEADER_NA;
	planarFlags |= PLANAR_FORMAT_HEADER_RLE;

	if (!encoder->planar)
	{
		encoder->planar = freerdp_bitmap_planar_context_new(planarFlags,
				encoder->maxTileWidth, encoder->maxTileHeight);
	}

	if (!encoder->planar)
		return -1;

	if (!encoder->bts)
		encoder->bts = Stream_New(NULL, encoder->maxTileWidth * encoder->maxTileHeight * 4);

	if (!encoder->bts)
		return -1;

	encoder->codecs |= SHADOW_CODEC_BITMAP;

	return 1;
}

int shadow_encoder_init(rdpShadowEncoder* encoder)
{
	encoder->maxTileWidth = 64;
	encoder->maxTileHeight = 64;

	shadow_encoder_init_grid(encoder);

	if (!encoder->bs)
		encoder->bs = Stream_New(NULL, encoder->maxTileWidth * encoder->maxTileHeight * 4);

	if (!encoder->bs)
		return -1;

	return 1;
}

int shadow_encoder_uninit_rfx(rdpShadowEncoder* encoder)
{
	if (encoder->rfx)
	{
		rfx_context_free(encoder->rfx);
		encoder->rfx = NULL;
	}

	if (encoder->frameList)
	{
		ListDictionary_Free(encoder->frameList);
		encoder->frameList = NULL;
	}

	encoder->codecs &= ~SHADOW_CODEC_REMOTEFX;

	return 1;
}

int shadow_encoder_uninit_nsc(rdpShadowEncoder* encoder)
{
	if (encoder->nsc)
	{
		nsc_context_free(encoder->nsc);
		encoder->nsc = NULL;
	}

	if (encoder->frameList)
	{
		ListDictionary_Free(encoder->frameList);
		encoder->frameList = NULL;
	}

	encoder->codecs &= ~SHADOW_CODEC_NSCODEC;

	return 1;
}

int shadow_encoder_uninit_bitmap(rdpShadowEncoder* encoder)
{
	if (encoder->planar)
	{
		freerdp_bitmap_planar_context_free(encoder->planar);
		encoder->planar = NULL;
	}

	if (encoder->bts)
	{
		Stream_Free(encoder->bts, TRUE);
		encoder->bts = NULL;
	}

	encoder->codecs &= ~SHADOW_CODEC_BITMAP;

	return 1;
}

int shadow_encoder_uninit(rdpShadowEncoder* encoder)
{
	shadow_encoder_uninit_grid(encoder);

	if (encoder->bs)
	{
		Stream_Free(encoder->bs, TRUE);
		encoder->bs = NULL;
	}

	if (encoder->codecs & SHADOW_CODEC_REMOTEFX)
	{
		shadow_encoder_uninit_rfx(encoder);
	}

	if (encoder->codecs & SHADOW_CODEC_NSCODEC)
	{
		shadow_encoder_uninit_nsc(encoder);
	}

	if (encoder->codecs & SHADOW_CODEC_BITMAP)
	{
		shadow_encoder_uninit_bitmap(encoder);
	}

	return 1;
}

int shadow_encoder_reset(rdpShadowEncoder* encoder)
{
	int status;
	UINT32 codecs = encoder->codecs;

	status = shadow_encoder_uninit(encoder);

	if (status < 0)
		return -1;

	status = shadow_encoder_init(encoder);

	if (status < 0)
		return -1;

	status = shadow_encoder_prepare(encoder, codecs);

	if (status < 0)
		return -1;

	return 1;
}

int shadow_encoder_prepare(rdpShadowEncoder* encoder, UINT32 codecs)
{
	int status;

	if ((codecs & SHADOW_CODEC_REMOTEFX) && !(encoder->codecs & SHADOW_CODEC_REMOTEFX))
	{
		status = shadow_encoder_init_rfx(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & SHADOW_CODEC_NSCODEC) && !(encoder->codecs & SHADOW_CODEC_NSCODEC))
	{
		status = shadow_encoder_init_nsc(encoder);

		if (status < 0)
			return -1;
	}

	if ((codecs & SHADOW_CODEC_BITMAP) && !(encoder->codecs & SHADOW_CODEC_BITMAP))
	{
		status = shadow_encoder_init_bitmap(encoder);

		if (status < 0)
			return -1;
	}

	return 1;
}

rdpShadowEncoder* shadow_encoder_new(rdpShadowServer* server)
{
	rdpShadowEncoder* encoder;

	encoder = (rdpShadowEncoder*) calloc(1, sizeof(rdpShadowEncoder));

	if (!encoder)
		return NULL;

	encoder->server = server;

	encoder->fps = 16;
	encoder->maxFps = 32;

	encoder->width = server->screen->width;
	encoder->height = server->screen->height;

	if (shadow_encoder_init(encoder) < 0)
		return NULL;

	return encoder;
}

void shadow_encoder_free(rdpShadowEncoder* encoder)
{
	if (!encoder)
		return;

	shadow_encoder_uninit(encoder);

	free(encoder);
}
