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

INT32 freerdp_hevc_compress(FREERDP_HEVC_CONTEXT* hevc, const BYTE* pSrcData, DWORD SrcFormat,
                            UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
                            const RECTANGLE_16* regionRect, BYTE** ppDstData, UINT32* pDstSize,
                            RDPGFX_H264_METABLOCK* meta)
{
	WINPR_ASSERT(hevc);

	H264_CONTEXT* h264 = hevc->context;
	WINPR_ASSERT(h264);

	int rc = -1;
	BYTE* coded = nullptr;
	UINT32 codedSize = 0;

	if (!h264 || !h264->Compressor)
		return -1;

	if (!h264->subsystem->Compress)
		return -1;

	if (!avc420_ensure_buffer(h264, nSrcStep, nSrcWidth, nSrcHeight))
		return -1;

	if (!avc444_ensure_buffer(h264, nSrcHeight))
		return -1;

	prim_size_t roi = { .width = nSrcWidth, .height = nSrcHeight };

	const primitives_t* p = primitives_get();
	if (!p)
		return -1;

	const pstatus_t res = p->RGBToYUV444_8u_P3AC4R(pSrcData, SrcFormat, nSrcStep, h264->pYUV444Data,
	                                               h264->iYUV444Stride, &roi);
	if (res != 0)
		return -1;
	const BYTE* pcYUV444Data[3] = { h264->pYUV444Data[0], h264->pYUV444Data[1],
		                            h264->pYUV444Data[2] };

	if (!detect_changes(!h264->firstLumaFrameDone, h264->QP, regionRect, h264->pYUV444Data,
	                    h264->pOldYUV444Data, h264->iYUV444Stride, 0, meta))
		goto fail;
	if (h264->subsystem->Compress(h264, pcYUV444Data, h264->iYUV444Stride, &coded, &codedSize) < 0)
		goto fail;
	h264->firstLumaFrameDone = TRUE;

	*ppDstData = coded;
	*pDstSize = codedSize;
	rc = 1;
fail:
	if (rc < 0)
		free_h264_metablock(meta);
	return rc;
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
