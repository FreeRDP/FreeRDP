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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <freerdp/log.h>
#include <freerdp/utils/profiler.h>

#include "rdpgfx_common.h"

#include "rdpgfx_codec.h"

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_read_h264_metablock(WINPR_ATTR_UNUSED RDPGFX_PLUGIN* gfx, wStream* s,
                                       RDPGFX_H264_METABLOCK* meta)
{
	RDPGFX_H264_QUANT_QUALITY* quantQualityVal = nullptr;
	UINT error = ERROR_INVALID_DATA;
	meta->regionRects = nullptr;
	meta->quantQualityVals = nullptr;

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 4))
		goto error_out;

	Stream_Read_UINT32(s, meta->numRegionRects); /* numRegionRects (4 bytes) */

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(gfx->base.log, s, meta->numRegionRects, 8ull))
		goto error_out;

	meta->regionRects = (RECTANGLE_16*)calloc(meta->numRegionRects, sizeof(RECTANGLE_16));

	if (!meta->regionRects)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	meta->quantQualityVals =
	    (RDPGFX_H264_QUANT_QUALITY*)calloc(meta->numRegionRects, sizeof(RDPGFX_H264_QUANT_QUALITY));

	if (!meta->quantQualityVals)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "malloc failed!");
		error = CHANNEL_RC_NO_MEMORY;
		goto error_out;
	}

	WLog_Print(gfx->base.log, WLOG_TRACE, "H264_METABLOCK: numRegionRects: %" PRIu32 "",
	           meta->numRegionRects);

	for (UINT32 index = 0; index < meta->numRegionRects; index++)
	{
		RECTANGLE_16* regionRect = &(meta->regionRects[index]);

		if ((error = rdpgfx_read_rect16(gfx->base.log, s, regionRect, FALSE)))
		{
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "rdpgfx_read_rect16 failed with error %" PRIu32 "!", error);
			goto error_out;
		}

		WLog_Print(gfx->base.log, WLOG_TRACE,
		           "regionRects[%" PRIu32 "]: left: %" PRIu16 " top: %" PRIu16 " right: %" PRIu16
		           " bottom: %" PRIu16 "",
		           index, regionRect->left, regionRect->top, regionRect->right, regionRect->bottom);
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(gfx->base.log, s, meta->numRegionRects, 2ull))
	{
		error = ERROR_INVALID_DATA;
		goto error_out;
	}

	for (UINT32 index = 0; index < meta->numRegionRects; index++)
	{
		quantQualityVal = &(meta->quantQualityVals[index]);
		Stream_Read_UINT8(s, quantQualityVal->qpVal);      /* qpVal (1 byte) */
		Stream_Read_UINT8(s, quantQualityVal->qualityVal); /* qualityVal (1 byte) */
		quantQualityVal->qp = quantQualityVal->qpVal & 0x3F;
		quantQualityVal->r = (quantQualityVal->qpVal >> 6) & 1;
		quantQualityVal->p = (quantQualityVal->qpVal >> 7) & 1;
		WLog_Print(gfx->base.log, WLOG_TRACE,
		           "quantQualityVals[%" PRIu32 "]: qp: %" PRIu8 " r: %" PRIu8 " p: %" PRIu8
		           " qualityVal: %" PRIu8 "",
		           index, quantQualityVal->qp, quantQualityVal->r, quantQualityVal->p,
		           quantQualityVal->qualityVal);
	}

	return CHANNEL_RC_OK;
error_out:
	free_h264_metablock(meta);
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_decode_AVC420(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	RDPGFX_AVC420_BITMAP_STREAM h264 = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_New(cmd->data, cmd->length);

	if (!s)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	UINT error = rdpgfx_read_h264_metablock(gfx, s, &(h264.meta));
	if (error != CHANNEL_RC_OK)
	{
		Stream_Free(s, FALSE);
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "rdpgfx_read_h264_metablock failed with error %" PRIu32 "!", error);
		return error;
	}

	h264.data = Stream_Pointer(s);
	h264.length = (UINT32)Stream_GetRemainingLength(s);
	Stream_Free(s, FALSE);
	cmd->extra = (void*)&h264;

	error = logSurfaceCommand(gfx, cmd);

	free_h264_metablock(&h264.meta);
	cmd->extra = nullptr;
	return error;
}

#if defined(WITH_GFX_AZURE)
/**
 * HEVC420 (undocumented) uses the AVC420 metablock, then a table of the NAL unit sizes and
 * the Annex B bitstream:
 *
 *   RDPGFX_H264_METABLOCK
 *   UINT32 cbNalSizes        size of the following table in bytes, 4 to 2048 (max 512 NAL units)
 *   UINT32 nalSize[cbNalSizes / 4]
 *   Annex B NAL units, each nalSize[i] bytes including its start code
 *
 * The table is checked against the bitstream and dropped, decoders get the Annex B data.
 *
 * With RDPGFX_CAPVERSION_113 and RDPGFX_CAPVERSION_115 a BYTE frameAttributes (bit 0: key
 * frame) follows the table.
 *
 * @return 0 on success, otherwise a Win32 error code
 */
WINPR_ATTR_NODISCARD
static UINT rdpgfx_decode_HEVC(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	RDPGFX_HEVC_BITMAP_STREAM hevc = WINPR_C_ARRAY_INIT;
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, cmd->data, cmd->length);

	UINT error = rdpgfx_read_h264_metablock(gfx, s, &(hevc.meta));
	if (error != CHANNEL_RC_OK)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "rdpgfx_read_h264_metablock failed with error %" PRIu32 "!", error);
		return error;
	}

	error = ERROR_INVALID_DATA;
	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 4))
		goto fail;

	hevc.cbNalSizes = Stream_Get_UINT32(s);
	if ((hevc.cbNalSizes == 0) || (hevc.cbNalSizes > 2048) || ((hevc.cbNalSizes % 4) != 0) ||
	    !Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, hevc.cbNalSizes))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "HEVC420 invalid NAL size table length %" PRIu32,
		           hevc.cbNalSizes);
		goto fail;
	}

	size_t total = 0;
	hevc.nalSize = Stream_Pointer(s);
	for (UINT32 x = 0; x < hevc.cbNalSizes / sizeof(UINT32); x++)
		total += Stream_Get_UINT32(s);

	if ((gfx->ConnectionCaps.version == RDPGFX_CAPVERSION_113) ||
	    (gfx->ConnectionCaps.version == RDPGFX_CAPVERSION_115))
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 1))
			goto fail;
		hevc.frameAttributes = Stream_Get_UINT8(s);
		WLog_Print(gfx->base.log, WLOG_TRACE, "HEVC420 frameAttributes 0x%02" PRIx8 "%s",
		           hevc.frameAttributes, (hevc.frameAttributes & 0x01) ? " (key frame)" : "");
	}
	else
		hevc.frameAttributes = -1;

	const size_t remaining = Stream_GetRemainingLength(s);
	if ((total != remaining) || (remaining > UINT32_MAX))
	{
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "HEVC420 NAL sizes add up to %" PRIuz " bytes, but %" PRIuz " bytes follow",
		           total, remaining);
		goto fail;
	}

	hevc.data = Stream_Pointer(s);
	hevc.length = (UINT32)remaining;
	cmd->extra = (void*)&hevc;

	error = logSurfaceCommand(gfx, cmd);

fail:
	free_h264_metablock(&hevc.meta);
	cmd->extra = nullptr;
	return error;
}
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT rdpgfx_decode_AVC444(RDPGFX_PLUGIN* gfx, RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT error = CHANNEL_RC_OK;
	UINT32 tmp = 0;
	size_t pos1 = 0;
	size_t pos2 = 0;

	RDPGFX_AVC444_BITMAP_STREAM h264 = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_New(cmd->data, cmd->length);

	if (!s)
	{
		WLog_Print(gfx->base.log, WLOG_ERROR, "Stream_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, 4))
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
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "rdpgfx_read_h264_metablock failed with error %" PRIu32 "!", error);
		goto fail;
	}

	pos2 = Stream_GetPosition(s);
	h264.bitstream[0].data = Stream_Pointer(s);

	if (h264.LC == 0)
	{
		const size_t bitstreamLen = 1ULL * h264.cbAvc420EncodedBitstream1 - pos2 + pos1;

		if ((bitstreamLen > UINT32_MAX) || (h264.cbAvc420EncodedBitstream1 < pos2 - pos1) ||
		    !Stream_CheckAndLogRequiredLengthWLog(gfx->base.log, s, bitstreamLen))
		{
			error = ERROR_INVALID_DATA;
			goto fail;
		}

		h264.bitstream[0].length = (UINT32)bitstreamLen;
		Stream_Seek(s, bitstreamLen);

		if ((error = rdpgfx_read_h264_metablock(gfx, s, &(h264.bitstream[1].meta))))
		{
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "rdpgfx_read_h264_metablock failed with error %" PRIu32 "!", error);
			goto fail;
		}

		h264.bitstream[1].data = Stream_Pointer(s);

		const size_t len = Stream_GetRemainingLength(s);
		if (len > UINT32_MAX)
			goto fail;
		h264.bitstream[1].length = (UINT32)len;
	}
	else
	{
		const size_t len = Stream_GetRemainingLength(s);
		if (len > UINT32_MAX)
			goto fail;
		h264.bitstream[0].length = (UINT32)len;
	}

	cmd->extra = (void*)&h264;

	error = logSurfaceCommand(gfx, cmd);

fail:
	Stream_Free(s, FALSE);
	free_h264_metablock(&h264.bitstream[0].meta);
	free_h264_metablock(&h264.bitstream[1].meta);
	cmd->extra = nullptr;
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
	WINPR_ASSERT(gfx);
	WINPR_ASSERT(cmd);

	PROFILER_ENTER(gfx->context->SurfaceProfiler)

	switch (cmd->codecId)
	{
#if defined(WITH_GFX_AV1) || defined(WITH_GFX_AV2)
		case RDPGFX_CODECID_AV1:
#endif
		case RDPGFX_CODECID_AVC420:
			if ((error = rdpgfx_decode_AVC420(gfx, cmd)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_decode_AVC420 failed with error %" PRIu32 "", error);

			break;

		case RDPGFX_CODECID_AVC444:
		case RDPGFX_CODECID_AVC444v2:
			if ((error = rdpgfx_decode_AVC444(gfx, cmd)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_decode_AVC444 failed with error %" PRIu32 "", error);

			break;

#if defined(WITH_GFX_AZURE)
		case RDPGFX_CODECID_HEVC420:
		case RDPGFX_CODECID_HEVC444:
			if ((error = rdpgfx_decode_HEVC(gfx, cmd)))
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "rdpgfx_decode_HEVC failed with error %" PRIu32 "", error);

			break;
#endif

		default:
			error = logSurfaceCommand(gfx, cmd);
			break;
	}

	PROFILER_EXIT(gfx->context->SurfaceProfiler)
	return error;
}
