/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <freerdp/log.h>
#include <freerdp/utils/profiler.h>

#include "rdpgfx_common.h"

#include "rdpgfx_codec.h"

#define TAG CHANNELS_TAG("rdpgfx.client")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_read_h264_metablock(RDPGFX_PLUGIN* gfx, wStream* s,
                                       RDPGFX_H264_METABLOCK* meta)
{
	UINT32 index;
	RECTANGLE_16* regionRect;
	RDPGFX_H264_QUANT_QUALITY* quantQualityVal;
	UINT error = ERROR_INVALID_DATA;
	meta->regionRects = NULL;
	meta->quantQualityVals = NULL;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG, "not enough data!");
		goto error_out;
	}

	Stream_Read_UINT32(s, meta->numRegionRects); /* numRegionRects (4 bytes) */

	if (Stream_GetRemainingLength(s) < (meta->numRegionRects * 8))
	{
		WLog_ERR(TAG, "not enough data!");
		goto error_out;
	}

	meta->regionRects = (RECTANGLE_16*) calloc(meta->numRegionRects, sizeof(RECTANGLE_16));

	if (!meta->regionRects)
	{
		WLog_ERR(TAG, "malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	meta->quantQualityVals = (RDPGFX_H264_QUANT_QUALITY*) calloc(meta->numRegionRects, sizeof(
	                             RDPGFX_H264_QUANT_QUALITY));

	if (!meta->quantQualityVals)
	{
		WLog_ERR(TAG, "malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	WLog_DBG(TAG, "H264_METABLOCK: numRegionRects: %"PRIu32"", meta->numRegionRects);

	for (index = 0; index < meta->numRegionRects; index++)
	{
		regionRect = &(meta->regionRects[index]);

		if ((error = rdpgfx_read_rect16(s, regionRect)))
		{
			WLog_ERR(TAG, "rdpgfx_read_rect16 failed with error %"PRIu32"!", error);
			goto error_out;
		}

		WLog_DBG(TAG,
		         "regionRects[%"PRIu32"]: left: %"PRIu16" top: %"PRIu16" right: %"PRIu16" bottom: %"PRIu16"",
		         index, regionRect->left, regionRect->top, regionRect->right, regionRect->bottom);
	}

	if (Stream_GetRemainingLength(s) < (meta->numRegionRects * 2))
	{
		WLog_ERR(TAG, "not enough data!");
		error = ERROR_INVALID_DATA;
		goto error_out;
	}

	for (index = 0; index < meta->numRegionRects; index++)
	{
		quantQualityVal = &(meta->quantQualityVals[index]);
		Stream_Read_UINT8(s, quantQualityVal->qpVal); /* qpVal (1 byte) */
		Stream_Read_UINT8(s, quantQualityVal->qualityVal); /* qualityVal (1 byte) */
		quantQualityVal->qp = quantQualityVal->qpVal & 0x3F;
		quantQualityVal->r = (quantQualityVal->qpVal >> 6) & 1;
		quantQualityVal->p = (quantQualityVal->qpVal >> 7) & 1;
		WLog_DBG(TAG,
		         "quantQualityVals[%"PRIu32"]: qp: %"PRIu8" r: %"PRIu8" p: %"PRIu8" qualityVal: %"PRIu8"",
		         index, quantQualityVal->qp, quantQualityVal->r, quantQualityVal->p, quantQualityVal->qualityVal);
	}

	return CHANNEL_RC_OK;
error_out:
	free(meta->regionRects);
	meta->regionRects = NULL;
	free(meta->quantQualityVals);
	meta->quantQualityVals = NULL;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_decode_AVC420(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT error;
	wStream* s;
	RDPGFX_AVC420_BITMAP_STREAM h264;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	s = Stream_New(cmd->data, cmd->length);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_read_h264_metablock(gfx, s, &(h264.meta))))
	{
		WLog_ERR(TAG, "rdpgfx_read_h264_metablock failed with error %"PRIu32"!", error);
		return error;
	}

	h264.data = Stream_Pointer(s);
	h264.length = (UINT32) Stream_GetRemainingLength(s);
	Stream_Free(s, FALSE);
	cmd->extra = (void*) &h264;

	if (context)
	{
		IFCALLRET(context->SurfaceCommand, error, context, cmd);

		if (error)
			WLog_ERR(TAG, "context->SurfaceCommand failed with error %"PRIu32"", error);
	}

	free(h264.meta.regionRects);
	free(h264.meta.quantQualityVals);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_decode_AVC444(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT error;
	UINT32 tmp;
	size_t pos1, pos2;
	wStream* s;
	RDPGFX_AVC444_BITMAP_STREAM h264 = { 0 };
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	s = Stream_New(cmd->data, cmd->length);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		error = ERROR_INVALID_DATA;
		goto fail;
	}
	Stream_Read_UINT32(s, tmp);
	h264.cbAvc420EncodedBitstream1 = tmp & 0x3FFFFFFFUL;
	h264.LC = (tmp >> 30UL) & 0x03UL;

	if (h264.LC == 0x03)
	{
		error = ERROR_INVALID_DATA;
		goto fail;
	}

	pos1 = Stream_GetPosition(s);

	if ((error = rdpgfx_read_h264_metablock(gfx, s, &(h264.bitstream[0].meta))))
	{
		WLog_ERR(TAG, "rdpgfx_read_h264_metablock failed with error %"PRIu32"!", error);
		goto fail;
	}

	pos2 = Stream_GetPosition(s);
	h264.bitstream[0].data = Stream_Pointer(s);

	if (h264.LC == 0)
	{
		tmp = h264.cbAvc420EncodedBitstream1 - pos2 + pos1;

		if (Stream_GetRemainingLength(s) < tmp)
		{
			error = ERROR_INVALID_DATA;
			goto fail;
		}

		h264.bitstream[0].length = tmp;
		Stream_Seek(s, tmp);

		if ((error = rdpgfx_read_h264_metablock(gfx, s, &(h264.bitstream[1].meta))))
		{
			WLog_ERR(TAG, "rdpgfx_read_h264_metablock failed with error %"PRIu32"!", error);
			goto fail;
		}

		h264.bitstream[1].data = Stream_Pointer(s);
		h264.bitstream[1].length = Stream_GetRemainingLength(s);
	}
	else
	{
		h264.bitstream[0].length = Stream_GetRemainingLength(s);
		memset(&h264.bitstream[1], 0, sizeof(h264.bitstream[1]));
	}

	cmd->extra = (void*) &h264;

	if (context)
	{
		IFCALLRET(context->SurfaceCommand, error, context, cmd);

		if (error)
			WLog_ERR(TAG, "context->SurfaceCommand failed with error %"PRIu32"", error);
	}

fail:
	Stream_Free(s, FALSE);
	free(h264.bitstream[0].meta.regionRects);
	free(h264.bitstream[0].meta.quantQualityVals);
	free(h264.bitstream[1].meta.regionRects);
	free(h264.bitstream[1].meta.quantQualityVals);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT rdpgfx_decode(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT error = CHANNEL_RC_OK;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;
	PROFILER_ENTER(context->SurfaceProfiler)

	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_AVC420:
			if ((error = rdpgfx_decode_AVC420(gfx, cmd)))
				WLog_ERR(TAG, "rdpgfx_decode_AVC420 failed with error %"PRIu32"", error);

			break;

		case RDPGFX_CODECID_AVC444:
		case RDPGFX_CODECID_AVC444v2:
			if ((error = rdpgfx_decode_AVC444(gfx, cmd)))
				WLog_ERR(TAG, "rdpgfx_decode_AVC444 failed with error %"PRIu32"", error);

			break;

		default:
			if (context)
			{
				IFCALLRET(context->SurfaceCommand, error, context, cmd);

				if (error)
					WLog_ERR(TAG, "context->SurfaceCommand failed with error %"PRIu32"", error);
			}

			break;
	}

	PROFILER_EXIT(context->SurfaceProfiler)
	return error;
}
