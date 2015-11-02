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

#include "rdpgfx_common.h"

#include "rdpgfx_codec.h"

#define TAG CHANNELS_TAG("rdpgfx.client")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_read_h264_metablock(RDPGFX_PLUGIN* gfx, wStream* s, RDPGFX_H264_METABLOCK* meta)
{
	UINT32 index;
	RDPGFX_RECT16* regionRect;
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

	meta->regionRects = (RDPGFX_RECT16*) malloc(meta->numRegionRects * sizeof(RDPGFX_RECT16));

	if (!meta->regionRects)
	{
		WLog_ERR(TAG, "malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	meta->quantQualityVals = (RDPGFX_H264_QUANT_QUALITY*) malloc(meta->numRegionRects * sizeof(RDPGFX_H264_QUANT_QUALITY));

	if (!meta->quantQualityVals)
	{
		WLog_ERR(TAG, "malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	WLog_DBG(TAG, "H264_METABLOCK: numRegionRects: %d", (int) meta->numRegionRects);

	for (index = 0; index < meta->numRegionRects; index++)
	{
		regionRect = &(meta->regionRects[index]);
		if ((error = rdpgfx_read_rect16(s, regionRect)))
		{
			WLog_ERR(TAG, "rdpgfx_read_rect16 failed with error %lu!", error);
			goto error_out;
		}
		WLog_DBG(TAG, "regionRects[%d]: left: %d top: %d right: %d bottom: %d",
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
		WLog_DBG(TAG, "quantQualityVals[%d]: qp: %d r: %d p: %d qualityVal: %d",
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
static UINT rdpgfx_decode_h264(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT error;
	wStream* s;
	RDPGFX_H264_BITMAP_STREAM h264;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	s = Stream_New(cmd->data, cmd->length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if ((error = rdpgfx_read_h264_metablock(gfx, s, &(h264.meta))))
	{
		WLog_ERR(TAG, "rdpgfx_read_h264_metablock failed with error %lu!", error);
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
			WLog_ERR(TAG, "context->SurfaceCommand failed with error %lu", error);
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
UINT rdpgfx_decode(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT error = CHANNEL_RC_OK;
	RdpgfxClientContext* context = (RdpgfxClientContext*) gfx->iface.pInterface;

	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_H264:
			if ((error = rdpgfx_decode_h264(gfx, cmd)))
			{
				WLog_ERR(TAG, "rdpgfx_decode_h264 failed with error %lu", error);
				return error;
			}
			break;

		default:
			if (context)
			{
				IFCALLRET(context->SurfaceCommand, error, context, cmd);
				if (error)
					WLog_ERR(TAG, "context->SurfaceCommand failed with error %lu", error);
			}
			break;
	}

	return error;
}
