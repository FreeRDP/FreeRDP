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

#include "shadow_encoder.h"

int shadow_encoder_grid_init(rdpShadowEncoder* encoder)
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

int shadow_encoder_grid_uninit(rdpShadowEncoder* encoder)
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

rdpShadowEncoder* shadow_encoder_new(rdpShadowServer* server)
{
	DWORD planarFlags;
	rdpShadowEncoder* encoder;

	encoder = (rdpShadowEncoder*) calloc(1, sizeof(rdpShadowEncoder));

	if (!encoder)
		return NULL;

	encoder->server = server;

	encoder->width = 1024;
	encoder->height = 768;

	encoder->bitsPerPixel = 32;
	encoder->bytesPerPixel = 4;
	encoder->scanline = (encoder->width + (encoder->width % 4)) * 4;

	encoder->data = (BYTE*) malloc(encoder->scanline * encoder->height);

	if (!encoder->data)
		return NULL;

	encoder->maxTileWidth = 64;
	encoder->maxTileHeight = 64;

	encoder->bs = Stream_New(NULL, encoder->maxTileWidth * encoder->maxTileHeight * 4);
	encoder->bts = Stream_New(NULL, encoder->maxTileWidth * encoder->maxTileHeight * 4);

	planarFlags = PLANAR_FORMAT_HEADER_NA;
	planarFlags |= PLANAR_FORMAT_HEADER_RLE;

	encoder->planar = freerdp_bitmap_planar_context_new(planarFlags,
			encoder->maxTileWidth, encoder->maxTileHeight);

	encoder->rfx = rfx_context_new(TRUE);
	encoder->rfx_s = Stream_New(NULL, encoder->maxTileWidth * encoder->maxTileHeight * 4);

	encoder->rfx->mode = RLGR3;
	encoder->rfx->width = encoder->width;
	encoder->rfx->height = encoder->height;

	encoder->nsc = nsc_context_new();
	encoder->nsc_s = Stream_New(NULL, encoder->maxTileWidth * encoder->maxTileHeight * 4);

	rfx_context_set_pixel_format(encoder->rfx, RDP_PIXEL_FORMAT_B8G8R8A8);
	nsc_context_set_pixel_format(encoder->nsc, RDP_PIXEL_FORMAT_B8G8R8A8);

	shadow_encoder_grid_init(encoder);

	return encoder;
}

void shadow_encoder_free(rdpShadowEncoder* encoder)
{
	if (!encoder)
		return;

	Stream_Free(encoder->bs, TRUE);
	Stream_Free(encoder->bts, TRUE);

	Stream_Free(encoder->rfx_s, TRUE);
	rfx_context_free(encoder->rfx);

	Stream_Free(encoder->nsc_s, TRUE);
	nsc_context_free(encoder->nsc);

	freerdp_bitmap_planar_context_free(encoder->planar);

	shadow_encoder_grid_uninit(encoder);

	free(encoder);
}
