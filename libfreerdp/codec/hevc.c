/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * HEVC Bitmap Compression
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

#include <freerdp/codec/hevc.h>
#include <freerdp/primitives.h>
#include <freerdp/log.h>
#include <freerdp/codec/region.h>
#include <freerdp/codec/h264.h>

#include "h264.h"

#define TAG FREERDP_TAG("codec.hevc")

struct S_FREERDP_HEVC_CONTEXT
{
	BOOL Compressor;
	wLog* log;
	H264_CONTEXT* context;
};

WINPR_ATTR_NODISCARD
static H264_CONTEXT_OPTION hevc2h264option(FREERDP_HEVC_CONTEXT_OPTION option)
{
	switch (option)
	{
		case FREERDP_HEVC_CONTEXT_OPTION_PROFILE:
			return H264_CONTEXT_OPTION_HEVC444;
		case FREERDP_HEVC_CONTEXT_OPTION_RATECONTROL:
			return H264_CONTEXT_OPTION_RATECONTROL;
		case FREERDP_HEVC_CONTEXT_OPTION_BITRATE:
			return H264_CONTEXT_OPTION_BITRATE;
		case FREERDP_HEVC_CONTEXT_OPTION_USAGETYPE:
			return H264_CONTEXT_OPTION_USAGETYPE;
		case FREERDP_HEVC_CONTEXT_OPTION_HW_ACCEL:
			return H264_CONTEXT_OPTION_HW_ACCEL;
		default:
			return 0;
	}
}
BOOL freerdp_hevc_context_set_option(FREERDP_HEVC_CONTEXT* hevc, FREERDP_HEVC_CONTEXT_OPTION option,
                                     UINT32 value)
{
	WINPR_ASSERT(hevc);
	return h264_context_set_option(hevc->context, hevc2h264option(option), value);
}

UINT32 freerdp_hevc_context_get_option(FREERDP_HEVC_CONTEXT* hevc,
                                       FREERDP_HEVC_CONTEXT_OPTION option)
{
	WINPR_ASSERT(hevc);
	return h264_context_get_option(hevc->context, hevc2h264option(option));
}

INT32 freerdp_hevc_compress(WINPR_ATTR_UNUSED FREERDP_HEVC_CONTEXT* hevc,
                            WINPR_ATTR_UNUSED const BYTE* pSrcData,
                            WINPR_ATTR_UNUSED DWORD SrcFormat, WINPR_ATTR_UNUSED UINT32 nSrcStep,
                            WINPR_ATTR_UNUSED UINT32 nSrcWidth, WINPR_ATTR_UNUSED UINT32 nSrcHeight,
                            WINPR_ATTR_UNUSED const RECTANGLE_16* regionRect,
                            WINPR_ATTR_UNUSED BYTE** ppDstData, WINPR_ATTR_UNUSED UINT32* pDstSize,
                            WINPR_ATTR_UNUSED RDPGFX_H264_METABLOCK* meta)
{
	WINPR_ASSERT(hevc);
	WLog_Print(hevc->log, WLOG_ERROR,
	           "This build does not support HEVC codec. Recompile with '-DWITH_AVM=ON' or "
	           "'-DWITH_DHEVCD=ON'");
	return -1;
}

INT32 freerdp_hevc_decompress(FREERDP_HEVC_CONTEXT* hevc, const BYTE* pSrcData, UINT32 SrcSize,
                              BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth,
                              UINT32 nDstHeight, const RECTANGLE_16* regionRects,
                              UINT32 numRegionRect)
{
	WINPR_ASSERT(hevc);
	return h26X_decompress(hevc->context, pSrcData, SrcSize, pDstData, DstFormat, nDstStep,
	                       nDstWidth, nDstHeight, regionRects, numRegionRect);
}

BOOL freerdp_hevc_context_reset(FREERDP_HEVC_CONTEXT* hevc, UINT32 width, UINT32 height)
{
	WINPR_ASSERT(hevc);
	return h264_context_reset(hevc->context, width, height);
}

void freerdp_hevc_context_free(FREERDP_HEVC_CONTEXT* hevc)
{
	if (!hevc)
		return;

	h264_context_free(hevc->context);
	free(hevc);
}

FREERDP_HEVC_CONTEXT* freerdp_hevc_context_new(BOOL Compressor)
{
	FREERDP_HEVC_CONTEXT* ctx = calloc(1, sizeof(FREERDP_HEVC_CONTEXT));
	if (!ctx)
		return nullptr;
	ctx->Compressor = Compressor;
	ctx->log = WLog_Get(TAG);
	ctx->context = h264_context_new_ex(Compressor, TRUE, TRUE);
	if (!ctx->context)
		goto fail;
	return ctx;

fail:
	freerdp_hevc_context_free(ctx);
	return nullptr;
}
