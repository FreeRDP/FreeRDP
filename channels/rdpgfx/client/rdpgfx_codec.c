/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
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

#include <winpr/crt.h>
#include <winpr/stream.h>

#include "rdpgfx_common.h"

#include "rdpgfx_codec.h"

int rdpgfx_decode_uncompressed(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_remotefx(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_clear(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_planar(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_read_h264_metablock(RDPGFX_PLUGIN* gfx, wStream* s)
{
	UINT32 index;
	RDPGFX_RECT16* regionRect;
	RDPGFX_H264_METABLOCK metablock;
	RDPGFX_H264_QUANT_QUALITY* quantQualityVal;

	Stream_Read_UINT32(s, metablock.numRegionRects); /* numRegionRects (4 bytes) */

	metablock.regionRects = (RDPGFX_RECT16*) malloc(metablock.numRegionRects * sizeof(RDPGFX_RECT16));

	if (!metablock.regionRects)
		return -1;

	metablock.quantQualityVals = (RDPGFX_H264_QUANT_QUALITY*) malloc(metablock.numRegionRects * sizeof(RDPGFX_H264_QUANT_QUALITY));

	if (!metablock.quantQualityVals)
		return -1;

	printf("H264_METABLOCK: numRegionRects: %d\n", (int) metablock.numRegionRects);

	for (index = 0; index < metablock.numRegionRects; index++)
	{
		regionRect = &(metablock.regionRects[index]);
		rdpgfx_read_rect16(s, regionRect);

		printf("regionRects[%d]: left: %d top: %d right: %d bottom: %d\n",
				index, regionRect->left, regionRect->top, regionRect->right, regionRect->bottom);
	}

	for (index = 0; index < metablock.numRegionRects; index++)
	{
		quantQualityVal = &(metablock.quantQualityVals[index]);
		Stream_Read_UINT8(s, quantQualityVal->qpVal); /* qpVal (1 byte) */
		Stream_Read_UINT8(s, quantQualityVal->qualityVal); /* qualityVal (1 byte) */

		quantQualityVal->qp = quantQualityVal->qpVal & 0x3F;
		quantQualityVal->r = (quantQualityVal->qpVal >> 6) & 1;
		quantQualityVal->p = (quantQualityVal->qpVal >> 7) & 1;

		printf("quantQualityVals[%d]: qp: %d r: %d p: %d qualityVal: %d\n",
				index, quantQualityVal->qp, quantQualityVal->r, quantQualityVal->p, quantQualityVal->qualityVal);
	}

	free(metablock.regionRects);
	free(metablock.quantQualityVals);

	return 1;
}

int rdpgfx_decode_h264(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	wStream* s;

	s = Stream_New(cmd->data, cmd->length);

	rdpgfx_read_h264_metablock(gfx, s);

	Stream_Free(s, FALSE);

	return 1;
}

int rdpgfx_decode_alpha(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode_progressive(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	return 1;
}

int rdpgfx_decode(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	int status;

	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_UNCOMPRESSED:
			status = rdpgfx_decode_uncompressed(gfx, cmd);
			break;

		case RDPGFX_CODECID_CAVIDEO:
			status = rdpgfx_decode_remotefx(gfx, cmd);
			break;

		case RDPGFX_CODECID_CLEARCODEC:
			status = rdpgfx_decode_clear(gfx, cmd);
			break;

		case RDPGFX_CODECID_PLANAR:
			status = rdpgfx_decode_planar(gfx, cmd);
			break;

		case RDPGFX_CODECID_H264:
			status = rdpgfx_decode_h264(gfx, cmd);
			break;

		case RDPGFX_CODECID_ALPHA:
			status = rdpgfx_decode_alpha(gfx, cmd);
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE:
			status = rdpgfx_decode_progressive(gfx, cmd);
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			break;
	}

	return 1;
}
