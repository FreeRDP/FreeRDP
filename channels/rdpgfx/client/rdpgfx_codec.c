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
	RECTANGLE_16* regionRect = nullptr;
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
		regionRect = &(meta->regionRects[index]);

		if ((error = rdpgfx_read_rect16(gfx->base.log, s, regionRect)))
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

WINPR_ATTR_NODISCARD
static BOOL checkSurfaceCommand(const RDPGFX_PLUGIN* gfx, const RDPGFX_SURFACE_COMMAND* cmd)
{
	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_UNCOMPRESSED:
		case RDPGFX_CODECID_CAVIDEO:
		case RDPGFX_CODECID_CLEARCODEC:
		case RDPGFX_CODECID_PLANAR:
		case RDPGFX_CODECID_AVC420:
		case RDPGFX_CODECID_ALPHA:
		case RDPGFX_CODECID_AVC444:
		case RDPGFX_CODECID_AVC444v2:
		case RDPGFX_CODECID_CAPROGRESSIVE:
		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			return TRUE;
#if defined(WITH_GFX_AV1)
		case RDPGFX_CODECID_AV1:
			if (gfx->ConnectionCaps.version != RDPGFX_CAPVERSION_FRDP_1)
			{
				WLog_Print(gfx->base.log, WLOG_ERROR,
				           "RDPGFX_SURFACE_COMMAND::codecId %" PRIu32
				           " only supported with %s but connection uses %s [0x%08" PRIx32 "]",
				           cmd->codecId, rdpgfx_caps_version_str(RDPGFX_CAPVERSION_FRDP_1),
				           rdpgfx_caps_version_str(gfx->ConnectionCaps.version),
				           gfx->ConnectionCaps.version);
				return FALSE;
			}
			return TRUE;
#endif
		default:
			WLog_Print(gfx->base.log, WLOG_ERROR,
			           "Unknown RDPGFX_SURFACE_COMMAND::codecId %" PRIu32, cmd->codecId);
			return FALSE;
	}
}

static UINT logSurfaceCommand(RDPGFX_PLUGIN* gfx, const RDPGFX_SURFACE_COMMAND* cmd)
{
	WINPR_ASSERT(gfx);

	WLog_Print(gfx->base.log, WLOG_DEBUG, "Got GFX %s",
	           rdpgfx_get_codec_id_string(WINPR_ASSERTING_INT_CAST(UINT16, cmd->codecId)));

	RdpgfxClientContext* context = gfx->context;
	if (!context)
		return CHANNEL_RC_OK;

	if (!checkSurfaceCommand(gfx, cmd))
		return CHANNEL_RC_NULL_DATA;

	const UINT error = IFCALLRESULT(CHANNEL_RC_OK, context->SurfaceCommand, context, cmd);

	if (error)
		WLog_Print(gfx->base.log, WLOG_ERROR,
		           "context->SurfaceCommand failed with error %" PRIu32 "", error);
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

		if ((bitstreamLen > UINT32_MAX) ||
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
	PROFILER_ENTER(context->SurfaceProfiler)

	switch (cmd->codecId)
	{
#if defined(WITH_GFX_AV1)
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

		default:
			error = logSurfaceCommand(gfx, cmd);
			break;
	}

	PROFILER_EXIT(context->SurfaceProfiler)
	return error;
}
